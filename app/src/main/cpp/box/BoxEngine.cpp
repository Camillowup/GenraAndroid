// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android: the engine. The sequencer follows GENRA's (src/genra/GenraModule.hpp in Genra-for-Vcv: the
// tick, swing, ratchets, fills, MUTATE per pass, the arranger's cues and song rows), the voices follow GENRA BOX
// (src/box/GenraBox.cpp, BoxVoices.hpp) and the pads follow ABLAUF MINI (src/box/AblaufMini.cpp).
#include "BoxEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../genra-src/GenreMatch.hpp"

extern "C" const jdx_api* jdx_get(void);

namespace gbox {

template <typename T>
static T clampv(T x, T a, T b) { return x < a ? a : (x > b ? b : x); }
static float c01(float x) { return clampv(x, 0.f, 1.f); }
/** A macro at 0.5 leaves the value as it is; 0 / 1 move it by -range/2 / +range/2 (GENRA BOX). */
static float mac(float base, float m, float range) { return c01(base + (m - 0.5f) * range); }
static float mac2(float base, float m, float down, float up) {
	return c01(base + (m < 0.5f ? (m - 0.5f) * 2.f * down : (m - 0.5f) * 2.f * up));
}
static float gainOf(float knob) {
	float g = knob / 0.75f;  // 0.75 = 0 dB, quadratic as on MIX
	return g * g;
}

static const int LENGTHS[4] = {1, 2, 4, 8};
static const char* const LANE_NAMES[JDX_NLANE] = {"KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL"};
static const char* const SND_NAMES[JDX_NSND] = {"PITCH", "PMOD", "DECAY", "NOISE", "COLOR"};
// GENRA's 8 drum tracks onto DRUM-X's lanes (as DRUM-X's GENRA input does)
static const int TO_LANE[NUM_DRUMS] = {0, 1, 2, 6, 3, 4, 5, 7};

// ---------------------------------------------------------------- the synth pages' parameters (preset value order)
static std::vector<std::string> names(const char* const* a, int n) {
	std::vector<std::string> v;
	for (int i = 0; i < n; i++)
		v.push_back(a[i]);
	return v;
}
static const std::vector<ParamDef>& monoDefs() {
	static std::vector<ParamDef> d = {
	    {"wave1", "WAVE 1", 0, 1, .33f, 0, {}}, {"wave2", "WAVE 2", 0, 1, .33f, 0, {}},
	    {"tune1", "TUNE 1", -1, 1, 0, 0, {}}, {"tune2", "TUNE 2", -1, 1, 0, 0, {}},
	    {"level1", "LEVEL 1", 0, 1, .8f, 0, {}}, {"level2", "LEVEL 2", 0, 1, .8f, 0, {}},
	    {"glide", "GLIDE", 0, 1, 0, 0, {}},
	    {"ftype", "TYPE", 0, gmono::NUM_FILTERS - 1, 0, 1, names(gmono::FILTER_NAMES, gmono::NUM_FILTERS)},
	    {"cutoff", "CUTOFF", 0, 1, .5f, 0, {}}, {"res", "RESO", 0, 1, 0, 0, {}},
	    {"egamt", "EG AMT", -1, 1, .3f, 0, {}}, {"drive", "DRIVE", 0, 1, 0, 0, {}},
	    {"keytrk", "KEYTRK", 0, 1, 0, 0, {}},
	    {"fa", "ATTACK", 0, 1, 0, 0, {}}, {"fd", "DECAY", 0, 1, .4f, 0, {}},
	    {"fs", "SUSTAIN", 0, 1, 0, 0, {}}, {"fr", "RELEASE", 0, 1, .3f, 0, {}},
	    {"aa", "ATTACK", 0, 1, 0, 0, {}}, {"ad", "DECAY", 0, 1, .4f, 0, {}},
	    {"as", "SUSTAIN", 0, 1, 1, 0, {}}, {"ar", "RELEASE", 0, 1, .3f, 0, {}},
	    {"rate", "RATE", 0, 1, .4f, 0, {}}, {"lfotri", "WAVE", 0, 1, 1, 2, {"SQR", "TRI"}},
	    {"lfopitch", "> PITCH", 0, 1, 0, 0, {}}, {"lfofilt", "> FILTER", 0, 1, 0, 0, {}},
	    {"accent", "ACCENT", 0, 1, .5f, 0, {}}, {"width", "WIDTH", 0, 1, 0, 0, {}},
	};
	return d;
}
static const std::vector<ParamDef>& funkeDefs() {
	static const char* const MODES[3] = {"MONO", "LEGATO", "POLY"};
	static std::vector<ParamDef> d = {
	    {"model", "MODEL", 0, gfunke::NUM_MODELS - 1, 0, 1, names(gfunke::MODEL_NAMES, gfunke::NUM_MODELS)},
	    {"timbre", "TIMBRE", 0, 1, .5f, 0, {}}, {"bite", "BITE", 0, 1, .4f, 0, {}},
	    {"width", "WIDTH", 0, 1, .4f, 0, {}}, {"sub", "SUB", 0, 1, 0, 0, {}},
	    {"mode", "VOICES", 0, 2, 2, 1, names(MODES, 3)},
	    {"glide", "GLIDE", 0, 1, .3f, 0, {}}, {"accent", "ACCENT", 0, 1, .5f, 0, {}},
	    {"ftype", "TYPE", 0, gmono::NUM_FILTERS - 1, 0, 1, names(gmono::FILTER_NAMES, gmono::NUM_FILTERS)},
	    {"cutoff", "CUTOFF", 0, 1, .7f, 0, {}}, {"res", "RESO", 0, 1, .15f, 0, {}},
	    {"fenv", "F.ENV", -1, 1, .3f, 0, {}}, {"key", "KEY", 0, 1, .5f, 0, {}},
	    {"fatk", "F.ATK", 0, 1, 0, 0, {}}, {"fdec", "F.DEC", 0, 1, .45f, 0, {}},
	    {"drive", "DRIVE", 0, 1, 0, 0, {}},
	    {"atk", "ATTACK", 0, 1, 0, 0, {}}, {"dec", "DECAY", 0, 1, .5f, 0, {}},
	    {"sus", "SUSTAIN", 0, 1, 1, 0, {}}, {"rel", "RELEASE", 0, 1, .55f, 0, {}},
	    {"vdepth", "DEPTH", 0, 1, 0, 0, {}}, {"vrate", "RATE", 0, 1, .5f, 0, {}},
	    {"vdelay", "DELAY", 0, 1, .4f, 0, {}},
	    {"div", "ECHO TIME", 0, gfunke::NUM_DIVS - 1, 2, 1, names(gfunke::DIV_NAMES, gfunke::NUM_DIVS)},
	    {"chorus", "CHORUS", 0, 1, .2f, 0, {}}, {"echo", "ECHO", 0, 1, .2f, 0, {}},
	    {"fdbk", "FEEDBACK", 0, 1, .35f, 0, {}}, {"lofi", "LO-FI", 0, 1, 0, 0, {}},
	    {"verb", "REVERB", 0, 1, .2f, 0, {}},
	};
	return d;
}
static const std::vector<ParamDef>& schmelzDefs() {
	static const char* const OCTS[3] = {"BASE", "BOTH", "8VA"};
	static const char* const SPLITS[3] = {"LAYER", "LOW", "HIGH"};
	static std::vector<ParamDef> d = {
	    {"reg", "REGISTER", 0, 1, .29f, 0, {}}, {"cresc", "CRESC", 0, 1, .3f, 0, {}},
	    {"rel", "RELEASE", 0, 1, .45f, 0, {}}, {"tone", "TONE", 0, 1, .5f, 0, {}},
	    {"ensdepth", "DEPTH", 0, 1, .7f, 0, {}},
	    {"oct", "OCTAVE", 0, 2, 0, 1, names(OCTS, 3)},
	    {"ens", "ENSEMBLE", 0, gschmelz::NUM_ENS - 1, 2, 1, {"OFF", "ENS", "ENS+CHO", "CHORUS"}},
	    {"sound", "SOUND", 0, 1, .25f, 0, {}}, {"decay", "DECAY", 0, 1, .5f, 0, {}},
	    {"sustain", "SUSTAIN", 0, 1, .6f, 0, {}}, {"trem", "TREMOLO", 0, 1, 0, 0, {}},
	    {"split", "SPLIT", 0, 2, 0, 1, names(SPLITS, 3)},
	    {"atk", "ATTACK", 0, 1, 0, 0, {}}, {"trate", "T.RATE", 0, 1, .4f, 0, {}},
	    {"vel", "VELOCITY", 0, 1, .5f, 0, {}}, {"hold", "HOLD", 0, 1, 0, 2, {"OFF", "HOLD"}},
	    {"bal", "BALANCE", 0, 1, .5f, 0, {}}, {"anim", "ANIMATE", 0, 1, 0, 0, {}},
	    {"arate", "A.RATE", 0, 1, .3f, 0, {}}, {"phaser", "PHASER", 0, 1, 0, 0, {}},
	    {"prate", "P.RATE", 0, 1, .3f, 0, {}}, {"verb", "REVERB", 0, 1, .25f, 0, {}},
	    {"size", "SIZE", 0, 1, .6f, 0, {}}, {"volume", "VOLUME", 0, 1, .7f, 0, {}},
	};
	return d;
}
static const std::vector<ParamDef>& defsOf(int t) {
	static std::vector<ParamDef> none;
	return t == T_BASS ? monoDefs() : t == T_MELODY ? funkeDefs() : t == T_PADS ? schmelzDefs() : none;
}

// ---------------------------------------------------------------- JSON
static void jstr(std::string& o, const std::string& s) {
	o += '"';
	for (unsigned char c : s) {
		if (c == '"' || c == '\\') {
			o += '\\';
			o += (char) c;
		}
		else if (c < 0x20) {
			char b[8];
			std::snprintf(b, sizeof b, "\\u%04x", c);
			o += b;
		}
		else
			o += (char) c;
	}
	o += '"';
}
static void jnum(std::string& o, double v) {
	char b[32];
	if (!std::isfinite(v))
		v = 0;
	std::snprintf(b, sizeof b, "%.4g", v);
	o += b;
}
static void jint(std::string& o, long long v) { o += std::to_string(v); }

// ---------------------------------------------------------------- binary storage
struct Writer {
	std::vector<uint8_t> d;
	void u32(uint32_t v) {
		for (int i = 0; i < 4; i++)
			d.push_back((uint8_t) (v >> (8 * i)));
	}
	void i32(int32_t v) { u32((uint32_t) v); }
	void f32(float f) {
		uint32_t v;
		std::memcpy(&v, &f, 4);
		u32(v);
	}
	void bytes(const std::vector<uint8_t>& b) {
		u32((uint32_t) b.size());
		d.insert(d.end(), b.begin(), b.end());
	}
	void str(const std::string& s) { bytes(std::vector<uint8_t>(s.begin(), s.end())); }
};
struct Reader {
	const std::vector<uint8_t>& d;
	size_t p = 0;
	bool ok = true;
	explicit Reader(const std::vector<uint8_t>& v) : d(v) {}
	uint32_t u32() {
		if (p + 4 > d.size()) {
			ok = false;
			return 0;
		}
		uint32_t v = 0;
		for (int i = 0; i < 4; i++)
			v |= (uint32_t) d[p + i] << (8 * i);
		p += 4;
		return v;
	}
	int32_t i32() { return (int32_t) u32(); }
	float f32() {
		uint32_t v = u32();
		float f;
		std::memcpy(&f, &v, 4);
		return std::isfinite(f) ? f : 0.f;
	}
	std::vector<uint8_t> bytes() {
		uint32_t n = u32();
		if (!ok || p + n > d.size()) {
			ok = false;
			return {};
		}
		std::vector<uint8_t> b(d.begin() + p, d.begin() + p + n);
		p += n;
		return b;
	}
	std::string str() {
		std::vector<uint8_t> b = bytes();
		return std::string(b.begin(), b.end());
	}
};
static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f)
		return false;
	out.clear();
	uint8_t buf[8192];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
		out.insert(out.end(), buf, buf + n);
	std::fclose(f);
	return true;
}
static bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
	std::string tmp = path + ".tmp";
	FILE* f = std::fopen(tmp.c_str(), "wb");
	if (!f)
		return false;
	bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
	ok = std::fclose(f) == 0 && ok;
	if (!ok)
		return false;
	return std::rename(tmp.c_str(), path.c_str()) == 0;
}
static void putSlot(Writer& w, const Slot& s) {
	w.u32(s.used ? 1 : 0);
	if (!s.used)
		return;
	w.bytes(savePattern(s.pat));
	for (int t = 0; t < NUM_TRACKS; t++)
		w.i32(s.sel[t]);
	w.i32(s.lengthIdx);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.f32(s.mods.dens[t]);
	w.f32(s.mods.ghost);
	w.f32(s.mods.fill);
	w.i32(s.tag);
	w.str(std::string(s.name, strnlen(s.name, sizeof s.name)));
	w.i32(s.family);
}
static bool getSlot(Reader& r, Slot& s) {
	s = Slot();
	if (!r.u32())
		return r.ok;
	if (!loadPattern(r.bytes(), s.pat))
		return false;
	for (int t = 0; t < NUM_TRACKS; t++)
		s.sel[t] = (int8_t) r.i32();
	s.lengthIdx = (int8_t) clampv(r.i32(), 0, 3);
	for (int t = 0; t < NUM_TRACKS; t++)
		s.mods.dens[t] = r.f32();
	s.mods.ghost = r.f32();
	s.mods.fill = r.f32();
	s.tag = (int8_t) r.i32();
	std::string n = r.str();
	std::memset(s.name, 0, sizeof s.name);
	std::memcpy(s.name, n.data(), std::min(n.size(), sizeof s.name - 1));
	s.family = (int8_t) r.i32();
	s.used = true;
	return r.ok;
}
static void setName(Slot& s, const std::string& n) {
	std::memset(s.name, 0, sizeof(s.name));
	std::memcpy(s.name, n.data(), std::min(n.size(), sizeof(s.name) - 1));
}

// ================================================================ setup
Engine::Engine() {
	monoV.reset(new gmono::Voice);
	funkeE.reset(new gfunke::Engine);
	sv.reset(new gschmelz::StringsVoice[NP]);
	so.reset(new gschmelz::SoloVoice[NP]);
	sbus.reset(new gschmelz::Bus);
	for (int c = 0; c < NP; c++)
		sv[c].seed(0x1234u * (c + 1)), so[c].seed(0x777u * (c + 3));
	initPattern(pat);
}
Engine::~Engine() {}

void Engine::init(const std::string& filesDir) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (ready)
		return;
	dir = filesDir;
	initGenres();
	rng = Rng(0x9E3779B97F4A7C15ULL ^ (uint64_t) (uintptr_t) this ^ (uint64_t) std::time(nullptr));
	dx = jdx_get();
	if (dx && dx->api == JDX_API_VERSION)
		dx->init(rng.next() | 1u);
	else
		dx = nullptr;
	// the presets that fit each genre (GenreMatch), for SOUND: SAME GENRE
	for (int t = 1; t < NUM_TRACKS; t++) {
		sameGenre[t].assign(numProfiles(), std::vector<int>());
		for (int p = 0; p < numProfiles(); p++)
			sameGenre[t][p] = genrematch::candidates(factoryCount(t), [&](int i) {
				return t == T_BASS ? gmono::PRESETS[i].style : t == T_MELODY ? gfunke::PRESETS[i].style : gschmelz::PRESETS[i].style;
			}, prof(p).name);
	}
	loadUserPresets();
	setSampleRate(sr);
	if (!loadSession()) {
		// a fresh box: UK Garage, a pattern and the presets of its genre
		mainProfile = 0;
		for (int g = 0; g < NUM_GENRES; g++)
			if (numVariants(g) > 0 && std::string(prof(profileOf(g, 0)).name) == "UK Garage")
				mainProfile = profileOf(g, 0);
		bpm = prof(mainProfile).bpm;
		for (int t = 0; t < NUM_TRACKS; t++)
			loadPreset(t, 0);
		doGenerate(true);
		for (int t = 0; t < NUM_TRACKS; t++)
			rollPreset(t);
	}
	ready = true;
	changed();
}

void Engine::setSampleRate(float s) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	sr = s > 1000.f ? s : 48000.f;
	monoV->setSampleRate(sr);
	funkeE->setSampleRate(sr);
	sbus->setSampleRate(sr);
	for (int c = 0; c < NP; c++)
		sv[c].setSampleRate(sr), so[c].setSampleRate(sr);
	dxPos = 0.0;
}

// ================================================================ GENRA
int Engine::lengthSteps() const { return LENGTHS[clampv(lengthIdx, 0, 3)] * STEPS_PER_BAR; }

Params Engine::buildParams(bool withArr) {
	Params P;
	P.genre = mainProfile;
	P.key = clampv(key, 0, 11);
	P.scale = prof(pat.chordGenre).scale;
	P.scaleOverride = false;
	P.complexK = 0.5f;
	P.colorK = color;
	P.swingK = swing;
	P.lengthBars = lengthSteps() / STEPS_PER_BAR;
	for (int t = 0; t < NUM_TRACKS; t++)
		P.density[t] = density[t];
	P.fill = fillK;
	P.ghost = 0.3f;
	P.move = moveK;
	P.range = rangeK;
	P.voicing = voicingK;
	P.fillActive = fillActive;
	P.pass = passCount;
	return withArr ? withMods(P, arrMods) : P;
}

int Engine::trackTarget(int t) const {
	if (trackSel[t] >= 0)
		return clampv(trackSel[t], 0, numProfiles() - 1);
	return mainProfile;
}

void Engine::doGenerate(bool all) {
	Params P = buildParams();
	if (all)
		for (int t = 0; t < NUM_TRACKS; t++)
			trackSel[t] = -1;  // NEW ALL: the genre fields back to the main genre (as GENRA)
	if (!chordLock)
		generateChords(pat, mainProfile, rng, P);
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (!all && trackSel[t] >= 0)
			continue;
		generateTrack(pat, t, trackTarget(t), rng, P);
	}
	layoutTimeline(pat, lengthSteps());
	padHasPrev = false;
	patChanges++;
	changed();
}

void Engine::newTrack(int t) {
	Params P = buildParams();
	generateTrack(pat, t, trackTarget(t), rng, P);
	layoutTimeline(pat, lengthSteps());
	if (t == T_PADS)
		padHasPrev = false;
	patChanges++;
	changed();
}

// ================================================================ the sounds
int Engine::factoryCount(int t) const {
	switch (t) {
		case T_DRUMS: return dx ? std::max(1, (int) dx->nkits()) : 1;
		case T_BASS: return gmono::NUM_PRESETS;
		case T_MELODY: return gfunke::NUM_PRESETS;
		default: return gschmelz::NUM_PRESETS;
	}
}
int Engine::presetCount(int t) const { return factoryCount(t) + (int) user[t].size(); }
std::string Engine::presetName(int t, int i) const {
	int f = factoryCount(t);
	if (i >= f && i - f < (int) user[t].size())
		return user[t][i - f].name;
	switch (t) {
		case T_DRUMS: return dx ? dx->kit_name((uint32_t) clampv(i, 0, f - 1)) : "DRUM-X";
		case T_BASS: return gmono::PRESETS[clampv(i, 0, f - 1)].name;
		case T_MELODY: return gfunke::PRESETS[clampv(i, 0, f - 1)].name;
		default: return gschmelz::PRESETS[clampv(i, 0, f - 1)].name;
	}
}

void Engine::setDxParam(int j, int v) {
	if (j < 0 || j >= JDX_NP)
		return;
	dxVals[j] = clampv(v, 0, 127);
	if (!dx)
		return;
	if (j == JDX_TUNE || j == JDX_DECAY)
		dxApplyMacros();
	else if (j >= JDX_LN0 && j <= JDX_LN7)
		dx->set((uint32_t) j, laneMute[j - JDX_LN0] ? 0 : dxVals[j]);
	else
		dx->set((uint32_t) j, dxVals[j]);
}
void Engine::dxApplyMacros() {
	if (!dx)
		return;
	dx->set(JDX_TUNE, clampv((int) std::lround(dxVals[JDX_TUNE] + (macA[T_DRUMS] - 0.5f) * 100.f), 0, 127));
	dx->set(JDX_DECAY, clampv((int) std::lround(dxVals[JDX_DECAY] + (macB[T_DRUMS] - 0.5f) * 110.f), 0, 127));
}

/** A preset (factory or the user's) into track t's voice. */
void Engine::loadPreset(int t, int i) {
	int n = presetCount(t);
	i = clampv(i, 0, n - 1);
	preset[t] = i;
	int f = factoryCount(t);
	const UserPreset* up = i >= f ? &user[t][i - f] : nullptr;
	if (t == T_DRUMS) {
		if (!dx)
			return;
		if (up) {
			if (up->kit.size() == dx->kit_size())
				dx->kit_import(up->kit.data());
			for (int j = 0; j < JDX_NP && j < (int) up->vals.size(); j++)
				dxVals[j] = (int) up->vals[j];
		}
		else {
			int lvl = dx->get(JDX_LEVEL);
			dx->load_kit((uint32_t) i);
			for (int j = 0; j < JDX_NP; j++)
				dxVals[j] = dx->get((uint32_t) j);
			dxVals[JDX_LEVEL] = lvl;
		}
		for (int j = 0; j < JDX_NP; j++)
			setDxParam(j, dxVals[j]);
		dxOrigKit.resize(dx->kit_size());
		dx->kit_export(dxOrigKit.data());
		std::memcpy(dxOrigVals, dxVals, sizeof dxVals);
		dxKit = i;
	}
	else {
		const std::vector<ParamDef>& d = defsOf(t);
		std::vector<float> v(d.size(), 0.f);
		float g = 1.f;
		if (up) {
			for (size_t k = 0; k < v.size() && k < up->vals.size(); k++)
				v[k] = up->vals[k];
			g = up->gain;
		}
		else if (t == T_BASS) {
			gmono::PRESETS[i].values(v.data());
			g = gmono::PRESETS[i].gain;
		}
		else if (t == T_MELODY) {
			gfunke::PRESETS[i].values(v.data());
			g = gfunke::PRESETS[i].gain;
		}
		else {
			gschmelz::PRESETS[i].values(v.data());
			g = gschmelz::PRESETS[i].gain;
		}
		vals[t] = v;
		orig[t] = v;
		gain[t] = g;
		dirty[t] = true;
	}
	abOrig[t] = false;
	synUndo[t].clear();
	changed();
}

/** The values (and the box's macros on top) into the voice's parameters (control rate). */
void Engine::applyVals(int t) {
	if (!dirty[t] || vals[t].empty())
		return;
	dirty[t] = false;
	const std::vector<float>& v = vals[t];
	float a = macA[t], b = macB[t];
	if (t == T_BASS) {
		gmono::Params p;
		p.wave1 = v[0], p.wave2 = v[1], p.tune1 = v[2], p.tune2 = v[3], p.level1 = v[4], p.level2 = v[5];
		p.glide = v[6], p.ftype = clampv((int) std::lround(v[7]), 0, gmono::NUM_FILTERS - 1);
		p.cutoff = v[8], p.res = v[9], p.egAmt = v[10], p.drive = v[11], p.keytrk = v[12];
		p.fA = v[13], p.fD = v[14], p.fS = v[15], p.fR = v[16], p.aA = v[17], p.aD = v[18], p.aS = v[19], p.aR = v[20];
		p.rate = v[21], p.lfoSquare = v[22] < 0.5f, p.lfoPitch = v[23], p.lfoFilter = v[24];
		p.accent = v[25], p.width = v[26];
		p.outGain = gain[t];
		p.cutoff = mac2(p.cutoff, a, std::min(0.15f, 0.35f * p.cutoff), 0.40f);  // a dark sub closes less
		p.res = mac2(p.res, b, 0.30f, 0.45f);
		monoP = p;
	}
	else if (t == T_MELODY) {
		gfunke::Params p;
		p.model = clampv((int) std::lround(v[0]), 0, gfunke::NUM_MODELS - 1);
		p.timbre = v[1], p.bite = v[2], p.width = v[3], p.sub = v[4];
		p.mode = clampv((int) std::lround(v[5]), 0, 2);
		p.glide = v[6], p.accent = v[7];
		p.ftype = clampv((int) std::lround(v[8]), 0, gmono::NUM_FILTERS - 1);
		p.cutoff = v[9], p.res = v[10], p.fenv = v[11], p.key = v[12], p.fatk = v[13], p.fdec = v[14], p.drive = v[15];
		p.atk = v[16], p.dec = v[17], p.sus = v[18], p.rel = v[19];
		p.vdepth = v[20], p.vrate = v[21], p.vdelay = v[22];
		p.div = clampv((int) std::lround(v[23]), 0, gfunke::NUM_DIVS - 1);
		p.chorus = v[24], p.echo = v[25], p.fdbk = v[26], p.lofi = v[27], p.verb = v[28];
		p.outGain = gain[t];
		p.cutoff = mac2(p.cutoff, a, 0.08f, 0.35f);
		p.timbre = mac2(p.timbre, a, 0.1f, 0.2f);
		p.verb = mac2(p.verb, b, 0.25f, 0.45f);
		p.echo = mac2(p.echo, b, 0.2f, 0.3f);
		funkeP = p;
	}
	else if (t == T_PADS) {
		gschmelz::StringsParams sp;
		gschmelz::SoloParams op;
		gschmelz::BusParams bp;
		sp.reg = v[0], sp.cresc = v[1], sp.rel = v[2], sp.tone = v[3];
		sp.oct = clampv((int) std::lround(v[5]), 0, 2);
		bp.ensDepth = v[4], bp.ens = clampv((int) std::lround(v[6]), 0, gschmelz::NUM_ENS - 1);
		op.sound = v[7], op.dec = v[8], op.sus = v[9];
		bp.trem = v[10];
		padSplit = clampv((int) std::lround(v[11]), 0, 2);
		op.atk = v[12], bp.tremRate = v[13], op.velSens = v[14];
		padHold = v[15] > 0.5f;
		bp.bal = v[16], bp.anim = v[17], bp.animRate = v[18], bp.phaser = v[19], bp.phaserRate = v[20];
		bp.verb = v[21], bp.size = v[22], bp.vol = v[23];
		bp.outGain = gain[t];
		padReg = sp.reg;
		bp.anim = mac(bp.anim, a, 0.9f);
		bp.phaser = mac(bp.phaser, a, 0.5f);
		bp.ensDepth = mac(bp.ensDepth, a, 0.4f);
		bp.verb = mac(bp.verb, b, 0.9f);
		bp.size = mac(bp.size, b, 0.5f);
		padSP = sp, padOP = op, padBP = bp;
	}
}

/** NEW with SOUND: a new preset for track t (same genre or all; never the same one again). */
void Engine::rollPreset(int t) {
	if (!soundOn || hold[t])
		return;
	int n = factoryCount(t);
	if (n <= 1)
		return;
	int cur = preset[t];
	int k;
	if (soundAll == 2 && t != T_DRUMS) {
		int pr = clampv((int) pat.trackGenre[t], 0, numProfiles() - 1);
		const std::vector<int>& c = sameGenre[t][pr];
		if (!c.empty()) {
			k = c[rng.range((int) c.size())];
			for (int tries = 0; k == cur && c.size() > 1 && tries < 8; tries++)
				k = c[rng.range((int) c.size())];
			loadPreset(t, k);
			return;
		}
	}
	k = rng.range(n - 1);
	if (cur < n && k >= cur)
		k++;
	loadPreset(t, k);
}

Engine::SynSnap Engine::snapSyn(int t) {
	SynSnap s;
	s.vals = vals[t];
	std::memcpy(s.dx, dxVals, sizeof s.dx);
	if (t == T_DRUMS && dx) {
		s.kit.resize(dx->kit_size());
		dx->kit_export(s.kit.data());
	}
	return s;
}
void Engine::restoreSyn(int t, const SynSnap& s) {
	if (t == T_DRUMS) {
		if (dx && s.kit.size() == dx->kit_size())
			dx->kit_import(s.kit.data());
		for (int j = 0; j < JDX_NP; j++)
			setDxParam(j, s.dx[j]);
	}
	else {
		vals[t] = s.vals;
		dirty[t] = true;
	}
	changed();
}
void Engine::markUndo(int t) {
	synUndo[t].push_back(snapSyn(t));
	if (synUndo[t].size() > 40)
		synUndo[t].erase(synUndo[t].begin());
	abOrig[t] = false;
}

void Engine::savePreset(int t, const std::string& nameIn) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (t < 0 || t >= NUM_TRACKS)
		return;
	std::string name = nameIn;
	for (char& c : name)
		if (c >= 'a' && c <= 'z')
			c = (char) (c - 'a' + 'A');
	if (name.empty())
		name = "USER " + std::to_string(user[t].size() + 1);
	if (name.size() > 24)
		name.resize(24);
	UserPreset u;
	u.name = name;
	if (t == T_DRUMS) {
		if (!dx)
			return;
		u.kit.resize(dx->kit_size());
		dx->kit_export(u.kit.data());
		for (int j = 0; j < JDX_NP; j++)
			u.vals.push_back((float) dxVals[j]);
	}
	else {
		u.vals = vals[t];
		u.gain = gain[t];
	}
	// the same name: overwrite
	int at = -1;
	for (size_t k = 0; k < user[t].size(); k++)
		if (user[t][k].name == name)
			at = (int) k;
	if (at >= 0)
		user[t][at] = u;
	else {
		user[t].push_back(u);
		at = (int) user[t].size() - 1;
	}
	saveUserPresets();
	preset[t] = factoryCount(t) + at;
	orig[t] = vals[t];
	if (t == T_DRUMS && dx) {
		dxOrigKit = u.kit;
		std::memcpy(dxOrigVals, dxVals, sizeof dxVals);
	}
	say("Saved: " + name);
}

// ================================================================ commands
void Engine::cmd(int op, int a, int b, float f) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (!ready)
		return;
	int t = clampv(a, 0, NUM_TRACKS - 1);
	switch (op) {
		case OP_PLAY:
			if (songOn)
				stopSong(false);
			restart();
			running = true;
			break;
		case OP_STOP:
			if (songOn)
				stopSong(false);
			running = false;
			step = -1;
			resetArmed = true;
			bassV.gate = melV.gate = padV.gate = 0.f;
			break;
		case OP_NEWALL:
			doGenerate(true);
			for (int k = 0; k < NUM_TRACKS; k++)
				rollPreset(k);
			break;
		case OP_LOCK: chordLock = !chordLock; break;
		case OP_SOUND: soundOn = !soundOn; break;
		case OP_SOUNDMODE: soundAll = soundAll == 2 ? 1 : 2; break;
		case OP_BPM: bpm = clampv(f, 40.f, 220.f); break;
		case OP_BPM_GENRE: bpm = clampv(prof(mainProfile).bpm, 40.f, 220.f); break;
		case OP_KEY:
			key = ((a % 12) + 12) % 12;
			break;
		case OP_COLOR: color = c01(f); break;
		case OP_SWING: swing = clampv(f, -1.f, 1.f); break;
		case OP_LENGTH:
			lengthIdx = clampv(a, 0, 3);
			layoutTimeline(pat, lengthSteps());
			break;
		case OP_MUTATE: mutateAmt = c01(f); break;
		case OP_MASTER: master = c01(f); break;
		case OP_DUCK: duck = c01(f); break;
		case OP_GENRE:
			mainProfile = clampv(a, 0, numProfiles() - 1);
			doGenerate(false);
			for (int k = 0; k < NUM_TRACKS; k++)
				if (trackSel[k] < 0)
					rollPreset(k);
			break;
		case OP_TGENRE:
			trackSel[t] = b < 0 ? -1 : clampv(b, 0, numProfiles() - 1);
			newTrack(t);
			rollPreset(t);
			break;
		case OP_DENS: density[t] = c01(f); break;
		case OP_TK2:
			if (t == T_DRUMS)
				fillK = c01(f);
			else if (t == T_BASS)
				moveK = c01(f);
			else if (t == T_MELODY)
				rangeK = c01(f);
			else
				voicingK = c01(f);
			break;
		case OP_TNEW:
			newTrack(t);
			rollPreset(t);
			break;
		case OP_HOLD: hold[t] = !hold[t]; break;
		case OP_PRESET: loadPreset(t, b); break;
		case OP_MACRO: {
			int tt = clampv(a / 2, 0, NUM_TRACKS - 1);
			if (a % 2 == 0)
				macA[tt] = c01(f);
			else
				macB[tt] = c01(f);
			if (tt == T_DRUMS)
				dxApplyMacros();
			else
				dirty[tt] = true;
			break;
		}
		case OP_LEVEL: level[t] = c01(f); break;
		case OP_MUTE: mute[t] = !mute[t]; break;
		case OP_SYN: {
			const std::vector<ParamDef>& d = defsOf(t);
			if (b < 0 || b >= (int) d.size() || b >= (int) vals[t].size())
				break;
			float v = clampv(f, d[b].min, d[b].max);
			if (d[b].kind != 0)
				v = std::round(v);
			vals[t][b] = v;
			dirty[t] = true;
			abOrig[t] = false;
			break;
		}
		case OP_SYN_MARK: markUndo(t); break;
		case OP_SYN_INIT:
			markUndo(t);
			if (t == T_DRUMS) {
				if (dx && dxOrigKit.size() == dx->kit_size())
					dx->kit_import(dxOrigKit.data());
				for (int j = 0; j < JDX_NP; j++)
					setDxParam(j, dxOrigVals[j]);
			}
			else {
				vals[t] = orig[t];
				dirty[t] = true;
			}
			say("Back to the preset");
			break;
		case OP_SYN_UNDO:
			if (synUndo[t].empty()) {
				say("Nothing to undo");
				break;
			}
			restoreSyn(t, synUndo[t].back());
			synUndo[t].pop_back();
			abOrig[t] = false;
			break;
		case OP_SYN_AB:
			// compare: the edited sound and the preset as loaded
			if (!abOrig[t]) {
				abEdit[t] = snapSyn(t);
				SynSnap o;
				o.vals = orig[t];
				std::memcpy(o.dx, dxOrigVals, sizeof o.dx);
				o.kit = dxOrigKit;
				restoreSyn(t, o);
				abOrig[t] = true;
			}
			else {
				restoreSyn(t, abEdit[t]);
				abOrig[t] = false;
			}
			break;
		case OP_SYN_TEST:
			if (t == T_DRUMS)
				dxHit(clampv(b, 0, JDX_NLANE - 1), 110);
			if (t == T_BASS)
				startMono(bassV, [] { NoteEv n; n.on = true; n.pitch = 36; n.vel = 0.9f; n.len = 1; n.gate = 1.f; return n; }());
			if (t == T_MELODY)
				startMono(melV, [] { NoteEv n; n.on = true; n.pitch = 60; n.vel = 0.9f; n.len = 1; n.gate = 1.f; return n; }());
			if (t == T_PADS) {
				PadEv e;
				e.on = true;
				int ns[4] = {57, 60, 64, 67};
				std::memcpy(e.notes, ns, sizeof ns);
				e.vel = 0.8f;
				e.len = 1;
				e.gate = 1.f;
				startPad(e);
			}
			if (t != T_DRUMS) {
				MonoGate* m = t == T_BASS ? &bassV : t == T_MELODY ? &melV : nullptr;
				if (m)
					m->gate = 0.6f * sr;
				else
					padV.gate = 0.9f * sr;
			}
			break;
		case OP_DX_SND:
			// a = lane, b = patch * 8 + k
			if (dx) {
				int lane = clampv(a, 0, JDX_NLANE - 1), patch = (b >> 3) & 1, k = clampv(b & 7, 0, JDX_NSND - 1);
				dx->snd_set((uint32_t) lane, (uint32_t) patch, (uint32_t) k, clampv((int) std::lround(f), 0, 127));
				abOrig[T_DRUMS] = false;
			}
			break;
		case OP_DX_KIT:
			setDxParam(b, (int) std::lround(f));
			abOrig[T_DRUMS] = false;
			break;
		case OP_DX_LANEMUTE: {
			int lane = clampv(a, 0, JDX_NLANE - 1);
			laneMute[lane] = !laneMute[lane];
			setDxParam(JDX_LN0 + lane, dxVals[JDX_LN0 + lane]);
			break;
		}
		case OP_AB_PART: partButton(clampv(a, 0, NUM_SECTIONS - 1)); break;
		case OP_AB_PAD: tapPad(clampv(a, 0, NUM_PADS - 1)); break;
		case OP_AB_UNDO:
			if (padUndo.empty()) {
				say("Nothing to undo");
				break;
			}
			if (padUndo.back().pad < 0 && padUndo.back().all) {
				for (int i = 0; i < NUM_PADS; i++)
					slots[i] = (*padUndo.back().all)[i], slotVer[i]++;
				song.clear();
			}
			else {
				int i = padUndo.back().pad;
				slots[i] = padUndo.back().before;
				slotVer[i]++;
			}
			padUndo.pop_back();
			session.active = false;
			say("Undone");
			break;
		case OP_AB_CLEAR:
			clearArmed = !clearArmed;
			say(clearArmed ? "CLEAR: tap the pad to clear" : "");
			break;
		case OP_AB_MODE: trackMode[t] = clampv(b, 0, 2); break;
		case OP_AB_SONG: songButton(); break;
		case OP_AB_NEXT: {
			int from = cuedSlot >= 0 ? cuedSlot : playingSlot;
			for (int k = 1; k <= NUM_PADS; k++) {
				int i = ((from < 0 ? -1 : from) + k + NUM_PADS) % NUM_PADS;
				if (slots[i].used) {
					if (songOn)
						stopSong(false);
					cuePad(i);
					break;
				}
			}
			break;
		}
		case OP_PRESET_DELETE: {
			int f0 = factoryCount(t), k = b - f0;
			if (k >= 0 && k < (int) user[t].size()) {
				std::string n = user[t][k].name;
				user[t].erase(user[t].begin() + k);
				saveUserPresets();
				if (preset[t] == b)
					preset[t] = 0;
				else if (preset[t] > b)
					preset[t]--;
				say("Deleted: " + n);
			}
			break;
		}
		default: break;
	}
	changed();
}

std::string Engine::message() {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	return msg;
}

// ================================================================ the sequencer (GENRA's tick)
void Engine::restart() {
	step = -1;
	phase = 0.f;
	resetArmed = true;
	passCount = 0;
	firstPass = true;
	barIndex = -1;
	dDrums = dBass = dMel = dPad = -1.f;
	arrRowEnd = -1;
	arrFillEnd = -1;
}

float Engine::swingAt(const Params& P, int t, int st) const {
	if (st % 2 == 0)
		return 0.f;
	return (swingOf(pat, P, t, -1) - 0.5f) * 2.f * stepPeriod;
}

PartPos Engine::curPartPos(int L) const {
	if (arrRowEnd >= 0 && arrRowBars > 0) {
		int64_t bar = barIndex - (arrRowEnd - arrRowBars);
		int b = (int) std::max<int64_t>(0, std::min<int64_t>(arrRowBars - 1, bar));
		return partPos(pat, b * STEPS_PER_BAR + step % STEPS_PER_BAR, arrRowBars * STEPS_PER_BAR);
	}
	return partPos(pat, step, L);
}

void Engine::dxHit(int lane, int vel) {
	if (dx)
		dx->hit((uint32_t) lane, (uint32_t) clampv(vel, 1, 127));
}

void Engine::fireDrum(int v, float vel, int ratchet) {
	DrumVoice& d = drv[v];
	float interval = stepPeriod / std::max(1, ratchet);
	d.vel = vel;
	d.ratLeft = ratchet - 1;
	d.ratInterval = interval;
	d.ratCounter = interval;
	dxHit(TO_LANE[v], (int) std::lround(vel * 127.f));
	if (v == KICK)
		duckRise = true;
	lastHit[v] = 1.f;
}
void Engine::queueDrum(int v, const Hit& h) {
	if (h.delay > 0.f) {
		DrumVoice& d = drv[v];
		if (d.delayCnt >= 0.f) {  // a hit still waits: it plays now, it must not get lost
			d.delayCnt = -1.f;
			fireDrum(v, d.pVel, d.pRat);
		}
		d.delayCnt = h.delay * sr;
		d.pVel = h.vel;
		d.pRat = h.ratchet;
		return;
	}
	fireDrum(v, h.vel, h.ratchet);
}
void Engine::startMono(MonoGate& m, const NoteEv& n) {
	if (m.slidePending && m.gate > 0.f)
		m.slideOut = std::min(stepPeriod, 0.25f * sr);  // glide: no retrigger
	else if (m.gate > 0.f)
		m.retrig = 0.002f * sr;
	m.slidePending = n.slide;
	m.pitch = (n.pitch - 60) / 12.f;
	m.vel = n.vel;
	m.acc = n.acc;
	m.gate = std::max(0.004f * sr, n.len * stepPeriod * n.gate);
}
void Engine::startPad(const PadEv& e) {
	if (padV.gate > 0.f)
		padV.retrig = 0.002f * sr;
	for (int k = 0; k < 4; k++)
		padV.pitch[k] = (e.notes[k] - 60) / 12.f;
	padV.vel = e.vel;
	padV.gate = std::max(0.004f * sr, e.len * stepPeriod * e.gate);
}
void Engine::flushQueued() {
	if (dDrums >= 0.f)
		for (int v = 0; v < NUM_DRUMS; v++)
			if (qDrums[v].on)
				queueDrum(v, qDrums[v]);
	if (dBass >= 0.f)
		startMono(bassV, qBass);
	if (dMel >= 0.f)
		startMono(melV, qMel);
	if (dPad >= 0.f)
		startPad(qPad);
	dDrums = dBass = dMel = dPad = -1.f;
}
void Engine::processQueued() {
	if (dDrums >= 0.f && (dDrums -= 1.f) <= 0.f) {
		dDrums = -1.f;
		for (int v = 0; v < NUM_DRUMS; v++)
			if (qDrums[v].on)
				queueDrum(v, qDrums[v]);
	}
	if (dBass >= 0.f && (dBass -= 1.f) <= 0.f) {
		dBass = -1.f;
		startMono(bassV, qBass);
	}
	if (dMel >= 0.f && (dMel -= 1.f) <= 0.f) {
		dMel = -1.f;
		startMono(melV, qMel);
	}
	if (dPad >= 0.f && (dPad -= 1.f) <= 0.f) {
		dPad = -1.f;
		startPad(qPad);
	}
}

void Engine::doTick() {
	flushQueued();
	int L = lengthSteps();
	if (pat.layoutLen != L)
		layoutTimeline(pat, L);
	step++;
	if (step >= L)
		step = 0;
	if (step % STEPS_PER_BAR == 0) {
		barIndex++;
		bool applied = false;
		if (cueQ.on && (cueQ.now || cueQ.atBar <= barIndex)) {
			Cue c = cueQ;
			cueQ.on = false;
			applyCue(c, true);
			applied = true;
		}
		else if (songOn && arrRowEnd >= 0 && barIndex >= arrRowEnd) {
			songBar();
			applied = true;
		}
		if (applied) {
			if (!running)
				return;  // the song ended at this bar line
			step = 0;
			L = lengthSteps();
			layoutTimeline(pat, L);
		}
		// ABLAUF: a fill over the last bar of a song row, and at the end of every 8-bar phrase of a long row
		arrFillEnd = -1;
		if (arrFill && arrRowEnd >= 0 && barIndex == arrRowEnd - 1) {
			arrFillEnd = step + STEPS_PER_BAR;
			fillActive = true;
		}
		else if (arrRowEnd >= 0 && arrRowBars > 0 &&
		         partPhraseFill(pat, (int) (barIndex - (arrRowEnd - arrRowBars)), arrRowBars)) {
			arrFillEnd = step + STEPS_PER_BAR;
			fillActive = true;
		}
	}
	if (step == 0 && !firstPass)
		passCount++;
	Params P = buildParams();
	if (step == 0) {
		fillActive = rng.uni() < P.fill || arrFillEnd > 0;
		P.fillActive = fillActive;
		if (!firstPass && mutateAmt > 0.001f) {
			bool lk[NUM_TRACKS] = {false, false, false, false};
			mutate(pat, mutateAmt, rng, lk, chordLock, P);
			patChanges++;
			changed();
		}
		firstPass = false;
	}
	P.pass = passCount;
	const PartPos pp = curPartPos(L);
	drumStep(pat, P, step, arrFillEnd > 0 ? arrFillEnd : L, fillActive, qDrums);
	if (pp.sec >= 0 && (pat.partMask & (1u << T_DRUMS)))
		shapeDrums(pp, qDrums);
	{
		float d = swingAt(P, T_DRUMS, step);
		if (d >= 1.f)
			dDrums = d;
		else
			for (int v = 0; v < NUM_DRUMS; v++)
				if (qDrums[v].on)
					queueDrum(v, qDrums[v]);
	}
	auto place = [&](int t, bool on, float& dq, auto start) {
		if (!on)
			return;
		float off = swingAt(P, t, step);
		if (off >= 1.f)
			dq = off;
		else
			start();
	};
	NoteEv nb = bassAt(pat, P, step, L);
	if (pp.sec >= 0 && (pat.partMask & (1u << T_BASS)))
		shapeNote(pp, T_BASS, nb);
	qBass = nb;
	place(T_BASS, nb.on, dBass, [&] { startMono(bassV, nb); });
	NoteEv nm = melAt(pat, P, step, L);
	if (pp.sec >= 0 && (pat.partMask & (1u << T_MELODY)))
		shapeNote(pp, T_MELODY, nm);
	qMel = nm;
	place(T_MELODY, nm.on, dMel, [&] { startMono(melV, nm); });
	PadEv np = padAt(pat, P, step, L, padPrev, padHasPrev);
	qPad = np;
	place(T_PADS, np.on, dPad, [&] { startPad(np); });
}

// ================================================================ audio
void Engine::renderVoices(float& outL, float& outR) {
	float tl[NUM_TRACKS] = {}, tr[NUM_TRACKS] = {};
	// DRUM-X (44.1 kHz) to the device's rate: Hermite between the last four frames
	if (dx) {
		double inc = (double) dx->sample_rate() / (double) sr;
		dxPos += inc;
		while (dxPos >= 1.0) {
			dxPos -= 1.0;
			for (int k = 0; k < 3; k++)
				dxBufL[k] = dxBufL[k + 1], dxBufR[k] = dxBufR[k + 1];
			dx->render(1, &dxBufL[3], &dxBufR[3]);
		}
		auto herm = [](const float* y, float x) {
			float c0 = y[1], c1 = 0.5f * (y[2] - y[0]);
			float c2 = y[0] - 2.5f * y[1] + 2.f * y[2] - 0.5f * y[3];
			float c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
			return ((c3 * x + c2) * x + c1) * x + c0;
		};
		tl[T_DRUMS] = herm(dxBufL, (float) dxPos) * 5.f;
		tr[T_DRUMS] = herm(dxBufR, (float) dxPos) * 5.f;
	}
	auto monoGate = [&](MonoGate& m, bool& g, bool& slide) {
		g = m.gate > 0.f && m.retrig <= 0.f;
		if (m.gate > 0.f)
			m.gate -= 1.f;
		else
			m.slidePending = false;
		if (m.retrig > 0.f)
			m.retrig -= 1.f;
		slide = m.slideOut > 0.f;
		if (m.slideOut > 0.f)
			m.slideOut -= 1.f;
	};
	{
		bool g, s;
		monoGate(bassV, g, s);
		gmono::Inputs in;
		in.voct = bassV.pitch, in.gate = g, in.vel = bassV.vel, in.acc = g && bassV.acc;
		in.slide = s, in.slideConnected = true;
		monoV->process(monoP, in, tl[T_BASS], tr[T_BASS]);
	}
	{
		bool g, s;
		monoGate(melV, g, s);
		funkeCh.voct = melV.pitch, funkeCh.gate = g, funkeCh.vel = melV.vel, funkeCh.acc = g && melV.acc;
		funkeCh.slide = s;
		float env;
		int div = clampv(funkeP.div, 0, gfunke::NUM_DIVS - 1);
		float echoS = 60.f / std::max(20.f, bpm) * gfunke::DIV_BEATS[div];
		funkeE->process(funkeP, &funkeCh, 1, true, echoS, tl[T_MELODY], tr[T_MELODY], env);
	}
	{
		bool g = padV.gate > 0.f && padV.retrig <= 0.f;
		if (padV.gate > 0.f)
			padV.gate -= 1.f;
		if (padV.retrig > 0.f)
			padV.retrig -= 1.f;
		float anim = sbus->animate(padBP);
		float str = 0.f, solo = 0.f;
		for (int c = 0; c < NP; c++) {
			bool was = pGate[c];
			pGate[c] = g;
			bool rise = g && !was;
			float voct = padV.pitch[c];
			if (rise) {
				strSide[c] = gschmelz::stringsTakes(padSplit, voct);
				soloSide[c] = gschmelz::soloTakes(padSplit, voct);
			}
			if (voct != lastV[c]) {
				lastV[c] = voct;
				hz[c] = gschmelz::C4 * std::pow(2.f, voct);
			}
			gschmelz::StringsParams q = padSP;
			q.reg = c01(padReg + anim);
			if (sv[c].envStep(q, g && strSide[c], rise && strSide[c], padV.vel))
				str += sv[c].render(q, hz[c]);
			bool trig = rise && soloSide[c];
			if (trig)
				held[c] = true;
			if (!padHold)
				held[c] = false;
			bool gg = (g && soloSide[c]) || (padHold && held[c]);
			if (so[c].envStep(padOP, gg, trig, padV.vel))
				solo += so[c].render(padOP, hz[c]);
		}
		float st, s2;
		sbus->process(padBP, str, solo, tl[T_PADS], tr[T_PADS], st, s2);
	}
	// DUCK: 3 ms down after the kick, then back over about 150 ms (bass and pads)
	if (duckRise) {
		duckEnv += (1.f - duckEnv) * std::min(1.f, 1.f / (0.003f * sr));
		if (duckEnv > 0.97f)
			duckRise = false;
	}
	else
		duckEnv *= std::exp(-1.f / (0.15f * sr));
	float dg = 1.f - 0.85f * duck * duckEnv;
	tl[T_BASS] *= dg, tr[T_BASS] *= dg, tl[T_PADS] *= dg, tr[T_PADS] *= dg;
	float ml = 0.f, mr = 0.f;
	for (int t = 0; t < NUM_TRACKS; t++) {
		float target = mute[t] ? 0.f : 1.f;
		muteGain[t] += (target - muteGain[t]) * 0.002f;
		float gn = gainOf(level[t]) * muteGain[t];
		ml += tl[t] * gn, mr += tr[t] * gn;
	}
	float mg = gainOf(master);
	auto soft = [](float x) {
		return x > 8.f || x < -8.f ? (x > 0.f ? 1.f : -1.f) * (8.f + 2.f * std::tanh((std::fabs(x) - 8.f) * 0.5f)) : x;
	};
	outL = clampv(soft(ml * mg) * 0.1f, -1.f, 1.f);
	outR = clampv(soft(mr * mg) * 0.1f, -1.f, 1.f);
	if (!std::isfinite(outL) || !std::isfinite(outR))
		outL = outR = 0.f;
}

void Engine::render(float* L, float* R, int n) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (!ready) {
		std::memset(L, 0, sizeof(float) * n);
		std::memset(R, 0, sizeof(float) * n);
		return;
	}
	for (int t = 0; t < NUM_TRACKS; t++)
		applyVals(t);
	for (int i = 0; i < n; i++) {
		stepPeriod = sr * 60.f / clampv(bpm, 40.f, 220.f) / 4.f;
		if (running) {
			bool tick = false;
			phase += 1.f / stepPeriod;
			if (phase >= 1.f || resetArmed) {
				phase = resetArmed ? 0.f : phase - 1.f;
				resetArmed = false;
				tick = true;
			}
			processQueued();
			if (tick)
				doTick();
		}
		for (int v = 0; v < NUM_DRUMS; v++) {
			DrumVoice& d = drv[v];
			if (d.delayCnt >= 0.f) {
				d.delayCnt -= 1.f;
				if (d.delayCnt < 0.f)
					fireDrum(v, d.pVel, d.pRat);
			}
			if (d.ratLeft > 0) {
				d.ratCounter -= 1.f;
				if (d.ratCounter <= 0.f) {
					d.ratLeft--;
					d.ratCounter += d.ratInterval;
					dxHit(TO_LANE[v], (int) std::lround(d.vel * 127.f));
				}
			}
		}
		renderVoices(L[i], R[i]);
	}
	for (int v = 0; v < NUM_DRUMS; v++)
		lastHit[v] *= std::exp(-n / (0.12f * sr));
}

// ================================================================ ABLAUF MINI
unsigned Engine::rerollMask() const {
	unsigned m = 0;
	for (int t = 0; t < NUM_TRACKS; t++)
		if (trackMode[t] >= 1)
			m |= 1u << t;
	return m;
}
unsigned Engine::varyMask() const {
	unsigned m = 0;
	for (int t = 0; t < NUM_TRACKS; t++)
		if (trackMode[t] == 2)
			m |= 1u << t;
	return m;
}
static int nameNumber(const char* name, const std::string& base) {
	std::string n(name, strnlen(name, 9));
	if (n.size() > base.size() + 1 && n.compare(0, base.size() + 1, base + " ") == 0)
		return std::atoi(n.c_str() + base.size() + 1);
	return 0;
}
int Engine::lastVariant(int k, int family, int skip) const {
	int best = -1, top = -1;
	if (family < 0)
		return -1;
	for (int i = 0; i < NUM_PADS; i++) {
		if (i == skip || !slots[i].used || slots[i].family != family || slots[i].tag != k)
			continue;
		int n = nameNumber(slots[i].name, SECTION_NAMES[k]);
		if (n > top)
			top = n, best = i;
	}
	return best;
}
std::string Engine::nextName(const std::string& base, int skip, int family) const {
	int top = 0;
	for (int i = 0; i < NUM_PADS; i++)
		if (i != skip && slots[i].used && slots[i].family == family)
			top = std::max(top, nameNumber(slots[i].name, base));
	return base + " " + std::to_string(top + 1);
}
int Engine::newFamily() const {
	int top = 0;
	for (int i = 0; i < NUM_PADS; i++)
		if (slots[i].used)
			top = std::max(top, (int) slots[i].family);
	return top + 1;
}
int Engine::freePad() const {
	for (int i = 1; i < NUM_PADS; i++)
		if (!slots[i].used)
			return i;
	return slots[0].used ? -1 : 0;
}
int Engine::familySource(int i) const {
	if (i < 0 || i >= NUM_PADS || !slots[i].used)
		return -1;
	int f = slots[i].family;
	if (f >= 0)
		for (int k = 0; k < NUM_PADS; k++)
			if (slots[k].used && slots[k].family == f && slots[k].tag < 0)
				return k;
	return i;
}
int Engine::playingPad() const {
	if (playingSlot >= 0 && playingSlot < NUM_PADS && slots[playingSlot].used && patChanges == patAtCue)
		return playingSlot;
	return -1;
}
void Engine::remember(int i) {
	padUndo.push_back({i, slots[i], nullptr});
	if (padUndo.size() > 32)
		padUndo.erase(padUndo.begin());
	slotVer[i]++;
}
void Engine::rememberAll() {
	PadUndo u;
	u.pad = -1;
	u.all = std::make_shared<std::vector<Slot>>(slots, slots + NUM_PADS);
	padUndo.push_back(u);
	if (padUndo.size() > 32)
		padUndo.erase(padUndo.begin());
	for (int i = 0; i < NUM_PADS; i++)
		slotVer[i]++;
}
Slot Engine::snapshot() {
	Slot s;
	s.pat = pat;
	for (int t = 0; t < NUM_TRACKS; t++)
		s.sel[t] = (int8_t) trackSel[t];
	s.lengthIdx = (int8_t) lengthIdx;
	s.mods = arrMods;
	s.tag = -1;
	s.used = true;
	return s;
}
int64_t Engine::loopEndBar() const {
	int L = std::max(16, lengthSteps());
	int s = std::max(0, step);
	int barIn = (s % L) / 16;
	return barIndex + (L / 16 - barIn);
}
void Engine::cuePad(int i) {
	Cue c;
	c.on = true;
	c.slot = i;
	c.now = !running;
	c.atBar = loopEndBar();
	if (c.now)
		applyCue(c, false);
	else {
		cueQ = c;
		cuedSlot = i;
	}
	changed();
}
void Engine::applyCue(const Cue& c, bool atBarLine) {
	cuedSlot = -1;
	if (c.stop) {
		running = false;
		step = -1;
		resetArmed = true;
		arrRowEnd = -1;
		arrFillEnd = -1;
		arrFill = false;
		playingSlot = -1;
		songOn = false;
		changed();
		return;
	}
	const Slot& s = slots[clampv(c.slot, 0, NUM_PADS - 1)];
	if (!s.used)
		return;
	pat = s.pat;
	for (int t = 0; t < NUM_TRACKS; t++)
		trackSel[t] = s.sel[t];
	lengthIdx = clampv((int) s.lengthIdx, 0, 3);
	padHasPrev = false;
	arrMods = s.mods;
	arrFill = c.fill;
	int64_t start = atBarLine ? barIndex : barIndex + 1;
	arrRowEnd = c.rowBars > 0 ? start + c.rowBars : -1;
	arrRowBars = c.rowBars > 0 ? c.rowBars : 0;
	songRowStart = start;
	arrFillEnd = -1;
	firstPass = true;
	passCount = 0;
	if (!atBarLine) {
		step = -1;
		resetArmed = true;
		barIndex = -1;
		if (c.rowBars > 0) {
			arrRowEnd = c.rowBars;
			songRowStart = 0;
		}
	}
	playingSlot = c.slot;
	patChanges++;
	patAtCue = patChanges;
	changed();
}
void Engine::tapPad(int i) {
	if (clearArmed) {
		clearArmed = false;
		if (slots[i].used) {
			remember(i);
			slots[i] = Slot();
			if (playingSlot == i)
				playingSlot = -1;
			say(std::string(i == 0 ? "ROOT" : "Pad") + " cleared");
		}
		else
			say("");
		return;
	}
	if (songOn)
		stopSong(false);  // a pad by hand: the song lets go
	if (slots[i].used) {
		cuePad(i);
		return;
	}
	storeSnap(i);
	session.active = false;
	playingSlot = i;
	patAtCue = patChanges;
	say(std::string(slots[i].name) + " stored");
}
void Engine::storeSnap(int i) {
	remember(i);
	slots[i] = snapshot();
	slots[i].tag = -1;
	int f = newFamily();
	slots[i].family = (int8_t) f;
	setName(slots[i], i == 0 ? std::string("ROOT") : "GENRA " + std::to_string(f));
}
void Engine::partButton(int k) {
	bool same = false;
	if (session.active) {
		int pl = playingPad();
		for (auto& mv : session.made)
			if (pl >= 0 && mv.first == pl && slotVer[pl] == mv.second)
				same = true;
		if (cuedSlot >= 0)
			for (auto& mv : session.made)
				if (mv.first == cuedSlot && slotVer[cuedSlot] == mv.second)
					same = true;
	}
	if (!same) {
		int pl = playingPad();
		session.made.clear();
		if (!slots[0].used && pl < 0) {  // nothing stored yet: what plays becomes ROOT, the parts its family
			storeSnap(0);
			playingSlot = 0;
			patAtCue = patChanges;
			session.source = slots[0];
		}
		else {
			session.source = snapshot();
			setName(session.source, pl >= 0 ? std::string(slots[pl].name, strnlen(slots[pl].name, 9)) : "");
			session.source.family = pl >= 0 ? slots[pl].family : (int8_t) -1;
		}
		session.P = buildParams(false);
		session.active = true;
	}
	makePart(k);
}
void Engine::makePart(int k) {
	int t = freePad();
	if (t < 0) {
		say("All 16 pads are full");
		return;
	}
	remember(t);
	makeSection(session.source, slots[t], k, rerollMask(), session.P, rng, partOpts);
	int prev = varyMask() ? lastVariant(k, slots[t].family, t) : -1;
	if (prev >= 0)
		varySection(slots[prev], slots[t], varyMask(), session.P, rng);
	slots[t].used = true;
	setName(slots[t], nextName(SECTION_NAMES[k], t, slots[t].family));
	slotVer[t]++;
	session.made.push_back({t, slotVer[t]});
	char b[64];
	std::snprintf(b, sizeof b, "%s -> pad %02d", std::string(slots[t].name, strnlen(slots[t].name, 9)).c_str(), t + 1);
	say(b);
	if (!songOn)
		cuePad(t);
}
void Engine::songButton() {
	if (songOn) {
		stopSong(true);
		say("Song stopped");
		return;
	}
	int pl = playingPad();
	if (pl >= 0) {
		makeSong(pl);
		return;
	}
	if (slots[0].used) {
		makeSong(0);
		return;
	}
	// nothing stored: what plays becomes ROOT first
	storeSnap(0);
	playingSlot = 0;
	patAtCue = patChanges;
	makeSong(0);
}
void Engine::makeSong(int sel) {
	int src = familySource(sel);
	if (src < 0) {
		say("Tap a pad with a pattern first");
		return;
	}
	int fam = slots[src].family;
	const Genre& G = prof(clampv((int) slots[src].pat.trackGenre[T_DRUMS], 0, numProfiles() - 1));
	int form = clampv(songFormForGenre(G.name), 0, NUM_SONG_FORMS - 1);
	const SongForm& F = SONG_FORMS[form];
	std::vector<int> have[NUM_SECTIONS];
	if (fam >= 0)
		for (int i = 0; i < NUM_PADS; i++)
			if (i != src && slots[i].used && slots[i].family == fam && slots[i].tag >= 0 && slots[i].tag < NUM_SECTIONS)
				have[slots[i].tag].push_back(i);
	for (int k = 0; k < NUM_SECTIONS; k++)
		std::stable_sort(have[k].begin(), have[k].end(), [&](int a, int b) {
			return nameNumber(slots[a].name, SECTION_NAMES[k]) < nameNumber(slots[b].name, SECTION_NAMES[k]);
		});
	rememberAll();
	Params P = buildParams(false);
	P.lengthBars = LENGTH_BARS[clampv((int) slots[src].lengthIdx, 0, 3)];
	P = withMods(P, slots[src].mods);
	int taken[NUM_SECTIONS] = {}, fallback = 0;
	song.clear();
	for (int k = 0; k < F.n; k++) {
		int sec = clampv(F.sec[k], 0, NUM_SECTIONS - 1);
		int pad = -1;
		if (taken[sec] < (int) have[sec].size())
			pad = have[sec][taken[sec]];
		else {
			int t = freePad();
			if (t >= 0) {
				makeSection(slots[src], slots[t], sec, rerollMask(), P, rng, partOpts);
				if (varyMask() && !have[sec].empty())
					varySection(slots[have[sec].back()], slots[t], varyMask(), P, rng);
				slots[t].used = true;
				setName(slots[t], nextName(SECTION_NAMES[sec], t, slots[t].family));
				have[sec].push_back(t);
				pad = t;
			}
		}
		if (pad < 0)
			pad = src, fallback++;
		taken[sec]++;
		int bars = F.bars[k];
		if (sec == SEC_BUILD && G.buildBars > 0)
			bars = G.buildBars;
		song.push_back({pad, bars, false});
	}
	for (size_t k = 0; k + 1 < song.size(); k++) {
		int a = F.sec[k], b = F.sec[k + 1];
		song[k].fill = (a == SEC_VERSE || a == SEC_DROP) && b != a;
	}
	session.active = false;
	int total = 0;
	for (const SongPart& p : song)
		total += p.bars;
	char b[96];
	std::snprintf(b, sizeof b, "Song: %s, %d bars", F.name, total);
	std::string m = b;
	if (fallback)
		m += " - pads full: " + std::to_string(fallback) + " parts play the original";
	say(m);
	startSong();
}
void Engine::startSong() {
	if (song.empty())
		return;
	cueQ.on = false;
	songOn = true;
	songPos = 0;
	restart();
	running = true;
	Cue c;
	c.on = true;
	c.slot = song[0].pad;
	c.rowBars = song[0].bars;
	c.fill = song[0].fill;
	c.row = 0;
	applyCue(c, false);
	songOn = true;
}
void Engine::stopSong(bool stopPlay) {
	songOn = false;
	songPos = -1;
	arrRowEnd = -1;
	arrRowBars = 0;
	arrFill = false;
	if (stopPlay) {
		running = false;
		step = -1;
		resetArmed = true;
	}
	changed();
}
/** At the bar line where the song row ends: the next row, or the end. */
void Engine::songBar() {
	int next = songPos + 1;
	if (next >= (int) song.size()) {
		Cue c;
		c.stop = true;
		applyCue(c, true);
		songPos = -1;
		say("Song over");
		return;
	}
	songPos = next;
	Cue c;
	c.on = true;
	c.slot = song[next].pad;
	c.rowBars = song[next].bars;
	c.fill = song[next].fill;
	c.row = next;
	applyCue(c, true);
}

// ================================================================ MIDI
std::vector<uint8_t> Engine::exportMidi(int what, int idx, std::string& name) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	Params P = buildParams(false);
	std::vector<genra::SongPart> parts;
	auto add = [&](const Pattern* p, const ArrMods& mods, int li, int bars, bool fill, const std::string& label) {
		genra::SongPart sp;
		sp.pat = p;
		sp.P = withMods(P, mods);
		sp.P.lengthBars = LENGTH_BARS[clampv(li, 0, 3)];
		sp.L = sp.P.lengthBars * STEPS_PER_BAR;
		sp.bars = bars > 0 ? bars : sp.P.lengthBars;
		sp.fill = fill;
		sp.label = label;
		parts.push_back(sp);
	};
	auto nm = [](const Slot& s) { return std::string(s.name, strnlen(s.name, 9)); };
	if (what == 0) {
		Params Pl = buildParams(true);
		int L = lengthSteps();
		std::vector<MidiNote> notes = renderLoop(pat, Pl, L);
		std::string g = prof(mainProfile).name;
		name = "genra-box-" + g;
		return writeSmf(notes, bpm, g.c_str(), L, &pat);
	}
	if (what == 1) {
		for (const SongPart& p : song)
			if (slots[p.pad].used)
				add(&slots[p.pad].pat, slots[p.pad].mods, slots[p.pad].lengthIdx, p.bars, p.fill, nm(slots[p.pad]));
		name = "genra-box-song";
	}
	else {
		for (int i = 0; i < NUM_PADS; i++)
			if (slots[i].used && (what == 2 || i == idx))
				add(&slots[i].pat, slots[i].mods, slots[i].lengthIdx, 0, false, nm(slots[i]));
		name = what == 2 ? "genra-box-pads" : "genra-box-" + (idx >= 0 && idx < NUM_PADS ? nm(slots[idx]) : std::string("pad"));
	}
	if (parts.empty())
		return {};
	std::vector<SongMarker> marks;
	std::vector<MidiNote> notes = renderSong(parts, &marks);
	int total = 0;
	for (const genra::SongPart& sp : parts)
		total += sp.bars * STEPS_PER_BAR;
	return writeSmf(notes, bpm, prof(mainProfile).name, total, what == 3 && idx >= 0 && idx < NUM_PADS ? &slots[idx].pat : nullptr, &marks);
}

// ================================================================ the UI's JSON
static void defsJson(std::string& o, const std::vector<ParamDef>& d) {
	o += '[';
	for (size_t i = 0; i < d.size(); i++) {
		if (i)
			o += ',';
		o += "{\"key\":";
		jstr(o, d[i].key);
		o += ",\"label\":";
		jstr(o, d[i].label);
		o += ",\"min\":";
		jnum(o, d[i].min);
		o += ",\"max\":";
		jnum(o, d[i].max);
		o += ",\"kind\":";
		jint(o, d[i].kind);
		o += ",\"opts\":[";
		for (size_t k = 0; k < d[i].opts.size(); k++) {
			if (k)
				o += ',';
			jstr(o, d[i].opts[k]);
		}
		o += "]}";
	}
	o += ']';
}

std::string Engine::meta() {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	std::string o = "{\"profiles\":[";
	for (int p = 0; p < numProfiles(); p++) {
		if (p)
			o += ',';
		const Genre& g = prof(p);
		o += "{\"g\":";
		jint(o, genreOfProfile(p));
		o += ",\"name\":";
		jstr(o, g.name);
		o += ",\"variant\":";
		jstr(o, g.variant);
		o += ",\"bpm\":";
		jnum(o, g.bpm);
		o += '}';
	}
	o += "],\"defs\":[[],";
	defsJson(o, monoDefs());
	o += ',';
	defsJson(o, funkeDefs());
	o += ',';
	defsJson(o, schmelzDefs());
	o += "],\"lanes\":[";
	for (int i = 0; i < JDX_NLANE; i++) {
		if (i)
			o += ',';
		jstr(o, LANE_NAMES[i]);
	}
	o += "],\"snd\":[";
	for (int i = 0; i < JDX_NSND; i++) {
		if (i)
			o += ',';
		jstr(o, SND_NAMES[i]);
	}
	o += "],\"sections\":[";
	for (int i = 0; i < NUM_SECTIONS; i++) {
		if (i)
			o += ',';
		jstr(o, SECTION_NAMES[i]);
	}
	o += "],\"notes\":[";
	for (int i = 0; i < 12; i++) {
		if (i)
			o += ',';
		jstr(o, NOTE_NAMES[i]);
	}
	o += "],\"sr\":";
	jnum(o, sr);
	o += ",\"drumx\":";
	o += dx ? "true" : "false";
	o += '}';
	return o;
}

std::string Engine::state(int known) {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (known == stateVer)
		return "";
	Params P = buildParams();
	std::string o = "{\"ver\":";
	jint(o, stateVer);
	o += ",\"running\":";
	o += running ? "true" : "false";
	o += ",\"bpm\":";
	jnum(o, bpm);
	o += ",\"key\":";
	jint(o, key);
	int sc = clampv(P.scale, 0, NUM_SCALES - 1);
	o += ",\"scale\":";
	jstr(o, SCALES[sc].name);
	o += ",\"color\":";
	jnum(o, color);
	o += ",\"swing\":";
	jnum(o, swing);
	o += ",\"length\":";
	jint(o, lengthIdx);
	o += ",\"mutate\":";
	jnum(o, mutateAmt);
	o += ",\"master\":";
	jnum(o, master);
	o += ",\"duck\":";
	jnum(o, duck);
	o += ",\"lock\":";
	o += chordLock ? "true" : "false";
	o += ",\"soundOn\":";
	o += soundOn ? "true" : "false";
	o += ",\"soundAll\":";
	jint(o, soundAll);
	o += ",\"genre\":";
	jint(o, mainProfile);
	o += ",\"tracks\":[";
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (t)
			o += ',';
		float k2 = t == T_DRUMS ? fillK : t == T_BASS ? moveK : t == T_MELODY ? rangeK : voicingK;
		o += "{\"sel\":";
		jint(o, trackSel[t]);
		o += ",\"plays\":";
		jint(o, clampv((int) pat.trackGenre[t], 0, numProfiles() - 1));
		o += ",\"dens\":";
		jnum(o, density[t]);
		o += ",\"k2\":";
		jnum(o, k2);
		o += ",\"preset\":";
		jint(o, preset[t]);
		o += ",\"presetName\":";
		jstr(o, presetName(t, preset[t]));
		o += ",\"presetCount\":";
		jint(o, presetCount(t));
		o += ",\"factory\":";
		jint(o, factoryCount(t));
		o += ",\"macA\":";
		jnum(o, macA[t]);
		o += ",\"macB\":";
		jnum(o, macB[t]);
		o += ",\"level\":";
		jnum(o, level[t]);
		o += ",\"mute\":";
		o += mute[t] ? "true" : "false";
		o += ",\"hold\":";
		o += hold[t] ? "true" : "false";
		o += ",\"ab\":";
		o += abOrig[t] ? "true" : "false";
		o += ",\"vals\":[";
		for (size_t k = 0; k < vals[t].size(); k++) {
			if (k)
				o += ',';
			jnum(o, vals[t][k]);
		}
		o += "],\"orig\":[";
		for (size_t k = 0; k < orig[t].size(); k++) {
			if (k)
				o += ',';
			jnum(o, orig[t][k]);
		}
		o += "],\"presets\":[";
		int n = presetCount(t);
		for (int i = 0; i < n; i++) {
			if (i)
				o += ',';
			jstr(o, presetName(t, i));
		}
		o += "],\"styles\":[";
		int fc = factoryCount(t);
		for (int i = 0; i < fc; i++) {
			if (i)
				o += ',';
			const char* st = t == T_BASS ? gmono::PRESETS[i].style : t == T_MELODY ? gfunke::PRESETS[i].style : t == T_PADS ? gschmelz::PRESETS[i].style : "DRUM-X";
			jstr(o, st);
		}
		o += "]}";
	}
	o += "],\"dx\":{\"kit\":[";
	for (int j = 0; j < JDX_NP; j++) {
		if (j)
			o += ',';
		jint(o, dxVals[j]);
	}
	o += "],\"kitText\":[";
	for (int j = 0; j < JDX_NP; j++) {
		if (j)
			o += ',';
		char b[32] = "";
		if (dx)
			dx->format((uint32_t) j, dxVals[j], b, sizeof b);
		jstr(o, b);
	}
	o += "],\"snd\":[";
	for (int l = 0; l < JDX_NLANE; l++) {
		if (l)
			o += ',';
		o += '[';
		for (int p = 0; p < 2; p++)
			for (int k = 0; k < JDX_NSND; k++) {
				if (p || k)
					o += ',';
				jint(o, dx ? dx->snd_get((uint32_t) l, (uint32_t) p, (uint32_t) k) : 64);
			}
		o += ']';
	}
	o += "],\"laneMute\":[";
	for (int l = 0; l < JDX_NLANE; l++) {
		if (l)
			o += ',';
		o += laneMute[l] ? "true" : "false";
	}
	o += "]},\"ablauf\":{\"pads\":[";
	for (int i = 0; i < NUM_PADS; i++) {
		if (i)
			o += ',';
		const Slot& s = slots[i];
		o += "{\"used\":";
		o += s.used ? "true" : "false";
		if (s.used) {
			o += ",\"name\":";
			jstr(o, std::string(s.name, strnlen(s.name, sizeof s.name)));
			o += ",\"tag\":";
			jint(o, s.tag);
			o += ",\"family\":";
			jint(o, s.family);
		}
		o += '}';
	}
	o += "],\"modes\":[";
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (t)
			o += ',';
		jint(o, trackMode[t]);
	}
	o += "],\"song\":[";
	for (size_t k = 0; k < song.size(); k++) {
		if (k)
			o += ',';
		o += "{\"pad\":";
		jint(o, song[k].pad);
		o += ",\"bars\":";
		jint(o, song[k].bars);
		o += ",\"tag\":";
		jint(o, slots[song[k].pad].used ? slots[song[k].pad].tag : -1);
		o += '}';
	}
	o += "],\"clear\":";
	o += clearArmed ? "true" : "false";
	o += "},\"msg\":";
	jstr(o, msg);
	o += ",\"msgId\":";
	jint(o, msgId);
	o += '}';
	return o;
}

std::string Engine::fast() {
	std::lock_guard<std::recursive_mutex> lk(mtx);
	std::string o = "{\"ver\":";
	jint(o, stateVer);
	o += ",\"running\":";
	o += running ? "true" : "false";
	int L = lengthSteps();
	int s = step >= 0 ? step % L : 0;
	o += ",\"step\":";
	jint(o, running && step >= 0 ? s : -1);
	// the lanes of the bar that plays (as the box's display)
	Params P = buildParams();
	int barStart = (s / 16) * 16;
	int prev[4];
	std::memcpy(prev, padPrev, sizeof prev);
	bool hp = false;
	o += ",\"lanes\":[";
	for (int t = 0; t < 4; t++) {
		if (t)
			o += ',';
		o += '"';
		for (int i = 0; i < 16; i++) {
			int st2 = barStart + i;
			bool on = false;
			if (st2 < L) {
				if (t == 0) {
					Hit hits[NUM_DRUMS];
					drumStep(pat, P, st2, L, false, hits);
					for (int v = 0; v < NUM_DRUMS; v++)
						on |= hits[v].on;
				}
				else if (t == 1)
					on = bassAt(pat, P, st2, L).on;
				else if (t == 2)
					on = melAt(pat, P, st2, L).on;
				else
					on = padAt(pat, P, st2, L, prev, hp).on;
			}
			o += on ? '1' : '0';
		}
		o += '"';
	}
	o += "],\"bar\":";
	jint(o, s / 16);
	o += ",\"bars\":";
	jint(o, L / 16);
	o += ",\"hits\":[";
	for (int l = 0; l < JDX_NLANE; l++) {
		if (l)
			o += ',';
		jnum(o, dx ? dx->lane_env((uint32_t) l) : 0.f);
	}
	o += "],\"playing\":";
	jint(o, playingSlot);
	o += ",\"playingSame\":";
	o += playingPad() >= 0 ? "true" : "false";
	o += ",\"cued\":";
	jint(o, cuedSlot);
	o += ",\"songOn\":";
	o += songOn ? "true" : "false";
	o += ",\"songPos\":";
	jint(o, songPos);
	float frac = 0.f;
	if (songOn && songPos >= 0 && songPos < (int) song.size() && song[songPos].bars > 0)
		frac = clampv((float) (barIndex - songRowStart) / song[songPos].bars, 0.f, 1.f);
	o += ",\"songFrac\":";
	jnum(o, frac);
	o += ",\"msgId\":";
	jint(o, msgId);
	o += '}';
	return o;
}

// ================================================================ storage
static const uint32_t SESSION_MAGIC = 0x58424e47;  // "GNBX"
static const uint32_t SESSION_VERSION = 1;

void Engine::saveSession() {
	Writer w;
	std::string path;
	{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (!ready || dir.empty())
		return;
	path = dir + "/session.gbx";
	w.u32(SESSION_MAGIC);
	w.u32(SESSION_VERSION);
	w.bytes(savePattern(pat));
	w.i32(mainProfile);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.i32(trackSel[t]);
	w.i32(key);
	w.f32(color);
	w.f32(swing);
	w.f32(mutateAmt);
	w.i32(lengthIdx);
	w.f32(bpm);
	w.u32(chordLock);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.f32(density[t]);
	w.f32(fillK);
	w.f32(moveK);
	w.f32(rangeK);
	w.f32(voicingK);
	w.u32(soundOn);
	w.i32(soundAll);
	w.f32(master);
	w.f32(duck);
	for (int t = 0; t < NUM_TRACKS; t++) {
		w.i32(preset[t]);
		w.f32(macA[t]);
		w.f32(macB[t]);
		w.f32(level[t]);
		w.u32(mute[t]);
		w.u32(hold[t]);
		w.f32(gain[t]);
		w.u32((uint32_t) vals[t].size());
		for (float v : vals[t])
			w.f32(v);
		w.u32((uint32_t) orig[t].size());
		for (float v : orig[t])
			w.f32(v);
	}
	// DRUM-X: the kit as edited, its parameters, the kit as loaded
	std::vector<uint8_t> kit;
	if (dx) {
		kit.resize(dx->kit_size());
		dx->kit_export(kit.data());
	}
	w.bytes(kit);
	w.bytes(dxOrigKit);
	for (int j = 0; j < JDX_NP; j++)
		w.i32(dxVals[j]), w.i32(dxOrigVals[j]);
	for (int l = 0; l < JDX_NLANE; l++)
		w.u32(laneMute[l]);
	// ABLAUF MINI
	for (int i = 0; i < NUM_PADS; i++)
		putSlot(w, slots[i]);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.i32(trackMode[t]);
	w.u32((uint32_t) song.size());
	for (const SongPart& p : song)
		w.i32(p.pad), w.i32(p.bars), w.u32(p.fill);
	}
	// the file is written outside the lock: the audio thread never waits for the storage
	std::lock_guard<std::mutex> fl(fileMtx);
	writeFile(path, w.d);
}

bool Engine::loadSession() {
	std::vector<uint8_t> d;
	if (dir.empty() || !readFile(dir + "/session.gbx", d))
		return false;
	Reader r(d);
	if (r.u32() != SESSION_MAGIC || r.u32() != SESSION_VERSION)
		return false;
	Pattern p;
	if (!loadPattern(r.bytes(), p))
		return false;
	pat = p;
	mainProfile = clampv(r.i32(), 0, numProfiles() - 1);
	for (int t = 0; t < NUM_TRACKS; t++) {
		int s = r.i32();
		trackSel[t] = s < 0 ? -1 : clampv(s, 0, numProfiles() - 1);
	}
	key = clampv(r.i32(), 0, 11);
	color = c01(r.f32());
	swing = clampv(r.f32(), -1.f, 1.f);
	mutateAmt = c01(r.f32());
	lengthIdx = clampv(r.i32(), 0, 3);
	bpm = clampv(r.f32(), 40.f, 220.f);
	chordLock = r.u32() != 0;
	for (int t = 0; t < NUM_TRACKS; t++)
		density[t] = c01(r.f32());
	fillK = c01(r.f32());
	moveK = c01(r.f32());
	rangeK = c01(r.f32());
	voicingK = c01(r.f32());
	soundOn = r.u32() != 0;
	soundAll = r.i32() == 1 ? 1 : 2;
	master = c01(r.f32());
	duck = c01(r.f32());
	for (int t = 0; t < NUM_TRACKS; t++) {
		int pr = r.i32();
		float ma = r.f32(), mb = r.f32(), lv = r.f32();
		bool mu = r.u32() != 0, ho = r.u32() != 0;
		float g = r.f32();
		std::vector<float> v(r.u32());
		if (v.size() > 64)
			return false;
		for (float& x : v)
			x = r.f32();
		std::vector<float> o(r.u32());
		if (o.size() > 64)
			return false;
		for (float& x : o)
			x = r.f32();
		if (!r.ok)
			return false;
		macA[t] = c01(ma), macB[t] = c01(mb), level[t] = c01(lv), mute[t] = mu, hold[t] = ho;
		if (t == T_DRUMS)
			preset[t] = clampv(pr, 0, presetCount(t) - 1), dxKit = preset[t];
		else {
			loadPreset(t, pr);
			if (v.size() == defsOf(t).size())
				vals[t] = v;
			if (o.size() == defsOf(t).size())
				orig[t] = o;
			gain[t] = g > 0.f ? g : gain[t];
			dirty[t] = true;
		}
	}
	std::vector<uint8_t> kit = r.bytes(), okit = r.bytes();
	int dv[JDX_NP], dov[JDX_NP];
	for (int j = 0; j < JDX_NP; j++)
		dv[j] = r.i32(), dov[j] = r.i32();
	for (int l = 0; l < JDX_NLANE; l++)
		laneMute[l] = r.u32() != 0;
	if (dx) {
		if (preset[T_DRUMS] < factoryCount(T_DRUMS))
			dx->load_kit((uint32_t) preset[T_DRUMS]);
		if (kit.size() == dx->kit_size())
			dx->kit_import(kit.data());
		dxOrigKit = okit.size() == dx->kit_size() ? okit : kit;
		for (int j = 0; j < JDX_NP; j++)
			dxOrigVals[j] = clampv(dov[j], 0, 127), setDxParam(j, dv[j]);
	}
	for (int i = 0; i < NUM_PADS; i++)
		if (!getSlot(r, slots[i]))
			slots[i] = Slot();
	for (int t = 0; t < NUM_TRACKS; t++)
		trackMode[t] = clampv(r.i32(), 0, 2);
	uint32_t n = r.u32();
	song.clear();
	for (uint32_t k = 0; k < n && k < 64 && r.ok; k++) {
		SongPart p;
		p.pad = clampv(r.i32(), 0, NUM_PADS - 1);
		p.bars = clampv(r.i32(), 1, 64);
		p.fill = r.u32() != 0;
		song.push_back(p);
	}
	if (!r.ok)
		song.clear();
	layoutTimeline(pat, lengthSteps());
	return true;
}

static const uint32_t USER_MAGIC = 0x52535550;  // "PUSR"

void Engine::loadUserPresets() {
	std::vector<uint8_t> d;
	if (dir.empty() || !readFile(dir + "/presets.gbx", d))
		return;
	Reader r(d);
	if (r.u32() != USER_MAGIC || r.u32() != 1)
		return;
	for (int t = 0; t < NUM_TRACKS && r.ok; t++) {
		uint32_t n = r.u32();
		for (uint32_t k = 0; k < n && k < 512 && r.ok; k++) {
			UserPreset u;
			u.name = r.str();
			u.gain = r.f32();
			uint32_t nv = r.u32();
			if (nv > 64) {
				r.ok = false;
				break;
			}
			for (uint32_t i = 0; i < nv; i++)
				u.vals.push_back(r.f32());
			u.kit = r.bytes();
			if (r.ok)
				user[t].push_back(u);
		}
	}
}
void Engine::saveUserPresets() {
	if (dir.empty())
		return;
	Writer w;
	w.u32(USER_MAGIC);
	w.u32(1);
	for (int t = 0; t < NUM_TRACKS; t++) {
		w.u32((uint32_t) user[t].size());
		for (const UserPreset& u : user[t]) {
			w.str(u.name);
			w.f32(u.gain);
			w.u32((uint32_t) u.vals.size());
			for (float v : u.vals)
				w.f32(v);
			w.bytes(u.kit);
		}
	}
	writeFile(dir + "/presets.gbx", w.d);
}

}  // namespace gbox
