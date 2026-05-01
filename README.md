# Sonism Synth

Sonism is a simple, modern, versatile software synthesizer plugin built using the JUCE framework. It features an intuitive dual-oscillator architecture with real-time waveform and spectrum visualization.

## Features

- **Dual Oscillators**: Two independent oscillators (OSC 1 and OSC 2) with mix controls.
- **Selectable Waveforms**: Choose between Sine, Triangle, Square, Sawtooth, and Pulse waves for each oscillator.
- **Real-Time Visualizations**:
  - Individual waveform displays for each oscillator.
  - A comprehensive output spectrum analyzer.
- **Multiple Plugin Formats**: Built for Standalone, AU, VST3, AUv3, and CLAP.
- **Modern UI**: Designed with JUCE's powerful UI components and debuggable via the included Melatonin Inspector.

## Technical Details

Sonism Synth is built upon the awesome [Pamplejuce](https://github.com/sudara/pamplejuce), utilizing modern C++20 and CMake for a streamlined build process.

- **Framework**: [JUCE 8](https://juce.com/)
- **Build System**: CMake (v3.25+)
- **Testing**: Includes unit tests and benchmarks using [Catch2](https://github.com/catchorg/Catch2).
- **DSP**: Leverages JUCE DSP modules (e.g., FFT, Windowing) and optionally Intel IPP for performance.

## Getting Started

### Prerequisites

- CMake (3.25 or newer)
- A modern C++ compiler (Clang/Apple Clang, GCC, or MSVC)
- Git

### Building the Project

1. **Clone the repository and submodules**:
   ```bash
   git clone https://github.com/bilusgarage/sonismsynth sonismsynth
   cd sonismsynth
   git submodule update --init --recursive
   ```

2. **Configure and build using CMake**:
   ```bash
   # Generate build files
   cmake -B build -DCMAKE_BUILD_TYPE=Release

   # Build the project
   cmake --build build --config Release
   ```

3. **Install / Run**:
   - The standalone application and plugin binaries (VST3, AU, CLAP) will be located in `build/sonismsynth_artefacts/Release/`.
   - On macOS, plugins are automatically copied to your `~/Library/Audio/Plug-Ins/` folder during the build process.

## License

*(Add license information here)*