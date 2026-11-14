# GATE-VERB – Bedienungsanleitung

**by studiobro.net for Sound&Recording Magazin**

GATE-VERB verbindet einen Stereo-Hall mit einer zeichnbaren Lautstärkekurve. Die Kurve bearbeitet die Hallfahne. Damit entstehen pumpende Räume, abgehackte Hallrhythmen, langsam anschwellende Flächen oder ein klassischer Hall ohne Modulation.

## 1. Installation auf dem Mac

Lade im Repository unter **Actions → Build macOS VST3 → erfolgreicher Lauf → Artifacts** das macOS-Paket herunter. Zum Download von Actions-Artifacts musst du bei GitHub angemeldet sein. Entpacke es und gegebenenfalls die darin enthaltene Produkt-ZIP.

Kopiere das vollständige Bundle **GATE-VERB.vst3** in den Ordner:

```text
~/Library/Audio/Plug-Ins/VST3/
```

Der Ordner liegt in deiner Benutzer-Library. Du kannst ihn im Finder mit **Gehe zu → Gehe zum Ordner …** öffnen und bei Bedarf anlegen. Verschiebe das ganze `.vst3`-Bundle, nicht die darin enthaltene ausführbare Datei. Starte anschließend die DAW neu oder lasse ihre Plug-ins erneut einlesen.

Das Universal-Bundle enthält Intel (`x86_64`) und Apple Silicon (`arm64`). Das Build-Ziel ist macOS 10.15 für Intel; Apple Silicon benötigt mindestens macOS 11. Es enthält keine Developer-ID-Signatur und ist nicht notarisiert. Die im Build verwendete Ad-hoc-Signatur dient der Entwicklung. Für eine Veröffentlichung übernimmt der Herausgeber Signierung und Notarisierung; Hinweise stehen in `docs/BUILD.md`.

## 2. Der erste Sound

1. Setze GATE-VERB als Insert auf eine Drum-, Synth- oder Audiospur.
2. Wähle **01 - Gated Bloom**.
3. Starte den DAW-Transport. Im Sync-Modus bewegt sich die Kurve nach der musikalischen Position der DAW.
4. Stelle WET/DRY so ein, dass der Hall gut hörbar ist.
5. Zeichne im großen Gate-Feld eine Kurve: oben ist der Hall offen, unten ist er geschlossen.
6. Vergleiche **Pulse**, **Duck**, **Rise** und **Sine** als Ausgangsformen.

Für einen Send-/Aux-Kanal stelle **WET/DRY auf 100%**. Die Presets `11 - Wide Open Send` und `12 - Pumping Send` sind bereits so eingestellt.

## 3. Was das Gate verändert

Das Eingangssignal wird nach IN in zwei Wege aufgeteilt. Ein Weg bleibt trocken. Der zweite läuft durch Hall und Gate. WET/DRY mischt beide Wege, OUT regelt anschließend den Gesamtpegel.

Das Gate ist eine periodische Lautstärkehüllkurve, kein pegelabhängiges Noise Gate. Es besitzt daher keinen Threshold und benötigt keinen Sidechain-Eingang. Eine geschlossene Kurve blendet den Hall aus, während der Hall intern weiterklingt. Öffnet die Kurve wieder, wird die verbleibende Hallfahne hörbar.

## 4. Eine Kurve zeichnen

**Free Draw:** Klicke und ziehe im großen Feld. Jeder Strich verändert den durchfahrenen Teil der Kurve. Schnelle Mausbewegungen werden zwischen den Stützstellen aufgefüllt. Die anderen Abschnitte bleiben erhalten. Die Kurve besitzt 256 Amplitudenwerte pro Zyklus.

**Steps:** Wähle die gewünschte Rastergröße und zeichne die Höhe ganzer Zellen. Verfügbar sind 4, 8, 16, 32 und 64 Zellen pro vollständigem Gate-Zyklus. Ein 16er-Raster bei einem Ein-Takt-Zyklus ergibt in 4/4 Sechzehntelschritte. Ein 16er-Raster bei `1/4` unterteilt dagegen diese einzelne Viertelnote in 16 Zellen.

Das Wechseln des Zeichenmodus verändert die vorhandene Kurve nicht automatisch. In Steps werden nur die Zellen verändert, über die du tatsächlich zeichnest. Die vertikale Position bleibt frei wählbar; das Raster bezieht sich auf die Zeit.

Mit gedrückter **Shift-Taste** rastet die Amplitude beim Zeichnen in 10%-Schritten ein. Das Ende des Zyklus wird mit dem Anfang verbunden; der kurze Übergang am rechten Rand zeigt diese Schleifenverbindung.

Die Auswahl **QUICK SHAPES** liefert Ausgangskurven:

| Form | Wirkung |
| --- | --- |
| Open | Durchgehend offene Hallfahne |
| Pulse | Eine offene und eine geschlossene Hälfte |
| Duck | Der Hall setzt nach dem Zyklusbeginn weich wieder ein |
| Rise | Anschwellender Hall |
| Fall | Abfallende Halllautstärke |
| Sine | Kontinuierliches, weiches Auf- und Abschwellen |
| Steps | Ein rhythmisches Muster mit verschiedenen Pegeln |

Undo/Redo im Editor betrifft die gezeichnete Kurve. Die aktuelle Kurve wird vollständig mit dem DAW-Projekt und in User-Presets gespeichert. Die Zeichenhistorie selbst ist keine dauerhafte Preset-Eigenschaft.

## 5. Gate-Timing und Modulation

| Parameter | Bereich / Verhalten |
| --- | --- |
| Gate | Aktiviert bzw. deaktiviert die Amplitudenmodulation; der Hall bleibt aktiv |
| Sync | Verknüpft die Gate-Phase mit der musikalischen DAW-Position |
| Division | `1/64`, `1/32`, `1/16 T`, `1/16`, `1/8 T`, `1/8`, `1/4`, `1/4 D`, `1/2`, `1 bar`, `2 bars`, `4 bars` |
| Free Rate | 0,05–20 Hz, wenn Sync ausgeschaltet ist |
| Depth | 0–100%; bei 0% bleibt der Hall unabhängig von der Kurve offen |
| Smooth | 0,2–50 ms; glättet die Pegelübergänge |
| Phase | Verschiebt die Kurve um 0–100% eines Zyklus |

**T** bedeutet Triole, **D** bedeutet punktiert. Die Taktbeschriftungen sind feste Längen auf Basis von 4/4: `1 bar` entspricht vier Viertelnoten. Ein Wechsel der Taktart verändert diese Länge nicht automatisch.

Im Sync-Modus folgt die Position dem vom Host gelieferten PPQ-Wert. Dadurch richtet sich das Gate beim Springen oder Loopen im Arrangement wieder an der musikalischen Position aus. Wenn die DAW angehalten wird, hält auch die Sync-Position an; vorhandener Hall kann durch die aktuelle Gate-Öffnung weiterklingen. Im Free-Modus läuft der interne Oszillator weiter, solange die DAW Audio verarbeitet – auch bei angehaltenem Transport.

Liefert der Host keine musikalische Position, verwendet das Plug-in einen internen Taktgeber. Ohne Tempoangabe beträgt das Ersatztempo 120 BPM. Ein Host, der keine Audio-Callbacks mehr liefert, hält naturgemäß auch die Audioverarbeitung an.

Bei sehr kurzen Zyklen kann eine große Smooth-Zeit das Muster deutlich weicher und flacher machen. Für harte Rhythmen sind etwa 1–5 ms ein sinnvoller Startpunkt; für weichere Bewegungen kannst du höhere Werte ausprobieren.

## 6. Hall und Pegel

| Parameter | Bereich | Funktion |
| --- | --- | --- |
| IN | −24 bis +24 dB | Eingangspegel für trockenen und nassen Zweig |
| OUT | −24 bis +12 dB | Gesamtpegel nach WET/DRY |
| WET/DRY | 0–100% | Lineare Mischung aus trockenem Signal und bearbeiteter Hallfahne |
| PRE-DELAY | 0–250 ms | Verzögert den Eintritt des Signals in den Hall |
| DECAY | 0,2–20 s | Nominale Abklingzeit der Rückkopplung; Damping und Filter beeinflussen die hörbare Länge |
| SIZE | 10–100% | Ändert die Laufzeiten im Hallraum |
| DAMPING | 0–100% | Dämpft hohe Frequenzen in der Hallrückkopplung |
| DIFFUSION | 0–100% | Verteilt die Eingangsenergie über zusätzliche Allpass-Stufen |
| LOW CUT | 20–2000 Hz | Entfernt tiefe Frequenzen aus dem Halleingang |
| HIGH CUT | 2000–20000 Hz | Begrenzt Höhen am Hallausgang |
| WIDTH | 0–200% | 0% mono, 100% normale Stereobreite, darüber verbreitert |

Die Hallfilter bearbeiten den nassen Zweig. Der trockene Zweig wird durch diese Filter nicht verändert. Bei niedrigen Sampleraten begrenzt die Engine Filterfrequenzen intern auf einen geeigneten Wert unter Nyquist.

Ein Doppelklick auf einen Drehregler setzt ihn auf seinen Standardwert zurück. Mit gedrückter Shift-Taste lässt sich feiner einstellen. Alle normalen Klang- und Timing-Regler können vom Host automatisiert werden. Die gezeichnete Kurve wird als Plug-in-Zustand gespeichert.

Das Plug-in enthält keinen abschließenden Limiter. Behalte die Pegelanzeige im Blick und reduziere IN oder OUT, wenn die Mischung übersteuert. Hohe IN-Werte und lange Hallzeiten können die Ausgangslautstärke erhöhen.

## 7. Presets

Im Preset-Menü stehen zwölf Werkspresets zur Verfügung. Einige sind bewusst zurückhaltend, andere demonstrieren starkes Gate-Pumping oder lange Texturen. Ein Stern am Presetnamen zeigt eine Änderung an.

**Save** schreibt die aktuellen Parameter und die gesamte Gate-Kurve in eine `.gateverb`-Datei. **Load** lädt eine solche Datei wieder. Standardmäßig wird der Preset-Ordner unter der Benutzer-Anwendungsablage verwendet:

```text
~/Library/Application Support/studiobro/GATE-VERB/Presets/
```

Du kannst eigene Dateien auch an einem anderen Ort speichern und von dort laden. Die Dateien sind lesbares XML mit einer Versionsnummer. Defekte oder unvollständige Presets werden mit einer Fehlermeldung abgelehnt; sie ersetzen den aktuellen Klang nicht.

Beim Speichern eines DAW-Projekts werden alle Parameter, die Gate-Kurve und die Editor-Einstellungen im Plug-in-Zustand abgelegt. Eine separat gespeicherte User-Preset-Datei wird dafür nicht benötigt.

## 8. Anzeige und Audioexport

Die Positionslinie zeigt die aktuelle Gate-Phase aus der Audioverarbeitung. Pegel und die darauf reagierende Visualisierung beruhen auf dem verarbeiteten Signal. Die grafische Aktualisierung erfolgt unabhängig von der Samplerate.

Die Oberfläche muss weder geöffnet noch sichtbar sein, damit das Gate arbeitet. Echtzeit-Wiedergabe und Offline-Export verwenden dieselbe Audio-Engine. Nach einem Reset oder einem erneuten Prepare durch den Host wird der Hallpuffer geleert und der Taktgeber initialisiert. Bei Sync bindet sich die folgende Verarbeitung wieder an die Host-Position.

## 9. Quellcode und Lizenz

Das Projekt ist für Leserinnen und Leser des Sound&Recording-Artikels offen einsehbar. Der ursprüngliche GATE-VERB-Code bleibt unter der MIT-Lizenz des Repositorys. JUCE hat eigene Lizenzbedingungen. Das bereitgestellte Open-Source-Paket nutzt den AGPLv3-Lizenzweg für die Verbindung mit JUCE; die vollständigen passenden Quellen und Lizenztexte werden neben dem Plug-in angeboten.

Für eine spätere proprietäre Veröffentlichung kann eine kommerzielle JUCE-Lizenz erforderlich sein. Details und Links stehen in `THIRD_PARTY_NOTICES.md`.
