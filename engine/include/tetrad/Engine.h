// The Tetrad engine: four linked LFO channels (A, B, C, D) with sine, asgn and rect outputs, in four modes, sent as
// smooth 14-bit CC (see Outputs.h, Waves.h and CcStream.h).
//
// Plain C++17. Nothing here knows about JUCE, Logic or MIDI devices; the plugin hands in the control values, the
// incoming mod wheel and notes, and Logic's tempo and position, and gets back CC messages and notes placed at exact
// sample positions.
//
// How the channels move. Each channel has a position counted in cycles (its phase is the part after the point):
//   - free mode: all four run on their own (Hz, or a note length when following Logic's tempo);
//   - phase mode: B, C, D follow A, each delayed by its slider (0-360 degrees) plus "mod from";
//   - divide mode: B, C, D run at A's speed divided by 1, 2, 3, 4, 5, 8, 16 or 32;
//   - multiply mode: B, C, D run at A's speed times those ratios (stepped down so nothing goes above 20 Hz).
//   Entering a mode restarts A and the others with it, so they all start together (as on the module).
// Everything is worked out at ticks of 32 samples, counted from the start, so nothing depends on how Logic slices the
// audio into blocks. "Mod from" and "reset from" read the other channels as they were one tick earlier, so the order
// in which the channels are worked out never matters.

#pragma once

#include "tetrad/CcStream.h"
#include "tetrad/Outputs.h"
#include "tetrad/Waves.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tetrad
{
    // The sliders (0..1) read as Hz, degrees, ratios and note lengths.
    inline double sliderHz (double x)            { x = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); return kSliderMinHz * std::pow (kTopHz / kSliderMinHz, x); }
    inline double sliderForHz (double hz)        { return std::log (hz / kSliderMinHz) / std::log (kTopHz / kSliderMinHz); }
    inline int stepIndex (double x, int count)   { const double v = std::floor (x * (count - 1) + 0.5); return (int) (v < 0.0 ? 0.0 : (v > count - 1 ? count - 1 : v)); }
    inline int sliderRatioIndex (double x)       { return stepIndex (x, kNumRatios); }
    inline int sliderLengthIndex (double x)      { return stepIndex (x, kNumLengths); }

    struct Settings
    {
        int    mode = 0;                                    // Mode
        int    asgnWave = 0;                                // Wave
        bool   tempoFollow = false;                         // false: Hz; true: note lengths at Logic's tempo
        int    rectOut = 0;                                 // RectOut
        double slider[kNumChannels]   { sliderForHz (0.5), 0.25, 0.5, 0.75 };   // A at 0.5 Hz
        int    modFrom[kNumChannels]  { 0, 0, 0, 0 };       // see kNumModSources
        double modDepth[kNumChannels] { 0.0, 0.0, 0.0, 0.0 };   // -1..+1
        int    resetFrom[kNumChannels] { 0, 0, 0, 0 };      // see kNumResetSources
        bool   sineOn[kNumChannels] { true, true, true, true };
        bool   asgnOn[kNumChannels] { true, true, true, true };
        bool   rectOn[kNumChannels] { true, true, true, true };

        // Tetrad Ex. While exOn is off none of these does anything, and Tetrad moves exactly as 0.1.0 did.
        bool   exOn = false;
        double sineLevel[kNumChannels]   { 1.0, 1.0, 1.0, 1.0 };   // 0..1
        double asgnLevel[kNumChannels]   { 1.0, 1.0, 1.0, 1.0 };   // 0..1
        int    exWave[kNumChannels]      { 0, 0, 0, 0 };           // 0: Main (the global wave); 1-6: Wave + 1
        double shapeAmount[kNumChannels] { 0.0, 0.0, 0.0, 0.0 };   // -1..+1
        int    shapeFrom[kNumChannels]   { 0, 0, 0, 0 };           // see kNumShapeSources
        int    shapeTarget[kNumChannels] { 0, 0, 0, 0 };           // ShapeTarget
    };

    // What Logic says about time, at the first sample of a block.
    struct HostTime
    {
        bool   hasTempo = false;
        double bpm = 120.0;
        bool   playing = false;
        bool   hasPosition = false;
        double ppq = 0.0;                                   // song position in beats (quarter notes)
    };

    struct InputEvent
    {
        enum Kind { modWheel, noteOn };
        int    offset = 0;                                  // sample position inside the block
        Kind   kind = modWheel;
        double value = 0.0;                                 // mod wheel 0..1
    };

    class Engine
    {
    public:
        static constexpr int kTickSamples = 32;
        static constexpr int kTraceTicks = 8;               // a Waves-view point every 8 ticks (about 5 ms)
        static constexpr int kTracePoints = 512;            // about 2.7 s of trace

        Engine();

        // Sets the sample rate. The channels stay where they are; a rect note still on is ended at the next tick.
        void prepare (double sampleRate);

        void setSeed (uint32_t s)                    { seed = s == 0 ? 1u : s; }
        uint32_t getSeed() const                     { return seed; }

        void setSettings (const Settings& s)         { settings = s; }
        const Settings& getSettings() const          { return settings; }
        void setHostTime (const HostTime& h)         { host = h; }

        // Runs one block. Input events must be sorted by offset.
        void render (int numSamples, const InputEvent* events, int numEvents, CcSink& sink);

        // Ends every rect note and every rect CC that is high, now (bypass, a new MIDI channel). Afterwards everything
        // is sent again from the next tick.
        void closeAll (int offset, CcSink& sink);

        // Values for the panel and the tests (read after render).
        double sine (int ch) const                   { return sineOut[(std::size_t) ch]; }
        double asgn (int ch) const                   { return asgnOut[(std::size_t) ch]; }
        bool   rect (int ch) const                   { return rectOut[(std::size_t) ch]; }
        double phase (int ch) const                  { return fractionOf (pos[(std::size_t) ch]); }
        double position (int ch) const               { return pos[(std::size_t) ch]; }
        double hz (int ch) const                     { return hzNow[(std::size_t) ch]; }
        int    ratioUsed (int ch) const              { return ratioNow[(std::size_t) ch]; }      // divide / multiply
        int    lengthUsed (int ch) const             { return lengthNow[(std::size_t) ch]; }     // tempo: index into kLengthBeats
        double shiftUsed (int ch) const              { return shiftNow[(std::size_t) ch]; }      // phase mode, in cycles
        bool   noteOn (int ch) const                 { return notes[(std::size_t) ch].on; }
        uint64_t notesSent (int ch) const            { return notes[(std::size_t) ch].count; }
        uint64_t resetsDone (int ch) const           { return resets[(std::size_t) ch]; }
        uint64_t samplesDone() const                 { return samples; }
        uint64_t ticksDone() const                   { return ticks; }
        bool   jumpedLastTick (int ch) const         { return jumped[(std::size_t) ch]; }  // a reset, ratio or mode change moved it at once
        bool   locked() const                        { return wasLocked; }
        double shape (int ch) const                  { return shapeNow[(std::size_t) ch]; }      // Tetrad Ex: -1..+1, 0 while Ex is off
        double duty (int ch) const                   { return dutyNow[(std::size_t) ch]; }       // the rect's width, 0.5 unless shaped
        int    waveUsed (int ch) const               { return waveNow[(std::size_t) ch]; }       // the asgn wave the channel plays
        bool   tempoMissing() const                  { return settings.tempoFollow && ! host.hasTempo; }

        // The Waves view: point k (0 = oldest) of channel ch's sine (which 0) or asgn (which 1).
        float trace (int ch, int which, int k) const;

        CcStream& ccStream()                         { return cc; }

        // The ratio actually used in multiply mode for a slider ratio index when A runs at aHz: stepped down until the
        // channel stays at or below the top rate (the module does the same at 5 kHz).
        static int cappedMultiplyIndex (int index, double aHz);
        // The note length used for an index at a tempo: stepped longer until it stays at or below the top rate.
        static int cappedLengthIndex (int index, double bpm);

    private:
        void tick (int offset, CcSink& sink);
        void enterMode (int newMode);
        void restartSelfTimed (int ch);
        double modSource (int ch) const;
        bool resetRequested (int ch) const;
        double randomFor (int ch, int64_t k) const;
        double asgnValue (int ch, int wave, double p, double ph) const;
        double shapeValue (int ch, const std::array<double, kNumChannels>& sineBefore, const std::array<double, kNumChannels>& asgnBefore) const;
        void computeOutputs();
        void send (int offset, double dt, CcSink& sink);
        void recordTrace();

        Settings settings;
        HostTime host;
        uint32_t seed = 1;
        double sampleRate = 48000.0;
        uint64_t samples = 0, ticks = 0;
        int samplesToTick = 0;
        int noteTicks = 30;
        int riseGapTicks = 72;
        double modWheel = 0.0;
        bool noteResetPending = false;

        int lastMode = -1;
        bool wasLocked = false;
        double songBeats = 0.0;                             // the song position while following Logic
        bool restartA = false;                              // entering a mode: A (and the others with it) restarts this tick
        int64_t aOrigin = 0;                                // A's position when the current mode (or A's last reset) began

        std::array<double, kNumChannels> pos {}, lockOffset {}, offset {}, modShift {}, hzNow {}, shiftNow {}, sliderBefore {};
        std::array<int, kNumChannels> ratioNow {}, lengthNow {};
        std::array<double, kNumChannels> sineOut {}, asgnOut {};
        std::array<bool, kNumChannels> rectOut {}, rectBefore {}, rising {}, jumped {}, resetNow {};
        std::array<int, kNumChannels> rectSent {};          // -1 not sent yet, 0 or 127
        std::array<bool, kNumSmooth> laneWasOn {};
        std::array<uint64_t, kNumChannels> resets {};
        std::array<double, kNumChannels> shapeNow {}, dutyNow { 0.5, 0.5, 0.5, 0.5 };
        std::array<int, kNumChannels> waveNow {};
        std::array<int64_t, kNumChannels> lastRiseTick { -1000000, -1000000, -1000000, -1000000 };
        struct Note { bool on = false; int ticksLeft = 0; uint64_t count = 0; };
        std::array<Note, kNumChannels> notes {};

        std::array<std::array<float, kTracePoints * 2>, kNumChannels> traces {};
        int traceHead = 0, traceFill = 0, traceCountdown = 0;

        CcStream cc;
    };
}
