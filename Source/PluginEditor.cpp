#include "PluginEditor.h"
#include "PluginProcessor.h"

// ==============================================================================
// VuMeter Implementation (Pivot on Bottom Edge)
// ==============================================================================
void VuMeter::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // --- Outer bezel ---
    juce::Path outerBezel;
    outerBezel.addRoundedRectangle(bounds, 8.f);
    juce::ColourGradient bezelGrad(juce::Colour(0xff1a1a1a), bounds.getX(), bounds.getY(),
        juce::Colour(0xff0a0a0a), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bezelGrad);
    g.fillPath(outerBezel);
    g.setColour(juce::Colours::black);
    g.strokePath(outerBezel, juce::PathStrokeType(2.0f));

    auto inner = bounds.reduced(6.f, 6.f);
    g.setColour(juce::Colour(0xff0d0d0d));
    g.fillRoundedRectangle(inner, 4.f);

    auto face = inner.reduced(4.f, 4.f);
    g.setColour(juce::Colour(0xfffdfbf0));
    g.fillRoundedRectangle(face, 3.f);
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.drawRoundedRectangle(face, 3.f, 1.5f);

    // --- Geometry: pivot at bottom edge of face (shared with drawDynamicElements) ---
    auto geo = getGeometry();
    auto centreX = geo.centreX;
    auto centreY = geo.centreY;
    auto radius = geo.radius;

    // --- Subtle arc behind needle ---
    juce::Path arcPath;
    arcPath.addCentredArc(centreX, centreY, radius * 0.95f, radius * 0.95f, 0.0f,
        dbToAngle(-20.0f), dbToAngle(3.0f), true);
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.strokePath(arcPath, juce::PathStrokeType(radius * 0.04f, juce::PathStrokeType::curved));

    // --- Tick marks and labels ---
    struct Tick { float db; bool major; bool isRed; };
    std::vector<Tick> ticks = {
        { -20.0f, true, false },
        { -10.0f, true, false },
        {  -7.0f, false, false },
        {  -5.0f, true, false },
        {  -3.0f, false, false },
        {   0.0f, true, false },
        {   3.0f, true, true }
    };

    const float innerRadius = radius * 0.78f;
    const float outerRadius = radius * 0.90f;
    const float textRadius = radius * 0.97f;

    g.setColour(juce::Colours::black);
    juce::Font tickFont(juce::FontOptions().withHeight(juce::jmax(7.0f, radius * 0.07f)).withStyle("Bold"));

    for (const auto& tick : ticks)
    {
        const float angle = dbToAngle(tick.db);
        const float sinA = std::sin(angle);
        const float cosA = std::cos(angle);

        // Draw tick line
        const float startX = centreX + innerRadius * sinA;
        const float startY = centreY - innerRadius * cosA;
        const float endX = centreX + outerRadius * sinA;
        const float endY = centreY - outerRadius * cosA;

        g.setColour(tick.isRed ? juce::Colours::red : juce::Colours::black);
        const float thickness = tick.major ? 2.0f : 1.2f;
        g.drawLine(startX, startY, endX, endY, thickness);

        // Draw label only for major ticks
        if (tick.major)
        {
            const float labelX = centreX + textRadius * sinA;
            const float labelY = centreY - textRadius * cosA;

            juce::String text;
            if (tick.db == 0.0f) text = "0";
            else if (tick.db > 0.0f) text = "+" + juce::String(juce::roundToInt(tick.db));
            else text = juce::String(juce::roundToInt(tick.db));

            const float labelWidth = juce::jmax(14.0f, tickFont.getStringWidth(text) + 4.0f);
            const float labelHeight = tickFont.getHeight() + 2.0f;
            const float labelLeft = labelX - labelWidth * 0.5f;
            const float labelTop = labelY - labelHeight * 0.5f;

            g.setColour(juce::Colours::black);
            g.setFont(tickFont);
            g.drawText(text,
                juce::Rectangle<float>(labelLeft, labelTop, labelWidth, labelHeight),
                juce::Justification::centred, false);
        }
    }

    // --- Bottom label "TLT" (placed just above the pivot, scaled with radius) ---
    g.setColour(juce::Colours::black);
    juce::Font bottomFont(juce::FontOptions().withHeight(juce::jmax(11.0f, radius * 0.10f)).withStyle("Bold"));
    g.setFont(bottomFont);
    const float labelW = juce::jmax(50.0f, radius * 0.5f);
    const float labelH = bottomFont.getHeight() + 6.0f;
    g.drawText("TLT", centreX - labelW * 0.5f, centreY - labelH - 4.0f, labelW, labelH, juce::Justification::centred);

    // --- Glass reflection ---
    juce::ColourGradient glass(juce::Colours::white.withAlpha(0.15f), face.getX(), face.getY(),
        juce::Colours::white.withAlpha(0.0f), face.getX(), face.getCentreY(), false);
    g.setGradientFill(glass);
    g.fillRoundedRectangle(face, 3.f);
}

void VuMeter::drawDynamicElements(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto geo = getGeometry();
    auto centreX = geo.centreX;
    auto centreY = geo.centreY;
    auto radius = geo.radius;

    // --- Needle (starts at pivot on bottom edge) ---
    const float angle = dbToAngle(currentLevel);
    const float pointerLength = radius * 0.92f;   // matches the tick arc reach
    const float endX = centreX + pointerLength * std::sin(angle);
    const float endY = centreY - pointerLength * std::cos(angle);

    // shadow
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawLine(centreX + 2, centreY + 2, endX + 2, endY + 2, 2.5f);

    // main needle
    g.setColour(juce::Colour(0xffb01010));
    g.drawLine(centreX, centreY, endX, endY, 2.0f);

    // --- Pivot (small circle at the bottom edge) ---
    g.setColour(juce::Colours::black);
    g.fillEllipse(centreX - 6, centreY - 6, 12, 12);
    juce::ColourGradient pivotGrad(juce::Colour(0xff3a3a3a), centreX - 4, centreY - 4,
        juce::Colours::black, centreX + 4, centreY + 4, true);
    g.setGradientFill(pivotGrad);
    g.fillEllipse(centreX - 4, centreY - 4, 8, 8);

    // --- Peak LED ---
    const float ledSize = 12.0f;
    const float ledX = bounds.getRight() - ledSize - 10.0f;
    const float ledY = 12.0f;
    if (peakLED)
    {
        g.setColour(juce::Colours::red.withAlpha(0.3f));
        g.fillEllipse(ledX - 4, ledY - 4, ledSize + 8, ledSize + 8);
        g.setColour(juce::Colours::red);
    }
    else
    {
        g.setColour(juce::Colour(0xff440000));
    }
    g.fillEllipse(ledX, ledY, ledSize, ledSize);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.fillEllipse(ledX + 2, ledY + 2, ledSize * 0.4f, ledSize * 0.4f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions().withHeight(9.0f).withStyle("Bold"));
    g.drawText("PEAK", ledX - 34, ledY + 2, 28, 10, juce::Justification::centredRight);
}

// ==============================================================================
// Main Editor Implementation (unchanged from previous)
// ==============================================================================
ViaU2AudioProcessorEditor::ViaU2AudioProcessorEditor(ViaU2AudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(850, 420);
    setResizable(true, true);
    setResizeLimits(750, 380, 1400, 600);

    for (auto* s : { &screwTL, &screwTR, &screwBL, &screwBR }) addAndMakeVisible(s);

    for (auto* btn : { &resetButton, &modeButton, &intButton, &vuButton, &exportButton, &bypassButton, &viewButton })
    {
        btn->setLookAndFeel(&buttonLAF);
        addAndMakeVisible(btn);
    }
    resetButton.onClick = [this]() { processor.resetIntegratedLoudness(); };
    modeButton.onClick = [this]() { processor.cycleMeasurementMode(); };
    intButton.onClick = [this]() { processor.cycleIntegratedStandard(); };
    vuButton.onClick = [this]() { processor.cycleVuBallisticMode(); };
    exportButton.onClick = [this]() { processor.exportCSV(); };
    bypassButton.onClick = [this]() {
        bool state = !processor.isBypassed.load();
        processor.isBypassed.store(state);
        bypassButton.setButtonText(state ? "ENGAGED" : "BYPASS");
        };
    bypassButton.setButtonText(processor.isBypassed.load() ? "ENGAGED" : "BYPASS");
    viewButton.onClick = [this]() {
        // 5 AID bar-graph styles (0-4) + A/B before/after (5) + target overlay (6)
        int next = (processor.aidViewStyle.load(std::memory_order_relaxed) + 1) % 7;
        processor.aidViewStyle.store(next, std::memory_order_relaxed);
        repaint();
        };

    auto setupKnob = [this](juce::Slider& knob, juce::Label& label, const juce::String& name,
        double min, double max, double currentValue)
        {
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            knob.setRange(min, max, 0.1);
            // dontSendNotification: this is a one-time sync from the processor's
            // live, persistent value (read by the caller below) — it must NOT
            // fire onValueChange, or every editor reopen would re-store the
            // value right back into the processor (harmless here since it's
            // the same value, but onValueChange is wired up further down and
            // we don't want construction-time churn triggering it before the
            // lambda even exists).
            knob.setValue(currentValue, juce::dontSendNotification);
            knob.setLookAndFeel(&knobLAF);
            addAndMakeVisible(knob);
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::FontOptions().withName("Courier New").withStyle("Bold").withHeight(10.0f));
            label.setColour(juce::Label::textColourId, BroadcastColour::labelText);
            addAndMakeVisible(label);
        };

    // Read the processor's CURRENT live values (set previously by the user,
    // or the processor's own construction-time defaults on first load) rather
    // than hardcoding factory defaults here — otherwise every time the editor
    // is destroyed and recreated (e.g. closing/reopening the plugin popup in
    // FL Studio's mixer) it would silently snap all parameters back to factory
    // defaults, audibly changing the sound mid-session.
    setupKnob(targetKnob, targetLabel, "TARGET", -30.0, -8.0, static_cast<double>(processor.targetLUFS.load()));
    setupKnob(lowThreshKnob, lowThreshLabel, "LOW", -60.0, 0.0, static_cast<double>(processor.lowBandThresh.load()));
    setupKnob(midThreshKnob, midThreshLabel, "MID", -60.0, 0.0, static_cast<double>(processor.midBandThresh.load()));
    setupKnob(highThreshKnob, highThreshLabel, "HIGH", -60.0, 0.0, static_cast<double>(processor.highBandThresh.load()));
    setupKnob(driveKnob, driveLabel, "TEXTURE", 1.0, 5.0, static_cast<double>(processor.driveAmount.load()));
    setupKnob(ceilingKnob, ceilingLabel, "CEILING", -10.0, 0.0, static_cast<double>(processor.ceilingDB.load()));
    setupKnob(phaseKnob, phaseLabel, "PHASE", 0.0, 1.0, static_cast<double>(processor.phaseRotAmount.load()));

    targetKnob.onValueChange = [this]() {
        processor.targetLUFS.store(static_cast<float>(targetKnob.getValue()));
        processor.notifyTargetKnobMovedManually();
        };
    lowThreshKnob.onValueChange = [this]() { processor.lowBandThresh.store(static_cast<float>(lowThreshKnob.getValue())); };
    midThreshKnob.onValueChange = [this]() { processor.midBandThresh.store(static_cast<float>(midThreshKnob.getValue())); };
    highThreshKnob.onValueChange = [this]() { processor.highBandThresh.store(static_cast<float>(highThreshKnob.getValue())); };
    driveKnob.onValueChange = [this]() { processor.driveAmount.store(static_cast<float>(driveKnob.getValue())); };
    ceilingKnob.onValueChange = [this]() { processor.ceilingDB.store(static_cast<float>(ceilingKnob.getValue())); };
    phaseKnob.onValueChange = [this]() { processor.phaseRotAmount.store(static_cast<float>(phaseKnob.getValue())); };

    addAndMakeVisible(vuMeter);
    startTimerHz(30);
}

ViaU2AudioProcessorEditor::~ViaU2AudioProcessorEditor()
{
    for (auto* btn : { &resetButton, &modeButton, &intButton, &vuButton, &exportButton, &bypassButton, &viewButton }) btn->setLookAndFeel(nullptr);
    targetKnob.setLookAndFeel(nullptr); lowThreshKnob.setLookAndFeel(nullptr); midThreshKnob.setLookAndFeel(nullptr);
    highThreshKnob.setLookAndFeel(nullptr); driveKnob.setLookAndFeel(nullptr); ceilingKnob.setLookAndFeel(nullptr);
    phaseKnob.setLookAndFeel(nullptr);
}

void ViaU2AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const int earW = 34;
    const int railH = 16;

    screwTL.setBounds(earW - 24, railH + 10, 16, 16);
    screwTR.setBounds(bounds.getWidth() - earW + 8, railH + 10, 16, 16);
    screwBL.setBounds(earW - 24, bounds.getHeight() - railH - 26, 16, 16);
    screwBR.setBounds(bounds.getWidth() - earW + 8, bounds.getHeight() - railH - 26, 16, 16);

    auto panel = bounds.reduced(earW, railH + 8);

    auto lcdArea = panel.removeFromTop(panel.getHeight() * 0.50f).reduced(8);

    auto buttonRow = panel.removeFromTop(34).reduced(4, 2);
    int btnW = buttonRow.getWidth() / 7;
    resetButton.setBounds(buttonRow.removeFromLeft(btnW).reduced(2));
    modeButton.setBounds(buttonRow.removeFromLeft(btnW).reduced(2));
    intButton.setBounds(buttonRow.removeFromLeft(btnW).reduced(2));
    vuButton.setBounds(buttonRow.removeFromLeft(btnW).reduced(2));
    exportButton.setBounds(buttonRow.removeFromLeft(btnW).reduced(2));
    bypassButton.setBounds(buttonRow.removeFromLeft(btnW).reduced(2));
    viewButton.setBounds(buttonRow.reduced(2));

    panel.removeFromTop(8);

    auto bottomArea = panel;
    auto vuArea = bottomArea.removeFromRight(bottomArea.getWidth() * 0.38f);

    int vuWidth = vuArea.getWidth();
    int vuHeight = vuWidth / 2;
    if (vuHeight > vuArea.getHeight()) { vuHeight = vuArea.getHeight(); vuWidth = vuHeight * 2; }
    vuMeter.setBounds(vuArea.withSizeKeepingCentre(vuWidth, vuHeight).toNearestInt());

    separators.clear();
    auto knobArea = bottomArea.reduced(10, 4);
    int knobSize = juce::jmin(100, knobArea.getHeight() - 20);
    int gapSize = knobSize / 5;
    int sepSize = 8;

    int totalW = 7 * knobSize + 2 * sepSize + 10 * gapSize;
    float scale = static_cast<float>(knobArea.getWidth()) / static_cast<float>(totalW);
    if (scale < 1.0f) {
        knobSize = static_cast<int>(knobSize * scale);
        gapSize = static_cast<int>(gapSize * scale);
        sepSize = static_cast<int>(sepSize * scale);
        totalW = 7 * knobSize + 2 * sepSize + 10 * gapSize;
    }

    int startX = knobArea.getX() + (knobArea.getWidth() - totalW) / 2;
    int knobY = knobArea.getY() + (knobArea.getHeight() - (knobSize + 16)) / 2;
    int x = startX;

    auto placeKnob = [&](juce::Slider& k, juce::Label& l) {
        x += gapSize;
        k.setBounds(x, knobY, knobSize, knobSize);
        l.setBounds(x, knobY + knobSize, knobSize, 16);
        x += knobSize;
        };

    auto placeSep = [&]() {
        x += gapSize;
        separators.add(juce::Rectangle<int>(x, knobY, sepSize, knobSize + 16));
        x += sepSize;
        };

    placeKnob(targetKnob, targetLabel);
    placeSep();
    placeKnob(lowThreshKnob, lowThreshLabel);
    placeKnob(midThreshKnob, midThreshLabel);
    placeKnob(highThreshKnob, highThreshLabel);
    placeSep();
    placeKnob(driveKnob, driveLabel);
    placeKnob(ceilingKnob, ceilingLabel);
    placeKnob(phaseKnob, phaseLabel);
}

void ViaU2AudioProcessorEditor::timerCallback()
{
    auto smooth = [](float curr, float target, float coeff) { return curr + coeff * (target - curr); };
    smoothMom = smooth(smoothMom, processor.getCurrentMomentaryLUFS(), 0.4f);
    smoothShort = smooth(smoothShort, processor.getCurrentShortTermLUFS(), 0.2f);
    smoothInt = smooth(smoothInt, processor.getCurrentIntegratedLUFS(), 0.05f);
    smoothPeak = smooth(smoothPeak, processor.getCurrentTruePeak(), 0.6f);

    // The needle is fed directly from the processor's VU/PPM ballistic
    // envelope (computed at audio rate with the correct attack/release time
    // constants for whichever mode is active) rather than smoothing
    // Short-Term LUFS in the UI — that ballistic IS the needle's correct
    // motion; adding UI-side smoothing on top would blur PPM's fast 1.7ms
    // attack and misrepresent both ballistic laws.
    smoothNeedleVU = processor.getCurrentVuReading();

    // Keep the knob's visual position in sync when MODE changes the target
    // programmatically (dontSendNotification avoids re-flagging as Custom).
    const double liveTarget = static_cast<double>(processor.targetLUFS.load(std::memory_order_relaxed));
    if (std::abs(targetKnob.getValue() - liveTarget) > 0.001)
        targetKnob.setValue(liveTarget, juce::dontSendNotification);

    vuMeter.setLevel(smoothNeedleVU);
    if (smoothPeak > 0.0f) vuMeter.triggerPeak();
    repaint();
}

void ViaU2AudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawChassisAndRack(g, bounds);

    const float earW = 34.f, railH = 16.f;
    auto panel = bounds.reduced(earW, railH + 8.f);
    auto lcdArea = panel.removeFromTop(panel.getHeight() * 0.50f).reduced(8.f);
    drawLCDScreen(g, lcdArea);

    for (auto& sep : separators) {
        juce::ColourGradient sepGrad(juce::Colour(0xff004488), static_cast<float>(sep.getX()), static_cast<float>(sep.getY()),
            juce::Colour(0xff0088ff), static_cast<float>(sep.getRight()), static_cast<float>(sep.getBottom()), false);
        g.setGradientFill(sepGrad);
        g.fillRoundedRectangle(sep.toFloat(), 2.0f);
        g.setColour(juce::Colour(0x6600aaff));
        g.drawRoundedRectangle(sep.toFloat().expanded(1.0f), 2.0f, 1.0f);
    }
}

void ViaU2AudioProcessorEditor::drawChassisAndRack(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float W = bounds.getWidth(), H = bounds.getHeight(), earW = 34.f, railH = 16.f;
    g.setColour(BroadcastColour::chassis); g.fillRoundedRectangle(bounds, 4.f);
    juce::ColourGradient leftEar(BroadcastColour::rackEarLight, 0.f, 0.f, BroadcastColour::rackEar, earW, 0.f, false);
    g.setGradientFill(leftEar); g.fillRoundedRectangle(0.f, 0.f, earW, H, 4.f);
    g.setColour(BroadcastColour::rackEar); g.fillRect(W - earW, 0.f, earW, H);
    g.setColour(BroadcastColour::rackRail);
    g.fillRect(earW, 0.f, W - 2.f * earW, railH);
    g.fillRect(earW, H - railH, W - 2.f * earW, railH);
    for (float yy = 2.f; yy < railH - 1.f; yy += 3.f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.fillRect(earW, yy, W - 2.f * earW, 1.f);
        g.fillRect(earW, H - railH + yy, W - 2.f * earW, 1.f);
    }
}

void ViaU2AudioProcessorEditor::drawLCDScreen(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::Path bezelPath; bezelPath.addRoundedRectangle(bounds, 12.f);
    juce::ColourGradient bezelGrad(juce::Colour(0xff2a2a2a), bounds.getX(), bounds.getY(),
        juce::Colour(0xff050505), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bezelGrad); g.fillPath(bezelPath);
    g.setColour(juce::Colours::black); g.strokePath(bezelPath, juce::PathStrokeType(2.0f));

    auto lcd = bounds.reduced(12.f, 8.f);
    g.setColour(BroadcastColour::screenBg); g.fillRect(lcd);
    g.setColour(juce::Colours::black.withAlpha(0.8f)); g.drawRect(lcd, 3);

    auto content = lcd.reduced(10.f, 8.f);
    auto fontDigital = juce::FontOptions().withName("Courier New").withStyle("Bold");
    auto drawTealText = [&](const juce::String& text, juce::Rectangle<float> area, float height,
        juce::Justification just, bool active = true)
        {
            g.setFont(fontDigital.withHeight(height));
            g.setColour(BroadcastColour::tealGlow.withAlpha(0.2f));
            g.drawFittedText(text, area.expanded(2).toNearestInt(), just, 1);
            g.setColour(active ? BroadcastColour::tealGlow : BroadcastColour::tealDim);
            g.drawFittedText(text, area.toNearestInt(), just, 1);
        };

    auto topRow = content.removeFromTop(20.f);
    drawTealText("VIAU-MAX v2", topRow.removeFromLeft(120.f), 13.f, juce::Justification::centredLeft);
    juce::String intText = processor.getTargetName() + "  TGT " + juce::String(processor.targetLUFS.load(), 1);
    drawTealText(intText, topRow, 13.f, juce::Justification::centredRight);

    auto subRow = content.removeFromTop(16.f);
    juce::String modeVuText = processor.getMeasurementModeName() + "   " + processor.getVuBallisticModeName();
    drawTealText(modeVuText, subRow, 11.f, juce::Justification::centredRight);

    content.removeFromTop(8.f);
    auto readoutArea = content.removeFromTop(content.getHeight() * 0.45f);
    float colW = readoutArea.getWidth() / 3.f;
    auto drawReadout = [&](juce::Rectangle<float> area, const juce::String& label, float val, bool useTargetColour = false)
        {
            drawTealText(label, area.removeFromTop(16.f), 12.f, juce::Justification::centred);
            juce::String str = (val <= -90.f) ? "-inf" : juce::String(val, 1);

            if (useTargetColour && val > -90.f)
            {
                const float target = processor.targetLUFS.load();
                const float diff = std::abs(val - target);
                juce::Colour readoutColour = diff <= 1.0f ? juce::Colour(0xff30e060)
                    : diff <= 3.0f ? BroadcastColour::tealGlow
                    : juce::Colour(0xffff5040);

                g.setFont(fontDigital.withHeight(34.f));
                g.setColour(readoutColour.withAlpha(0.2f));
                g.drawFittedText(str, area.expanded(2).toNearestInt(), juce::Justification::centred, 1);
                g.setColour(readoutColour);
                g.drawFittedText(str, area.toNearestInt(), juce::Justification::centred, 1);
            }
            else
            {
                drawTealText(str, area, 34.f, juce::Justification::centred);
            }
        };
    drawReadout(readoutArea.removeFromLeft(colW), "INTEGRATED", smoothInt, true);
    drawReadout(readoutArea.removeFromLeft(colW), "SHORT-TERM", smoothShort);
    drawReadout(readoutArea, "MOMENTARY", smoothMom);

    content.removeFromTop(4.f);
    auto bottomRow = content.removeFromTop(16.f);
    juce::String peakStr = (smoothPeak <= -90.f) ? "-inf" : juce::String(smoothPeak, 1) + " dBTP";
    juce::String measStr = juce::String(processor.getCurrentMeasurement(), 1);
    drawTealText("TP " + peakStr + "   " + processor.getMeasurementModeName() + " " + measStr,
        bottomRow, 12.f, juce::Justification::centredLeft);

    // VIEW button cycles 7 display styles:
    //   0-4: AID Bark-band bar graph (solid / segmented-block / outline-fill / light-blue / rainbow)
    //   5:   A/B before-and-after comparison (horizontal dual bars)
    //   6:   Wet signal vs. target overlay (current output level against the active target LUFS)
    auto graphArea = content;
    const int viewStyle = processor.aidViewStyle.load(std::memory_order_relaxed);

    if (viewStyle == 5)
    {
        // --- A/B Before & After comparison view (horizontal bars, clearer at LCD size) ---
        const float before = processor.getPreProcessLevel();
        const float after = processor.getPostProcessLevel();
        const float deltaDb = (before <= -90.f || after <= -90.f) ? 0.0f : (after - before);

        auto abArea = graphArea.reduced(4.f, 2.f);
        auto headerRow = abArea.removeFromTop(14.f);
        drawTealText("A/B COMPARISON", headerRow.removeFromLeft(headerRow.getWidth() * 0.6f), 10.f, juce::Justification::centredLeft);
        juce::String deltaStr = (deltaDb == 0.0f) ? "" : (deltaDb > 0.0f ? "+" : "") + juce::String(deltaDb, 1) + " dB GR/GAIN";
        drawTealText(deltaStr, headerRow, 10.f, juce::Justification::centredRight);

        abArea.removeFromTop(4.f);

        // Horizontal bar layout: label column, then a wide bar, then numeric readout
        auto drawHorizBar = [&](juce::Rectangle<float> rowArea, const juce::String& caption, float levelDb)
            {
                auto labelCol = rowArea.removeFromLeft(60.f);
                auto valueCol = rowArea.removeFromRight(60.f);
                auto barCol = rowArea.reduced(4.f, 6.f);

                drawTealText(caption, labelCol, 11.f, juce::Justification::centredLeft);

                const float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
                const float w = norm * barCol.getWidth();

                g.setColour(BroadcastColour::tealGlow.withAlpha(0.15f));
                g.fillRect(barCol);
                g.setColour(BroadcastColour::tealGlow.withAlpha(0.85f));
                int segW = 4;
                for (float sx = barCol.getX(); sx < barCol.getX() + w; sx += (segW + 1))
                    g.fillRect(sx, barCol.getY(), static_cast<float>(segW), barCol.getHeight());

                juce::String valStr = (levelDb <= -90.f) ? "-inf" : juce::String(levelDb, 1) + " dB";
                drawTealText(valStr, valueCol, 11.f, juce::Justification::centredRight);
            };

        auto rowH = abArea.getHeight() * 0.5f;
        drawHorizBar(abArea.removeFromTop(rowH).reduced(0.f, 2.f), "BEFORE", before);
        drawHorizBar(abArea.reduced(0.f, 2.f), "AFTER", after);
    }
    else if (viewStyle == 6)
    {
        // --- Wet (post-processing) signal vs. Target overlay ---
        // Shows the current output level as a bar, with a marker line at the
        // active target LUFS so the user can see at a glance how far off
        // (and in which direction) the processed signal sits from target.
        const float wetLevel = smoothInt; // Integrated LUFS of the current output — the most relevant "where am I" number
        const float target = processor.targetLUFS.load();

        auto ovArea = graphArea.reduced(4.f, 2.f);
        auto headerRow = ovArea.removeFromTop(14.f);
        drawTealText("OUTPUT vs TARGET", headerRow.removeFromLeft(headerRow.getWidth() * 0.6f), 10.f, juce::Justification::centredLeft);
        juce::String diffStr = (wetLevel <= -90.f) ? "" : ((wetLevel >= target ? "+" : "") + juce::String(wetLevel - target, 1) + " LU");
        drawTealText(diffStr, headerRow, 10.f, juce::Justification::centredRight);

        ovArea.removeFromTop(6.f);

        // Scale: -40 LUFS (left) to 0 LUFS (right) covers the practical range
        // of both the target standards and typical program material.
        const float scaleMin = -40.0f, scaleMax = 0.0f;
        auto barArea = ovArea.removeFromTop(ovArea.getHeight() * 0.55f);

        auto dbToX = [&](float db) {
            const float norm = juce::jlimit(0.0f, 1.0f, (db - scaleMin) / (scaleMax - scaleMin));
            return barArea.getX() + norm * barArea.getWidth();
            };

        // Background track
        g.setColour(BroadcastColour::tealGlow.withAlpha(0.12f));
        g.fillRect(barArea);

        // Wet signal block-style fill from left up to current level
        if (wetLevel > -90.f)
        {
            const float wetX = dbToX(wetLevel);
            g.setColour(BroadcastColour::tealGlow.withAlpha(0.8f));
            int segW = 4;
            for (float sx = barArea.getX(); sx < wetX; sx += (segW + 1))
                g.fillRect(sx, barArea.getY(), static_cast<float>(segW), barArea.getHeight());
        }

        // Target marker: a bright vertical line at the target LUFS position
        const float targetX = dbToX(target);
        g.setColour(juce::Colour(0xffffb000)); // amber, distinct from teal fill
        g.fillRect(targetX - 1.0f, barArea.getY() - 4.f, 2.0f, barArea.getHeight() + 8.f);

        ovArea.removeFromTop(barArea.getHeight() + 4.f);

        // Scale labels
        auto scaleRow = ovArea.removeFromTop(12.f);
        drawTealText(juce::String(static_cast<int>(scaleMin)), scaleRow.removeFromLeft(40.f), 9.f, juce::Justification::centredLeft);
        drawTealText("TARGET " + juce::String(target, 1), scaleRow, 9.f, juce::Justification::centred);
        drawTealText(juce::String(static_cast<int>(scaleMax)), scaleRow.removeFromRight(40.f), 9.f, juce::Justification::centredRight);

        // Numeric current reading
        ovArea.removeFromTop(4.f);
        juce::String wetStr = (wetLevel <= -90.f) ? "-inf" : juce::String(wetLevel, 1) + " LUFS";
        drawTealText("CURRENT: " + wetStr, ovArea, 13.f, juce::Justification::centred);
    }
    else
    {
        float barW = graphArea.getWidth() / 24.0f - 1.0f;
        float maxLvl = 0.0001f;
        auto barkLevels = processor.getBarkBandLevels();
        for (auto l : barkLevels) maxLvl = juce::jmax(maxLvl, l);

        for (int i = 0; i < 24; ++i) {
            float norm = barkLevels[static_cast<size_t>(i)] / maxLvl;
            float h = norm * graphArea.getHeight();
            float x = graphArea.getX() + i * (barW + 1.0f);
            float y = graphArea.getBottom() - h;

            switch (viewStyle) {
            case 0:
                g.setColour(BroadcastColour::tealGlow.withAlpha(0.8f));
                g.fillRect(x, y, barW, h);
                break;
            case 1:
                g.setColour(BroadcastColour::tealGlow.withAlpha(0.8f));
                {
                    int segH = 3;
                    for (float sy = graphArea.getBottom(); sy > y; sy -= (segH + 1)) {
                        g.fillRect(x, sy - segH, barW, static_cast<float>(segH));
                    }
                }
                break;
            case 2:
                g.setColour(BroadcastColour::tealGlow.withAlpha(0.2f));
                g.fillRect(x, graphArea.getY(), barW, graphArea.getHeight());
                g.setColour(BroadcastColour::tealGlow);
                g.fillRect(x, y, barW, h);
                break;
            case 3:
                g.setColour(juce::Colours::lightblue);
                g.fillRect(x, y, barW, h);
                break;
            case 4:
            {
                float hue = static_cast<float>(i) / 24.0f;
                g.setColour(juce::Colour::fromHSV(hue, 0.8f, 0.6f, 0.9f));
                g.fillRect(x, y, barW, h);
            }
            break;
            default: break;
            }
        }
    }

    for (float yy = lcd.getY(); yy < lcd.getBottom(); yy += 2.f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRect(lcd.getX(), yy, lcd.getWidth(), 1.f);
    }
    juce::ColourGradient bloom(BroadcastColour::tealGlow.withAlpha(0.05f), lcd.getCentreX(), lcd.getCentreY(),
        BroadcastColour::tealGlow.withAlpha(0.f), lcd.getRight(), lcd.getBottom(), true);
    g.setGradientFill(bloom); g.fillRect(lcd);
}