# Oxita Camera Device Manager

A cross-platform Qt 6 desktop utility for discovering IP cameras, reviewing device/network information and performing a limited set of explicitly supported operations.

**Release status:** Pre-production. The cross-platform CI workflow builds and runs the automated test suite on Windows, Ubuntu and macOS. Check the repository Actions page for the result associated with the exact commit you intend to release. Physical-camera interoperability, model-specific compatibility evidence, security review and signed distribution packaging are still release gates. Do not treat a manufacturer name or an open TCP port as proof of compatibility.


> Network discovery and device-type evidence, confidence levels, filter behavior and known limitations are documented in [Network device discovery and classification](docs/network-device-discovery-and-classification.md).
## Implemented in this build

- Enumerates active, non-loopback IPv4 network interfaces and lets the user choose the interface for discovery.
- Sends ONVIF WS-Discovery probes (UDP 3702).
- Sends the project's Contract B discovery probe (UDP 9988) for Oxita firmware only.
- Parses bounded ONVIF ProbeMatch XML and project JSON responses, including sequence/type checks, same-host endpoint validation and source-IP consistency checks.
- Enriches compatible ONVIF devices using GetDeviceInformation, GetCapabilities, GetNetworkInterfaces and GetDNS where the device responds and authentication succeeds.
- Provides an opt-in local IPv4 candidate scan. It checks common TCP service ports across a bounded range: full scans are available up to 4096 hosts; larger subnets use a 256-address window centered around the selected interface address. Results are **unverified network candidates**, not confirmed cameras.
- Checks reachability using known/common TCP ports.
- Supports manual device entry, device labels, CSV export and local SQLite inventory persistence. SQLite integrity is checked on startup; invalid databases are not automatically overwritten.
- Offers guarded single-camera and sequential bulk ONVIF static IPv4 workflows when matching ONVIF interface tokens and current device-service endpoints have been obtained. Bulk mode requires per-camera unique addresses, validates and confirms the full assignment list, applies changes one camera at a time and records each verification result. There is no automatic rollback.
- Provides ONVIF factory reset through SetSystemFactoryDefault with explicit Soft/Hard selection and destructive-action confirmations when the camera exposes and authorizes that operation.
- Provides a local Operation History workspace and user-confirmed diagnostics text export with recent activity and OS/application/network-interface summary.
- Includes a local Contract B/limited ONVIF simulator for deterministic pre-production testing; see [simulator and integration testing](docs/simulator-and-integration-testing.md).
- Keeps activation, firmware flashing, reboot, NVR, general-purpose bulk provisioning and unsupported vendor operations unavailable rather than simulating success. Bulk support is currently limited to static IPv4 network assignments.

## Compatibility boundaries

The current code contains an ONVIF adapter and the project's own discovery codec. It does **not** contain verified CP Plus, Hikvision or Prama vendor adapters. ONVIF support varies by model, firmware, authentication configuration and enabled services. Static IP configuration and factory reset may be rejected by a camera, firmware variant or account permissions. Factory reset is only attempted through the ONVIF device service; vendor-specific reset paths are not implemented.

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
- [Credential-storage design and implementation gate](docs/credential-storage.md)
- [Release engineering and packaging](docs/release-engineering.md)
- [Universal compatibility strategy](docs/universal-compatibility.md)
- [Adapter development guide](docs/adapter-development.md)
- [Implementation roadmap](docs/implementation-roadmap.md)
- [Production-readiness checklist](docs/production-readiness.md)
- [Compatibility and hardware test matrix](docs/compatibility-matrix.md)
- [Firmware communication contracts](docs/protocol-contract.md)
- [Development setup](docs/development.md)
- [UI/UX design system and interaction guidelines](docs/ui-ux-guidelines.md)
- [Contributing](CONTRIBUTING.md)

## Safety and known limitations

- Discovery is local-link IPv4 only; limited broadcast and link-local multicast do not cross routers.
- The fallback scanner is bounded and user initiated. It does not identify a camera by itself.
- Camera credentials are requested interactively when challenged and are not intentionally written to the inventory database. OS-backed credential storage is not implemented.
- ONVIF write operations can disconnect a camera. Confirm reachability and network topology before changing its address; this build has no automatic rollback.
- No activation or firmware-upgrade command is defined for the project's Contract B protocol. Do not invent one.
- Cross-platform packaging, signing, security review and real-device compatibility evidence remain release gates.
