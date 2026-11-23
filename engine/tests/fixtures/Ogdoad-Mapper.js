// Ogdoad Mapper, for Logic's Scripter (MIDI FX). Version 0.2.0.
//
// Ogdoad sends its eight LFOs and the sixteen Ogdoad Ex outputs as MIDI CC on MIDI channel 16 (unless you changed it):
//   LFO 1 to 8        CC 20-27 (coarse) + 52-59 (fine)
//   Fold 1, 3, 5, 7   CC 12-15 + 44-47
//   Max / Min         CC 16-19 + 48-51   (Max 2.3, Min 2.3, Max 6.7, Min 6.7)
//   Steps             CC 28-31 + 60-63   (1-4, 5-8, odd, even)
//   Trig 2, 4, 6, 8   CC 102-105         (127 for 20 ms, then 0)
// This script turns them into movements of knobs on any plugin BELOW it on the same channel strip (the instrument, or
// an audio effect).
//
// Setup, once per Mac (and again after each new version of this script):
//   Add Scripter in the MIDI FX slot just BELOW Ogdoad, click "Open Script in Editor", delete everything,
//   paste this whole file, click "Run Script", close the editor, and in Scripter's preset menu choose
//   "Save As..." and call it "Ogdoad Mapper" (replace the old one). From then on it is in Scripter's preset menu.
// Per track:
//   Ogdoad in MIDI FX slot 1, Scripter with the "Ogdoad Mapper" preset in slot 2.
//   For each lane you want: pick a Source, open its Target menu, choose "Learn plug-in parameter" and move the knob
//   you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it upside down).
//   A Trig source jumps the knob from Min to Max for 20 ms. Need more than 8? Add a second Scripter with the same
//   preset below the first.
//
// Ogdoad's CC messages stop here, so they never reach the instrument. Everything else passes through unchanged,
// including the trigger notes when Ogdoad's "Trig out" is set to Note or Both.

var PluginParameters = [
    { name: "Ogdoad MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 16 }
];

var LANES = 8;
// The first nine entries are those of version 0.1.0, in the same places, so saved lanes keep their source.
var SOURCES = ["Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8",
               "Fold 1", "Fold 3", "Fold 5", "Fold 7",
               "Max 2+3", "Min 2+3", "Max 6+7", "Min 6+7",
               "Trig 2", "Trig 4", "Trig 6", "Trig 8",
               "Steps 1-4", "Steps 5-8", "Steps odd", "Steps even"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

// For every CC number Ogdoad uses: which Source it belongs to (index into SOURCES), and how it is sent.
var FINE_SOURCE = [];       // fine CC number -> source (14-bit pairs; the coarse part is the fine number - 32)
var PULSE_SOURCE = [];      // 7-bit trigger CC number -> source
var COARSE = [];            // coarse CC numbers in use
(function () {
    function pairs(firstCoarse, firstSource, count) {
        for (var k = 0; k < count; k++) { COARSE[firstCoarse + k] = true; FINE_SOURCE[firstCoarse + 32 + k] = firstSource + k; }
    }
    pairs(20, 1, 8);        // LFO 1-8
    pairs(12, 9, 4);        // Fold
    pairs(16, 13, 4);       // Max / Min
    pairs(28, 21, 4);       // Steps
    for (var k = 0; k < 4; k++) PULSE_SOURCE[102 + k] = 17 + k;     // Trig
})();

var channel = 16;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [];
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}
for (var j = 0; j < 128; j++) coarse.push(0);

function readAll() {
    channel = Math.round(GetParameter("Ogdoad MIDI channel"));
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
        var n = event.number;
        if (COARSE[n] === true) {
            coarse[n] = event.value;                    // wait for the fine part
            return;
        }
        var src = FINE_SOURCE[n];
        if (src !== undefined) {
            moveLanes(src, (coarse[n - 32] * 128 + event.value) / 16383);
            return;
        }
        src = PULSE_SOURCE[n];
        if (src !== undefined) {
            moveLanes(src, event.value >= 64 ? 1 : 0);
            return;
        }
    }
    event.send();
}
