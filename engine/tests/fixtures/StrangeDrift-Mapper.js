// Strange Drift Mapper, for Logic's Scripter (MIDI FX). Version 0.1.0.
//
// Strange Drift sends its seven outputs as 14-bit MIDI CC on MIDI channel 15 (unless you changed it):
//   Ripple X  CC 106 (coarse) + 113 (fine)      Tide X     CC 108 + 115      Glacier X  CC 110 + 117
//   Ripple Y  CC 107 + 114                      Tide Y     CC 109 + 116      Glacier Y  CC 111 + 118
//   Blend     CC 112 + 119
// Z (Y upside down) and Blend + / Blend - are worked out here, from Y and Blend.
// This script turns them into movements of knobs on any plugin BELOW it on the same channel strip (the instrument, or
// an audio effect).
//
// Setup, once per Mac (and again after each new version of this script):
//   Add Scripter in the MIDI FX slot just BELOW Strange Drift, click "Open Script in Editor", delete everything,
//   paste this whole file, click "Run Script", close the editor, and in Scripter's preset menu choose
//   "Save As..." and call it "Strange Drift Mapper". From then on it is in Scripter's preset menu.
// Per track:
//   Strange Drift in a MIDI FX slot, Scripter with the "Strange Drift Mapper" preset in the slot below it.
//   For each lane you want: pick a Source, open its Target menu, choose "Learn plug-in parameter" and move the knob
//   you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it upside down).
//   Blend + rests at Min and rises to Max as the blend goes above zero; Blend - does the same below zero.
//   Need more than 8? Add a second Scripter with the same preset below the first.
//
// Strange Drift's CC messages stop here, so they never reach the instrument. Everything else passes through
// unchanged, including Ogdoad's CC on channel 16 (so the Ogdoad Mapper can sit on the same track).

var PluginParameters = [
    { name: "Strange Drift MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 15 }
];

var LANES = 8;
// This order is fixed: saved lanes keep their source.
var SOURCES = ["Off",
               "Ripple X", "Ripple Y", "Ripple Z",
               "Tide X", "Tide Y", "Tide Z",
               "Glacier X", "Glacier Y", "Glacier Z",
               "Blend +", "Blend -", "Blend +/-"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

var FIRST_COARSE = 106, FIRST_FINE = 113, OUTPUTS = 7, BLEND = 6;

// For each of the seven outputs: the sources that follow it, and how (0 = as sent, 1 = upside down,
// 2 = above zero only, 3 = below zero only).
var FOLLOW = [
    [[1, 0]],               // Ripple X
    [[2, 0], [3, 1]],       // Ripple Y, Ripple Z
    [[4, 0]],               // Tide X
    [[5, 0], [6, 1]],       // Tide Y, Tide Z
    [[7, 0]],               // Glacier X
    [[8, 0], [9, 1]],       // Glacier Y, Glacier Z
    [[10, 2], [11, 3], [12, 0]]   // Blend +, Blend -, Blend +/-
];

var channel = 15;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [];
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}
for (var j = 0; j < OUTPUTS; j++) coarse.push(64);

function readAll() {
    channel = Math.round(GetParameter("Strange Drift MIDI channel"));
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

function arrived(output, raw) {             // raw: 0..16383; 8192 is the centre (zero)
    var list = FOLLOW[output];
    for (var k = 0; k < list.length; k++) {
        var how = list[k][1], v;
        if (how == 0) v = raw / 16383;
        else if (how == 1) v = 1 - raw / 16383;
        else if (how == 2) v = raw > 8192 ? (raw - 8192) / 8191 : 0;
        else v = raw < 8192 ? (8192 - raw) / 8192 : 0;
        moveLanes(list[k][0], v);
    }
}

function HandleMIDI(event) {
    if (!ready) readAll();
    if (event instanceof ControlChange && event.channel == channel) {
        var n = event.number;
        if (n >= FIRST_COARSE && n < FIRST_COARSE + OUTPUTS) {
            coarse[n - FIRST_COARSE] = event.value;     // wait for the fine part
            return;
        }
        if (n >= FIRST_FINE && n < FIRST_FINE + OUTPUTS) {
            var o = n - FIRST_FINE;
            arrived(o, coarse[o] * 128 + event.value);
            return;
        }
    }
    event.send();
}
