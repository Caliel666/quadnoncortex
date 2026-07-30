# Simple VST3 Host

A minimal, **touch-first** fullscreen VST3 host built with **JUCE 9**.  
Designed for small ~600p screens (no keyboard / mouse required).

Inspired by the workflow of [PiPedal](https://github.com/rerdavies/pipedal) but focused exclusively on VST3 plugins and a native desktop/embedded UI.

---

## Features

| Feature | Description |
|---------|-------------|
| **VST3 only** | Uses JUCE’s built-in VST3 hosting (`JUCE_PLUGINHOST_VST3=1`) |
| **ASIO** | Preferred audio backend on Windows (WASAPI / CoreAudio / ALSA as fallback) |
| **Fullscreen** | No title bar, fills the primary display |
| **Plugin blocks** | Horizontal chain of large touchable blocks |
| **Reorder** | Drag a block onto another to reorder the chain |
| **Remove** | Red **X** on each block – tap to delete |
| **Add / Replace** | Big “+ ADD PLUGIN” button; browser also supports replace |
| **Parameter panel** | Tap a block → bottom panel lists **all** parameters with large sliders |
| **MIDI Learn** | Tap **LEARN** on a parameter, move a MIDI CC / note → bound |
| **Clear binding** | **Double-tap** a parameter row to clear its MIDI assignment |
| **Touch-friendly** | Large hit targets, 600p-oriented layout |

---

## Requirements

- **JUCE 9** (you already have it)
- CMake ≥ 3.22
- C++17 compiler (MSVC 2022 / Xcode 15 / GCC 11+ / Clang 14+)
- On Windows: ASIO SDK (optional but recommended) – place `asiosdk` somewhere and add its `common` folder to include paths if needed

---

## Build

```bash
# From the folder that contains both JUCE/ and SimpleVST3Host/
cd SimpleVST3Host

cmake -B build -DJUCE_PATH=../JUCE   # or absolute path to your JUCE 9
cmake --build build --config Release
```

### Windows (Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH=C:\path\to\JUCE
cmake --build build --config Release
```

The executable will be at:

```
build/SimpleVST3Host_artefacts/Release/Simple VST3 Host.exe
```

### Linux

```bash
cmake -B build -DJUCE_PATH=/path/to/JUCE
cmake --build build -j$(nproc)
```

You may need `libasound2-dev`, `libfreetype6-dev`, `libx11-dev`, etc.

### macOS

```bash
cmake -B build -G Xcode -DJUCE_PATH=/path/to/JUCE
cmake --build build --config Release
```

---

## First run

1. Launch the app (it goes fullscreen).
2. Tap **AUDIO** → select your ASIO (or other) device and MIDI input.
3. Tap **SCAN PLUGINS** inside the Add dialog (or it will use any previously scanned list).
4. Tap **+ ADD PLUGIN** → choose a VST3 → it appears as a block.
5. Tap the block → parameter panel appears at the bottom.
6. Tap **LEARN** on a parameter → move a MIDI controller → bound (button turns green “MIDI”).
7. Double-tap the parameter row to clear the binding.
8. Drag blocks to reorder; tap the red **X** to remove.

---

## Project layout

```
SimpleVST3Host/
├── CMakeLists.txt
├── README.md
└── Source/
    ├── Main.cpp                 # Application entry + fullscreen window
    ├── MainComponent.h/cpp      # Root UI, layout, blocks + panel orchestration
    ├── AudioEngine.h/cpp        # Device manager, audio callback, MIDI routing
    ├── PluginChain.h/cpp        # Serial VST3 chain, scan, save/load state
    ├── PluginBlockComponent.*   # Single touchable plugin block + drag-reorder
    ├── ParameterPanel.*         # Bottom parameter list + Learn buttons
    ├── PluginBrowser.*          # Full-screen plugin picker
    └── MidiLearnManager.*       # MIDI → parameter bindings
```

---

## Notes / future ideas

- State persistence (chain + MIDI maps) is partially implemented in `PluginChain` / `MidiLearnManager` but not yet auto-saved on quit – easy to wire to `ApplicationProperties`.
- Plugin editors (the original GUIs) are intentionally **not** opened; the design is parameter-strip only for small screens. You can add a “GUI” button later if desired.
- Only stereo in/out is configured. Multi-channel / side-chain would need bus layout work.
- On embedded Linux boards you may want to force a specific display / disable window manager decorations further.

---

Enjoy building on it!
