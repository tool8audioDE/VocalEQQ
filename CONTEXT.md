# Projekt-Kontext: Vocal EQQ

_Zuletzt aktualisiert: 2026-09-26_

Für den Einstieg in eine neue Sitzung. Wiederholt **nicht**, was im
`README.md` steht (Bauen, Benutzen, Messwerte), sondern was sonst verloren
ginge: Ziel, Entscheidungen, verworfene Wege, offener Stand.

**Zuerst lesen:** `README.md`, danach diese Datei.

---

## Ziel

Der Python-Channel-Strip `ai-vocal-eq` (De-Ess → EQ → Kompressor) soll wie
Keyy als **kostenloser Download auf tool8.online** erscheinen. Name nach dem
Muster von Keyy (doppeltes Y): **Vocal EQQ**, doppeltes Q.

## Entscheidungen (2026-09-26, mit dem Nutzer)

| Entscheidung | Grund |
|---|---|
| **VST3 + Standalone in JUCE/C++**, nicht die Python-App verpacken | Nutzer will ein Plugin wie Keyy |
| **AGPLv3**, Quellcode öffentlich wie Keyy | JUCE-Lizenz; einheitlich mit Keyy |
| Hersteller tooL8 (`TooL`), Plugin-Code `Veqq`, Produkt „Vocal EQQ“ | wie Keyy |
| Eigenes Repo `Selfmade/VocalEQQ`, **nicht** im Python-Repo | Python-Repo ist privat und bleibt es; das neue Repo wird öffentlich |
| Englische Oberfläche, Doku und Kommentare deutsch | wie Keyy |
| Design: tooL8-Palette aus `theme.py` (dunkel `#121212`, Akzent `#3B8ED0`, Pink für Absenkungen), Karten, Pillen-Schalter, Regler-Zeilen wie `widgets.py` | Keyy selbst hat noch keine gestaltete Oberfläche (nur JUCE-Standard); die Palette war dort aber schon als Ziel notiert. `Tool8LookAndFeel` ist so geschrieben, dass Keyy es übernehmen kann |
| Stufenweise dieselben Regler und Vorgaben wie die Python-GUI | bekannte Bedienung, gehörte Vorgaben |

## Technik und warum

| Entscheidung | Grund |
|---|---|
| DSP-Kern ohne JUCE (`source/dsp`), Catch2, CLI | wie Keyy/Voxx: derselbe Code in Tests, CLI und Plugin |
| **Nur der dynamische EQ-Modus** | Der statische braucht das Langzeitspektrum der ganzen Datei, das ein Plugin nie hat. In der Python-GUI war Dynamic Vorgabe und wurde praktisch nur benutzt |
| EQ als STFT-Overlap-Add, 4096/1024 (skaliert mit der Samplerate) | exakt die Rechnung des Vorbilds, nullphasig; kostet 93 ms Latenz |
| Stereo: beide Kanäle in *einer* komplexen FFT | die Verstärkung ist reell und symmetrisch, die Kanäle trennen sich bei der Rücktransformation wieder |
| De-Esser: LR8-Weiche statt filtfilt | gleicher Betragsgang je Band, ohne Vorausschau; derselbe Allpass im trockenen Zweig, sonst kämmt der Mix |
| Perzentile (De-Esser, Kompressor) als laufende Quantil-Schätzer | Python rechnet sie über die ganze Datei. Schrittweite am Kompressor gemessen (README) |
| Makeup des Kompressors entfällt, stattdessen Auto-Gain am Ende der Kette | entspricht `run_chain` (Lautheitsabgleich auf den Eingang); gegatet, Zeitkonstante 3 s, ±12 dB |
| Feste Latenz, Stufen rechnen auch ausgeschaltet weiter | DAW bekommt eine konstante Zahl; Schalten knackt nicht |
| Bypass als eigener Parameter, als Host-Bypass gemeldet | blendet über und hält die Latenz |

## Verworfen — nicht erneut vorschlagen

1. **Die Python-App mit PyInstaller verpacken** statt zu portieren — vom
   Nutzer zugunsten des Plugins entschieden.
2. **De-Essing im STFT des EQ** (spart die Weiche): Sprung 1024 = 23 ms,
   Fenster 93 ms — eine Absenkung würde über benachbarte Vokale schmieren.
3. **Schnelle Quantil-Schätzer im Kompressor** (0,05 dB je Schritt):
   2,1–2,6 dB mittlere Abweichung vom Vorbild statt 0,8–1,1.

## Offen / nächste Schritte

- [ ] **In FL Studio testen**: laden, Latenzausgleich, Automation, Speichern
      im Projekt, Bypass, Mono-Spur. Das Standalone wurde gebaut, aber noch
      nicht per Bildschirm bedient (Zugriff im Sitzungsverlauf abgelehnt);
      die Oberfläche ist nur per `vocaleqq-snapshot` offscreen geprüft.
- [ ] **Hörtest** gegen das Python-Ergebnis an eigenen Takes.
- [x] **Erledigt 2026-09-26:** öffentliches Repo
      https://github.com/tool8audioDE/VocalEQQ angelegt und gepusht.
- [ ] **Keyy ist privat** (Stand 2026-09-26, `tool8audioDE` hatte 0
      öffentliche Repos), obwohl sein README den öffentlichen Quellcode als
      Grund für die AGPLv3 nennt. Vor der Verteilung öffentlich stellen.
- [ ] Release-Paket für die Website (ZIP mit VST3, Standalone, LICENSE,
      Kurzanleitung), Seite `tool8.online/vocaleqq`.
- [ ] Beobachten: Der De-Esser erkennt (wie das Vorbild) nicht jeden
      Rahmen eines Zischlauts — an einem Kunstsignal ~4 statt 6 dB.
      Falls es beim Hören auffällt, an der Erkennung ansetzen, nicht an
      der Portierung.
- [ ] Später: Keyy auf `Tool8LookAndFeel` umstellen, damit beide gleich
      aussehen; Presets; Schwellen-Zustand im Projekt speichern (spart das
      Neu-Lernen nach dem Laden).
