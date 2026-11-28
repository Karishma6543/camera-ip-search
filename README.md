# Oxita Camera Device Manager

A cross-platform Qt 6 desktop utility for discovering IP cameras and other network devices, inspecting device/network information, and performing a limited set of explicitly supported ONVIF operations.

**Release status: Pre-production.** CI builds and automated tests run on Windows, Ubuntu and macOS, but CI is not proof of physical-camera compatibility. Real-device interoperability, exact model/firmware evidence, security review, and signed distribution packaging remain release gates. Do not treat a manufacturer name or an open TCP port as proof of compatibility.

> Discovery evidence, classification confidence, filters and known limitations are documented in [Network device discovery and classification](docs/network-device-discovery-and-classification.md).

## Technology stack

| Layer | Technology | Purpose |
|---|---|---|
| Language | C++20 | Desktop application and network/protocol logic |
| Desktop UI | Qt 6 Widgets | Native cross-platform desktop interface and dialogs |
| Networking | Qt Network | IPv4 interfaces, UDP discovery, TCP reachability and ONVIF HTTP/SOAP |
| Data | SQLite via Qt SQL | Local device inventory, schema/integrity checks and operation history |
| Parsing | Qt XML plus bounded JSON parsing | ONVIF XML and Oxita Contract B discovery responses |
| Build | CMake 3.21+ | Cross-platform configure, compile and install |
| Tests | Qt Test / CTest and Python simulator fixtures | Unit, parser, loopback network, startup and deterministic protocol tests |
| CI | GitHub Actions | Windows, Ubuntu and macOS build/test matrix, sanitizers and CodeQL |

The desktop client is written in C++/Qt; it is not a Flutter application and does not currently require a hosted backend or cloud service. Discovery and supported camera operations run locally on the user's network. Device inventory and operation history are stored in a local SQLite database.

## Live scan results versus saved data

The application currently has **persistent inventory**, not a fully separate live-only results workspace. It is important to understand the distinction:

- When the user starts **Discover cameras**, the current device table is cleared before the new ONVIF/Oxita/SSDP discovery run starts. The new results shown during that run come from current discovery responses.
- Clearing the table does **not** delete saved SQLite inventory or operation history.
- On application startup, saved inventory is loaded back into the device table and its reachability status is marked **Stale**. A stale row is historical information, not proof that the device is currently online or was found by the latest scan.
- **Scan subnet** is a separate, opt-in TCP port scan. It adds unverified network candidates; an open port is not proof that the endpoint is a camera.
- The current UI does not yet provide fully independent **Live Scan Results** and **Saved Inventory / History** workspaces. In particular, startup-loaded stale inventory and current scan results are not yet separated into distinct views. If the product requirement is to show only devices found in the latest scan, that UI/data-flow separation is still development work.

The selected-device details can show a last-seen UTC timestamp. Manual entries do not receive a fabricated last-seen time. Historical inventory and operation history remain stored locally unless the user explicitly uses the supported inventory-management workflow.

## Implemented in this build

- Enumerates active, non-loopback IPv4 network interfaces and allows selection of one interface or all eligible active IPv4 interfaces for discovery.
- Sends ONVIF WS-Discovery probes (UDP 3702).
- Sends the project's Contract B discovery probe (UDP 9988) for Oxita firmware only.
- Sends SSDP/UPnP M-SEARCH and parses bounded responses; SSDP LOCATION values are treated as untrusted metadata and are not fetched.
- Parses bounded ONVIF ProbeMatch XML and project JSON responses, including sequence/type checks, same-host endpoint validation and source-IP consistency checks.
- Enriches compatible ONVIF devices using GetDeviceInformation, GetCapabilities, GetServices, GetNetworkInterfaces and GetDNS where the device responds and authentication succeeds.
- Provides an opt-in local IPv4 TCP candidate scan across common management and device-service ports. Full scans are bounded to 4096 hosts; larger subnets use a 256-address window around the selected interface address. Results are **unverified candidates**, not confirmed cameras. The scanner does not identify devices solely from an open port.
- Checks reachability using known/common TCP ports.
- Supports manual device entry, local labels, CSV export and SQLite inventory persistence. SQLite integrity is checked before schema changes; invalid databases are not automatically overwritten.
- Provides SQLite backup/validation and guarded restore with a pre-restore recovery snapshot.
- Offers guarded ONVIF static IPv4, gateway, DHCP enable/disable and DNS workflows when matching ONVIF interface tokens and current device-service endpoints have been obtained. Static IPv4 and gateway settings use ONVIF read-back verification. DHCP and DNS require post-write read-back; DHCP address changes can require rediscovery. Bulk static IPv4 validates and confirms the assignment list, applies changes one camera at a time and records each result. There is no automatic rollback.
- Provides ONVIF reboot requests with response validation and explicit confirmation. A successful response means the request was accepted, not proof that the device completed rebooting.
- Implements ONVIF HTTP authentication challenge handling and WS-Security UsernameToken PasswordDigest with session-only credentials. Credentials are not intentionally saved in SQLite or application settings.
- Provides local Operation History and user-confirmed diagnostic text export. Diagnostics can contain local network details and must be reviewed before sharing.
- Includes a local Contract B/limited ONVIF simulator and deterministic fixture tests; see [Simulator and integration testing](docs/simulator-and-integration-testing.md).
- Keeps activation, firmware flashing, NVR management, general-purpose bulk provisioning and unsupported vendor operations unavailable rather than simulating success. Bulk support is currently limited to static IPv4 network assignments.

## Quick start and common workflow

1. Start the application and choose **All active IPv4 interfaces** or a specific Ethernet/Wi-Fi adapter.
2. Select **Discover cameras** for ONVIF WS-Discovery, Oxita Contract B and SSDP/UPnP discovery. Starting this action clears the current device table before collecting the new discovery results.
3. Optionally select **Scan subnet** to look for hosts that did not answer discovery probes. This is a TCP port scan; results are unverified candidates and may include non-camera devices.
4. Select a row to inspect it in the **Device Info** workspace. Use **Configuration** for the supported device/network workflows and **Operation History** for recorded activity.
5. Use filters/search to narrow the currently displayed inventory, **Rename** to set a local friendly label, and **Export CSV** to save an inventory snapshot.
6. Use **Check reachability** to test selected/checked devices or the whole displayed inventory.
7. Before changing network settings, verify the selected camera, credentials, permissions and recovery access. Read the [UI user guide](docs/ui-user-guide.md).

## Camera compatibility: what “support” means

The application uses a standards-first ONVIF adapter and the project's own Oxita Contract B discovery codec. A CP PLUS, Prama, Hikvision or other manufacturer's camera may be discoverable and manageable through ONVIF **if its exact model and firmware expose the required ONVIF services and the network permits discovery traffic**.

This does not mean every model or every feature from that brand is supported. The repository does **not** currently contain validated CP PLUS, Prama or Hikvision proprietary vendor adapters. ONVIF feature availability, authentication, permissions and write behavior vary by model and firmware. Factory-reset request construction exists, but the UI action remains disabled pending exact-model/firmware validation.

The [compatibility matrix](docs/compatibility-matrix.md) is the source of truth for model-level evidence. No physical CP PLUS, Prama or Hikvision model/firmware combination is certified by CI. Do not advertise a brand as universally supported until representative physical hardware has been tested and the matrix records the results.

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

## Remaining engineering work

The detailed, maintained checklist is [Implementation roadmap](docs/implementation-roadmap.md) and the release gate is [Production readiness](docs/production-readiness.md). The highest-priority open work is:

1. **Separate live and historical data in the UI.** Add distinct Live Scan Results and Saved Inventory/History views. Ensure each new discovery/refresh run creates a new live result set, subnet candidates are clearly distinguished, and stale saved records never appear as current scan results. Define explicit retention/deletion controls for historical records.
2. **Complete discovery integration validation.** Add full DiscoveryService tests for interface selection, multiple NICs, multicast/broadcast delivery, duplicate responses, timeouts, cancellation races and enrichment. Validate Windows/macOS/Linux behavior on physical Ethernet and Wi-Fi networks, including VLANs and firewalls.
3. **Expand UI and scanner acceptance coverage.** Test every workspace, filters, dialogs, CSV export, empty/error states, repeated scans, large inventories, full-subnet/window selection, cancellation and false-positive handling.
4. **Validate real cameras.** Test exact CP PLUS and Prama models (and additional target vendors) with model, hardware revision, firmware, account role and network topology recorded in the compatibility matrix. Simulator and CI results are not hardware evidence.
5. **Implement vendor-specific adapters only where justified.** Research authorized official APIs/SDKs and licensing for Hikvision and other target vendors. Do not guess proprietary endpoints or bypass authentication.
6. **Finish safe provisioning recovery.** Validate static-IP, gateway, DHCP, DNS and reboot on physical cameras; add transport-loss/reboot-required and remaining read-back edge cases, plus guided recovery for partial failures.
7. **Add secure persistent credential storage.** Integrate Windows Credential Manager, macOS Keychain and Linux Secret Service with explicit opt-in. Until then, credentials are session-only.
8. **Complete security/privacy review.** Finish adversarial log-redaction review, TLS certificate interoperability tests, endpoint/URL trust review, shutdown/cancellation checks and diagnostic-export privacy validation.
9. **Complete release engineering.** Produce and test installers/portable builds, signing/notarization and update distribution; publish a release evidence record tying source commit, CI, security review and hardware matrix together.

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
- [UI user guide](docs/ui-user-guide.md)
- [UI/UX design system](docs/ui-ux-guidelines.md)
- [Contributing](CONTRIBUTING.md)

## Safety and known limitations

- Discovery is local-link IPv4 only; limited broadcast and link-local multicast do not cross routers. The application does not promise discovery across VLANs or routed subnets.
- The fallback scanner is bounded, user initiated and TCP-only. UDP-only services may be missed; a positive TCP connection is only an unverified candidate.
- Persisted inventory is loaded as **Stale** on startup. The device table is not yet a dedicated live-only view; see [Live scan results versus saved data](#live-scan-results-versus-saved-data).
- Camera credentials are requested interactively when challenged and are session-only. OS-backed credential-vault integration is not implemented.
- ONVIF write operations can disconnect a camera. Confirm reachability and network topology before changing its address; there is no automatic rollback.
- There is no project-camera activation or firmware-upgrade command in the Contract B protocol.
- Cross-platform packaging, signing, security review and real-device compatibility evidence remain release gates.
