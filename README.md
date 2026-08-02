# quadnoncortex

<img width="1024" height="600" alt="Capture" src="https://github.com/user-attachments/assets/eb8f682a-d03f-4fd5-abb4-7da81591806a" />

## What is this?
it's a plugin host masked as a guitar pedal interface, meant to be used on low powered devices with low resolution screens (though i only tested on 600p)
includes native effects:
- Nam loader + tone3000 integration (WIP)
- Compressor
- Denoiser + Gate
- Echo
- Reverb
- Limiter
- ParametricEQ
- Pitchshifter by [VoLum](https://github.com/guitarlum/VoLum)
- more to come +

supports midi devices for toggle switching

## Why not use any other plugin host?
honestly i tried, but i did not like any of the other hosts interfaces, the only one that came close to what i was looking for was [pipedal](https://github.com/rerdavies/pipedal/), but the lack of vst3 support was a dealbreaker for me, any other host required making a OSC interface and that just seems like too much work and running a browser on top of the host seemed like too much wasted resources

## Requirements

### All platforms
- [JUCE 9](https://github.com/juce-framework/JUCE) (pass `-DJUCE_PATH=` or place next to this folder)
- CMake 3.22+
- Git (for FetchContent of NeuralAmpModelerCore)

### Windows
- Windows 10/11 x64
- MSVC Build Tools 2022 (or Visual Studio 2022)
- Optional: [ASIO SDK](https://www.steinberg.net/asiosdk/) via `-DASIO_SDK_PATH=`

### Linux (Debian / Ubuntu / Raspberry Pi OS)
```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxcomposite-dev libxi-dev \
  libgl1-mesa-dev \
  libasound2-dev libjack-jackd2-dev \
  libcurl4-openssl-dev

# PipeWire (recommended): ALSA + JACK both go through PipeWire
sudo apt install -y pipewire-alsa pipewire-jack pipewire-audio
```

| Package | Why |
|---------|-----|
| `libfreetype-dev` | `ft2build.h` — juce_graphics / juceaide |
| `libfontconfig1-dev` | Font discovery |
| `libxi-dev` | `X11/extensions/XInput2.h` — juce_gui_basics |
| `libx11-dev` + related | Windowing |
| `libasound2-dev` | ALSA (and PipeWire-ALSA) |
| `libjack-jackd2-dev` | JACK (and PipeWire-JACK) |
| `libcurl4-openssl-dev` | Updates / HTTP (Tone3000) |
| `git` | CMake FetchContent for NeuralAmpModelerCore |

## Build

### Windows (Visual Studio)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH="C:\path\to\JUCE"
cmake --build build --config Release
```

Binary: `build\quadnoncortex_artefacts\Release\quadnoncortex.exe`

### Linux (x86_64 & ARM / Raspberry Pi)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=/path/to/JUCE
cmake --build build -j$(nproc)
```

Binary: `build/quadnoncortex_artefacts/Release/quadnoncortex`

## How can i contribute?
honestly i'm not a good developer, i used a good amount of AI on this, so anyone who can help rewrite the project into something better would be my savior, otherwise people can contribute by just making themes and presets to share with others, i would also love effects suggestions
