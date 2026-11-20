# Camera IP Search

A cross-platform desktop device manager for discovering, inspecting and safely configuring IP cameras and recorders across multiple manufacturers.

**Development status:** Active development; not feature-complete and not yet at Swatak Device Manager parity. The application currently includes ONVIF discovery and device-information queries, project-firmware discovery, bounded local subnet candidate scanning, a SQLite device inventory, reachability checks, CSV export, manual device entry and guarded ONVIF static IPv4 configuration.

## Product direction

The Swatak Device Manager screenshots supplied for this project establish the initial comparison baseline: device inventory, IP assignment (including automatic and range workflows), device details, camera preview, maintenance/password actions, firmware update, bulk camera configuration and NVR channels. The screenshots do not establish the underlying protocols, complete menus or supported models. See [Swatak feature parity specification](docs/swatak-feature-parity.md).

The goal is a multi-vendor device manager with capability-based operations. It must not pretend that a feature is supported merely because a device was discovered or a TCP port is open.

## Current implemented capabilities

- Qt 6 desktop application (C++20) for Windows, macOS and Linux.
- Active IPv4 interface enumeration and ONVIF WS-Discovery (UDP 3702).
- Project firmware discovery using its own Contract B UDP 9988 protocol.
- Bounded, opt-in local IPv4 subnet candidate scanning.
- ONVIF GetDeviceInformation, GetCapabilities and GetNetworkInterfaces read queries.
- SQLite inventory for non-sensitive device metadata; credentials are not stored.
- Manual device entry, device labels, search/filtering, CSV export and open-web-interface action.
- TCP reachability checks with explicit “no response” semantics.
- Validated ONVIF static IPv4 configuration with explicit confirmation and post-change verification.

## Not implemented / not hardware-validated yet

- Multi-device automatic and range-based IP assignment.
- Camera live preview and snapshot capture.
- Password/activation workflows and reboot.
- Vendor-specific CP Plus, Hikvision, Prama or Securus management operations.
- Firmware package compatibility verification and upgrade transports.
- Bulk image/day-night/IR configuration.
- NVR channel discovery and management.
- Exact-model hardware validation and production installers.

These are separate engineering workstreams. They will be enabled only when a protocol or vendor-authorized SDK is verified for the exact model/firmware and has appropriate licensing.

## Build

Install CMake 3.21+, a C++20 compiler, and Qt 6.5+ with Widgets, Network, Sql, Xml and Test modules.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Developer documentation

- [Architecture](docs/architecture.md)
- [Universal compatibility strategy](docs/universal-compatibility.md)
- [Adapter development guide](docs/adapter-development.md)
- [Implementation roadmap](docs/implementation-roadmap.md)
- [Swatak feature parity specification](docs/swatak-feature-parity.md)
- [Compatibility and hardware test matrix](docs/compatibility-matrix.md)
- [Firmware communication contracts](docs/protocol-contract.md)
- [Development setup](docs/development.md)
- [Contributing](CONTRIBUTING.md)

## Safety and release policy

The application will not guess passwords, bypass authentication or send undocumented configuration/activation commands. Unknown capabilities remain disabled. Network scans are bounded and opt-in. Firmware upgrades require verified model, hardware revision, region and package integrity checks. A feature is not considered hardware-supported until tested against the exact documented device and firmware. See the roadmap for the evidence labels used by this project.
