# Tetrad

An original AU / VST3 MIDI effect for Logic Pro: four linked LFO channels (A, B, C, D), each with a sine, an assignable
(asgn) and a rectangle (rect) output, that run free or follow each other in phase, divide and multiply modes, all sent
out as smooth MIDI CC. Inspired by the XAOC Devices Batumi II quad LFO and its Poti II expander (the expander section is
called Tetrad Ex). All code is our own, written from the manual's description of the controls; it is not a port or
copy of anything, contains no XAOC code or artwork, and is not affiliated with XAOC Devices.

Status: **version 0.2.0 (milestone 2: Tetrad Ex live, on top of milestone 1) tested in Logic on 2026-11-23 and
working on both test Macs: the M4 Mac mini (macOS Tahoe, Logic Pro 11.2.2) and the Intel Mac (macOS 12.7.8, Logic Pro
10.7.8).** Milestone 1 (0.1.0, the four channels, every mode and output, the mapper) passed on both Macs on 2026-11-22.

0.2.0 tested on both Macs: it opens as v0.2.0 with Ex dimmed and off; switching Ex on changes nothing until a control is
moved; sine level, shape with fold, width with the shape knob left, a stepped random asgn wave, Ex off again, and
save and reopen with Ex on. The mapper is unchanged (0.1.0).

The open window's CPU cost was **not** reduced in practice. Intel Mac, 0.2.0 (Logic's % CPU, transport running, a few
notes, Tetrad and the Tetrad Mapper on the track): 20-21% window closed, 19-21% window closed with the mapper bypassed,
33-35% window open. For 0.1.0 the same readings were 18-21% closed and 29-33% open. The drawing was made cheaper (on
GitHub's Mac the open window's extra cost measured about 7.3 to 6.4 points), but about 2.5 points of that cost is the
floor of any open plugin window of this kind, and on the Intel Mac the readings did not improve (the open window
added about 13 points, against about 11). Treat an open window as costing about 10 to 14 points on that Mac and close
it while playing if CPU is tight.

Milestone 1 tested on both Macs: installing, the window and its lights, resizing, the mapper moving a synth's cutoff smoothly,
phase and multiply mode, mod from, reset from Notes, following Logic's tempo in time with the click, switching an
output off and on, save and reopen, and sharing a track with Ogdoad (Ogdoad, Tetrad, Ogdoad Mapper, Tetrad Mapper).
CPU on the Intel Mac (Activity Monitor, Logic's % CPU, transport running, a few notes playing): Tetrad alone 18-21%
with its window closed, 29-33% open, 28-32% open with the mapper bypassed; with Ogdoad on the same track 23-25%
closed, 32-40% open, 39% open with the Tetrad Mapper bypassed. Bypassing the mapper made almost no difference, so the
mapper's work is small; the open window is the larger share.

## The panel

- **Mode** (top left; the light beside it blinks as a slider crosses 90, 180 or 270 degrees in phase mode, or moves to
  another ratio in divide and multiply mode):
  - *free*: four separate LFOs; each slider sets its channel's speed, 0.01 to 20 Hz.
  - *phase*: B, C and D run at A's speed, each delayed by its slider (0 to 360 degrees).
  - *divide*: B, C and D run at A's speed divided by 1, 2, 3, 4, 5, 8, 16 or 32 (up to 53 minutes per cycle).
  - *multiply*: B, C and D run at A's speed multiplied by those ratios. A ratio that would go above 20 Hz steps down
    to the largest one that does not.
  - Entering a mode starts all four together at the start of a cycle, so in divide and multiply mode they meet again
    there after every whole number of cycles.
- **Asgn wave**: triangle, saw down, saw up, trapezoid, step random, smooth random (the asgn output of all four).
  - The random waves take a new value twice per cycle. Step random holds it; smooth random glides to it.
  - In free mode the four random waves are unrelated.
  - In phase mode, B, C and D play A's sequence, delayed (or ahead, with a negative shift): a fugue of one sequence.
  - In divide mode they play every 2nd, 3rd... value of A's sequence; in multiply mode every 2nd, 3rd... value is A's.
- **Tempo**: *Hz*, or *Logic*. On Logic the sliders pick note lengths instead (32 bars to 1/16, with dotted and
  triplet values; 1 bar = 4 beats): all four in free mode, A only in the other modes (B, C, D keep their phase or
  ratio, so they follow A in time with the song). While Logic plays, everything is locked to the song position, so
  every playback lines up the same way; while it is stopped it keeps running at the project tempo. A length that
  would go above 20 Hz (1/16 above 300 BPM) steps to the next longer one. If Logic sends no tempo, it runs at
  120 BPM and the panel says "no tempo".
- **Rect out**: *CC* (127 while high, 0 while low), *Note* (a 20 ms note at each rising edge: A 49 C#2, B 50 D2,
  C 51 D#2, D 52 E2, velocity 100, on the MIDI out channel; it passes the mapper and plays the track's instrument),
  or *Both*. Edges closer than 20 ms merge into one longer note.
- **View**: *lights* (the default) shows each channel as a big light that follows its sine; *waves* shows the last
  2.7 seconds of each channel's sine (amber) and asgn (grey) instead. Saved with the project; not an audio parameter.
- **MIDI out** channel (default 12). Your own notes and every other message pass through untouched.
- **Each channel** (A to D):
  - **the slider**: speed (Hz or note length), phase, or ratio, depending on the mode; the read-out below says which.
  - **mod from** and **depth** (−/+): another channel's sine or asgn, or the mod wheel, moving this channel, like a
    patch cable into the module's CV input. In free mode (and for A in every mode) it bends the speed, up to ±5
    octaves; in phase mode it moves the delay, up to ±2.5 cycles; in divide and multiply mode it steps the ratio,
    up to 7 steps either way; following the tempo it steps the note length. In phase mode the movement glides just
    enough that no channel goes faster than 20 Hz. A channel never modulates itself.
  - **reset from**: your notes (any note-on), or another channel's rect (at each rising edge), restarts the channel
    at the start of its cycle. Resetting A restarts B, C and D with it in phase, divide and multiply mode. In phase
    mode a reset of B, C or D gives that channel a new offset, kept until the mode changes. Resetting A from a
    channel that follows A makes a loop that can hold A in a very short cycle, as the same patch does on the module.
  - **the three output lights**: sine, asgn, rect, with their CC numbers. Click a light to switch that output off
    (an empty ring): it stops sending, which saves MIDI and CPU. Click again to switch it back on.
- **Resizing**: drag the corner at the bottom right. The window scales from 80% to 200% of its normal size (736 x 448
  to 1840 x 1120) with its proportions locked. The size is saved with the project.
- Each project opens with every channel at the start of its cycle, like the module switched on. The random seed is
  saved with the project, so the random waves repeat; a new copy of the plugin gets a new seed.

## Tetrad Ex (the Poti II section)

Switch it on with the pill at the top of the Ex section; while it is off the controls are dimmed and Tetrad sends
exactly the MIDI of 0.1.0. With Ex on and every control at its default the MIDI is also identical. Ex adds no outputs
and no MIDI of its own; it changes the values the channels already send, so the mapper needs nothing new. Per channel:

- **sine** and **asgn** (level knobs): the size of that output around its centre; fully left holds it at half.
- **wave**: the channel's own asgn wave, or *Main* to follow the global asgn wave.
- **shape**, **from** and the target: the shape knob goes from -1 to +1. *from* is *off* (the knob alone sets the
  shape), the mod wheel, or another channel's sine or asgn (read one tick earlier). The target is one of:
  - *fold*: the sine and asgn fold back on themselves, more as the shape moves from the middle (gain 1 to 5).
  - *wave*: steps along the wave menu by up to 5 places, wrapping round.
  - *width*: the rect's duty cycle moves from 5% to 95% (50% at the middle). Rising edges are at least 48 ms apart,
    so edges, notes and resets never multiply.

## How the outputs reach a knob in Logic

Logic has no CV. The sine and asgn outputs go out as standard 14-bit CC pairs (16,384 steps instead of 128), the rect
as a CC (127 / 0), all on channel 12:

| Output | A | B | C | D |
|---|---|---|---|---|
| sine coarse / fine CC | 2 / 34 | 4 / 36 | 5 / 37 | 8 / 40 |
| asgn coarse / fine CC | 66 / 67 | 68 / 69 | 70 / 71 | 72 / 73 |
| rect CC (127 / 0) | 91 | 92 | 93 | 94 |
| rect note (Note / Both) | 49 (C#2) | 50 (D2) | 51 (D#2) | 52 (E2) |

None of these numbers is used by Ogdoad (channel 16: CC 12-31, 44-63, 102-105, notes 36-39), Strange Drift (channel
15: CC 106-119), Hillside (channel 16: CC 3, 9, 35, 41, 85-89, notes 40-44), Bryn (channel 14: CC 80-83, 90, notes
45-47) or particlehold (channel 13: CC 75-79, note 48), on any channel, so all six can share a track (even all on one
channel). Every CC that MIDI leaves without a job is already taken by those five, so Tetrad's numbers have standard
jobs (breath, foot, portamento time and balance with their standard fine partners; the pedal switches 66-69 and
sound controllers 1-4 (70-73); effect depths 91-94). That is safe: the Tetrad Mapper stops them before the instrument, and none of those plugins
listens to them. Avoided on purpose: bank select, mod wheel, data entry, volume, pan, expression, sustain, RPN / NRPN
and the channel mode messages. CC 65, 74, 84 and 95 stay unused (kept free in case Ex ever needs them).

The **Tetrad Mapper** (a Scripter script, in the download as `Tetrad Mapper (paste into Scripter).txt`, source in
`tools/mapper/Tetrad-Mapper.js`) sits in a MIDI FX slot below Tetrad. It has 8 lanes; each lane has a Source (A, B, C
or D: sine, asgn or rect), a Target (Logic's *Learn plug-in parameter*), and Min % / Max %. It moves the learnt knob on
any plugin below it on the same channel strip and stops Tetrad's CC from reaching the instrument; notes (including the
rect notes) pass through. A rect source jumps the knob between Min and Max. Set up once per Mac (saved as a Scripter
preset), then chosen from Scripter's preset menu on each track. `INSTALL.txt` in the download has the steps. Tetrad
Ex adds no outputs, so this mapper is the final one.

Sharing a track: the plugins first, then their mappers. With Ogdoad: Ogdoad, Tetrad, Ogdoad Mapper, Tetrad Mapper
(Ogdoad's mapper first, so the Tetrad Mapper only sees Tetrad's messages). Plugins only hear notes from above them:
put Hillside, Bryn or particlehold above Tetrad unless you want Tetrad's rect notes to play them.

Smoothness and CPU: each output is sent only when it has moved at least 0.2% of its range, or after 100 ms for smaller
moves, at most once every 6 ms, at its exact sample (Strange Drift 0.1.1's rule); the coarse part only when it changes.
Any LFO faster than about 0.2 Hz already sends at the 6 ms limit, so the traffic is about the same at normal speeds as
at the top rate: about 2,800 MIDI messages a second with all 12 outputs on (Ogdoad with Ex on sends about 4,600), about
1,900 at the defaults, about 500 with all four at 0.05 Hz (with Ex shaping every channel hard: about 2,950 and 740). Switching off the outputs you do not use cuts it. With
Ogdoad (Ex on) on the same track, Logic carries about 7,400 a second, and the two mappers together handle about 10,200
(Ogdoad's mapper first) instead of 4,600. The engine uses well under 0.1% of one core.

## Folders

| Folder | What it is |
|---|---|
| `engine/` | The engine. Plain C++, no plugin code. `CcStream` from Strange Drift 0.1.1 (plus switching a lane back on). New: `Outputs.h` (the MIDI numbers and menus), `Waves.h` (the wave shapes and random sequences), `Engine` (the four channels, the modes, tempo following, mod and reset routing, rect CC and notes). |
| `plugin/` | The plugin wrapper: what Logic sees, the list of controls, and the panel (`PluginEditor`). `plugin/preview/` draws the window to pictures and checks the plugin without Logic. |
| `tools/mapper/` | The Scripter mapper script. |
| `tools/install/` | `Install-Unofficial-Plugin.command`, the same installer as the other particledots plugins (Audio Unit only). |
| `.github/workflows/` | The automatic build. |

## Getting the plugin

1. Open the **Actions** tab of this repo and click the latest **Build Tetrad** run.
2. At the bottom, under **Artifacts**, download **Tetrad-macOS** and unzip it (twice: the download is a zip containing `Tetrad-macOS.zip`).
3. Inside the `Tetrad-macOS` folder are `Tetrad.component`, `Tetrad.vst3`, `INSTALL.txt`, `auval.log`,
   `pluginval-AU.log`, `pluginval-VST3.log`, `Tetrad Mapper (paste into Scripter).txt` and `Install-Unofficial-Plugin.command`.
4. In Terminal, once, clear the download block: `xattr -dr com.apple.quarantine ~/Downloads/Tetrad-macOS/*`
   (adjust the path if you unzipped elsewhere).
5. **Easy install (Logic, Audio Unit):** double-click `Install-Unofficial-Plugin.command`. It asks for your Mac
   password once, installs the AU for all users and restarts the AU registrar.
6. In Logic it is a MIDI effect: MIDI FX slot of a Software Instrument track, **Audio Units > particledots > Tetrad**.

## What the build checks

- **Engine self-test** (Linux, before the Mac build): the wave shapes and where each starts; the sliders' Hz,
  degrees, ratios and note lengths; free, phase, divide and multiply mode (speeds, delays, ratios, starting together,
  and the random waves' relationships); the 20 Hz top rate in every mode with fast modulation; resets from notes and
  from rect, the phase-mode offset; following the tempo (locked while playing, running when stopped, lining up again
  at play, 120 BPM without a tempo, lengths stepped under the top rate); every mod-from and reset-from choice in every
  mode and every reset combination (about 3,800 runs: always finite, inside −1..+1, never a standstill, never faster
  than 20 Hz); the same speeds and random sequences at 44.1, 48 and 96 kHz; the same MIDI at any block size; the CC
  rules; rect CC and notes (20 ms, merging, never left hanging); the output switches; the MIDI rate against a ceiling
  worked out from the send rule; the CPU.
- **Mapper test**: the real mapper script run in a stand-in for Scripter, fed by the engine's real MIDI stream:
  values, Min/Max, upside-down lanes, rect lanes, 14-bit detail, rect notes passing, all of Tetrad's CC stopped, the
  channel setting; and chained with copies of the Ogdoad, Strange Drift, Hillside, Bryn and particlehold mappers, with
  Tetrad's mapper first, last, and all six on one MIDI channel: each lane follows its own plugin, no CC reaches the
  instrument, every note does.
- **Mac build**: universal binary (Apple Silicon + Intel), ad-hoc signing, Apple's `auval` (as a MIDI effect,
  `aumi Ttrd Pdot`), pluginval at strictness 5 on the AU and the VST3, the package contents.
- **Window picture and plugin checks**: the real window drawn to pictures (artifact **Tetrad-window-pictures**:
  default, running, divide, multiply, tempo without a host tempo, outputs switched off, waves, small / medium /
  large), every control moving only its own parameter, the choice buttons following their parameters, the mode light,
  only what moved drawn again, save and reload (including the seed, the view and the window size), the window size
  limits and locked proportions, MIDI passing through, CC on the chosen channel, changing the channel, bypass, rect
  notes, notes resetting, the mod wheel, Logic's tempo, block sizes; Ex off and Ex at defaults sending the same MIDI as
  0.1.0, every Ex control, fold / wave / width and every shape source, the 48 ms rise gap, a 0.1.0 project opening.
- **Window cost** (GitHub's Mac): the real window hosted inside a host window, 0.1.0 against this build, three rounds.

## Fixed names

Plugin code `Ttrd`, manufacturer `Pdot`, bundle id `com.particledots.tetrad`, target `Tetrad`, engine library
`tetrad_engine`. Parameter ids: `mode`, `asgnWave`, `tempoFollow`, `rectOut`, `outChannel`, `slider1` to `slider4`,
`modFrom1` to `modFrom4`, `modDepth1` to `modDepth4`, `resetFrom1` to `resetFrom4`, `sineOn1` to `sineOn4`, `asgnOn1`
to `asgnOn4`, `rectOn1` to `rectOn4` (1-4 = A-D). Tetrad Ex (0.2.0): `exOn`, `sineLevel1-4`,
`asgnLevel1-4`, `exWave1-4`, `shapeAmount1-4`, `shapeFrom1-4`, `shapeTarget1-4`. Other saved state: `randomSeed`,
`wavesView`, `editorWidth`. The CC and note numbers in the table above, the menu orders (mode, asgn wave, rect out,
mod from, reset from) and the mapper's Source menu order are fixed too. These never change, so saved projects keep
finding the plugin and its settings.
