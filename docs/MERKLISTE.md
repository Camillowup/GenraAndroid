# Merkliste (nur Offenes)

Gesammelt für den nächsten Build (gebaut wird erst auf „bauen“):

- **Presets nach Klang** (schon im Code, noch nicht gebaut): FUNKE und SCHMELZ in BASS, LEAD, PAD, KEYS, FX wie
  GENRA 2.41 (`genra-src/ui/SoundGroups.hpp`); GENRA MONO und DRUM-X-Kits eine Liste. SAME GENRE würfelt weiter nach Genre.
- **BOX-Seite:** Felder KEY und SCALE im Display (Auswahl: 12 Grundtöne; AUTO = Skala des Genres, MAJOR, MINOR, übrige
  Skalen); Akkorde im Display; der KEY-Regler fällt weg, DUCK kommt von SETUP an seine Stelle.
- **GRID** (neuer Reiter zwischen ABLAUF und DRUM-X): Piano-Roll für Bass/Melodie/Pads (ALL/SCALE/USED, Akkordleiste,
  VEL-Spur), Drum-Raster mit 8 Zeilen; 2 Takte pro Ansicht, blättern mit < BAR / BAR >; VEL, CHANCE, SLIDE bzw. ROLL;
  UNDO, REDO, COPY BAR, PASTE, CLEAR, NEW, Länge pro Spur. Entwürfe: genra_box_android_7/8/9.

- **Ableton Link** (nur auf SETUP, nicht auf der BOX-Seite): LINK an/aus mit Anzahl Geräte, Start/Stop-Sync,
  Ausgleich der Verzögerung (auch für Bluetooth). Ableton Link ist GPLv2+, passt zur GPL-3 der App. Entwurf noch zeigen.

- **UNDO** (NEW, NEW ALL, Genre-Wechsel; mehrfach), **LOCK pro Spur** (neben PATTERN; der Akkord-LOCK heisst jetzt
  CHORDS), **Projekte** (Name unter dem Titel; NEW, SAVE AS, RENAME, DUPLICATE, DELETE). Entwürfe 10–12.
- **PERFORM-Seite** (wichtiger als PLAY, Entwurf 13): 16 Effekt-Pads zum Halten (REPEAT, FILTER, TIME, COLOR),
  Doppeltipp = LATCH, XY-Feld, Ziel-Spuren, MUTE, DJ-EQ mit KILL, FILL und BREAK zum Halten. Noch nicht festgelegt,
  in welchen Build.
- **PLAY-Seite** (nach hinten): Tasten in Tonart/Skala, LIVE pro Spur (GENRAs Noten schweigen, Pattern bleibt), REC.

Vorgeschlagen, noch nicht entschieden: Wiedergabe im
Hintergrund, WAV-Export, Mixer-Seite, PLAY-Seite, PERFORM-Seite, FILL zum Halten, Erklärung bei langem Drücken,
Vibration, DICE auf den Synth-Seiten.

Später: MIDI über USB (Controller), REC im GRID.
