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
- more to come +

supports midi devices for toggle switching

## Why not use any other plugin host?
honestly i tried, but i did not like any of the other hosts interfaces, the only one that came close to what i was looking for was [pipedal](https://github.com/rerdavies/pipedal/), but the lack of vst3 support was a dealbreaker for me, any other host required making a OSC interface and that just seems like too much work and running a browser on top of the host seemed like too much wasted resources

## What about linux and raspberrypi?
while i do plan on making a linux version in the future after finishing up the general structure of the app
ARM support will be tricky as i don't currently own any ARM device other than my phone

i gave up on trying to make a MacOS version, though i doubt people will try to make a pedal out of a Mac mini

# Build
### Windows (Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH=C:\path\to\JUCE
cmake --build build --config Release
```

## Linux (not ready yet)

```bash
cmake -B build -DJUCE_PATH=../JUCE   # or absolute path to your JUCE 9
cmake --build build --config Release
```

## How can i contribute?
honestly i'm not a good developer, i used a good amount of AI on this, so anyone who can help rewrite the project into something better would be my savior, otherwise people can contribute by just making themes and presets to share with others, i would also love effects suggestions
