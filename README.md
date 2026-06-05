# Sonism *(Alpha version)*

![Sonism Supersaw Synthesizer V0.1](assets/sonism-v0.1.png)

Sonism is a modern **Supersaw synthesizer plugin** built with the JUCE framework. It features a 7-oscillator engine with independent detuning and stereo panning per oscillator, two independent LFOs, a state-variable filter, an amplitude envelope, and real-time waveform and spectrum visualization.

## Features

### Oscillator Engine
- **7 Independent Oscillators**
- **5 Waveform Types** per oscillator — Sine, Triangle, Square, Sawtooth, Pulse.
- **Per-Oscillator Mix** — linear mix slider to blend each oscillator into the final output.
- **Detune** (OSC 2–7) — ±100 cents of independent pitch offset per oscillator, enabling fat supersaw stacks.
- **Pan** (all OSCs) — constant-power stereo panning per oscillator for wide stereo spread.
- **Global Unison** — Global Unison Detune and Spread controls for rapidly setting up a thick supersaw. 

### Modulation & Envelopes
- **LFO Section** — Two independent LFOs (LFO 2-4 and LFO 5-7) with controls for:
  - Wave Type (Sine, Saw, Triangle)
  - Amount
  - Rate
  - Phase
- **Amplitude Envelope (ADSR)** — Attack, Decay, Sustain, Release.

### Filter & Effects
- **State-Variable Low-Pass Filter** with:
  - **Cutoff** — 20 Hz to 20 kHz.
  - **Resonance** — 0.1 to 5.0 Q factor.

### UI & Visualization
- **Per-Oscillator Waveform Display** — real-time oscilloscope showing the summed stereo output of the active oscillator.
- **Output Spectrum Analyzer** — FFT-based frequency display of the final stereo mix.

### Polyphony & Format
- **6-voice polyphony** (configurable in source).
- **Multiple Plugin Formats**: Standalone, AU, VST3, AUv3, CLAP.

### Coming Soon: Effects
- **Reverb**
- **Compressor**
- **Delay**
- **(Considering other)**

---

## Technical Details

Sonism is built on top of the awesome [Pamplejuce](https://github.com/sudara/pamplejuce), leveraging modern C++20 and CMake.

| | |
|---|---|
| **Framework** | [JUCE 8](https://juce.com/) |
| **Build System** | CMake 3.25+ |
| **Testing** | [Catch2](https://github.com/catchorg/Catch2) — unit tests & benchmarks |
| **DSP** | JUCE DSP modules (FFT, Windowing, StateVariableTPTFilter) |

---

## Getting Started

### Prerequisites

- CMake 3.25 or newer
- Apple Clang, GCC, or MSVC (C++20 support required)
- Git

### Building

```bash
# Clone with submodules
git clone https://github.com/bilusgarage/sonismsynth sonism
cd sonism
git submodule update --init --recursive

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

Binaries are written to `build/sonismsynth_artefacts/Release/`. On macOS, AU and VST3 plugins are automatically copied to `~/Library/Audio/Plug-Ins/`.

### Running (Debug)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/sonismsynth_artefacts/Debug/Standalone/Sonism.app/Contents/MacOS/Sonism
```