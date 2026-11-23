// What the open window costs, measured in a real window on the build Mac (not part of the plugin).
// Built twice by the workflow: from this version, and (as that version's preview program) from 0.1.0, so the two can
// be compared on the same Mac in the same way.
//
// The processor runs as in Logic (a block every 10 ms on its own thread), and the process's CPU time is read
// (all threads, as Activity Monitor counts it): with no window, with the window open at 100%, and at 150%.
// Usage: TetradWindowCost <label> [folder for screenshots] [seconds per reading, default 8]

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <sys/resource.h>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
    double cpuSeconds()
    {
        rusage ru {};
        getrusage (RUSAGE_SELF, &ru);
        return (double) ru.ru_utime.tv_sec + (double) ru.ru_utime.tv_usec * 1.0e-6 + (double) ru.ru_stime.tv_sec + (double) ru.ru_stime.tv_usec * 1.0e-6;
    }

    struct AudioThread : juce::Thread
    {
        explicit AudioThread (TetradProcessor& p) : juce::Thread ("audio"), proc (p) {}
        void run() override
        {
            juce::AudioBuffer<float> audio (0, 480);
            juce::MidiBuffer midi;
            while (! threadShouldExit())
            {
                midi.clear();
                proc.processBlock (audio, midi);
                wait (10);
            }
        }
        TetradProcessor& proc;
    };

    // Percent of one core used by the whole process while the message loop runs for `seconds`.
    double measure (double seconds)
    {
        const double c0 = cpuSeconds(), t0 = juce::Time::getMillisecondCounterHiRes();
        juce::MessageManager::getInstance()->runDispatchLoopUntil ((int) (seconds * 1000.0));
        const double wall = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        return 100.0 * (cpuSeconds() - c0) / wall;
    }

    void screenshot (const juce::String& folder, const juce::String& name)
    {
       #if JUCE_MAC
        if (folder.isEmpty()) return;
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (folder).getChildFile (name);
        file.getParentDirectory().createDirectory();
        std::system (("screencapture -x \"" + file.getFullPathName() + "\"").toRawUTF8());
       #else
        juce::ignoreUnused (folder, name);
       #endif
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
   #if JUCE_MAC
    juce::Process::setDockIconVisible (true);
   #endif
    const juce::String label = argc > 1 ? juce::String (argv[1]) : juce::String ("this build");
    const juce::String shots = argc > 2 ? juce::String (argv[2]) : juce::String();
    const double seconds = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 8.0;

    TetradProcessor proc;
    proc.prepareToPlay (48000.0, 480);
    AudioThread audio (proc);
    audio.startThread();

    measure (2.0);
    const double closed = measure (seconds);
    std::cout << "windowcost " << label << ": window closed " << juce::String (closed, 1) << "% of a core" << std::endl;

    auto openWindow = [&] (const juce::String& name, int width, bool shot)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setTopLeftPosition (40, 60);
       #if JUCE_MAC
        ed->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
       #else
        ed->addToDesktop (0);
       #endif
        ed->setSize (width, juce::roundToInt (width * 560.0 / 920.0));
        ed->setVisible (true);
        ed->toFront (true);
        measure (2.0);
        const double open = measure (seconds);
        if (shot) screenshot (shots, "real-window-" + label + "-" + name + ".png");
        ed->removeFromDesktop();
        std::cout << "windowcost " << label << " " << name << ": open " << juce::String (open, 1) << "% (+" << juce::String (open - closed, 1) << ")" << std::endl;
    };

   #ifdef TETRAD_WINDOWCOST_VARIANTS
    // This build: the same window drawn in different ways, twice round, to compare.
    struct Variant { const char* name; double fps; bool turns, cache; };
    const Variant variants[] = { { "still", 0.0, false, false }, { "5fps", 5.0, false, false }, { "15fps", 15.0, false, false },
                                 { "30fps", 30.0, false, false }, { "30fps-rows-take-turns", 30.0, true, false },
                                 { "30fps-cached-controls", 30.0, false, true } };
    for (int round = 0; round < 1; ++round)
        for (auto& v : variants)
        {
            LightsLayer::drawSettings = { v.fps, v.turns, v.cache };
            openWindow (v.name, 920, round == 0);
        }
    LightsLayer::drawSettings = {};
   #else
    openWindow ("100", 920, true);
    openWindow ("150", 1380, true);
   #endif
    audio.stopThread (2000);

    return 0;
}
