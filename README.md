# GENRA BOX for Android

The groovebox of the [GENRA VCV Rack plugin](https://github.com/Camillowup/Genra-for-Vcv) as an Android app for phones
held sideways: GENRA's generator with its four voices (DRUM-X, GENRA MONO, FUNKE, SCHMELZ) and ABLAUF MINI, playing on
the phone itself, without Rack and without a computer.

## Pages

| Page | What it does |
| --- | --- |
| **BOX** | Project, genre, BPM, KEY and SCALE, DUCK, COLOR, SWING, LENGTH, MUTATE, MASTER, PLAY / STOP / NEW ALL / CHORDS, UNDO. Per track: LOCK, pattern genre, DENSITY and a second knob, NEW, HOLD, the preset, two macros, LEVEL, MUTE. |
| **ABLAUF** | ABLAUF MINI: the part buttons (INTRO … OUTRO) put a new variant on the next free pad; 16 pads; KEEP / RE-ROLL / VARY per track; MAKE SONG builds and plays a song of the genre's form. |
| **PERFORM** | 16 effect pads (REPEAT, FILTER, TIME, COLOR; hold, double tap = latch, long press = choose the effect), XY field, the tracks the effects work on, MUTE, DJ EQ with kills, FILL and BREAK to hold. |
| **GRID** | Piano roll for bass, melody and pads (ALL / SCALE / USED, chords, VEL lane), step grid for the drums; two bars at a time; VEL, CHANCE, SLIDE / ROLL, UNDO, REDO, COPY / PASTE BAR, CLEAR, NEW, length of the track. |
| **DRUM-X** | The kit, its 8 sounds (PITCH, PMOD, DECAY, NOISE, COLOR for patch A and B, LEVEL, MUTE), the whole kit's macros (MORPH …) and the sends. |
| **MONO / FUNKE / SCHMELZ** | Every parameter of the synth. SAVE (your own presets), INIT, UNDO, A / B (compare with the preset as loaded), TEST. |
| **SETUP** | Ableton Link (on / off, peers, start / stop sync, offset), MIDI export (loop, song, all pads → `Download/GENRA BOX`), audio info. |

Knobs: drag up or to the right; double tap sets the default (on the synth pages: the preset's value).
Projects (several songs) and your presets are kept when the app closes.

## Install

Every push to `main` builds the APK in GitHub Actions (**Actions › Build APK › Artifacts › GenraBox-APK**).
Unzip it, copy the `.apk` to the phone and open it (allow installing apps from that source once).
All builds are signed with the same key (`app/genrabox.keystore`), so a new build installs over the old one.

Needs Android 8.0 or newer on a 64-bit phone (arm64).

## Building

- Android Studio, or `gradle assembleRelease` with the Android SDK, NDK and CMake 3.22.1 (Gradle 8.9, JDK 17).
- `app/src/main/cpp/genra-src`: GENRA's engine and the voices, copied from Genra-for-Vcv.
- `app/src/main/cpp/drumx`: the DRUM-X core (the JIANT FM firmware's DRUM engine) with the firmware's sources.
- `app/src/main/cpp/box`: the app's engine (sequencer, voices, ABLAUF MINI, PERFORM effects, projects), the JNI bridge
  and AAudio. Ableton Link (Link-3.1.5) is fetched by CMake when the build is configured.
- `app/src/main/assets/ui`: the panel (HTML / JS in a WebView).
- `tools/devserver`: the panel in a desktop browser with the real engine behind it (no sound), for UI work:
  `tools/devserver/build.sh && python3 tools/devserver/server.py`, then open `http://localhost:8765/ui/index.html`.

## License

GPL-3.0-only (see `LICENSE`). Ableton Link is GPL-2.0-or-later. Third-party parts and their licenses: `licenses/` and
`app/src/main/cpp/drumx/jiant/LICENSING.md`.
