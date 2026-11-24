# Development setup

## Requirements

- CMake 3.21 or newer
- C++20 compiler (MSVC 2022, recent Clang or GCC)
- Qt 6.5 or newer with Widgets, Network, Sql, Xml, Concurrent and Test components
- Python 3.12+ for simulator fixture tests
- Git
- Linux only: an iputils-compatible arping executable for the optional IPv4 duplicate-address probe

The Linux conflict-probe backend uses arping -D on the selected interface. A missing executable, unsupported environment or inconclusive result is reported as **Unknown** and blocks the corresponding IP-assignment preflight. Windows uses native SendARP for positive conflict evidence only; a no-reply result remains Unknown. macOS remains fail-closed as Unknown until a native duplicate-address-detection backend is verified.

## Configure, build and test

~~~bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
~~~

Run deterministic simulator fixture tests:

~~~bash
python -m unittest discover -s tests -p "test_*.py" -v
~~~

Run the application:

~~~bash
./build/camera-ip-search
~~~

On multi-configuration generators (for example Visual Studio), the executable may be under build/Release/.

## Qt installation

Install Qt 6.5+ with the required components for your platform. Ensure CMake can locate the Qt installation. If needed, set CMAKE_PREFIX_PATH to the Qt installation prefix.

## Build layout

The camera-core static library contains shared application implementation. The desktop executable and focused Qt Test binaries link this library, so production source files are compiled once rather than separately for every test target.

The CTest suite covers parsers, adapter registration/capabilities, ONVIF network configuration, scanner and reachability behavior, IP assignment/conflict preflight, batch-operation safety, firmware package metadata/integrity checks, diagnostics, loopback wire integration and a desktop startup smoke test.

## Simulator and hardware validation

The local simulator is a deterministic fixture for the documented Oxita Contract B discovery response and a limited ONVIF exchange. It is not a camera emulator and cannot prove physical-NIC multicast/broadcast behavior, vendor compatibility, camera authentication or state-changing operations.

Keep physical-camera validation separate from CI. Record exact model, hardware revision, firmware, topology and observed results. Never interpret a successful firmware metadata/hash candidate check as signature verification or permission to flash. Unknown adapter capabilities remain disabled.

## Current scope and release boundaries

The application currently includes ONVIF and project-firmware discovery, bounded local subnet candidate scanning, device-information enrichment, a SQLite inventory, reachability checks, operation history, diagnostics export and a guarded ONVIF static-IPv4 workflow.

Vendor-specific provisioning, activation/password workflows, reboot, firmware flashing, preview/snapshot, bulk image/IR settings and NVR channel management remain unavailable until the exact protocol/SDK, authorization, compatibility and recovery behavior are verified.
