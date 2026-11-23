// Tetrad Mapper, for Logic's Scripter (MIDI FX). Version 0.1.0.
//
// Tetrad sends its twelve outputs as MIDI CC on MIDI channel 12 (unless you changed it):
//   A sine  CC 2 (coarse) + 34 (fine)    A asgn  CC 66 + 67    A rect  CC 91   (127 while high, 0 while low)
//   B sine  CC 4 + 36                    B asgn  CC 68 + 69    B rect  CC 92
//   C sine  CC 5 + 37                    C asgn  CC 70 + 71    C rect  CC 93
//   D sine  CC 8 + 40                    D asgn  CC 72 + 73    D rect  CC 94
// (Rect goes out as CC when Tetrad's "rect out" is CC or Both. With Note, rect plays notes 49-52 instead, which pass
// through this script to the instrument.)
// This script turns them into movements of knobs on any plugin BELOW it on the same channel strip (the instrument, or
// an audio effect).
//
// Setup, once per Mac (and again after each new version of this script):
//   Add Scripter in a MIDI FX slot below Tetrad, click "Open Script in Editor", delete everything, paste this whole
//   file, click "Run Script", close the editor, and in Scripter's preset menu choose "Save As..." and call it
//   "Tetrad Mapper". From then on it is in Scripter's preset menu.
// Per track:
//   Tetrad in a MIDI FX slot, Scripter with the "Tetrad Mapper" preset in a slot below it.
//   For each lane you want: pick a Source, open its Target menu, choose "Learn plug-in parameter" and move the knob
//   you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it upside down).
//   A rect source jumps the knob between Min (low) and Max (high). Need more than 8 lanes? Add a second Scripter with
//   the same preset below the first.
//
// Tetrad's CC messages stop here, so they never reach the instrument. Everything else passes through unchanged: your
// notes, Tetrad's rect notes, and the CC of Ogdoad, Strange Drift, Hillside, Bryn and particlehold, so their mappers
// can sit on the same track. With Ogdoad: Ogdoad, Tetrad, Ogdoad Mapper, Tetrad Mapper.

var PluginParameters = [
    { name: "Tetrad MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 12 }
];

var LANES = 8;
// This order is fixed: saved lanes keep their source.
var SOURCES = ["Off",
               "A sine", "A asgn", "A rect",
               "B sine", "B asgn", "B rect",
               "C sine", "C asgn", "C rect",
               "D sine", "D asgn", "D rect"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

// For every CC number Tetrad uses: the Source it belongs to (index into SOURCES), and how it is sent.
var COARSE_OF = [], FINE_OF = [], GATE_OF = [];
(function () {
    var sineCoarse = [2, 4, 5, 8], sineFine = [34, 36, 37, 40];
    var asgnCoarse = [66, 68, 70, 72], asgnFine = [67, 69, 71, 73];
    var rect = [91, 92, 93, 94];
    for (var ch = 0; ch < 4; ch++) {
        COARSE_OF[sineCoarse[ch]] = 1 + 3 * ch; FINE_OF[sineFine[ch]] = 1 + 3 * ch;
        COARSE_OF[asgnCoarse[ch]] = 2 + 3 * ch; FINE_OF[asgnFine[ch]] = 2 + 3 * ch;
        GATE_OF[rect[ch]] = 3 + 3 * ch;
    }
})();

var channel = 12;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [];
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}
for (var j = 0; j < SOURCES.length; j++) coarse.push(64);

function readAll() {
    channel = Math.round(GetParameter("Tetrad MIDI channel"));
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
