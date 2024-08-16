Tool to convert github wikis into chm files.

Requires chmcmd or html help workshop.

# Building

## Dependencies windows

Single winget command to install all dependencies required to build.

`winget install mesonbuild.meson Ninja-build.Ninja Kitware.CMake Microsoft.VisualStudio.2022.BuildTools`

## Dependencies linux

List of dependencies since every distro has its own packages:
- meson
- ninja
- cmake
- any compiler
- openssl (optional, for https support)

## Compiling

`meson setup bin`

`meson compile -C bin`