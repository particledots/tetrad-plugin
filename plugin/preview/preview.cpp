// Offline window picture and plugin checks (not part of the plugin; only built with -DTETRAD_BUILD_PREVIEW=ON).
//   1. draws the window to picture files (default, the approved example, Waves view, divide, multiply, tempo with no
//      tempo from the host, outputs switched off, three sizes);
//   2. checks that all 33 ids exist, each has exactly one control, and each control moves exactly its own parameter;
//   3. checks saving and reloading: every setting, the random seed, the Lights | Waves choice and the window size;
//   4. checks the MIDI: everything that comes in passes through untouched, the outputs come out on CC 2-8, 34-40,
//      66-73, 91-94 on the chosen channel, the mod wheel and notes work, changing the channel and bypassing leave
//      nothing hanging, Logic's tempo and song position are followed, and Logic's block size makes no difference.
// Usage: TetradPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <iostream>
#include <map>
#include <set>

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok) { ++failures; std::cout << "FAIL: " << what << std::endl; }
    }

    juce::RangedAudioParameter* param (juce::AudioProcessor& p, const juce::String& id)
    {
        for (auto* x : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x))
                if (r->paramID == id) return r;
        return nullptr;
    }

    void setParam (juce::AudioProcessor& p, const juce::String& id, float real)
    {
        if (auto* r = param (p, id)) r->setValueNotifyingHost (r->convertTo0to1 (real));
        else expect (false, "no parameter " + id);
    }

    std::map<juce::String, float> snapshot (juce::AudioProcessor& p)
    {
        std::map<juce::String, float> m;
        for (auto* x : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x)) m[r->paramID] = r->getValue();
        return m;
    }

    void snapshotTo (juce::Component& c, const juce::File& file)
    {
        auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (image, out); }
    }

    // Logic's transport, as a stand-in.
    struct FakePlayHead : juce::AudioPlayHead
    {
        bool hasTempo = true, playing = false;
        double bpm = 120.0, ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            if (hasTempo) info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (playing);
            return info;
        }
    };

    struct Msg { long long sample; juce::MidiMessage m; };

    std::vector<Msg> runBlocks (TetradProcessor& p, double seconds, int block, const std::vector<Msg>& in = {}, bool bypassed = false,
                                FakePlayHead* head = nullptr)
    {
        std::vector<Msg> out;
        juce::AudioBuffer<float> audio (0, 8192);
        long long pos = 0, total = (long long) (seconds * 48000.0);
        juce::Random rng (5);
        while (pos < total)
        {
            int n = block > 0 ? block : 1 + rng.nextInt (1024);
            n = (int) juce::jmin<long long> (n, total - pos);
            audio.setSize (0, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : in)
                if (e.sample >= pos && e.sample < pos + n) midi.addEvent (e.m, (int) (e.sample - pos));
            if (bypassed) p.processBlockBypassed (audio, midi); else p.processBlock (audio, midi);
            for (const auto meta : midi) out.push_back ({ pos + meta.samplePosition, meta.getMessage() });
            pos += n;
            if (head && head->playing) head->ppq += n / 48000.0 * head->bpm / 60.0;
        }
        return out;
    }

    bool isOurCc (const juce::MidiMessage& m)
    {
        if (! m.isController()) return false;
        const int n = m.getControllerNumber();
        return n == 2 || n == 4 || n == 5 || n == 8 || n == 34 || n == 36 || n == 37 || n == 40 || (n >= 66 && n <= 73) || (n >= 91 && n <= 94);
    }
    bool isOurNote (const juce::MidiMessage& m) { return (m.isNoteOn() || m.isNoteOff()) && m.getNoteNumber() >= 49 && m.getNoteNumber() <= 52; }

    bool sameStream (const std::vector<Msg>& a, const std::vector<Msg>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].sample != b[i].sample || a[i].m.getDescription() != b[i].m.getDescription()) return false;
        return true;
    }

    juce::StringArray allIds()
    {
        juce::StringArray ids { "mode", "asgnWave", "tempoFollow", "rectOut", "outChannel" };
        for (int ch = 1; ch <= 4; ++ch)
            for (auto base : { "slider", "modFrom", "modDepth", "resetFrom", "sineOn", "asgnOn", "rectOn" })
                ids.add (juce::String (base) + juce::String (ch));
        return ids;
    }

    void pump (TetradProcessor& proc, double seconds)
    {
        for (int k = 0; k < (int) (seconds * 50.0); ++k)
        {
            runBlocks (proc, 0.02, 480);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
    }

    // The approved draft's example: phase mode, A at 0.5 Hz, B, C, D at 90, 180, 270 degrees, B moved by D's asgn,
    // D by B's sine, A reset by notes, C by A's rect, D's rect switched off.
    void approvedExample (TetradProcessor& p)
    {
        setParam (p, "mode", 1.0f);
        setParam (p, "modFrom2", 7.0f);  setParam (p, "modDepth2", 0.35f);
        setParam (p, "modFrom4", 4.0f);  setParam (p, "modDepth4", -0.4f);
        setParam (p, "resetFrom1", 1.0f);
        setParam (p, "resetFrom3", 2.0f);
        setParam (p, "rectOn4", 0.0f);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    const auto ids = allIds();

    // ---- 1 and 2: the window and its controls ------------------------------------------------------------------
    {
        TetradProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        expect (proc.getParameters().size() == 33 && ids.size() == 33, "33 parameters");
        for (auto& id : ids)
            expect (param (proc, id) != nullptr, "parameter id " + id);
        expect (proc.isMidiEffect() && proc.acceptsMidi() && proc.producesMidi(), "a MIDI effect");
        expect (proc.getTotalNumInputChannels() == 0 && proc.getTotalNumOutputChannels() == 0, "no audio channels");
        expect (param (proc, "mode")->getCurrentValueAsText() == "Free" && param (proc, "asgnWave")->getCurrentValueAsText() == "Triangle"
                && param (proc, "rectOut")->getCurrentValueAsText() == "CC" && param (proc, "tempoFollow")->getValue() < 0.5f,
                "defaults: free mode, triangle, rect out CC, Hz");
        expect (juce::roundToInt (param (proc, "outChannel")->convertFrom0to1 (param (proc, "outChannel")->getValue())) == 12, "MIDI out starts on channel 12");
        bool menusOff = true, switchesOn = true;
        for (int ch = 1; ch <= 4; ++ch)
        {
            menusOff = menusOff && param (proc, "modFrom" + juce::String (ch))->getCurrentValueAsText() == "Off"
                       && param (proc, "resetFrom" + juce::String (ch))->getCurrentValueAsText() == "Off"
                       && param (proc, "modDepth" + juce::String (ch))->getCurrentValueAsText() == "0%";
            for (auto base : { "sineOn", "asgnOn", "rectOn" }) switchesOn = switchesOn && param (proc, juce::String (base) + juce::String (ch))->getValue() > 0.5f;
        }
        expect (menusOff && switchesOn, "mod from and reset from start on Off, depth 0%, every output on");
        expect (TetradParams::modFromNames (0).joinIntoString (",") == "Off,Mod wheel,B sine,B asgn,C sine,C asgn,D sine,D asgn"
                && TetradParams::modFromNames (2).joinIntoString (",") == "Off,Mod wheel,A sine,A asgn,B sine,B asgn,D sine,D asgn"
                && TetradParams::resetFromNames (3).joinIntoString (",") == "Off,Notes,A rect,B rect,C rect",
                "menus in their fixed order, never the channel itself");
        expect (TetradParams::hzText (0.5) == "0.50 Hz" && TetradParams::hzText (20.0) == "20.0 Hz" && TetradParams::hzText (0.01) == "0.010 Hz"
                && TetradParams::hzText (0.003) == "333 s", "Hz read-outs (" + TetradParams::hzText (0.01) + ")");

        std::unique_ptr<TetradEditor> ed (dynamic_cast<TetradEditor*> (proc.createEditor()));
        expect (ed != nullptr, "editor opens");
        expect (ed->getWidth() == 920 && ed->getHeight() == 560, "window size 920 x 560");

        pump (proc, 0.3);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-default.png"));

        auto controls = ed->controls();
        std::set<juce::String> seen;
        for (auto& c : controls) seen.insert (c.paramId);
        expect (controls.size() == 33 && seen.size() == 33, "every parameter has exactly one control");
        for (auto& c : controls)
        {
            auto before = snapshot (proc);
            if (auto* s = dynamic_cast<juce::Slider*> (c.control)) s->setValue (s->getMinimum() + (s->getMaximum() - s->getMinimum()) * 0.83, juce::sendNotificationSync);
            else if (auto* b = dynamic_cast<juce::ComboBox*> (c.control)) b->setSelectedItemIndex (2, juce::sendNotificationSync);
            else if (auto* t = dynamic_cast<juce::Button*> (c.control)) t->setToggleState (! t->getToggleState(), juce::sendNotificationSync);
            else if (auto* ch = dynamic_cast<Choices*> (c.control)) ch->choose (ch->numItems() - 1);
            auto after = snapshot (proc);
            int changed = 0; juce::String which;
            for (auto& kv : after) if (kv.second != before[kv.first]) { ++changed; which = kv.first; }
            expect (changed == 1 && which == c.paramId, "control for " + c.paramId + " moves only " + c.paramId + " (moved " + which + ")");
            const auto inWindow = ed->getLocalArea (c.control, c.control->getLocalBounds());
            bool visible = true;
            for (auto* x = c.control; x != nullptr && x != ed.get(); x = x->getParentComponent()) visible = visible && x->isVisible();
            expect (visible && ! inWindow.isEmpty() && ed->getLocalBounds().contains (inWindow), "control for " + c.paramId + " is visible inside the window");
        }
        // The choice rows follow their parameter, both ways.
        for (auto& c : controls)
            if (auto* ch = dynamic_cast<Choices*> (c.control))
            {
                setParam (proc, c.paramId, 1.0f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                const bool follows = ch->getSelected() == 1;
                ch->choose (0);
                expect (follows && juce::roundToInt (param (proc, c.paramId)->convertFrom0to1 (param (proc, c.paramId)->getValue())) == 0,
                        "the " + c.paramId + " buttons follow the parameter and set it");
            }
        for (auto* x : proc.getParameters()) x->setValueNotifyingHost (x->getDefaultValue());

        // Menu text as drawn.
        std::vector<juce::ComboBox*> boxes;
        for (auto& c : ed->controls()) if (auto* b = dynamic_cast<juce::ComboBox*> (c.control)) boxes.push_back (b);
        expect (boxes.size() == 9 && boxes[0]->getText() == "channel 12" && boxes[1]->getText() == "Off", "menus read 'channel 12' and 'Off'");

        approvedExample (proc);
        pump (proc, 3.0);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-running.png"));
        expect (proc.view[1].marker.load() == 1 && proc.view[2].marker.load() == 2 && proc.view[3].marker.load() == 3 && proc.view[0].marker.load() == -1,
                "phase mode: the panel gets each slider's quarter for the mode light (90, 180, 270 degrees)");
        {
            const double blinkBefore = ed->panel.lightsLayer().framesDrawn;
            setParam (proc, "slider2", 0.55f);
            pump (proc, 0.1);
            expect (proc.view[1].marker.load() == 2 && ed->panel.lightsLayer().framesDrawn > blinkBefore, "moving B past 180 degrees changes its quarter (the mode light blinks)");
            setParam (proc, "slider2", 0.25f);
            pump (proc, 0.3);
        }

        setParam (proc, "mode", 2.0f);
        pump (proc, 1.0);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-divide.png"));
        expect (proc.view[2].ratio.load() == 5, "divide mode: C divided by 5 at its default slider");
        setParam (proc, "mode", 3.0f);
        setParam (proc, "asgnWave", 5.0f);
        pump (proc, 1.0);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-multiply.png"));

        setParam (proc, "tempoFollow", 1.0f);
        pump (proc, 0.3);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-tempo-no-host-tempo.png"));
        expect (proc.tempoMissing.load(), "following the tempo with no tempo from the host: the panel says so");
        setParam (proc, "tempoFollow", 0.0f);
        setParam (proc, "mode", 0.0f);
        for (int ch = 1; ch <= 4; ++ch) { setParam (proc, "sineOn" + juce::String (ch), ch % 2 == 0 ? 0.0f : 1.0f); setParam (proc, "asgnOn" + juce::String (ch), ch == 3 ? 0.0f : 1.0f); }
        pump (proc, 0.5);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-outputs-off.png"));
        for (auto* x : proc.getParameters()) x->setValueNotifyingHost (x->getDefaultValue());

        // Lights | Waves: starts on Lights, switches, saves, moves no parameter.
        expect (! ed->isWavesView() && ed->panel.viewChoices.getSelected() == 0, "window starts in Lights view");
        approvedExample (proc);
        const auto exampleParams = snapshot (proc);
        ed->panel.viewChoices.choose (1);
        expect (ed->isWavesView() && (bool) proc.getValueTreeState().state.getProperty (TetradPanel::wavesViewProperty, false),
                "Waves button shows the traces and keeps the choice in the saved state");
        pump (proc, 3.0);
        snapshotTo (*ed, outDir.getChildFile ("tetrad-window-waves.png"));
        expect (snapshot (proc) == exampleParams, "switching the view moves no parameter");
        ed->panel.viewChoices.choose (0);
        expect (! ed->isWavesView() && ! (bool) proc.getValueTreeState().state.getProperty (TetradPanel::wavesViewProperty, false), "Lights button shows the lights again");

        // The window only draws what moved: with everything slow, a small share of the panel each frame.
        for (auto* x : proc.getParameters()) x->setValueNotifyingHost (x->getDefaultValue());
        for (int ch = 1; ch <= 4; ++ch) setParam (proc, "slider" + juce::String (ch), 0.0f);
        pump (proc, 0.5);
        double a0 = ed->panel.lightsLayer().areaDrawn; int f0 = ed->panel.lightsLayer().framesDrawn;
        pump (proc, 2.0);
        const double slowShare = (ed->panel.lightsLayer().areaDrawn - a0) / juce::jmax (1, ed->panel.lightsLayer().framesDrawn - f0) / (920.0 * 560.0);
        for (auto* x : proc.getParameters()) x->setValueNotifyingHost (x->getDefaultValue());
        pump (proc, 0.5);
        a0 = ed->panel.lightsLayer().areaDrawn; f0 = ed->panel.lightsLayer().framesDrawn;
        pump (proc, 3.0);
        const double share = (ed->panel.lightsLayer().areaDrawn - a0) / juce::jmax (1, ed->panel.lightsLayer().framesDrawn - f0) / (920.0 * 560.0);
        std::cout << "window: " << juce::String (100.0 * share, 1) << "% of the panel drawn again per frame at the defaults, "
                  << juce::String (100.0 * slowShare, 1) << "% with all four at 0.01 Hz" << std::endl;
        expect (share > 0.002 && share < 0.20, "at the defaults the window draws only what moved (" + juce::String (100.0 * share, 1) + "% of the panel per frame)");
        expect (slowShare < share, "slow LFOs: even less is drawn again (" + juce::String (100.0 * slowShare, 1) + "%)");

        bool rising = true;
        for (int k = 1; k <= 100; ++k) rising = rising && TetradStyle::lightAlpha (k / 100.0) >= TetradStyle::lightAlpha ((k - 1) / 100.0);
        expect (std::fabs (TetradStyle::lightAlpha (0.0) - 0.10f) < 1.0e-6f && std::fabs (TetradStyle::lightAlpha (1.0) - 1.0f) < 1.0e-6f
                && TetradStyle::lightAlpha (0.3) > 0.15f + 0.85f * 0.3f && rising, "light brightness curve as on Ogdoad 0.2.1");
    }

    // ---- Resizing: 80% to 200%, proportions locked; everything stays inside -------------------------------------
    {
        TetradProcessor proc; proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<TetradEditor> ed (dynamic_cast<TetradEditor*> (proc.createEditor()));
        expect (ed->isResizable() && ed->getConstrainer() != nullptr, "the window is resizable");
        auto* con = ed->getConstrainer();
        const double ratio = (double) TetradEditor::kWidth / TetradEditor::kHeight;
        auto clamp = [&] (int w, int h)
        {
            juce::Rectangle<int> b (0, 0, w, h);
            con->checkBounds (b, juce::Rectangle<int> (0, 0, 920, 560), juce::Rectangle<int> (0, 0, 4000, 3000), false, false, false, true);
            return b;
        };
        auto big = clamp (3000, 3000), small = clamp (100, 100), mid = clamp (1200, 1200);
        expect (big.getWidth() <= 1840 && big.getHeight() <= 1120 && small.getWidth() >= 736 && small.getHeight() >= 448
                && std::fabs ((double) mid.getWidth() / mid.getHeight() - ratio) < 0.01 && std::fabs ((double) big.getWidth() / big.getHeight() - ratio) < 0.01,
                "size limits 80% to 200% and the proportions stay locked");

        struct Size { int w; const char* name; } sizes[] = { { TetradEditor::kMaxWidth, "large" }, { TetradEditor::kMinWidth, "small" }, { 1380, "medium" } };
        for (auto& sz : sizes)
        {
            ed->setSize (sz.w, TetradEditor::heightForWidth (sz.w));
            const double scale = (double) sz.w / TetradEditor::kWidth;
            expect (std::fabs (ed->panel.getTransform().getScaleFactor() - scale) < 1.0e-3, juce::String ("panel scale at ") + sz.name + " is " + juce::String (scale, 2));
            bool inside = true;
            std::vector<juce::Component*> all;
            for (auto& c : ed->controls()) all.push_back (c.control);
            all.push_back (&ed->panel.viewChoices);
            for (auto* c : all)
            {
                const auto r = ed->getLocalArea (c, c->getLocalBounds());
                inside = inside && ed->getLocalBounds().contains (r) && ! r.isEmpty();
            }
            expect (inside, juce::String ("every control inside the window at ") + sz.name + " size");
            expect ((int) proc.getValueTreeState().state.getProperty (TetradEditor::widthProperty, -1) == sz.w, juce::String ("the ") + sz.name + " width is kept in the saved state");
            pump (proc, 0.4);
            snapshotTo (*ed, outDir.getChildFile (juce::String ("tetrad-window-") + sz.name + ".png"));
        }
    }

    // ---- 3: saving and reloading ----------------------------------------------------------------------------------
    {
        TetradProcessor a; a.prepareToPlay (48000.0, 512);
        approvedExample (a);
        setParam (a, "asgnWave", 4.0f); setParam (a, "rectOut", 2.0f); setParam (a, "tempoFollow", 1.0f); setParam (a, "outChannel", 5.0f);
        setParam (a, "slider3", 0.11f); setParam (a, "asgnOn2", 0.0f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        TetradProcessor b; b.prepareToPlay (48000.0, 512);
        expect (b.randomSeed() != a.randomSeed(), "two new copies of the plugin get different random seeds");
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        expect (snapshot (a) == snapshot (b), "every setting comes back after save and reload");
        expect (b.randomSeed() == a.randomSeed(), "the random seed comes back, so the random waves repeat");
        expect (! b.getValueTreeState().state.hasProperty (TetradProcessor::seedProperty), "the seed is kept out of the parameter list");
        setParam (a, "tempoFollow", 0.0f); setParam (b, "tempoFollow", 0.0f);
        TetradProcessor a2; a2.setStateInformation (mb.getData(), (int) mb.getSize()); setParam (a2, "tempoFollow", 0.0f);
        a2.prepareToPlay (48000.0, 512); b.prepareToPlay (48000.0, 512);
        auto ra = runBlocks (a2, 3.0, 512), rb = runBlocks (b, 3.0, 512);
        expect (sameStream (ra, rb) && ! ra.empty(), "two copies opened from the same project move exactly alike (zero phase, same seed)");

        // A project with no seed saved (or a broken one) opens with its settings and a seed of its own.
        auto xml = a.getValueTreeState().copyState().createXml();
        juce::MemoryBlock noSeed; juce::AudioProcessor::copyXmlToBinary (*xml, noSeed);
        TetradProcessor c; const auto cSeed = c.randomSeed();
        c.setStateInformation (noSeed.getData(), (int) noSeed.getSize());
        expect (snapshot (c) == snapshot (a) && c.randomSeed() == cSeed, "a project without a saved seed opens with its settings");
        xml->setAttribute (TetradProcessor::seedProperty, "broken");
        juce::MemoryBlock broken; juce::AudioProcessor::copyXmlToBinary (*xml, broken);
        TetradProcessor d; const auto dSeed = d.randomSeed();
        d.setStateInformation (broken.getData(), (int) broken.getSize());
        expect (snapshot (d) == snapshot (a) && d.randomSeed() == dSeed, "a broken saved seed is ignored safely");

        // The Lights | Waves choice and the window size are saved with the project.
        a.getValueTreeState().state.setProperty (TetradPanel::wavesViewProperty, true, nullptr);
        a.getValueTreeState().state.setProperty (TetradEditor::widthProperty, 1380, nullptr);
        juce::MemoryBlock withView; a.getStateInformation (withView);
        TetradProcessor f; f.setStateInformation (withView.getData(), (int) withView.getSize());
        std::unique_ptr<TetradEditor> edF (dynamic_cast<TetradEditor*> (f.createEditor()));
        expect (edF->isWavesView() && edF->panel.viewChoices.getSelected() == 1, "Waves view is saved and comes back");
        expect (edF->getWidth() == 1380 && edF->getHeight() == TetradEditor::heightForWidth (1380), "the window size comes back");
        for (auto pr : { std::pair<int, int> { 5000, TetradEditor::kMaxWidth }, { 100, TetradEditor::kMinWidth } })
        {
            TetradProcessor g1; g1.getValueTreeState().state.setProperty (TetradEditor::widthProperty, pr.first, nullptr);
            juce::MemoryBlock gb; g1.getStateInformation (gb);
            TetradProcessor g2; g2.setStateInformation (gb.getData(), (int) gb.getSize());
            std::unique_ptr<TetradEditor> edG (dynamic_cast<TetradEditor*> (g2.createEditor()));
            expect (edG->getWidth() == pr.second, "saved window width " + juce::String (pr.first) + " reopens at " + juce::String (pr.second));
        }
        TetradProcessor h;
        std::unique_ptr<TetradEditor> edH (dynamic_cast<TetradEditor*> (h.createEditor()));
        expect (! edH->isWavesView() && edH->getWidth() == TetradEditor::kWidth, "a new copy opens on Lights at the normal size");
    }

    // ---- 4: MIDI --------------------------------------------------------------------------------------------------
    {
        TetradProcessor p; p.prepareToPlay (48000.0, 512);
        std::vector<Msg> in {
            { 100, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
            { 2000, juce::MidiMessage::controllerEvent (1, 7, 90) },
            { 3000, juce::MidiMessage::controllerEvent (16, 20, 64) },      // Ogdoad's LFO 1, coarse
            { 3000, juce::MidiMessage::controllerEvent (16, 52, 10) },      // and fine
            { 3500, juce::MidiMessage::controllerEvent (13, 2, 1) },        // channel 13, one of our numbers: not ours
            { 30000, juce::MidiMessage::noteOff (1, 60) },
            { 40000, juce::MidiMessage::pitchWheel (2, 9000) },
        };
        auto out = runBlocks (p, 2.0, 512, in);
        std::vector<Msg> others; int ours = 0; std::set<int> numbers;
        for (auto& e : out)
        {
            if (isOurCc (e.m) && e.m.getChannel() == 12) { ++ours; numbers.insert (e.m.getControllerNumber()); }
            else others.push_back (e);
        }
        bool passOk = others.size() == in.size();
        for (size_t i = 0; passOk && i < in.size(); ++i)
            passOk = others[i].sample == in[i].sample && others[i].m.getDescription() == in[i].m.getDescription();
        expect (passOk, "notes, CC, Ogdoad's CC and pitch bend pass through unchanged, at the same moment");
        expect (ours > 500, "the outputs come out as CC on channel 12 (" + juce::String (ours) + " messages in 2 s)");
        expect (numbers.size() == 20, "all 20 CC numbers used (" + juce::String ((int) numbers.size()) + "), nothing else");

        // A new channel: the rect gates that are high get 0 on the old channel, then everything moves to the new one.
        setParam (p, "rectOut", 2.0f);
        setParam (p, "slider1", 1.0f);
        runBlocks (p, 0.3, 512);
        setParam (p, "outChannel", 3.0f);
        auto out3 = runBlocks (p, 0.5, 512);
        bool rest3 = true; int closedOld = 0, noteOffsOld = 0, onNew = 0;
        for (auto& e : out3)
        {
            if (e.m.getChannel() == 12) { if (e.m.isController() && e.m.getControllerValue() == 0) ++closedOld; if (e.m.isNoteOff()) ++noteOffsOld; continue; }
            rest3 = rest3 && e.m.getChannel() == 3 && (isOurCc (e.m) || isOurNote (e.m));
            ++onNew;
        }
        expect (rest3 && onNew > 100, "MIDI out channel 3: everything moves to channel 3");
        expect (closedOld >= 1, "changing the channel ends the high rect gates on the old one (" + juce::String (closedOld) + ")");
        setParam (p, "outChannel", 12.0f);

        // Bypass: MIDI passes, nothing new is added (only the end of a note or gate that was on).
        auto byp = runBlocks (p, 0.5, 512, { { 50, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80) } }, true);
        int added = 0; bool onlyEnds = true;
        for (auto& e : byp) if (! (e.m.isNoteOn() && e.m.getNoteNumber() == 64)) { ++added; onlyEnds = onlyEnds && (e.m.isNoteOff() || (e.m.isController() && e.m.getControllerValue() == 0)); }
        expect (onlyEnds && added <= 8, "bypassed: notes pass, nothing added except ends of what was on");

        // No note hanging anywhere in a long run with Rect out on Both and fast channels.
        TetradProcessor q; q.prepareToPlay (48000.0, 512);
        setParam (q, "rectOut", 1.0f);
        for (int ch = 1; ch <= 4; ++ch) setParam (q, "slider" + juce::String (ch), 0.8f + 0.05f * ch);
        auto nq = runBlocks (q, 3.0, 0);
        nq.push_back ({ 0, juce::MidiMessage() });
        std::map<int, int> balance; int noteCount = 0;
        for (auto& e : nq) if (isOurNote (e.m)) { balance[e.m.getNoteNumber()] += e.m.isNoteOn() ? 1 : -1; if (e.m.isNoteOn()) ++noteCount; }
        bool balanced = true; for (auto& kv : balance) balanced = balanced && (kv.second == 0 || kv.second == 1);
        expect (balanced && noteCount > 50 && balance.size() == 4, "rect notes 49-52 on and off in pairs (" + juce::String (noteCount) + " notes)");

        // Notes reset a channel; the mod wheel moves one.
        TetradProcessor r; r.prepareToPlay (48000.0, 512);
        setParam (r, "resetFrom1", 1.0f);
        setParam (r, "modFrom2", 1.0f); setParam (r, "modDepth2", 1.0f);
        TetradProcessor s; s.prepareToPlay (48000.0, 512);
        juce::MemoryBlock sameSeed; r.getStateInformation (sameSeed); s.setStateInformation (sameSeed.getData(), (int) sameSeed.getSize());
        runBlocks (r, 1.0, 512, { { 30000, juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90) }, { 100, juce::MidiMessage::controllerEvent (1, 1, 127) } });
        runBlocks (s, 1.0, 512);
        expect (std::fabs (r.view[0].sine.load() - s.view[0].sine.load()) > 1e-3, "a note restarts a channel set to reset from Notes");
        expect (r.view[1].hz.load() > s.view[1].hz.load() * 20.0f, "the mod wheel speeds up a channel set to mod from Mod wheel");

        // Following Logic's tempo while it plays: A on 1/4 at 100 BPM runs at 1.67 Hz, lined up with the song.
        TetradProcessor t; t.prepareToPlay (48000.0, 512);
        FakePlayHead head; head.bpm = 100.0; head.playing = true; head.ppq = 16.0;
        t.setPlayHead (&head);
        setParam (t, "tempoFollow", 1.0f);
        setParam (t, "slider1", 10.0f / 15.0f);
        runBlocks (t, 1.0, 512, {}, false, &head);
        expect (std::fabs (t.view[0].hz.load() - 100.0f / 60.0f) < 1e-4f && ! t.tempoMissing.load(), "following Logic: 1/4 at 100 BPM runs at 1.67 Hz");
        t.setPlayHead (nullptr);
    }

    // ---- block sizes inside the real plugin --------------------------------------------------------------------------
    {
        auto stream = [] (int block)
        {
            TetradProcessor p; p.prepareToPlay (48000.0, 1024);
            juce::MemoryBlock fixedSeed;
            {
                TetradProcessor x; x.getValueTreeState().state.setProperty (TetradProcessor::seedProperty, 4242, nullptr);
                auto xml = x.getValueTreeState().copyState().createXml();
                xml->setAttribute (TetradProcessor::seedProperty, 4242);
                juce::AudioProcessor::copyXmlToBinary (*xml, fixedSeed);
            }
            p.setStateInformation (fixedSeed.getData(), (int) fixedSeed.getSize());
            approvedExample (p);
            setParam (p, "asgnWave", 5.0f); setParam (p, "rectOut", 2.0f);
            return runBlocks (p, 5.0, block, { { 12345, juce::MidiMessage::noteOn (1, 50, (juce::uint8) 90) }, { 99999, juce::MidiMessage::controllerEvent (1, 1, 64) } });
        };
        auto ref = stream (512);
        for (int b : { 64, 333, 1024, 0 })
            expect (sameStream (stream (b), ref), "same MIDI stream with block size " + (b > 0 ? juce::String (b) : juce::String ("random")));
        expect (ref.size() > 1000, "the comparison stream is not empty");
    }

    if (failures == 0) std::cout << "preview checks: " << checks << " checks passed" << std::endl;
    else std::cout << "preview checks: " << failures << " of " << checks << " checks FAILED" << std::endl;
    return failures == 0 ? 0 : 1;
}
