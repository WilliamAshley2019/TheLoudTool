#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <vector>
#include <array>

class ViaU2AudioProcessor;

namespace BroadcastColour
{
    static const juce::Colour rackEar{ 0xff2a2d30 };
    static const juce::Colour rackEarLight{ 0xff4a5055 };
    static const juce::Colour rackRail{ 0xff15171a };
    static const juce::Colour chassis{ 0xff1c1e22 };
    static const juce::Colour screenBg{ 0xff050a0f };
    static const juce::Colour tealGlow{ 0xff00e6ff };
    static const juce::Colour tealDim{ 0xff005566 };
    static const juce::Colour orangeFlash{ 0xffff8c00 };
    static const juce::Colour labelText{ 0xff8899aa };
    static const juce::Colour amberGlow{ 0xffffb000 };
    static const juce::Colour oceanBlue{ 0xff0066cc }; // 80s Ocean Blue for separators
}

class RackScrew : public juce::Component
{
public:
    explicit RackScrew(float angleDeg = 0.f) : angle(juce::degreesToRadians(angleDeg)) {}
    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced(1.f);
        const float cx = b.getCentreX(), cy = b.getCentreY(), r = b.getWidth() * 0.5f;
        juce::ColourGradient rim(juce::Colour(0xff5a5a5a), cx - r * 0.3f, cy - r * 0.3f, juce::Colour(0xff1a1a1a), cx + r, cy + r, true);
        g.setGradientFill(rim); g.fillEllipse(b);
        juce::ColourGradient disc(juce::Colour(0xff3c3c3c), cx - r * 0.2f, cy - r * 0.2f, juce::Colour(0xff232323), cx + r * 0.6f, cy + r * 0.6f, true);
        g.setGradientFill(disc); g.fillEllipse(b.reduced(2.f));
        g.setColour(juce::Colour(0xff111111));
        const float slotLen = r * 0.7f, slotW = r * 0.15f, cosA = std::cos(angle), sinA = std::sin(angle);
        auto drawSlot = [&](float dx, float dy) {
            g.drawLine(cx + (-dx * slotLen) * cosA - (-dy * slotLen) * sinA,
                cy + (-dx * slotLen) * sinA + (-dy * slotLen) * cosA,
                cx + (dx * slotLen) * cosA - (dy * slotLen) * sinA,
                cy + (dx * slotLen) * sinA + (dy * slotLen) * cosA, slotW);
            };
        drawSlot(1.f, 0.f); drawSlot(0.f, 1.f);
        g.setColour(juce::Colours::white.withAlpha(0.15f)); g.drawEllipse(b.reduced(1.f), 0.7f);
    }
private:
    float angle;
};

class BroadcastKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BroadcastKnobLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, BroadcastColour::tealGlow);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a0a0a));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::Label::textColourId, BroadcastColour::labelText);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        const float cx = x + width * 0.5f, cy = y + height * 0.5f;
        const float outer = juce::jmin(width, height) * 0.5f - 2.f;
        const float capR = outer * 0.62f;

        juce::Path arcBg; arcBg.addCentredArc(cx, cy, outer - 2.f, outer - 2.f, 0.f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff1e282e)); g.strokePath(arcBg, juce::PathStrokeType(3.f));

        juce::Path arcVal;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        arcVal.addCentredArc(cx, cy, outer - 2.f, outer - 2.f, 0.f, startAngle, angle, true);
        g.setColour(BroadcastColour::tealGlow.withAlpha(0.3f));
        g.strokePath(arcVal, juce::PathStrokeType(7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(BroadcastColour::tealGlow);
        g.strokePath(arcVal, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colour(0xff3a3a3a)); g.fillEllipse(cx - outer, cy - outer, outer * 2.f, outer * 2.f);

        const int numRidges = juce::roundToInt(outer * 1.6f);
        const float ridgeLen = outer * 0.038f;
        for (int i = 0; i < numRidges; ++i)
        {
            const float a = static_cast<float>(i) / static_cast<float>(numRidges) * juce::MathConstants<float>::twoPi;
            juce::Path ridge; ridge.addRectangle(-0.45f, -outer, 0.9f, ridgeLen);
            ridge.applyTransform(juce::AffineTransform::rotation(a).translated(cx, cy));
            g.setColour(i % 2 == 0 ? juce::Colour(0xffd4d4d4) : juce::Colour(0xff606060)); g.fillPath(ridge);
        }

        juce::ColourGradient capBlend(juce::Colours::white, cx - capR * 0.35f, cy - capR * 0.4f,
            juce::Colour(0xffb8bcc0), cx + capR * 0.7f, cy + capR * 0.75f, true);
        capBlend.addColour(0.55, juce::Colour(0xffe2e4e6));
        capBlend.addColour(0.8, BroadcastColour::tealGlow.withAlpha(0.12f).overlaidWith(juce::Colour(0xffd6dadc)));
        g.setGradientFill(capBlend); g.fillEllipse(cx - capR, cy - capR, capR * 2.f, capR * 2.f);

        juce::ColourGradient sweep(juce::Colour(0x28ffffff), cx - capR * 0.6f, cy - capR * 0.7f,
            juce::Colour(0x00ffffff), cx + capR * 0.4f, cy + capR * 0.5f, false);
        g.setGradientFill(sweep); g.fillEllipse(cx - capR, cy - capR, capR * 2.f, capR * 2.f);
    }
};

class WhiteButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
        bool, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        juce::Colour baseColour = shouldDrawButtonAsDown ? BroadcastColour::orangeFlash : juce::Colour::fromRGB(240, 240, 242);
        juce::ColourGradient grad(baseColour.brighter(0.1f), bounds.getX(), bounds.getY(),
            baseColour.darker(0.15f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad); g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff888888).withAlpha(0.6f)); g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setFont(juce::FontOptions().withHeight(static_cast<float>(button.getHeight()) / 2.8f).withStyle("Bold"));
        g.setColour(button.isDown() ? juce::Colours::black : juce::Colour(0xff444444));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 2), juce::Justification::centred, 2);
    }
};

class VuMeter : public juce::Component, private juce::Timer
{
public:
    VuMeter() { startTimerHz(10); }
    void setLevel(float newLevel) { currentLevel = juce::jlimit(-60.0f, 6.0f, newLevel); repaint(); }
    void triggerPeak() { peakLED = true; peakLEDCounter = 10; repaint(); }
    void paint(juce::Graphics& g) override
    {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        drawBackground(g);
        drawDynamicElements(g);
    }
private:
    // Shared geometry so the background (ticks/arc) and the dynamic elements
    // (needle/pivot) always agree on exactly the same face, centre, and radius.
    struct Geometry
    {
        juce::Rectangle<float> face;
        float centreX, centreY, radius;
    };

    Geometry getGeometry() const
    {
        Geometry geo;
        auto bounds = getLocalBounds().toFloat();
        auto inner = bounds.reduced(6.f, 6.f);
        geo.face = inner.reduced(4.f, 4.f);
        geo.centreX = geo.face.getCentreX();
        geo.centreY = geo.face.getBottom();
        // Use much more of the available face than before (was an extra flat
        // 20% shrink on top of these limits) — increases the usable arc/needle
        // travel by roughly a third while still keeping ticks/labels clear
        // of the face edges and the "TLT" caption at the bottom.
        geo.radius = juce::jmin(geo.face.getWidth() * 0.50f, geo.face.getHeight() * 0.98f);
        return geo;
    }

    static float dbToAngle(float db)
    {
        // Display range matches a typical analog VU/PPM scale face (-20..+3).
        // currentVuReading is dBFS-referenced (0 = digital full scale), so this
        // assumes reasonably normalized program material; it isn't re-referenced
        // to a calibrated analog 0VU/PPM4 alignment level (e.g. -18dBFS=0VU).
        const float minDB = -20.0f;
        const float maxDB = 3.0f;
        const float norm = juce::jlimit(0.0f, 1.0f, (db - minDB) / (maxDB - minDB));
        const float startA = -juce::MathConstants<float>::pi * 0.35f;
        const float endA = juce::MathConstants<float>::pi * 0.35f;
        return startA + norm * (endA - startA);
    }

    void drawBackground(juce::Graphics& g);
    void drawDynamicElements(juce::Graphics& g);
    void timerCallback() override
    {
        if (peakLED && --peakLEDCounter <= 0) { peakLED = false; repaint(); }
    }
    float currentLevel = -100.0f;
    bool peakLED = false;
    int peakLEDCounter = 0;
};

class ViaU2AudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ViaU2AudioProcessorEditor(ViaU2AudioProcessor&);
    ~ViaU2AudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawChassisAndRack(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawLCDScreen(juce::Graphics& g, juce::Rectangle<float> bounds);

    ViaU2AudioProcessor& processor;

    BroadcastKnobLookAndFeel knobLAF;
    WhiteButtonLookAndFeel buttonLAF;

    RackScrew screwTL{ 15.f }, screwTR{ -20.f }, screwBL{ -10.f }, screwBR{ 25.f };

    // Buttons
    juce::TextButton resetButton{ "RESET INT" };
    juce::TextButton modeButton{ "MODE" };       // Short-term measurement type (K/A/RMS/dBu)
    juce::TextButton intButton{ "INT" };         // Integrated target standard (EBU/Streaming/ATSC/Apple/Custom)
    juce::TextButton vuButton{ "VU" };           // VU/PPM ballistic + weighting mode
    juce::TextButton exportButton{ "EXPORT CSV" };
    juce::TextButton bypassButton{ "BYPASS" };
    juce::TextButton viewButton{ "VIEW" }; // Cycles AID graph styles + A/B view

    // Knobs & Labels
    juce::Slider targetKnob, lowThreshKnob, midThreshKnob, highThreshKnob, driveKnob, ceilingKnob, phaseKnob;
    juce::Label targetLabel, lowThreshLabel, midThreshLabel, highThreshLabel, driveLabel, ceilingLabel, phaseLabel;

    VuMeter vuMeter;

    // State
    // aidViewStyle now lives on the processor (see ViaU2AudioProcessor::aidViewStyle)
    // so the VIEW button's selection survives editor close/reopen.
    juce::Array<juce::Rectangle<int>> separators; // For drawing the ocean blue dividers

    float smoothMom = -100.0f, smoothShort = -100.0f, smoothInt = -100.0f;
    float smoothPeak = -100.0f, smoothNeedleVU = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ViaU2AudioProcessorEditor)
};