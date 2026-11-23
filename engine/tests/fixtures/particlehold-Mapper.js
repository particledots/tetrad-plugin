// particlehold Mapper, for Logic's Scripter (MIDI FX). Version 0.1.0.
//
// particlehold sends its outputs as MIDI CC on MIDI channel 13 (unless you changed it):
//   Signal   CC 75 (coarse) + CC 76 (fine)   the held value (0 V is the middle, or the bottom with the rectifier on)
//   Noise    CC 77 (coarse) + CC 78 (fine)   the white noise, a new value every few milliseconds
//   Clock    CC 79   (127 while the clock is high, then 0)
// This script turns them into movements of knobs on any plugin BELOW it on the same channel strip (the instrument, or
// an audio effect).
//
// Setup, once per Mac (and again after each new version of this script):
//   Add Scripter in a MIDI FX slot below particlehold, click "Open Script in Editor", delete everything, paste this
//   whole file, click "Run Script", close the editor, and in Scripter's preset menu choose "Save As..." and call it
//   "particlehold Mapper". From then on it is in Scripter's preset menu.
// Per track:
//   particlehold in a MIDI FX slot, Scripter with the "particlehold Mapper" preset in a slot below it.
//   For each lane you want: pick a Source, open its Target menu, choose "Learn plug-in parameter" and move the knob
//   you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it upside down).
//   The Clock source jumps the knob from Min to Max while the clock is high. Need more than 8 lanes? Add a second
//   Scripter with the same preset below the first.
//
// particlehold's CC messages stop here, so they never reach the instrument. Everything else passes through unchanged:
// your notes (and particlehold's Clock note), and Bryn's, Hillside's, Ogdoad's and Strange Drift's CC, so their
// mappers can sit on the same track. With all five: the plugins first, then their mappers in any order.

var PluginParameters = [
    { name: "particlehold MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 13 }
];

var LANES = 8;
// This order is fixed: saved lanes keep their source.
var SOURCES = ["Off", "Signal", "Noise", "Clock"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

// particlehold's CC numbers: which Source each belongs to.
var COARSE_OF = [];   // coarse CC number -> source
var FINE_OF = [];     // fine CC number -> source
var GATE_OF = [];     // on/off CC number -> source
COARSE_OF[75] = 1; FINE_OF[76] = 1;   // Signal
COARSE_OF[77] = 2; FINE_OF[78] = 2;   // Noise
GATE_OF[79] = 3;                      // Clock

var channel = 13;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [0, 0, 0];               // the latest coarse part of Signal and Noise (index = source)
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}

function readAll() {
    channel = Math.round(GetParameter("particlehold MIDI channel"));
    for (var i = 0; i < LANES; i++) {
        laneSource[i] = Math.round(GetParameter("Lane " + (i + 1) + " source"));
        laneMin[i] = GetParameter("Lane " + (i + 1) + " min %") / 100;
        laneMax[i] = GetParameter("Lane " + (i + 1) + " max %") / 100;
        laneLast[i] = -1;
    }
    ready = true;
}

function ParameterChanged(index, value) {
    if (index == 0) { channel = Math.round(value); return; }
    var lane = Math.floor((index - 1) / 4), which = (index - 1) % 4;
    if (lane < 0 || lane >= LANES) return;
    if (which == 0) laneSource[lane] = Math.round(value);
    else if (which == 2) laneMin[lane] = value / 100;
    else if (which == 3) laneMax[lane] = value / 100;
    laneLast[lane] = -1;
}

function moveLanes(source, v) {
    for (var lane = 0; lane < LANES; lane++) {
        if (laneSource[lane] != source) continue;
        var x = laneMin[lane] + (laneMax[lane] - laneMin[lane]) * v;
        if (x == laneLast[lane]) continue;
        laneLast[lane] = x;
        var t = new TargetEvent();
        t.target = laneTarget[lane];
        t.value = x;
        t.send();
    }
}

function HandleMIDI(event) {
    if (!ready) readAll();
    if (event instanceof ControlChange && event.channel == channel) {
        var n = event.number, src = COARSE_OF[n];
        if (src !== undefined) {
            coarse[src] = event.value;              // wait for the fine part
            return;
        }
        src = FINE_OF[n];
        if (src !== undefined) {
            moveLanes(src, (coarse[src] * 128 + event.value) / 16383);
            return;
        }
        src = GATE_OF[n];
        if (src !== undefined) {
            moveLanes(src, event.value >= 64 ? 1 : 0);
            return;
        }
    }
    event.send();
}
