# Camera IP Search

A cross-platform desktop utility for discovering IP cameras from multiple manufacturers and provisioning only the capabilities verified for each device.

**Status:** Early scaffold. The Qt desktop shell is present; universal discovery and camera management are being implemented in milestones.

## Compatibility objective

Target devices include CP Plus, Hikvision, Prama, the project's own camera firmware, generic ONVIF cameras and other manufacturers. Compatibility is model- and firmware-specific: discovery does not guarantee activation, network configuration or firmware-upgrade support.

Discovery strategy:
- ONVIF WS-Discovery (UDP 3702) for standards-based devices.
- Project firmware discovery (UDP 9988) only for our own firmware.
- Verified vendor adapters when official protocol/SDK details and licensing permit.
- Bounded, opt-in local network scanning to find additional candidate endpoints.
- Manual device entry for devices that cannot be discovered automatically.

See [Universal compatibility strategy](docs/universal-compatibility.md) before adding integrations.

## Technology

- C++20
- Qt 6 Widgets, Network and Sql
- CMake
- SQLite
- Windows, macOS and Linux

## Build

Install CMake 3.21+, a C++20 compiler, and Qt 6.5+ with Widgets, Network and Sql modules.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run `./build/camera-ip-search` (on Windows, use the executable under the build configuration directory).

## Developer documentation

- [Architecture](docs/architecture.md)
- [Universal compatibility strategy](docs/universal-compatibility.md)
- [Adapter development guide](docs/adapter-development.md)
- [Implementation roadmap](docs/implementation-roadmap.md)
- [Compatibility and hardware test matrix](docs/compatibility-matrix.md)
- [Firmware communication contracts](docs/protocol-contract.md)
- [Development setup](docs/development.md)
- [Contributing](CONTRIBUTING.md)

## Safety and limitations

The tool will not guess passwords, bypass authentication or send undocumented configuration/activation commands. It will show unknown capabilities honestly. Firmware upgrades require verified model compatibility and integrity checks. Network scans are local, bounded and user initiated.

No software can guarantee support for every proprietary camera model. Actual support is recorded against exact model and firmware combinations in the compatibility matrix.
