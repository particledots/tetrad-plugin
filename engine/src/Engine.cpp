#include "tetrad/Engine.h"

#include <algorithm>
#include <cmath>

namespace tetrad
{
    Engine::Engine()
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            cc.setLane (ch, kSineCoarse[ch], kSineFine[ch]);
            cc.setLane (kNumChannels + ch, kAsgnCoarse[ch], kAsgnFine[ch]);
        }
        rectSent.fill (-1);
        ratioNow.fill (1);
        prepare (48000.0);
        computeOutputs();
    }

    void Engine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        noteTicks = std::max (1, (int) std::lround (kNoteSeconds * sampleRate / kTickSamples));
        // The clock restarts; the channels stay where they are. Everything is sent again from the next tick, and a rect
        // note still on is ended there (a plugin cannot send MIDI from here).
        samples = ticks = 0;
        samplesToTick = 0;
        traceCountdown = 0;
        cc.reset();
        rectSent.fill (-1);
        laneWasOn.fill (false);
        for (auto& n : notes) n.ticksLeft = 0;
    }

    int Engine::cappedMultiplyIndex (int index, double aHz)
    {
        index = std::clamp (index, 0, kNumRatios - 1);
        while (index > 0 && aHz * kRatios[index] > kTopHz * (1.0 + 1.0e-9))
            --index;
        return index;
    }

    int Engine::cappedLengthIndex (int index, double bpm)
    {
        index = std::clamp (index, 0, kNumLengths - 1);
        while (index > 0 && bpm / 60.0 / kLengthBeats[index] > kTopHz * (1.0 + 1.0e-9))
            --index;
        return index;
    }

    float Engine::trace (int ch, int which, int k) const
    {
        const int idx = ((traceHead - traceFill + k) % kTracePoints + kTracePoints) % kTracePoints;
        return traces[(std::size_t) ch][(std::size_t) (idx * 2 + which)];
    }

    double Engine::modSource (int ch) const
    {
        const int m = std::clamp (settings.modFrom[ch], 0, kNumModSources - 1);
        if (m == 0) return 0.0;
        if (m == 1) return modWheel;                                     // 0..1: at rest it does nothing
        const int other = otherChannel (ch, (m - 2) / 2);
        return (m - 2) % 2 == 0 ? sineOut[(std::size_t) other] : asgnOut[(std::size_t) other];
    }

    bool Engine::resetRequested (int ch) const
    {
        const int m = std::clamp (settings.resetFrom[ch], 0, kNumResetSources - 1);
        if (m == 0) return false;
        if (m == 1) return noteResetPending;
        return rising[(std::size_t) otherChannel (ch, m - 2)];
    }

    // A restart: the channel jumps to the start of its next cycle (phase 0). Used for resets and for entering a mode.
    void Engine::restartSelfTimed (int ch)
    {
        auto& p = pos[(std::size_t) ch];
        const double np = std::floor (p) + 1.0;
        lockOffset[(std::size_t) ch] += np - p;
        p = np;
        jumped[(std::size_t) ch] = true;
        if (ch == 0 && lastMode != (int) Mode::free)
        {
            aOrigin = (int64_t) np;                                      // B, C and D follow A
            for (int k = 1; k < kNumChannels; ++k) jumped[(std::size_t) k] = true;
        }
    }

    void Engine::enterMode (int newMode)
    {
        lastMode = newMode;
        offset.fill (0.0);
        modShift.fill (0.0);
        jumped.fill (true);
        if (wasLocked)
        {
            // Following Logic while it plays: everything stays lined up with the song instead of restarting.
            lockOffset[0] = 0.0;
            aOrigin = 0;
        }
        else
            restartA = true;                            // done after A has moved on this tick, as a reset is
    }

    double Engine::randomFor (int ch, int64_t k) const
    {
        const int mode = lastMode;
        if (ch == 0 || mode == (int) Mode::free)
            return randomValue (seed, ch, k);
        if (mode == (int) Mode::phase)
            return randomValue (seed, 0, k);                             // A's sequence, delayed
        const int64_t r = ratioNow[(std::size_t) ch];
        if (mode == (int) Mode::divide)
            return randomValue (seed, 0, 2 * aOrigin + k * r);          // every r-th value of A's sequence
        // multiply: every r-th value is A's, the ones between are the channel's own
        const int64_t q = (k >= 0 ? k / r : -((-k + r - 1) / r));
        if (q * r == k)
            return randomValue (seed, 0, 2 * aOrigin + q);
        return randomValue (seed, ch, k);
    }

    void Engine::computeOutputs()
    {
        const auto wave = (Wave) std::clamp (settings.asgnWave, 0, kNumWaves - 1);
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            const double p = pos[(std::size_t) ch];
            const double ph = fractionOf (p);
            sineOut[(std::size_t) ch] = sineWave (ph);
            rectOut[(std::size_t) ch] = rectHigh (ph);
            double a = 0.0;
            switch (wave)
            {
                case Wave::triangle:  a = triangleWave (ph); break;
                case Wave::sawDown:   a = sawDownWave (ph); break;
                case Wave::sawUp:     a = sawUpWave (ph); break;
                case Wave::trapezoid: a = trapezoidWave (ph); break;
                case Wave::stepRandom:
                    a = randomFor (ch, floorToInt (2.0 * p));
                    break;
                case Wave::smoothRandom:
                {
                    const int64_t k = floorToInt (2.0 * p);
                    const double before = randomFor (ch, k - 1), now = randomFor (ch, k);
                    a = before + (now - before) * fractionOf (2.0 * p);
                    break;
                }
            }
            asgnOut[(std::size_t) ch] = a;
        }
    }

    void Engine::render (int numSamples, const InputEvent* events, int numEvents, CcSink& sink)
    {
        int at = 0, ev = 0;
        auto take = [this] (const InputEvent& e)
        {
            if (e.kind == InputEvent::noteOn) noteResetPending = true;
            else modWheel = std::clamp (e.value, 0.0, 1.0);
        };
        while (at < numSamples)
        {
            if (samplesToTick == 0)
            {
                while (ev < numEvents && events[ev].offset <= at)
                    take (events[ev++]);
                tick (at, sink);
                samplesToTick = kTickSamples;
            }
            const int n = std::min (samplesToTick, numSamples - at);
            at += n;
            samples += (uint64_t) n;
            samplesToTick -= n;
        }
        for (; ev < numEvents; ++ev)
            take (events[ev]);
    }

    void Engine::tick (int at, CcSink& sink)
    {
        const double dt = (double) kTickSamples / sampleRate;
        ++ticks;
        jumped.fill (false);

        const int mode = std::clamp (settings.mode, 0, kNumModes - 1);
        const bool lockedNow = settings.tempoFollow && host.playing && host.hasPosition;
        const double bpm = host.hasTempo ? std::clamp (host.bpm, 20.0, 999.0) : 120.0;
        const double ppq = host.ppq + (double) at / sampleRate * bpm / 60.0;

        // The song position: Logic's, followed tick by tick (so it is exactly the same at any block size); taken again
        // from Logic whenever the two differ by more than a thousandth of a beat (Logic jumped, looped or changed tempo).
        if (lockedNow)
        {
            const double expected = songBeats + dt * bpm / 60.0;
            songBeats = (! wasLocked || std::fabs (ppq - expected) > 1.0e-3) ? ppq : expected;
        }

        if (lockedNow && ! wasLocked)
        {
            // Logic started playing: everything lines up with the song position.
            lockOffset.fill (0.0);
            offset.fill (0.0);
            aOrigin = 0;
            jumped.fill (true);
        }
        wasLocked = lockedNow;

        if (lastMode < 0) lastMode = mode;          // the first tick: the channels simply start, all at phase 0
        else if (mode != lastMode) enterMode (mode);

        for (int ch = 0; ch < kNumChannels; ++ch)
            resetNow[(std::size_t) ch] = resetRequested (ch);
        noteResetPending = false;

        // Channels that run on their own: all four in free mode, A in the others.
        for (int ch = 0; ch < (mode == (int) Mode::free ? kNumChannels : 1); ++ch)
        {
            const auto c = (std::size_t) ch;
            const double depth = settings.modFrom[ch] != 0 ? std::clamp (settings.modDepth[ch], -1.0, 1.0) * modSource (ch) : 0.0;
            double f;
            if (settings.tempoFollow)
            {
                const int idx = cappedLengthIndex (stepIndex (settings.slider[ch] + depth, kNumLengths), bpm);
                if (idx != lengthNow[c]) jumped[c] = jumped[c] || lockedNow;
                lengthNow[c] = idx;
                const double beats = kLengthBeats[idx];
                f = bpm / 60.0 / beats;
                if (lockedNow) pos[c] = songBeats / beats + lockOffset[c];
                else pos[c] += f * dt;
            }
            else
            {
                f = std::clamp (sliderHz (settings.slider[ch]) * std::exp2 (kModOctaves * depth), kBottomHz, kTopHz);
                pos[c] += f * dt;
            }
            hzNow[c] = f;
            if (resetNow[c] || (ch == 0 && restartA))
            {
                restartSelfTimed (ch);
                if (resetNow[c]) ++resets[c];
            }
            if (ch == 0) restartA = false;
        }

        // B, C and D following A.
        if (mode != (int) Mode::free)
        {
            const double fA = hzNow[0];
            for (int ch = 1; ch < kNumChannels; ++ch)
            {
                const auto c = (std::size_t) ch;
                const double depth = settings.modFrom[ch] != 0 ? std::clamp (settings.modDepth[ch], -1.0, 1.0) * modSource (ch) : 0.0;
                if (mode == (int) Mode::phase)
                {
                    // The modulation part of the shift glides no faster than keeps the channel at or under the top rate.
                    const double target = depth * kModCycles;
                    const double maxStep = std::max (0.0, kTopHz - fA) * dt;
                    modShift[c] += std::clamp (target - modShift[c], -maxStep, maxStep);
                    const double shift = settings.slider[ch] + modShift[c];
                    if (settings.slider[ch] != sliderBefore[c]) jumped[c] = true;     // the slider itself moved
                    shiftNow[c] = shift;
                    pos[c] = pos[0] - shift + offset[c];
                    hzNow[c] = fA;
                }
                else
                {
                    int idx = stepIndex (settings.slider[ch] + depth, kNumRatios);
                    if (mode == (int) Mode::multiply) idx = cappedMultiplyIndex (idx, fA);
                    const int r = kRatios[idx];
                    if (r != ratioNow[c]) jumped[c] = true;
                    ratioNow[c] = r;
                    const double fromA = pos[0] - (double) aOrigin;
                    pos[c] = (mode == (int) Mode::divide ? fromA / r : fromA * r) + offset[c];
                    hzNow[c] = mode == (int) Mode::divide ? fA / r : fA * r;
                }
                if (jumped[0]) jumped[c] = true;
                if (resetNow[c])
                {
                    const double np = std::floor (pos[c]) + 1.0;
                    offset[c] += np - pos[c];
                    pos[c] = np;
                    jumped[c] = true;
                    ++resets[c];
                }
            }
        }

        for (int ch = 0; ch < kNumChannels; ++ch) sliderBefore[(std::size_t) ch] = settings.slider[ch];
        computeOutputs();
        send (at, dt, sink);

        if (--traceCountdown <= 0)
        {
            recordTrace();
            traceCountdown = kTraceTicks;
        }
    }

    void Engine::send (int at, double dt, CcSink& sink)
    {
        const auto rectMode = (RectOut) std::clamp (settings.rectOut, 0, 2);
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            const auto c = (std::size_t) ch;
            const bool on[2] = { settings.sineOn[ch], settings.asgnOn[ch] };
            const double value[2] = { sineOut[c], asgnOut[c] };
            for (int w = 0; w < 2; ++w)
            {
                const int lane = w * kNumChannels + ch;
                if (on[w] && ! laneWasOn[(std::size_t) lane]) cc.resetLane (lane);
                laneWasOn[(std::size_t) lane] = on[w];
                if (on[w]) cc.update (lane, value[w], dt, at, sink);
            }

            // Rect as a CC: 127 while high, 0 while low, sent when it changes.
            const bool high = rectOut[c];
            rising[c] = high && ! rectBefore[c];
            rectBefore[c] = high;
            const bool wantCc = settings.rectOn[ch] && rectMode != RectOut::note;
            if (wantCc)
            {
                const int v = high ? 127 : 0;
                if (v != rectSent[c]) { sink.cc (at, kRectCc[ch], v); rectSent[c] = v; }
            }
            else if (rectSent[c] == 127)
            {
                sink.cc (at, kRectCc[ch], 0);
                rectSent[c] = 0;
            }

            // Rect as a note: 20 ms at each rising edge; an edge while the note is still on keeps it on (they merge).
            auto& n = notes[c];
            const bool wantNote = settings.rectOn[ch] && rectMode != RectOut::cc;
            if (rising[c] && wantNote)
            {
                if (! n.on) { sink.note (at, kRectNote[ch], kNoteVelocity); n.on = true; ++n.count; }
                n.ticksLeft = noteTicks;
            }
            else if (n.on && (! wantNote || --n.ticksLeft <= 0))
            {
                sink.note (at, kRectNote[ch], 0);
                n.on = false;
                n.ticksLeft = 0;
            }
        }
    }

    void Engine::closeAll (int at, CcSink& sink)
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            const auto c = (std::size_t) ch;
            if (rectSent[c] == 127) sink.cc (at, kRectCc[ch], 0);
            rectSent[c] = -1;
            if (notes[c].on) sink.note (at, kRectNote[ch], 0);
            notes[c].on = false;
            notes[c].ticksLeft = 0;
        }
        cc.reset();
        laneWasOn.fill (false);
    }

    void Engine::recordTrace()
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            traces[(std::size_t) ch][(std::size_t) (traceHead * 2)]     = (float) sineOut[(std::size_t) ch];
            traces[(std::size_t) ch][(std::size_t) (traceHead * 2 + 1)] = (float) asgnOut[(std::size_t) ch];
        }
        traceHead = (traceHead + 1) % kTracePoints;
        traceFill = std::min (kTracePoints, traceFill + 1);
    }
}
