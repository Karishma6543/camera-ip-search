# Architecture

## Product goal

Camera IP Search is a cross-platform desktop utility for discovering and, where verified, provisioning IP cameras from multiple manufacturers. It targets Windows, macOS and Linux and uses capability-based compatibility rather than assuming every camera supports every operation.

See:
- [Universal compatibility strategy](universal-compatibility.md)
- [Adapter development guide](adapter-development.md)
- [Implementation roadmap](implementation-roadmap.md)
- [Compatibility and hardware test matrix](compatibility-matrix.md)
- [Firmware communication contract](protocol-contract.md)

## Technology

- C++20
- Qt 6 Widgets for UI
- Qt Network for network transports
- SQLite for non-sensitive preferences, device history and operation metadata
- CMake
- Qt Test / GoogleTest for automated tests

## Target module boundaries

```text
src/
  app/                 Application composition and startup
  core/                Device, observation, capability and error models
  discovery/           Discovery coordinator and provider interfaces
  discovery/onvif/     ONVIF WS-Discovery provider
  discovery/project/   Project firmware UDP 9988 provider
  discovery/vendor/    Verified vendor-specific discovery providers
  discovery/scanner/   Bounded opt-in network candidate scanner
  devices/             Device enrichment and operation services
  network/             Interface enumeration and network utilities
  adapters/            Capability adapters for verified device families
  provisioning/        Activation and network configuration workflows
  firmware/            Verified firmware upgrade orchestration
  storage/             SQLite repositories and migrations
  ui/                  Qt Widgets presentation layer
tests/
  unit/
  integration/
  fixtures/
docs/
```

These are intended boundaries; contributors should add modules incrementally and keep them independently testable.

## Layering and dependency direction

UI -> application coordinator/services -> core interfaces. Discovery providers and vendor/project adapters implement those interfaces using network transports. SQLite repositories persist only approved non-sensitive state. The core domain must not depend on Qt Widgets.

A discovery provider returns observations, not UI rows. The coordinator normalizes and merges observations while retaining provenance. Device management operations are enabled only when an adapter advertises that capability and the device's authentication/authorization state permits it.

## Discovery strategy

1. ONVIF WS-Discovery (UDP 3702 multicast) for standards-based devices.
2. Project firmware discovery (UDP 9988) strictly for the project's own firmware.
3. Vendor-specific providers only when protocol/SDK behavior is verified and licensing permits use.
4. Bounded, opt-in IPv4 scanning for additional candidate endpoints.
5. Manual entry for devices that cannot be discovered automatically.

These paths are complementary. They must not be conflated. Port responses and unverified fingerprints are candidate evidence, not proof of camera identity.

## Network limitations

Limited broadcast `255.255.255.255` and link-local multicast do not cross routers by themselves. Per-interface scanning can improve coverage on a host with multiple connected networks, but routed-subnet discovery needs directed broadcast where allowed or an explicit relay/agent. Do not claim subnet-independent scanning until tested end-to-end.

## Reliability and security

- Never ship default camera credentials or guess passwords.
- Do not persist plaintext credentials; prefer OS credential storage when implemented.
- Redact secrets from logs and diagnostics.
- Validate packet size, schema, encoding and endpoint data before use.
- Bound retries, timeouts, concurrency and scan ranges; support cancellation.
- Keep network I/O off the UI thread.
- Require confirmation for activation, network changes, reboot, batch actions and firmware upgrades.
- Verify firmware model/hardware compatibility and integrity before upgrade.
- Show per-device outcomes for batch operations and never hide partial failures.
- Avoid intrusive scanning and only scan user-selected local networks/ranges.

## Current implementation status

- [x] Qt Widgets application shell
- [x] CMake build definition
- [x] Universal compatibility and adapter strategy documented
- [ ] Normalized device/capability models
- [ ] Network interface enumeration
- [ ] ONVIF WS-Discovery provider
- [ ] Project UDP 9988 discovery provider
- [ ] Bounded fallback scanner
- [ ] Discovery coordinator and duplicate merging
- [ ] Device inventory binding and SQLite history
- [ ] Verified vendor adapters
- [ ] Provisioning and firmware workflows
- [ ] Cross-platform packaging and hardware validation
