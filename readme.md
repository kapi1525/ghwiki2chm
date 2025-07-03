# ghwiki2chm

Tool that makes it easy to convert github wikis into chm files.

Requires one of the supported chm compilers:
- chmcmd (part of free pascal)
- hhc (html help workshop. dead)

# Building

## Dependencies:

- meson
- ninja
- cmake (for some of the dependencies)
- any C++20 compiler
- libcurl (if not present, will be built from source. but it may require some other dependencies as well.)

On linux i recoment to build curl from source as a static library, `.github/scripts/static_deps_from_source.sh` script makes that easy.
That script requires additionally:
- make
- curl (to download source archives)
- tar
- autoconf
- automake
- libtool

## Compiling

- `meson setup bin` (Or if you used static_deps_from_source.sh script, use the command it displayed.)

- `meson compile -C bin`