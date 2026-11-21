// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android: the whole groovebox in one object, without Rack.
// GENRA's generator (genra-src/genra), the four voices of GENRA BOX (DRUM-X core, GENRA MONO, FUNKE, SCHMELZ),
// ABLAUF MINI (16 pads, parts, song) and the app's own state (sessions, user presets).
// Threads: every public method takes the engine's mutex. The audio callback calls render(); the UI (WebView bridge)
// calls the rest. Nothing here blocks for long: generating a pattern takes well under a millisecond.
#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../genra-src/genra/Arranger.hpp"
#include "../genra-src/genra/Engine.hpp"
#include "../genra-src/funke/FunkePresets.hpp"
#include "../genra-src/mono/MonoPresets.hpp"
#include "../genra-src/schmelz/SchmelzPresets.hpp"
#include "../drumx/jiantcore/drumcore.h"

namespace gbox {

using namespace genra;

// ---- the commands of the UI (the same numbers in assets/ui/app.js)
enum Op {
	OP_PLAY = 1, OP_STOP = 2, OP_NEWALL = 3, OP_LOCK = 4, OP_SOUND = 5, OP_SOUNDMODE = 6,
	OP_BPM = 10, OP_BPM_GENRE = 11, OP_KEY = 12, OP_COLOR = 13, OP_SWING = 14, OP_LENGTH = 15, OP_MUTATE = 16,
	OP_MASTER = 17, OP_DUCK = 18,
	OP_GENRE = 20, OP_TGENRE = 21, OP_DENS = 22, OP_TK2 = 23, OP_TNEW = 24, OP_HOLD = 25, OP_PRESET = 26,
	OP_MACRO = 27, OP_LEVEL = 28, OP_MUTE = 29,
	OP_SYN = 30, OP_SYN_INIT = 31, OP_SYN_UNDO = 32, OP_SYN_AB = 33, OP_SYN_TEST = 34, OP_SYN_MARK = 35,
	OP_DX_SND = 40, OP_DX_KIT = 41, OP_DX_LANEMUTE = 42,
	OP_AB_PART = 50, OP_AB_PAD = 51, OP_AB_UNDO = 52, OP_AB_CLEAR = 53, OP_AB_MODE = 54, OP_AB_SONG = 55,
	OP_AB_NEXT = 56,
	OP_PRESET_DELETE = 61,
};

/** One knob / switch / list of a synth page. */
struct ParamDef {
	const char* key;
	const char* label;
	float min, max, def;
	int kind;                       // 0 knob, 1 list, 2 switch
	std::vector<std::string> opts;  // list / switch: the names
};

struct UserPreset {
	std::string name;
	std::vector<float> vals;        // tonal voices: the values; DRUM-X: the kit's parameters (JDX_NP)
	std::vector<uint8_t> kit;       // DRUM-X: the kit's sounds (kit_export)
	float gain = 1.f;
};

/** One mono note channel as GENRA's outputs give it (gate with retrigger, slide). */
struct MonoGate {
	float pitch = 0.f, vel = 0.8f;
	bool acc = false;
	float gate = 0.f, retrig = 0.f;
	bool slidePending = false;
	float slideOut = 0.f;
};
struct PadGate {
	float pitch[4] = {0.f, 0.f, 0.f, 0.f};
	float vel = 0.7f;
	float gate = 0.f, retrig = 0.f;
};

static const int NUM_PADS = 16;

class Engine {
public:
	Engine();
	~Engine();

	/** filesDir: where sessions and user presets live. */
	void init(const std::string& filesDir);
	void setSampleRate(float sr);
	/** Audio thread: n stereo frames, -1..1. */
	void render(float* L, float* R, int n);

	void cmd(int op, int a, int b, float f);
	void savePreset(int track, const std::string& name);
	std::string meta();
	/** The full state as JSON; "" when the version is still `known`. */
	std::string state(int known);
	/** The fast-changing state (step, lights) as JSON. */
	std::string fast();
	/** MIDI: what 0 the loop, 1 the song, 2 every pad, 3 one pad (idx). Empty when there is nothing. */
	std::vector<uint8_t> exportMidi(int what, int idx, std::string& name);
	void saveSession();
	std::string message();

private:
	std::recursive_mutex mtx;
	float sr = 48000.f;
	std::string dir;
	bool ready = false;

	// ---------------------------------------------------------------- GENRA
	Pattern pat;
	Rng rng;
	int mainProfile = 0;            // the genre field (a profile: genre + variant)
	int trackSel[NUM_TRACKS] = {-1, -1, -1, -1};
	int key = 9;
	float color = 0.5f, swing = 0.f, mutateAmt = 0.f;
	int lengthIdx = 2;              // 1 2 4 8 bars
	float bpm = 120.f;
	bool chordLock = false;
	float density[NUM_TRACKS] = {0.5f, 0.5f, 0.5f, 0.5f};
	float fillK = 0.3f, moveK = 0.5f, rangeK = 0.5f, voicingK = 0.5f;

	bool running = false;
	int step = -1;
	float phase = 0.f, stepPeriod = 6000.f;
	bool resetArmed = true, firstPass = true, fillActive = false;
	int passCount = 0;
	int64_t barIndex = -1;
	uint32_t patChanges = 1;
	int padPrev[4] = {60, 64, 67, 72};
	bool padHasPrev = false;

	// swing-delayed events of the step
	Hit qDrums[NUM_DRUMS];
	float dDrums = -1.f, dBass = -1.f, dMel = -1.f, dPad = -1.f;
	NoteEv qBass, qMel;
	PadEv qPad;
	struct DrumVoice {
		float delayCnt = -1.f, pVel = 0.f;
		int pRat = 1;
		int ratLeft = 0;
		float ratInterval = 0.f, ratCounter = 0.f, vel = 0.f;
	} drv[NUM_DRUMS];
	MonoGate bassV, melV;
	PadGate padV;
	float lastHit[NUM_DRUMS] = {};  // lights

	// ---------------------------------------------------------------- the voices
	const jdx_api* dx = nullptr;
	int dxKit = 0;
	int dxVals[JDX_NP] = {};        // the kit's parameters as set (TUNE, DECAY: before the macros)
	bool laneMute[JDX_NLANE] = {};
	double dxPos = 0.0;             // the resampler: position between dxBuf[1] and dxBuf[2]
	float dxBufL[4] = {}, dxBufR[4] = {};
	std::vector<uint8_t> dxOrigKit; // the loaded kit (INIT)
	int dxOrigVals[JDX_NP] = {};

	std::unique_ptr<gmono::Voice> monoV;
	std::unique_ptr<gfunke::Engine> funkeE;
	gfunke::Channel funkeCh;
	static const int NP = 4;
	std::unique_ptr<gschmelz::StringsVoice[]> sv;
	std::unique_ptr<gschmelz::SoloVoice[]> so;
	std::unique_ptr<gschmelz::Bus> sbus;
	bool pGate[NP] = {}, strSide[NP] = {}, soloSide[NP] = {}, held[NP] = {};
	float lastV[NP] = {1e9f, 1e9f, 1e9f, 1e9f}, hz[NP] = {};

	// per track: the sound (index 0 drums, 1 bass, 2 melody, 3 pads)
	int preset[NUM_TRACKS] = {0, 0, 0, 0};
	float macA[NUM_TRACKS] = {0.5f, 0.5f, 0.5f, 0.5f}, macB[NUM_TRACKS] = {0.5f, 0.5f, 0.5f, 0.5f};
	float level[NUM_TRACKS] = {0.75f, 0.75f, 0.75f, 0.75f};
	bool mute[NUM_TRACKS] = {};
	bool hold[NUM_TRACKS] = {};
	bool soundOn = true;
	int soundAll = 2;               // 2 same genre, 1 all genres
	float master = 0.75f, duck = 0.f;
	float duckEnv = 0.f;
	bool duckRise = false;
	float muteGain[NUM_TRACKS] = {1.f, 1.f, 1.f, 1.f};

	// the tonal voices' values (synth pages), the loaded preset's values (INIT) and gain
	std::vector<float> vals[NUM_TRACKS], orig[NUM_TRACKS];
	float gain[NUM_TRACKS] = {1.f, 1.f, 1.f, 1.f};
	bool dirty[NUM_TRACKS] = {true, true, true, true};
	gmono::Params monoP;
	gfunke::Params funkeP;
	gschmelz::StringsParams padSP;
	gschmelz::SoloParams padOP;
	gschmelz::BusParams padBP;
	int padSplit = 0;
	bool padHold = false;
	float padReg = 0.f;
	// undo of the synth pages and A/B (edit / the preset as loaded)
	struct SynSnap {
		std::vector<float> vals;
		std::vector<uint8_t> kit;
		int dx[JDX_NP];
	};
	std::vector<SynSnap> synUndo[NUM_TRACKS];
	bool abOrig[NUM_TRACKS] = {};
	SynSnap abEdit[NUM_TRACKS];
	std::vector<UserPreset> user[NUM_TRACKS];
	std::vector<std::vector<int>> sameGenre[NUM_TRACKS];

	// ---------------------------------------------------------------- ABLAUF MINI
	Slot slots[NUM_PADS];
	ArrMods arrMods;
	int playingSlot = -1;           // the pad that plays (unchanged since its cue: patChanges == patAtCue)
	uint32_t patAtCue = 0;
	struct Cue {
		bool on = false, now = false, stop = false, fill = false;
		int slot = -1, rowBars = 0, row = -1;
		int64_t atBar = 0;
	} cueQ;
	int cuedSlot = -1;
	int64_t arrRowEnd = -1;
	int arrRowBars = 0;
	bool arrFill = false;
	int arrFillEnd = -1;
	int trackMode[NUM_TRACKS] = {1, 1, 0, 0};  // Keep 0, Re-roll 1, Vary 2
	PartOpts partOpts;
	struct SongPart {
		int pad, bars;
		bool fill;
	};
	std::vector<SongPart> song;
	bool songOn = false;
	int songPos = -1;
	int64_t songRowStart = 0;
	struct Session {
		bool active = false;
		Slot source;
		Params P;
		std::vector<std::pair<int, uint32_t>> made;
	} session;
	uint32_t slotVer[NUM_PADS] = {};
	struct PadUndo {
		int pad;
		Slot before;
		std::shared_ptr<std::vector<Slot>> all;
	};
	std::vector<PadUndo> padUndo;
	bool clearArmed = false;
	std::string msg;
	uint32_t msgId = 0;

	int stateVer = 1;
	std::mutex fileMtx;             // one session write at a time

	// ---------------------------------------------------------------- helpers
	void changed() { stateVer++; }
	void say(const std::string& s) {
		msg = s;
		msgId++;
		changed();
	}
	int lengthSteps() const;
	Params buildParams(bool withArr = true);
	int trackTarget(int t) const;
	void doGenerate(bool all);
	void newTrack(int t);
	void rollPreset(int t);
	int presetCount(int t) const;
	int factoryCount(int t) const;
	std::string presetName(int t, int i) const;
	void loadPreset(int t, int i);
	void applyVals(int t);
	void setDxParam(int j, int v);
	void dxApplyMacros();
	SynSnap snapSyn(int t);
	void restoreSyn(int t, const SynSnap& s);
	void markUndo(int t);

	void restart();
	void doTick();
	void processQueued();
	void flushQueued();
	void fireDrum(int v, float vel, int ratchet);
	void queueDrum(int v, const Hit& h);
	void startMono(MonoGate& m, const NoteEv& n);
	void startPad(const PadEv& e);
	float swingAt(const Params& P, int t, int st) const;
	PartPos curPartPos(int L) const;
	void dxHit(int lane, int vel);
	void renderVoices(float& outL, float& outR);

	// ABLAUF MINI
	void cuePad(int i);
	void applyCue(const Cue& c, bool atBarLine);
	int64_t loopEndBar() const;
	Slot snapshot();
	int playingPad() const;
	void tapPad(int i);
	void storeSnap(int i);
	void partButton(int k);
	void makePart(int k);
	void songButton();
	void makeSong(int sel);
	void startSong();
	void stopSong(bool stopPlay);
	void songBar();
	int freePad() const;
	int newFamily() const;
	int familySource(int i) const;
	int lastVariant(int k, int family, int skip) const;
	std::string nextName(const std::string& base, int skip, int family) const;
	void remember(int i);
	void rememberAll();
	unsigned rerollMask() const;
	unsigned varyMask() const;

	// storage
	bool loadSession();
	void loadUserPresets();
	void saveUserPresets();
};

}  // namespace gbox
