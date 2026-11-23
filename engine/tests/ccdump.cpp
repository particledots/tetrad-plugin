// Writes the engine's real MIDI stream as text, for the mapper test (engine/tests/mapper_test.js).
// Usage: tetrad_ccdump <seconds> <mode 0-3> <rect out 0-2>
// Lines: "<sample> cc <number> <value>" or "<sample> note <number> <velocity>", then "final <lane> <14-bit value>" for
// the eight 14-bit lanes (sine A-D, asgn A-D) and "rect <channel> <0 or 1>" for the four rects.

#include "tetrad/Engine.h"

#include <cstdio>
#include <cstdlib>

using namespace tetrad;

struct PrintSink : CcSink
{
    unsigned long long base = 0;
    void cc (int offset, int number, int value) override   { std::printf ("%llu cc %d %d\n", base + (unsigned long long) offset, number, value); }
    void note (int offset, int number, int value) override { std::printf ("%llu note %d %d\n", base + (unsigned long long) offset, number, value); }
};

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 30.0;
    Engine e;
    e.prepare (48000.0);
    e.setSeed (2026);
    Settings s;
    s.mode = argc > 2 ? std::atoi (argv[2]) : 0;
    s.rectOut = argc > 3 ? std::atoi (argv[3]) : 0;
    s.asgnWave = (int) Wave::smoothRandom;
    s.slider[0] = sliderForHz (1.7);
    s.slider[1] = sliderForHz (0.4);
    s.slider[2] = 3.0 / 7.0;
    s.slider[3] = sliderForHz (6.0);
    s.modFrom[2] = 3; s.modDepth[2] = 0.4;
    e.setSettings (s);
    PrintSink sink;
    const long total = (long) (seconds * 48000.0);
    for (long pos = 0; pos < total; pos += 512)
    {
        sink.base = e.samplesDone();
        e.render (512, nullptr, 0, sink);
    }
    for (int i = 0; i < kNumSmooth; ++i) std::printf ("final %d %d\n", i, e.ccStream().lastSent (i));
    for (int ch = 0; ch < kNumChannels; ++ch) std::printf ("rect %d %d\n", ch, e.rect (ch) ? 1 : 0);
    return 0;
}
