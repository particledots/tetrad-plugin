// Hillside Mapper, for Logic's Scripter (MIDI FX). Version 0.1.0.
//
// Hillside sends its envelope as 14-bit MIDI CC on MIDI channel 16 (unless you changed it):
//   Envelope (follows Shape)  CC 3 (coarse) + CC 35 (fine)
//   Linear (always straight)  CC 9 (coarse) + CC 41 (fine)
//   Gate A, D, S, R           CC 85, 86, 87, 88   (127 while the stage runs, then 0)       from Hillside 0.2.0
//   Trig                      CC 89               (127 for 20 ms at each stage's start)   from Hillside 0.2.0
// This script turns them into movements of knobs on any plugin BELOW it on the same channel strip (the instrument, or
// an audio effect).
//
// Setup, once per Mac (and again after each new version of this script):
//   Add Scripter in a MIDI FX slot below Hillside, click "Open Script in Editor", delete everything, paste this whole
//   file, click "Run Script", close the editor, and in Scripter's preset menu choose "Save As..." and call it
//   "Hillside Mapper". From then on it is in Scripter's preset menu.
// Per track:
//   Hillside in a MIDI FX slot, Scripter with the "Hillside Mapper" preset in a slot below it.
//   For each lane you want: pick a Source, open its Target menu, choose "Learn plug-in parameter" and move the knob
//   you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it upside down).
//   A Gate or Trig source jumps the knob from Min to Max while the gate is high. Need more than 8 lanes? Add a second
//   Scripter with the same preset below the first.
//
// Hillside's CC messages stop here, so they never reach the instrument. Everything else passes through unchanged:
// your notes, Ogdoad's CC (channel 16) and Strange Drift's CC (channel 15), so their mappers can sit on the same track.
// With all three: Hillside, Ogdoad, Strange Drift, Ogdoad Mapper, Strange Drift Mapper, Hillside Mapper.

var PluginParameters = [
    { name: "Hillside MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 16 }
];

var LANES = 8;
// This order is fixed: saved lanes keep their source.
var SOURCES = ["Off", "Envelope", "Linear", "Gate A", "Gate D", "Gate S", "Gate R", "Trig"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

// Hillside's CC numbers: which Source each belongs to.
var COARSE_OF = [];   // coarse CC number -> source
var FINE_OF = [];     // fine CC number -> source
var GATE_OF = [];     // on/off CC number -> source
COARSE_OF[3] = 1; FINE_OF[35] = 1;     // Envelope
COARSE_OF[9] = 2; FINE_OF[41] = 2;     // Linear
GATE_OF[85] = 3; GATE_OF[86] = 4; GATE_OF[87] = 5; GATE_OF[88] = 6;   // Gate A, D, S, R
GATE_OF[89] = 7;                       // Trig

var channel = 16;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [0, 0, 0];               // the latest coarse part of Envelope and Linear (index = source)
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}

function readAll() {
    channel = Math.round(GetParameter("Hillside MIDI channel"));
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
