# Architecture

## Product goal

Oxita Camera Device Manager is a cross-platform desktop utility for discovering IP cameras, inspecting supported device information and performing only explicitly verified operations. It uses capability-based compatibility; it does not assume every camera supports every operation.

See:
- [Universal compatibility strategy](universal-compatibility.md)
- [Adapter development guide](adapter-development.md)
- [Implementation roadmap](implementation-roadmap.md)
- [Production-readiness checklist](production-readiness.md)
- [Compatibility and hardware test matrix](compatibility-matrix.md)
- [Firmware communication contract](protocol-contract.md)

## Technology currently used

- C++20
- Qt 6 Widgets for the desktop UI
- Qt Network for UDP discovery, TCP reachability and ONVIF HTTP/SOAP requests
- Qt SQL with SQLite for local, non-sensitive device inventory
- CMake
- Qt Test for automated unit tests

## Current source layout

```text
src/
  adapters/       ONVIF adapter registry and IPv4 validation
  core/           Device record model
  discovery/      ONVIF/project discovery, enrichment and subnet scanner
  ui/             Qt Widgets application and Oxita theme
tests/            Parser, adapter, ONVIF network-config, IPv4 validation
                  and subnet-scanner tests
docs/             Architecture, compatibility, protocol and release guidance
```

The source is currently more compact than the long-term target architecture. Application coordination, persistence and some protocol orchestration still live in the desktop window and discovery service. The modular boundaries below are a target for future refactoring, not a claim that all those modules already exist.

## Target module boundaries

- UI -> application coordinator/services -> core interfaces.
- Discovery providers return observations, not UI rows.
- A coordinator normalizes and merges observations while retaining provenance.
- Device operations are enabled only when a matching adapter and the required device information are available.
- SQLite repositories persist only approved non-sensitive state.
- The core domain should not depend on Qt Widgets.

## Discovery strategy

1. ONVIF WS-Discovery (UDP 3702 multicast) for standards-based devices.
2. Project firmware discovery (UDP 9988) strictly for the project's own firmware and Contract B.
3. Vendor-specific providers only when official protocol/SDK behavior is verified and licensing permits use.
4. Bounded, opt-in IPv4 scanning for additional candidate endpoints.
5. Manual entry for devices that cannot be discovered automatically.

These paths are complementary. Port responses and unverified fingerprints are candidate evidence, not proof of camera identity.

## Network limitations

Limited broadcast and link-local multicast do not cross routers by themselves. Current discovery is local-link IPv4; multi-interface selection does not create routed-subnet discovery. The fallback scanner checks at most 256 host addresses on the selected interface and common TCP ports. On larger subnets it scans a window centered around the selected interface address, not the entire subnet; it can still miss devices outside that window and reports candidates only.

## Reliability and security requirements

- Never ship default camera credentials or guess passwords.
- Do not persist plaintext credentials; prefer OS credential storage when implemented.
- Redact secrets from logs and diagnostics.
- Validate and bound packet size, schema, encoding, endpoint data, retries, timeouts, concurrency and scan ranges.
- Use asynchronous Qt networking and event-driven sockets so network waits do not block the UI; move CPU-heavy work off the GUI thread.
- Require confirmation for activation, network changes, reboot, batch actions and firmware upgrades.
- Verify firmware model/hardware compatibility and integrity before upgrade.
- Show per-device outcomes for batch operations and never hide partial failures.
- Avoid intrusive scanning and scan only user-selected local networks/ranges.

## Implementation status (source inspection; not hardware certification)

- [x] Qt Widgets desktop shell and Oxita visual theme.
- [x] Active IPv4 interface enumeration and interface selection.
- [x] ONVIF WS-Discovery probe and ProbeMatch parsing.
- [x] Project Contract B UDP 9988 discovery probe/response parser.
- [x] Bounded local IPv4 TCP candidate scanner (maximum 256 addresses).
- [x] ONVIF GetDeviceInformation, GetCapabilities, GetNetworkInterfaces and GetDNS request/response handling.
- [x] Reachability checks, manual device entry, CSV export and local SQLite inventory.
- [x] Guarded ONVIF static IPv4 request and post-change verification path.
- [ ] Full discovery and scanner integration tests on Windows, macOS and Linux.
- [ ] Hardware validation against exact camera models/firmware and network topologies.
- [ ] CP Plus, Hikvision, Prama and other vendor-specific adapters.
- [ ] Project-firmware activation and configuration (wire contract not yet defined).
- [ ] Secure credential storage and complete security review.
- [ ] Cross-platform installers, signing, upgrade/uninstall and release packaging.

