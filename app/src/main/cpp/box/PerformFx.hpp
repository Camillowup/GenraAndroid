// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android: the PERFORM page's effects. They work on a bus (the tracks chosen under EFFECTS ON):
// pads you hold (or latch) switch effects on, the XY field moves their main parameters, a DJ EQ with kills follows.
// The time effects (REPEAT, TAPE STOP, REVERSE, HALF SPEED) play from a ring buffer of the bus; only the newest of them
// is heard. ECHO THROW and REVERB WASH are sends: their tails ring on after the pad is let go.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gbox {

enum FxType {
	FX_REP4, FX_REP8, FX_REP16, FX_REP32,
	FX_LP, FX_HP, FX_BP, FX_WOBBLE,
	FX_TAPESTOP, FX_REVERSE, FX_HALF, FX_GATE,
	FX_CRUSH, FX_ECHO, FX_VERB, FX_VINYL,
	NUM_FX
};
static const char* const FX_NAMES[NUM_FX] = {
    "1/4", "1/8", "1/16", "1/32", "LOW PASS", "HIGH PASS", "BAND", "WOBBLE",
    "TAPE STOP", "REVERSE", "HALF SPEED", "GATE", "BITCRUSH", "ECHO THROW", "REVERB WASH", "VINYL",
};
static const char* const FX_GROUP[NUM_FX] = {
    "REPEAT", "REPEAT", "REPEAT", "REPEAT", "FILTER", "FILTER", "FILTER", "FILTER",
    "TIME", "TIME", "TIME", "TIME", "COLOR", "COLOR", "COLOR", "COLOR",
};
static const char* const FX_XY[NUM_FX][2] = {
    {"LENGTH", "FADE"}, {"LENGTH", "FADE"}, {"LENGTH", "FADE"}, {"LENGTH", "FADE"},
    {"CUTOFF", "RESO"}, {"CUTOFF", "RESO"}, {"CUTOFF", "RESO"}, {"RATE", "DEPTH"},
    {"TIME", "-"}, {"LENGTH", "-"}, {"SPEED", "-"}, {"RATE", "DEPTH"},
    {"RATE", "BITS"}, {"TIME", "FEEDBACK"}, {"SIZE", "AMOUNT"}, {"WOW", "NOISE"},
};
inline bool fxIsTime(int t) { return t <= FX_REP32 || (t >= FX_TAPESTOP && t <= FX_HALF); }

/** A TPT state-variable filter (one channel). */
struct Svf {
	float ic1 = 0.f, ic2 = 0.f;
	void reset() { ic1 = ic2 = 0.f; }
	// mode 0 low, 1 high, 2 band
	float run(float x, float g, float k, int mode) {
		float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
		float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		if (!std::isfinite(ic1) || !std::isfinite(ic2))
			reset();
		return mode == 0 ? v2 : mode == 1 ? x - k * v1 - v2 : v1;
	}
};

class PerformFx {
public:
	int assign[16];
	bool held[16] = {}, latched[16] = {};
	uint32_t order[16] = {};         // when a pad went on (the newest time effect plays)
	float x = 0.5f, y = 0.5f;        // the XY field (0..1)
	bool xyTouch = false;
	float eq[3] = {0.5f, 0.5f, 0.5f};
	bool kill[3] = {};

	PerformFx() {
		for (int i = 0; i < 16; i++)
			assign[i] = i;
	}

	void setSampleRate(float s) {
		sr = s;
		ringN = (int) (sr * 8.f);
		ringL.assign(ringN, 0.f);
		ringR.assign(ringN, 0.f);
		dlyN = (int) (sr * 2.f);
		dlyL.assign(dlyN, 0.f);
		dlyR.assign(dlyN, 0.f);
		static const int COMB[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
		static const int AP[4] = {556, 441, 341, 225};
		float k = sr / 44100.f;
		for (int c = 0; c < 2; c++) {
			for (int i = 0; i < 8; i++)
				comb[c][i].init((int) ((COMB[i] + c * 23) * k));
			for (int i = 0; i < 4; i++)
				ap[c][i].init((int) ((AP[i] + c * 23) * k));
		}
		w = 0;
	}

	void press(int pad, bool down) {
		if (pad < 0 || pad >= 16)
			return;
		if (down) {
			held[pad] = true;
			order[pad] = ++clock;
		}
		else
			held[pad] = false;
	}
	void latch(int pad) {
		if (pad < 0 || pad >= 16)
			return;
		latched[pad] = !latched[pad];
		if (latched[pad])
			order[pad] = ++clock;
	}
	bool on(int pad) const { return held[pad] || latched[pad]; }
	bool anyOn() const {
		for (int i = 0; i < 16; i++)
			if (on(i))
				return true;
		return false;
	}

	/** One stereo frame of the bus. beatsPerSample: tempo; the sends' tails come back in sendL / sendR. */
	void process(float& L, float& R, float bpm, float& sendL, float& sendR) {
		float beat = 60.f / std::max(20.f, bpm) * sr;  // samples per beat
		// which effects are on; the newest time effect
		bool act[NUM_FX] = {};
		int timeFx = -1;
		uint32_t newest = 0;
		for (int p = 0; p < 16; p++)
			if (on(p)) {
				int t = assign[p];
				if (t < 0 || t >= NUM_FX)
					continue;
				act[t] = true;
				if (fxIsTime(t) && order[p] >= newest)
					newest = order[p], timeFx = t;
			}
		float X = xyTouch ? x : -1.f, Y = xyTouch ? y : -1.f;  // -1: the effect's default

		// the ring of the bus (time effects read from it)
		ringL[w] = L;
		ringR[w] = R;
		float tl = L, tr = R;
		if (timeFx != curTime) {  // a new time effect starts here
			curTime = timeFx;
			t0 = w;
			tpos = 0.0;
			tspeed = 1.f;
		}
		if (curTime >= 0) {
			int t = curTime;
			if (t <= FX_REP32) {
				static const float DIV[4] = {1.f, 0.5f, 0.25f, 0.125f};
				float len = beat * DIV[t] * (X >= 0.f ? std::pow(2.f, -2.f * X) : 1.f);
				int N = std::max(32, std::min(ringN / 2, (int) len));
				int el = (int) tpos;
				if (el >= N) {  // repeat the slice that was recorded at the press
					int k = el % N;
					float fade = Y >= 0.f ? std::pow(1.f - 0.6f * Y, (float) (el / N)) : 1.f;
					float edge = std::min(1.f, std::min(k, N - k) / (0.002f * sr));  // no click at the seam
					tl = at(ringL, t0 + k) * fade * edge;
					tr = at(ringR, t0 + k) * fade * edge;
				}
				tpos += 1.0;
			}
			else if (t == FX_TAPESTOP) {
				float time = 0.15f + 1.85f * (X >= 0.f ? X : 0.3f);  // seconds to standstill
				tspeed = std::max(0.f, tspeed - 1.f / (time * sr));
				tpos += tspeed;
				tl = lerp(ringL, t0 + tpos) * std::min(1.f, tspeed * 4.f);
				tr = lerp(ringR, t0 + tpos) * std::min(1.f, tspeed * 4.f);
			}
			else if (t == FX_REVERSE) {
				float len = beat * (X >= 0.f ? 0.25f + 1.75f * X : 1.f);
				int N = std::max(64, std::min(ringN / 2, (int) len));
				int k = ((int) tpos) % N;
				float edge = std::min(1.f, std::min(k, N - k) / (0.003f * sr));
				tl = at(ringL, t0 - k) * edge;
				tr = at(ringR, t0 - k) * edge;
				tpos += 1.0;
			}
			else if (t == FX_HALF) {
				float sp = X >= 0.f ? 0.25f + 0.5f * X : 0.5f;
				tpos += sp;
				if (tpos > ringN * 0.45)  // too far behind: catch up
					tpos = 0.0, t0 = w;
				tl = lerp(ringL, t0 + tpos);
				tr = lerp(ringR, t0 + tpos);
			}
		}
		// a short crossfade when a time effect comes or goes
		float wantT = curTime >= 0 ? 1.f : 0.f;
		tmix += (wantT - tmix) * std::min(1.f, 1.f / (0.004f * sr));
		L = L + (tl - L) * tmix;
		R = R + (tr - R) * tmix;
		w = (w + 1) % ringN;

		// filters
		if (act[FX_LP] || act[FX_HP] || act[FX_BP] || act[FX_WOBBLE]) {
			auto cut = [&](float def) { return X >= 0.f ? 40.f * std::pow(400.f, X) : def; };
			float k = 2.f - 1.85f * (Y >= 0.f ? Y : 0.35f);
			if (act[FX_LP]) {
				float g = gOf(cut(700.f));
				L = fl[0][0].run(L, g, k, 0), R = fl[0][1].run(R, g, k, 0);
			}
			if (act[FX_HP]) {
				float g = gOf(cut(1500.f));
				L = fl[1][0].run(L, g, k, 1), R = fl[1][1].run(R, g, k, 1);
			}
			if (act[FX_BP]) {
				float g = gOf(cut(1000.f));
				float kb = std::max(0.25f, k * 0.8f);
				L = fl[2][0].run(L, g, kb, 2) * 1.6f, R = fl[2][1].run(R, g, kb, 2) * 1.6f;
			}
			if (act[FX_WOBBLE]) {
				static const float RATES[4] = {1.f, 0.5f, 0.25f, 0.125f};  // beats per wobble
				float r = RATES[std::min(3, (int) ((X >= 0.f ? X : 0.33f) * 3.99f))];
				wob += 1.f / (beat * r);
				if (wob >= 1.f)
					wob -= 1.f;
				float depth = Y >= 0.f ? Y : 0.8f;
				float lfo = 0.5f - 0.5f * std::cos(wob * 6.2831853f);
				float hz = 80.f * std::pow(60.f, 1.f - depth + depth * lfo);
				float g = gOf(hz);
				L = fl[3][0].run(L, g, 0.5f, 0), R = fl[3][1].run(R, g, 0.5f, 0);
			}
		}
		else
			for (auto& f : fl)
				f[0].reset(), f[1].reset();
		// gate
		if (act[FX_GATE]) {
			static const float RATES[3] = {0.5f, 0.25f, 0.125f};
			float r = RATES[std::min(2, (int) ((X >= 0.f ? X : 0.5f) * 2.99f))];
			gph += 1.f / (beat * r);
			if (gph >= 1.f)
				gph -= 1.f;
			float depth = Y >= 0.f ? Y : 1.f;
			float g = gph < 0.5f ? 1.f : 1.f - depth;
			gsm += (g - gsm) * std::min(1.f, 1.f / (0.002f * sr));
			L *= gsm, R *= gsm;
		}
		else
			gsm = 1.f, gph = 0.f;
		// bitcrush
		if (act[FX_CRUSH]) {
			float hold = 1.f + 40.f * (X >= 0.f ? X : 0.4f);
			float bits = 3.f + 9.f * (Y >= 0.f ? 1.f - Y : 0.5f);
			float q = std::pow(2.f, bits);
			crPh += 1.f;
			if (crPh >= hold) {
				crPh -= hold;
				crL = std::round(L * q) / q;
				crR = std::round(R * q) / q;
			}
			L = crL, R = crR;
		}
		// vinyl: band-limited, a slow wobble in pitch, crackle
		if (act[FX_VINYL]) {
			float wow = X >= 0.f ? X : 0.4f, noise = Y >= 0.f ? Y : 0.4f;
			vph += 0.55f / sr;
			if (vph >= 1.f)
				vph -= 1.f;
			float d = (0.004f + 0.003f * wow * std::sin(vph * 6.2831853f)) * sr;  // a short moving delay
			float vl = lerp(ringL, (double) w - 1 - d), vr = lerp(ringR, (double) w - 1 - d);
			if (curTime >= 0)
				vl = L, vr = R;
			float g = gOf(4500.f), gh = gOf(120.f);
			vl = vf[0].run(vl, g, 1.2f, 0);
			vr = vf[1].run(vr, g, 1.2f, 0);
			vl = vf[2].run(vl, gh, 1.4f, 1);
			vr = vf[3].run(vr, gh, 1.4f, 1);
			rnd = rnd * 1664525u + 1013904223u;
			float n = ((rnd >> 9) * (1.f / 8388608.f)) - 1.f;
			float crackle = (rnd % 9000u) < (uint32_t) (6 + 30 * noise) ? n * 0.6f : 0.f;
			L = vl * 0.9f + crackle * noise + n * 0.004f * noise;
			R = vr * 0.9f + crackle * noise + n * 0.004f * noise;
		}

		// sends: ECHO THROW and REVERB WASH (the tails stay after the pad)
		float eIn = act[FX_ECHO] ? 1.f : 0.f, vIn = act[FX_VERB] ? 1.f : 0.f;
		echoAmt += (eIn - echoAmt) * std::min(1.f, 1.f / (0.01f * sr));
		verbAmt += (vIn - verbAmt) * std::min(1.f, 1.f / (0.02f * sr));
		{
			static const float DIVS[3] = {0.5f, 0.75f, 1.f};
			float div = DIVS[std::min(2, (int) ((act[FX_ECHO] && X >= 0.f ? X : 0.5f) * 2.99f))];
			float fb = 0.35f + 0.5f * (act[FX_ECHO] && Y >= 0.f ? Y : 0.4f);
			if (act[FX_ECHO])
				echoFb = fb, echoDiv = div;
			int D = std::max(1, std::min(dlyN - 1, (int) (beat * echoDiv)));
			int rp = (dw - D + dlyN) % dlyN;
			float el = dlyL[rp], er = dlyR[rp];
			// ping-pong
			dlyL[dw] = L * echoAmt * 0.8f + er * echoFb;
			dlyR[dw] = R * echoAmt * 0.8f + el * echoFb;
			dw = (dw + 1) % dlyN;
			sendL = el, sendR = er;
		}
		{
			if (act[FX_VERB])
				verbSize = 0.7f + 0.28f * (Y >= 0.f ? Y : 0.6f), verbDamp = 0.2f + 0.5f * (X >= 0.f ? 1.f - X : 0.5f);
			float in = (L + R) * 0.5f * verbAmt * 0.6f;
			float o[2] = {0.f, 0.f};
			verbIdle = verbAmt > 1e-5f ? 0 : verbIdle + 1;  // the reverb rests once its tail is over (CPU)
			for (int c = 0; c < 2 && verbIdle < (int) (sr * 8.f); c++) {
				for (int i = 0; i < 8; i++)
					o[c] += comb[c][i].run(in, verbSize, verbDamp);
				for (int i = 0; i < 4; i++)
					o[c] = ap[c][i].run(o[c]);
			}
			sendL += o[0] * 0.25f;
			sendR += o[1] * 0.25f;
		}
		// the duck of the bus while REVERB WASH / ECHO THROW are held is left out: the throw plays on top
		// DJ EQ with kills (on the bus)
		eqLp1L += (L - eqLp1L) * cLo, eqLp1R += (R - eqLp1R) * cLo;
		eqLp2L += (L - eqLp2L) * cHi, eqLp2R += (R - eqLp2R) * cHi;
		float lowL = eqLp1L, lowR = eqLp1R, highL = L - eqLp2L, highR = R - eqLp2R;
		float midL = L - lowL - highL, midR = R - lowR - highR;
		float gl = bandGain(0), gm = bandGain(1), gh = bandGain(2);
		L = lowL * gl + midL * gm + highL * gh;
		R = lowR * gl + midR * gm + highR * gh;
		if (!std::isfinite(L) || !std::isfinite(R))
			L = R = 0.f;
	}

	void prepareEq() {
		cLo = 1.f - std::exp(-6.2831853f * 250.f / sr);
		cHi = 1.f - std::exp(-6.2831853f * 2800.f / sr);
	}

private:
	float sr = 48000.f;
	std::vector<float> ringL, ringR, dlyL, dlyR;
	int ringN = 1, w = 0, dlyN = 1, dw = 0;
	int curTime = -1, t0 = 0;
	double tpos = 0.0;
	float tspeed = 1.f, tmix = 0.f;
	uint32_t clock = 0;
	Svf fl[4][2], vf[4];
	float wob = 0.f, gph = 0.f, gsm = 1.f, crPh = 0.f, crL = 0.f, crR = 0.f, vph = 0.f;
	uint32_t rnd = 22222u;
	int verbIdle = 1 << 30;
	float echoAmt = 0.f, verbAmt = 0.f, echoFb = 0.5f, echoDiv = 0.75f, verbSize = 0.85f, verbDamp = 0.4f;
	float eqLp1L = 0.f, eqLp1R = 0.f, eqLp2L = 0.f, eqLp2R = 0.f, cLo = 0.03f, cHi = 0.3f;
	struct Comb {
		std::vector<float> b;
		int i = 0;
		float st = 0.f;
		void init(int n) { b.assign(std::max(1, n), 0.f), i = 0, st = 0.f; }
		float run(float x, float fb, float damp) {
			float o = b[i];
			st = o * (1.f - damp) + st * damp;
			b[i] = x + st * fb;
			if (++i >= (int) b.size())
				i = 0;
			return o;
		}
	} comb[2][8];
	struct Ap {
		std::vector<float> b;
		int i = 0;
		void init(int n) { b.assign(std::max(1, n), 0.f), i = 0; }
		float run(float x) {
			float o = b[i];
			b[i] = x + o * 0.5f;
			if (++i >= (int) b.size())
				i = 0;
			return o - x;
		}
	} ap[2][4];

	float gOf(float hz) const { return std::tan(3.14159265f * std::min(hz, sr * 0.45f) / sr); }
	float at(const std::vector<float>& r, int i) const { return r[((i % ringN) + ringN) % ringN]; }
	float lerp(const std::vector<float>& r, double pos) const {
		double p = std::fmod(pos, (double) ringN);
		if (p < 0)
			p += ringN;
		int i = (int) p;
		float f = (float) (p - i);
		return r[i] + (r[(i + 1) % ringN] - r[i]) * f;
	}
	/** EQ knob 0..1: 0 = off, 0.5 = flat, 1 = +6 dB; a kill takes the band away. */
	float bandGain(int b) const {
		if (kill[b])
			return 0.f;
		float k = eq[b];
		return k < 0.5f ? (k * 2.f) * (k * 2.f) : 1.f + (k - 0.5f) * 2.f;
	}
};

}  // namespace gbox
