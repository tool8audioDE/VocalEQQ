Vocal EQQ @VERSION@ - vocal channel strip by tooL8
====================================================

De-Esser -> Resonance EQ -> Compressor in one window. The chain adapts to
the voice: the EQ only tames resonances while they actually ring out, and
the compressor sets its own threshold and ratio.

Free software under the GNU AGPLv3 (see LICENSE.txt).
Source code: https://github.com/tool8audioDE/VocalEQQ


WHAT'S IN THIS ZIP
------------------
  Vocal EQQ.vst3   the plugin (a folder - keep it as it is)
  Vocal EQQ.exe    standalone app, runs without a DAW
  LICENSE.txt      GNU AGPLv3
  README.txt       this file

Windows 10/11, 64-bit. VST3 only.


INSTALL THE PLUGIN
------------------
1. Copy the whole folder "Vocal EQQ.vst3" to
       C:\Program Files\Common Files\VST3
   (Windows will ask for administrator permission.)
2. FL Studio: Options -> Manage plugins -> Find more plugins.
   Other DAWs: rescan your VST3 plugins.
3. Put Vocal EQQ as an effect on your vocal track.

The standalone app needs no installation - just start "Vocal EQQ.exe".
Windows SmartScreen may warn because the app is not code-signed:
click "More info" -> "Run anyway".


HOW TO USE
----------
The defaults are a good start for most takes.

  1 De-Esser      Reduction, Sensitivity   lowers the band above 4 kHz
                                           while an S or T sound is detected
  2 Resonance EQ  Strength, Low Cut        cuts narrow peaks (150 Hz - 16 kHz,
                                           up to 6 dB), plus a 100 Hz high-pass
                                           and a gentle shelf around 250 Hz
  3 Compressor    Amount                   threshold and ratio set themselves

  Mix        blend with the unprocessed signal
  Auto Gain  matches the output loudness to the input, so Bypass is a fair A/B
  Bypass     compare with the original (no click, same latency)

The spectrum shows the incoming signal; the pink area is what the EQ is
cutting right now.

De-Esser and Compressor learn from what they hear. During the first
seconds of singing they act more gently, then they settle in.

Latency is about 130 ms. Your DAW compensates for it automatically, but it
makes the plugin unsuitable for monitoring while recording - use it for
mixing.


https://tool8.online
