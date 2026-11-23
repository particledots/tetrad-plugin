// Tetrad engine self-test. Runs on the Linux checking computer before every Mac build.
//
// What it proves, without Logic:
//   - the wave shapes and where each wave starts; the sliders' Hz, degrees, ratios and note lengths;
//   - free, phase, divide and multiply mode: speeds, delays, ratios, everything starting together on entering a mode,
//     and the random waves' relationships (phase: A's sequence delayed; divide: every r-th value of A's; multiply:
//     every r-th value is A's);
//   - the top rate: no channel ever moves faster than 20 Hz, whatever the routing (multiply ratios step down, phase
//     modulation glides);
//   - reset from notes and from other channels' rect, the phase-mode offset and its end on a mode change;
//   - following Logic's tempo: locked to the song position while playing, running at the tempo when stopped, lining up
//     again when playback starts, 120 BPM with no tempo, lengths stepped down to stay under the top rate;
//   - every mod-from and reset-from combination in every mode: always finite, inside -1..+1, never a standstill;
//   - the same movement at 44.1, 48 and 96 kHz, and the same MIDI at every block size;
//   - the CC rules from Strange Drift 0.1.1 (6 ms, 0.2% or 100 ms, coarse before fine), the rect CC and notes (20 ms,
//     merging, never left hanging), the output switches;
//   - the CPU use and the MIDI rate, against a ceiling worked out from the send rule.

#include "tetrad/Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace tetrad;

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const std::string& what)
    {
        ++checks;
        if (! ok) { ++failures; std::printf ("FAIL: %s\n", what.c_str()); }
    }

    std::string num (double v, int digits = 3) { char b[64]; std::snprintf (b, sizeof (b), "%.*f", digits, v); return b; }

    struct Msg
    {
        uint64_t sample; bool isNote; int number, value;
        bool operator== (const Msg& o) const { return sample == o.sample && isNote == o.isNote && number == o.number && value == o.value; }
    };

    struct RecordSink : CcSink
    {
        std::vector<Msg>* out = nullptr;
        uint64_t base = 0;
        long count = 0;
        void cc (int offset, int number, int value) override   { ++count; if (out) out->push_back ({ base + (uint64_t) offset, false, number, value }); }
        void note (int offset, int number, int value) override { ++count; if (out) out->push_back ({ base + (uint64_t) offset, true, number, value }); }
    };

    // Logic's transport, as a stand-in: tempo, playing, and the song position at any sample.
    struct Transport
    {
        bool hasTempo = true, playing = false, hasPosition = true;
        double bpm = 120.0, ppqAtZero = 0.0;
        HostTime at (uint64_t sample, double sr) const
        {
            HostTime h;
            h.hasTempo = hasTempo; h.bpm = bpm; h.playing = playing; h.hasPosition = hasPosition;
            h.ppq = ppqAtZero + (double) sample / sr * bpm / 60.0;
            return h;
        }
    };

    struct NoteIn { uint64_t sample; };

    // Runs an engine for a number of seconds in blocks of a given size (0 = random sizes 1..4096). After each block
    // `each` is called. Mod wheel changes and notes are given in absolute samples.
    void run (Engine& e, double sr, double seconds, int block, std::vector<Msg>* out = nullptr,
              const std::function<void (Engine&)>& each = nullptr, const Transport* transport = nullptr,
              const std::vector<std::pair<uint64_t, double>>& wheel = {}, const std::vector<uint64_t>& notesIn = {},
              uint32_t blockSeed = 7)
    {
        RecordSink sink;
        sink.out = out;
        std::mt19937 rng (blockSeed);
        std::uniform_int_distribution<int> sizes (1, 4096);
        const uint64_t total = (uint64_t) std::llround (seconds * sr);
        const uint64_t start = e.samplesDone();
        uint64_t done = 0;
        std::size_t w = 0, nIdx = 0;
        std::vector<InputEvent> events;
        while (done < total)
        {
            int n = block > 0 ? block : sizes (rng);
            n = (int) std::min<uint64_t> ((uint64_t) n, total - done);
            const uint64_t abs0 = start + done;
            events.clear();
            // Merge wheel and notes in order of their sample.
            while ((w < wheel.size() && wheel[w].first < abs0 + (uint64_t) n) || (nIdx < notesIn.size() && notesIn[nIdx] < abs0 + (uint64_t) n))
            {
                const bool takeWheel = nIdx >= notesIn.size() || (w < wheel.size() && wheel[w].first <= notesIn[nIdx]);
                if (takeWheel && w < wheel.size() && wheel[w].first < abs0 + (uint64_t) n)
                {
                    events.push_back ({ (int) (wheel[w].first >= abs0 ? wheel[w].first - abs0 : 0), InputEvent::modWheel, wheel[w].second });
                    ++w;
                }
                else
                {
                    events.push_back ({ (int) (notesIn[nIdx] >= abs0 ? notesIn[nIdx] - abs0 : 0), InputEvent::noteOn, 0.0 });
                    ++nIdx;
                }
            }
            if (transport) e.setHostTime (transport->at (abs0, sr));
            sink.base = abs0;
            e.render (n, events.data(), (int) events.size(), sink);
            done += (uint64_t) n;
            if (each) each (e);
        }
    }

    double maxStep (double sr) { return kTopHz * (double) Engine::kTickSamples / sr; }
}

int main()
{
    // ---- Shapes ---------------------------------------------------------------------------------------------------
    {
        bool ok = std::fabs (sineWave (0.0)) < 1e-12 && std::fabs (sineWave (0.25) - 1.0) < 1e-12 && std::fabs (sineWave (0.75) + 1.0) < 1e-12;
        expect (ok, "sine starts at 0 rising, top at a quarter, bottom at three quarters");
        ok = triangleWave (0.0) == 0.0 && triangleWave (0.25) == 1.0 && std::fabs (triangleWave (0.5)) < 1e-12 && triangleWave (0.75) == -1.0
             && triangleWave (0.125) > 0.0;
        expect (ok, "triangle starts at 0 rising, peaks with the sine");
        expect (sawUpWave (0.0) == -1.0 && sawUpWave (0.5) == 0.0 && sawUpWave (0.999) > 0.99, "upward saw starts at its bottom and climbs");
        expect (sawDownWave (0.0) == 1.0 && sawDownWave (0.999) < -0.99, "downward saw starts at its top and falls");
        expect (trapezoidWave (0.0) == 0.0 && trapezoidWave (0.125) == 1.0 && trapezoidWave (0.3) == 1.0 && trapezoidWave (0.375) == 1.0
                && std::fabs (trapezoidWave (0.5)) < 1e-12 && trapezoidWave (0.7) == -1.0 && trapezoidWave (0.0625) > 0.0,
                "trapezoid starts at 0 rising, holds its top and bottom");
        expect (rectHigh (0.0) && rectHigh (0.49) && ! rectHigh (0.5) && ! rectHigh (0.99), "rect high for the first half");
        double lo = 1, hi = -1, sum = 0;
        for (int k = -5000; k < 5000; ++k) { const double v = randomValue (9, 0, k); lo = std::min (lo, v); hi = std::max (hi, v); sum += v; }
        expect (lo >= -1.0 && hi <= 1.0 && lo < -0.99 && hi > 0.99 && std::fabs (sum / 10000.0) < 0.03, "random values spread evenly over -1..+1");
        expect (randomValue (9, 0, 5) == randomValue (9, 0, 5) && randomValue (9, 0, 5) != randomValue (9, 1, 5) && randomValue (9, 0, 5) != randomValue (10, 0, 5),
                "random value k depends on the seed, the sequence and k only");
    }

    // ---- Sliders ----------------------------------------------------------------------------------------------------
    {
        expect (std::fabs (sliderHz (0.0) - 0.01) < 1e-12 && std::fabs (sliderHz (1.0) - 20.0) < 1e-9, "Hz sliders run 0.01 to 20 Hz");
        expect (std::fabs (sliderHz (Settings().slider[0]) - 0.5) < 0.001, "channel A starts at 0.5 Hz (" + num (sliderHz (Settings().slider[0])) + ")");
        expect (std::fabs (sliderHz (sliderForHz (3.0)) - 3.0) < 1e-9, "slider for a frequency and back");
        bool ok = true;
        for (int i = 0; i < kNumRatios; ++i) ok = ok && sliderRatioIndex (i / 7.0) == i;
        expect (ok && kRatios[0] == 1 && kRatios[7] == 32, "ratio sliders step through 1, 2, 3, 4, 5, 8, 16, 32");
        ok = true;
        for (int i = 0; i < kNumLengths; ++i) ok = ok && sliderLengthIndex (i / 15.0) == i;
        expect (ok && kLengthBeats[5] == 4.0 && kLengthBeats[10] == 1.0 && kLengthBeats[15] == 0.25, "length sliders step from 32 bars to 1/16");
        expect (Engine::cappedLengthIndex (15, 300.0) == 15 && Engine::cappedLengthIndex (15, 400.0) == 14,
                "1/16 at 300 BPM is 20 Hz and allowed; at 400 BPM it steps to 1/8 triplet");
        expect (Engine::cappedMultiplyIndex (7, 2.0) == 5 && Engine::cappedMultiplyIndex (7, 0.5) == 7 && Engine::cappedMultiplyIndex (3, 20.0) == 0,
                "multiply ratios step down to stay at or under 20 Hz (x32 at 2 Hz becomes x8)");
    }

    const double sr = 48000.0;

    // ---- Free mode ----------------------------------------------------------------------------------------------------
    {
        Engine e; e.prepare (sr);
        Settings s; s.slider[0] = 0.0; s.slider[1] = 1.0; s.slider[2] = sliderForHz (3.0); s.slider[3] = Settings().slider[0];
        e.setSettings (s);
        run (e, sr, 10.0, 512);
        const double t = (double) e.ticksDone() * Engine::kTickSamples / sr;
        expect (std::fabs (e.position (0) - 0.01 * t) < 1e-9 && std::fabs (e.position (1) - 20.0 * t) < 1e-6
                && std::fabs (e.position (2) - 3.0 * t) < 1e-7 && std::fabs (e.position (3) - 0.5 * t) < 1e-6,
                "free mode: each channel at its own speed (0.01, 20, 3, 0.5 Hz)");
        // Mod wheel pushing a 20 Hz channel up: still 20 Hz; the bottom is the module's 9.76 microhertz.
        Settings m = s; m.modFrom[1] = 1; m.modDepth[1] = 1.0; m.modFrom[0] = 1; m.modDepth[0] = -1.0;
        Engine f; f.prepare (sr); f.setSettings (m);
        run (f, sr, 1.0, 512, nullptr, nullptr, nullptr, { { 0, 1.0 } });
        expect (std::fabs (f.hz (1) - 20.0) < 1e-9 && std::fabs (f.hz (0) - 0.01 / 32.0) < 1e-9, "mod wheel: up to the 20 Hz top, down 5 octaves");
        expect (std::fabs (sliderHz (0.0) * std::exp2 (-5.0) - 0.0003125) < 1e-12 && kBottomHz < 0.0003125, "the bottom limit is far below anything the controls reach");
        // Free mode random: four unrelated sequences, two values per cycle.
        Settings r = Settings(); r.asgnWave = (int) Wave::stepRandom;
        for (int ch = 0; ch < 4; ++ch) r.slider[ch] = sliderForHz (2.0);
        Engine g; g.prepare (sr); g.setSettings (r);
        std::vector<std::vector<double>> seen (4);
        run (g, sr, 5.0, 32, nullptr, [&] (Engine& x) { for (int ch = 0; ch < 4; ++ch) if (seen[(size_t) ch].empty() || seen[(size_t) ch].back() != x.asgn (ch)) seen[(size_t) ch].push_back (x.asgn (ch)); });
        expect (seen[0].size() >= 19 && seen[0].size() <= 21, "step random: two new values per cycle (" + std::to_string (seen[0].size()) + " in 5 s at 2 Hz)");
        expect (seen[0] != seen[1] && seen[1] != seen[2] && seen[0][3] != seen[1][3], "free mode: the four random waves are unrelated");
    }

    // ---- Phase mode ----------------------------------------------------------------------------------------------------
    {
        Settings s; s.mode = (int) Mode::phase; s.slider[0] = sliderForHz (1.0);
        s.slider[1] = 0.25; s.slider[2] = 0.5; s.slider[3] = 0.75;
        Engine e; e.prepare (sr); e.setSettings (s);
        bool ok = true;
        run (e, sr, 4.0, 32, nullptr, [&] (Engine& x)
        {
            for (int ch = 1; ch < 4; ++ch)
                ok = ok && std::fabs (x.position (ch) - (x.position (0) - s.slider[ch])) < 1e-9 && std::fabs (x.hz (ch) - x.hz (0)) < 1e-12;
        });
        expect (ok, "phase mode: B, C, D follow A, delayed by 90, 180, 270 degrees");

        // Random: B's values are A's, a quarter cycle later (and with a negative shift, earlier: "anti-delayed").
        Settings r = s; r.asgnWave = (int) Wave::stepRandom;
        r.modFrom[3] = 1; r.modDepth[3] = -1.0;               // D pushed by the mod wheel to -2.5 cycles: ahead of A
        Engine g; g.prepare (sr); g.setSettings (r);
        std::vector<double> aVals, bVals, dVals;
        run (g, sr, 6.0, 32, nullptr, [&] (Engine& x)
        {
            auto add = [] (std::vector<double>& v, double a) { if (v.empty() || v.back() != a) v.push_back (a); };
            add (aVals, x.asgn (0)); add (bVals, x.asgn (1)); add (dVals, x.asgn (3));
        }, nullptr, { { 0, 1.0 } });
        int matched = 0;
        for (std::size_t k = 0; k + 1 < bVals.size() && k < 8; ++k)
            if (std::find (aVals.begin(), aVals.end(), bVals[k + 1]) != aVals.end()) ++matched;
        expect (matched >= 7, "phase mode: B's random values are A's, later (" + std::to_string (matched) + " of 8)");
        bool ahead = false;
        for (std::size_t k = 0; k + 4 < dVals.size(); ++k)
            if (dVals[k] == aVals[std::min (aVals.size() - 1, (std::size_t) 4)]) ahead = true;
        const double dShift = g.shiftUsed (3);
        expect (std::fabs (dShift - (0.75 - 2.5)) < 1e-6 && g.position (3) > g.position (0) && ahead,
                "phase mode: a negative shift puts D ahead of A (A's random values arrive at D first)");
    }

    // ---- Divide and multiply ---------------------------------------------------------------------------------------------
    for (int mode : { (int) Mode::divide, (int) Mode::multiply })
    {
        const bool div = mode == (int) Mode::divide;
        const std::string name = div ? "divide" : "multiply";
        Settings s; s.mode = (int) Mode::free; s.slider[0] = sliderForHz (div ? 4.0 : 0.5);
        s.slider[1] = 1.0 / 7.0; s.slider[2] = 2.0 / 7.0; s.slider[3] = 5.0 / 7.0;      // ratios 2, 3, 8
        Engine e; e.prepare (sr); e.setSettings (s);
        run (e, sr, 0.37, 512);                                                          // somewhere mid-cycle
        s.mode = mode; e.setSettings (s);
        bool startTogether = false;
        run (e, sr, (double) Engine::kTickSamples / sr, Engine::kTickSamples, nullptr, [&] (Engine& x)
        {
            startTogether = x.phase (0) == 0.0 && x.phase (1) == 0.0 && x.phase (2) == 0.0 && x.phase (3) == 0.0;
        });
        expect (startTogether, name + " mode: entering it starts all four together at phase 0");
        const double a0 = e.position (0);
        std::vector<double> b0 { e.position (1), e.position (2), e.position (3) };
        run (e, sr, 8.0, 333);
        const double da = e.position (0) - a0;
        const int r[3] = { 2, 3, 8 };
        bool ok = true;
        for (int k = 0; k < 3; ++k)
            ok = ok && std::fabs ((e.position (k + 1) - b0[(size_t) k]) - (div ? da / r[k] : da * r[k])) < 1e-6 && e.ratioUsed (k + 1) == r[k];
        expect (ok, name + " mode: B, C, D at A's speed " + (div ? "divided" : "multiplied") + " by 2, 3, 8");

        // Random relationships.
        Settings rs = s; rs.asgnWave = (int) Wave::stepRandom;
        rs.slider[1] = 4.0 / 7.0;                                                        // ratio 5
        Engine g; g.prepare (sr); g.setSeed (77); g.setSettings (rs);
        std::set<double> aSet;
        std::vector<double> bVals;
        run (g, sr, div ? 12.0 : 6.0, 32, nullptr, [&] (Engine& x)
        {
            aSet.insert (x.asgn (0));
            if (bVals.empty() || bVals.back() != x.asgn (1)) bVals.push_back (x.asgn (1));
        });
        int fromA = 0;
        for (double v : bVals) if (aSet.count (v)) ++fromA;
        if (div)
            expect (fromA == (int) bVals.size() && bVals.size() >= 8, "divide mode: B's random values are every 5th of A's (" + std::to_string (fromA) + " of " + std::to_string (bVals.size()) + ")");
        else
            expect (fromA >= (int) bVals.size() / 5 - 1 && fromA <= (int) bVals.size() / 5 + 2 && bVals.size() > 25,
                    "multiply mode: every 5th of B's random values is A's (" + std::to_string (fromA) + " of " + std::to_string (bVals.size()) + ")");
    }

    // ---- The top rate, in every mode, with fast modulation ----------------------------------------------------------------
    {
        bool ok = true; double worst = 0.0;
        for (int mode = 0; mode < kNumModes; ++mode)
        {
            Settings s; s.mode = mode;
            s.slider[0] = sliderForHz (mode == (int) Mode::phase ? 15.0 : 3.0);
            for (int ch = 1; ch < 4; ++ch) { s.slider[ch] = 1.0; s.modFrom[ch] = 2 + 2 * (ch == 1 ? 1 : 0); s.modDepth[ch] = 1.0; }
            s.modFrom[0] = 3; s.modDepth[0] = 1.0;
            Engine e; e.prepare (sr); e.setSettings (s);
            std::array<double, 4> before {};
            for (int ch = 0; ch < 4; ++ch) before[(size_t) ch] = e.position (ch);
            run (e, sr, 20.0, 32, nullptr, [&] (Engine& x)
            {
                for (int ch = 0; ch < 4; ++ch)
                {
                    const double step = std::fabs (x.position (ch) - before[(size_t) ch]);
                    if (! x.jumpedLastTick (ch)) { worst = std::max (worst, step / maxStep (sr)); ok = ok && step <= maxStep (sr) * (1.0 + 1e-9); }
                    before[(size_t) ch] = x.position (ch);
                }
            });
        }
        expect (ok, "no channel ever moves faster than 20 Hz, in any mode, with fast modulation (worst " + num (100.0 * worst, 1) + "% of the top rate)");
    }

    // ---- Resets ----------------------------------------------------------------------------------------------------------
    {
        Settings s; s.resetFrom[0] = 1; s.slider[0] = sliderForHz (0.3);
        Engine e; e.prepare (sr); e.setSettings (s);
        const uint64_t at = 48000 + 100;                 // a note a little after 1 s
        double phaseAfter = -1.0;
        uint64_t resetTick = 0;
        run (e, sr, 1.1, 32, nullptr, [&] (Engine& x)
        {
            if (resetTick == 0 && x.resetsDone (0) > 0) { resetTick = x.samplesDone(); phaseAfter = x.phase (0); }
        }, nullptr, {}, { at });
        expect (e.resetsDone (0) == 1 && phaseAfter == 0.0 && resetTick - 32 >= at && resetTick - 32 < at + 32,
                "reset from notes: A restarts at phase 0 at the tick after the note");
        expect (e.resetsDone (1) == 0, "channels set to Off are not reset");

        // B reset from A's rect, free mode: B is at phase 0 right after each of A's rising edges.
        Settings t; t.slider[0] = sliderForHz (2.0); t.slider[1] = sliderForHz (0.3); t.resetFrom[1] = 2;   // B: "A rect"
        Engine f; f.prepare (sr); f.setSettings (t);
        int atZero = 0, edges = 0; bool aBefore = false, aRoseLastTick = false;
        run (f, sr, 5.0, 32, nullptr, [&] (Engine& x)
        {
            if (x.resetsDone (1) > (uint64_t) edges) { ++edges; if (x.phase (1) == 0.0 && aRoseLastTick) ++atZero; }
            aRoseLastTick = x.rect (0) && ! aBefore;
            aBefore = x.rect (0);
        });
        expect (f.resetsDone (1) >= 9 && f.resetsDone (1) <= 11 && atZero >= 9,
                "reset from A rect: B restarts the tick after each rising edge of A (" + std::to_string (f.resetsDone (1)) + " in 5 s at 2 Hz)");

        // Phase mode: a reset of B gives it an offset, kept until the mode changes.
        Settings p; p.mode = (int) Mode::phase; p.slider[0] = sliderForHz (1.0); p.slider[1] = 0.1; p.resetFrom[1] = 1;
        Engine g; g.prepare (sr); g.setSettings (p);
        run (g, sr, 0.3, 32, nullptr, nullptr, nullptr, {}, { 6000 });
        const double off = g.position (1) - (g.position (0) - 0.1);
        expect (std::fabs (off) > 0.05 && g.phase (1) < 0.2, "phase mode: a reset of B sets a new offset (" + num (off) + " cycles)");
        run (g, sr, 1.0, 32);
        expect (std::fabs (g.position (1) - (g.position (0) - 0.1) - off) < 1e-9, "the offset stays while the mode stays");
        p.mode = (int) Mode::divide; g.setSettings (p); run (g, sr, 0.01, 32);
        p.mode = (int) Mode::phase; g.setSettings (p); run (g, sr, 0.01, 32);
        expect (std::fabs (g.position (1) - (g.position (0) - 0.1)) < 1e-9, "changing the mode cancels the offset");

        // A reset in divide mode: B, C and D follow (all restart).
        Settings d; d.mode = (int) Mode::divide; d.slider[0] = sliderForHz (2.0); d.resetFrom[0] = 1;
        Engine h; h.prepare (sr); h.setSettings (d);
        bool together = false;
        run (h, sr, 1.0, 32, nullptr, [&] (Engine& x) { if (x.resetsDone (0) == 1 && ! together) together = x.phase (0) == 0.0 && x.phase (1) == 0.0 && x.phase (2) == 0.0 && x.phase (3) == 0.0; },
             nullptr, {}, { 30000 });
        expect (together, "divide mode: resetting A restarts B, C and D with it");
    }

    // ---- Tempo -------------------------------------------------------------------------------------------------------------
    {
        Settings s; s.tempoFollow = true; s.slider[0] = 10.0 / 15.0; s.slider[1] = 5.0 / 15.0; s.slider[2] = 15.0 / 15.0; s.slider[3] = 0.0;
        Transport tr; tr.bpm = 100.0; tr.playing = true; tr.ppqAtZero = 37.3;
        Engine e; e.prepare (sr); e.setSettings (s);
        bool locked = true;
        run (e, sr, 6.0, 512, nullptr, [&] (Engine& x)
        {
            const double t = (double) (x.ticksDone() - 1) * Engine::kTickSamples / sr;
            const double ppq = tr.ppqAtZero + t * tr.bpm / 60.0;
            locked = locked && std::fabs (x.position (0) - ppq / 1.0) < 1e-9 && std::fabs (x.position (1) - ppq / 4.0) < 1e-9
                     && std::fabs (x.position (2) - ppq / 0.25) < 1e-9 && std::fabs (x.position (3) - ppq / 128.0) < 1e-9;
        }, &tr);
        expect (locked, "following Logic while it plays: 1/4, 1 bar, 1/16 and 32 bars locked to the song position");
        expect (std::fabs (e.hz (0) - 100.0 / 60.0) < 1e-12 && e.lengthUsed (0) == 10, "1/4 at 100 BPM runs at 1.67 Hz");

        tr.playing = false;
        const double p0 = e.position (0);
        run (e, sr, 3.0, 512, nullptr, nullptr, &tr);
        expect (std::fabs ((e.position (0) - p0) - 3.0 * 100.0 / 60.0) < 0.01, "stopped: it keeps running at the tempo");
        tr.playing = true; tr.ppqAtZero = 0.0;
        run (e, sr, 0.1, 512, nullptr, nullptr, &tr);
        {
            const double tickSample = (double) (e.ticksDone() - 1) * Engine::kTickSamples;
            const double ppqNow = tickSample / sr * 100.0 / 60.0;
            expect (e.locked() && std::fabs (e.position (0) - ppqNow) < 1e-9 && std::fabs (e.position (2) - ppqNow * 4.0) < 1e-9,
                    "playback starts: everything lines up with the song again");
        }
        Transport none; none.hasTempo = false; none.playing = false;
        Engine n; n.prepare (sr); n.setSettings (s);
        run (n, sr, 1.0, 512, nullptr, nullptr, &none);
        expect (std::fabs (n.hz (0) - 2.0) < 1e-12 && n.tempoMissing(), "no tempo from Logic: 120 BPM (1/4 = 2 Hz) and the panel can say so");

        Transport fast; fast.bpm = 400.0; fast.playing = true;
        Engine q; q.prepare (sr); q.setSettings (s);
        run (q, sr, 1.0, 512, nullptr, nullptr, &fast);
        expect (q.lengthUsed (2) == 14 && q.hz (2) <= kTopHz + 1e-9, "1/16 at 400 BPM steps to 1/8 triplet (20 Hz)");

        // Divide mode following the tempo: B lined up with the song too.
        Settings d = s; d.mode = (int) Mode::divide; d.slider[1] = 3.0 / 7.0;            // A 1/4, B divided by 4 = 1 bar
        Transport play; play.bpm = 90.0; play.playing = true; play.ppqAtZero = 8.0;
        Engine v; v.prepare (sr); v.setSettings (d);
        run (v, sr, 4.0, 256, nullptr, nullptr, &play);
        const double ppq = play.ppqAtZero + (double) (v.ticksDone() - 1) * Engine::kTickSamples / sr * 90.0 / 60.0;
        expect (std::fabs (v.position (1) - ppq / 4.0) < 1e-9, "divide mode while following: B (A / 4) lined up with the bars");

        // Mod in tempo mode steps through the lengths.
        Settings m = s; m.modFrom[0] = 1; m.modDepth[0] = 1.0; m.slider[0] = 5.0 / 15.0;
        Engine w; w.prepare (sr); w.setSettings (m);
        run (w, sr, 0.1, 512, nullptr, nullptr, &none, { { 0, 0.4 } });
        expect (w.lengthUsed (0) == 11, "following the tempo, mod from steps through the lengths (1 bar + 6 steps = 1/8 dotted)");
    }

    // ---- Every routing, every mode: finite, inside -1..+1, never a standstill ---------------------------------------
    {
        long runs = 0; int badValue = 0, stalls = 0, tooFast = 0;
        std::string firstStall;
        auto checkRun = [&] (const Settings& s, double seconds, uint32_t seed, const std::vector<uint64_t>& notesIn, const Transport* tr)
        {
            ++runs;
            Engine e; e.prepare (sr); e.setSeed (seed); e.setSettings (s);
            std::array<double, 4> before {}, start {};
            std::array<double, 4> lo { 9, 9, 9, 9 }, hi { -9, -9, -9, -9 };
            std::array<double, 4> hzSum {};
            int ticks = 0;
            for (int ch = 0; ch < 4; ++ch) start[(size_t) ch] = before[(size_t) ch] = e.position (ch);
            run (e, sr, seconds, 32, nullptr, [&] (Engine& x)
            {
                ++ticks;
                for (int ch = 0; ch < 4; ++ch)
                {
                    const double sv = x.sine (ch), av = x.asgn (ch);
                    if (! std::isfinite (sv) || ! std::isfinite (av) || std::fabs (sv) > 1.0 || std::fabs (av) > 1.0 || ! std::isfinite (x.position (ch))) ++badValue;
                    if (! x.jumpedLastTick (ch) && std::fabs (x.position (ch) - before[(size_t) ch]) > maxStep (sr) * (1.0 + 1e-9)) ++tooFast;
                    before[(size_t) ch] = x.position (ch);
                    lo[(size_t) ch] = std::min (lo[(size_t) ch], sv); hi[(size_t) ch] = std::max (hi[(size_t) ch], sv);
                    hzSum[(size_t) ch] += x.hz (ch);
                }
            }, tr, { { 0, 0.7 } }, notesIn);
            // One loop is allowed to hold A in a very short cycle, as patching it on the module would: A reset from a
            // channel that follows A (phase, divide, multiply mode), which restarts A whenever that channel's rect rises.
            const bool aLoop = s.mode != (int) Mode::free && s.resetFrom[0] >= 2;
            for (int ch = aLoop ? 4 : 0; ch < 4; ++ch)
            {
                const double cycles = hzSum[(size_t) ch] / ticks * seconds;
                if (cycles > 3.0 && hi[(size_t) ch] - lo[(size_t) ch] < 0.05)
                {
                    ++stalls;
                    if (firstStall.empty())
                    {
                        firstStall = "mode " + std::to_string (s.mode) + " channel " + std::to_string (ch) + " range " + num (hi[(size_t) ch] - lo[(size_t) ch]);
                        std::printf ("stall: tempo %d mode %d wave %d\n", (int) s.tempoFollow, s.mode, s.asgnWave);
                        for (int k = 0; k < 4; ++k) std::printf ("  ch %d slider %.3f modFrom %d depth %.2f resetFrom %d\n", k, s.slider[k], s.modFrom[k], s.modDepth[k], s.resetFrom[k]);
                    }
                }
            }
        };
        std::mt19937 rng (2026);
        std::uniform_real_distribution<double> u (0.0, 1.0);
        std::vector<uint64_t> notes;
        for (uint64_t k = 1; k < 20; ++k) notes.push_back (k * 23456);
        Transport play; play.playing = true; play.bpm = 128.0;
        // 1. Each channel's every mod-from and reset-from choice, in each mode, Hz and tempo; the others random.
        for (int tempo = 0; tempo < 2; ++tempo)
            for (int mode = 0; mode < kNumModes; ++mode)
                for (int ch = 0; ch < 4; ++ch)
                    for (int mf = 0; mf < kNumModSources; ++mf)
                        for (int rf = 0; rf < kNumResetSources; ++rf)
                        {
                            Settings s; s.mode = mode; s.tempoFollow = tempo == 1; s.asgnWave = (int) (u (rng) * 6) % 6;
                            for (int k = 0; k < 4; ++k)
                            {
                                s.slider[k] = u (rng); s.modFrom[k] = (int) (u (rng) * 8) % 8; s.modDepth[k] = u (rng) * 2 - 1;
                                s.resetFrom[k] = u (rng) < 0.5 ? 0 : (int) (u (rng) * 5) % 5;
                            }
                            s.modFrom[ch] = mf; s.resetFrom[ch] = rf; s.modDepth[ch] = u (rng) < 0.5 ? 1.0 : -1.0;
                            s.slider[0] = 0.4 + 0.6 * u (rng);                       // A not too slow, so stalls would show
                            checkRun (s, 6.0, (uint32_t) runs + 1, notes, tempo ? &play : nullptr);
                        }
        // 2. Every reset-from combination (625), each mode: chains and loops of resets.
        for (int mode = 0; mode < kNumModes; ++mode)
            for (int combo = 0; combo < 625; ++combo)
            {
                Settings s; s.mode = mode;
                int c = combo;
                for (int k = 0; k < 4; ++k) { s.resetFrom[k] = c % 5; c /= 5; s.slider[k] = 0.3 + 0.7 * u (rng); }
                s.slider[0] = 0.55 + 0.45 * u (rng);
                checkRun (s, 3.0, (uint32_t) combo + 9, notes, nullptr);
            }
        expect (badValue == 0, "every routing: values always finite and inside -1..+1 (" + std::to_string (runs) + " runs)");
        expect (stalls == 0, "every routing: no channel ever comes to a standstill (except A reset from a channel that follows it, a loop, as on the module)" + (firstStall.empty() ? std::string() : " (first: " + firstStall + ")"));
        expect (tooFast == 0, "every routing: never faster than 20 Hz (" + std::to_string (tooFast) + " ticks too fast)");
    }

    // ---- The same movement at every sample rate --------------------------------------------------------------------------
    {
        auto runAt = [] (double rate, int mode)
        {
            Settings s; s.mode = mode; s.asgnWave = (int) Wave::stepRandom; s.slider[0] = sliderForHz (1.3);
            s.slider[1] = 0.3; s.slider[2] = 0.6; s.slider[3] = 0.9;
            Engine g; g.prepare (rate); g.setSeed (5); g.setSettings (s);
            // the sequence of random values each channel went through, then A's measured speed
            std::vector<std::vector<double>> seq (4);
            run (g, rate, 30.0, 512, nullptr, [&] (Engine& x) { for (int ch = 0; ch < 4; ++ch) if (seq[(size_t) ch].empty() || seq[(size_t) ch].back() != x.asgn (ch)) seq[(size_t) ch].push_back (x.asgn (ch)); });
            seq.push_back ({ g.position (0) / ((double) g.ticksDone() * Engine::kTickSamples / rate) });
            return seq;
        };
        bool ok = true;
        for (int mode = 0; mode < kNumModes; ++mode)
        {
            auto a = runAt (48000.0, mode), b = runAt (44100.0, mode), c = runAt (96000.0, mode);
            for (int ch = 0; ch < 4; ++ch)
            {
                const auto& x = a[(size_t) ch];
                auto prefix = [&] (const std::vector<double>& y) { const std::size_t n = std::min (x.size(), y.size()) - 1; return std::equal (x.begin(), x.begin() + (long) n, y.begin()); };
                ok = ok && prefix (b[(size_t) ch]) && prefix (c[(size_t) ch]) && x.size() >= 5;
            }
            ok = ok && std::fabs (a[4][0] - 1.3) < 1e-9 && std::fabs (b[4][0] - 1.3) < 1e-9 && std::fabs (c[4][0] - 1.3) < 1e-9;
        }
        expect (ok, "the same speeds and the same random sequences at 44.1, 48 and 96 kHz, in every mode");
    }

    // ---- The same MIDI at every block size ---------------------------------------------------------------------------------
    {
        auto stream = [] (int block, bool tempo)
        {
            Settings s; s.mode = (int) Mode::phase; s.asgnWave = (int) Wave::smoothRandom; s.rectOut = (int) RectOut::both;
            s.tempoFollow = tempo;
            s.slider[0] = sliderForHz (3.0); s.modFrom[1] = 4; s.modDepth[1] = 0.6; s.resetFrom[2] = 1; s.resetFrom[3] = 2;
            Engine e; e.prepare (48000.0); e.setSeed (3); e.setSettings (s);
            std::vector<Msg> out;
            Transport tr; tr.playing = true; tr.bpm = 117.0; tr.ppqAtZero = 3.3;
            std::vector<uint64_t> notes { 1000, 40000, 90001, 150000 };
            run (e, 48000.0, 4.0, block, &out, nullptr, tempo ? &tr : nullptr, { { 20000, 0.3 }, { 60000, 0.9 } }, notes, (uint32_t) block + 11);
            return out;
        };
        bool same = true;
        for (int tempo = 0; tempo < 2; ++tempo)
        {
            const auto ref = stream (32, tempo == 1);
            for (int b : { 64, 333, 512, 1024, 0 })
            {
                const auto other = stream (b, tempo == 1);
                if (! (other == ref))
                {
                    std::size_t i = 0;
                    while (i < std::min (other.size(), ref.size()) && other[i] == ref[i]) ++i;
                    std::printf ("block %d tempo %d: differs at message %zu of %zu / %zu (sample %llu)\n", b, tempo, i, ref.size(), other.size(),
                                 (unsigned long long) (i < ref.size() ? ref[i].sample : 0));
                    same = false;
                }
            }
            same = same && ref.size() > 2000;
        }
        expect (same, "the same MIDI (CC and notes, at the same samples) at block sizes 32, 64, 333, 512, 1024 and random, Hz and tempo");
    }

    // ---- CC rules, rect CC, notes, switches ---------------------------------------------------------------------------------
    {
        Settings s; s.asgnWave = (int) Wave::sawUp; s.rectOut = (int) RectOut::both;
        s.slider[0] = sliderForHz (0.05); s.slider[1] = sliderForHz (1.0); s.slider[2] = sliderForHz (7.0); s.slider[3] = 1.0;
        Engine e; e.prepare (sr); e.setSettings (s);
        std::vector<Msg> out;
        run (e, sr, 30.0, 0, &out);
        std::map<int, uint64_t> lastFine;
        std::map<int, int> lastValue;
        bool interval = true, coarseFirst = true, rule = true, numbersOk = true;
        std::map<int, int> coarseValue;
        std::set<int> smoothFine, smoothCoarse;
        for (int ch = 0; ch < 4; ++ch) { smoothFine.insert (kSineFine[ch]); smoothFine.insert (kAsgnFine[ch]); smoothCoarse.insert (kSineCoarse[ch]); smoothCoarse.insert (kAsgnCoarse[ch]); }
        std::map<int, int> partner;
        for (int ch = 0; ch < 4; ++ch) { partner[kSineCoarse[ch]] = kSineFine[ch]; partner[kAsgnCoarse[ch]] = kAsgnFine[ch]; }
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const auto& m = out[i];
            if (m.isNote) { numbersOk = numbersOk && m.number >= 49 && m.number <= 52; continue; }
            const bool isRect = m.number >= 91 && m.number <= 94;
            numbersOk = numbersOk && (isRect || smoothFine.count (m.number) || smoothCoarse.count (m.number));
            if (smoothCoarse.count (m.number))
            {
                coarseFirst = coarseFirst && i + 1 < out.size() && ! out[i + 1].isNote && out[i + 1].number == partner[m.number] && out[i + 1].sample == m.sample;
                coarseValue[partner[m.number]] = m.value;
            }
            else if (smoothFine.count (m.number))
            {
                const int v = coarseValue[m.number] * 128 + m.value;
                if (lastFine.count (m.number))
                {
                    const double gap = (double) (m.sample - lastFine[m.number]) / sr;
                    interval = interval && gap >= kMinIntervalSeconds - 1e-9;
                    const int step = std::abs (v - lastValue[m.number]);
                    rule = rule && (step >= kMinStep || gap >= kQuietSeconds - 1e-9);
                }
                lastFine[m.number] = m.sample; lastValue[m.number] = v;
            }
        }
        expect (numbersOk, "only Tetrad's CC numbers (2-8, 34-40, 66-73, 91-94) and notes 49-52 are sent");
        expect (interval, "each output at most once every 6 ms");
        expect (rule, "each output sent only after a 0.2% move, or after 100 ms for smaller moves");
        expect (coarseFirst, "the coarse part only with its fine part, just before it, at the same sample");
        bool receiverOk = true;
        for (int ch = 0; ch < 4; ++ch)
        {
            receiverOk = receiverOk && lastValue[kSineFine[ch]] == e.ccStream().lastSent (ch) && lastValue[kAsgnFine[ch]] == e.ccStream().lastSent (4 + ch);
            if (ch == 0) receiverOk = receiverOk && std::abs (toFourteenBit (e.sine (ch)) - lastValue[kSineFine[ch]]) < 100;
        }
        expect (receiverOk, "a receiver ends up with exactly what was sent, close to the outputs");

        // Rect as CC: 127 / 0 alternating, one per edge; first value sent at the start.
        std::map<int, std::vector<int>> rect;
        for (auto& m : out) if (! m.isNote && m.number >= 91 && m.number <= 94) rect[m.number].push_back (m.value);
        bool alt = true;
        for (auto& kv : rect) { for (std::size_t i = 1; i < kv.second.size(); ++i) alt = alt && kv.second[i] != kv.second[i - 1] && (kv.second[i] == 0 || kv.second[i] == 127); alt = alt && kv.second[0] == 127; }
        expect (alt && rect[92].size() >= 59 && rect[92].size() <= 61 && rect[94].size() >= 1199, "rect CC: 127 at the start, then 0 / 127 at every edge (1 Hz: 60 in 30 s; 20 Hz: 1200)");

        // Notes: one per rising edge, 20 ms long, never two on at once on a pitch, each one ended.
        std::map<int, uint64_t> onAt;
        bool notesOk = true; int count = 0;
        for (auto& m : out)
        {
            if (! m.isNote) continue;
            if (m.value > 0) { notesOk = notesOk && ! onAt.count (m.number); onAt[m.number] = m.sample; ++count; }
            else
            {
                notesOk = notesOk && onAt.count (m.number);
                const double len = (double) (m.sample - onAt[m.number]) / sr;
                notesOk = notesOk && len >= 0.0195 && len <= 0.0215;
                onAt.erase (m.number);
            }
        }
        expect (notesOk && count >= 600 + 30, "rect notes: one 20 ms note at each rising edge (" + std::to_string (count) + "), never overlapping");
        expect (onAt.size() <= 4, "at most one note per channel is on at the end");
    }

    // Notes merge when edges come closer than 20 ms; nothing is ever left hanging.
    {
        Settings s; s.mode = (int) Mode::phase; s.rectOut = (int) RectOut::note; s.slider[0] = sliderForHz (0.5);
        Engine e; e.prepare (sr); e.setSettings (s);
        std::vector<Msg> out;
        RecordSink sink; sink.out = &out;
        // Swing B's slider back and forth every 8 ms: its rect rises again and again, closer than 20 ms.
        for (int k = 0; k < 400; ++k)
        {
            s.slider[1] = (k % 2) ? 0.3 : 0.8;
            e.setSettings (s);
            sink.base = e.samplesDone();
            e.render (384, nullptr, 0, sink);
        }
        int on = 0, off = 0; bool overlap = false; std::map<int, bool> isOn;
        for (auto& m : out) if (m.isNote) { if (m.value > 0) { overlap = overlap || isOn[m.number]; isOn[m.number] = true; ++on; } else { isOn[m.number] = false; ++off; } }
        expect (! overlap && on > 0 && on < 50, "edges closer than 20 ms merge into one longer note (" + std::to_string (on) + " notes for 200 edges)");

        // Switching to CC, switching the output off, closing (bypass / new channel), a new sample rate: notes end.
        for (int how = 0; how < 4; ++how)
        {
            Settings t; t.rectOut = (int) RectOut::both; t.slider[0] = sliderForHz (10.0);
            Engine f; f.prepare (sr); f.setSettings (t);
            std::vector<Msg> o; RecordSink k; k.out = &o;
            while (! f.noteOn (0)) { k.base = f.samplesDone(); f.render (32, nullptr, 0, k); }
            if (how == 0) { t.rectOut = (int) RectOut::cc; f.setSettings (t); }
            if (how == 1) { t.rectOn[0] = false; f.setSettings (t); }
            if (how == 2) { k.base = f.samplesDone(); f.closeAll (0, k); }
            if (how == 3) f.prepare (44100.0);
            k.base = f.samplesDone();
            f.render (32, nullptr, 0, k);
            int ons = 0, offs = 0;
            for (auto& m : o) if (m.isNote && m.number == 49) (m.value > 0 ? ons : offs)++;
            const char* names[4] = { "switching Rect out to CC", "switching the rect output off", "closing (bypass, a new MIDI channel)", "a new sample rate" };
            expect (ons == offs && ! f.noteOn (0), std::string ("no note left hanging after ") + names[how]);
            if (how == 1 || how == 2)
            {
                bool zero = false;
                for (auto& m : o) if (! m.isNote && m.number == 91 && m.value == 0) zero = true;
                expect (zero || ! f.rect (0), std::string ("the rect CC goes back to 0 after ") + names[how]);
            }
        }
    }

    // Output switches.
    {
        Settings s; s.slider[0] = sliderForHz (2.0);
        s.sineOn[0] = false; s.asgnOn[1] = false; s.rectOn[2] = false;
        Engine e; e.prepare (sr); e.setSettings (s);
        std::vector<Msg> out;
        run (e, sr, 2.0, 512, &out);
        bool none = true;
        for (auto& m : out) if (! m.isNote && (m.number == 2 || m.number == 34 || m.number == 68 || m.number == 69 || m.number == 93)) none = false;
        expect (none, "an output switched off sends nothing");
        s.sineOn[0] = true; e.setSettings (s);
        out.clear();
        RecordSink k; k.out = &out; k.base = e.samplesDone();
        e.render (32, nullptr, 0, k);
        bool back = false;
        for (auto& m : out) if (m.number == 34 && ! m.isNote) back = true;
        expect (back, "switched back on, it is sent again at once");
    }

    // ---- The MIDI rate and the CPU ---------------------------------------------------------------------------------------------
    {
        // The ceiling, from the send rule: each 14-bit output at most once every 6 ms, coarse and fine (2 messages);
        // each rect at most two CC changes and one note on and off per cycle at 20 Hz.
        const double perSmooth = 2.0 / kMinIntervalSeconds;
        const double ceiling = 8.0 * perSmooth + 4.0 * (2.0 + 2.0) * kTopHz;
        Settings s; s.rectOut = (int) RectOut::cc;
        for (int ch = 0; ch < 4; ++ch) s.slider[ch] = 1.0 - 0.01 * ch;
        Engine e; e.prepare (sr); e.setSettings (s);
        RecordSink sink;
        const auto t0 = std::chrono::steady_clock::now();
        const double seconds = 120.0;
        for (long k = 0; k < (long) (seconds * sr / 512); ++k) e.render (512, nullptr, 0, sink);
        const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / seconds * 100.0;
        const double rate = (double) sink.count / seconds;

        Settings d;                                                           // the defaults (free mode, 0.07 to 3 Hz)
        Engine f; f.prepare (sr); f.setSettings (d);
        RecordSink ds;
        for (long k = 0; k < (long) (60.0 * sr / 512); ++k) f.render (512, nullptr, 0, ds);
        const double rateDefault = (double) ds.count / 60.0;

        Settings slow; for (int ch = 0; ch < 4; ++ch) slow.slider[ch] = sliderForHz (0.05);
        Engine g; g.prepare (sr); g.setSettings (slow);
        RecordSink ss;
        for (long k = 0; k < (long) (120.0 * sr / 512); ++k) g.render (512, nullptr, 0, ss);
        const double rateSlow = (double) ss.count / 120.0;

        std::printf ("MIDI: %.0f messages a second at the top rate with all 12 outputs on (ceiling %.0f from the send rule; Ogdoad with Ex on: about 4,600); %.0f at the defaults; %.0f with all four at 0.05 Hz\n",
                     rate, ceiling, rateDefault, rateSlow);
        std::printf ("CPU: %.4f%% of one core\n", cpu);
        expect (rate <= ceiling && rate < 4600.0, "MIDI rate at the top rate within the ceiling from the send rule (" + num (rate, 0) + " <= " + num (ceiling, 0) + ")");
        expect (rateSlow < rate / 3.0, "slow LFOs send far fewer messages (" + num (rateSlow, 0) + ")");
        expect (cpu < 1.0, "CPU under 1% of one core (" + num (cpu, 4) + "%)");
    }

    if (failures == 0) std::printf ("engine tests: %d checks passed\n", checks);
    else std::printf ("engine tests: %d of %d checks FAILED\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
