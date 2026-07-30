# quadnoncortex

## Build

```bash
cmake -B build -DJUCE_PATH=../JUCE   # or absolute path to your JUCE 9
cmake --build build --config Release
```

### Windows (Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH=C:\path\to\JUCE
cmake --build build --config Release
```
