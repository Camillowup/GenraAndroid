# Übersicht (wo liegt was)

- **App-Engine:** `app/src/main/cpp/box/BoxEngine.*` – Sequencer (wie GENRAs Tick), die vier Stimmen, ABLAUF MINI,
  Session und eigene Presets (`session.gbx`, `presets.gbx` im App-Ordner).
- **Brücke + Audio:** `app/src/main/cpp/box/jni.cpp` (AAudio, Ausgabe mit der Rate des Geräts; DRUM-X läuft mit 44,1 kHz
  und wird umgerechnet).
- **GENRA-Code:** `app/src/main/cpp/genra-src/` – Kopie aus Genra-for-Vcv (`src/genra`, `mono`, `funke`, `schmelz`,
  `GenreMatch.hpp`, `OutLimit.hpp`). Bei Änderungen dort von Hand nachziehen.
- **DRUM-X-Core:** `app/src/main/cpp/drumx/` (`jiantcore/drumcore.c` + ausgepackte Firmware `jiant/`).
- **Oberfläche:** `app/src/main/assets/ui/` (`app.js`, `style.css`, Schrift Inter). Eine Bühne 2400 × 1080, auf den
  Bildschirm skaliert. Die Befehlsnummern (`OP`) müssen mit `gbox::Op` übereinstimmen.
- **Android:** `app/src/main/java/.../MainActivity.java` (WebView, MIDI-Export nach `Download/GENRA BOX`).
- **Build:** `.github/workflows/build.yml` → APK als Artifact (kein Release). Schlüssel: `app/genrabox.keystore`.
- **UI am PC testen:** `tools/devserver/`.
