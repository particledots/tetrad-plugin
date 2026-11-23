// The wave shapes and the random sequences. Our own definitions, drawn from the manual's description and figures only.
//
// Every wave is read from a channel's phase (0..1, one cycle). At phase 0 (the start of a cycle, and right after a
// reset) every wave is at its middle and rising, as in the manual's waveform figure: sine and triangle at 0 going up,
// the trapezoid at 0 going up, the upward saw at its bottom, the downward saw at its top, and rect high. Rect is high for
// the first half of the cycle.
//
// The two random waves take a new value twice per cycle (at phase 0 and 0.5). Step random holds each value for its half
// cycle; smooth random glides in a straight line from the previous value to the new one over that half cycle, so it
// arrives as the next step begins (the manual's random figure).
//
// The random values come from a counter, not a running generator: value number k of a sequence is worked out from the
// seed, the sequence and k alone. That is what lets phase, divide and multiply mode read channel A's sequence at
// any point, earlier or later (phase mode's delayed and "anti-delayed" copies).

#pragma once

#include <cmath>
#include <cstdint>

namespace tetrad
{
    inline double sineWave (double phase)     { return std::sin (2.0 * 3.14159265358979323846 * phase); }

    inline double triangleWave (double phase)
    {
        if (phase < 0.25) return 4.0 * phase;
        if (phase < 0.75) return 2.0 - 4.0 * phase;
        return 4.0 * phase - 4.0;
    }

    inline double sawUpWave (double phase)    { return 2.0 * phase - 1.0; }
    inline double sawDownWave (double phase)  { return 1.0 - 2.0 * phase; }

    // A triangle twice as tall, clipped: rises for an eighth of the cycle, holds for a quarter, falls for a quarter,
    // holds for a quarter, rises for the last eighth.
    inline double trapezoidWave (double phase)
    {
        const double t = 2.0 * triangleWave (phase);
        return t > 1.0 ? 1.0 : (t < -1.0 ? -1.0 : t);
    }

    inline bool rectHigh (double phase)       { return phase < 0.5; }

    // Random value number k of sequence `stream`, from the seed: evenly spread over -1..+1.
    inline double randomValue (uint32_t seed, int stream, int64_t k)
    {
        const uint64_t golden = UINT64_C (0x9E3779B97F4A7C15);
        uint64_t v = static_cast<uint64_t> (k) * golden ^ (static_cast<uint64_t> (seed) << 32) ^ (static_cast<uint64_t> (stream + 1) * UINT64_C (0xD1B54A32D192ED03));
        v += golden;
        v = (v ^ (v >> 30)) * UINT64_C (0xBF58476D1CE4E5B9);
        v = (v ^ (v >> 27)) * UINT64_C (0x94D049BB133111EB);
        v ^= v >> 31;
        return (double) (v >> 11) / (double) (UINT64_C (1) << 53) * 2.0 - 1.0;
    }

    inline int64_t floorToInt (double x)      { return (int64_t) std::floor (x); }
    inline double fractionOf (double x)       { return x - std::floor (x); }
}
