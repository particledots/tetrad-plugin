// Tetrad's outputs, menus and MIDI numbers. Every number and every menu order here is fixed from the first build:
// saved projects and the mapper rely on them.
//
// Four channels (A, B, C, D), each with three outputs, all on one MIDI channel (12 by default):
//
//   channel          A        B        C        D
//   sine  coarse     2        4        5        8      fine 34, 36, 37, 40   (the standard "coarse + 32" partners)
//   asgn  coarse    66       68       70       72      fine 67, 69, 71, 73   (direct pairs)
//   rect  (127/0)   91       92       93       94
//   rect note       49       50       51       52      (C#2 to E2; Note / Both: 20 ms at each rising edge)
//
// None of these CC numbers is used by Ogdoad (CC 12-31, 44-63, 102-105), Strange Drift (CC 106-119), Hillside
// (CC 3, 9, 35, 41, 85-89), Bryn (CC 80-83, 90) or particlehold (CC 75-79), on any channel, so all six plugins can
// share a track, even all on one channel. Every CC that MIDI leaves without a job is already taken by those five, so
// Tetrad's have standard jobs (breath, foot, portamento time, balance and their fine parts; pedal switches 66-69 and
// sound controllers 1-4 at 70-73; effect depths 91-94). That is safe: the Tetrad Mapper stops them before the instrument, and none of those plugins listens to them.
// Avoided on purpose: bank select (0, 32), mod wheel (1, 33), data entry (6, 38), volume (7), pan (10), expression
// (11), sustain (64), data increment / RPN / NRPN (96-101), channel mode messages (120-127).
// Kept free: CC 65, 74, 84, 95 (reserved for Tetrad Ex; Ex turned out to need no outputs of its own, so they stay unused).
// The rect notes stay outside notes 36-48 (Ogdoad 36-39, Hillside 40-44, Bryn 45-47, particlehold 48).

#pragma once

namespace tetrad
{
    inline constexpr int kNumChannels = 4;                  // A, B, C, D
    inline constexpr int kNumSmooth = 8;                    // sine A-D, then asgn A-D (the 14-bit lanes)

    inline constexpr int kSineCoarse[kNumChannels] = { 2, 4, 5, 8 };
    inline constexpr int kSineFine[kNumChannels]   = { 34, 36, 37, 40 };
    inline constexpr int kAsgnCoarse[kNumChannels] = { 66, 68, 70, 72 };
    inline constexpr int kAsgnFine[kNumChannels]   = { 67, 69, 71, 73 };
    inline constexpr int kRectCc[kNumChannels]     = { 91, 92, 93, 94 };
    inline constexpr int kRectNote[kNumChannels]   = { 49, 50, 51, 52 };
    inline constexpr int kSpareCc[4]               = { 65, 74, 84, 95 };
    inline constexpr int kNoteVelocity = 100;
    inline constexpr double kNoteSeconds = 0.020;           // rect notes last 20 ms; edges closer together merge

    inline constexpr int kDefaultChannel = 12;

    // The top rate: what 14-bit CC sent at most every 6 ms still draws smoothly (8 or more points per cycle).
    inline constexpr double kTopHz = 20.0;
    // The sliders' range in Hz mode (the module's slider runs 0.01 to 100 Hz).
    inline constexpr double kSliderMinHz = 0.01;
    // The slowest a channel ever goes (as the module with full negative CV: 9.76 microhertz).
    inline constexpr double kBottomHz = 9.76e-6;

    // ---- Menus (order fixed: saved projects store the index) --------------------------------------------------------
    enum class Mode : int { free = 0, phase, divide, multiply };
    inline constexpr int kNumModes = 4;

    enum class Wave : int { triangle = 0, sawDown, sawUp, trapezoid, stepRandom, smoothRandom };
    inline constexpr int kNumWaves = 6;

    enum class RectOut : int { cc = 0, note, both };

    // Divide and multiply ratios, as on the module.
    inline constexpr int kNumRatios = 8;
    inline constexpr int kRatios[kNumRatios] = { 1, 2, 3, 4, 5, 8, 16, 32 };

    // Note lengths when following Logic's tempo, in beats (quarter notes; 1 bar = 4 beats), slowest first, so a slider
    // pushed up goes faster.
    inline constexpr int kNumLengths = 16;
    inline constexpr double kLengthBeats[kNumLengths] = { 128.0, 64.0, 32.0, 16.0, 8.0, 4.0, 3.0, 2.0, 1.5, 4.0 / 3.0,
                                                          1.0, 0.75, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25 };
    inline constexpr const char* kLengthNames[kNumLengths] = { "32 bars", "16 bars", "8 bars", "4 bars", "2 bars", "1 bar",
                                                               "1/2 dot", "1/2", "1/4 dot", "1/2 trip", "1/4", "1/8 dot",
                                                               "1/4 trip", "1/8", "1/8 trip", "1/16" };

    // Mod from (per channel): Off, Mod wheel, then the other three channels' sine and asgn, in A-D order, skipping the
    // channel itself (a channel never modulates itself).
    inline constexpr int kNumModSources = 8;
    // Reset from (per channel): Off, Notes, then the other three channels' rect, in A-D order, skipping itself.
    inline constexpr int kNumResetSources = 5;

    // The channel that entry k (0, 1, 2) of the "other channels" part of a menu means, for channel `self`.
    inline constexpr int otherChannel (int self, int k) { return k < self ? k : k + 1; }

    // How far "mod from" reaches at full depth.
    inline constexpr double kModOctaves = 5.0;              // free mode, and channel A in every mode: +-5 octaves
    inline constexpr double kModCycles = 2.5;               // phase mode, B-D: +-2.5 cycles of phase shift
    inline constexpr double kModSteps = 7.0;                // divide / multiply ratios and note lengths: +-7 steps (+-15 for lengths)

    // ---- Tetrad Ex (milestone 2; menus in fixed order from 0.2.0) ----------------------------------------------------
    // Per channel: sine level, asgn level (0..1, as the expander's output attenuators), the channel's own asgn wave
    // ("Main" follows the global wave), and one shape: an amount (-1..+1), where it comes from, and what it shapes.
    // Shape from: the same menu as Mod from (Off, Mod wheel, then the other three channels' sine and asgn). Off: the
    // amount itself is the shape. Otherwise the shape is the amount times that source.
    inline constexpr int kNumExWaves = 7;                   // Main, then the six waves in the Wave order
    enum class ShapeTarget : int { fold = 0, wave, width };  // fold the sine, switch the asgn wave, the rect's width
    inline constexpr int kNumShapeTargets = 3;
    inline constexpr int kNumShapeSources = kNumModSources;
    inline constexpr double kMaxFold = 5.0;                 // full shape folds the sine 5 times as far (amplitude stays +-1)
    inline constexpr int kWaveSteps = 5;                    // full shape steps the asgn wave 5 places along the menu
    inline constexpr double kWidthRange = 0.45;             // full shape: the rect is high for 5% to 95% of the cycle
    // A rect whose width is being shaped rises at most once every 48 ms (about 20 times a second, the top rate), so
    // fast width modulation cannot multiply its edges, notes and resets beyond what an unshaped rect makes.
    inline constexpr double kRectRiseGapSeconds = 0.048;
}
