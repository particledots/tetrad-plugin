// The list of controls Logic can see and automate.
//
// The short ids below are what Logic stores in saved projects and automation. They were chosen once, before the
// first build (approved 2026-11-22), and are never renamed. 1-4 = channels A-D (numbers, not letters, as Strange Drift).
// Tetrad Ex (milestone 2, 0.2.0), with the ids reserved before the first build: exOn, sineLevel1-4, asgnLevel1-4,
// exWave1-4, shapeAmount1-4, shapeFrom1-4, shapeTarget1-4.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace TetradParams
{
    inline constexpr const char* mode        = "mode";          // Free, Phase, Divide, Multiply
    inline constexpr const char* asgnWave    = "asgnWave";      // the six asgn waves
    inline constexpr const char* tempoFollow = "tempoFollow";   // off: Hz; on: note lengths at Logic's tempo
    inline constexpr const char* rectOut     = "rectOut";       // CC, Note, Both
    inline constexpr const char* outChannel  = "outChannel";    // MIDI channel the outputs go out on, 1..16 (12 by default)
    inline constexpr const char* slider[4]    = { "slider1", "slider2", "slider3", "slider4" };
    inline constexpr const char* modFrom[4]   = { "modFrom1", "modFrom2", "modFrom3", "modFrom4" };
    inline constexpr const char* modDepth[4]  = { "modDepth1", "modDepth2", "modDepth3", "modDepth4" };
    inline constexpr const char* resetFrom[4] = { "resetFrom1", "resetFrom2", "resetFrom3", "resetFrom4" };
    inline constexpr const char* sineOn[4]    = { "sineOn1", "sineOn2", "sineOn3", "sineOn4" };
    inline constexpr const char* asgnOn[4]    = { "asgnOn1", "asgnOn2", "asgnOn3", "asgnOn4" };
    inline constexpr const char* rectOn[4]    = { "rectOn1", "rectOn2", "rectOn3", "rectOn4" };

    inline constexpr const char* exOn = "exOn";               // Tetrad Ex on / off (off: Tetrad moves exactly as 0.1.0)
    inline constexpr const char* sineLevel[4]   = { "sineLevel1", "sineLevel2", "sineLevel3", "sineLevel4" };
    inline constexpr const char* asgnLevel[4]   = { "asgnLevel1", "asgnLevel2", "asgnLevel3", "asgnLevel4" };
    inline constexpr const char* exWave[4]      = { "exWave1", "exWave2", "exWave3", "exWave4" };
    inline constexpr const char* shapeAmount[4] = { "shapeAmount1", "shapeAmount2", "shapeAmount3", "shapeAmount4" };
    inline constexpr const char* shapeFrom[4]   = { "shapeFrom1", "shapeFrom2", "shapeFrom3", "shapeFrom4" };
    inline constexpr const char* shapeTarget[4] = { "shapeTarget1", "shapeTarget2", "shapeTarget3", "shapeTarget4" };

    inline constexpr int kNumParameters = 5 + 4 * 7 + 1 + 4 * 6;

    inline const juce::StringArray modeNames { "Free", "Phase", "Divide", "Multiply" };
    inline const juce::StringArray waveNames { "Triangle", "Saw down", "Saw up", "Trapezoid", "Step random", "Smooth random" };
    inline const juce::StringArray rectOutNames { "CC", "Note", "Both" };
    inline const juce::String channelLetter[4] { "A", "B", "C", "D" };
    inline const juce::StringArray exWaveNames { "Main", "Triangle", "Saw down", "Saw up", "Trapezoid", "Step random", "Smooth random" };
    inline const juce::StringArray shapeTargetNames { "Fold", "Wave", "Width" };

    // The menus of channel ch, in their fixed order: Off, Mod wheel, then the other three channels' sine and asgn.
    juce::StringArray modFromNames (int ch);
    // Shape from (Tetrad Ex) uses the same menu as mod from.
    // Off, Notes, then the other three channels' rect.
    juce::StringArray resetFromNames (int ch);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // "0.50 Hz", "12.0 Hz", "0.010 Hz", "100 s" for a frequency.
    juce::String hzText (double hz);
}
