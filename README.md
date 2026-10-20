# Subside

A **sidechain ducker** in the spirit of Devious Machines *Duck* and Plugin
Alliance's *Laser*. Duck the volume of any signal with a punchy, musical
envelope triggered by an external **audio sidechain**, **MIDI notes**, or both —
and shape the exact duck curve by hand.

Built with JUCE and delivered as **VST3** + **CLAP** (plus **AU** on macOS).

## What's inside

- **Two trigger lanes**, each with its own hand-drawn duck curve:
  - **Audio** — the sidechain input's envelope crossing a threshold plays the
    curve. A live oscilloscope shows the sidechain waveform behind it.
  - **MIDI** — every note-on plays the curve.
- **Drawable curves** — click + drag in a lane to draw the duck envelope
  (attack / hold / release, any shape). Double-click resets. The playhead shows
  where the envelope is, live.
- **Main oscilloscope** — the input waveform with the combined duck amount
  filled underneath, so you can *see* how much is being ducked.
- **3-band ducking** — split the signal into Low / Mid / High and duck each band
  independently.
- **Lookahead** for click-free attacks, and a holographic, audio-reactive UI
  (matching Sympathy).

## Parameters

| Group   | Control      | Meaning |
|---------|--------------|---------|
| Trigger | Threshold    | Sidechain level that triggers the audio lane |
| Trigger | Audio Time   | Duration of the audio lane's drawn curve |
| Trigger | MIDI Time    | Duration of the MIDI lane's drawn curve |
| Bands   | Low / Mid / High | Duck amount per frequency band |
| Global  | Depth        | Overall duck amount |
| Global  | Lookahead    | Click-free attack latency |
| Global  | Mix          | Dry / wet |
| Global  | Output       | Master gain |

## Requirements

- **Windows 10/11, x64**
- **macOS 10.15+** (Apple Silicon or Intel; universal 2 binary)
- A VST3 / AU / CLAP host. The sidechain routing is exposed as the plugin's
  second input in Bitwig, Reaper, Cubase, Studio One, and Ableton Live.

## Building

Everything is vendored in-tree (JUCE 8.0.8 + clap-juce-extensions + CLAP SDK),
so no network is needed at build time.

- **Windows**: `build.bat` (needs CMake + Visual Studio 2022), then
  `installer\build_installer.bat` for the Inno Setup `.exe`.
- **macOS**: `./build_mac.sh` then `./make_pkg.sh` (needs Xcode CLT + CMake).

## Continuous integration

`.github/workflows/build-macos.yml` builds the macOS universal2 VST3 + AU + CLAP
bundles on every push to `main`, every tag, and on demand, uploading the bundles
and a ready-to-use `.pkg`.

## License

MIT — see the project's license terms. JUCE and clap-juce-extensions retain
their own licenses.
