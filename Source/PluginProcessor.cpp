#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <chrono>

#if _MSC_VER
#pragma float_control(precise, off)
#pragma fp_contract(on)
#endif

ViaU2AudioProcessor::ViaU2AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool ViaU2AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void ViaU2AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentMomentaryLUFS.store(-100.0f, std::memory_order_relaxed);
    currentShortTermLUFS.store(-100.0f, std::memory_order_relaxed);
    currentIntegratedLUFS.store(-100.0f, std::memory_order_relaxed);
    currentTruePeak.store(-100.0f, std::memory_order_relaxed);
    heldTruePeak.store(-100.0f, std::memory_order_relaxed);
    peakHit.store(false, std::memory_order_relaxed);

    oversampling.initProcessing(static_cast<size_t>(samplesPerBlock));

    peakHoldDuration = static_cast<int>(sampleRate * 1.5);
    peakHoldSamples = 0;

    maxMomSamples = static_cast<int>(sampleRate * 0.4);
    momentaryBufferL.assign(static_cast<size_t>(maxMomSamples), 0.0f);
    momentaryBufferR.assign(static_cast<size_t>(maxMomSamples), 0.0f);
    momWritePos = 0;
    momentarySum = 0.0;

    maxStSamples = static_cast<int>(sampleRate * 3.0);
    shortTermBufferL.assign(static_cast<size_t>(maxStSamples), 0.0f);
    shortTermBufferR.assign(static_cast<size_t>(maxStSamples), 0.0f);
    stWritePos = 0;
    shortTermSum = 0.0;

    integratedSum = 0.0;
    integratedCount = 0.0;

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.13547087602444f);
    auto shelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1500.0f, 0.707f, 4.0f);

    kFilterL1.coefficients = hpCoeffs;    kFilterR1.coefficients = hpCoeffs;
    kFilterL2.coefficients = shelfCoeffs; kFilterR2.coefficients = shelfCoeffs;
    kFilterL1.reset(); kFilterR1.reset();
    kFilterL2.reset(); kFilterR2.reset();

    // --- A-weighting filter (for LEQ(A) measurement mode and VU/PPM A-weighted ballistics) ---
    aWeightL.prepare(sampleRate);
    aWeightR.prepare(sampleRate);

    // --- Extra 400ms momentary windows for A-weighted and unweighted measurement modes ---
    aWeightedMomBufferL.assign(static_cast<size_t>(maxMomSamples), 0.0f);
    aWeightedMomBufferR.assign(static_cast<size_t>(maxMomSamples), 0.0f);
    unweightedMomBufferL.assign(static_cast<size_t>(maxMomSamples), 0.0f);
    unweightedMomBufferR.assign(static_cast<size_t>(maxMomSamples), 0.0f);
    aWeightedMomWritePos = 0;
    unweightedMomWritePos = 0;
    aWeightedMomSum = 0.0;
    unweightedMomSum = 0.0;
    currentMeasurementValue.store(-100.0f, std::memory_order_relaxed);
    preProcessLevelDb.store(-100.0f, std::memory_order_relaxed);
    postProcessLevelDb.store(-100.0f, std::memory_order_relaxed);

    // --- VU / PPM ballistic time constants ---
    // Classic VU (IEC 60268-17): ~99% in 300ms for both attack and release;
    // a one-pole envelope reaches ~99% of a step input in about 5 time constants,
    // so tau = 300ms / 5 ≈ 60ms gives the standard's characteristic speed.
    const float vuTau = 0.060f;
    vuAttackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * vuTau));
    vuReleaseCoeff = vuAttackCoeff; // VU rise and fall times are equal per spec
    vuEnvelope = 0.0f;
    currentVuReading.store(-100.0f, std::memory_order_relaxed);

    // BBC-style PPM (IEC 60268-10 Type IIa): 1.7ms attack time constant,
    // 650ms decay time constant (the combination that yields the spec's
    // 1.5s-to-fall-20dB characteristic).
    ppmAttackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.0017f));
    ppmReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.650f));
    ppmEnvelope = 0.0f;

    for (int b = 0; b < 3; ++b)
    {
        bands[static_cast<size_t>(b)].lp1.reset(); bands[static_cast<size_t>(b)].lp2.reset();
        bands[static_cast<size_t>(b)].hp1.reset(); bands[static_cast<size_t>(b)].hp2.reset();
        bands[static_cast<size_t>(b)].env = 0.0f;
        bands[static_cast<size_t>(b)].gain = 1.0f;
    }

    auto lpLow = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverFreq[0], 0.7071f);
    auto hpMid = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, crossoverFreq[0], 0.7071f);
    bands[0].lp1.coefficients = lpLow; bands[0].lp2.coefficients = lpLow;
    bands[1].hp1.coefficients = hpMid; bands[1].hp2.coefficients = hpMid;

    auto lpMid = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverFreq[1], 0.7071f);
    auto hpHigh = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, crossoverFreq[1], 0.7071f);
    bands[1].lp1.coefficients = lpMid; bands[1].lp2.coefficients = lpMid;
    bands[2].hp1.coefficients = hpHigh; bands[2].hp2.coefficients = hpHigh;

    for (int i = 0; i < 4; ++i)
    {
        float tanWc = std::tan(static_cast<float>(juce::MathConstants<double>::pi) * phaseFreqs[static_cast<size_t>(i)] / static_cast<float>(sampleRate));
        float a = (tanWc - 1.0f) / (tanWc + 1.0f);
        phaseRotatorL[static_cast<size_t>(i)].a = a;
        phaseRotatorR[static_cast<size_t>(i)].a = a;
        phaseRotatorL[static_cast<size_t>(i)].reset();
        phaseRotatorR[static_cast<size_t>(i)].reset();
    }

    lookaheadSamples = juce::jmax(1, static_cast<int>(sampleRate * 0.005));
    lookaheadBufferL.assign(static_cast<size_t>(lookaheadSamples), 0.0f);
    lookaheadBufferR.assign(static_cast<size_t>(lookaheadSamples), 0.0f);
    lookaheadWritePos = 0;
    peakEnv = 0.0f;
    currentGainReduction = 1.0f;
    smoothedMakeupGain = 1.0f;
    limiterPeakDeque.clear();

    // Precompute limiter time constants once here instead of recomputing
    // std::exp() on every single processBlock call (cheap but needless churn).
    limiterAttackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.001f));
    limiterReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.050f));
    mbAttackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.005f));
    mbReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.100f));

    // Precompute Goertzel coefficients for the 24 Bark-centre frequencies.
    // coeff[b] = 2 * cos(2π * f_b / fs) — computed once, used every block.
    static constexpr std::array<float, 24> barkCenters = {
        50.f, 150.f, 250.f, 350.f, 450.f, 570.f, 700.f, 840.f,
        1000.f, 1170.f, 1370.f, 1600.f, 1850.f, 2150.f, 2500.f, 2900.f,
        3300.f, 3700.f, 4400.f, 5100.f, 5800.f, 6800.f, 8000.f, 10000.f
    };
    for (int b = 0; b < 24; ++b)
        goertzelCoeff[static_cast<size_t>(b)] = 2.0f * std::cos(2.0f * juce::MathConstants<float>::pi
            * barkCenters[static_cast<size_t>(b)] / static_cast<float>(sampleRate));
    goertzelN = samplesPerBlock > 0 ? samplesPerBlock : 512;
    barkBandLevels.fill(0.0f);
    aidUpdateCounter = 0;

    setLatencySamples(static_cast<int>(oversampling.getLatencyInSamples()) + lookaheadSamples);
}

void ViaU2AudioProcessor::releaseResources() { oversampling.reset(); }

void ViaU2AudioProcessor::resetIntegratedLoudness() noexcept
{
    integratedSum = 0.0;
    integratedCount = 0.0;
    currentIntegratedLUFS.store(-100.0f, std::memory_order_relaxed);
}

void ViaU2AudioProcessor::exportCSV() noexcept
{
    juce::String filename = "VIAU_Loudness_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".csv";
    auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile(filename);
    if (auto stream = file.createOutputStream())
    {
        *stream << "Timestamp,Integrated_LUFS,ShortTerm_LUFS,Momentary_LUFS,TruePeak_dBTP,Target_Standard,Target_LUFS,Measurement_Mode,Measurement_Value,VU_Ballistic_Mode\n";
        *stream << juce::Time::getCurrentTime().toString(true, true, true, true) << ",";
        *stream << currentIntegratedLUFS.load() << "," << currentShortTermLUFS.load() << ",";
        *stream << currentMomentaryLUFS.load() << "," << currentTruePeak.load() << ",";
        *stream << getTargetName() << "," << targetLUFS.load() << ",";
        *stream << getMeasurementModeName() << "," << getCurrentMeasurement() << ",";
        *stream << getVuBallisticModeName() << "\n";
        stream->flush();
    }
}

void ViaU2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    bool bypassed = isBypassed.load(std::memory_order_relaxed);
    if (bypassed)
    {
        const float* leftIn = buffer.getReadPointer(0);
        const float* rightIn = numChannels > 1 ? buffer.getReadPointer(1) : leftIn;
        float blockPeak = 0.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            float peak = juce::jmax(std::abs(leftIn[n]), std::abs(rightIn[n]));
            blockPeak = juce::jmax(blockPeak, peak);
        }
        currentTruePeak.store(juce::Decibels::gainToDecibels(blockPeak, -100.0f), std::memory_order_relaxed);
        return;
    }

    const float sampleRateF = static_cast<float>(getSampleRate());

    // --- A/B "before" capture: cheap unweighted RMS of the raw input, taken
    // before any band/limiter/phase processing touches it. Used by the VIEW
    // button's before/after comparison display. ---
    {
        const float* rawL = buffer.getReadPointer(0);
        const float* rawR = numChannels > 1 ? buffer.getReadPointer(1) : rawL;
        double sumSq = 0.0;
        for (int n = 0; n < numSamples; ++n)
            sumSq += static_cast<double>(rawL[n]) * rawL[n] + static_cast<double>(rawR[n]) * rawR[n];
        const double meanSq = sumSq / (static_cast<double>(numSamples) * 2.0);
        const float beforeDb = static_cast<float>(10.0 * std::log10(meanSq + 1e-12));
        // Light smoothing so the A/B readout doesn't flicker block-to-block
        float prevBefore = preProcessLevelDb.load(std::memory_order_relaxed);
        preProcessLevelDb.store(prevBefore + 0.3f * (beforeDb - prevBefore), std::memory_order_relaxed);
    }

    // Load parameters once per block for conditional execution
    bool doMultiband = multibandEnabled.load(std::memory_order_relaxed);
    float phaseBlend = phaseRotAmount.load(std::memory_order_relaxed);
    bool doPhase = phaseBlend > 0.01f;
    float drive = driveAmount.load(std::memory_order_relaxed);
    bool doExciter = drive > 1.05f; // Bypass if drive is effectively 1.0 (clean)

    // =====================================================================
    // 1. MULTIBAND DENSITY COMPRESSION (Conditional)
    // =====================================================================
    if (doMultiband)
    {
        float threshLin[3];
        threshLin[0] = juce::Decibels::decibelsToGain(lowBandThresh.load(std::memory_order_relaxed));
        threshLin[1] = juce::Decibels::decibelsToGain(midBandThresh.load(std::memory_order_relaxed));
        threshLin[2] = juce::Decibels::decibelsToGain(highBandThresh.load(std::memory_order_relaxed));

        const float attackCoeff = mbAttackCoeff;
        const float releaseCoeff = mbReleaseCoeff;
        const float ratio = 3.0f;

        float* mbPtrL = buffer.getWritePointer(0);
        float* mbPtrR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            float inL = mbPtrL[n];
            float inR = (mbPtrR != nullptr) ? mbPtrR[n] : inL;
            float outL = 0.0f, outR = 0.0f;
            float bandSig[3][2];

            float lowLP1 = bands[0].lp1.processSample(inL);
            float lowLP2 = bands[0].lp2.processSample(lowLP1);
            bandSig[0][0] = lowLP2;

            float midHP1 = bands[1].hp1.processSample(inL);
            float midHP2 = bands[1].hp2.processSample(midHP1);
            float midLP1 = bands[1].lp1.processSample(midHP2);
            float midLP2 = bands[1].lp2.processSample(midLP1);
            bandSig[1][0] = midLP2;

            float highHP1 = bands[2].hp1.processSample(inL);
            float highHP2 = bands[2].hp2.processSample(highHP1);
            bandSig[2][0] = highHP2;

            if (numChannels > 1)
            {
                lowLP1 = bands[0].lp1.processSample(inR);
                lowLP2 = bands[0].lp2.processSample(lowLP1);
                bandSig[0][1] = lowLP2;

                midHP1 = bands[1].hp1.processSample(inR);
                midHP2 = bands[1].hp2.processSample(midHP1);
                midLP1 = bands[1].lp1.processSample(midHP2);
                midLP2 = bands[1].lp2.processSample(midLP1);
                bandSig[1][1] = midLP2;

                highHP1 = bands[2].hp1.processSample(inR);
                highHP2 = bands[2].hp2.processSample(highHP1);
                bandSig[2][1] = highHP2;
            }
            else
            {
                bandSig[0][1] = bandSig[0][0];
                bandSig[1][1] = bandSig[1][0];
                bandSig[2][1] = bandSig[2][0];
            }

            for (int b = 0; b < 3; ++b)
            {
                float peak = juce::jmax(std::abs(bandSig[b][0]), std::abs(bandSig[b][1]));
                float& env = bands[static_cast<size_t>(b)].env;
                if (peak > env) env += attackCoeff * (peak - env);
                else            env += releaseCoeff * (peak - env);

                float gr = 1.0f;
                if (env > threshLin[b] && env > 0.0001f)
                {
                    float overDB = juce::Decibels::gainToDecibels(env) - juce::Decibels::gainToDecibels(threshLin[b]);
                    gr = juce::Decibels::decibelsToGain(-overDB * (1.0f - 1.0f / ratio));
                }

                float& smoothGain = bands[static_cast<size_t>(b)].gain;
                smoothGain += 0.05f * (gr - smoothGain);

                bandSig[b][0] *= smoothGain;
                bandSig[b][1] *= smoothGain;

                float grDB = juce::Decibels::gainToDecibels(smoothGain, -100.0f);
                bandGR[static_cast<size_t>(b)].store(grDB, std::memory_order_relaxed);

                outL += bandSig[b][0];
                outR += bandSig[b][1];
            }

            mbPtrL[n] = outL;
            if (mbPtrR != nullptr) mbPtrR[n] = outR;
        }
    }
    else
    {
        for (int b = 0; b < 3; ++b)
            bandGR[static_cast<size_t>(b)].store(0.0f, std::memory_order_relaxed);
    }

    // =====================================================================
    // 2. PHASE ROTATOR (Conditional)
    // =====================================================================
    if (doPhase)
    {
        float* phPtrL = buffer.getWritePointer(0);
        float* phPtrR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            float inL = phPtrL[n];
            float inR = (phPtrR != nullptr) ? phPtrR[n] : inL;
            float rotL = inL, rotR = inR;

            for (int s = 0; s < 4; ++s)
            {
                rotL = phaseRotatorL[static_cast<size_t>(s)].processSample(rotL);
                rotR = phaseRotatorR[static_cast<size_t>(s)].processSample(rotR);
            }

            phPtrL[n] = inL * (1.0f - phaseBlend) + rotL * phaseBlend;
            if (phPtrR != nullptr)
                phPtrR[n] = inR * (1.0f - phaseBlend) + rotR * phaseBlend;
        }
    }

    // =====================================================================
    // 3. HARMONIC EXCITER (Conditional - Skips heavy oversampling if clean)
    // =====================================================================
    if (doExciter)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::AudioBlock<float> osBlock(oversampling.processSamplesUp(block));
        float driveNorm = std::tanh(drive);
        if (driveNorm < 0.0001f) driveNorm = 0.0001f;

        for (int ch = 0; ch < static_cast<int>(osBlock.getNumChannels()); ++ch)
        {
            auto* data = osBlock.getChannelPointer(ch);
            for (int n = 0; n < static_cast<int>(osBlock.getNumSamples()); ++n)
            {
                data[n] = std::tanh(data[n] * drive) / driveNorm;
            }
        }
        oversampling.processSamplesDown(block);
    }

    // =====================================================================
    // 4. LOOKAHEAD LIMITER — O(N) sliding-window maximum (monotone deque)
    //    instead of the previous O(N×lookahead) inner scan.
    //    The deque holds {peak, ringbuffer-position} pairs in decreasing order
    //    of peak; the front is always the maximum in the current lookahead window.
    // =====================================================================
    {
        const float ceilingLin = juce::Decibels::decibelsToGain(ceilingDB.load(std::memory_order_relaxed));
        float* outPtrL = buffer.getWritePointer(0);
        float* outPtrR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = outPtrL[n];
            const float inR = (outPtrR != nullptr) ? outPtrR[n] : inL;
            const float absPeak = juce::jmax(std::abs(inL), std::abs(inR));

            // Write new sample into the ring buffer
            lookaheadBufferL[static_cast<size_t>(lookaheadWritePos)] = inL;
            lookaheadBufferR[static_cast<size_t>(lookaheadWritePos)] = inR;

            // Maintain the monotone deque: remove back entries smaller than
            // the new sample (they can never be the maximum going forward)
            while (!limiterPeakDeque.empty() && limiterPeakDeque.back().first <= absPeak)
                limiterPeakDeque.pop_back();
            limiterPeakDeque.push_back({ absPeak, lookaheadWritePos });

            // Remove front entry if it has exited the lookahead window
            const int oldest = (lookaheadWritePos - lookaheadSamples + 2 * lookaheadSamples) % lookaheadSamples;
            if (limiterPeakDeque.front().second == oldest)
                limiterPeakDeque.pop_front();

            // Front of deque = maximum peak in window — O(1) lookup
            const float maxPeak = limiterPeakDeque.empty() ? absPeak : limiterPeakDeque.front().first;

            if (maxPeak > peakEnv) peakEnv += limiterAttackCoeff * (maxPeak - peakEnv);
            else                   peakEnv += limiterReleaseCoeff * (maxPeak - peakEnv);

            const float gr = (peakEnv > ceilingLin && peakEnv > 0.0001f) ? (ceilingLin / peakEnv) : 1.0f;
            currentGainReduction += 0.1f * (gr - currentGainReduction);

            const int readPos = (lookaheadWritePos - lookaheadSamples + 2 * lookaheadSamples) % lookaheadSamples;
            outPtrL[n] = lookaheadBufferL[static_cast<size_t>(readPos)] * currentGainReduction;
            if (outPtrR != nullptr)
                outPtrR[n] = lookaheadBufferR[static_cast<size_t>(readPos)] * currentGainReduction;

            lookaheadWritePos = (lookaheadWritePos + 1) % lookaheadSamples;
        }
    }

    // =====================================================================
    // 5. BS.1770 METERING + AID ANALYSIS
    // =====================================================================
    const float* leftIn = buffer.getReadPointer(0);
    const float* rightIn = numChannels > 1 ? buffer.getReadPointer(1) : leftIn;

    float blockPeak = 0.0f;
    const auto activeMeasMode = measurementMode.load(std::memory_order_relaxed);
    const auto activeVuMode = vuBallisticMode.load(std::memory_order_relaxed);
    const bool vuUsesAWeight = (activeVuMode == VuBallisticMode::ClassicVU_A || activeVuMode == VuBallisticMode::PPM_A);
    const bool vuIsPpm = (activeVuMode == VuBallisticMode::PPM_K || activeVuMode == VuBallisticMode::PPM_A);
    // Only run A-weighting when a mode that actually needs it is selected.
    // The filter runs every sample, so skipping it when unused saves ~8 biquad
    // evaluations per sample — meaningful at 96kHz or with large block sizes.
    const bool needsAWeight = (activeMeasMode == MeasurementMode::AWeightedLeqA) || vuUsesAWeight;
    const bool needsUnweighted = (activeMeasMode == MeasurementMode::UnweightedRMS
        || activeMeasMode == MeasurementMode::Dbu);

    for (int n = 0; n < numSamples; ++n)
    {
        float leftFiltered = kFilterL1.processSample(leftIn[n]);
        leftFiltered = kFilterL2.processSample(leftFiltered);
        float rightFiltered = kFilterR1.processSample(rightIn[n]);
        rightFiltered = kFilterR2.processSample(rightFiltered);

        const float leftPower = leftFiltered * leftFiltered;
        const float rightPower = rightFiltered * rightFiltered;
        const float framePower = leftPower + rightPower;

        // --- A-weighted parallel path (only processed when needed) ---
        float leftAWeighted = 0.0f, rightAWeighted = 0.0f;
        float leftAPower = 0.0f, rightAPower = 0.0f, frameAPower = 0.0f;
        if (needsAWeight)
        {
            leftAWeighted = aWeightL.processSample(leftIn[n]);
            rightAWeighted = aWeightR.processSample(rightIn[n]);
            leftAPower = leftAWeighted * leftAWeighted;
            rightAPower = rightAWeighted * rightAWeighted;
            frameAPower = leftAPower + rightAPower;
        }

        // --- Unweighted parallel signal (feeds Unweighted RMS + dBu modes) ---
        const float leftUnweightedPower = leftIn[n] * leftIn[n];
        const float rightUnweightedPower = rightIn[n] * rightIn[n];
        const float frameUnweightedPower = leftUnweightedPower + rightUnweightedPower;

        blockPeak = juce::jmax(blockPeak, std::abs(leftIn[n]));
        blockPeak = juce::jmax(blockPeak, std::abs(rightIn[n]));

        // K-weighted momentary (BS.1770, existing)
        float oldMomL = momentaryBufferL[static_cast<size_t>(momWritePos)];
        float oldMomR = momentaryBufferR[static_cast<size_t>(momWritePos)];
        momentaryBufferL[static_cast<size_t>(momWritePos)] = leftPower;
        momentaryBufferR[static_cast<size_t>(momWritePos)] = rightPower;
        momentarySum += framePower - (oldMomL + oldMomR);
        momWritePos = (momWritePos + 1) % maxMomSamples;
        double currentMomCount = juce::jmin(momWritePos == 0 ? maxMomSamples : momWritePos, maxMomSamples);
        if (momentarySum > 0.0 && currentMomCount > 0)
            currentMomentaryLUFS.store(static_cast<float>(-0.691 + 10.0 * std::log10(momentarySum / (currentMomCount * 2.0) + 1e-12)), std::memory_order_relaxed);

        // A-weighted momentary (LEQ(A) mode) — only updated when needed
        if (needsAWeight)
        {
            float oldAMomL = aWeightedMomBufferL[static_cast<size_t>(aWeightedMomWritePos)];
            float oldAMomR = aWeightedMomBufferR[static_cast<size_t>(aWeightedMomWritePos)];
            aWeightedMomBufferL[static_cast<size_t>(aWeightedMomWritePos)] = leftAPower;
            aWeightedMomBufferR[static_cast<size_t>(aWeightedMomWritePos)] = rightAPower;
            aWeightedMomSum += frameAPower - (oldAMomL + oldAMomR);
            aWeightedMomWritePos = (aWeightedMomWritePos + 1) % maxMomSamples;
        }

        // Unweighted momentary (Unweighted RMS + dBu modes)
        float oldUMomL = unweightedMomBufferL[static_cast<size_t>(unweightedMomWritePos)];
        float oldUMomR = unweightedMomBufferR[static_cast<size_t>(unweightedMomWritePos)];
        unweightedMomBufferL[static_cast<size_t>(unweightedMomWritePos)] = leftUnweightedPower;
        unweightedMomBufferR[static_cast<size_t>(unweightedMomWritePos)] = rightUnweightedPower;
        unweightedMomSum += frameUnweightedPower - (oldUMomL + oldUMomR);
        unweightedMomWritePos = (unweightedMomWritePos + 1) % maxMomSamples;
        if (unweightedMomSum > 0.0 && currentMomCount > 0)
            postProcessLevelDb.store(static_cast<float>(10.0 * std::log10(unweightedMomSum / (currentMomCount * 2.0) + 1e-12)), std::memory_order_relaxed);

        // Publish whichever measurement mode is currently active (cheap: just a switch + one store)
        if (currentMomCount > 0)
        {
            switch (activeMeasMode)
            {
            case MeasurementMode::KWeightedMomentary:
                // Already published above via currentMomentaryLUFS; mirror it here too.
                currentMeasurementValue.store(currentMomentaryLUFS.load(std::memory_order_relaxed), std::memory_order_relaxed);
                break;
            case MeasurementMode::AWeightedLeqA:
                if (aWeightedMomSum > 0.0)
                    currentMeasurementValue.store(static_cast<float>(-0.691 + 10.0 * std::log10(aWeightedMomSum / (currentMomCount * 2.0) + 1e-12)), std::memory_order_relaxed);
                break;
            case MeasurementMode::UnweightedRMS:
                if (unweightedMomSum > 0.0)
                    currentMeasurementValue.store(static_cast<float>(10.0 * std::log10(unweightedMomSum / (currentMomCount * 2.0) + 1e-12)), std::memory_order_relaxed);
                break;
            case MeasurementMode::Dbu:
            {
                if (unweightedMomSum > 0.0)
                {
                    const float dbfs = static_cast<float>(10.0 * std::log10(unweightedMomSum / (currentMomCount * 2.0) + 1e-12));
                    // EBU R68: -18 dBFS = 0 dBu  |  SMPTE RP155: -20 dBFS = +4 dBu (0 dBu at -24 dBFS)
                    const float dbuOffset = (dbuReference.load(std::memory_order_relaxed) == DbuReference::EBU_R68) ? 18.0f : 24.0f;
                    currentMeasurementValue.store(dbfs + dbuOffset, std::memory_order_relaxed);
                }
                break;
            }
            default: break;
            }
        }

        // --- VU / PPM ballistic envelope (drives the analog needle) ---
        // Uses peak-of-channels (not summed power) since both VU and PPM are
        // designed around single-channel signal amplitude, not loudness power.
        const float vuInputL = vuUsesAWeight ? std::abs(leftAWeighted) : std::abs(leftFiltered);
        const float vuInputR = vuUsesAWeight ? std::abs(rightAWeighted) : std::abs(rightFiltered);
        const float vuInput = juce::jmax(vuInputL, vuInputR);

        if (vuIsPpm)
        {
            if (vuInput > ppmEnvelope) ppmEnvelope += ppmAttackCoeff * (vuInput - ppmEnvelope);
            else                       ppmEnvelope += ppmReleaseCoeff * (vuInput - ppmEnvelope);
            currentVuReading.store(juce::Decibels::gainToDecibels(ppmEnvelope, -100.0f), std::memory_order_relaxed);
        }
        else
        {
            if (vuInput > vuEnvelope) vuEnvelope += vuAttackCoeff * (vuInput - vuEnvelope);
            else                      vuEnvelope += vuReleaseCoeff * (vuInput - vuEnvelope);
            currentVuReading.store(juce::Decibels::gainToDecibels(vuEnvelope, -100.0f), std::memory_order_relaxed);
        }

        float oldStL = shortTermBufferL[static_cast<size_t>(stWritePos)];
        float oldStR = shortTermBufferR[static_cast<size_t>(stWritePos)];
        shortTermBufferL[static_cast<size_t>(stWritePos)] = leftPower;
        shortTermBufferR[static_cast<size_t>(stWritePos)] = rightPower;
        shortTermSum += framePower - (oldStL + oldStR);
        stWritePos = (stWritePos + 1) % maxStSamples;
        double currentStCount = juce::jmin(stWritePos == 0 ? maxStSamples : stWritePos, maxStSamples);
        if (shortTermSum > 0.0 && currentStCount > 0)
            currentShortTermLUFS.store(static_cast<float>(-0.691 + 10.0 * std::log10(shortTermSum / (currentStCount * 2.0) + 1e-12)), std::memory_order_relaxed);

        integratedSum += framePower;
        integratedCount += 2.0;
        if (integratedSum > 0.0 && integratedCount > 0.0)
            currentIntegratedLUFS.store(static_cast<float>(-0.691 + 10.0 * std::log10(integratedSum / integratedCount + 1e-12)), std::memory_order_relaxed);
    }

    const float peakDb = juce::Decibels::gainToDecibels(blockPeak, -100.0f);
    currentTruePeak.store(peakDb, std::memory_order_relaxed);

    float currentHeld = heldTruePeak.load(std::memory_order_relaxed);
    if (peakDb > currentHeld)
    {
        heldTruePeak.store(peakDb, std::memory_order_relaxed);
        peakHoldSamples = peakHoldDuration;
    }
    else
    {
        peakHoldSamples -= numSamples;
        if (peakHoldSamples <= 0)
        {
            const float decayed = currentHeld * std::pow(0.9995f, static_cast<float>(numSamples));
            heldTruePeak.store(juce::jmax(decayed, -100.0f), std::memory_order_relaxed);
        }
    }
    peakHit.store(peakDb >= -0.3f, std::memory_order_relaxed);

    // =====================================================================
    // AID BARK-BAND ANALYSIS — Goertzel algorithm (O(N) per band, no sin/cos
    // in the inner loop). Only runs when the VIEW button shows an AID bar-graph
    // (styles 0-4); skips entirely for A/B (5) and overlay (6) views.
    // Also skips if the plugin is bypassed since the user likely cares less
    // about spectral analysis in that state.
    // =====================================================================
    const int currentView = aidViewStyle.load(std::memory_order_relaxed);
    const bool aidViewActive = (currentView >= 0 && currentView <= 4);

    if (aidViewActive && !isBypassed.load(std::memory_order_relaxed) && ++aidUpdateCounter >= 4)
    {
        aidUpdateCounter = 0;

        // Mono sum for spectral analysis (saves processing one channel worth of Goertzel)
        const int N = numSamples;
        float totalActive = 0.0f;
        const float noiseFloorLin = juce::Decibels::decibelsToGain(-70.0f);

        for (int b = 0; b < 24; ++b)
        {
            // Goertzel: two-multiply-add recurrence, no trig per sample.
            // s[n] = x[n] + coeff * s[n-1] - s[n-2]
            // Magnitude² = s[N-1]² + s[N-2]² - coeff * s[N-1] * s[N-2]
            const float coeff = goertzelCoeff[static_cast<size_t>(b)];
            float s1 = 0.0f, s2 = 0.0f;
            for (int n = 0; n < N; ++n)
            {
                const float monoSample = 0.5f * (leftIn[n] + rightIn[n]);
                const float s0 = monoSample + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            // Power estimate normalized by block size
            const float power = (s1 * s1 + s2 * s2 - coeff * s1 * s2) / static_cast<float>(N * N);
            const float energy = std::sqrt(juce::jmax(0.0f, power));

            float& disp = barkBandLevels[static_cast<size_t>(b)];
            disp = disp * 0.75f + energy * 0.25f; // exponential smoothing

            if (disp > noiseFloorLin) totalActive += 1.0f;
        }
        spectralOccupancy.store(totalActive / 24.0f, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* ViaU2AudioProcessor::createEditor() { return new ViaU2AudioProcessorEditor(*this); }
void ViaU2AudioProcessor::getStateInformation(juce::MemoryBlock& destData) { juce::ignoreUnused(destData); }
void ViaU2AudioProcessor::setStateInformation(const void* data, int sizeInBytes) { juce::ignoreUnused(data, sizeInBytes); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ViaU2AudioProcessor(); }