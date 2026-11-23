// Bryn Mapper, for Logic's Scripter (MIDI FX). Version 0.1.0.
//
// Bryn sends its function as 14-bit MIDI CC on MIDI channel 14 (unless you changed it):
//   Function    CC 80 (coarse) + CC 81 (fine)
//   Rise gate   CC 82   (127 while the function rises, then 0)
//   Fall gate   CC 83   (127 while the function falls, then 0)
//   EOC         CC 90   (127 for 20 ms at the end of every cycle)
// This script turns them into movements of knobs on any plugin BELOW it on the same channel strip (the instrument, or
// an audio effect).
//
// Setup, once per Mac (and again after each new version of this script):
//   Add Scripter in a MIDI FX slot below Bryn, click "Open Script in Editor", delete everything, paste this whole
//   file, click "Run Script", close the editor, and in Scripter's preset menu choose "Save As..." and call it
//   "Bryn Mapper". From then on it is in Scripter's preset menu.
// Per track:
//   Bryn in a MIDI FX slot, Scripter with the "Bryn Mapper" preset in a slot below it.
//   For each lane you want: pick a Source, open its Target menu, choose "Learn plug-in parameter" and move the knob
//   you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it upside down; Min 30
//   and Max 70 swings the knob around its middle, like the module's bipolar output).
//   A gate or EOC source jumps the knob from Min to Max while it is high. Need more than 8 lanes? Add a second
//   Scripter with the same preset below the first.
//
// Bryn's CC messages stop here, so they never reach the instrument. Everything else passes through unchanged:
// your notes (and Bryn's gate notes), and Hillside's, Ogdoad's and Strange Drift's CC, so their mappers can sit on the
// same track. With all four: Bryn, Hillside, Ogdoad, Strange Drift, then their mappers.

var PluginParameters = [
    { name: "Bryn MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 14 }
];

var LANES = 8;
// This order is fixed: saved lanes keep their source.
var SOURCES = ["Off", "Function", "Rise gate", "Fall gate", "EOC"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

// Bryn's CC numbers: which Source each belongs to.
var COARSE_OF = [];   // coarse CC number -> source
var FINE_OF = [];     // fine CC number -> source
var GATE_OF = [];     // on/off CC number -> source
COARSE_OF[80] = 1; FINE_OF[81] = 1;   // Function
GATE_OF[82] = 2; GATE_OF[83] = 3;     // Rise gate, Fall gate
GATE_OF[90] = 4;                      // EOC

var channel = 14;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [0, 0];                  // the latest coarse part of the Function (index = source)
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}

function readAll() {
    channel = Math.round(GetParameter("Bryn MIDI channel"));
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
