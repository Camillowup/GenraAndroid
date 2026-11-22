// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android – the GRID page: the notes of bass, melody and pads as a piano roll, the drums as a step grid.
// Two bars at a time (< BAR / BAR >). Piano roll: tap an empty place = a note, drag a note = move it, drag its end =
// its length, double tap = delete; the VEL lane sets the velocity. Drums: tap = hit on / off, long press = select.
// An edit turns the track into fixed notes (as in GENRA GRID); NEW generates it again.
'use strict';

const GX = 130, GW = 1750, GY = 196, GH = 634, VY = 850, VH = 180;  // the grid and the VEL lane (stage px)
const DRUM_NAMES = ['KICK', 'SNARE', 'CLAP', 'RIM', 'HAT CL', 'HAT OP', 'TOM', 'PERC'];
const DRUM_TO_LANE = [0, 1, 2, 6, 3, 4, 5, 7];  // GENRA's drums onto DRUM-X's lanes
const NOTE_NM = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const LENGTHS = [0, 4, 6, 8, 12, 16, 24, 32, 48, 64];
const gr = {
  track: 1, bar0: 0, view: 'ALL', gs: null, gsVer: -1, gsKey: '', notes: [], sel: -1, top: 0,
  drumSel: null, drumLane: 0, drag: null, canvas: null, ctx: null,
};

function noteName(p) { return NOTE_NM[((p % 12) + 12) % 12] + (Math.floor(p / 12) - 1); }
function gFrom() { return gr.bar0 * 16; }
function stepW() { return GW / 32; }

function buildGrid() {
  const p = pages.GRID = el('div', 'page', stage);
  header(p, 'GRID');
  plate(p, 24, 112, 1900, 1056);
  gr.title = text(p, 44, 122, '', 26, TC[1], { weight: 800, sp: 5 });
  gr.info = text(p, 230, 128, '', 18, C.dim, { weight: 600, sp: 3 });
  text(p, 40, 880, 'VEL', 18, C.dim, { weight: 600, sp: 3 });
  const cv = gr.canvas = el('canvas', null, p, { position: 'absolute', left: '24px', top: '140px', width: '1876px', height: '900px', touchAction: 'none' });
  gr.ctx = cv.getContext('2d');
  bindGridCanvas(cv);

  plate(p, 1916, 112, 2376, 1056);
  cap(p, 1940, 132, 'TRACK');
  gr.tracks = ['DRUM', 'BASS', 'MEL', 'PADS'].map((lab, t) => {
    const x = 1940 + t * 108;
    const c = chip(p, x, 162, x + 96, 222, lab, () => { gr.track = t; gr.sel = -1; gr.drumSel = null; gr.top = 0; refreshGrid(true); }, { color: TC[t], border: TC[t] });
    c.style.fontSize = '18px';
    return c;
  });
  gr.viewCap = cap(p, 1940, 250, 'VIEW');
  gr.views = ['ALL', 'SCALE', 'USED'].map((v, k) => {
    const x = 1940 + k * 144;
    const c = chip(p, x, 280, x + 132, 336, v, () => { gr.view = v; gr.top = 0; drawGrid(); updateGridPanel(); });
    c.style.fontSize = '20px';
    return c;
  });
  const nav = [['< BAR', () => { gr.bar0 = Math.max(0, gr.bar0 - 2); refreshGrid(true); }], ['', null],
    ['BAR >', () => { const bars = gr.gs ? Math.max(1, gr.gs.L / 16) : 4; if (gr.bar0 + 2 < bars) gr.bar0 += 2; refreshGrid(true); }]];
  gr.navEls = nav.map(([lab, fn], k) => {
    const x = 1940 + k * 144;
    const c = chip(p, x, 350, x + 132, 406, lab, fn || (() => {}));
    c.style.fontSize = '18px';
    return c;
  });
  gr.noteCap = cap(p, 1940, 440, 'NOTE');
  gr.vel = new Knob(p, 2010, 525, 38, 'VEL', { min: 1, max: 127, def: 100, steps: 127, fmt: v => String(Math.round(v)), start: () => {}, change: v => setSel('vel', Math.round(v)) });
  gr.chance = new Knob(p, 2146, 525, 38, 'CHANCE', { min: 1, max: 100, def: 100, steps: 100, fmt: v => Math.round(v) + ' %', change: v => setSel('chance', Math.round(v)) });
  gr.slide = new Btn(p, 2290, 525, 32, 'SLIDE', { color: TC[1], tap: () => setSel('slide', -1) });
  gr.roll = new Knob(p, 2290, 525, 38, 'ROLL', { min: 1, max: 4, def: 1, steps: 4, fmt: v => Math.round(v) === 1 ? 'OFF' : Math.round(v) + 'x', change: v => setSel('roll', Math.round(v)) });
  cap(p, 1940, 640, 'EDIT');
  const ed = [
    ['UNDO', () => cmd(OP.UNDO)], ['REDO', () => cmd(OP.REDO)], ['COPY BAR', () => cmd(OP.G_COPY, gr.track, gr.lastBar || gr.bar0)],
    ['PASTE', () => cmd(OP.G_PASTE, gr.track, gr.lastBar || gr.bar0)], ['CLEAR', () => clearArm()], ['NEW', () => { cmd(OP.TNEW, gr.track); gr.sel = -1; }],
  ];
  gr.edit = ed.map(([lab, fn], i) => {
    const r = Math.floor(i / 3), c = i % 3, x = 1940 + c * 144, y = 672 + r * 76;
    const e = chip(p, x, y, x + 132, y + 62, lab, fn);
    e.style.fontSize = '18px';
    return e;
  });
  gr.help = [0, 1, 2, 3].map(k => text(p, 1940, 850 + k * 26, '', 15, C.dim, { weight: 600, sp: 1 }));
  gr.len = chip(p, 1940, 968, 2352, 1032, 'LENGTH', pickLength);
  gr.len.style.color = C.txt;
  sizeGridCanvas();
  window.addEventListener('resize', () => setTimeout(() => { sizeGridCanvas(); drawGrid(); }, 50));
}

let clearTimer = 0;
function clearArm() {
  const b = gr.edit[4];
  if (b.textContent === 'SURE?') { clearTimeout(clearTimer); b.textContent = 'CLEAR'; cmd(OP.G_CLEAR, gr.track); gr.sel = -1; return; }
  b.textContent = 'SURE?';
  clearTimer = setTimeout(() => { b.textContent = 'CLEAR'; }, 2000);
}

function pickLength() {
  const cur = gr.gs ? gr.gs.cycle : 0;
  openSheet(TSPUR[gr.track] + ' · LENGTH', [{ name: 'STEPS OF THE TRACK (ITS OWN CYCLE)', items: LENGTHS.map(n => ({ label: n ? n + ' STEPS' : 'THE LOOP', sub: n ? (n / 16).toFixed(n % 16 ? 2 : 0) + ' BARS' : 'FOLLOWS LENGTH', value: n })) }], cur,
    v => cmd(OP.G_LEN, gr.track, v));
}

function sizeGridCanvas() {
  const s = (stage._scale || 1) * (window.devicePixelRatio || 1);
  gr.canvas.width = Math.round(1876 * s);
  gr.canvas.height = Math.round(900 * s);
  gr.pxScale = s;
}

/** Reload the track's notes from the engine (when it changed, or another track / window). */
function refreshGrid(force) {
  if (!meta) return;
  const key = gr.track + ':' + gr.bar0;
  if (!force && gr.gsVer === st.ver && gr.gsKey === key) return;
  if (gr.drag) return;  // not while a finger edits
  try { gr.gs = JSON.parse(call('gridState', gr.track, 0, 64) || 'null'); } catch (e) { gr.gs = null; }
  gr.gsVer = st.ver;
  gr.gsKey = key;
  if (!gr.gs) return;
  const bars = Math.max(1, gr.gs.L / 16);
  if (gr.bar0 >= bars) gr.bar0 = Math.max(0, Math.floor((bars - 1) / 2) * 2);
  gr.notes = gr.track ? gr.gs.notes.map(n => ({ s: n[0], l: n[1], p: n[2], v: n[3], sl: n[4], c: n[5] })) : [];
  if (gr.sel >= gr.notes.length) gr.sel = -1;
  drawGrid();
  updateGridPanel();
}

/** The chords of the window: drawn from gridState of the window (64 steps = the whole loop up to 4 bars). */
function windowChords() {
  if (!gr.gs || !gr.gs.chords) return [];
  return gr.gs.chords;
}

/** The rows of the piano roll (pitches, top first). */
function gridRows() {
  const gs = gr.gs;
  const base = gr.track === 1 ? 36 : gr.track === 2 ? 60 : 55;
  let lo = base - 12, hi = base + 24;
  for (const n of gr.notes) { lo = Math.min(lo, n.p - 2); hi = Math.max(hi, n.p + 2); }
  lo = Math.max(12, lo); hi = Math.min(115, hi);
  let rows = [];
  for (let pch = hi; pch >= lo; pch--) rows.push(pch);
  if (gr.view === 'SCALE' && gs) rows = rows.filter(pch => (gs.scaleMask >> (pch % 12)) & 1);
  if (gr.view === 'USED') {
    const used = [...new Set(gr.notes.map(n => n.p))].sort((a, b) => b - a);
    rows = used.length ? used : rows.slice(0, 12);
  }
  return rows;
}
function visibleRows() {
  const rows = gridRows();
  const n = gr.view === 'ALL' ? 18 : gr.view === 'SCALE' ? 14 : Math.min(18, Math.max(6, rows.length));
  // first time: center on the notes
  if (gr.top === 0 && gr.notes.length && rows.length > n) {
    const ps = gr.notes.map(x => x.p).sort((a, b) => a - b);
    const mid = ps[Math.floor(ps.length / 2)];
    let idx = rows.findIndex(r => r <= mid);
    gr.top = clamp(idx - Math.floor(n / 2), 0, rows.length - n) || 0.0001;
  }
  const top = clamp(Math.round(gr.top), 0, Math.max(0, rows.length - n));
  return { rows: rows.slice(top, top + n), all: rows, n, top };
}

// ---------------------------------------------------------------- drawing
function drawGrid() {
  const g = gr.ctx;
  if (!g || !gr.gs || page !== 'GRID') return;
  const s = gr.pxScale || 1;
  g.setTransform(s, 0, 0, s, 0, 0);
  g.clearRect(0, 0, 1876, 900);
  g.save();
  g.translate(-24, -140);  // stage coordinates
  const from = gFrom(), L = gr.gs.L, sw = stepW();
  const c = TC[gr.track];
  // the VEL lane
  g.fillStyle = '#0f1014';
  g.fillRect(GX, VY, GW, VH);
  if (gr.track === 0) drawDrums(g, from, L, sw, c);
  else drawPiano(g, from, L, sw, c);
  // bar lines and the inactive part (beyond the loop)
  for (let k = 0; k <= 32; k += 4) {
    const x = GX + k * sw, major = k % 16 === 0;
    g.fillStyle = major ? '#3a3d47' : '#2a2c35';
    g.fillRect(x - (major ? 1.5 : 0.5), gr.track ? 196 : 160, major ? 3 : 1, gr.track ? 634 : 670);
  }
  if (from + 32 > L) {
    const x = GX + Math.max(0, L - from) * sw;
    g.fillStyle = 'rgba(23,24,29,0.75)';
    g.fillRect(x, 150, GX + GW - x, VY + VH - 150);
  }
  g.fillStyle = C.dim;
  g.font = '600 16px Inter, sans-serif';
  g.fillText('BAR ' + (gr.bar0 + 1), GX, 842);
  if (from + 16 < L) g.fillText('BAR ' + (gr.bar0 + 2), GX + 16 * sw + 6, 842);
  // the playhead
  if (fst && fst.step >= 0 && fst.step >= from && fst.step < from + 32) {
    const x = GX + (fst.step - from + 0.5) * sw;
    g.fillStyle = C.white;
    g.fillRect(x - 1.5, 150, 3, 680);
  }
  g.restore();
}

function drawPiano(g, from, L, sw, c) {
  const { rows } = visibleRows();
  const rh = GH / rows.length;
  gr.rowsNow = rows;
  gr.rh = rh;
  // chord strip
  const chords = windowChords();
  let curPcs = 0;
  const play = fst && fst.step >= 0 ? fst.step : -1;
  for (const ch of chords) {
    const a = Math.max(ch.s, from), b = Math.min(ch.s + ch.n, from + 32);
    if (b <= a) continue;
    const on = play >= ch.s && play < ch.s + ch.n;
    if (on) curPcs = ch.pcs;
    g.fillStyle = on ? '#1f6f62' : '#2b2d35';
    roundRect(g, GX + (a - from) * sw + 2, 150, (b - a) * sw - 4, 36, 6);
    g.fillStyle = '#f0f1f4';
    g.font = '700 22px Inter, sans-serif';
    g.fillText(ch.name, GX + (a - from) * sw + 12, 176);
  }
  if (!curPcs && chords.length) {
    const ch = chords.find(x => x.s <= from && x.s + x.n > from) || chords[0];
    curPcs = ch.pcs;
  }
  // rows
  rows.forEach((pch, i) => {
    const y = GY + i * rh;
    const inChord = (curPcs >> (pch % 12)) & 1, inScale = (gr.gs.scaleMask >> (pch % 12)) & 1;
    g.fillStyle = inChord ? '#15211f' : inScale ? '#13141a' : '#0e0f12';
    g.fillRect(GX, y, GW, rh - 2);
    const black = [1, 3, 6, 8, 10].includes(pch % 12);
    g.fillStyle = black ? '#3a3d47' : '#d9dbe2';
    roundRect(g, 36, y + 2, 84, rh - 5, 5);
    g.fillStyle = black ? '#d9dbe2' : '#111216';
    g.font = '700 ' + Math.min(16, Math.floor(rh * 0.5)) + 'px Inter, sans-serif';
    g.fillText(noteName(pch), 64, y + rh / 2 + 6);
  });
  // notes
  gr.notes.forEach((n, i) => {
    if (n.s >= from + 32 || n.s + n.l <= from) return;
    const r = rows.indexOf(n.p);
    if (r < 0) return;
    const x0 = GX + (Math.max(n.s, from) - from) * sw + 2, x1 = GX + (Math.min(n.s + n.l, from + 32) - from) * sw - 3;
    const y = GY + r * rh + 5;
    const alpha = n.c < 100 ? 0.55 : 1;
    g.globalAlpha = alpha;
    g.fillStyle = c;
    roundRect(g, x0, y, x1 - x0, rh - 12, 6);
    g.globalAlpha = 1;
    if (i === gr.sel) {
      g.strokeStyle = C.white;
      g.lineWidth = 4;
      g.strokeRect(x0, y, x1 - x0, rh - 12);
      g.fillStyle = '#111216';
      g.fillRect(x1 - 14, y + 5, 6, rh - 22);
    }
    if (n.sl) { g.fillStyle = '#111216'; g.font = '700 16px Inter, sans-serif'; g.fillText('>', x0 + 6, y + rh / 2); }
    if (n.c < 100) { g.fillStyle = '#111216'; g.font = '700 14px Inter, sans-serif'; g.fillText(n.c + '%', Math.max(x0 + 4, x1 - 46), y + rh / 2); }
    // the VEL lane
    if (n.s >= from) {
      const h = (n.v / 127) * (VH - 10);
      g.fillStyle = i === gr.sel ? C.white : c;
      g.fillRect(GX + (n.s - from + 0.5) * sw - 8, VY + VH - h, 16, h);
    }
  });
}

function drawDrums(g, from, L, sw, c) {
  const hits = gr.gs.hits;
  const rh = (830 - 160) / 8;
  gr.rh = rh;
  for (let v = 0; v < 8; v++) {
    const y = 160 + v * rh;
    g.fillStyle = v % 2 ? '#0f1014' : '#121318';
    g.fillRect(GX, y, GW, rh - 4);
    const lit = fst && fst.hits ? Math.min(1, fst.hits[DRUM_TO_LANE[v]] * 1.5) : 0;
    g.fillStyle = v === gr.drumLane ? '#2b2d35' : C.disp;
    roundRect(g, 36, y + 4, 88, rh - 12, 6);
    if (lit > 0.05) { g.fillStyle = c; g.globalAlpha = lit; g.fillRect(40, y + rh - 14, 80, 4); g.globalAlpha = 1; }
    g.fillStyle = v === gr.drumLane ? c : C.txt;
    g.font = '700 15px Inter, sans-serif';
    g.fillText(DRUM_NAMES[v], 44, y + rh / 2 + 4);
    for (let k = 0; k < 32; k++) {
      const st2 = from + k;
      const h = hits[v] && hits[v][st2];
      const on = h && h[0] > 0;
      const x = GX + k * sw;
      g.globalAlpha = on && h[2] < 100 ? 0.55 : 1;
      g.fillStyle = on ? c : '#1c1d24';
      roundRect(g, x + 4, y + 8, sw - 8, rh - 20, 5);
      g.globalAlpha = 1;
      if (on && h[1] > 1) {  // a roll: small bars
        g.fillStyle = '#2a2350';
        for (let r = 0; r < h[1]; r++) g.fillRect(x + 8 + r * ((sw - 16) / h[1]), y + rh - 26, (sw - 16) / h[1] - 3, 6);
      }
      if (gr.drumSel && gr.drumSel[0] === v && gr.drumSel[1] === st2) {
        g.strokeStyle = C.white;
        g.lineWidth = 4;
        g.strokeRect(x + 4, y + 8, sw - 8, rh - 20);
      }
      if (v === gr.drumLane && on) {
        const hh = (h[0] / 127) * (VH - 10);
        g.fillStyle = c;
        g.fillRect(x + sw / 2 - 8, VY + VH - hh, 16, hh);
      }
    }
  }
}

function roundRect(g, x, y, w, h, r) {
  if (w <= 0 || h <= 0) return;
  r = Math.min(r, w / 2, h / 2);
  g.beginPath();
  g.moveTo(x + r, y);
  g.arcTo(x + w, y, x + w, y + h, r);
  g.arcTo(x + w, y + h, x, y + h, r);
  g.arcTo(x, y + h, x, y, r);
  g.arcTo(x, y, x + w, y, r);
  g.closePath();
  g.fill();
}

// ---------------------------------------------------------------- touch
function stagePos(ev) {
  const r = gr.canvas.getBoundingClientRect();
  const s = r.width / 1876;
  return [(ev.clientX - r.left) / s + 24, (ev.clientY - r.top) / s + 140];
}
function writeNotes() {
  const arr = gr.notes.map(n => [n.s, n.l, n.p, n.v, n.sl ? 1 : 0, n.c]);
  call('gridWrite', gr.track, JSON.stringify(arr));
  gr.gsVer = -1;
}

function bindGridCanvas(cv) {
  let lastTap = { t: 0, i: -1, cell: '' }, longTimer = 0;
  cv.addEventListener('pointerdown', ev => {
    ev.preventDefault();
    if (!gr.gs) return;
    try { cv.setPointerCapture(ev.pointerId); } catch (e) {}
    const [x, y] = stagePos(ev);
    const sw = stepW(), from = gFrom();
    const stp = from + Math.floor((x - GX) / sw);
    const now = performance.now();
    if (gr.track === 0) {
      // drums: lane names, cells, VEL lane
      const rh = (830 - 160) / 8, v = Math.floor((y - 160) / rh);
      if (x < GX && v >= 0 && v < 8) { gr.drumLane = v; cmd(OP.SYN_TEST, 0, DRUM_TO_LANE[v]); drawGrid(); return; }
      if (x < GX || stp >= from + 32 || stp >= gr.gs.L) return;
      if (y >= VY) {
        gr.drag = { mode: 'dvel', id: ev.pointerId };
        cmd(OP.G_MARK);
        dragDrumVel(x, y);
        return;
      }
      if (v < 0 || v >= 8) return;
      gr.drumLane = v;
      const h = gr.gs.hits[v][stp];
      gr.drag = { mode: 'dtap', id: ev.pointerId, v, stp, on: h && h[0] > 0, long: false };
      clearTimeout(longTimer);
      longTimer = setTimeout(() => {  // long press on a hit: select it (no toggle)
        if (gr.drag && gr.drag.mode === 'dtap' && gr.drag.on) { gr.drag.long = true; gr.drumSel = [v, stp]; drawGrid(); updateGridPanel(); }
      }, 400);
      return;
    }
    // piano roll
    if (y >= VY) {
      gr.drag = { mode: 'vel', id: ev.pointerId, changed: false };
      dragVel(x, y);
      return;
    }
    if (x < GX) {  // the keys: drag to scroll
      gr.drag = { mode: 'scroll', id: ev.pointerId, y0: y, top0: visibleRows().top };
      return;
    }
    if (y < GY || stp >= from + 32 || stp >= gr.gs.L) return;
    const rows = gr.rowsNow || [], rh = gr.rh || 30;
    const r = Math.floor((y - GY) / rh);
    if (r < 0 || r >= rows.length) return;
    const pch = rows[r];
    // a note under the finger?
    let hit = -1, end = false;
    gr.notes.forEach((n, i) => {
      if (n.p !== pch) return;
      if (stp >= n.s && stp < n.s + n.l) {
        hit = i;
        const x1 = GX + (Math.min(n.s + n.l, from + 32) - from) * sw;
        end = x > x1 - Math.max(26, Math.min(60, n.l * sw * 0.3));
      }
    });
    if (hit >= 0) {
      if (lastTap.i === hit && now - lastTap.t < 320) {  // double tap: delete
        gr.notes.splice(hit, 1);
        gr.sel = -1;
        lastTap = { t: 0, i: -1 };
        writeNotes();
        drawGrid();
        updateGridPanel();
        return;
      }
      lastTap = { t: now, i: hit };
      gr.sel = hit;
      const n = gr.notes[hit];
      gr.drag = { mode: end ? 'len' : 'move', id: ev.pointerId, i: hit, s0: n.s, p0: n.p, l0: n.l, x0: x, y0: y, r0: r, changed: false };
    } else {
      const len = gr.track === 3 ? 4 : 2;
      const n = { s: stp, l: Math.min(len, gr.gs.L - stp), p: pch, v: 100, sl: 0, c: 100 };
      gr.notes.push(n);
      gr.sel = gr.notes.length - 1;
      lastTap = { t: now, i: gr.sel };
      gr.drag = { mode: 'len', id: ev.pointerId, i: gr.sel, s0: n.s, p0: n.p, l0: n.l, x0: x, y0: y, r0: r, changed: true };
    }
    gr.lastBar = Math.floor(stp / 16);
    drawGrid();
    updateGridPanel();
  });
  cv.addEventListener('pointermove', ev => {
    const d = gr.drag;
    if (!d || ev.pointerId !== d.id) return;
    const [x, y] = stagePos(ev);
    const sw = stepW(), from = gFrom();
    if (d.mode === 'scroll') {
      const rh = gr.rh || 30;
      gr.top = Math.max(0, d.top0 - Math.round((y - d.y0) / rh)) || 0.0001;
      drawGrid();
      return;
    }
    if (d.mode === 'vel') { dragVel(x, y); return; }
    if (d.mode === 'dvel') { dragDrumVel(x, y); return; }
    if (d.mode === 'dtap') return;
    const n = gr.notes[d.i];
    if (!n) return;
    if (d.mode === 'move') {
      const ds = Math.round((x - d.x0) / sw);
      const rows = gr.rowsNow || [], rh = gr.rh || 30;
      const r = clamp(d.r0 + Math.round((y - d.y0) / rh), 0, rows.length - 1);
      const ns = clamp(d.s0 + ds, 0, gr.gs.L - 1), np = rows[r];
      if (ns !== n.s || np !== n.p) {
        n.s = ns; n.p = np; n.l = Math.min(n.l, gr.gs.L - ns);
        d.changed = true;
        if (np !== d.lastP) { d.lastP = np; }
      }
    } else if (d.mode === 'len') {
      const endStep = clamp(from + Math.floor((x - GX) / sw), n.s, gr.gs.L - 1);
      const nl = endStep - n.s + 1;
      if (nl !== n.l) { n.l = nl; d.changed = true; }
    }
    drawGrid();
  });
  const up = ev => {
    const d = gr.drag;
    clearTimeout(longTimer);
    if (!d || ev.pointerId !== d.id) return;
    gr.drag = null;
    if (d.mode === 'dtap') {
      if (!d.long) {
        cmd(OP.G_MARK);
        cmd(OP.G_DRUM, d.v, d.stp, d.on ? 0 : 100);
        gr.drumSel = d.on ? null : [d.v, d.stp];
        gr.gsVer = -1;
      }
      updateGridPanel();
      return;
    }
    if (d.mode === 'vel' && d.changed) writeNotes();
    if ((d.mode === 'move' || d.mode === 'len') && d.changed) writeNotes();
    drawGrid();
  };
  cv.addEventListener('pointerup', up);
  cv.addEventListener('pointercancel', up);
}

function dragVel(x, y) {
  const from = gFrom(), sw = stepW();
  const stp = from + Math.floor((x - GX) / sw);
  const v = clamp(Math.round((VY + VH - y) / (VH - 10) * 127), 1, 127);
  // the selected note if it starts here, else every note starting here
  let changed = false;
  gr.notes.forEach((n, i) => {
    if (n.s === stp && (gr.sel < 0 || gr.notes[gr.sel].s !== stp || i === gr.sel)) { n.v = v; changed = true; }
  });
  if (changed) { gr.drag.changed = true; drawGrid(); readout.textContent = 'VEL   ' + v; readout.style.display = 'block'; setTimeout(() => { if (!gr.drag) readout.style.display = 'none'; }, 600); }
}
function dragDrumVel(x, y) {
  const from = gFrom(), sw = stepW();
  const stp = from + Math.floor((x - GX) / sw);
  const v = clamp(Math.round((VY + VH - y) / (VH - 10) * 127), 1, 127);
  const h = gr.gs.hits[gr.drumLane] && gr.gs.hits[gr.drumLane][stp];
  if (!h || h[0] <= 0) return;
  h[0] = v;
  cmd(OP.G_DRUM, gr.drumLane, stp, v);
  drawGrid();
}

/** The NOTE / HIT knobs act on the selection. */
function setSel(what, v) {
  if (gr.track === 0) {
    if (!gr.drumSel) return;
    const [voice, stp] = gr.drumSel;
    cmd(OP.G_MARK);
    if (what === 'vel') cmd(OP.G_DRUM, voice, stp, v);
    if (what === 'chance') cmd(OP.G_DRUMCH, voice, stp, v);
    if (what === 'roll') cmd(OP.G_DRUMRAT, voice, stp, v);
    gr.gsVer = -1;
    return;
  }
  const n = gr.notes[gr.sel];
  if (!n) return;
  if (what === 'vel') n.v = v;
  if (what === 'chance') n.c = v;
  if (what === 'slide') n.sl = n.sl ? 0 : 1;
  clearTimeout(gr.writeTimer);
  gr.writeTimer = setTimeout(writeNotes, 150);  // knobs send many values: write once they rest
  drawGrid();
  updateGridPanel();
}

function updateGridPanel() {
  if (!gr.tracks) return;
  const t = gr.track, c = TC[t];
  gr.tracks.forEach((ch, k) => ch.setOn(k === t, TC[k]));
  gr.title.textContent = ['DRUMS', 'BASS', 'MELODY', 'PADS'][t];
  gr.title.style.color = c;
  const tr = st.tracks[t];
  const g = meta.profiles[tr.plays];
  const bars = gr.gs ? Math.max(1, gr.gs.L / 16) : 4;
  gr.info.textContent = '· ' + upper(g ? g.name : '') + ' · ' + (tr.fixed ? 'EDITED' : 'GENERATED') +
    (tr.lock ? ' · LOCKED' : '') + ' · BAR ' + (gr.bar0 + 1) + (bars > gr.bar0 + 1 ? '-' + (gr.bar0 + 2) : '') + ' / ' + bars +
    ' · ' + meta.notes[st.key] + ' ' + upper(st.scale);
  gr.title.style.letterSpacing = '5px';
  gr.info.style.left = px(44 + gr.title.offsetWidth + 16);
  gr.navEls[1].textContent = (gr.bar0 + 1) + (bars > gr.bar0 + 1 ? '-' + (gr.bar0 + 2) : '') + ' / ' + bars;
  gr.navEls[1].style.color = C.txt;
  const piano = t !== 0;
  gr.viewCap.style.display = piano ? 'block' : 'none';
  gr.views.forEach((v, k) => { v.style.display = piano ? 'flex' : 'none'; v.setOn(['ALL', 'SCALE', 'USED'][k] === gr.view, C.txt); });
  gr.noteCap.textContent = piano ? 'NOTE' : 'HIT';
  gr.slide.e.style.display = t === 1 || t === 2 ? 'block' : 'none';
  gr.slide.o.color = c;
  gr.roll.e.style.display = t === 0 ? 'block' : 'none';
  if (piano) {
    const n = gr.notes[gr.sel];
    gr.vel.set(n ? n.v : 100);
    gr.chance.set(n ? n.c : 100);
    gr.slide.lit(n && n.sl, c);
  } else if (gr.drumSel && gr.gs && gr.gs.hits) {
    const h = gr.gs.hits[gr.drumSel[0]][gr.drumSel[1]];
    if (h) { gr.vel.set(h[0] || 100); gr.roll.set(h[1]); gr.chance.set(h[2]); }
  }
  const help = piano
    ? ['TAP: A NOTE · DRAG: MOVE', 'DRAG ITS END: LENGTH', 'DOUBLE TAP: DELETE', 'DRAG THE KEYS: SCROLL']
    : ['TAP A STEP: HIT ON / OFF', 'LONG PRESS A HIT: SELECT IT', 'TAP A NAME: PLAY IT', 'VEL LANE: THAT LANE'];
  help.forEach((s2, k) => { gr.help[k].textContent = s2; });
  const cyc = gr.gs ? gr.gs.cycle : 0;
  gr.len.textContent = cyc ? 'LENGTH ' + cyc + ' STEPS' : 'LENGTH: THE LOOP';
}

function updateGrid() {
  if (!gr.tracks) return;
  refreshGrid(false);
  updateGridPanel();
}
function updateGridFast() {
  if (page === 'GRID') {
    if (gr.gsVer !== st.ver) refreshGrid(false);
    drawGrid();
  }
}
