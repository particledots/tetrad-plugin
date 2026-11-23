// Turns slowly moving values into a smooth, thin stream of MIDI CC messages (copied from Strange Drift 0.1.1, which copied Ogdoad's).
//
// Each output ("lane") is sent as a 14-bit CC pair: a coarse part (MSB) and a fine part (LSB), 16,384 steps instead
// of 128. Tetrad's pairs are listed in Outputs.h (sine and asgn of each channel).
//
// Rules, per lane:
//   - send only when the 14-bit value has changed;
//   - at most once every kMinIntervalSeconds (6 ms), as in Ogdoad;
//   - and only once it has moved at least kMinStep (32 of 16,384, 0.2%), or kQuietSeconds (100 ms) have passed:
//     so fast moves stay as smooth as in 0.1.0 while slow drifts send far fewer messages. Every message costs the
//     Scripter mapper (and the knob it moves) work in Logic; on the Intel Mac 0.1.0's ~900 messages a second showed
//     clearly on Logic's CPU meter (test of 2026-11-19). 0.1.1 sends about a third as many;
//   - the coarse part is sent only when it changed, and always immediately before the fine part, at the same sample;
//     the fine part is always sent, so a receiver acts when the fine part arrives.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tetrad
{
    struct CcSink
    {
        virtual ~CcSink() = default;
        virtual void cc (int sampleOffset, int ccNumber, int value) = 0;
        virtual void note (int /*sampleOffset*/, int /*noteNumber*/, int /*velocity, 0 = note off*/) {}   // as Ogdoad's
    };

    inline constexpr int kMaxCcLanes = 16;
    inline constexpr double kMinIntervalSeconds = 0.006;
    inline constexpr double kQuietSeconds = 0.100;
    inline constexpr int kMinStep = 32;

    // -1..+1 to 0..16383
    inline int toFourteenBit (double v)
    {
        double x = (v + 1.0) * 0.5 * 16383.0 + 0.5;
        if (x < 0.0) x = 0.0;
        if (x > 16383.0) x = 16383.0;
        return (int) x;
    }

    class CcStream
    {
    public:
        void setLane (int lane, int msbNumber, int lsbNumber);
        void reset();            // forget what was sent: the next update sends everything
        void resetLane (int lane);   // forget what one lane sent: its next update sends it (an output switched back on)
        void setNumLanes (int n) { numLanes = n; }

        // Called once per engine tick. dtSeconds = time since the previous tick.
        void update (int lane, double value, double dtSeconds, int offset, CcSink& sink);

        int lastSent (int lane) const { return lanes[(std::size_t) lane].last; }

    private:
        struct Lane { int msbCc = -1, lsbCc = -1, last = -1, lastMsb = -1; double sinceSend = 1.0e9; };
        std::array<Lane, kMaxCcLanes> lanes {};
        int numLanes = 0;
    };
}
