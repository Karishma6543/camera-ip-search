# Camera IP Search

A cross-platform desktop utility for discovering IP cameras from multiple manufacturers and provisioning only the capabilities verified for each device.

**Status:** Early implementation. The Qt desktop shell now runs active-interface discovery using ONVIF WS-Discovery (UDP 3702) and the project's own Contract B protocol (UDP 9988). Vendor-specific discovery and fallback subnet scanning are next.

## Compatibility objective

Target devices include CP Plus, Hikvision, Prama, the project's own camera firmware, generic ONVIF cameras and other manufacturers. Compatibility is model- and firmware-specific: discovery does not guarantee activation, network configuration or firmware-upgrade support.

Discovery strategy:
- ONVIF WS-Discovery (UDP 3702) for standards-based devices.
- Project firmware discovery (UDP 9988) only for our own firmware.
- Verified vendor adapters when official protocol/SDK details and licensing permit.
- Bounded, opt-in local network scanning to find additional candidate endpoints.
- Manual device entry for devices that cannot be discovered automatically.

## Build

Install CMake 3.21+, a C++20 compiler, and Qt 6.5+ with Widgets, Network, Sql and Xml modules.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Developer documentation

- [Architecture](docs/architecture.md)
- [Universal compatibility strategy](docs/universal-compatibility.md)
- [Adapter development guide](docs/adapter-development.md)
- [Implementation roadmap](docs/implementation-roadmap.md)
- [Compatibility and hardware test matrix](docs/compatibility-matrix.md)
- [Firmware communication contracts](docs/protocol-contract.md)
- [Development setup](docs/development.md)
- [Contributing](CONTRIBUTING.md)

## Current implementation

- [x] Qt desktop shell
- [x] Active IPv4 interface enumeration
- [x] ONVIF WS-Discovery probe and basic ProbeMatch parsing
- [x] Project firmware UDP 9988 probe and Contract B response parsing
- [x] Basic device inventory population
- [ ] Vendor-specific adapters
- [ ] Bounded fallback subnet scanning
- [ ] ONVIF device-information and capability queries
- [ ] SQLite history, credentials integration and provisioning

## Safety and limitations

The tool will not guess passwords, bypass authentication or send undocumented configuration/activation commands. It shows unknown capabilities honestly. Firmware upgrades require verified model compatibility and integrity checks. Discovery is limited to active local IPv4 interfaces; it does not scan remote routed subnets.
