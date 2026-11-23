#include "Parameters.h"
#include "tetrad/Engine.h"

namespace TetradParams
{
    juce::StringArray modFromNames (int ch)
    {
        juce::StringArray names { "Off", "Mod wheel" };
        for (int k = 0; k < 3; ++k)
        {
            const auto& other = channelLetter[tetrad::otherChannel (ch, k)];
            names.add (other + " sine");
            names.add (other + " asgn");
        }
        return names;
    }

    juce::StringArray resetFromNames (int ch)
    {
        juce::StringArray names { "Off", "Notes" };
        for (int k = 0; k < 3; ++k)
            names.add (channelLetter[tetrad::otherChannel (ch, k)] + " rect");
        return names;
    }

    juce::String hzText (double hz)
    {
        if (hz >= 10.0)  return juce::String (hz, 1) + " Hz";
        if (hz >= 0.1)   return juce::String (hz, 2) + " Hz";
        if (hz >= 0.0095) return juce::String (hz, 3) + " Hz";
        return juce::String (juce::roundToInt (1.0 / hz)) + " s";
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        const int v = 1;
        auto percent = [] (float x, int) { const int p = roundToInt (x * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; };
        auto plain = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { mode, v }, "Mode", modeNames, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { asgnWave, v }, "Asgn wave", waveNames, 0));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { tempoFollow, v }, "Follow Logic tempo", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { rectOut, v }, "Rect out", rectOutNames, 0));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { outChannel, v }, "MIDI out channel", 1, 16, tetrad::kDefaultChannel));

        const tetrad::Settings defaults;
        for (int ch = 0; ch < 4; ++ch)
        {
            const String c = channelLetter[ch];
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { slider[ch], v }, c + " slider", NormalisableRange<float> (0.0f, 1.0f),
                (float) defaults.slider[ch], AudioParameterFloatAttributes().withStringFromValueFunction (plain)));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { modFrom[ch], v }, c + " mod from", modFromNames (ch), 0));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { modDepth[ch], v }, c + " mod depth", NormalisableRange<float> (-1.0f, 1.0f),
                0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { resetFrom[ch], v }, c + " reset from", resetFromNames (ch), 0));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { sineOn[ch], v }, c + " sine on", true));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { asgnOn[ch], v }, c + " asgn on", true));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { rectOn[ch], v }, c + " rect on", true));
        }
        return layout;
    }
}
