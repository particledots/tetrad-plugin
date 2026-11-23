// Runs the real Scripter mapper script with a stand-in for Logic's Scripter, fed by the engine's real MIDI stream
// (from tetrad_ccdump). Also chains it with copies of the Ogdoad, Strange Drift, Hillside, Bryn and particlehold
// mappers (tests/fixtures), in both orders and all on one MIDI channel, to show all six plugins can share a track.
// Usage: TETRAD_CCDUMP=<path to tetrad_ccdump> node mapper_test.js <Tetrad-Mapper.js> <fixtures folder>

"use strict";
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const { execFileSync } = require("child_process");

let checks = 0, failures = 0;
function expect(ok, what) { checks++; if (!ok) { failures++; console.log("FAIL: " + what); } }

const scriptText = fs.readFileSync(process.argv[2], "utf8");
const fixtures = process.argv[3];
const dumpExe = process.env.TETRAD_CCDUMP;

const SINE_COARSE = [2, 4, 5, 8], SINE_FINE = [34, 36, 37, 40];
const ASGN_COARSE = [66, 68, 70, 72], ASGN_FINE = [67, 69, 71, 73];
const RECT = [91, 92, 93, 94], RECT_NOTE = [49, 50, 51, 52];
const OURS = new Set([...SINE_COARSE, ...SINE_FINE, ...ASGN_COARSE, ...ASGN_FINE, ...RECT]);

function readDump(seconds, mode, rectOut) {
    const lines = execFileSync(dumpExe, [String(seconds), String(mode), String(rectOut)], { maxBuffer: 1 << 28 }).toString().trim().split("\n");
    const d = { events: [], finals: [], rects: [] };
    for (const line of lines) {
        const p = line.split(" ");
        if (p[0] === "final") d.finals[+p[1]] = +p[2];
        else if (p[0] === "rect") d.rects[+p[1]] = +p[2];
        else d.events.push({ sample: +p[0], kind: p[1], number: +p[2], value: +p[3] });
    }
    return d;
}

// ---- A stand-in for Scripter ---------------------------------------------------------------------------------
function makeScripter(text, settings, callParameterChanged, passTo) {
    const out = { passed: [], targets: [] };
    class Event { send() { out.passed.push(this); if (passTo) passTo(this); } trace() {} }
    class ControlChange extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, number: 0, value: 0 }, o); } }
    class NoteOn extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, pitch: 60, velocity: 100 }, o); } }
    class NoteOff extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, pitch: 60, velocity: 0 }, o); } }
    class TargetEvent { constructor() { this.target = ""; this.value = 0; } send() { out.targets.push({ target: this.target, value: this.value }); } }
    const values = {};
    const sandbox = {
        Event, ControlChange, NoteOn, NoteOff, TargetEvent, Math,
        GetParameter: (name) => { if (!(name in values)) throw new Error("GetParameter unknown name " + name); return values[name]; },
        Trace: () => {},
    };
    vm.createContext(sandbox);
    vm.runInContext(text, sandbox);
    const params = vm.runInContext("PluginParameters", sandbox);
    params.forEach(p => { if (p.type !== "target") values[p.name] = p.defaultValue !== undefined ? p.defaultValue : 0; });
    for (const [name, v] of Object.entries(settings)) {
        if (!(name in values)) throw new Error("test uses unknown setting " + name);
        values[name] = v;
        if (callParameterChanged) sandbox.ParameterChanged(params.findIndex(p => p.name === name), v);
    }
    return { sandbox, params, out, values };
}
const near = (a, b) => Math.abs(a - b) < 1e-9;
function feed(s, ev, channel) {
    const { ControlChange, NoteOn, NoteOff } = s.sandbox;
    if (ev.kind === "cc") s.sandbox.HandleMIDI(new ControlChange({ channel, number: ev.number, value: ev.value }));
    else if (ev.value > 0) s.sandbox.HandleMIDI(new NoteOn({ channel, pitch: ev.number, velocity: ev.value }));
    else s.sandbox.HandleMIDI(new NoteOff({ channel, pitch: ev.number, velocity: 0 }));
}
const lane = (s, n) => s.out.targets.filter(t => t.target === "Lane " + n + " target").map(t => t.value);
const last = a => a[a.length - 1];

const free = readDump(60, 0, 0), mult = readDump(60, 3, 0), noteDump = readDump(20, 1, 1);
for (const [d, name] of [[free, "free"], [mult, "multiply"]]) {
    expect(d.events.length > 20000, "the engine produced a MIDI stream in " + name + " mode (" + d.events.length + " messages)");
    expect(d.events.every(e => e.kind === "cc" && OURS.has(e.number)), "the " + name + " stream uses only Tetrad's CC numbers");
    expect(d.finals.length === 8 && d.finals.every(v => v >= 0 && v <= 16383) && d.rects.length === 4, "eight final values and four rects (" + name + ")");
}

// ---- 1. The script itself -----------------------------------------------------------------------------------------
{
    const { params } = makeScripter(scriptText, {}, false);
    const names = params.map(p => p.name);
    expect(params.length === 33, "33 settings (channel + 8 lanes x 4)");
    expect(new Set(names).size === names.length, "setting names are unique");
    expect(params.filter(p => p.type === "target").length === 8, "8 Learn targets");
    expect(params.every(p => ["lin", "menu", "target"].includes(p.type)), "only setting types Scripter knows");
    expect(params[0].name === "Tetrad MIDI channel" && params[0].defaultValue === 12, "channel setting, 12 by default");
    const src = params.find(p => p.name === "Lane 1 source").valueStrings;
    expect(src.join() === "Off,A sine,A asgn,A rect,B sine,B asgn,B rect,C sine,C asgn,C rect,D sine,D asgn,D rect", "Source menu in its fixed order");
    expect(src.every(x => /^[ -~]+$/.test(x)) && /^[\x09\x0a\x0d -~]*$/.test(scriptText), "plain characters only (safe to paste)");
    expect(!/\.trace\(\)/.test(scriptText), "no trace printing in the mapper");
}

// ---- 2. Nothing set up: Tetrad's CC stops, everything else passes ------------------------------------------------------
{
    const s = makeScripter(scriptText, {}, false);
    const { ControlChange, NoteOn, NoteOff } = s.sandbox;
    for (const e of free.events) feed(s, e, 12);
    expect(s.out.passed.length === 0 && s.out.targets.length === 0, "the whole 60 s stream stops at the mapper, no knob moves");
    s.sandbox.HandleMIDI(new NoteOn({ pitch: 64 }));
    s.sandbox.HandleMIDI(new NoteOff({ pitch: 64 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 1, number: 1, value: 90 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 12, number: 7, value: 100 }));
    let others = 0;
    for (let ch = 1; ch <= 16; ch++) {
        if (ch === 12) continue;
        for (const n of OURS) { s.sandbox.HandleMIDI(new ControlChange({ channel: ch, number: n, value: 5 })); others++; }
    }
    expect(s.out.passed.length === 4 + others, "notes, other CC and Tetrad's numbers on other channels pass through (" + s.out.passed.length + ")");
    expect(s.out.passed[0].pitch === 64 && s.out.passed[0] instanceof NoteOn, "notes pass unchanged");
}

// ---- 3. All lanes set up, real streams --------------------------------------------------------------------------------
for (const [d, name] of [[free, "free mode"], [mult, "multiply mode"]])
for (const viaParameterChanged of [true, false]) {
    const how = " (" + name + (viaParameterChanged ? ", settings changed in the panel)" : ", settings restored with a project)");
    const settings = {
        "Lane 1 source": 1,                                           // A sine
        "Lane 2 source": 2, "Lane 2 min %": 20, "Lane 2 max %": 80,   // A asgn, 20..80%
        "Lane 3 source": 3,                                           // A rect
        "Lane 4 source": 4, "Lane 4 min %": 100, "Lane 4 max %": 0,   // B sine upside down
        "Lane 5 source": 8,                                           // C asgn
        "Lane 6 source": 10,                                          // D sine
        "Lane 7 source": 12, "Lane 7 min %": 10, "Lane 7 max %": 60,  // D rect, 10..60%
        "Lane 8 source": 9,                                           // C rect
    };
    const s = makeScripter(scriptText, settings, viaParameterChanged);
    const { NoteOn } = s.sandbox;
    let notesIn = 0;
    const t0 = process.hrtime.bigint();
    d.events.forEach((e, k) => {
        feed(s, e, 12);
        if (k % 500 === 0) { s.sandbox.HandleMIDI(new NoteOn({ pitch: 40 + (k % 30) })); notesIn++; }
    });
    const usPerEvent = Number(process.hrtime.bigint() - t0) / 1000 / d.events.length;
    const L = [null]; for (let n = 1; n <= 8; n++) L.push(lane(s, n));
    expect(L.slice(1).every(a => a.length > 20), "every lane moves its knob" + how);
    expect(near(last(L[1]), d.finals[0] / 16383), "A sine arrives exactly" + how);
    expect(near(last(L[2]), 0.2 + 0.6 * d.finals[4] / 16383) && L[2].every(v => v >= 0.2 - 1e-12 && v <= 0.8 + 1e-12), "A asgn scaled to 20..80%" + how);
    expect(L[3].every(v => v === 0 || v === 1) && last(L[3]) === d.rects[0] && L[3].length > 50, "A rect jumps between Min and Max" + how);
    expect(near(last(L[4]), 1 - d.finals[1] / 16383), "B sine with max below min turns upside down" + how);
    expect(near(last(L[5]), d.finals[6] / 16383), "C asgn arrives exactly" + how);
    expect(near(last(L[6]), d.finals[3] / 16383), "D sine arrives exactly" + how);
    expect(L[7].every(v => near(v, 0.1) || near(v, 0.6)) && near(last(L[7]), d.rects[3] ? 0.6 : 0.1), "D rect between 10% and 60%" + how);
    expect(near(last(L[8]), d.rects[2]), "C rect arrives" + how);
    expect(L.slice(1).every(a => a.every(v => v >= 0 && v <= 1)), "targets always 0..1" + how);
    const distinct = new Set(L[5].map(v => Math.round(v * 16383))).size;
    expect(distinct > 2000, "14-bit detail reaches the target (" + distinct + " distinct values)" + how);
    expect(L[1].filter(v => Math.round(v * 16383) % 128 !== 0).length > L[1].length / 2, "values arrive in 14-bit steps, not 128 coarse steps" + how);
    expect(Math.max(...L[1]) > 0.99 && Math.min(...L[1]) < 0.01, "A sine sweeps the whole range" + how);
    expect(s.out.passed.length === notesIn, "notes still pass with lanes in use" + how);
    console.log("mapper: " + usPerEvent.toFixed(2) + " microseconds per CC message in Node" + how);
}

// ---- 4. A different channel ------------------------------------------------------------------------------------------
{
    const s = makeScripter(scriptText, { "Tetrad MIDI channel": 3, "Lane 1 source": 1 }, true);
    for (const e of free.events.slice(0, 400)) feed(s, e, 12);
    expect(s.out.targets.length === 0 && s.out.passed.length === 400, "channel 3 set: channel 12 CC is left alone");
    for (const e of free.events.slice(0, 400)) feed(s, e, 3);
    expect(s.out.targets.length > 10 && s.out.passed.length === 400, "channel 3 set: channel 3 CC is mapped and stopped");
}

// ---- 5. Rect as notes: the notes pass through to the instrument ---------------------------------------------------------
{
    const s = makeScripter(scriptText, { "Lane 1 source": 3 }, true);
    for (const e of noteDump.events) feed(s, e, 12);
    const notes = s.out.passed.filter(e => e instanceof s.sandbox.NoteOn || e instanceof s.sandbox.NoteOff);
    expect(noteDump.events.some(e => e.kind === "note") && !noteDump.events.some(e => e.kind === "cc" && RECT.includes(e.number)),
        "with Rect out on Note, rect is sent as notes and not as CC");
    expect(notes.length === noteDump.events.filter(e => e.kind === "note").length && notes.every(n => RECT_NOTE.includes(n.pitch)),
        "rect notes 49-52 pass through the mapper to the instrument (" + notes.length + ")");
    expect(s.out.passed.length === notes.length, "only the notes pass");
}

// ---- 6. Sharing a track with Ogdoad, Strange Drift, Hillside, Bryn and particlehold ---------------------------------------
{
    const others = [
        { name: "Ogdoad",        file: "Ogdoad-Mapper.js",       channel: 16, pair: [20, 52], extra: [12, 44, 28, 60], gates: [102], notes: [36, 39] },
        { name: "Strange Drift", file: "StrangeDrift-Mapper.js", channel: 15, pair: [106, 113], extra: [112, 119], gates: [], notes: [] },
        { name: "Hillside",      file: "Hillside-Mapper.js",     channel: 16, pair: [3, 35], extra: [9, 41], gates: [85, 89], notes: [40, 44] },
        { name: "Bryn",          file: "Bryn-Mapper.js",         channel: 14, pair: [80, 81], extra: [], gates: [82, 90], notes: [45, 47] },
        { name: "particlehold",  file: "particlehold-Mapper.js", channel: 13, pair: [75, 76], extra: [77, 78], gates: [79], notes: [48] },
    ];
    for (const o of others) o.text = fs.readFileSync(path.join(fixtures, o.file), "utf8");

    // A mixed stream: Tetrad's real stream on its channel, and for each other plugin a climbing 14-bit value on its
    // lane 1 pair, its other pairs, gates and trigger notes.
    function mixedStream(oneChannel) {
        const mixed = [], lastOf = {};
        let k = 0;
        for (const e of free.events) {
            mixed.push({ kind: e.kind, channel: oneChannel || 12, number: e.number, value: e.value });
            if (k++ % 4 !== 0) continue;
            for (const o of others) {
                const ch = oneChannel || o.channel;
                const v = (k * (37 + o.pair[0])) % 16384;
                mixed.push({ kind: "cc", channel: ch, number: o.pair[0], value: v >> 7 }, { kind: "cc", channel: ch, number: o.pair[1], value: v & 127 });
                lastOf[o.name] = v / 16383;
                for (let x = 0; x + 1 < o.extra.length; x += 2)
                    mixed.push({ kind: "cc", channel: ch, number: o.extra[x], value: (v >> 7) ^ 64 }, { kind: "cc", channel: ch, number: o.extra[x + 1], value: v & 127 });
                if (k % 400 === 1) for (const g of o.gates) mixed.push({ kind: "cc", channel: ch, number: g, value: 127 }, { kind: "cc", channel: ch, number: g, value: 0 });
                if (k % 800 === 1) for (const n of o.notes) mixed.push({ kind: "on", channel: ch, number: n, value: 100 }, { kind: "off", channel: ch, number: n, value: 0 });
            }
        }
        return { mixed, lastOf };
    }
    const carry = (to, ev) => new to.sandbox[ev.constructor.name](Object.assign({}, ev));

    for (const variant of ["Tetrad Mapper last", "Tetrad Mapper first", "all six on MIDI channel 1"]) {
        const oneChannel = variant.startsWith("all") ? 1 : 0;
        const { mixed, lastOf } = mixedStream(oneChannel);
        const instrument = [];
        const chainNames = variant === "Tetrad Mapper first" ? ["Tetrad", ...others.map(o => o.name)] : [...others.map(o => o.name), "Tetrad"];
        const mappers = {};
        let nextMapper = null;              // the mapper below the one being made (null: the instrument)
        for (let i = chainNames.length - 1; i >= 0; i--) {
            const nm = chainNames[i];
            const settings = { "Lane 1 source": 1 };
            if (oneChannel) settings[nm + " MIDI channel"] = 1;
            const text = nm === "Tetrad" ? scriptText : others.find(o => o.name === nm).text;
            const below = nextMapper;
            const passTo = below ? (ev => below.sandbox.HandleMIDI(carry(below, ev))) : (ev => instrument.push(ev));
            mappers[nm] = makeScripter(text, settings, true, passTo);
            nextMapper = mappers[nm];
        }
        const first = mappers[chainNames[0]];
        let notesIn = 0;
        for (const m of mixed) {
            if (m.kind === "cc") first.sandbox.HandleMIDI(new first.sandbox.ControlChange({ channel: m.channel, number: m.number, value: m.value }));
            else if (m.kind === "on" || (m.kind === "note" && m.value > 0)) { first.sandbox.HandleMIDI(new first.sandbox.NoteOn({ channel: m.channel, pitch: m.number, velocity: m.value })); notesIn++; }
            else { first.sandbox.HandleMIDI(new first.sandbox.NoteOff({ channel: m.channel, pitch: m.number })); notesIn++; }
        }
        first.sandbox.HandleMIDI(new first.sandbox.NoteOn({ pitch: 60 })); notesIn++;
        const tet = lane(mappers["Tetrad"], 1);
        expect(tet.length > 100 && near(last(tet), free.finals[0] / 16383), "Tetrad's lane follows A sine exactly (" + variant + ")");
        for (const o of others) {
            const l = lane(mappers[o.name], 1);
            expect(l.length > 100 && near(last(l), lastOf[o.name]), o.name + "'s lane follows its own plugin (" + variant + ")");
        }
        const ccLeft = instrument.filter(e => e instanceof Object && "number" in e && !("pitch" in e));
        expect(ccLeft.length === 0, "no plugin's CC reaches the instrument: each stops at its own mapper (" + variant + ", " + ccLeft.length + " left)");
        expect(instrument.length === notesIn, "every note reaches the instrument (" + variant + ")");
    }
}

if (failures === 0) console.log("mapper tests: " + checks + " checks passed");
else console.log("mapper tests: " + failures + " of " + checks + " checks FAILED");
process.exit(failures === 0 ? 0 : 1);
