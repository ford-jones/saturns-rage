# Saturns Rage
Dodge asteroids in the rings of saturn

## Getting started:
### Install the dependencies:
Download Lazarus Engine from https://lazarusengine.xyz/downloads or clone the git repository:
```
git clone https://github.com/ford-jones/lazarus.git
```
Lazarus doesn't provide prebuilt binaries so the library must be compiled from src. \
Follow the installation instructions found in the [Getting Started](https://github.com/ford-jones/lazarus/blob/main/docs/getting-started.md) section of the readme.

### Compiling the application:
Once the engine has been installed you should be able to compile the saturns rage application like so:

**Compiling with `G++`:**
```
g++ main.cpp -o saturn -lGL -lGLEW -lglfw -lfmod -llazarus -lfreetype
```

**Compiling with `clang`:**
```
clang -std=c++17 main.cpp -lstdc++ -llazarus -lfreetype -lGLEW -l glfw -lGL -lfmod -lm -o saturn
```
*Note: Some machines may not have to link `-lstdc++` or `-lm`.*

**Compiling with `MSVC`:**
```
cl /EHsc /std:c++17 main.cpp /link fmod_vc.lib freetype.lib glfw3.lib glew32.lib opengl32.lib liblazarus.lib msvcrt.lib user32.lib gdi32.lib shell32.lib /out:saturn.exe /NODEFAULTLIB:libcmt
```
*Note: Again, some machines may not need to link against `user32.lib`, `msvcrt.lib`, `shell32.lib` or `gdi32.lib`. This depends on how your libraries are installed and how your compilers `PATH` variable is configured.*

## Gameplay:
- Use the mouse to move.
- Use the `X` key to exit the game.
- Keep the ship's health above 0 for as long as you can!
