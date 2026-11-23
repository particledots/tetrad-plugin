# Changelog

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
