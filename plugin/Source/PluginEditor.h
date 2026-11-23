// The Tetrad window, as approved in the draft (2026-11-22): the global controls on the left (mode, asgn wave, tempo,
// rect out, view, MIDI out), four channel columns A-D (a big glowing light, the slider and its read-out, mod from and
// depth, reset from, and the sine / asgn / rect output lights, each clickable to switch that output off), and Tetrad Ex
// on the right, drawn dimmed until milestone 2.
// The panel is drawn at its design size (920 x 560) and the editor scales it (80% to 200%), as on Ogdoad and Strange
// Drift. Only what moved is drawn again (Strange Drift 0.1.1).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include "PluginProcessor.h"

namespace TetradStyle
{
    inline const juce::Colour panel     { 0xff2c2c2a };
    inline const juce::Colour cardFill  { 0xff363634 };
    inline const juce::Colour exFill    { 0xff323230 };
    inline const juce::Colour knobFill  { 0xff444441 };
    inline const juce::Colour line      { 0xff5f5e5a };
    inline const juce::Colour dim       { 0xff888780 };
    inline const juce::Colour soft      { 0xffb4b2a9 };
    inline const juce::Colour text      { 0xffd3d1c7 };
    inline const juce::Colour bright    { 0xfff1efe8 };
    inline const juce::Colour amber     { 0xfffac775 };
    inline const juce::Colour track     { 0xff242422 };
    inline const juce::Colour offLight  { 0xff4a4946 };

    // How strongly a light shines for a level from 0 to 1: a faint glow at 0, clearly lit already at 30%, full at 1
    // (Ogdoad 0.2.1's curve).
    inline float lightAlpha (double level)
    {
        return 0.10f + 0.90f * (float) std::pow (juce::jlimit (0.0, 1.0, level), 0.6);
    }

    // The four channel columns: left edge of each (design coordinates) and their centres.
    inline constexpr int kColX[4] = { 160, 270, 380, 490 };
    inline constexpr int kColW = 104, kColTop = 62, kColH = 448;
    inline int colCentre (int ch) { return kColX[ch] + kColW / 2; }
    inline int outX (int ch, int which) { return colCentre (ch) + 32 * (which - 1); }   // which: 0 sine, 1 asgn, 2 rect
    inline constexpr int kLightY = 120, kOutY = 468;
}

class TetradLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TetradLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 6; }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (11.5f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.0f)); }
};

// A row (or grid) of buttons that picks one value: the mode, the asgn wave, Hz | Logic, rect out, Lights | Waves.
// Attached to a parameter, or (for the view) to a callback.
class Choices : public juce::Component
{
public:
    struct Item { juce::Rectangle<int> area; juce::String label; int icon = -1; };   // icon: an asgn wave (0-5) instead of text
    Choices (std::vector<Item> items, float fontSize);
    void attach (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void choose (int index);                       // as a click would
    void show (int index) { if (index != selected) { selected = index; repaint(); } }
    int getSelected() const { return selected; }
    std::function<void (int)> onChoose;
    int numItems() const { return (int) items.size(); }

private:
    std::vector<Item> items;
    float fontSize;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
};

// A clickable output light: switches its output on and off. The light itself is drawn by the LightsLayer.
class OutputSwitch : public juce::Button
{
public:
    OutputSwitch() : juce::Button ("output") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool, bool) override {}
};

// Everything that moves: the channel lights, the output lights, the read-outs, the mode light, the Waves view.
// Drawn over the panel, under the controls, and never takes mouse clicks.
class LightsLayer : public juce::Component, private juce::Timer
{
public:
    explicit LightsLayer (TetradProcessor& p);
    ~LightsLayer() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void setWaves (bool shouldShowWaves) { if (waves != shouldShowWaves) { waves = shouldShowWaves; repaint(); } }
    bool isWaves() const { return waves; }

    static juce::Rectangle<int> lightArea (int ch)   { return { TetradStyle::kColX[ch] + 2, 92, TetradStyle::kColW - 4, 58 }; }
    static juce::Rectangle<int> readoutArea (int ch) { return { TetradStyle::kColX[ch] + 2, 298, TetradStyle::kColW - 4, 32 }; }
    static juce::Rectangle<int> outArea (int ch, int which) { return { TetradStyle::outX (ch, which) - 15, TetradStyle::kOutY - 15, 30, 44 }; }
    static juce::Rectangle<int> modeLightArea()      { return { 114, 58, 28, 24 }; }
    static juce::Rectangle<int> tempoNoteArea()      { return { 60, 286, 76, 16 }; }

    int framesDrawn = 0;
    double areaDrawn = 0.0;                  // for the offline checks: total area drawn again, in panel pixels

private:
    void timerCallback() override;
    juce::String readout (int ch, juce::String& under) const;
    TetradProcessor& proc;
    bool waves = false;
    TetradProcessor::Traces traces {};
    std::array<int, 4> drawnLight {};
    std::array<std::array<int, 3>, 4> drawnOut {};
    std::array<juce::String, 4> drawnReadout;
    std::array<int, 4> drawnMarker { -2, -2, -2, -2 };
    bool drawnBlink = false, drawnMissing = false, drawnWaves = false, firstFrame = true;
    double blinkUntil = 0.0;
    int drawnMode = -1;
};

// The whole panel at its design size (920 x 560). The editor below scales it to the window.
class TetradPanel : public juce::Component, private juce::ValueTree::Listener
{
public:
    explicit TetradPanel (TetradProcessor&);
    ~TetradPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the offline checks: every control and the parameter it moves.
    struct ControlInfo { juce::Component* control; juce::String paramId; };
    std::vector<ControlInfo> controls();

    // Lights | Waves: what the channel lights show. Saved with the project (not an audio parameter).
    Choices viewChoices;
    bool isWavesView() const { return lights.isWaves(); }
    const LightsLayer& lightsLayer() const { return lights; }
    static inline const juce::Identifier wavesViewProperty { "wavesView" };

    static constexpr int kWidth = 920, kHeight = 560;      // the design size; everything is laid out at this size

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void showView (bool waves);
    void paintEx (juce::Graphics&);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    TetradProcessor& proc;
    TetradLookAndFeel lnf;
    LightsLayer lights;

    Choices modeChoices, waveChoices, tempoChoices, rectOutChoices;
    std::array<juce::Slider, 4> sliders, depthKnobs;
    std::array<juce::ComboBox, 4> modFromBoxes, resetFromBoxes;
    std::array<std::array<OutputSwitch, 3>, 4> outSwitches;
    juce::ComboBox channelBox;

    std::array<std::unique_ptr<SliderAttachment>, 4> sliderAtt, depthAtt;
    std::array<std::unique_ptr<ComboAttachment>, 4> modFromAtt, resetFromAtt;
    std::array<std::array<std::unique_ptr<ButtonAttachment>, 3>, 4> outAtt;
    std::unique_ptr<ComboAttachment> channelAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TetradPanel)
};

// The plugin window: resizable between 80% and 200% of the design size, proportions locked. The panel is drawn at its
// design size and scaled to fit, so every control, light and letter grows together. The width is saved with the project.
class TetradEditor : public juce::AudioProcessorEditor
{
public:
    explicit TetradEditor (TetradProcessor&);
    ~TetradEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::vector<TetradPanel::ControlInfo> controls() { return panel.controls(); }
    bool isWavesView() const { return panel.isWavesView(); }

    static constexpr int kWidth = TetradPanel::kWidth, kHeight = TetradPanel::kHeight;
    static constexpr int kMinWidth = 736, kMaxWidth = 1840;     // 80% and 200%
    static inline const juce::Identifier widthProperty { "editorWidth" };
    static int heightForWidth (int width) { return juce::roundToInt ((double) width * kHeight / kWidth); }

    TetradPanel panel;

private:
    TetradProcessor& proc;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TetradEditor)
};
