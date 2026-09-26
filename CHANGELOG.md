# Changelog

## 1.0.0 — unveröffentlicht

Erste Fassung als Plugin, portiert aus dem Python-Werkzeug `ai-vocal-eq`.

**Neu**

* `ResonanceEq` — dynamischer Resonanz-EQ (Detail 1/6 Oktave gegen Basis
  1 Oktave, Überschuss über 5 dB abgesenkt, höchstens 6 dB), dazu Hochpass
  100 Hz und weiches Low-Shelf um 250 Hz. Nullphasig im Overlap-Add, Stereo
  gemeinsam erkannt. Auf echten Aufnahmen auf 0,06 dB gleich dem Vorbild.
* `DeEsser` — Zischlaut-Erkennung aus Hochton-Anteil, Schwerpunkt,
  Nulldurchgängen und Flux mit laufend geschätzten Schwellen; Absenkung über
  4 kHz mit einer Linkwitz-Riley-Weiche 8. Ordnung.
* `AutoCompressor` — Schwelle und Ratio aus der Dynamik der Stimme, ein
  Regler; Perzentile laufend geschätzt.
* `VocalChain` — Mix mit phasengleichem trockenem Zweig, Auto-Gain auf die
  Lautheit des Eingangs, knackfreier Bypass, feste Latenz.
* Plugin (VST3 und Standalone) mit Oberfläche im tooL8-Design:
  Live-Spektrum mit EQ-Korrektur, drei Stufen-Karten mit Messwerten.
* `vocaleqq-cli` (offline, Latenz ausgeglichen) und `vocaleqq-snapshot`
  (Oberfläche als PNG).
* Catch2-Tests, GitHub-Actions-Workflow.
