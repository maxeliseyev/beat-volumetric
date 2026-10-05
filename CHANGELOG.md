# Changelog

## 0.7.0

- Window is replaced by Hold (how long a hit keeps its corrected level) and Release (how long the level glides back to unity, 1-200 ms, default 8 ms). Release has a Linear or Curved (raised-cosine knee) shape. The `window_ms` parameter ID is retired, not reused; old sessions fall back to the defaults.
- The scope shows INPUT and OUTPUT as two separate panels. Output is yellow so it reads as the result of the leveling.

## 0.6.0

- The meter is a scroll-locked oscilloscope. The top lane is the input and the bottom lane is the output, shaded across two bars when the host reports tempo.

## 0.5.0

- Strength and Level are vertical faders. In Auto the Level fader shows the followed target and does not move by hand.
- The meter is a held gain-change history. Each bar is one hit and stays until newer hits replace it.

## 0.4.0

- Gain stays on the hit for the application window (default 120 ms), separate from the 30 ms measurement.
- The editor leads with Smoothing. Target level is shown only in Manual, and Window sets how long a hit keeps its coefficient.
- The scope uses a fixed scale and marks each hit's level before and after. A spread readout compares recent hits.

## 0.1.0

- C++20/CMake Debug/Release scaffold with a standalone DSP library and Catch2 tests.
- Deterministic synthetic hits with known onsets and peak levels.
- Offline mono/stereo WAV runner, configurable block pattern and measured CSV report.
- Streaming detector, level meter and base gain scheduler with a fixed 56 ms lookahead.
- Development AU/VST3/Standalone targets with Strength, Auto/Manual Target and Dry/Wet.
- macOS packaging script for Developer ID signing, notarization, stapling and DMG creation.
- The real-recording host matrix, sidechain, bypass/difference and final plugin IDs remain open.
