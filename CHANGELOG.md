# Changelog

## 0.2.0 (milestone 2, 2026-11-23): tested in Logic on the M4 Mac mini and the Intel Mac

- Tetrad Ex is live with the reserved ids: on switch; per channel sine level, asgn level, own asgn wave or Main,
  shape amount, shape from (off, mod wheel, another channel's sine or asgn) and shape target (fold, wave, width).
  Ex off, or on with defaults, sends the same MIDI as 0.1.0. No new CC; the Tetrad Mapper stays 0.1.0.
- Width keeps rising edges at least 48 ms apart. MIDI with Ex shaping every channel hard: about 2,950 messages a
  second at the top rate (ceiling 2,987).
- Lights drawn more cheaply: pre-drawn light images, a cached panel, small per-light components, one 30 frame a second
  timer. On GitHub's Mac the open window's extra CPU measured about 7.3 to 6.4 points against 0.1.0.
- Tested 2026-11-23: steps 1-8 passed on the M4 Mac mini and the Intel Mac. Intel CPU: 20-21% window closed, 19-21%
  closed with the mapper bypassed, 33-35% window open (0.1.0: 18-21% closed, 29-33% open). **The open window's cost did
  not improve on the Intel Mac**; it is still open work.

## 0.1.0 (milestone 1, 2026-11-22): tested in Logic on the M4 Mac mini and the Intel Mac

- First build. Four linked LFO channels (A-D) with sine, asgn and rect outputs; free, phase, divide and multiply
  modes; six asgn waves (two random); Hz or Logic's tempo (note lengths, locked to the song while playing); per
  channel mod from / depth and reset from; rect as CC, notes 49-52, or both; outputs switched off by clicking their
  lights; 20 Hz top rate.
- MIDI out on channel 12: sine CC 2/34, 4/36, 5/37, 8/40; asgn CC 66-73; rect CC 91-94. Strange Drift 0.1.1's send
  rule from the start (0.2% or 100 ms, at most every 6 ms).
- The Tetrad Mapper 0.1.0 (8 lanes, 13 sources). Final: Tetrad Ex adds no outputs.
- The window as approved in the draft, resizable from 80% to 200%, Lights | Waves, drawing only what moved; Tetrad Ex
  drawn dimmed until milestone 2.
- Tested 2026-11-22: every step passed on the M4 Mac mini (macOS Tahoe, Logic Pro 11.2.2) and the Intel Mac (macOS
  12.7.8, Logic Pro 10.7.8), including sharing a track with Ogdoad. Intel CPU (Activity Monitor): 18-21% window
  closed, 29-33% open; with Ogdoad 23-25% closed, 32-40% open; bypassing the mapper made almost no difference.
