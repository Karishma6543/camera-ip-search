# Development setup

## Requirements

- CMake 3.21 or newer
- C++20 compiler (MSVC 2022, recent Clang or GCC)
- Qt 6.5 or newer with Widgets, Network, Sql, Xml, Test and Concurrent components
- Git

On Linux, the current conflict-probe backend additionally expects an iputils-compatible `arping` executable. A missing utility or inconclusive result is handled as Unknown and blocks the corresponding IP-assignment preflight.

## Configure, build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Run the application:

```bash
./build/camera-ip-search
```

On multi-configuration generators (for example Visual Studio), the executable may be under `build/Release/`.

## Qt installation

Install Qt 6.5+ with the required components for your platform. Ensure CMake can locate the Qt installation. If needed, set `CMAKE_PREFIX_PATH` to the Qt installation prefix.

## Portable staging archives

After configuring and building, generate the CPack ZIP and TGZ staging archives with:

```bash
cpack --config build/CPackConfig.cmake
```

These archives currently package the installed application target only. They are development/release-review artifacts, not self-contained end-user installers. Qt runtime deployment, platform-specific installer generation, signing, macOS notarization, Linux dependency bundling and clean-machine installation tests remain release gates.

## Validation expectations

- Run CTest on Windows, macOS and Linux before release.
- Keep physical-camera validation separate from normal CI; record exact model, hardware revision and firmware.
- Never interpret a successful metadata/hash firmware candidate check as signature verification or permission to flash.
- Unknown adapter capabilities remain disabled. A new vendor adapter must include protocol fixtures and documented compatibility evidence.
