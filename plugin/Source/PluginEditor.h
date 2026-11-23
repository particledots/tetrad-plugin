// The Tetrad window, as approved in the draft (2026-11-22): the global controls on the left (mode, asgn wave, tempo,
// rect out, view, MIDI out), four channel columns A-D (a big glowing light, the slider and its read-out, mod from and
// depth, reset from, and the sine / asgn / rect output lights, each clickable to switch that output off), and Tetrad Ex
// on the right (live from 0.2.0: an on switch, and per channel sine level, asgn level, shape, its own wave, shape from
// and fold / wave / width; dimmed while Ex is off).
// The panel is drawn at its design size (920 x 560) and the editor scales it (80% to 200%), as on Ogdoad and Strange
// Drift. Only what moved is drawn again (Strange Drift 0.1.1).
//
// Cheaper drawing (0.2.0; 0.1.0's open window cost about 10 points of Logic's CPU meter on the Intel Mac):
//   - macOS merges all the areas asked to be redrawn before one screen refresh into one rectangle around them all. In
//     0.1.0 the big lights (top) and the output lights (bottom) changed together, so nearly the whole window was
//     redrawn 30 times a second. From 0.2.0 the two rows take turns on alternate screen refreshes (each still about
//     30 times a second), so each redraw is one thin row;
//   - each light is its own small component painting its own patch, so only those patches are drawn;
//   - everything that never moves (labels, cards, lines, the output names and CC numbers) is drawn once into a picture
//     (PanelBackground, kept by JUCE at the window's real pixel size);
//   - each kind of light is drawn once into small pictures (its glow and its core) at the window's real pixel size,
//     and every frame only copies them at the right brightness, instead of working out soft circles again;
//   - a light is redrawn only when its brightness moves by a 64th, and the big lights' patches stop above the
//     sliders.

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
    inline int exRowTop (int ch) { return 100 + 96 * ch; }                              // Tetrad Ex: channel rows
    inline constexpr int kExKnobX[3] = { 664, 706, 748 };                              // sine level, asgn level, shape

    // A light's brightness in 64 steps: what is drawn, and what decides whether it is drawn again.
    inline int lightStep (double level01) { return juce::roundToInt (juce::jlimit (0.0, 1.0, level01) * 64.0); }
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

// Everything on the panel that never moves, drawn once into a picture (setBufferedToImage) and copied from there.
// Drawn again only when Tetrad Ex is switched on or off (its labels dim) or the window size changes.
class PanelBackground : public juce::Component
{
public:
    PanelBackground() { setOpaque (true); setInterceptsMouseClicks (false, false); setBufferedToImage (true); }
    void paint (juce::Graphics&) override;
    void setExOn (bool on) { if (on != exOn) { exOn = on; repaint(); } }
    void setOutputOn (int ch, int which, bool on);           // the name under an output light reads "off" while it is off
    static juce::Rectangle<int> outLabelArea (int ch, int which) { return { TetradStyle::outX (ch, which) - 15, 482, 30, 32 }; }
    int timesDrawn = 0;                      // for the offline checks
private:
    bool exOn = false;
    std::array<std::array<bool, 3>, 4> outOn {{ { true, true, true }, { true, true, true }, { true, true, true }, { true, true, true } }};
};

// Tetrad Ex's on switch, a small pill: knob on the left when off, on the right and amber when on.
class ExSwitch : public juce::Button
{
public:
    ExSwitch() : juce::Button ("Ex on") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// One light (or read-out) on the panel: a small patch that paints its own background and what it shows. No clicks.
class LightSpot : public juce::Component
{
public:
    explicit LightSpot (std::function<void (juce::Graphics&)> p) : painter (std::move (p)) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override { painter (g); }
private:
    std::function<void (juce::Graphics&)> painter;
};

// A clickable output light: switches its output on and off, and paints its own light (a patch, as LightSpot).
class OutputSwitch : public juce::Button
{
public:
    OutputSwitch() : juce::Button ("output") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool, bool) override { if (painter) painter (g); }
    std::function<void (juce::Graphics&)> painter;
};

// Everything that moves: the channel lights, the output lights, the read-outs, the mode light, the Waves view. Not a
// component itself: it owns the light patches (added to the panel by attach) and keeps what each one shows. At each
// screen refresh it updates one of two rows in turn: the top row (big lights, read-outs, mode light, "no tempo") or
// the bottom row (the output lights), asking only the patches that changed to draw again. A timer takes over if the
// screen refreshes stop arriving.
// How the window draws. The plugin uses the defaults; the real-window cost check (windowcost.cpp) tries others to
// compare them on the build Mac.
struct LightsDrawSettings
{
    double framesPerSecond = 30.0;
    bool rowsTakeTurns = false;              // the two rows on alternate screen refreshes, instead of together
    bool cacheControls = false;              // every control kept as a picture (setBufferedToImage)
};

class LightsLayer : private juce::Timer
{
public:
    static inline LightsDrawSettings drawSettings;

    explicit LightsLayer (TetradProcessor& p);
    ~LightsLayer() override { stopTimer(); }
    void attach (juce::Component& parent, std::array<std::array<OutputSwitch, 3>, 4>& outputs);
    void update();                           // both rows at once (the offline checks, and the timer if refreshes stop)
    void updateRow (int row, bool force);    // 0: the top row; 1: the output lights
    void setWaves (bool shouldShowWaves);
    bool isWaves() const { return waves; }

    // The big light's patch stops just above the slider (y 146).
    static juce::Rectangle<int> lightArea (int ch)   { return { TetradStyle::kColX[ch] + 2, 93, TetradStyle::kColW - 4, 53 }; }
    static juce::Rectangle<int> readoutArea (int ch) { return { TetradStyle::kColX[ch] + 2, 298, TetradStyle::kColW - 4, 32 }; }
    static juce::Rectangle<int> outArea (int ch, int which) { return { TetradStyle::outX (ch, which) - 14, TetradStyle::kOutY - 14, 28, 28 }; }
    static juce::Rectangle<int> modeLightArea()      { return { 114, 58, 28, 24 }; }
    static juce::Rectangle<int> tempoNoteArea()      { return { 60, 286, 76, 16 }; }

    int framesDrawn = 0;
    double areaDrawn = 0.0;                  // for the offline checks: total area drawn again, in panel pixels
    juce::RectangleList<int> lastFrameAreas; // for the offline checks: what the last frame asked to redraw

private:
    // A kind of light drawn once into pictures at the window's real pixel size: its glow (amber circles) and its core
    // (the dark disc and the amber disc on it); each frame copies them at the light's brightness.
    struct Sprite
    {
        float core = 6.0f; float halos[3] {}; float alphas[3] {};
        float scale = 0.0f; juce::Image glow, base, lit;
    };
    enum { bigLight = 0, outLight, numSprites };
    std::array<Sprite, numSprites> sprites;
    void drawSprite (juce::Graphics& g, int kind, float cx, float cy, double level);

    void timerCallback() override;
    void onVBlank();
    juce::String readout (int ch, juce::String& under) const;
    void paintChannelLight (juce::Graphics&, int ch);
    void paintOutput (juce::Graphics&, int ch, int which);

    TetradProcessor& proc;
    bool waves = false;
    TetradProcessor::Traces traces {};
    std::array<std::unique_ptr<LightSpot>, 4> channelLights, readouts;
    std::unique_ptr<LightSpot> modeLight, tempoNote;
    std::array<std::array<OutputSwitch*, 3>, 4> outs {};
    std::unique_ptr<juce::VBlankAttachment> vblank;
    int nextRow = 0;
    double lastRowAt = 0.0, lastVBlankAt = -1.0e9;

    // What each patch shows (what it was last asked to draw).
    std::array<int, 4> drawnLight {};
    std::array<std::array<int, 3>, 4> drawnOut {};      // light step 0-64, or -1 for switched off
    std::array<juce::String, 4> drawnReadout, drawnUnder;
    std::array<int, 4> drawnMarker { -2, -2, -2, -2 };
    bool drawnBlink = false, drawnMissing = false, firstFrame = true;
    double blinkUntil = 0.0;
    int drawnMode = -1;
};

// The whole panel at its design size (920 x 560). The editor below scales it to the window.
class TetradPanel : public juce::Component, private juce::ValueTree::Listener
{
public:
    explicit TetradPanel (TetradProcessor&);
    ~TetradPanel() override;

    void resized() override;

    // For the offline checks: every control and the parameter it moves.
    struct ControlInfo { juce::Component* control; juce::String paramId; };
    std::vector<ControlInfo> controls();

    // Lights | Waves: what the channel lights show. Saved with the project (not an audio parameter).
    Choices viewChoices;
    bool isWavesView() const { return lights.isWaves(); }
    const LightsLayer& lightsLayer() const { return lights; }
    LightsLayer& lightsLayer() { return lights; }
    const PanelBackground& panelBackground() const { return background; }
    bool exControlsDimmed() const { return sineLevelKnobs[0].getAlpha() < 1.0f; }
    static inline const juce::Identifier wavesViewProperty { "wavesView" };

    static constexpr int kWidth = 920, kHeight = 560;      // the design size; everything is laid out at this size

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void showView (bool waves);
    void setExLook (bool on);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    TetradProcessor& proc;
    TetradLookAndFeel lnf;
    PanelBackground background;
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

    // Tetrad Ex
    ExSwitch exSwitch;
    std::array<juce::Slider, 4> sineLevelKnobs, asgnLevelKnobs, shapeKnobs;
    std::array<juce::ComboBox, 4> exWaveBoxes, shapeFromBoxes;
    std::array<std::unique_ptr<Choices>, 4> shapeTargetChoices;
    std::unique_ptr<ButtonAttachment> exSwitchAtt;
    std::array<std::unique_ptr<SliderAttachment>, 4> sineLevelAtt, asgnLevelAtt, shapeAtt;
    std::array<std::unique_ptr<ComboAttachment>, 4> exWaveAtt, shapeFromAtt;
    std::unique_ptr<juce::ParameterAttachment> exLookAtt;
    std::array<std::array<std::unique_ptr<juce::ParameterAttachment>, 3>, 4> outLookAtt;

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
