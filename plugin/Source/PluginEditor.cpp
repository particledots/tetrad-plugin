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
    auto set = [this] (int kind, float core, std::initializer_list<float> halos, std::initializer_list<float> alphas)
    {
        auto& sp = sprites[(size_t) kind];
        sp.core = core;
        int k = 0; for (float h : halos) sp.halos[k++] = h;
        k = 0; for (float a : alphas) sp.alphas[k++] = a;
    };
    set (bigLight, 11.0f, { 26.0f, 20.0f, 14.0f }, { 0.10f, 0.20f, 0.40f });
    set (outLight, 6.0f, { 13.0f, 9.0f, 0.0f }, { 0.15f, 0.30f, 0.0f });
}

void LightsLayer::attach (juce::Component& parent, std::array<std::array<OutputSwitch, 3>, 4>& outputs)
{
    auto add = [&parent] (std::unique_ptr<LightSpot>& spot, juce::Rectangle<int> area, std::function<void (juce::Graphics&)> painter)
    {
        spot = std::make_unique<LightSpot> (std::move (painter));
        spot->setBounds (area);
        parent.addAndMakeVisible (*spot);
    };

    add (modeLight, modeLightArea(), [this] (juce::Graphics& g)
    {
        g.fillAll (panel);
        const auto o = modeLightArea().getPosition().toFloat();
        const float halos[3] = { 9.0f, 0.0f, 0.0f }, alpha[3] = { 0.18f, 0.0f, 0.0f };
        drawLight (g, 128.0f - o.x, 70.0f - o.y, 5.0f, halos, alpha, drawnBlink ? 0.05 : 1.0);
    });
    add (tempoNote, tempoNoteArea(), [this] (juce::Graphics& g)
    {
        g.fillAll (panel);
        const auto o = tempoNoteArea().getPosition().toFloat();
        if (drawnMissing)
            textAt (g, "no tempo", 134.0f - o.x, 298.0f - o.y, 11.0f, amber, juce::Justification::centredRight, 80.0f);
    });
    for (int ch = 0; ch < 4; ++ch)
    {
        add (channelLights[(size_t) ch], lightArea (ch), [this, ch] (juce::Graphics& g) { paintChannelLight (g, ch); });
        add (readouts[(size_t) ch], readoutArea (ch), [this, ch] (juce::Graphics& g)
        {
            g.fillAll (cardFill);
            const auto o = readoutArea (ch).getPosition().toFloat();
            const float cx = (float) colCentre (ch) - o.x;
            textAt (g, drawnReadout[(size_t) ch], cx, 316.0f - o.y, 13.0f, bright, juce::Justification::centred, 98.0f);
            textAt (g, drawnUnder[(size_t) ch], cx, 329.0f - o.y, 11.0f, dim, juce::Justification::centred, 98.0f);
        });
        for (int w = 0; w < 3; ++w)
        {
            auto& o = outputs[(size_t) ch][(size_t) w];
            outs[(size_t) ch][(size_t) w] = &o;
            o.painter = [this, ch, w] (juce::Graphics& g) { paintOutput (g, ch, w); };
        }
    }
    update();
    if (drawSettings.framesPerSecond > 0.0)
        startTimerHz (juce::roundToInt (drawSettings.framesPerSecond));
}

void LightsLayer::timerCallback()
{
    update();
}

void LightsLayer::setWaves (bool shouldShowWaves)
{
    if (waves == shouldShowWaves) return;
    waves = shouldShowWaves;
    if (modeLight != nullptr) updateRow (0, true);
}

void LightsLayer::paintChannelLight (juce::Graphics& g, int ch)
{
    g.fillAll (cardFill);
    const auto area = lightArea (ch);
    if (waves)
    {
        const auto box = area.withZeroOrigin().toFloat().reduced (4.0f, 2.0f);
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
        drawSprite (g, bigLight, (float) (colCentre (ch) - area.getX()), (float) (kLightY - area.getY()), drawnLight[(size_t) ch] / 64.0);
}

// The three output lights: brightness follows the value; a switched-off output shows an empty ring.
void LightsLayer::paintOutput (juce::Graphics& g, int ch, int which)
{
    g.fillAll (cardFill);
    const int code = drawnOut[(size_t) ch][(size_t) which];
    if (code >= 0)
        drawSprite (g, outLight, 14.0f, 14.0f, code / 64.0);
    else
    {
        g.setColour (line);
        juce::Path ring;
        ring.addEllipse (8.0f, 8.0f, 12.0f, 12.0f);
        juce::Path dashed;
        const float dashes[2] = { 2.0f, 2.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, ring, dashes, 2);
        g.fillPath (dashed);
    }
}

void LightsLayer::drawSprite (juce::Graphics& g, int kind, float cx, float cy, double level)
{
    auto& sp = sprites[(size_t) kind];
    const float scale = juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const float reach = juce::jmax (sp.halos[0], sp.core) + 1.0f;                // the picture's half width, panel units
    if (std::fabs (scale - sp.scale) > 0.001f || sp.glow.isNull())
    {
        // Drawn once per window size: the same circles drawLight () makes, at full brightness.
        sp.scale = scale;
        const int px = juce::jmax (4, (int) std::ceil (2.0f * reach * scale));
        auto make = [&] (const std::function<void (juce::Graphics&)>& paintIt)
        {
            juce::Image img (juce::Image::ARGB, px, px, true);
            juce::Graphics sg (img);
            sg.addTransform (juce::AffineTransform::scale (scale));
            paintIt (sg);
            return img;
        };
        sp.glow = make ([&] (juce::Graphics& sg)
        {
            for (int k = 0; k < 3; ++k)
                if (sp.halos[k] > 0.0f)
                {
                    sg.setColour (amber.withAlpha (sp.alphas[k]));
                    sg.fillEllipse (reach - sp.halos[k], reach - sp.halos[k], 2.0f * sp.halos[k], 2.0f * sp.halos[k]);
                }
        });
        sp.base = make ([&] (juce::Graphics& sg) { sg.setColour (knobFill); sg.fillEllipse (reach - sp.core, reach - sp.core, 2.0f * sp.core, 2.0f * sp.core); });
        sp.lit  = make ([&] (juce::Graphics& sg) { sg.setColour (amber);    sg.fillEllipse (reach - sp.core, reach - sp.core, 2.0f * sp.core, 2.0f * sp.core); });
    }
    const auto place = juce::AffineTransform::scale (1.0f / scale).translated (cx - reach, cy - reach);
    const float lv = (float) juce::jlimit (0.0, 1.0, level);
    if (lv > 0.0f) { g.setOpacity (lv); g.drawImageTransformed (sp.glow, place); }
    g.setOpacity (1.0f);
    g.drawImageTransformed (sp.base, place);
    g.setOpacity (lightAlpha (lv));
    g.drawImageTransformed (sp.lit, place);
    g.setOpacity (1.0f);
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

void LightsLayer::update()
{
    updateRow (0, firstFrame);
    updateRow (1, firstFrame);
    firstFrame = false;
}

void LightsLayer::updateRow (int row, bool force)
{
    if (modeLight == nullptr) return;                    // not attached yet
    auto& vts = proc.getValueTreeState();
    auto draw = [this] (juce::Component& c) { c.repaint(); lastFrameAreas.add (c.getBounds()); areaDrawn += (double) c.getWidth() * c.getHeight(); };

    if (row == 0)
    {
        lastFrameAreas.clear();
        if (waves) proc.copyTraces (traces);
        const int mode = juce::roundToInt (vts.getRawParameterValue (TetradParams::mode)->load());
        const bool missing = proc.tempoMissing.load() && vts.getRawParameterValue (TetradParams::tempoFollow)->load() >= 0.5f;

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
        if (force || blink != drawnBlink || mode != drawnMode) { drawnBlink = blink; draw (*modeLight); }
        if (force || missing != drawnMissing) { drawnMissing = missing; draw (*tempoNote); }
        drawnMode = mode;

        for (int ch = 0; ch < 4; ++ch)
        {
            const auto c = (size_t) ch;
            const int light = TetradStyle::lightStep ((proc.view[c].sine.load() + 1.0) * 0.5);
            if (force || waves || light != drawnLight[c]) { drawnLight[c] = light; draw (*channelLights[c]); }

            juce::String under;
            const auto r = readout (ch, under);
            if (force || r != drawnReadout[c] || under != drawnUnder[c]) { drawnReadout[c] = r; drawnUnder[c] = under; draw (*readouts[c]); }
        }
        return;
    }

    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        const auto& v = proc.view[c];
        const bool on[3] = { vts.getRawParameterValue (TetradParams::sineOn[ch])->load() >= 0.5f,
                             vts.getRawParameterValue (TetradParams::asgnOn[ch])->load() >= 0.5f,
                             vts.getRawParameterValue (TetradParams::rectOn[ch])->load() >= 0.5f };
        const int level[3] = { TetradStyle::lightStep ((v.sine.load() + 1.0) * 0.5), TetradStyle::lightStep ((v.asgn.load() + 1.0) * 0.5), v.rect.load() ? 64 : 0 };
        for (int w = 0; w < 3; ++w)
        {
            const int code = on[w] ? level[w] : -1;
            if (force || code != drawnOut[c][(size_t) w]) { drawnOut[c][(size_t) w] = code; draw (*outs[c][(size_t) w]); }
        }
    }
    ++framesDrawn;
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
    setOpaque (true);
    auto& vts = proc.getValueTreeState();

    addAndMakeVisible (background);                // the picture of everything that never moves
    lights.attach (*this, outSwitches);            // then the light patches, so the controls sit on top of them

    auto attachChoices = [&] (Choices& c, const char* id) { c.attach (*vts.getParameter (id)); addAndMakeVisible (c); };
    attachChoices (modeChoices, TetradParams::mode);
    attachChoices (waveChoices, TetradParams::asgnWave);
    attachChoices (tempoChoices, TetradParams::tempoFollow);
    attachChoices (rectOutChoices, TetradParams::rectOut);
    modeChoices.setTitle ("Mode");
    waveChoices.setTitle ("Asgn wave");
    tempoChoices.setTitle ("Tempo");
    rectOutChoices.setTitle ("Rect out");

    auto setUpKnob = [this] (juce::Slider& k, const char* name, double doubleClickValue)
    {
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (juce::MathConstants<float>::pi * -0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        k.setPopupDisplayEnabled (true, true, this);
        k.setDoubleClickReturnValue (true, doubleClickValue);
        k.setName (name);
        addAndMakeVisible (k);
    };

    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        auto& s = sliders[c];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setName ("slider");
        addAndMakeVisible (s);
        sliderAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::slider[ch], s);

        setUpKnob (depthKnobs[c], "depth", 0.0);
        depthAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::modDepth[ch], depthKnobs[c]);

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
            outLookAtt[c][(size_t) w] = std::make_unique<juce::ParameterAttachment> (*vts.getParameter (ids[w]),
                [this, ch, w] (float v) { background.setOutputOn (ch, w, v >= 0.5f); }, nullptr);
            outLookAtt[c][(size_t) w]->sendInitialUpdate();
        }

        // Tetrad Ex, channel ch.
        setUpKnob (sineLevelKnobs[c], "sine level", 1.0);
        sineLevelAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::sineLevel[ch], sineLevelKnobs[c]);
        setUpKnob (asgnLevelKnobs[c], "asgn level", 1.0);
        asgnLevelAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::asgnLevel[ch], asgnLevelKnobs[c]);
        setUpKnob (shapeKnobs[c], "shape", 0.0);
        shapeAtt[c] = std::make_unique<SliderAttachment> (vts, TetradParams::shapeAmount[ch], shapeKnobs[c]);

        auto& wb = exWaveBoxes[c];
        const char* shortWaves[7] = { "main", "triangle", "saw down", "saw up", "trapezoid", "step rnd", "smooth rnd" };   // room for the Mac's wider font
        for (int k = 0; k < TetradParams::exWaveNames.size(); ++k)
            wb.addItem (juce::String ("wave: ") + shortWaves[k], k + 1);
        addAndMakeVisible (wb);
        exWaveAtt[c] = std::make_unique<ComboAttachment> (vts, TetradParams::exWave[ch], wb);

        auto& sf = shapeFromBoxes[c];
        const auto names = TetradParams::modFromNames (ch);
        for (int k = 0; k < names.size(); ++k)
            sf.addItem ("from: " + (k < 2 ? names[k].toLowerCase() : names[k]), k + 1);
        addAndMakeVisible (sf);
        shapeFromAtt[c] = std::make_unique<ComboAttachment> (vts, TetradParams::shapeFrom[ch], sf);

        const int top = exRowTop (ch);
        shapeTargetChoices[c] = std::make_unique<Choices> (std::vector<Choices::Item> {
            { { 778, top + 48, 35, 18 }, "fold" }, { { 815, top + 48, 35, 18 }, "wave" }, { { 852, top + 48, 34, 18 }, "width" } }, 11.0f);
        shapeTargetChoices[c]->setTitle (TetradParams::channelLetter[ch] + " shape target");
        attachChoices (*shapeTargetChoices[c], TetradParams::shapeTarget[ch]);
    }

    for (int n = 1; n <= 16; ++n) channelBox.addItem ("channel " + juce::String (n), n);
    addAndMakeVisible (channelBox);
    channelAtt = std::make_unique<ComboAttachment> (vts, TetradParams::outChannel, channelBox);

    exSwitch.setTitle ("Tetrad Ex on");
    exSwitch.setTooltip ("Switch Tetrad Ex on or off");
    addAndMakeVisible (exSwitch);
    exSwitchAtt = std::make_unique<ButtonAttachment> (vts, TetradParams::exOn, exSwitch);
    exLookAtt = std::make_unique<juce::ParameterAttachment> (*vts.getParameter (TetradParams::exOn), [this] (float v) { setExLook (v >= 0.5f); }, nullptr);
    exLookAtt->sendInitialUpdate();

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

void TetradPanel::setExLook (bool on)
{
    // While Ex is off its controls stay usable (set them up first, then switch on) but are dimmed.
    background.setExOn (on);
    const float a = on ? 1.0f : 0.45f;
    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        for (juce::Component* x : { (juce::Component*) &sineLevelKnobs[c], (juce::Component*) &asgnLevelKnobs[c], (juce::Component*) &shapeKnobs[c],
                                    (juce::Component*) &exWaveBoxes[c], (juce::Component*) &shapeFromBoxes[c], (juce::Component*) shapeTargetChoices[c].get() })
            x->setAlpha (a);
    }
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
    v.push_back ({ &exSwitch, TetradParams::exOn });
    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        v.push_back ({ &sineLevelKnobs[c], TetradParams::sineLevel[ch] });
        v.push_back ({ &asgnLevelKnobs[c], TetradParams::asgnLevel[ch] });
        v.push_back ({ &exWaveBoxes[c], TetradParams::exWave[ch] });
        v.push_back ({ &shapeKnobs[c], TetradParams::shapeAmount[ch] });
        v.push_back ({ &shapeFromBoxes[c], TetradParams::shapeFrom[ch] });
        v.push_back ({ shapeTargetChoices[c].get(), TetradParams::shapeTarget[ch] });
    }
    return v;
}

// ================================================================================================
void ExSwitch::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    const auto b = getLocalBounds().toFloat();
    g.setColour (knobFill);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    if (over) { g.setColour (line); g.drawRoundedRectangle (b.reduced (0.5f), b.getHeight() * 0.5f, 0.8f); }
    const float d = b.getHeight() - 6.0f;
    const float x = on ? b.getRight() - 3.0f - d : b.getX() + 3.0f;
    if (on)
    {
        g.setColour (amber.withAlpha (0.25f));
        g.fillEllipse (x - 3.0f, b.getY(), d + 6.0f, d + 6.0f);
    }
    g.setColour (on ? amber : dim);
    g.fillEllipse (x, b.getY() + 3.0f, d, d);
}

void PanelBackground::setOutputOn (int ch, int which, bool on)
{
    auto& o = outOn[(size_t) ch][(size_t) which];
    if (o != on) { o = on; repaint (outLabelArea (ch, which)); }
}

void PanelBackground::paint (juce::Graphics& g)
{
    ++timesDrawn;
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

        // Under each output light: its name (or "off") and its CC number.
        for (int w = 0; w < 3; ++w)
        {
            const bool on = outOn[(size_t) ch][(size_t) w];
            const float ox = (float) outX (ch, w);
            textAt (g, on ? juce::String (kOutNames[w]) : juce::String ("off"), ox, 495.0f, 11.0f, on ? soft : line, juce::Justification::centred, 30.0f);
            textAt (g, juce::String (outCc (ch, w)), ox, 508.0f, 11.0f, on ? dim : line, juce::Justification::centred, 30.0f);
        }
    }

    // Tetrad Ex: its card and labels (the controls are real components on top). Dimmed while Ex is off.
    const juce::Rectangle<float> card (608.5f, 62.5f, 291.0f, 447.0f);
    g.setColour (exFill);
    g.fillRoundedRectangle (card, 6.0f);
    g.setColour (knobFill);
    g.drawRoundedRectangle (card, 6.0f, 0.8f);
    textAt (g, "Tetrad Ex", 622.0f, 88.0f, 15.0f, exOn ? bright : soft);
    textAt (g, exOn ? "on" : "off", 840.0f, 86.0f, 11.0f, exOn ? amber : dim, juce::Justification::centredRight, 40.0f);
    const auto labels = exOn ? text : line;
    for (int ch = 0; ch < 4; ++ch)
    {
        const float top = (float) exRowTop (ch);
        textAt (g, TetradParams::channelLetter[ch], 624.0f, top + 22.0f, 14.0f, exOn ? bright : dim);
        const char* names[3] = { "sine", "asgn", "shape" };
        for (int k = 0; k < 3; ++k)
            textAt (g, names[k], (float) kExKnobX[k], top + 54.0f, 11.0f, labels, juce::Justification::centred, 40.0f);
    }
    textAt (g, utf8 ("shape: fold the sine \xc2\xb7 switch the asgn wave \xc2\xb7 rect width"), 754.0f, 494.0f, 11.0f, exOn ? dim : line,
            juce::Justification::centred, 280.0f);

    // Footer
    g.setColour (line);
    g.fillRect (20.0f, 522.0f, 880.0f, 0.8f);
    textAt (g, utf8 ("particledots \xc2\xb7 not affiliated with any module maker"), 20.0f, 546.0f, 12.0f, dim, juce::Justification::centredLeft, 400.0f);
    textAt (g, "v" + juce::String (JucePlugin_VersionString), 880.0f, 546.0f, 12.0f, dim, juce::Justification::centredRight, 100.0f);
}

void TetradPanel::resized()
{
    background.setBounds (getLocalBounds());
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

        const int top = exRowTop (ch);
        sineLevelKnobs[c].setBounds (kExKnobX[0] - 13, top + 13, 26, 26);
        asgnLevelKnobs[c].setBounds (kExKnobX[1] - 13, top + 13, 26, 26);
        shapeKnobs[c].setBounds (kExKnobX[2] - 13, top + 13, 26, 26);
        exWaveBoxes[c].setBounds (778, top + 4, 108, 18);
        shapeFromBoxes[c].setBounds (778, top + 26, 108, 18);
    }
    channelBox.setBounds (24, 468, 110, 24);
    exSwitch.setBounds (846, 72, 40, 20);
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
