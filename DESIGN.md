# Gestaltung und Stil

## Sprache

- Doku, `STATUS.md`, `RESUME.md`, Commits: Deutsch. Kommentare im Code und sichtbare Texte im Spiel: Englisch (wie beim Tempest-Port, derselbe Automat, dasselbe SDK).
- Kommentare erklären das Warum und nennen Werte, die geschätzt sind, als solche.

## Bild

- Alles ist Drahtmodell (`vp3d_draw_mesh_wire`): jede Kante sichtbar, nichts verdeckt. Das ist der Look der Vektor-Arcade (Star Wars, Battlezone) und spart die Verdeckungsrechnung auf dem Pi.
- Was den Strahl wirklich kostet, ist der Weg, hell und dunkel zusammen, nicht die Zahl der Linien (am Automaten gemessen, siehe `STATUS.md`). Darum: Zusammenhängendes in einem Zug, Verstreutes sortiert (`world_dash`), und im Kreis um das Bild statt kreuz und quer.
- Jede Linie kostet den Strahl. Modelle sind auf das reduziert, was die Sache aus der Entfernung erkennen lässt: ein, zwei Ringe für den Rumpf, Tragflächen als Umriss, Leitwerk als wenige Striche. Kleine Gegner 25 bis 45 Linien, Bosse 50 bis 80.
- Tiefe entsteht nur über Helligkeit (Nebel ab 450 m bis 1600 m, Boden 22 Prozent) und Größe. Darum sind Gegner 1,5-fach gezeichnet.
- Mehrfarbig, wo es etwas bedeutet: Rumpf, Flügel, Leitwerk oder Antrieb und Glas haben je eine Farbe, höchstens vier je Modell, damit der Strahl selten die Farbe wechselt. Die Großen sind in jeder Ära rot und gelb, damit man sie sofort als 1500er erkennt. Gebäude: Wände, Dächer und Türme getrennt, nie in den Farben der Flieger.
- Farben je Art: Boden grün (0,200,90), Wolken blaugrau, Spieler und Cockpit cyan, Fadenkreuz gelb, eigene Schüsse hellgelb, Gegnerkugeln rot, Raketen weiß, Funken orange nach rot verblassend. Gegner je Ära: 1910 Messing, 1940 grün, 1970 orange, 1982 blaugrau, 2001 magenta. Getroffene blinken weiß.
- Helligkeiten getrennt nach Art und in `game.ini` einstellbar (`brightness_*`), weil die Röhre anders wirkt als das Fenster; unter 15 Prozent zeichnet der Strahl nichts.
- HUD in Board-Einheiten, Schrift `VP_FONT_ASTEROIDS`: Punkte links oben grün, Highscore mittig blau, Jahr rechts oben gelb; unten Leben (cyan Dreiecke), Gegnerzähler (gelbe Marken), Radar (grüner Kreis). Alles, was im Attract-Modus lange steht, wandert um wenige Einheiten (`text()`).
- Die Kamera im Cockpit rollt mit der Querlage, die Verfolgerkamera nur zu einem Viertel und läuft dem Kurs nach (`cam_heading`, 12 Prozent je Schritt).

## Cockpit und HUD

- Das Flugzeug des Spielers ist in jeder Ära derselbe Jet, sein Cockpit also auch: Haubenrahmen und Instrumentenbrett blau, HUD-Symbolik grün (0,255,120), Warnungen und erfasstes Ziel rot, nicht erfasstes Ziel gelb.
- Zahlen stehen ohne Beschriftung in Pfeilkästen (Geschwindigkeit zeigt nach rechts, Höhe nach links). Skalen haben wenige Striche. Jede Linie des HUD wird in jedem Bild gezeichnet.
- Die Nickleiter wird aus echten Punkten am Horizont projiziert, nicht aus Bildschirmwinkeln gerechnet. So stimmt sie bei jeder Lage.
- In der Verfolger-Ansicht bleibt nur, was man zum Fliegen braucht.

## Gebäude

- Eigene Farbe je Ära, nie die der Gegner: Holz, Beton, Oliv, Stahlblau, Zukunftsgrün. Gezeichnet mit der Boden-Helligkeit mal 1,4.
- Bauten vor 1982 sind größer als in Wirklichkeit (Faktor 2,2 und 1,6), sonst wären sie aus der Luft Punkte.
- Ein Quader auf dem Boden zeichnet seinen Boden nicht, ein Dach nicht seine Unterkante: Die Linien wären doppelt.

## Abschüsse

- Was getroffen wird, verschwindet nie einfach. Es zerplatzt in seine eigenen Linien, brennt, verliert Teile oder trudelt ab, und jedes Wrack endet in einem Einschlag.
- Trümmer glühen weiß, nehmen dann die Farbe der Ära an und verlöschen. Wracks flackern zwischen Ärenfarbe und Feuer. Rauch sind kurze graue Striche, die stehen bleiben und steigen.
- Druckwellen in der Luft sind dem Betrachter zugewandte Ringe, am Boden liegen sie flach.
- Der Boss bekommt den größten Abgang und hält dafür den Zeittunnel knapp drei Sekunden auf.

## Partikel

- Die Röhre hat Reserven (Rückmeldung vom Automaten), also wird geklotzt: Punktringe, Punktkugeln und Regenbogenfunken nach dem Vorbild von Tempest 2000. Jeder Punkt ist ein kurzer Strich nach außen.
- Farben laufen um den Farbkreis (`hue_rgb`), entlang des Rings und mit der Zeit.
- Feuer, Rauch und Weißglut bleiben daneben bestehen: Die bunten Ringe sind der Jubel, das Feuer ist der Schaden.
- Ein großer Moment bekommt ein Wort, das aus der Bildmitte wächst (`cheer`), nie mehr als eins zugleich.

## Zeittunnel

- Vierzehn Sekunden, davon knapp elf vom Spieler geflogen; hinein und hinaus wird das Flugzeug getragen.
- Das Flugzeug steht im Bild fest, der Tunnel bewegt sich um es herum. Wohin es gehen muss, zeigen die fernen Ringe und die Raute der Tunnelmitte.
- Farbe der alten Ära geht in die der neuen über, die Jahreszahl zählt in derselben Mischfarbe. Die Ringe sind abwechselnd in der Ärenfarbe und zur Hälfte aufgehellt, ferne dunkler. Vier Schienen laufen durch ihre Ecken.
- Die Wand tötet nicht sofort, sie kostet Energie: Der Tunnel ist Belohnung und Risiko nach dem Boss, kein zweiter Boss.
- Der Tunnel ist auch der Hintergrund der Titelseite und der Initialen-Eingabe, dort von der Mittellinie aus gesehen.

## Spielgefühl

- Das Flugzeug fliegt immer; der Yoke setzt Kurs- und Nickrate, nicht die Lage. Volle Auslenkung dreht 1,25 rad/s bei 85 m/s (langsamer enger, schneller weiter), nickt 0,85 rad/s, Nickwinkel bis 57 Grad.
- Der Schub ist das zweite Steuer im Luftkampf: Wer einen engen Kurvenkampf gewinnen will, nimmt Fahrt weg, und riskiert dabei den Strömungsabriss.
- Der Boden verzeiht nichts. Die Decke drückt die Nase weich zurück.
- Jede Ära kämpft anders (siehe `STATUS.md`); wie gut, steht in drei Zahlen der Ären-Tabelle: Vorhalt, Ausweichbereitschaft, Drang zur Sechs-Uhr-Position.
- Gegner zielen auf einen Punkt neben dem Spieler (`offset`, alle 3 bis 6 s neu), damit ein Schwarm nicht auf einer Linie liegt; nach dem Vorbeiflug (unter 70 m) fliegen sie 1,6 bis 2,6 s geradeaus, bevor sie wieder wenden.
- Zielhilfe statt Trefferfreundlichkeit über Riesenradien: innerhalb des Kegels wird exakt auf den Vorhaltepunkt geschossen; außerhalb geradeaus.
- Ein neues Flugzeug ist 3 s unverwundbar und blinkt; Gegner im Umkreis von 350 m sind beim Wiedereinstieg weg, der Boss wird auf 600 m weggeschoben.

## Ton

- Das eigene Flugzeug klingt immer gleich. Was sonst fliegt, bringt seinen Klang mit und ist in Stereo zu orten; das ist neben Radar und Pfeil der dritte Weg, einen Gegner hinter sich zu bemerken.
- Keine Dateien: alle Klänge sind Formeln in `src/sound.c`. Schleifen sind 1 s lang mit ganzzahligen Frequenzen, damit sie ohne Knacken schließen.
- Lautstärken der Effekte stehen in `fx_level` und sind nach Gehör gesetzt.
