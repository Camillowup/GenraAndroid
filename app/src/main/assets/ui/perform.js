// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android – the PERFORM page: 16 effect pads (hold; double tap latches; long press picks the effect),
// the XY field, the tracks the effects work on, mutes, the DJ EQ with kills, FILL and BREAK to hold.
'use strict';

const FX_COLOR = { REPEAT: '#ff5d6c', FILTER: '#3dc9ff', TIME: '#7b6cff', COLOR: '#e0779e' };
const pf = { pads: [], lastPad: -1 };

function buildPerform() {
  const p = pages.PERFORM = el('div', 'page', stage);
  header(p, 'PERFORM');
  plate(p, 24, 112, 1260, 1056);
  text(p, 50, 132, 'HOLD: THE EFFECT PLAYS · DOUBLE TAP: IT STAYS ON · LONG PRESS: CHOOSE THE EFFECT', 16, C.dim, { weight: 600, sp: 2 });
  for (let i = 0; i < 16; i++) {
    const r = Math.floor(i / 4), c = i % 4;
    const x = 60 + c * 292, y = 176 + r * 218;
    const e = el('div', 'abs', p, { left: px(x), top: px(y), width: '272px', height: '196px', borderRadius: '16px', border: '2px solid #34363f', background: '#2b2d35', touchAction: 'none' });
    const grp = el('div', 'tc', e, { left: '50%', top: '26px', fontSize: '16px', fontWeight: 600, letterSpacing: '3px' });
    const nm = el('div', 'tc', e, { left: '50%', top: '80px', fontSize: '26px', letterSpacing: '2px' });
    const lt = el('div', 'tc', e, { left: '50%', top: '138px', fontSize: '15px', fontWeight: 600, letterSpacing: '2px' });
    const P = { e, grp, nm, lt, down: false, timer: 0, lastTap: 0, id: -1, longDone: false };
    e.addEventListener('pointerdown', ev => {
      ev.preventDefault();
      try { e.setPointerCapture(ev.pointerId); } catch (x) {}
      const now = performance.now();
      if (now - P.lastTap < 300) {  // double tap: latch
        P.lastTap = 0;
        cmd(OP.PF_LATCH, i);
        return;
      }
      P.lastTap = now;
      P.down = true; P.id = ev.pointerId; P.longDone = false;
      pf.lastPad = i;
      cmd(OP.PF_PAD, i, 1);
      clearTimeout(P.timer);
      P.timer = setTimeout(() => {  // long press: choose the effect of this pad
        if (!P.down) return;
        P.longDone = true;
        P.down = false;
        cmd(OP.PF_PAD, i, 0);
        pickFx(i);
      }, 650);
    });
    const up = ev => {
      if (ev.pointerId !== P.id) return;
      clearTimeout(P.timer);
      if (P.down) cmd(OP.PF_PAD, i, 0);
      P.down = false;
    };
    e.addEventListener('pointerup', up);
    e.addEventListener('pointercancel', up);
    pf.pads.push(P);
  }

  // the XY field
  plate(p, 1276, 112, 1900, 1056);
  pf.xyTitle = text(p, 1300, 132, 'XY', 17, C.dim, { weight: 600, sp: 2 });
  const xy = pf.xy = el('div', 'abs', p, { left: '1300px', top: '170px', width: '576px', height: '720px', background: '#0d0e11', border: '3px solid #2c2e36', borderRadius: '14px', touchAction: 'none', overflow: 'hidden' });
  for (let k = 1; k < 8; k++) {
    el('div', 'abs', xy, { left: px(k * 72), top: '0px', width: '2px', height: '720px', background: '#16171c' });
    el('div', 'abs', xy, { left: '0px', top: px(k * 90), width: '576px', height: '2px', background: '#16171c' });
  }
  pf.xLab = text(xy, 12, 690, 'X', 16, C.dim, { weight: 600, sp: 2 });
  pf.yLab = text(xy, 330, 12, 'Y', 16, C.dim, { weight: 600, sp: 2 });
  pf.ball = el('div', 'abs', xy, { width: '64px', height: '64px', borderRadius: '50%', background: '#ff5d6c', boxShadow: '0 0 0 22px rgba(255,93,108,0.25)', display: 'none' });
  let xyId = -1, last = 0;
  const move = (ev, force) => {
    const r = xy.getBoundingClientRect();
    const x = clamp((ev.clientX - r.left) / r.width, 0, 1), y = clamp(1 - (ev.clientY - r.top) / r.height, 0, 1);
    pf.ball.style.display = 'block';
    pf.ball.style.left = px(x * 576 - 32);
    pf.ball.style.top = px((1 - y) * 720 - 32);
    const now = performance.now();
    if (force || now - last > 25) { last = now; cmd(OP.PF_X, 0, 0, x); cmd(OP.PF_Y, 0, 0, y); }
  };
  xy.addEventListener('pointerdown', ev => { ev.preventDefault(); xyId = ev.pointerId; try { xy.setPointerCapture(xyId); } catch (x) {} move(ev, true); });
  xy.addEventListener('pointermove', ev => { if (ev.pointerId === xyId) move(ev, false); });
  const xyUp = ev => { if (ev.pointerId !== xyId) return; xyId = -1; pf.ball.style.display = 'none'; cmd(OP.PF_XYUP); };
  xy.addEventListener('pointerup', xyUp);
  xy.addEventListener('pointercancel', xyUp);
  text(p, 1300, 914, 'THE XY FIELD MOVES THE EFFECTS THAT PLAY', 16, C.dim, { weight: 600, sp: 2 });
  text(p, 1300, 944, 'LIFT THE FINGER: BACK TO THEIR SETTING', 16, C.dim, { weight: 600, sp: 2 });

  // right: targets, mutes, EQ, FILL / BREAK
  plate(p, 1916, 112, 2376, 1056);
  cap(p, 1940, 132, 'EFFECTS ON');
  pf.targets = [];
  const tg = [['ALL', C.txt, -1], ['DRUM', TC[0], 0], ['BASS', TC[1], 1], ['MEL', TC[2], 2], ['PADS', TC[3], 3]];
  tg.forEach(([lab, col, bit], i) => {
    const x = 1940 + i * 86;
    const c = chip(p, x, 164, x + 78, 222, lab, () => {
      const m = st.pf.target;
      cmd(OP.PF_TARGET, bit < 0 ? (m === 15 ? 0 : 15) : m ^ (1 << bit));
    }, { color: col, border: col });
    c.style.fontSize = '16px';
    pf.targets.push([c, col, bit]);
  });
  cap(p, 1940, 256, 'MUTE');
  pf.mutes = [];
  ['DRUM', 'BASS', 'MEL', 'PADS'].forEach((lab, t) => {
    const x = 1940 + t * 108;
    const c = chip(p, x, 288, x + 96, 370, lab, () => cmd(OP.MUTE, t), { color: TC[t], border: TC[t] });
    c.style.fontSize = '18px';
    pf.mutes.push(c);
  });
  cap(p, 1940, 404, 'DJ EQ');
  pf.eq = ['LOW', 'MID', 'HIGH'].map((lab, b) => new Knob(p, 2010 + b * 136, 490, 40, lab, {
    def: 0.5, title: 'EQ ' + lab, fmt: v => v < 0.01 ? 'OFF' : v < 0.5 ? (20 * Math.log10((v * 2) ** 2)).toFixed(1) + ' dB' : '+' + (20 * Math.log10(1 + (v - 0.5) * 2)).toFixed(1) + ' dB',
    change: v => cmd(OP.PF_EQ, b, 0, v),
  }));
  cap(p, 1940, 600, 'KILL');
  pf.kill = ['LOW', 'MID', 'HIGH'].map((lab, b) => {
    const x = 1940 + b * 144;
    const c = chip(p, x, 630, x + 132, 690, lab, () => cmd(OP.PF_KILL, b));
    c.style.fontSize = '18px';
    return c;
  });
  const hold = (b, op) => {
    b.e.addEventListener('pointerdown', () => cmd(op, 0, 1));
    const off = () => cmd(op, 0, 0);
    b.e.addEventListener('pointerup', off);
    b.e.addEventListener('pointercancel', off);
  };
  pf.fill = new Btn(p, 2030, 830, 62, 'FILL', { color: '#ff5d6c' });
  pf.brk = new Btn(p, 2250, 830, 62, 'BREAK', { color: C.white });
  hold(pf.fill, OP.PF_FILL);
  hold(pf.brk, OP.PF_BREAK);
  [pf.fill, pf.brk].forEach(b => {
    b.e.addEventListener('pointerdown', () => b.lit(true));
    const off = () => b.lit(false);
    b.e.addEventListener('pointerup', off);
    b.e.addEventListener('pointercancel', off);
  });
  text(p, 1940, 960, 'FILL / BREAK: ON WHILE HELD', 15, C.dim, { weight: 600, sp: 1 });
  text(p, 1940, 986, 'BREAK: ONLY DRUMS AND PADS', 15, C.dim, { weight: 600, sp: 1 });
}

function pickFx(pad) {
  const groups = [];
  ['REPEAT', 'FILTER', 'TIME', 'COLOR'].forEach(g => {
    groups.push({ name: g, items: meta.fx.map((f, i) => ({ label: f.name, sub: 'X ' + f.x + (f.y !== '-' ? ' · Y ' + f.y : ''), value: i, color: FX_COLOR[g] })).filter(it => meta.fx[it.value].group === g) });
  });
  groups.push({ name: 'ALL PADS', items: [{ label: 'BACK TO THE START', sub: 'THE 16 EFFECTS AS AT THE START', value: -1 }] });
  openSheet('PAD ' + (pad + 1) + ' · EFFECT', groups, st.pf.assign[pad], v => {
    if (v < 0) cmd(OP.PF_RESET);
    else cmd(OP.PF_ASSIGN, pad, v);
  });
}

function updatePerform() {
  if (!pf.pads.length) return;
  pf.pads.forEach((P, i) => {
    const f = meta.fx[st.pf.assign[i]] || { name: '?', group: 'COLOR' };
    P.grp.textContent = f.group;
    P.grp.style.color = FX_COLOR[f.group];
    P.nm.textContent = f.name;
    P.col = FX_COLOR[f.group];
  });
  const m = st.pf.target;
  pf.targets.forEach(([c, col, bit]) => c.setOn(bit < 0 ? m === 15 : (m >> bit) & 1, col));
  pf.mutes.forEach((c, t) => { const on = st.tracks[t].mute; c.style.background = on ? '#5a2028' : '#2b2d35'; });
  pf.eq.forEach((k, b) => k.set(st.pf.eq[b]));
  pf.kill.forEach((c, b) => c.setOn(st.pf.kill[b], C.red));
}

function updatePerformFast() {
  if (!fst.pfOn || !pf.pads.length) return;
  let newest = -1;
  pf.pads.forEach((P, i) => {
    const s = fst.pfOn[i];  // 0 off, 1 held, 2 latched
    const col = P.col || C.txt;
    const bg = s === 1 ? col : '#2b2d35';
    if (P._bg !== bg) { P._bg = bg; P.e.style.background = bg; }
    P.e.style.borderColor = s === 1 ? C.white : s === 2 ? col : '#34363f';
    P.e.style.borderWidth = s ? '5px' : '2px';
    P.nm.style.color = s === 1 ? '#111216' : C.txt;
    P.grp.style.color = s === 1 ? '#111216' : col;
    P.lt.textContent = s === 2 ? 'LATCHED' : '';
    P.lt.style.color = col;
    if (s) newest = i;
  });
  // the XY labels follow the effect that was pressed last (or any that plays)
  const show = pf.lastPad >= 0 && fst.pfOn[pf.lastPad] ? pf.lastPad : newest;
  if (show >= 0) {
    const f = meta.fx[st.pf.assign[show]];
    pf.xyTitle.textContent = 'XY · ' + f.name;
    pf.xLab.textContent = 'X: ' + f.x;
    pf.yLab.textContent = f.y !== '-' ? 'Y: ' + f.y : '';
  } else {
    pf.xyTitle.textContent = 'XY · HOLD A PAD';
    pf.xLab.textContent = 'X';
    pf.yLab.textContent = 'Y';
  }
}
