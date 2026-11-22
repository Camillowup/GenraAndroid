# Übersicht (wo liegt was)

- **App-Engine:** `app/src/main/cpp/box/BoxEngine.*` – Sequencer (wie GENRAs Tick), die vier Stimmen, ABLAUF MINI,
  UNDO/REDO, GRID-Schnittstelle, Projekte (`projects/*.gbx`, `current.txt`), Einstellungen (`settings.gbx`: PERFORM-Pads,
  Link) und eigene Presets (`presets.gbx`) im App-Ordner. `PerformFx.hpp`: die 16 PERFORM-Effekte und der DJ-EQ.
- **Ableton Link:** Link-3.1.5 per FetchContent in `CMakeLists.txt` (nur im Android-Build; `GENRA_LINK`).
- **Brücke + Audio:** `app/src/main/cpp/box/jni.cpp` (AAudio, Ausgabe mit der Rate des Geräts; DRUM-X läuft mit 44,1 kHz
  und wird umgerechnet).
- **GENRA-Code:** `app/src/main/cpp/genra-src/` – Kopie aus Genra-for-Vcv (`src/genra`, `mono`, `funke`, `schmelz`,
  `GenreMatch.hpp`, `OutLimit.hpp`). Bei Änderungen dort von Hand nachziehen.
- **DRUM-X-Core:** `app/src/main/cpp/drumx/` (`jiantcore/drumcore.c` + ausgepackte Firmware `jiant/`).
- **Oberfläche:** `app/src/main/assets/ui/` (`app.js`, `perform.js`, `grid.js`, `style.css`, Schrift Inter). Eine Bühne 2400 × 1080, auf den
  Bildschirm skaliert. Die Befehlsnummern (`OP`) müssen mit `gbox::Op` übereinstimmen.
- **Android:** `app/src/main/java/.../MainActivity.java` (WebView, MIDI-Export nach `Download/GENRA BOX`).
- **Build:** `.github/workflows/build.yml` → APK als Artifact (kein Release). Schlüssel: `app/genrabox.keystore`.
- **UI am PC testen:** `tools/devserver/`.
