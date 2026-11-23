#include "PluginEditor.h"
#include "Parameters.h"

#include <cmath>

using namespace TetradStyle;

namespace
{
    juce::Font font (float size) { return juce::Font (juce::FontOptions (size)); }
    juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

    void textAt (juce::Graphics& g, const juce::String& s, float x, float baseline, float size, juce::Colour c,
                 juce::Justification j = juce::Justification::centredLeft, float width = 200.0f)
    {
        g.setColour (c);
        g.setFont (font (size));
        float left = x;
        if (j == juce::Justification::centred) left = x - width * 0.5f;
        else if (j == juce::Justification::centredRight) left = x - width;
        g.drawText (s, juce::Rectangle<float> (left, baseline - size, width, size + 4.0f), j, false);
    }

    // A round light with a soft halo; level 0 to 1 decides how brightly it shines.
    void drawLight (juce::Graphics& g, float cx, float cy, float core, const float halos[3], const float haloAlpha[3], double level)
    {
        const float lv = (float) juce::jlimit (0.0, 1.0, level);
        for (int k = 0; k < 3; ++k)
            if (halos[k] > 0.0f)
            {
                g.setColour (amber.withAlpha (haloAlpha[k] * lv));
                g.fillEllipse (cx - halos[k], cy - halos[k], 2.0f * halos[k], 2.0f * halos[k]);
            }
        g.setColour (knobFill);
        g.fillEllipse (cx - core, cy - core, 2.0f * core, 2.0f * core);
        g.setColour (amber.withAlpha (lightAlpha (lv)));
        g.fillEllipse (cx - core, cy - core, 2.0f * core, 2.0f * core);
    }

    void drawWaveIcon (juce::Graphics& g, int wave, juce::Rectangle<float> r, juce::Colour c)
    {
        static const std::vector<std::vector<float>> pts {
            { 5, 19, 11, 5, 17, 19, 23, 5, 29, 19 },
            { 5, 5, 17, 19, 17, 5, 29, 19 },
            { 5, 19, 17, 5, 17, 19, 29, 5 },
            { 4, 19, 9, 5, 16, 5, 21, 19, 29, 19 },
            { 4, 15, 10, 15, 10, 6, 17, 6, 17, 18, 23, 18, 23, 10, 30, 10 },
            { 4, 15, 11, 6, 18, 18, 24, 10, 30, 13 },
        };
        const auto& p = pts[(size_t) juce::jlimit (0, 5, wave)];
        const float sx = r.getWidth() / 34.0f, sy = r.getHeight() / 24.0f;
        juce::Path path;
        path.startNewSubPath (r.getX() + p[0] * sx, r.getY() + p[1] * sy);
        for (size_t k = 2; k + 1 < p.size(); k += 2) path.lineTo (r.getX() + p[k] * sx, r.getY() + p[k + 1] * sy);
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

    const char* kOutNames[3] = { "sine", "asgn", "rect" };
    int outCc (int ch, int which) { return which == 0 ? tetrad::kSineCoarse[ch] : which == 1 ? tetrad::kAsgnCoarse[ch] : tetrad::kRectCc[ch]; }
}

// ================================================================================================
TetradLookAndFeel::TetradLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff383836));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, knobFill.brighter (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, bright);
    setColour (juce::Label::textColourId, text);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff383836));
    setColour (juce::BubbleComponent::outlineColourId, line);
    setColour (juce::TooltipWindow::textColourId, text);
}

void TetradLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const float size = (float) juce::jmin (w, h) - 2.0f;
    const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    g.setColour (knobFill);
    g.fillEllipse (centre.x - r, centre.y - r, size, size);
    g.setColour (line);
    g.drawEllipse (centre.x - r, centre.y - r, size, size, 1.0f);
    const float angle = start + pos * (end - start);
    const float len = r * 0.9f;
    g.setColour (bright);
    g.drawLine ({ centre, { centre.x + len * std::sin (angle), centre.y - len * std::cos (angle) } }, 1.6f);
}

void TetradLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (track);
    g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);
    g.setColour (text);
    g.fillRoundedRectangle (cx - 17.0f, pos - 6.0f, 34.0f, 12.0f, 2.0f);
    g.setColour (panel.withAlpha (0.6f));
    g.fillRect (cx - 11.0f, pos - 0.5f, 22.0f, 1.0f);
}

void TetradLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (box.isMouseOver() ? dim : line);
    g.drawRoundedRectangle (b, 3.0f, 0.8f);
    juce::Path arrow;
    const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (dim);
    g.strokePath (arrow, juce::PathStrokeType (1.2f));
}

void TetradLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (3, 0, box.getWidth() - 16, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

// ================================================================================================
Choices::Choices (std::vector<Item> it, float size) : items (std::move (it)), fontSize (size)
{
    juce::Rectangle<int> all;
    for (auto& i : items) all = all.isEmpty() ? i.area : all.getUnion (i.area);
    for (auto& i : items) i.area -= all.getPosition();
    setBounds (all);
}

void Choices::attach (juce::RangedAudioParameter& p)
{
    param = &p;
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { show (juce::roundToInt (v)); }, nullptr);
    attachment->sendInitialUpdate();
}

void Choices::choose (int index)
{
    index = juce::jlimit (0, (int) items.size() - 1, index);
    if (attachment) attachment->setValueAsCompleteGesture ((float) index);
    show (index);
    if (onChoose) onChoose (index);
}

void Choices::mouseDown (const juce::MouseEvent& e)
{
    for (int k = 0; k < (int) items.size(); ++k)
        if (items[(size_t) k].area.contains (e.getPosition())) { choose (k); return; }
}

void Choices::paint (juce::Graphics& g)
{
    for (int k = 0; k < (int) items.size(); ++k)
    {
        const auto& it = items[(size_t) k];
        const bool on = k == selected;
        const auto r = it.area.toFloat().reduced (0.5f);
        g.setColour (on ? knobFill : cardFill);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? amber : line);
        g.drawRoundedRectangle (r, 4.0f, on ? 1.2f : 0.8f);
        if (it.icon >= 0) drawWaveIcon (g, it.icon, it.area.toFloat(), on ? bright : text);
        else
        {
            g.setColour (on ? bright : text);
            g.setFont (font (fontSize));
            g.drawText (it.label, it.area, juce::Justification::centred, false);
        }
    }
}

// ================================================================================================
LightsLayer::LightsLayer (TetradProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    startTimerHz (30);
}

juce::String LightsLayer::readout (int ch, juce::String& under) const
{
    auto& vts = const_cast<TetradProcessor&> (proc).getValueTreeState();
    const int mode = juce::roundToInt (vts.getRawParameterValue (TetradParams::mode)->load());
    const bool tempo = vts.getRawParameterValue (TetradParams::tempoFollow)->load() >= 0.5f;
    const double slider = vts.getRawParameterValue (TetradParams::slider[ch])->load();
    const auto& v = proc.view[(size_t) ch];
    const bool selfTimed = mode == (int) tetrad::Mode::free || ch == 0;
    if (selfTimed && tempo)
    {
        under = "length";
        return tetrad::kLengthNames[juce::jlimit (0, tetrad::kNumLengths - 1, v.length.load())];
    }
    if (selfTimed)
    {
        under = "rate";
        return TetradParams::hzText (tetrad::sliderHz (slider));
    }
    if (mode == (int) tetrad::Mode::phase)
    {
        under = "phase";
        return juce::String (juce::roundToInt (slider * 360.0)) + utf8 ("\xc2\xb0");
    }
    if (mode == (int) tetrad::Mode::divide)
    {
        under = "divide";
        return utf8 ("\xc3\xb7") + juce::String (v.ratio.load());
    }
    under = "multiply";
    return utf8 ("\xc3\x97") + juce::String (v.ratio.load());
}

void LightsLayer::timerCallback()
{
    if (waves) proc.copyTraces (traces);

    auto& vts = proc.getValueTreeState();
    const int mode = juce::roundToInt (vts.getRawParameterValue (TetradParams::mode)->load());
    const bool missing = proc.tempoMissing.load() && vts.getRawParameterValue (TetradParams::tempoFollow)->load() >= 0.5f;
    const bool all = firstFrame || waves != drawnWaves;
    auto draw = [this] (juce::Rectangle<int> r) { repaint (r); areaDrawn += (double) r.getWidth() * r.getHeight(); };

    // The mode light blinks at each 90 degrees crossed in phase mode and at each ratio change in divide / multiply.
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (int ch = 0; ch < 4; ++ch)
    {
        const int m = proc.view[(size_t) ch].marker.load();
        if (drawnMarker[(size_t) ch] != -2 && m != drawnMarker[(size_t) ch] && m >= 0 && drawnMarker[(size_t) ch] >= 0 && mode == drawnMode)
            blinkUntil = now + 140.0;
        drawnMarker[(size_t) ch] = m;
    }
    const bool blink = now < blinkUntil;
    if (all || blink != drawnBlink || mode != drawnMode) draw (modeLightArea());
    if (all || missing != drawnMissing) draw (tempoNoteArea());
    drawnBlink = blink; drawnMissing = missing; drawnMode = mode;

    for (int ch = 0; ch < 4; ++ch)
    {
        const auto& v = proc.view[(size_t) ch];
        const int light = juce::roundToInt ((v.sine.load() + 1.0f) * 75.0f);
        if (all || waves || light != drawnLight[(size_t) ch]) draw (lightArea (ch));
        drawnLight[(size_t) ch] = light;

        const bool on[3] = { vts.getRawParameterValue (TetradParams::sineOn[ch])->load() >= 0.5f,
                             vts.getRawParameterValue (TetradParams::asgnOn[ch])->load() >= 0.5f,
                             vts.getRawParameterValue (TetradParams::rectOn[ch])->load() >= 0.5f };
        const int level[3] = { juce::roundToInt ((v.sine.load() + 1.0f) * 75.0f), juce::roundToInt ((v.asgn.load() + 1.0f) * 75.0f), v.rect.load() ? 150 : 0 };
        for (int w = 0; w < 3; ++w)
        {
            const int code = on[w] ? level[w] : -1;
            if (all || code != drawnOut[(size_t) ch][(size_t) w]) draw (outArea (ch, w));
            drawnOut[(size_t) ch][(size_t) w] = code;
        }

        juce::String under;
        const auto r = readout (ch, under) + "|" + under;
        if (all || r != drawnReadout[(size_t) ch]) draw (readoutArea (ch));
        drawnReadout[(size_t) ch] = r;
    }
    drawnWaves = waves;
    firstFrame = false;
    ++framesDrawn;
}

void LightsLayer::paint (juce::Graphics& g)
{
    auto& vts = proc.getValueTreeState();
    const int mode = juce::roundToInt (vts.getRawParameterValue (TetradParams::mode)->load());

    // The mode light.
    {
        const bool blink = juce::Time::getMillisecondCounterHiRes() < blinkUntil;
        const float halos[3] = { 9.0f, 0.0f, 0.0f }, alpha[3] = { 0.18f, 0.0f, 0.0f };
        drawLight (g, 128.0f, 70.0f, 5.0f, halos, alpha, blink ? 0.05 : 1.0);
        (void) mode;
    }
    if (proc.tempoMissing.load() && vts.getRawParameterValue (TetradParams::tempoFollow)->load() >= 0.5f)
        textAt (g, "no tempo", 134.0f, 298.0f, 11.0f, amber, juce::Justification::centredRight, 80.0f);

    for (int ch = 0; ch < 4; ++ch)
    {
        const auto& v = proc.view[(size_t) ch];
        const float cx = (float) colCentre (ch);

        if (waves)
        {
            const auto box = lightArea (ch).toFloat().reduced (4.0f, 2.0f);
            g.setColour (track);
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (line);
            g.fillRect (box.getX() + 3.0f, box.getCentreY() - 0.3f, box.getWidth() - 6.0f, 0.6f);
            for (int which = 1; which >= 0; --which)
            {
                juce::Path p;
                const int n = tetrad::Engine::kTracePoints;
                for (int k = 0; k < n; ++k)
                {
                    const float x = box.getX() + 3.0f + (box.getWidth() - 6.0f) * (float) k / (float) (n - 1);
                    const float y = box.getCentreY() - (box.getHeight() * 0.5f - 4.0f) * traces[(size_t) ch][(size_t) (k * 2 + which)];
                    if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (which == 0 ? amber : soft.withAlpha (0.75f));
                g.strokePath (p, juce::PathStrokeType (which == 0 ? 1.4f : 1.1f));
            }
        }
        else
        {
            const float halos[3] = { 28.0f, 21.0f, 15.0f }, alpha[3] = { 0.10f, 0.20f, 0.40f };
            drawLight (g, cx, (float) kLightY, 11.0f, halos, alpha, (v.sine.load() + 1.0) * 0.5);
        }

        // The read-out under the slider.
        juce::String under;
        const auto r = readout (ch, under);
        textAt (g, r, cx, 316.0f, 13.0f, bright, juce::Justification::centred, 98.0f);
        textAt (g, under, cx, 329.0f, 11.0f, dim, juce::Justification::centred, 98.0f);

        // The three output lights: brightness follows the value; a switched-off output shows an empty ring.
        const double levels[3] = { (v.sine.load() + 1.0) * 0.5, (v.asgn.load() + 1.0) * 0.5, v.rect.load() ? 1.0 : 0.0 };
        for (int w = 0; w < 3; ++w)
        {
            const bool on = vts.getRawParameterValue (w == 0 ? TetradParams::sineOn[ch] : w == 1 ? TetradParams::asgnOn[ch] : TetradParams::rectOn[ch])->load() >= 0.5f;
            const float x = (float) outX (ch, w), y = (float) kOutY;
            if (on)
            {
                const float halos[3] = { 13.0f, 9.0f, 0.0f }, alpha[3] = { 0.15f, 0.30f, 0.0f };
                drawLight (g, x, y, 6.0f, halos, alpha, levels[w]);
            }
            else
            {
                g.setColour (line);
                juce::Path ring;
                ring.addEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
                juce::Path dashed;
                const float dashes[2] = { 2.0f, 2.0f };
                juce::PathStrokeType (1.2f).createDashedStroke (dashed, ring, dashes, 2);
                g.fillPath (dashed);
            }
            textAt (g, on ? juce::String (kOutNames[w]) : juce::String ("off"), x, 495.0f, 11.0f, on ? soft : line, juce::Justification::centred, 30.0f);
            textAt (g, juce::String (outCc (ch, w)), x, 508.0f, 11.0f, on ? dim : line, juce::Justification::centred, 30.0f);
        }
    }
}

// ================================================================================================
namespace
{
    std::vector<Choices::Item> modeItems()
    {
        std::vector<Choices::Item> v;
        const char* names[4] = { "free", "phase", "divide", "multiply" };
        for (int k = 0; k < 4; ++k) v.push_back ({ { 24, 82 + 28 * k, 110, 24 }, names[k] });
        return v;
    }
    std::vector<Choices::Item> waveItems()
    {
        std::vector<Choices::Item> v;
        for (int k = 0; k < 6; ++k) v.push_back ({ { 24 + 38 * (k % 3), 220 + 28 * (k / 3), 34, 24 }, {}, k });
        return v;
    }
}

TetradPanel::TetradPanel (TetradProcessor& p)
    : viewChoices ({ { { 24, 412, 54, 24 }, "lights" }, { { 80, 412, 54, 24 }, "waves" } }, 12.0f),
      proc (p), lights (p),
      modeChoices (modeItems(), 12.0f),
      waveChoices (waveItems(), 12.0f),
      tempoChoices ({ { { 24, 304, 54, 24 }, "Hz" }, { { 80, 304, 54, 24 }, "Logic" } }, 12.0f),
      rectOutChoices ({ { { 24, 358, 34, 24 }, "CC" }, { { 62, 358, 34, 24 }, "note" }, { { 100, 358, 34, 24 }, "both" } }, 11.0f)
{
    setLookAndFeel (&lnf);
    auto& vts = proc.getValueTreeState();

    addAndMakeVisible (lights);                    // first, so the controls sit on top of it

    auto attachChoices = [&] (Choices& c, const char* id) { c.attach (*vts.getParameter (id)); addAndMakeVisible (c); };
    attachChoices (modeChoices, TetradParams::mode);
    attachChoices (waveChoices, TetradParams::asgnWave);
    attachChoices (tempoChoices, TetradParams::tempoFollow);
    attachChoices (rectOutChoices, TetradParams::rectOut);
    modeChoices.setTitle ("Mode");
    waveChoices.setTitle ("Asgn wave");
    tempoChoices.setTitle ("Tempo");
    rectOutChoices.setTitle ("Rect out");

    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        auto& s = sliders[c];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setName ("slider");
        addAndMakeVisible (s);
        sliderAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::slider[ch], s);

        auto& k = depthKnobs[c];
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (juce::MathConstants<float>::pi * -0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        k.setPopupDisplayEnabled (true, true, this);
        k.setDoubleClickReturnValue (true, 0.0);
        k.setName ("depth");
        addAndMakeVisible (k);
        depthAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::modDepth[ch], k);

        auto& mf = modFromBoxes[c];
        mf.addItemList (TetradParams::modFromNames (ch), 1);
        addAndMakeVisible (mf);
        modFromAtt[c] = std::make_unique<ComboAttachment> (vts, TetradParams::modFrom[ch], mf);

        auto& rf = resetFromBoxes[c];
        rf.addItemList (TetradParams::resetFromNames (ch), 1);
        addAndMakeVisible (rf);
        resetFromAtt[c] = std::make_unique<ComboAttachment> (vts, TetradParams::resetFrom[ch], rf);

        const char* ids[3] = { TetradParams::sineOn[ch], TetradParams::asgnOn[ch], TetradParams::rectOn[ch] };
        for (int w = 0; w < 3; ++w)
        {
            auto& sw = outSwitches[c][(size_t) w];
            sw.setTooltip ("Click to switch this output on or off");
            sw.setTitle (TetradParams::channelLetter[ch] + " " + kOutNames[w] + " on");
            addAndMakeVisible (sw);
            outAtt[c][(size_t) w] = std::make_unique<ButtonAttachment> (vts, ids[w], sw);
        }
    }

    for (int n = 1; n <= 16; ++n) channelBox.addItem ("channel " + juce::String (n), n);
    addAndMakeVisible (channelBox);
    channelAtt = std::make_unique<ComboAttachment> (vts, TetradParams::outChannel, channelBox);

    // Lights | Waves: one or the other; the choice is kept in the saved state.
    viewChoices.onChoose = [this] (int k) { proc.getValueTreeState().state.setProperty (wavesViewProperty, k == 1, nullptr); };
    addAndMakeVisible (viewChoices);
    showView ((bool) vts.state.getProperty (wavesViewProperty, false));
    vts.state.addListener (this);

    setSize (kWidth, kHeight);
}

TetradPanel::~TetradPanel()
{
    proc.getValueTreeState().state.removeListener (this);
    setLookAndFeel (nullptr);
}

void TetradPanel::showView (bool waves)
{
    lights.setWaves (waves);
    viewChoices.show (waves ? 1 : 0);
}

void TetradPanel::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree == proc.getValueTreeState().state)
        showView ((bool) tree.getProperty (wavesViewProperty, false));
}

void TetradPanel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == wavesViewProperty && tree == proc.getValueTreeState().state)
        showView ((bool) tree.getProperty (wavesViewProperty, false));
}

std::vector<TetradPanel::ControlInfo> TetradPanel::controls()
{
    std::vector<ControlInfo> v {
        { &modeChoices, TetradParams::mode }, { &waveChoices, TetradParams::asgnWave },
        { &tempoChoices, TetradParams::tempoFollow }, { &rectOutChoices, TetradParams::rectOut },
        { &channelBox, TetradParams::outChannel } };
    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        v.push_back ({ &sliders[c], TetradParams::slider[ch] });
        v.push_back ({ &modFromBoxes[c], TetradParams::modFrom[ch] });
        v.push_back ({ &depthKnobs[c], TetradParams::modDepth[ch] });
        v.push_back ({ &resetFromBoxes[c], TetradParams::resetFrom[ch] });
        v.push_back ({ &outSwitches[c][0], TetradParams::sineOn[ch] });
        v.push_back ({ &outSwitches[c][1], TetradParams::asgnOn[ch] });
        v.push_back ({ &outSwitches[c][2], TetradParams::rectOn[ch] });
    }
    return v;
}

void TetradPanel::paintEx (juce::Graphics& g)
{
    const juce::Rectangle<float> card (608.5f, 62.5f, 291.0f, 447.0f);
    g.setColour (exFill);
    g.fillRoundedRectangle (card, 6.0f);
    g.setColour (knobFill);
    g.drawRoundedRectangle (card, 6.0f, 0.8f);

    g.beginTransparencyLayer (0.38f);
    textAt (g, "Tetrad Ex", 622.0f, 88.0f, 15.0f, bright);
    g.setColour (knobFill);
    g.fillRoundedRectangle (846.0f, 72.0f, 40.0f, 20.0f, 10.0f);
    g.setColour (dim);
    g.fillEllipse (849.0f, 75.0f, 14.0f, 14.0f);
    auto knob = [&] (float cx, float cy, float angle, const char* label)
    {
        g.setColour (knobFill);
        g.fillEllipse (cx - 12.0f, cy - 12.0f, 24.0f, 24.0f);
        g.setColour (line);
        g.drawEllipse (cx - 12.0f, cy - 12.0f, 24.0f, 24.0f, 1.0f);
        g.setColour (bright);
        g.drawLine (cx, cy, cx + 11.0f * std::sin (angle), cy - 11.0f * std::cos (angle), 1.5f);
        textAt (g, label, cx, 28.0f + cy, 11.0f, text, juce::Justification::centred, 40.0f);
    };
    auto box = [&] (float x, float y, float w, const juce::String& s, bool filled)
    {
        g.setColour (filled ? knobFill : panel);
        g.fillRoundedRectangle (x, y, w, 18.0f, 3.0f);
        if (! filled) { g.setColour (line); g.drawRoundedRectangle (x, y, w, 18.0f, 3.0f, 0.8f); }
        g.setColour (text);
        g.setFont (font (11.0f));
        g.drawText (s, juce::Rectangle<float> (x + 4.0f, y, w - 6.0f, 18.0f), w > 60.0f ? juce::Justification::centredLeft : juce::Justification::centred, false);
    };
    for (int ch = 0; ch < 4; ++ch)
    {
        const float top = 100.0f + 96.0f * (float) ch;
        textAt (g, TetradParams::channelLetter[ch], 624.0f, top + 22.0f, 14.0f, bright);
        knob (664.0f, top + 26.0f, 0.75f, "sine");
        knob (706.0f, top + 26.0f, 0.75f, "asgn");
        knob (748.0f, top + 26.0f, 0.0f, "shape");
        box (778.0f, top + 4.0f, 108.0f, "wave: main", false);
        box (778.0f, top + 26.0f, 108.0f, "shape from: off", false);
        box (778.0f, top + 48.0f, 35.0f, "fold", true);
        box (815.0f, top + 48.0f, 35.0f, "wave", false);
        box (852.0f, top + 48.0f, 34.0f, "width", false);
    }
    g.endTransparencyLayer();
    textAt (g, "until milestone 2", 754.0f, 494.0f, 12.0f, amber, juce::Justification::centred, 160.0f);
}

void TetradPanel::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    // Header
    textAt (g, "Tetrad", 20.0f, 40.0f, 24.0f, bright, juce::Justification::centredLeft, 200.0f);
    textAt (g, utf8 ("four linked LFOs \xe2\x86\x92 MIDI CC"), 900.0f, 38.0f, 13.0f, dim, juce::Justification::centredRight, 300.0f);
    g.setColour (line);
    g.fillRect (20.0f, 52.0f, 880.0f, 0.8f);

    // Left strip labels
    textAt (g, "mode", 24.0f, 78.0f, 12.0f, dim);
    textAt (g, "asgn wave", 24.0f, 216.0f, 12.0f, dim);
    textAt (g, "tempo", 24.0f, 300.0f, 12.0f, dim);
    textAt (g, "rect out", 24.0f, 354.0f, 12.0f, dim);
    textAt (g, "view", 24.0f, 408.0f, 12.0f, dim);
    textAt (g, "MIDI out", 24.0f, 464.0f, 12.0f, dim);

    // The four channel columns
    for (int ch = 0; ch < 4; ++ch)
    {
        const float x = (float) kColX[ch], cx = (float) colCentre (ch);
        g.setColour (cardFill);
        g.fillRoundedRectangle (x, (float) kColTop, (float) kColW, (float) kColH, 6.0f);
        textAt (g, TetradParams::channelLetter[ch], cx, 90.0f, 16.0f, bright, juce::Justification::centred, 40.0f);
        g.setColour (line);
        for (int k = 0; k < 5; ++k)
        {
            const float y = 152.0f + 35.0f * (float) k;
            g.fillRect (cx - (k % 2 == 0 ? 20.0f : 18.0f), y - 0.4f, k % 2 == 0 ? 8.0f : 6.0f, 0.8f);
        }
        textAt (g, "mod from", x + 7.0f, 348.0f, 11.0f, dim);
        textAt (g, "depth", x + 44.0f, 398.0f, 11.0f, dim);
        textAt (g, "reset from", x + 7.0f, 424.0f, 11.0f, dim);
    }

    paintEx (g);

    // Footer
    g.setColour (line);
    g.fillRect (20.0f, 522.0f, 880.0f, 0.8f);
    textAt (g, utf8 ("particledots \xc2\xb7 not affiliated with any module maker"), 20.0f, 546.0f, 12.0f, dim, juce::Justification::centredLeft, 400.0f);
    textAt (g, "v" + juce::String (JucePlugin_VersionString), 880.0f, 546.0f, 12.0f, dim, juce::Justification::centredRight, 100.0f);
}

void TetradPanel::resized()
{
    lights.setBounds (getLocalBounds());
    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        const int x = kColX[ch], cx = colCentre (ch);
        sliders[c].setBounds (cx - 20, 146, 40, 152);
        modFromBoxes[c].setBounds (x + 7, 352, 90, 20);
        depthKnobs[c].setBounds (x + 15, 381, 24, 24);
        resetFromBoxes[c].setBounds (x + 7, 428, 90, 20);
        for (int w = 0; w < 3; ++w)
            outSwitches[c][(size_t) w].setBounds (outX (ch, w) - 14, kOutY - 14, 28, 28);
    }
    channelBox.setBounds (24, 468, 110, 24);
}

// ================================================================================================
TetradEditor::TetradEditor (TetradProcessor& p)
    : AudioProcessorEditor (&p), panel (p), proc (p)
{
    addAndMakeVisible (panel);

    constrainer.setFixedAspectRatio ((double) kWidth / (double) kHeight);
    constrainer.setSizeLimits (kMinWidth, heightForWidth (kMinWidth), kMaxWidth, heightForWidth (kMaxWidth));
    setConstrainer (&constrainer);
    setResizable (true, true);                       // with the drag corner

    const int saved = (int) proc.getValueTreeState().state.getProperty (widthProperty, kWidth);
    const int width = juce::jlimit (kMinWidth, kMaxWidth, saved);
    setSize (width, heightForWidth (width));
}

TetradEditor::~TetradEditor() = default;

void TetradEditor::paint (juce::Graphics& g)
{
    g.fillAll (TetradStyle::panel);
}

void TetradEditor::resized()
{
    const double scale = juce::jmin ((double) getWidth() / kWidth, (double) getHeight() / kHeight);
    panel.setTransform (juce::AffineTransform::scale ((float) scale));
    panel.setBounds (0, 0, kWidth, kHeight);

    // Keep the chosen size with the project.
    auto& state = proc.getValueTreeState().state;
    if ((int) state.getProperty (widthProperty, -1) != getWidth())
        state.setProperty (widthProperty, getWidth(), nullptr);
}
