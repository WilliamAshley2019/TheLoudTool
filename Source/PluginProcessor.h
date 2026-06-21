#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <array>

// Professional loudness target standards the INT button cycles through.
// Selecting a standard sets targetLUFS (the same atomic the TARGET knob
// drives) to that standard's reference value — the knob can still be
// fine-tuned afterward without losing the "currently selected standard" label.
enum class LoudnessStandard
{
    EBU_R128 = 0,   // EU broadcast: -23.0 LUFS integrated
    Streaming,      // Spotify / YouTube / Apple Music consensus: -14.0 LUFS integrated
    ATSC_A85,       // US broadcast (ATSC A/85): -24.0 LUFS integrated
    AppleMusic,     // Apple Music Sound Check target: -16.0 LUFS integrated
    Custom,         // User has manually adjusted the TARGET knob away from a standard
    NumStandards
};

// Short-term "current reading" measurement math the MODE button cycles through.
// All four are computed continuously and cheaply (no extra oversampling); only
// the lightweight display value differs.
enum class MeasurementMode
{
    KWeightedMomentary = 0, // ITU-R BS.1770 K-weighted, 400ms momentary window (existing default)
    AWeightedLeqA,          // ANSI S1.42 / IEC 61672 A-weighted, same 400ms momentary window
    UnweightedRMS,          // Flat dBFS RMS, no frequency weighting, same 400ms window
    Dbu,                    // Unweighted RMS re-referenced to an analog dBu scale
    NumMeasurementModes
};

// Which alignment convention calibrates the dBu measurement mode.
enum class DbuReference
{
    EBU_R68 = 0,    // -18 dBFS = 0 dBu
    SMPTE_RP155,    // -20 dBFS = +4 dBu  (so 0 dBu sits at -24 dBFS)
    NumDbuReferences
};

// Ballistic law + weighting curve driving the analog-style VU needle.
enum class VuBallisticMode
{
    ClassicVU_K = 0,  // IEC 60268-17: 300ms integration time, K-weighted
    ClassicVU_A,      // IEC 60268-17 ballistics, A-weighted
    PPM_K,            // BBC-style PPM: 1.7ms attack / 650ms decay, K-weighted
    PPM_A,            // BBC-style PPM ballistics, A-weighted
    NumVuModes
};

class ViaU2AudioProcessor final : public juce::AudioProcessor
{
public:
    ViaU2AudioProcessor();
    ~ViaU2AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Metering Getters
    float getCurrentMomentaryLUFS() const noexcept { return currentMomentaryLUFS.load(std::memory_order_relaxed); }
    float getCurrentShortTermLUFS() const noexcept { return currentShortTermLUFS.load(std::memory_order_relaxed); }
    float getCurrentIntegratedLUFS() const noexcept { return currentIntegratedLUFS.load(std::memory_order_relaxed); }
    float getCurrentTruePeak() const noexcept { return currentTruePeak.load(std::memory_order_relaxed); }
    float getHeldTruePeak() const noexcept { return heldTruePeak.load(std::memory_order_relaxed); }
    bool isPeakHit() const noexcept { return peakHit.load(std::memory_order_relaxed); }

    void resetIntegratedLoudness() noexcept;

    // ========================================================================
    // INT button: long-term integrated target standard.
    // Cycling sets targetLUFS to the standard's reference value; the TARGET
    // knob remains live and can be fine-tuned afterward (which flips the
    // active standard to Custom so the LCD never shows a stale/wrong label).
    // ========================================================================
    LoudnessStandard getLoudnessStandard() const noexcept { return loudnessStandard.load(std::memory_order_relaxed); }

    void cycleIntegratedStandard() noexcept
    {
        auto current = static_cast<int>(loudnessStandard.load(std::memory_order_relaxed));
        // Skip over Custom when cycling — it's a passive state, not something to land on by pressing INT.
        auto next = (current + 1) % (static_cast<int>(LoudnessStandard::NumStandards) - 1);
        auto standard = static_cast<LoudnessStandard>(next);
        loudnessStandard.store(standard, std::memory_order_relaxed);
        targetLUFS.store(getTargetLUFSFor(standard), std::memory_order_relaxed);
    }

    // Called by the editor when the TARGET knob is moved directly by the user,
    // so the LCD label correctly falls back to "CUSTOM" instead of lying.
    void notifyTargetKnobMovedManually() noexcept
    {
        loudnessStandard.store(LoudnessStandard::Custom, std::memory_order_relaxed);
    }

    // Target integrated LUFS for a given standard (static lookup table)
    static float getTargetLUFSFor(LoudnessStandard standard) noexcept
    {
        switch (standard)
        {
            case LoudnessStandard::EBU_R128:   return -23.0f;
            case LoudnessStandard::Streaming:  return -14.0f;
            case LoudnessStandard::ATSC_A85:   return -24.0f;
            case LoudnessStandard::AppleMusic: return -16.0f;
            default:                           return -23.0f;
        }
    }

    // Short display name for the currently active standard
    juce::String getTargetName() const noexcept
    {
        switch (getLoudnessStandard())
        {
            case LoudnessStandard::EBU_R128:   return "EBU R128";
            case LoudnessStandard::Streaming:  return "STREAMING";
            case LoudnessStandard::ATSC_A85:   return "ATSC A/85";
            case LoudnessStandard::AppleMusic: return "APPLE MUSIC";
            case LoudnessStandard::Custom:     return "CUSTOM";
            default:                           return "EBU R128";
        }
    }

    // ========================================================================
    // MODE button: short-term "current reading" measurement math.
    // ========================================================================
    MeasurementMode getMeasurementMode() const noexcept { return measurementMode.load(std::memory_order_relaxed); }

    void cycleMeasurementMode() noexcept
    {
        auto next = (static_cast<int>(measurementMode.load(std::memory_order_relaxed)) + 1)
                    % static_cast<int>(MeasurementMode::NumMeasurementModes);
        measurementMode.store(static_cast<MeasurementMode>(next), std::memory_order_relaxed);
    }

    // Cycles which alignment convention the dBu mode is calibrated against.
    // Separate from cycleMeasurementMode so it doesn't interrupt MODE cycling;
    // call from a long-press or secondary control if desired.
    void cycleDbuReference() noexcept
    {
        auto next = (static_cast<int>(dbuReference.load(std::memory_order_relaxed)) + 1)
                    % static_cast<int>(DbuReference::NumDbuReferences);
        dbuReference.store(static_cast<DbuReference>(next), std::memory_order_relaxed);
    }
    DbuReference getDbuReference() const noexcept { return dbuReference.load(std::memory_order_relaxed); }

    // Short display name for the currently active measurement mode
    juce::String getMeasurementModeName() const noexcept
    {
        switch (getMeasurementMode())
        {
            case MeasurementMode::KWeightedMomentary: return "K-WEIGHTED";
            case MeasurementMode::AWeightedLeqA:       return "LEQ(A)";
            case MeasurementMode::UnweightedRMS:       return "UNWEIGHTED";
            case MeasurementMode::Dbu:                  return getDbuReference() == DbuReference::EBU_R68 ? "dBu (EBU)" : "dBu (SMPTE)";
            default:                                    return "K-WEIGHTED";
        }
    }

    // Current reading for whichever MeasurementMode is active, in that mode's
    // native unit (LUFS for the weighted modes, dBFS for unweighted, dBu for dBu mode).
    float getCurrentMeasurement() const noexcept { return currentMeasurementValue.load(std::memory_order_relaxed); }

    // A/B before/after comparison (VIEW button's comparison mode), both in
    // unweighted dBFS RMS so the comparison is apples-to-apples regardless
    // of which MeasurementMode the MODE button currently has selected.
    float getPreProcessLevel() const noexcept { return preProcessLevelDb.load(std::memory_order_relaxed); }
    float getPostProcessLevel() const noexcept { return postProcessLevelDb.load(std::memory_order_relaxed); }

    // ========================================================================
    // VU button: ballistic law + weighting curve driving the analog needle.
    // ========================================================================
    VuBallisticMode getVuBallisticMode() const noexcept { return vuBallisticMode.load(std::memory_order_relaxed); }

    void cycleVuBallisticMode() noexcept
    {
        auto next = (static_cast<int>(vuBallisticMode.load(std::memory_order_relaxed)) + 1)
                    % static_cast<int>(VuBallisticMode::NumVuModes);
        vuBallisticMode.store(static_cast<VuBallisticMode>(next), std::memory_order_relaxed);
    }

    juce::String getVuBallisticModeName() const noexcept
    {
        switch (getVuBallisticMode())
        {
            case VuBallisticMode::ClassicVU_K: return "VU (K)";
            case VuBallisticMode::ClassicVU_A: return "VU (A)";
            case VuBallisticMode::PPM_K:        return "PPM (K)";
            case VuBallisticMode::PPM_A:        return "PPM (A)";
            default:                             return "VU (K)";
        }
    }

    // Current VU/PPM needle reading, in dB relative to that ballistic's own reference.
    float getCurrentVuReading() const noexcept { return currentVuReading.load(std::memory_order_relaxed); }

    void exportCSV() noexcept;

    // AID (Auditory Information Density) Getters
    const std::array<float, 24>& getBarkBandLevels() const noexcept { return barkBandLevels; }
    float getSpectralOccupancy() const noexcept { return spectralOccupancy.load(std::memory_order_relaxed); }

    // Multiband GR Getters
    float getBandGR(int band) const noexcept
    {
        if (band < 0 || band >= 3) return 0.0f;
        return bandGR[static_cast<size_t>(band)].load(std::memory_order_relaxed);
    }

    // Thread-safe parameter atomics
    std::atomic<float> targetLUFS{ -14.0f };
    std::atomic<float> driveAmount{ 1.0f };     // 1.0 = clean (bypassed), >1.05 = saturation
    std::atomic<float> ceilingDB{ -1.0f };
    std::atomic<float> phaseRotAmount{ 0.0f };  // 0.0 = bypassed, >0.01 = active
    std::atomic<float> lowBandThresh{ -20.0f };
    std::atomic<float> midBandThresh{ -18.0f };
    std::atomic<float> highBandThresh{ -16.0f };
    std::atomic<bool>  isBypassed{ false };
    std::atomic<bool>  multibandEnabled{ true };
    std::atomic<bool>  phaseEnabled{ false };

    // VIEW button's selected display style (AID bar styles 0-4, A/B comparison
    // 5, target-overlay 6). Lives on the processor, not the editor, so it
    // survives the editor being destroyed and recreated (e.g. closing and
    // reopening the plugin popup) instead of resetting to 0 every time.
    std::atomic<int> aidViewStyle{ 1 }; // defaults to the segmented/block style

private:
    // ========================================================================
    // A-weighting filter (ANSI S1.42 / IEC 61672-1) — IIR biquad cascade via
    // bilinear transform of the standard analog pole-zero prototype:
    //   poles at f1=20.598997 Hz (x2), f2=107.65265 Hz, f3=737.86223 Hz,
    //   f4=12194.217 Hz (x2); zeros: 4x at 0 Hz. Gain normalized to 0 dB @ 1kHz
    //   (A1000 = -1.9997 dB correction baked into the leading coefficient).
    // This is a real, distinct weighting curve from K-weighting — much steeper
    // low-end rolloff — not a relabeling of the same filter.
    // ========================================================================
    struct AWeightingFilter
    {
        // Two cascaded 2nd-order sections (double pole at f1, double pole at f4)
        // plus two cascaded 1st-order sections (single real poles at f2, f3).
        juce::dsp::IIR::Filter<float> hpDouble1, hpDouble2; // f1, f1 (cascade = double pole)
        juce::dsp::IIR::Filter<float> lpDouble1, lpDouble2; // f4, f4 (cascade = double pole)
        juce::dsp::IIR::Filter<float> hpSingle1, hpSingle2; // f2, f3 (single real poles via 1st-order HP)

        void prepare(double sampleRate)
        {
            constexpr float f1 = 20.598997f, f2 = 107.65265f, f3 = 737.86223f, f4 = 12194.217f;
            auto hp1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, f1);
            auto hp2 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, f2);
            auto hp3 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, f3);
            auto lp4 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, f4);

            hpDouble1.coefficients = hp1; hpDouble2.coefficients = hp1; // f1 applied twice = double pole
            lpDouble1.coefficients = lp4; lpDouble2.coefficients = lp4; // f4 applied twice = double pole
            hpSingle1.coefficients = hp2;
            hpSingle2.coefficients = hp3;

            hpDouble1.reset(); hpDouble2.reset();
            lpDouble1.reset(); lpDouble2.reset();
            hpSingle1.reset(); hpSingle2.reset();
        }

        float processSample(float x) noexcept
        {
            // Gain-normalize so 0 dB sits at 1 kHz (A1000 correction, ~+1.9997 dB
            // makeup since the raw cascade attenuates at 1kHz by that amount).
            constexpr float makeupGain = 1.2589f; // 10^(1.9997/20)
            float y = hpDouble1.processSample(x);
            y = hpDouble2.processSample(y);
            y = hpSingle1.processSample(y);
            y = hpSingle2.processSample(y);
            y = lpDouble1.processSample(y);
            y = lpDouble2.processSample(y);
            return y * makeupGain;
        }
    };
    AWeightingFilter aWeightL, aWeightR;

    // 400ms momentary-window power accumulators, mirroring the existing
    // K-weighted ones, but fed by whichever filter the active MeasurementMode needs.
    std::vector<float> aWeightedMomBufferL, aWeightedMomBufferR;
    std::vector<float> unweightedMomBufferL, unweightedMomBufferR;
    int aWeightedMomWritePos{ 0 };
    int unweightedMomWritePos{ 0 };
    double aWeightedMomSum{ 0.0 };
    double unweightedMomSum{ 0.0 };

    std::atomic<MeasurementMode> measurementMode{ MeasurementMode::KWeightedMomentary };
    std::atomic<DbuReference> dbuReference{ DbuReference::EBU_R68 };
    std::atomic<float> currentMeasurementValue{ -100.0f };
    std::atomic<float> preProcessLevelDb{ -100.0f };  // A/B "before" reading (unweighted dBFS RMS, pre-processing)
    std::atomic<float> postProcessLevelDb{ -100.0f }; // A/B "after" reading (unweighted dBFS RMS, post-processing)

    // ========================================================================
    // VU / PPM ballistics. Both run as simple one-pole envelope followers on
    // whichever weighted signal (K or A) is selected — cheap (a handful of
    // multiply-adds per sample), no extra oversampling required.
    //   Classic VU (IEC 60268-17): 300ms time constant for both attack & release
    //     (matches the standard's 99%-in-300ms full-wave-averaging behaviour).
    //   PPM (BBC-style, IEC 60268-10 Type IIa): 1.7ms attack time constant,
    //     650ms decay time constant (calibrated to the spec's 1.5s/-20dB falloff).
    // ========================================================================
    float vuEnvelope{ 0.0f };
    float vuAttackCoeff{ 0.0f }, vuReleaseCoeff{ 0.0f };
    float ppmEnvelope{ 0.0f };
    float ppmAttackCoeff{ 0.0f }, ppmReleaseCoeff{ 0.0f };
    std::atomic<VuBallisticMode> vuBallisticMode{ VuBallisticMode::ClassicVU_K };
    std::atomic<float> currentVuReading{ -100.0f };

    static_assert(std::atomic<MeasurementMode>::is_always_lock_free, "MeasurementMode atomic must be lock-free.");
    static_assert(std::atomic<DbuReference>::is_always_lock_free, "DbuReference atomic must be lock-free.");
    static_assert(std::atomic<VuBallisticMode>::is_always_lock_free, "VuBallisticMode atomic must be lock-free.");

    // Multiband Compressor
    struct BandProcessor
    {
        juce::dsp::IIR::Filter<float> lp1, lp2;
        juce::dsp::IIR::Filter<float> hp1, hp2;
        float env{ 0.0f };
        float gain{ 1.0f };
    };
    std::array<BandProcessor, 3> bands;
    std::array<std::atomic<float>, 3> bandGR{ 0.0f, 0.0f, 0.0f };
    std::array<float, 2> crossoverFreq{ 200.0f, 2000.0f };

    // Phase Rotator
    struct AllPassStage
    {
        float a{ 0.0f };
        float z1{ 0.0f };
        float processSample(float x)
        {
            float y = a * (x - z1) + z1;
            z1 = x;
            return y;
        }
        void reset() { z1 = 0.0f; }
    };
    std::array<AllPassStage, 4> phaseRotatorL;
    std::array<AllPassStage, 4> phaseRotatorR;
    std::array<float, 4> phaseFreqs{ 60.0f, 120.0f, 240.0f, 480.0f };

    // AID Analyzer
    std::array<float, 24> barkBandLevels{ 0.0f };
    std::atomic<float> spectralOccupancy{ 0.0f };
    int aidUpdateCounter{ 0 };

    // Lookahead Limiter
    std::vector<float> lookaheadBufferL, lookaheadBufferR;
    int lookaheadWritePos{ 0 };
    int lookaheadSamples{ 0 };
    float peakEnv{ 0.0f };
    float currentGainReduction{ 1.0f };
    float smoothedMakeupGain{ 1.0f };

    // Oversampling
    juce::dsp::Oversampling<float> oversampling{
        2, false, juce::dsp::Oversampling<float>::FilterType::filterHalfBandFIREquiripple
    };

    // BS.1770 Metering State
    std::atomic<float> currentTruePeak{ -100.0f };
    std::atomic<float> heldTruePeak{ -100.0f };
    std::atomic<bool>  peakHit{ false };
    int peakHoldSamples{ 0 };
    int peakHoldDuration{ 0 };

    juce::dsp::IIR::Filter<float> kFilterL1, kFilterR1;
    juce::dsp::IIR::Filter<float> kFilterL2, kFilterR2;

    std::vector<float> momentaryBufferL, momentaryBufferR;
    int momWritePos{ 0 }, maxMomSamples{ 0 };
    double momentarySum{ 0.0 };
    std::atomic<float> currentMomentaryLUFS{ -100.0f };

    std::vector<float> shortTermBufferL, shortTermBufferR;
    int stWritePos{ 0 }, maxStSamples{ 0 };
    double shortTermSum{ 0.0 };
    std::atomic<float> currentShortTermLUFS{ -100.0f };

    double integratedSum{ 0.0 }, integratedCount{ 0.0 };
    std::atomic<float> currentIntegratedLUFS{ -100.0f };
    std::atomic<LoudnessStandard> loudnessStandard{ LoudnessStandard::Streaming }; // matches targetLUFS default of -14.0f
    static_assert(std::atomic<LoudnessStandard>::is_always_lock_free,
        "LoudnessStandard atomic must be lock-free; it's read on both the audio and message threads.");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ViaU2AudioProcessor)
};