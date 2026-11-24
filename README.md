# Oxita Camera Device Manager

A cross-platform Qt 6 desktop utility for discovering IP cameras, reviewing device/network information and performing a limited set of explicitly supported operations.

**Release status:** Pre-production. The latest `main` CI run (#605, commit `47b99f2`) passed the Release build and CTest suite on Windows, Ubuntu and macOS. Physical-camera interoperability, model-specific compatibility evidence, security review and signed distribution packaging are still release gates. Do not treat a manufacturer name or an open TCP port as proof of compatibility.

## Implemented in this build

- Enumerates active, non-loopback IPv4 network interfaces and lets the user choose the interface for discovery.
- Sends ONVIF WS-Discovery probes (UDP 3702).
- Sends the project's Contract B discovery probe (UDP 9988) for Oxita firmware only.
- Parses bounded ONVIF ProbeMatch XML and project JSON responses, including sequence/type checks, same-host endpoint validation and source-IP consistency checks.
- Enriches compatible ONVIF devices using GetDeviceInformation, GetCapabilities, GetNetworkInterfaces and GetDNS where the device responds and authentication succeeds.
- Provides an opt-in local IPv4 candidate scan. It checks common TCP service ports across a bounded maximum of 256 host addresses; on larger subnets the window is centered around the selected interface address. Results are **unverified network candidates**, not confirmed cameras.
- Checks reachability using known/common TCP ports.
- Supports manual device entry, device labels, CSV export and local SQLite inventory persistence. SQLite integrity is checked on startup; invalid databases are not automatically overwritten.
- Offers a guarded ONVIF static IPv4 workflow when a matching ONVIF interface token and current device-service endpoint have been obtained. It validates input, asks for confirmation and attempts to verify the new address. There is no automatic rollback.
- Provides a local Operation History workspace and user-confirmed diagnostics text export with recent activity and OS/application/network-interface summary.
- Includes a local Contract B/limited ONVIF simulator for deterministic pre-production testing; see [simulator and integration testing](docs/simulator-and-integration-testing.md).
- Keeps activation, firmware flashing, reboot, NVR, bulk provisioning and unsupported vendor operations unavailable rather than simulating success.

## Compatibility boundaries

The current code contains an ONVIF adapter and the project's own discovery codec. It does **not** contain verified CP Plus, Hikvision or Prama vendor adapters. ONVIF support varies by model, firmware, authentication configuration and enabled services. Static IP configuration may be rejected by a camera or its user permissions.

The compatibility matrix is the source of truth for model-specific test evidence:
[docs/compatibility-matrix.md](docs/compatibility-matrix.md).

## Build

Requirements: CMake 3.21+, a C++20 compiler and Qt 6.5+ with Widgets, Network, Sql and Xml. Qt Test is required only when tests are enabled.

Production-style build without test dependencies:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
```

Developer build and tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --config Debug
ctest --test-dir build --build-config Debug --output-on-failure
```

## Documentation

- [Architecture](docs/architecture.md)
- [Security and privacy review](docs/security-and-privacy-review.md)
- [Universal compatibility strategy](docs/universal-compatibility.md)
- [Adapter development guide](docs/adapter-development.md)
- [Implementation roadmap](docs/implementation-roadmap.md)
- [Production-readiness checklist](docs/production-readiness.md)
- [Compatibility and hardware test matrix](docs/compatibility-matrix.md)
- [Firmware communication contracts](docs/protocol-contract.md)
- [Development setup](docs/development.md)
- [Contributing](CONTRIBUTING.md)

## Safety and known limitations

- Discovery is local-link IPv4 only; limited broadcast and link-local multicast do not cross routers.
- The fallback scanner is bounded and user initiated. It does not identify a camera by itself.
- Camera credentials are requested interactively when challenged and are not intentionally written to the inventory database. OS-backed credential storage is not implemented.
- ONVIF write operations can disconnect a camera. Confirm reachability and network topology before changing its address; this build has no automatic rollback.
- No activation or firmware-upgrade command is defined for the project's Contract B protocol. Do not invent one.
- Cross-platform packaging, signing, security review and real-device compatibility evidence remain release gates.
