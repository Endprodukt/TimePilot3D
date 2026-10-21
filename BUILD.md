# Bauen

Aufbau und Werkzeuge wie beim Tempest-2000-Port (`F:\Tempest 2000 Port`): dasselbe VectorPie-SDK (Kopie in `vendor/VectorPie`), dieselbe Werkzeugkette.

## PC (Windows, nativ)

Voraussetzung: MSYS2 mit MinGW-w64-GCC und SDL2 unter `C:\msys64\mingw64`, dazu CMake und Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc
build/pc/timepilot3d.exe --windowed            # 4:3
build/pc/timepilot3d.exe --windowed --vertical # 3:4
```

Tasten am PC (AdvanceMAME-Vorgaben, solange keine `advmame.rc` etwas anderes sagt): Pfeiltasten steuern (hoch = Yoke nach vorn = Sturzflug, siehe `invert_pitch`), Strg links = Feuer, Alt links = schneller, Leertaste = langsamer, Shift links = Blick nach hinten, 5 = Münze, 1 = Start und im Spiel Ansicht wechseln, F2 = Service-Menü, Esc = Ende. Ein Joystick wird als Yoke gelesen (`p1_stickx`, `p1_sticky`).

`VP_STATS=1` gibt einmal pro Sekunde die Kosten eines Bildes aus (Linien, Sprünge, Weglängen). Gemessen am PC im Demo: im Schnitt 550 Linien und 220 Sprünge je Bild, Spitzen bei 1250 Linien und 500 Sprüngen.

PC-Testversion für andere (seit 0.65.0): Das Programm braucht außer Windows nur `SDL2.dll` (alles andere ist eingebaut, geprüft mit `ldd` und einem Start ohne MinGW im Pfad). Ein Ordner `build/TimePilot3D-<version>-PC` mit `timepilot3d.exe`, `SDL2.dll`, `game.ini`, `sounds/`, `music/` und `LIESMICH.txt` (Start, Tasten, Zurücksetzen), gepackt als `build/TimePilot3D-<version>-PC.zip` (rund 12 MB). Ohne `save/`: Die Tester beginnen mit der Einrichtung des ersten Starts.

## Raspberry Pi (arm64)

Ohne WSL: clang und lld vom PC, gegen eine Kopie der Bibliotheken und Header des Automaten. Die Kopie liegt schon beim Tempest-Port (`F:\Tempest 2000 Port\build\pi-sysroot`, Anleitung zum Anlegen in dessen `BUILD.md`); sie wird hier mitbenutzt. Der Automat (192.168.0.122) gibt sein Dateisystem als Laufwerk P: frei.

Bauen (clang aus `C:/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin` im PATH):

```bash
export PATH="/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release "-DTP3D_PI_SYSROOT=F:/Tempest 2000 Port/build/pi-sysroot"
cmake --build build/pi
```

Ohne `TP3D_PI_SYSROOT` wird `build/pi-sysroot` im Projekt erwartet.

Auf den Automaten kopieren (der Ordner ist über P: beschreibbar):

```bash
D=/p/persistent/games/timepilot3d; mkdir -p $D && cp build/pi/timepilot3d $D/timepilot3d.new && cp package/game.ini package/marquee.png package/run.sh $D/ && ssh -i ~/.ssh/id_ed25519_vectorpie pi@192.168.0.122 'cd /persistent/games/timepilot3d && chmod 755 timepilot3d.new run.sh && mv -f timepilot3d.new timepilot3d'
```

Auf dem Automaten starten, vom PC aus. `run.sh` beendet das Menü, startet das Spiel auf dem Vektor-Board und holt das Menü danach zurück:

```bash
ssh -i ~/.ssh/id_ed25519_vectorpie pi@192.168.0.122 'chmod +x /persistent/games/timepilot3d/run.sh; nohup setsid /persistent/games/timepilot3d/run.sh > /dev/null 2>&1 < /dev/null &'
```

Die Bildkosten stehen danach laufend in `run.log` im Spielordner (`VP_STATS=1`).

## Auf den Automaten spielen und neu starten

`sh tools/deploy.sh` (aus dem Projektordner, Git Bash) kopiert `build/pi/timepilot3d` und `package/game.ini` auf den Automaten, beendet ein laufendes Spiel, setzt das neue Programm ein und startet es über `run.sh`. So läuft nach jedem Aufspielen sichtbar die neue Version (Wunsch des Nutzers); die Versionsnummer steht klein unten rechts auf dem Titelbild. Beim Verlassen des Spiels kommt das Menü des Automaten zurück.

## Bildkosten auf dem Automaten messen

`tools/bench.sh` liegt auch im Spielordner auf dem Automaten. Es hält das Menü an wie `run.sh`, lässt das Spiel eine Zeit lang mit `VP_STATS` auf dem Board laufen und schreibt die Kosten nach `bench.log`:

```bash
ssh -i ~/.ssh/id_ed25519_vectorpie pi@192.168.0.122 'cd /persistent/games/timepilot3d && ./bench.sh 60 TP3D_SELFTEST=1,9 TP3D_LEVEL=3 TP3D_KILLS=56 && grep stats bench.log | tail -n 20'
```

Worauf es ankommt: `lit length` plus `blank length` je Bild. Ab rund 300.000 blockiert das Board, zu sehen an `send` über 20 ms und einer Bildrate unter 40.

## Eigene Ergänzung im SDK

`vendor/VectorPie/sdk` ist nicht mehr unverändert: `vp_dvg_set_scale(dvg, scale)` in `include/vectorpie/vp_dvg.h` und `src/dvg/vp_dvg.cpp` zeichnet das ganze Bild kleiner um die Mitte (beim Beenden des Bildes, also auch Text, 3D, Fenster und Statistik). Beide Stellen sind mit "local addition" markiert. Bei einem neuen SDK-Stand muss die Ergänzung wieder hinein; sonst fehlt nur die Einstellung PICTURE SIZE.

## Einstellungen in game.ini

`package/game.ini` wird neben die ausführbare Datei kopiert und beim Start gelesen. Das Service-Menü (TAB) schreibt seine Änderungen nach `save/settings.ini`, das vor `game.ini` gilt.

- `lives`: Flugzeuge je Spiel, 1 bis 9. `difficulty`: `easy`, `normal`, `hard`, `ace`. `bonus_plane`: Punktzahl für das erste Extra-Flugzeug. `enemies_per_era`: Abschüsse bis zum Boss, 8 bis 96. `start_era`: Ära, in der ein Spiel beginnt, 1 bis 5 (zum Testen).
- `free_play`, `attract_sound`: `yes` oder `no`.
- `yoke_sensitivity`: wie scharf der Yoke dreht, 1.0 ist das Tempo des Spiels. `yoke_deadzone`: Totzone um die Mitte in Prozent.
- `invert_pitch`: `no` heißt Yoke nach vorn = Sturzflug (Flugzeug), `yes` dreht es um.
- `view`: `cockpit`, `hud` (nur das HUD) oder `chase`, die Ansicht beim Spielstart; START geht im Spiel reihum (HUD, Cockpit, Verfolger); Vorgabe `hud`.
- `aim_assist`: 0 bis 100, wie stark die Schüsse auf den Gegner am Fadenkreuz gezogen werden.
- `fire_button`, `faster_button`, `slower_button`, `rear_button`: advmame.rc-Namen, oder `yoke_button1` bis `yoke_button32` für eine Taste des Yoke selbst.
- Testhilfe `TP3D_BOSS2=n`: der Boss beginnt in Phase n, von 0 gezählt (1 die zweite; hat er weniger, die letzte). Beim Mutterschiff: 1 Ufo, 2 Roboter mit Armen, 3 Roboter ohne Arme.
- Testhilfe `TP3D_WEAK=1`: jedes Teil eines Bosses fällt mit einem Treffer; Phasen und Zwischenfilme bleiben (zum Ansehen der Filme).
- Musik: ganz im Code erzeugt, neu machen unter WSL (braucht numpy): `python3 tools/music_make.py package/music` (einzelne Stücke: Namen dahinter, z. B. `tunnel`). Tempo, Tonart, Akkorde, Hook und Aufbau jedes Stücks stehen im Skript. Paket, Aufspielen und PC-Build nehmen `package/music/*.wav` mit.
- Töne aus Aufnahmen: die Rohaufnahmen liegen in `sounds-src/` (Quelle und Lizenz: freesound.org, CC0; die Nummer am Dateinamen ist die freesound-Nummer). Neu erzeugen unter WSL (braucht numpy): `python3 tools/sounds_make.py sounds-src package/sounds` (nur einzelne: ihre Namen dahinter, z. B. `kill kill_2 kill_3`). Zuschnitt, Lautstärke und Tempo je Ton in der Tabelle `SOURCES` des Skripts, eine zweite Aufnahme darunter in `LAYER`. Das Skript liest WAV und AIFF; MP3 (die öffentlichen Vorschauen von freesound) vorher in WAV wandeln - ohne ffmpeg geht das mit der Medien-Grundlage von Windows (WinRT `MediaTranscoder`, in PowerShell).
- Eigene Töne: WAV-Dateien in `package/sounds/` mit den Namen aus `package/sounds/README.txt` ersetzen die erzeugten Effekte; `tools/make_vpk.sh` und `tools/deploy.sh` nehmen sie mit. Testhilfe `TP3D_SOUNDS_OUT=<Ordner>` schreibt alle erzeugten Effekte als WAV dorthin.
- `level_select`: `yes` oder `no`. Nur solange `DEBUG_LEVEL_SELECT` eingebaut ist: bei `no` beginnt START sofort ein Spiel statt der Debug-Auswahl. Service-Menü GAME, Zeile LEVEL SELECT.
- `occlusion`: `yes` oder `no`. Bei `yes` sind Gebäude, Flugzeuge und Bosse massiv: Linien hinter ihnen (Boden, Gebäude, Flugzeuge, Funken, Schüsse) werden abgeschnitten. Service-Menü VIDEO, Zeile SOLID OBJECTS.
- `control`: `yoke` oder `joystick` (Stick mit Schaltern und vier Tasten; HOW TO PLAY zeigt dann Stick und Tasten).
- `yoke_layout`: vier Nummern, welche Yoke-Taste wo sitzt: Trigger rechts, Trigger links, Daumen rechts, Daumen links. Wird im Service-Menü abgefragt (CONTROLS, WHERE THE BUTTONS ARE) und nur von HOW TO PLAY gebraucht.
- `volume`, `effects_volume`, `engine_volume`: je 0 bis 100.
- `brightness_*`: Helligkeit je Art, 0 bis 100 (unter 15 zeichnet der Strahl nichts).
- `picture_size`: 60 bis 100, wie viel Prozent des Platinenbereichs das Bild füllt. `beam_limit`: 100 bis 250, Strahlweg je Bild in Tausend Einheiten, ab dem das Spiel ausdünnt. Beide auf der Seite MONITOR des Service-Menüs.
- `cockpit_frame` gibt es seit 0.55.0 nicht mehr; ein altes `no` wird als `view = hud` gelesen.

## Nach einem System-Update des Automaten

Ein System-Update (neues Abbild auf die andere der zwei System-Partitionen, erlebt 2026-10-11 und 2026-10-20) ersetzt alles außerhalb von `/persistent`; zurückgespielt wird nur eine feste Liste (`vectorpie-restore-snapshot`: Netz, SSH-Hostschlüssel, `gamelist.ini`, Menü-Einstellungen, Highscores einiger mitgelieferter Spiele). Darum steht Time Pilot 3D seit 2026-10-21 **nicht** mehr in `gamelist.ini`: Das Menü findet es wie Vectortron und die anderen SDK-Spiele selbst über `/persistent/games/timepilot3d/game.ini` (im `vector_pie_menu.log` als `packages: timepilot3d ...`), nimmt Marquee und Highscores aus dem Ordner (`marquee.png`, `save/highscores.txt`) und setzt beim Start `VP_OUTPUT`. Alles liegt auf `/persistent` und übersteht Updates; Startverweise, `timepilot3d_dir` und eine Marquee-Kopie in `artwork/` braucht es nicht mehr (entfernt; die alte `gamelist.ini` liegt als `gamelist.ini.bak-2026-10-21` im Spielordner). Nach einem Update fehlt nur der SSH-Schlüssel in `/home/pi/.ssh/authorized_keys` (Kennwort nötig, macht der Nutzer). Über das Netzlaufwerk lassen sich Dateien überschreiben, aber keine Verweise anlegen und keine Rechte setzen.

## Menü-Eintrag auf dem Automaten

Am einfachsten über das Paket (unten): Das Menü legt dann alles selbst an. Von Hand: der Ordner `/persistent/games/timepilot3d` mit Programm, `game.ini`, `marquee.png`, `run.sh`, `sounds`, `music` und `save` (das macht `tools/deploy.sh`); das Menü findet ihn beim nächsten Start. Keine Zeile in `gamelist.ini`, keine Verweise in `/usr/local/share/advance` (so war es bis 2026-10-21; das überlebte keine System-Updates).

## Spielstände

`save/highscores.txt` (Bestenliste, vom SDK geschrieben) und `save/settings.ini` (Service-Menü). Zum Zurücksetzen löschen.

## Testhilfen

- `TP3D_LEVEL=3` startet jedes Spiel in der dritten Ära (1970).
- `TP3D_KILLS=55` lässt jedes Level mit 55 Abschüssen beginnen (der Boss kommt nach dem nächsten), `56` ruft den Boss sofort, `57` dazu mit nur einem Trefferpunkt.
- `TP3D_ITEMS=1` gibt jedem neuen Flugzeug Schild, Raketen und Dreifachschuss.
- `TP3D_PAGE=0..4` beginnt den Attract-Modus auf einer Seite: 0 Titel, 1 Bestenliste, 2 Gegnerübersicht, 4 How to play, 3 Demo (mit `TP3D_LEVEL` in der gewünschten Ära).
- `TP3D_KILLS=57` zusammen mit `TP3D_SELFTEST=1,3` zeigt den Zeittunnel nach wenigen Sekunden, vom Autopiloten geflogen. Mit `TP3D_HANDSOFF=1` dazu steuert niemand: So sieht man, was die Wand tut.
- `SDL_AUDIODRIVER=dummy` für stille Läufe.

- `TP3D_STUNT=1`: im Selbsttest zieht das Flugzeug alle zwölf Sekunden einen halben Looping und rollt sich dann aufrecht (prüft die Rolle). `TP3D_BOSS2=1`: jeder Boss kommt gleich in der letzten Stufe seines Kampfs (mit `TP3D_KILLS=56` sofort zu sehen). `TP3D_SERVICE=1`: das Service-Menü ist von Anfang an auf der Seite MONITOR offen. `TP3D_PAGE=5`: die Attract-Seite ITEMS; `6`: die Items der Anleitung nach START. `TP3D_CONTINUE=1`: ein Spiel, dessen letztes Flugzeug gerade verloren ist (Countdown des Continue). `TP3D_SETUP=n`: die Einrichtung des ersten Starts ist offen, auch wenn sie schon durchlaufen wurde, auf Seite n (0 die Frage Yoke/Joystick, 1 die Yoke-Tasten, gezeichnet als wäre ein Yoke da, 2 die empfohlene Belegung, 3 Joystick-Empfindlichkeit, 4 Joystick-Tasten, 5 deren Belegung). Ohne `save/settings.ini` mit `setup_done = yes` beginnt jeder Start mit der Einrichtung; Selbsttest, `TP3D_PAGE`, `TP3D_SERVICE` und `TP3D_DEBUGSEL` übergehen sie. `TP3D_CHUTES=1`: jede Ära beginnt mit zwei Fallschirmspringern vor dem Flugzeug, einer davon getroffen. Mit `TP3D_SELFTEST=1,5` läuft ein echtes Spiel mit Autopilot in Echtzeit und mit Bild, gut für Aufzeichnungen.

## Bildschirmfoto des Spielfensters

`tools/snap.ps1 -Out <pfad> [-Keys ...] [-ExeArgs ...]` startet das Spiel, schickt Tasten an das Fenster und speichert nur dieses Fenster als PNG. Tasten als virtuelle Tastencodes in Hex, `+`/`-` für gedrückt halten/loslassen, `wNNN` wartet, `snap` speichert ein Zwischenbild. `-Screen` holt sich das Bild vom Bildschirm statt über PrintWindow. Seit dem 2026-10-10 liefern beide Wege nur Schwarz (Ursache offen); stattdessen die Aufzeichnung im nächsten Abschnitt nehmen. Beispiel: `-Keys "w300,35,w300,31,w3000,snap,+11,w2000,snap,-11"` (Münze, Start, feuern).

## Bilder ohne Monitor (Aufzeichnung der Platinen-Daten)

Der sichere Weg, eine Szene anzusehen: Das Spiel läuft unter Linux (WSL `Ubuntu-24.04` auf dem PC) gegen eine vorgetäuschte Vektorplatine, und aus dem mitgeschnittenen Datenstrom werden PNGs gerechnet. Was man sieht, ist genau das, was die Platine bekäme, samt Letterbox-Ausschnitt.

```bash
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' wsl.exe -d Ubuntu-24.04 -- bash "/mnt/f/Time Pilot 3D/tools/record.sh" 57 /mnt/c/pfad/lauf.rec TP3D_LEVEL=1 TP3D_KILLS=57 TP3D_HANDSOFF=1
python tools/dvgview.py C:/pfad/lauf.rec C:/pfad/bild 33 39 0.3
```

`tools/record.sh <sekunden> <datei> [NAME=wert ...]` baut nach `~/tp3d-build` (nicht `/tmp`, das leert WSL) und startet `tools/fakedvg.py`; `tools/dvgview.py <datei> <präfix> [von bis schritt]` schreibt je Schritt ein Bild und nennt Linienzahl und Strahlweg (braucht Pillow). Ohne die beiden `MSYS`-Variablen schreibt die Git-Bash die Linux-Pfade um; `$VARIABLEN` in einer `wsl.exe`-Zeile werden vorher ersetzt, deshalb ein Skript. `fakedvg.py` läuft auch auf dem Automaten (dort `SDL_VIDEODRIVER=dummy` setzen, weil das Menü den Bildschirm hält). Mit TP3D_KILLS=57 beginnt der Boss-Absturz nach etwa 23 s, der Anflug nach etwa 33 s, der Tunnel nach 37 s, die nächste Ära nach 51 s.

## Selbsttest

Das Spiel spielt sich selbst (Autopilot, unendlich Leben) und meldet jede Ära auf der Fehlerausgabe. Im Ordner `build/pc`:

```bash
TP3D_SELFTEST=300,6 SDL_AUDIODRIVER=dummy ./timepilot3d.exe --windowed 2> selftest.txt
```

Die zwei Zahlen: Schritte je Bild (300 heißt eine Ära in rund einer Minute) und wie viele Ären. Je Ära eine Zeile mit Bildern, Toden, Punkten und einem Fingerabdruck des ganzen Spielzustands. Der Selbsttest ist deterministisch: Zwei Läufe liefern dieselben Zeilen, und jede Änderung am Verhalten ändert die Fingerabdrücke. So lässt sich ein Umbau prüfen, der nichts ändern soll: Lauf vorher und nachher vergleichen (im Selbsttest ist der Lastregler des Strahls abgeschaltet, der sonst vom echten Bildtakt abhängt); eine Ära, die nach 30000 Bildern nicht endet, wird als `stuck` gemeldet und das Programm beendet. Bestenliste und Einstellungen werden nicht angefasst. Gelesen werden die Einstellungen aber: Eine `save/settings.ini` neben dem Programm (etwa mit anderer Empfindlichkeit oder Steuerart) ändert den Lauf und die Fingerabdrücke - zum Vergleichen ohne sie laufen lassen.

## Paket für den Automaten

Nach dem Pi-Build:

```bash
sh tools/make_vpk.sh
```

Das schreibt `build/timepilot3d-<version>.vpk`, und zwar immer auch in den `build`-Ordner des Projekts selbst (`F:\Time Pilot 3D\build`), wenn in einem Worktree gebaut wird: Jede Version gehört dorthin (Regel des Nutzers, 2026-10-21). `tools/deploy.sh` ruft `make_vpk.sh` nach jedem Aufspielen auf, also liegt das Paket jeder aufgespielten Version dort. Auf einen USB-Stick in den Ordner `games` kopieren und am Automaten über Settings, GAMES, INSTALL FROM USB installieren.

Veröffentlicht wird das Paket im geteilten Google-Drive-Ordner `games` (https://drive.google.com/drive/folders/1N5vPb5K46OlYJ-xtTOY6IWD4E5GqMYiS), wie die anderen VectorPie-Spiele. Auf diesem PC ist Drive nicht eingebunden. Über Claude in Chrome ging es bei kleineren Paketen (Tempest Enhanced: New, File upload, Dateifeld per JavaScript im Fenster halten, dann `file_upload`); die Erweiterung lädt aber höchstens 10 MB je Aufruf hoch, und das Paket hat mit Musik rund 12 MB. Darum zieht der Nutzer die Datei selbst in den Ordner (2026-10-21: 0.62.0). Dort liegt noch `timepilot3d-0.51.0.vpk`.
