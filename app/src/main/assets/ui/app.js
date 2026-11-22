// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android – the panel. Everything plays in the native engine (window.GB, MainActivity.Bridge); this page
// draws the seven pages, sends the commands and shows the engine's state (polled ~20 times a second).
'use strict';

// ---- the commands (the same numbers as gbox::Op in cpp/box/BoxEngine.hpp)
const OP = {
  PLAY: 1, STOP: 2, NEWALL: 3, LOCK: 4, SOUND: 5, SOUNDMODE: 6,
  BPM: 10, BPM_GENRE: 11, KEY: 12, COLOR: 13, SWING: 14, LENGTH: 15, MUTATE: 16, MASTER: 17, DUCK: 18,
  GENRE: 20, TGENRE: 21, DENS: 22, TK2: 23, TNEW: 24, HOLD: 25, PRESET: 26, MACRO: 27, LEVEL: 28, MUTE: 29,
  SYN: 30, SYN_INIT: 31, SYN_UNDO: 32, SYN_AB: 33, SYN_TEST: 34, SYN_MARK: 35,
  DX_SND: 40, DX_KIT: 41, DX_LANEMUTE: 42,
  AB_PART: 50, AB_PAD: 51, AB_UNDO: 52, AB_CLEAR: 53, AB_MODE: 54, AB_SONG: 55, AB_NEXT: 56,
  PRESET_DELETE: 61,
};
// DRUM-X core parameters (drumcore.h)
const JDX = { MORPH: 0, TUNE: 1, TONE: 2, DECAY: 3, NOISE: 4, FMENV: 5, FOLD: 6, DRIVE: 7, LN0: 8, DIST: 16, DLY: 17, REV: 18, LEVEL: 19 };

const C = {
  bg: '#17181d', plate: '#212329', line: '#34363f', txt: '#d9dbe2', dim: '#8a8d99', disp: '#07080a',
  green: '#45e07a', yellow: '#ffc93c', red: '#ff5d6c', white: '#f0f1f4',
};
const TC = ['#7b6cff', '#b8f23a', '#3dc9ff', '#e0779e'];            // drums, bass, melody, pads
const TNAME = ['DRUM-X', 'GENRA MONO', 'FUNKE', 'SCHMELZ'];
const TK2 = ['FILL', 'MOVE', 'RANGE', 'VOICING'];
const TSPUR = ['DRUMS', 'BASS', 'MELODY', 'PADS'];
const MACROS = [['TUNE', 'DECAY'], ['CUTOFF', 'RESO'], ['BRIGHT', 'SPACE'], ['MOTION', 'SPACE']];
const SEC_COLOR = ['#3dc9ff', '#7b6cff', '#ffc93c', '#ff5d6c', '#45e07a', '#e0779e'];  // INTRO VERSE BUILD DROP BREAK OUTRO
const REG_NAMES = ['BASS', 'CELLO', 'VIOLA', 'VIOLIN', 'BRASS', 'ORGAN', 'CHOIR', 'SYNTH'];
const SOUND_NAMES = ['BASS', 'E-PIANO', 'CLAVI', 'SYNTH', 'BELL'];

const stage = document.getElementById('stage');
let meta = null, st = null, fst = null;
let page = 'BOX';
const pages = {};

// ================================================================ the bridge
function call(name, ...a) {
  try { return window.GB[name](...a); } catch (e) { console.log('GB.' + name + ' failed: ' + e); return null; }
}
function cmd(op, a = 0, b = 0, f = 0) { call('cmd', op, a | 0, b | 0, +f || 0); }

// ================================================================ scaling (the stage fills the screen, letterboxed)
function fit() {
  const w = window.innerWidth, h = window.innerHeight;
  const s = Math.min(w / 2400, h / 1080);
  stage.style.transform = `scale(${s})`;
  stage.style.left = Math.round((w - 2400 * s) / 2) + 'px';
  stage.style.top = Math.round((h - 1080 * s) / 2) + 'px';
  stage._scale = s;
}
window.addEventListener('resize', fit);

// ================================================================ small helpers
function el(tag, cls, parent, css) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (css) Object.assign(e.style, css);
  (parent || stage).appendChild(e);
  return e;
}
function px(v) { return Math.round(v) + 'px'; }
function plate(parent, x0, y0, x1, y1) {
  return el('div', 'plate', parent, { left: px(x0), top: px(y0), width: px(x1 - x0), height: px(y1 - y0) });
}
function text(parent, x, y, s, size, color, opts = {}) {
  const e = el('div', opts.center ? 'tc' : 't', parent, {
    left: px(x), top: px(y), fontSize: px(size), color: color || C.txt,
    fontWeight: opts.weight || 700, letterSpacing: px(opts.sp != null ? opts.sp : 3),
  });
  e.textContent = s;
  return e;
}
function cap(parent, x, y, s) { return text(parent, x, y, s, 18, C.dim, { weight: 600, sp: 4 }); }
function clamp(x, a, b) { return x < a ? a : x > b ? b : x; }
function pct(v) { return Math.round(v * 100) + ' %'; }
function bip(v) { const p = Math.round(v * 100); return (p > 0 ? '+' : '') + p + ' %'; }
function upper(s) { return (s || '').toUpperCase(); }
function chevron(color, w = 24) {
  const ns = 'http://www.w3.org/2000/svg';
  const s = document.createElementNS(ns, 'svg');
  s.setAttribute('width', w); s.setAttribute('height', w * 0.6); s.setAttribute('viewBox', '0 0 24 14');
  const p = document.createElementNS(ns, 'path');
  p.setAttribute('d', 'M2 2 L12 12 L22 2'); p.setAttribute('fill', 'none'); p.setAttribute('stroke', color || C.txt);
  p.setAttribute('stroke-width', '3.5'); p.setAttribute('stroke-linecap', 'round'); p.setAttribute('stroke-linejoin', 'round');
  s.appendChild(p);
  return s;
}
/** Tap with a press look; works with mouse and touch. */
function onTap(e, fn) {
  e.addEventListener('pointerdown', ev => { ev.preventDefault(); e.classList.add('down'); });
  const up = () => e.classList.remove('down');
  e.addEventListener('pointerup', ev => { if (e.classList.contains('down')) { up(); fn(ev); } });
  e.addEventListener('pointercancel', up);
  e.addEventListener('pointerleave', up);
}

// ---- the readout while a knob turns, the toast for the engine's messages
const readout = el('div', null, stage); readout.id = 'readout';
const toast = el('div', null, stage); toast.id = 'toast';
let toastTimer = 0;
function say(s, ms = 2600) {
  if (!s) return;
  toast.textContent = s;
  toast.style.display = 'block';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toast.style.display = 'none'; }, ms);
}

// ================================================================ knob
const ns = 'http://www.w3.org/2000/svg';
class Knob {
  /** o: min, max, def, steps (0 continuous), color, big, fmt(v), change(v), start(), label */
  constructor(parent, cx, cy, r, label, o) {
    this.o = Object.assign({ min: 0, max: 1, def: 0.5, steps: 0, fmt: pct }, o);
    this.r = r; this.label = label; this.v = this.o.def; this.drag = false;
    const pad = r + 16;
    const e = this.e = el('div', 'knob', parent, { left: px(cx - pad), top: px(cy - pad), width: px(pad * 2), height: px(pad * 2) });
    const s = this.svg = document.createElementNS(ns, 'svg');
    s.setAttribute('width', pad * 2); s.setAttribute('height', pad * 2);
    e.appendChild(s);
    this.dots = [];
    const n = r > 45 ? 34 : 28;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2;
      const d = document.createElementNS(ns, 'circle');
      d.setAttribute('cx', pad + (r + 9) * Math.cos(a)); d.setAttribute('cy', pad + (r + 9) * Math.sin(a));
      d.setAttribute('r', r > 45 ? 3 : 2.4);
      s.appendChild(d);
      // the angle of the dot on the knob's travel (-225° .. 45°), for the lit part
      let deg = a * 180 / Math.PI; if (deg > 45) deg -= 360;
      this.dots.push({ d, t: (deg + 225) / 270 });
    }
    const body = document.createElementNS(ns, 'circle');
    body.setAttribute('cx', pad); body.setAttribute('cy', pad); body.setAttribute('r', r);
    body.setAttribute('fill', '#0f1013'); body.setAttribute('stroke', '#2b2d35'); body.setAttribute('stroke-width', 3);
    s.appendChild(body);
    this.ptr = document.createElementNS(ns, 'line');
    this.ptr.setAttribute('stroke', '#f2f2f2'); this.ptr.setAttribute('stroke-width', r > 45 ? 7 : 6);
    this.ptr.setAttribute('stroke-linecap', 'round');
    s.appendChild(this.ptr);
    this.pad = pad;
    if (label) {
      const l = el('div', 'lab', e, { top: px(pad * 2 + 2), fontSize: px(r > 45 ? 22 : 20) });
      l.textContent = label;
      this.labEl = l;
    }
    this.bind();
    this.draw();
  }
  norm(v) { const o = this.o; return o.max > o.min ? (v - o.min) / (o.max - o.min) : 0; }
  draw() {
    const nv = clamp(this.norm(this.v), 0, 1), r = this.r, p = this.pad;
    const a = (-225 + 270 * nv) * Math.PI / 180;
    this.ptr.setAttribute('x1', p + r * 0.22 * Math.cos(a)); this.ptr.setAttribute('y1', p + r * 0.22 * Math.sin(a));
    this.ptr.setAttribute('x2', p + r * 0.8 * Math.cos(a)); this.ptr.setAttribute('y2', p + r * 0.8 * Math.sin(a));
    const col = this.o.color;
    for (const { d, t } of this.dots) {
      const inTravel = t >= -0.01 && t <= 1.01;
      const lit = inTravel && t <= nv + 0.001;
      d.setAttribute('fill', col ? (lit ? col : col + '55') : (lit ? '#7d808c' : '#3a3d47'));
    }
  }
  set(v) { if (!this.drag && v != null && isFinite(v)) { this.v = v; this.draw(); } }
  show() {
    readout.textContent = (this.o.title || this.label || '') + '   ' + this.o.fmt(this.v);
    readout.style.display = 'block';
  }
  bind() {
    let sx = 0, sy = 0, sv = 0, lastTap = 0, lastSend = 0, pending = false, id = -1;
    const send = force => {
      const now = performance.now();
      if (force || now - lastSend > 30) { lastSend = now; pending = false; this.o.change && this.o.change(this.v); }
      else pending = true;
    };
    this.e.addEventListener('pointerdown', ev => {
      ev.preventDefault();
      const now = performance.now();
      if (now - lastTap < 320) {  // double tap: the default
        lastTap = 0;
        this.o.start && this.o.start();
        this.v = this.o.def; this.draw(); send(true); this.show();
        setTimeout(() => { if (!this.drag) readout.style.display = 'none'; }, 700);
        return;
      }
      lastTap = now;
      id = ev.pointerId;
      try { this.e.setPointerCapture(id); } catch (e) {}
      this.drag = true; sx = ev.clientX; sy = ev.clientY; sv = this.norm(this.v);
      this.o.start && this.o.start();
      this.show();
    });
    this.e.addEventListener('pointermove', ev => {
      if (!this.drag || ev.pointerId !== id) return;
      const s = stage._scale || 1;
      const d = ((sy - ev.clientY) + (ev.clientX - sx)) / s;
      let n = clamp(sv + d / (this.r > 45 ? 380 : 320), 0, 1);
      if (this.o.steps > 1) n = Math.round(n * (this.o.steps - 1)) / (this.o.steps - 1);
      const v = this.o.min + n * (this.o.max - this.o.min);
      if (v !== this.v) { this.v = v; this.draw(); send(false); this.show(); }
    });
    const end = ev => {
      if (!this.drag || ev.pointerId !== id) return;
      this.drag = false;
      if (pending) send(true);
      setTimeout(() => { if (!this.drag) readout.style.display = 'none'; }, 350);
    };
    this.e.addEventListener('pointerup', end);
    this.e.addEventListener('pointercancel', end);
  }
}

// ================================================================ round button
class Btn {
  constructor(parent, cx, cy, r, label, o = {}) {
    this.o = o;
    const e = this.e = el('div', 'btn', parent, { left: px(cx - r), top: px(cy - r), width: px(r * 2), height: px(r * 2 + 48) });
    this.cap = el('div', 'cap0', e, { left: '0px', top: '0px', width: px(r * 2), height: px(r * 2) });
    if (o.ring) this.cap.style.border = `8px solid ${o.ring}`;
    this.lab = el('div', 'lab', e, { top: px(r * 2 + 14), color: o.labColor || C.txt });
    this.lab.textContent = label;
    onTap(e, () => o.tap && o.tap());
  }
  lit(on, color) { this.cap.style.background = on ? (color || this.o.color || C.white) : '#5a5c63'; }
  setLabel(s) { this.lab.textContent = s; }
}

// ================================================================ dropdown field
class Drop {
  constructor(parent, x0, y0, x1, y1, o = {}) {
    this.o = o;
    const e = this.e = el('div', 'drop' + (o.small ? ' small' : ''), parent, { left: px(x0), top: px(y0), width: px(x1 - x0), height: px(y1 - y0) });
    this.m = el('div', 'main', e);
    if (!o.small) this.r = el('div', 'right', e);
    this.chev = chevron(o.color || C.txt, o.small ? 20 : 26);
    e.appendChild(this.chev);
    onTap(e, () => o.open && o.open());
  }
  set(main, right, color) {
    if (this.m.textContent !== main) this.m.textContent = main;
    if (this.r && this.r.textContent !== (right || '')) this.r.textContent = right || '';
    this.m.style.color = color || C.txt;
  }
}

// ================================================================ chip (flat button)
function chip(parent, x0, y0, x1, y1, label, fn, o = {}) {
  const e = el('div', 'chip', parent, { left: px(x0), top: px(y0), width: px(x1 - x0), height: px(y1 - y0) });
  e.textContent = label;
  if (o.color) { e.style.color = o.color; e.style.borderColor = o.border || o.color; }
  onTap(e, fn);
  e.setOn = (on, col) => {
    e.classList.toggle('on', !!on);
    e.style.background = on ? (col || C.white) : C.disp;
    if (!on) e.style.color = o.color || C.dim;
  };
  return e;
}

// ================================================================ the sheet (a list to choose from)
function openSheet(title, groups, selected, pick, o = {}) {
  const sh = el('div', 'sheet', stage);
  const box = el('div', 'box', sh);
  const t = el('div', 'title', box); t.textContent = title;
  const close = chip(box, 0, 0, 160, 60, 'CLOSE', () => sh.remove());
  close.className = 'chip close';
  close.style.left = ''; close.style.right = '24px'; close.style.top = '18px';
  const list = el('div', 'list', box);
  let selEl = null;
  for (const g of groups) {
    if (g.name) { const h = el('div', 'grp', list); h.textContent = g.name; }
    for (const it of g.items) {
      const b = el('div', 'opt' + (it.value === selected ? ' sel' : ''), list);
      const a = el('div', 'a', b); a.textContent = it.label;
      if (it.sub) { const s2 = el('div', 'b', b); s2.textContent = it.sub; } else a.style.top = '24px';
      if (it.color) a.style.color = it.color;
      if (it.value === selected) selEl = b;
      // a tap picks; a move scrolls (a small threshold tells them apart)
      let y0 = 0, moved = false;
      b.addEventListener('pointerdown', ev => { y0 = ev.clientY; moved = false; });
      b.addEventListener('pointermove', ev => { if (Math.abs(ev.clientY - y0) > 12) moved = true; });
      b.addEventListener('pointerup', ev => {
        if (moved) return;
        if (ev.target.classList.contains('x')) return;
        sh.remove(); pick(it.value);
      });
      if (it.del) {
        const x = el('div', 'x', b); x.textContent = '✕';
        x.addEventListener('pointerup', ev => {
          ev.stopPropagation();
          if (moved) return;
          if (x.dataset.armed) { sh.remove(); it.del(); } else { x.dataset.armed = '1'; x.textContent = 'DEL'; x.style.fontSize = '15px'; }
        });
      }
    }
  }
  if (selEl) setTimeout(() => selEl.scrollIntoView({ block: 'center' }), 0);
  sh.addEventListener('pointerdown', ev => { if (ev.target === sh) sh.remove(); });
  return sh;
}

function openName(title, initial, done) {
  const sh = el('div', 'sheet', stage);
  const box = el('div', 'box', sh, { top: '40px', height: '320px' });
  const t = el('div', 'title', box); t.textContent = title;
  const inp = el('input', null, box);
  inp.value = initial || ''; inp.maxLength = 24; inp.setAttribute('autocapitalize', 'characters');
  chip(box, 1280, 120, 1560, 200, 'SAVE', () => { const v = inp.value.trim(); sh.remove(); done(v); }, { color: C.green });
  chip(box, 1600, 120, 1880, 200, 'CANCEL', () => sh.remove());
  setTimeout(() => inp.focus(), 50);
}

// ================================================================ the genres and presets
function profLabel(p) {
  const pr = meta.profiles[p];
  return pr ? String(pr.g + 1).padStart(2, '0') + '  ' + upper(pr.name) : '?';
}
function genreGroups(withMain) {
  const groups = [];
  if (withMain) groups.push({ name: '', items: [{ label: '= MAIN', sub: 'FOLLOWS THE MAIN GENRE', value: -1 }] });
  const items = meta.profiles.map((p, i) => ({ label: String(p.g + 1).padStart(2, '0') + ' ' + upper(p.name), sub: upper(p.variant) + ' · ' + Math.round(p.bpm) + ' BPM', value: i }));
  groups.push({ name: 'GENRES', items });
  return groups;
}
function pickGenre() {
  openSheet('GENRE', genreGroups(false), st.genre, v => cmd(OP.GENRE, v));
}
function pickTrackGenre(t) {
  openSheet(TSPUR[t] + ' · PATTERN GENRE', genreGroups(true), st.tracks[t].sel, v => cmd(OP.TGENRE, t, v));
}
function pickPreset(t) {
  const tr = st.tracks[t];
  const groups = [];
  // by sound (BASS, LEAD, PAD, KEYS, FX) where the synth has groups, else one list in the presets' order
  const ORDER = ['BASS', 'LEAD', 'PAD', 'KEYS', 'FX'];
  const byGroup = new Map();
  for (let i = 0; i < tr.factory; i++) {
    const g = (tr.groups && tr.groups[i]) || '';
    if (!byGroup.has(g)) byGroup.set(g, []);
    byGroup.get(g).push({ label: tr.presets[i], value: i });
  }
  const names = [...byGroup.keys()].sort((a, b) => (ORDER.indexOf(a) + 1 || 99) - (ORDER.indexOf(b) + 1 || 99));
  for (const name of names) groups.push({ name, items: byGroup.get(name) });
  const user = [];
  for (let i = tr.factory; i < tr.presetCount; i++)
    user.push({ label: tr.presets[i], sub: 'YOUR PRESET', value: i, color: TC[t], del: () => cmd(OP.PRESET_DELETE, t, i) });
  if (user.length) groups.push({ name: 'YOUR PRESETS', items: user });
  openSheet(TNAME[t] + (t === 0 ? ' · KIT' : ' · PRESET'), groups, tr.preset, v => cmd(OP.PRESET, t, v));
}

// ================================================================ header and tabs
const TABS = [['BOX', null], ['ABLAUF', null], ['DRUM-X', TC[0]], ['MONO', TC[1]], ['FUNKE', TC[2]], ['SCHMELZ', TC[3]], ['SETUP', null]];
const tabEls = {};
function header(p, sub) {
  text(p, 34, 24, 'GENRA BOX', 46, '#f0f1f4', { weight: 800, sp: 6 });
  text(p, 36, 82, sub, 17, C.dim, { weight: 600, sp: 4 });
  const x0 = 640, w = (2376 - 640 - 6 * 12) / 7;
  TABS.forEach(([name, col], i) => {
    const e = el('div', 'tab', p, { left: px(x0 + i * (w + 12)), width: px(w) });
    e.textContent = name;
    if (col) { const b = el('div', 'bar', e); b.style.background = col; }
    onTap(e, () => show(name));
    (tabEls[name] = tabEls[name] || []).push({ e, col });
  });
}
function show(name) {
  page = name;
  for (const k in pages) pages[k].classList.toggle('on', k === name);
  for (const k in tabEls)
    for (const { e, col } of tabEls[k]) {
      const on = k === name;
      e.classList.toggle('on', on);
      e.style.color = on ? (col || C.txt) : C.dim;
      e.style.borderColor = on ? (col || C.txt) : '#2c2e36';
    }
}

// ================================================================ page BOX
const box = {};
function buildBox() {
  const p = pages.BOX = el('div', 'page', stage);
  header(p, 'GENRA · GROOVEBOX');
  plate(p, 24, 120, 560, 1056);
  box.genre = new Drop(p, 44, 136, 540, 196, { open: pickGenre });
  // the display
  el('div', 'disp', p, { left: '44px', top: '208px', width: '496px', height: '312px' });
  const bpmBox = el('div', 'abs', p, { left: '64px', top: '224px', width: '186px', height: '78px', background: '#0d0e11', border: '2px solid #3a3d47', borderRadius: '8px' });
  box.bpm = text(bpmBox, 14, 10, '120', 52, '#f0f1f4', { weight: 800, sp: 0 });
  text(bpmBox, 116, 38, 'BPM', 18, C.dim, { sp: 2 });
  bindBpm(bpmBox);
  el('div', 'abs', p, { left: '272px', top: '246px', width: '16px', height: '16px', borderRadius: '50%', background: C.green });
  text(p, 296, 244, 'INT', 20, C.green, { sp: 3 });
  box.keyLine = text(p, 66, 318, 'A MINOR · 4 BARS', 20, C.dim, { weight: 600, sp: 3 });
  box.cells = [];
  for (let t = 0; t < 4; t++) {
    box.cells.push([]);
    for (let s = 0; s < 16; s++)
      box.cells[t].push(el('div', 'cell', p, { left: px(66 + s * 28), top: px(350 + t * 31) }));
  }
  box.head = el('div', 'abs', p, { left: '62px', top: '346px', width: '30px', height: '128px', border: '3px solid #f0f1f4', borderRadius: '5px', display: 'none' });
  box.barTxt = text(p, 520, 318, '', 20, C.dim, { weight: 600, sp: 2 });
  box.barTxt.style.transform = 'translateX(-100%)';
  box.status = text(p, 66, 482, 'SOUND: SAME GENRE', 20, C.yellow, { sp: 3 });
  // generator
  box.key = new Knob(p, 130, 600, 40, 'KEY', { min: 0, max: 11, def: 9, steps: 12, fmt: v => meta.notes[Math.round(v)], change: v => cmd(OP.KEY, Math.round(v)) });
  box.color = new Knob(p, 285, 600, 40, 'COLOR', { def: 0.5, change: v => cmd(OP.COLOR, 0, 0, v) });
  box.swing = new Knob(p, 440, 600, 40, 'SWING', { min: -1, max: 1, def: 0, fmt: v => v === 0 ? 'GENRE' : bip(v), change: v => cmd(OP.SWING, 0, 0, v) });
  box.length = new Knob(p, 130, 745, 40, 'LENGTH', { min: 0, max: 3, def: 2, steps: 4, fmt: v => [1, 2, 4, 8][Math.round(v)] + ' BARS', change: v => cmd(OP.LENGTH, Math.round(v)) });
  box.mutate = new Knob(p, 285, 745, 40, 'MUTATE', { def: 0, change: v => cmd(OP.MUTATE, 0, 0, v) });
  box.master = new Knob(p, 440, 745, 40, 'MASTER', { def: 0.75, fmt: v => gainDb(v), change: v => cmd(OP.MASTER, 0, 0, v) });
  box.play = new Btn(p, 110, 885, 46, 'PLAY', { color: C.green, tap: () => cmd(OP.PLAY) });
  box.stop = new Btn(p, 240, 885, 46, 'STOP', { tap: () => cmd(OP.STOP) });
  box.newAll = new Btn(p, 370, 885, 46, 'NEW ALL', { tap: () => { cmd(OP.NEWALL); flash(box.newAll, C.white); } });
  box.lock = new Btn(p, 490, 885, 36, 'LOCK', { color: C.white, tap: () => cmd(OP.LOCK) });
  box.sound = chip(p, 44, 996, 290, 1046, 'SOUND', () => {
    // three states: SAME GENRE -> ALL GENRES -> OFF -> SAME GENRE
    if (!st.soundOn) { cmd(OP.SOUND); if (st.soundAll !== 2) cmd(OP.SOUNDMODE); }
    else if (st.soundAll === 2) cmd(OP.SOUNDMODE);
    else cmd(OP.SOUND);
  }, { color: C.yellow, border: '#2c2e36' });
  chip(p, 300, 996, 540, 1046, 'MIDI EXPORT', () => say(call('exportMidi', 0, 0) || ''));

  // the four tracks
  const cw = (2400 - 584 - 24 - 3 * 16) / 4;
  box.tr = [];
  for (let t = 0; t < 4; t++) {
    const x0 = 584 + t * (cw + 16), x1 = x0 + cw, c = TC[t], T = {};
    plate(p, x0, 120, x1, 1056);
    el('div', 'abs', p, { left: px(x0), top: '120px', width: px(cw), height: '8px', background: c, borderRadius: '18px 18px 0 0' });
    const nm = text(p, (x0 + x1) / 2, 146, TNAME[t], 32, c, { center: true, weight: 800, sp: 8 });
    onTap(nm, () => show(['DRUM-X', 'MONO', 'FUNKE', 'SCHMELZ'][t]));
    cap(p, x0 + 20, 200, 'PATTERN');
    T.genre = new Drop(p, x0 + 18, 228, x1 - 18, 290, { color: c, open: () => pickTrackGenre(t) });
    const q = (cw - 36) / 4;
    T.dens = new Knob(p, x0 + 18 + q * 0.5, 368, 40, null, { def: 0.5, title: TSPUR[t] + ' DENSITY', change: v => cmd(OP.DENS, t, 0, v) });
    T.k2 = new Knob(p, x0 + 18 + q * 1.5, 368, 40, null, { def: t === 0 ? 0.3 : 0.5, title: TSPUR[t] + ' ' + TK2[t], change: v => cmd(OP.TK2, t, 0, v) });
    text(p, x0 + 18 + q * 0.5, 428, 'DENSITY', 18, C.txt, { center: true, sp: 1 });
    text(p, x0 + 18 + q * 1.5, 428, TK2[t], 18, C.txt, { center: true, sp: 1 });
    T.nw = new Btn(p, x0 + 18 + q * 2.5, 368, 38, 'NEW', { color: c, tap: () => { cmd(OP.TNEW, t); flash(T.nw, c); } });
    T.hold = new Btn(p, x0 + 18 + q * 3.5, 368, 38, 'HOLD', { color: c, tap: () => cmd(OP.HOLD, t) });
    el('div', 'abs', p, { left: px(x0 + 18), top: '470px', width: px(cw - 36), height: '2px', background: C.line });
    cap(p, x0 + 20, 490, 'SOUND');
    T.preset = new Drop(p, x0 + 18, 518, x1 - 18, 580, { color: c, open: () => pickPreset(t) });
    T.ma = new Knob(p, x0 + cw * 0.28, 690, 66, MACROS[t][0], { color: c, def: 0.5, title: TSPUR[t] + ' ' + MACROS[t][0], fmt: bip05, change: v => cmd(OP.MACRO, t * 2, 0, v) });
    T.mb = new Knob(p, x0 + cw * 0.72, 690, 66, MACROS[t][1], { color: c, def: 0.5, title: TSPUR[t] + ' ' + MACROS[t][1], fmt: bip05, change: v => cmd(OP.MACRO, t * 2 + 1, 0, v) });
    T.level = new Knob(p, x0 + cw * 0.28, 900, 44, 'LEVEL', { def: 0.75, title: TSPUR[t] + ' LEVEL', fmt: gainDb, change: v => cmd(OP.LEVEL, t, 0, v) });
    T.mute = new Btn(p, x0 + cw * 0.72, 900, 40, 'MUTE', { color: C.red, tap: () => cmd(OP.MUTE, t) });
    box.tr.push(T);
  }
}
function bip05(v) { return bip((v - 0.5) * 2); }
function gainDb(v) {
  const g = (v / 0.75) ** 2;
  if (g < 0.001) return '-INF dB';
  const db = 20 * Math.log10(g);
  return (db > 0 ? '+' : '') + db.toFixed(1) + ' dB';
}
function flash(b, col) { b.lit(true, col); setTimeout(() => b.lit(false), 180); }
function bindBpm(e) {
  let sy = 0, sv = 0, drag = false, lastTap = 0, last = 0;
  e.addEventListener('pointerdown', ev => {
    ev.preventDefault();
    const now = performance.now();
    if (now - lastTap < 320) { cmd(OP.BPM_GENRE); lastTap = 0; return; }
    lastTap = now;
    try { e.setPointerCapture(ev.pointerId); } catch (x) {}
    drag = true; sy = ev.clientY; sv = st.bpm;
  });
  e.addEventListener('pointermove', ev => {
    if (!drag) return;
    const v = clamp(Math.round(sv + (sy - ev.clientY) / (stage._scale || 1) / 6), 40, 220);
    if (v !== last) { last = v; box.bpm.textContent = v; cmd(OP.BPM, 0, 0, v); }
  });
  const end = () => { drag = false; };
  e.addEventListener('pointerup', end);
  e.addEventListener('pointercancel', end);
}

function updateBox() {
  const pr = meta.profiles[st.genre] || { variant: '' };
  box.genre.set(profLabel(st.genre), upper(pr.variant));
  if (document.activeElement !== box.bpm) box.bpm.textContent = Math.round(st.bpm);
  box.keyLine.textContent = meta.notes[st.key] + ' ' + upper(st.scale) + ' · ' + [1, 2, 4, 8][st.length] + (st.length ? ' BARS' : ' BAR');
  box.key.set(st.key); box.color.set(st.color); box.swing.set(st.swing); box.length.set(st.length);
  box.mutate.set(st.mutate); box.master.set(st.master);
  box.lock.lit(st.lock);
  let status, col;
  if (st.lock) { status = 'CHORDS LOCKED'; col = C.white; }
  else if (st.soundOn) { status = st.soundAll === 2 ? 'SOUND: SAME GENRE' : 'SOUND: ALL GENRES'; col = C.yellow; }
  else { status = 'SOUND OFF'; col = C.dim; }
  box.status.textContent = status; box.status.style.color = col;
  box.sound.textContent = !st.soundOn ? 'SOUND OFF' : st.soundAll === 2 ? 'SAME GENRE' : 'ALL GENRES';
  box.sound.style.color = st.soundOn ? C.yellow : C.dim;
  for (let t = 0; t < 4; t++) {
    const T = box.tr[t], tr = st.tracks[t];
    if (tr.sel < 0) T.genre.set(upper(meta.profiles[tr.plays] ? meta.profiles[tr.plays].name : ''), '= MAIN');
    else T.genre.set(upper(meta.profiles[tr.sel].name), 'OWN', TC[t]);
    T.dens.set(tr.dens); T.k2.set(tr.k2);
    T.hold.lit(tr.hold);
    T.preset.set(tr.presetName, String(tr.preset + 1).padStart(2, '0') + '/' + tr.presetCount, tr.preset >= tr.factory ? TC[t] : null);
    T.ma.set(tr.macA); T.mb.set(tr.macB); T.level.set(tr.level);
    T.mute.lit(tr.mute, C.red);
  }
}
function updateBoxFast() {
  box.play.lit(fst.running, C.green);
  for (let t = 0; t < 4; t++) {
    const lane = fst.lanes[t] || '';
    for (let s = 0; s < 16; s++) {
      const on = lane[s] === '1';
      const c = box.cells[t][s];
      const want = on ? TC[t] : '#1a1c22';
      if (c._bg !== want) { c._bg = want; c.style.background = want; }
    }
  }
  if (fst.step >= 0) {
    box.head.style.display = 'block';
    box.head.style.left = px(62 + (fst.step % 16) * 28);
  } else box.head.style.display = 'none';
  box.barTxt.textContent = fst.bars > 1 ? 'BAR ' + (fst.bar + 1) + '/' + fst.bars : '';
}

// ================================================================ page ABLAUF
const ab = {};
function buildAblauf() {
  const p = pages.ABLAUF = el('div', 'page', stage);
  header(p, 'ABLAUF MINI');
  plate(p, 24, 120, 520, 1056);
  cap(p, 50, 140, 'PARTS');
  ab.parts = [];
  meta.sections.forEach((name, j) => {
    const cx = 150 + (j % 2) * 220, cy = 250 + Math.floor(j / 2) * 170;
    const b = new Btn(p, cx, cy, 56, name, { ring: SEC_COLOR[j], labColor: SEC_COLOR[j], color: SEC_COLOR[j], tap: () => { cmd(OP.AB_PART, j); flash(b, SEC_COLOR[j]); } });
    ab.parts.push(b);
  });
  text(p, 50, 770, 'NEW VARIANT ON THE NEXT FREE PAD', 16, C.dim, { weight: 600, sp: 2 });
  ab.undo = new Btn(p, 150, 900, 50, 'UNDO', { tap: () => cmd(OP.AB_UNDO) });
  ab.clear = new Btn(p, 370, 900, 50, 'CLEAR', { color: C.red, tap: () => cmd(OP.AB_CLEAR) });

  plate(p, 544, 120, 1544, 1056);
  ab.pads = [];
  for (let i = 0; i < 16; i++) {
    const r = Math.floor(i / 4), c = i % 4;
    const e = el('div', 'pad', p, { left: px(574 + c * 226 + 16), top: px(140 + r * 226 + 8) });
    const bar = el('div', 'bar', e), nm = el('div', 'nm', e), no = el('div', 'no', e), plus = el('div', 'plus', e);
    plus.textContent = '+';
    onTap(e, () => cmd(OP.AB_PAD, i));
    ab.pads.push({ e, bar, nm, no, plus });
  }

  plate(p, 1568, 120, 2376, 1056);
  cap(p, 1594, 140, 'NEW PARTS');
  ab.modes = [];
  ['DRUMS', 'BASS', 'MELODY', 'PADS'].forEach((lab, j) => {
    const y = 190 + j * 110, c = TC[j];
    el('div', 'abs', p, { left: '1594px', top: px(y), width: '10px', height: '80px', background: c });
    text(p, 1626, y + 24, lab, 28, c, { sp: 4 });
    const row = [];
    ['KEEP', 'RE-ROLL', 'VARY'].forEach((m, k) => {
      const bx = 1880 + k * 160;
      row.push(chip(p, bx, y + 8, bx + 148, y + 72, m, () => cmd(OP.AB_MODE, j, k)));
    });
    ab.modes.push(row);
  });
  el('div', 'abs', p, { left: '1594px', top: '640px', width: '756px', height: '2px', background: C.line });
  cap(p, 1594, 660, 'SONG');
  ab.song = el('div', 'abs', p, { left: '1594px', top: '710px', width: '756px', height: '60px' });
  ab.songHead = el('div', 'abs', p, { left: '1594px', top: '696px', width: '4px', height: '88px', background: C.white, display: 'none' });
  ab.songTxt = text(p, 1594, 800, '', 18, C.dim, { weight: 600, sp: 2 });
  ab.make = new Btn(p, 1720, 940, 56, 'MAKE SONG', { color: C.green, tap: () => cmd(OP.AB_SONG) });
  ab.next = new Btn(p, 1960, 940, 50, 'NEXT', { tap: () => cmd(OP.AB_NEXT) });
  chip(p, 2080, 910, 2350, 970, 'MIDI EXPORT', () => say(call('exportMidi', st.ablauf.song.length ? 1 : 2, 0) || ''));
}
function padColor(pd) {
  if (!pd.used) return null;
  return pd.tag >= 0 ? SEC_COLOR[pd.tag] : C.txt;
}
function updateAblauf() {
  const A = st.ablauf;
  A.pads.forEach((pd, i) => {
    const P = ab.pads[i];
    P.e.classList.toggle('empty', !pd.used);
    P.e.classList.toggle('clear', A.clear && pd.used);
    P.bar.style.display = pd.used ? 'block' : 'none';
    P.bar.style.background = padColor(pd) || 'transparent';
    P.nm.textContent = pd.used ? pd.name : '';
    P.no.textContent = pd.used ? String(i + 1).padStart(2, '0') : '';
    P.plus.style.display = pd.used ? 'none' : 'block';
  });
  ab.modes.forEach((row, j) => row.forEach((c, k) => c.setOn(A.modes[j] === k, TC[j])));
  ab.clear.lit(A.clear, C.red);
  // the song as a bar of its parts
  ab.song.innerHTML = '';
  const total = A.song.reduce((s, p) => s + p.bars, 0);
  let x = 0;
  for (const part of A.song) {
    const w = 756 * part.bars / total;
    el('div', 'abs', ab.song, { left: px(x), top: '0px', width: px(Math.max(2, w - 6)), height: '60px', borderRadius: '6px', background: part.tag >= 0 ? SEC_COLOR[part.tag] : C.txt });
    x += w;
  }
  ab.songTxt.textContent = total ? A.song.length + ' PARTS · ' + total + ' BARS' : 'MAKE SONG BUILDS A SONG FROM THE PLAYING PAD';
}
function updateAblaufFast() {
  ab.pads.forEach((P, i) => {
    P.e.classList.toggle('play', fst.playing === i);
    P.e.classList.toggle('cue', fst.cued === i && fst.playing !== i);
  });
  ab.make.lit(fst.songOn, C.green);
  ab.make.setLabel(fst.songOn ? 'STOP SONG' : 'MAKE SONG');
  const A = st.ablauf;
  if (fst.songOn && fst.songPos >= 0 && A.song.length) {
    const total = A.song.reduce((s, p) => s + p.bars, 0);
    let bars = 0;
    for (let k = 0; k < fst.songPos; k++) bars += A.song[k].bars;
    bars += fst.songFrac * A.song[fst.songPos].bars;
    ab.songHead.style.display = 'block';
    ab.songHead.style.left = px(1594 + 756 * bars / total);
  } else ab.songHead.style.display = 'none';
}

// ================================================================ synth pages
function section(p, x0, y0, x1, y1, title, col) {
  plate(p, x0, y0, x1, y1);
  el('div', 'abs', p, { left: px(x0 + 20), top: px(y0 + 22), width: '6px', height: '24px', background: col });
  text(p, x0 + 38, y0 + 20, title, 22, col, { sp: 4 });
}
/** The cells of a section: items laid out in `cols` columns (as the drafts). */
function cells(x0, y0, x1, y1, n, cols) {
  const rows = Math.ceil(n / cols), cw = (x1 - x0 - 20) / cols, ch = (y1 - y0 - 70) / rows;
  const out = [];
  for (let i = 0; i < n; i++) {
    const r = Math.floor(i / cols), c = i % cols;
    out.push([x0 + 10 + cw * (c + 0.5), y0 + 62 + ch * r + ch * 0.42, cw]);
  }
  return out;
}
function presetBar(p, t, label, extra) {
  const c = TC[t];
  plate(p, 24, 112, 2376, 196);
  cap(p, 50, 140, label);
  const S = {};
  S.preset = new Drop(p, 180, 124, 1000, 184, { color: c, open: () => pickPreset(t) });
  chip(p, 1030, 124, 1184, 184, 'SAVE', () => {
    const tr = st.tracks[t];
    openName('SAVE ' + TNAME[t] + (t ? ' PRESET' : ' KIT'), tr.preset >= tr.factory ? tr.presetName : '', name => call('savePreset', t, name));
  }, { color: c });
  chip(p, 1200, 124, 1354, 184, 'INIT', () => cmd(OP.SYN_INIT, t));
  chip(p, 1370, 124, 1524, 184, 'UNDO', () => cmd(OP.SYN_UNDO, t));
  if (extra !== false) S.ab = chip(p, 1540, 124, 1694, 184, 'A / B', () => cmd(OP.SYN_AB, t));
  if (t) {
    text(p, 1740, 140, 'TRACK: ' + TSPUR[t], 20, C.dim, { weight: 600, sp: 3 });
    text(p, 2170, 140, 'TEST', 20, C.dim, { weight: 600, sp: 3 });
    const tb = el('div', 'abs', p, { left: '2296px', top: '132px', width: '52px', height: '52px', borderRadius: '50%', background: c });
    onTap(tb, () => cmd(OP.SYN_TEST, t));
  }
  return S;
}
function fmtFor(t, d) {
  if (d.kind === 1 || d.kind === 2) return v => d.opts[clamp(Math.round(v), 0, d.opts.length - 1)] || '';
  if (t === 3 && d.key === 'reg') return v => REG_NAMES[clamp(Math.round(v * 7), 0, 7)] + '  ' + pct(v);
  if (t === 3 && d.key === 'sound') return v => SOUND_NAMES[clamp(Math.round(v * 4), 0, 4)] + '  ' + pct(v);
  if (d.min < 0) return bip;
  return pct;
}
const syn = [null, {}, {}, {}];
/** A tonal synth page: sections of [title, box, cols, keys] (a key with '!' = a big knob). */
function buildSynth(t, pageName, sub, layout) {
  const p = pages[pageName] = el('div', 'page', stage);
  header(p, sub);
  const S = syn[t];
  Object.assign(S, presetBar(p, t, 'PRESET'));
  S.ctl = {};
  const defs = meta.defs[t], c = TC[t];
  const byKey = {};
  defs.forEach((d, i) => { byKey[d.key] = i; });
  for (const [title, [x0, y0, x1, y1], cols, keys] of layout) {
    section(p, x0, y0, x1, y1, title, c);
    cells(x0, y0, x1, y1, keys.length, cols).forEach(([cx, cy, cw], k) => {
      const big = keys[k].endsWith('!'), key = keys[k].replace('!', '');
      const i = byKey[key];
      const d = defs[i];
      if (d.kind === 1) {
        // a list: a small field that opens its choices
        const w = Math.min(cw - 20, 210);
        const dr = new Drop(p, cx - w / 2, cy - 30, cx + w / 2, cy + 30, {
          small: true, color: c,
          open: () => openSheet(d.label, [{ name: '', items: d.opts.map((o, v) => ({ label: o, value: v })) }], Math.round(st.tracks[t].vals[i]),
            v => { cmd(OP.SYN_MARK, t); cmd(OP.SYN, t, i, v); }),
        });
        text(p, cx, cy + 50, d.label, 20, C.txt, { center: true, sp: 2 });
        S.ctl[i] = { set: v => dr.set(d.opts[clamp(Math.round(v), 0, d.opts.length - 1)] || '', '', c) };
      } else if (d.kind === 2) {
        const b = new Btn(p, cx, cy, 34, d.label, {
          color: c, tap: () => { cmd(OP.SYN_MARK, t); cmd(OP.SYN, t, i, st.tracks[t].vals[i] > 0.5 ? 0 : 1); },
        });
        const v2 = text(p, cx, cy - 12, '', 18, '#111216', { center: true, sp: 1 });
        S.ctl[i] = { set: v => { b.lit(v > 0.5, c); v2.textContent = d.opts.length === 2 && d.opts[0] !== 'OFF' ? d.opts[v > 0.5 ? 1 : 0] : ''; v2.style.color = v > 0.5 ? '#111216' : C.txt; } };
      } else {
        const kn = new Knob(p, cx, cy, big ? 52 : 38, d.label, {
          min: d.min, max: d.max, def: d.min < 0 ? 0 : (d.max - d.min) / 2 + d.min, color: big ? c : null, fmt: fmtFor(t, d),
          start: () => cmd(OP.SYN_MARK, t), change: v => cmd(OP.SYN, t, i, v),
        });
        S.ctl[i] = kn;
      }
    });
  }
}
function updateSynth(t) {
  const S = syn[t], tr = st.tracks[t];
  S.preset.set(tr.presetName, String(tr.preset + 1).padStart(2, '0') + '/' + tr.presetCount, tr.preset >= tr.factory ? TC[t] : null);
  if (S.ab) { S.ab.setOn(tr.ab, TC[t]); S.ab.textContent = tr.ab ? 'ORIGINAL' : 'A / B'; }
  tr.vals.forEach((v, i) => {
    const k = S.ctl[i];
    if (!k) return;
    if (k.o && tr.orig && tr.orig[i] != null) k.o.def = tr.orig[i];  // double tap: the preset's own value
    k.set(v);
  });
}

// ---------------------------------------------------------------- DRUM-X
const dxp = { lane: 1, patch: 0 };
function buildDrumX() {
  const p = pages['DRUM-X'] = el('div', 'page', stage);
  header(p, 'DRUM-X · DRUMS');
  Object.assign(dxp, presetBar(p, 0, 'KIT', false));
  const c = TC[0];
  plate(p, 24, 212, 2376, 400);
  el('div', 'abs', p, { left: '44px', top: '234px', width: '6px', height: '24px', background: c });
  text(p, 62, 232, 'CHOOSE A SOUND · A TAP PLAYS IT', 22, c, { sp: 4 });
  const pw = (2376 - 44 - 20 - 7 * 16) / 8;
  dxp.chips = [];
  meta.lanes.forEach((name, i) => {
    const x = 44 + i * (pw + 16);
    const e = el('div', 'abs', p, { left: px(x), top: '278px', width: px(pw), height: '106px', borderRadius: '14px', border: '3px solid #34363f', background: '#2b2d35' });
    const nm = el('div', 'tc', e, { left: '50%', top: '22px', fontSize: '26px', letterSpacing: '3px' }); nm.textContent = name;
    const sub = el('div', 'tc', e, { left: '50%', top: '62px', fontSize: '16px', fontWeight: 600, letterSpacing: '1px', color: C.dim });
    const glow = el('div', 'abs', e, { left: '14px', right: '14px', bottom: '8px', height: '5px', borderRadius: '3px', background: c, opacity: 0 });
    onTap(e, () => { dxp.lane = i; cmd(OP.SYN_TEST, 0, i); updateDrumX(); });
    dxp.chips.push({ e, nm, sub, glow });
  });
  // the sound of the chosen lane
  section(p, 24, 416, 1200, 1056, 'SOUND', c);
  dxp.title = pages['DRUM-X'].lastChild;
  const sc = cells(24, 416, 1200, 1056, 8, 4);
  dxp.snd = [];
  meta.snd.forEach((name, k) => {
    const [cx, cy] = sc[k], big = k === 0 || k === 2;
    dxp.snd.push(new Knob(p, cx, cy, big ? 52 : 38, name, {
      min: 0, max: 127, def: 64, steps: 128, color: big ? c : null, fmt: v => String(Math.round(v)),
      start: () => cmd(OP.SYN_MARK, 0), change: v => cmd(OP.DX_SND, dxp.lane, dxp.patch * 8 + k, v),
    }));
  });
  dxp.level = new Knob(p, sc[5][0], sc[5][1], 52, 'LEVEL', {
    min: 0, max: 127, def: 127, steps: 128, color: c, fmt: v => String(Math.round(v)),
    start: () => cmd(OP.SYN_MARK, 0), change: v => cmd(OP.DX_KIT, 0, JDX.LN0 + dxp.lane, v),
  });
  dxp.patchBtn = new Btn(p, sc[6][0], sc[6][1], 34, 'PATCH A / B', { color: c, tap: () => { dxp.patch ^= 1; updateDrumX(); } });
  dxp.patchTxt = text(p, sc[6][0], sc[6][1] - 12, 'A', 22, '#111216', { center: true, sp: 0 });
  dxp.mute = new Btn(p, sc[7][0], sc[7][1], 34, 'MUTE', { color: C.red, tap: () => cmd(OP.DX_LANEMUTE, dxp.lane) });
  // the whole kit
  section(p, 1216, 416, 2016, 1056, 'WHOLE KIT', c);
  const kc = cells(1216, 416, 2016, 1056, 8, 4);
  const kitDefs = [['MORPH', JDX.MORPH], ['TUNE', JDX.TUNE], ['TONE', JDX.TONE], ['DECAY', JDX.DECAY], ['NOISE', JDX.NOISE], ['FM ENV', JDX.FMENV], ['FOLD', JDX.FOLD], ['DRIVE', JDX.DRIVE]];
  dxp.kit = {};
  const kitKnob = (cx, cy, big, name, j, max) => new Knob(p, cx, cy, big ? 52 : 38, name, {
    min: 0, max: max || 127, def: 64, steps: (max || 127) + 1, color: big ? c : null, fmt: v => kitText(j, v),
    start: () => cmd(OP.SYN_MARK, 0), change: v => cmd(OP.DX_KIT, 0, j, v),
  });
  kitDefs.forEach(([name, j], k) => { dxp.kit[j] = kitKnob(kc[k][0], kc[k][1], k === 0, name, j); });
  section(p, 2032, 416, 2376, 1056, 'SENDS', c);
  const ec = cells(2032, 416, 2376, 1056, 3, 1);
  dxp.kit[JDX.DIST] = kitKnob(ec[0][0], ec[0][1], false, 'DIST', JDX.DIST, 64);
  dxp.kit[JDX.DLY] = kitKnob(ec[1][0], ec[1][1], false, 'DELAY', JDX.DLY);
  dxp.kit[JDX.REV] = kitKnob(ec[2][0], ec[2][1], false, 'REVERB', JDX.REV);
}
function kitText(j, v) {
  const t = st && st.dx && st.dx.kitText ? st.dx.kitText[j] : '';
  return (st && Math.round(v) === st.dx.kit[j] && t) ? t : String(Math.round(v));
}
function updateDrumX() {
  const tr = st.tracks[0], D = st.dx, c = TC[0];
  dxp.preset.set(tr.presetName, String(tr.preset + 1).padStart(2, '0') + '/' + tr.presetCount, tr.preset >= tr.factory ? c : null);
  dxp.chips.forEach((ch, i) => {
    const on = i === dxp.lane;
    ch.e.style.background = on ? c : '#2b2d35';
    ch.e.style.borderColor = on ? '#f0f1f4' : '#34363f';
    ch.nm.style.color = on ? '#111216' : C.txt;
    ch.sub.style.color = on ? '#2a2350' : C.dim;
    ch.sub.textContent = D.laneMute[i] ? 'MUTED' : 'LEVEL ' + D.kit[JDX.LN0 + i];
  });
  dxp.title.textContent = 'SOUND: ' + meta.lanes[dxp.lane] + ' · PATCH ' + (dxp.patch ? 'B' : 'A');
  const snd = D.snd[dxp.lane];
  dxp.snd.forEach((k, i) => k.set(snd[dxp.patch * 5 + i]));
  dxp.level.set(D.kit[JDX.LN0 + dxp.lane]);
  dxp.patchBtn.lit(true, c);
  dxp.patchTxt.textContent = dxp.patch ? 'B' : 'A';
  dxp.mute.lit(D.laneMute[dxp.lane], C.red);
  for (const j in dxp.kit) dxp.kit[j].set(D.kit[j]);
}
function updateDrumXFast() {
  if (!fst.hits) return;
  dxp.chips.forEach((ch, i) => { ch.glow.style.opacity = Math.min(1, fst.hits[i] * 1.5); });
}

// ================================================================ page SETUP
const setup = {};
function buildSetup() {
  const p = pages.SETUP = el('div', 'page', stage);
  header(p, 'SETUP');
  section(p, 24, 112, 760, 560, 'MIX', C.txt);
  setup.duck = new Knob(p, 200, 330, 52, 'DUCK', { color: C.txt, def: 0, change: v => cmd(OP.DUCK, 0, 0, v) });
  text(p, 320, 280, 'THE KICK PUSHES', 20, C.dim, { weight: 600, sp: 2 });
  text(p, 320, 312, 'BASS AND PADS DOWN', 20, C.dim, { weight: 600, sp: 2 });
  section(p, 24, 576, 760, 1056, 'MIDI EXPORT', C.txt);
  chip(p, 60, 660, 720, 740, 'THE LOOP THAT PLAYS', () => say(call('exportMidi', 0, 0) || ''));
  chip(p, 60, 760, 720, 840, 'THE SONG (ABLAUF)', () => say(call('exportMidi', 1, 0) || ''));
  chip(p, 60, 860, 720, 940, 'ALL PADS (ABLAUF)', () => say(call('exportMidi', 2, 0) || ''));
  text(p, 60, 980, 'FILES GO TO  DOWNLOAD / GENRA BOX', 18, C.dim, { weight: 600, sp: 2 });
  section(p, 776, 112, 2376, 1056, 'ABOUT', C.txt);
  const lines = [
    'GENRA BOX · THE GROOVEBOX OF THE GENRA VCV RACK PLUGIN ON ANDROID',
    'GENERATOR: GENRA  ·  VOICES: DRUM-X (JIANT FM), GENRA MONO, FUNKE, SCHMELZ  ·  SONGS: ABLAUF MINI',
    '',
    'KNOBS: DRAG UP OR RIGHT  ·  DOUBLE TAP: BACK TO THE DEFAULT',
    'BPM: DRAG THE NUMBER  ·  DOUBLE TAP: THE GENRE’S TEMPO',
    'TAP A TRACK NAME ON THE BOX PAGE: ITS SYNTH PAGE',
    'EVERYTHING IS KEPT WHEN THE APP CLOSES (SESSION AND YOUR PRESETS)',
    '',
    'FREE SOFTWARE · GPL-3.0 · GITHUB.COM/CAMILLOWUP/GENRAANDROID',
  ];
  lines.forEach((s, i) => text(p, 810, 190 + i * 46, s, 22, i < 2 ? C.txt : C.dim, { weight: 600, sp: 2 }));
  setup.audio = text(p, 810, 190 + lines.length * 46 + 40, '', 22, C.green, { weight: 600, sp: 2 });
  setup.ver = text(p, 810, 190 + lines.length * 46 + 86, '', 20, C.dim, { weight: 600, sp: 2 });
}
function updateSetup() {
  setup.duck.set(st.duck);
  setup.audio.textContent = 'AUDIO: ' + upper(call('audioInfo') || '').replace(/^AUDIO: /, '');
  setup.ver.textContent = 'VERSION ' + (call('version') || '');
}

// ================================================================ the loop
let lastMsg = -1, lastSaveVer = 0, lastSave = 0;
function applyState() {
  updateBox();
  updateAblauf();
  updateDrumX();
  for (let t = 1; t < 4; t++) updateSynth(t);
  updateSetup();
  if (st.msgId !== lastMsg) { if (lastMsg >= 0 && st.msg) say(st.msg); lastMsg = st.msgId; }
}
function poll() {
  const f = call('fast');
  if (f) {
    try { fst = JSON.parse(f); } catch (e) { fst = null; }
  }
  if (fst) {
    if (!st || fst.ver !== st.ver) {
      const s = call('state', st ? st.ver : 0);
      if (s) { try { st = JSON.parse(s); applyState(); } catch (e) { console.log(e); } }
    }
    updateBoxFast();
    updateAblaufFast();
    updateDrumXFast();
  }
  // the session is written now and then (and when the app goes to the background)
  const now = performance.now();
  if (st && st.ver !== lastSaveVer && now - lastSave > 15000) { lastSave = now; lastSaveVer = st.ver; call('save'); }
}

function start() {
  fit();
  meta = JSON.parse(call('meta'));
  st = JSON.parse(call('state', 0));
  fst = JSON.parse(call('fast'));
  buildBox();
  buildAblauf();
  buildDrumX();
  buildSynth(1, 'MONO', 'GENRA MONO · BASS', [
    ['OSCILLATORS', [24, 212, 760, 628], 3, ['wave1', 'tune1', 'level1', 'wave2', 'tune2', 'level2']],
    ['LFO', [24, 640, 760, 1056], 4, ['rate', 'lfotri', 'lfopitch', 'lfofilt']],
    ['FILTER', [776, 212, 1600, 628], 3, ['ftype', 'cutoff!', 'res!', 'egamt', 'drive', 'keytrk']],
    ['FILTER ENVELOPE', [776, 640, 1600, 1056], 4, ['fa', 'fd', 'fs', 'fr']],
    ['OUTPUT', [1616, 212, 2376, 628], 3, ['glide', 'accent', 'width']],
    ['AMP ENVELOPE', [1616, 640, 2376, 1056], 4, ['aa', 'ad', 'as', 'ar']],
  ]);
  buildSynth(2, 'FUNKE', 'FUNKE · MELODY', [
    ['SOUND', [24, 212, 980, 628], 4, ['model', 'timbre!', 'bite!', 'width', 'sub', 'mode', 'glide', 'accent']],
    ['FILTER', [24, 640, 980, 1056], 4, ['ftype', 'cutoff!', 'res', 'drive', 'fenv', 'key', 'fatk', 'fdec']],
    ['ENVELOPE', [996, 212, 1660, 628], 4, ['atk', 'dec', 'sus', 'rel']],
    ['VIBRATO', [996, 640, 1660, 1056], 3, ['vdepth', 'vrate', 'vdelay']],
    ['EFFECTS', [1676, 212, 2376, 1056], 2, ['chorus', 'lofi', 'div', 'echo', 'fdbk', 'verb!']],
  ]);
  buildSynth(3, 'SCHMELZ', 'SCHMELZ · PADS', [
    ['STRINGS', [24, 212, 980, 628], 3, ['reg!', 'cresc!', 'tone!', 'oct', 'rel', 'bal']],
    ['ENSEMBLE AND ANIMATION', [24, 640, 980, 1056], 4, ['ens', 'ensdepth', 'anim!', 'arate']],
    ['SOLO', [996, 212, 1900, 1056], 3, ['sound', 'atk', 'decay', 'sustain', 'hold', 'vel', 'trem', 'trate', 'split']],
    ['EFFECTS', [1916, 212, 2376, 1056], 2, ['phaser', 'prate', 'verb!', 'size', 'volume']],
  ]);
  buildSetup();
  show('BOX');
  applyState();
  lastMsg = st.msgId;
  lastSaveVer = st.ver;
  setInterval(poll, 50);
}

if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
else start();
