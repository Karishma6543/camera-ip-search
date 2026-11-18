# Development setup

## Requirements

- CMake 3.21 or newer
- C++20 compiler (MSVC 2022, recent Clang or GCC)
- Qt 6.5 or newer with Widgets, Network and Sql modules
- Git

## Configure and build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run the application:

```bash
./build/camera-ip-search
```

On multi-configuration generators (for example Visual Studio), the executable may be under `build/Release/`.

## Qt installation

Install Qt 6.5+ with the Widgets, Network and Sql components for your platform. Ensure CMake can locate the Qt installation. If needed, set `CMAKE_PREFIX_PATH` to the Qt installation prefix.

## Current scope

This first scaffold is an application shell only. Camera discovery, activation, network configuration, SQLite persistence and firmware upgrades are not implemented yet.
