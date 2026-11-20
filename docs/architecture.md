# Architecture

## Product goal

Camera IP Search is a cross-platform, multi-vendor device manager for discovery, inspection and verified management of IP cameras and recorders. It uses capability-based compatibility: a device is not assumed to support activation, IP writes, preview, reboot, firmware or NVR management merely because discovery succeeded.

The initial UI comparison baseline is documented in [Swatak feature parity](swatak-feature-parity.md).

## Technology

- C++20
- Qt 6 Widgets for UI
- Qt Network for network transports
- SQLite for non-sensitive preferences, device history and operation metadata
- CMake and Qt Test

## Intended module boundaries

```text
src/
  app/                 Application composition and operation coordination
  core/                Device, observation, capability and error models
  discovery/           Discovery providers and coordinator
  adapters/            Capability adapters for verified device families
  devices/             Device information and read operations
  network/             Interface enumeration and network utilities
  provisioning/        IP assignment and activation workflows
  preview/             Live preview and snapshot services
  maintenance/         Reboot, password and maintenance operations
  firmware/            Package validation and authorized upgrade orchestration
  nvr/                 Recorder and channel inventory/management
  storage/             SQLite repositories and migrations
  ui/                  Qt Widgets presentation layer
tests/
  unit/
  integration/
  fixtures/
docs/
```

These are target boundaries; current code is incrementally migrating toward them.

## Layering

UI -> application coordinator/services -> core interfaces. Discovery providers and device adapters implement those interfaces using network transports. SQLite repositories persist approved non-sensitive state. The core domain should not depend on Qt Widgets. A provider returns observations, not UI rows. The coordinator normalizes and merges observations while retaining provenance.

Each adapter exposes explicit capabilities and operation contracts. The UI enables an operation only when the selected device has a verified adapter, the operation is supported for its model/firmware and the user has supplied required authorization.

## Discovery strategy

1. ONVIF WS-Discovery (UDP 3702 multicast) for standards-based devices.
2. Project firmware UDP 9988 Contract B discovery strictly for our own firmware.
3. Vendor-specific providers only when protocol/SDK behavior is verified and licensing permits.
4. Bounded, opt-in IPv4 scanning for candidate endpoints.
5. Manual entry for devices that cannot be discovered automatically.

Port responses and unverified fingerprints are candidate evidence, not proof of camera identity.

## Operation safety

- Never ship default camera credentials or guess passwords.
- Never persist plaintext credentials; use OS credential storage if persistent credentials are introduced.
- Redact secrets from logs and diagnostics.
- Validate packet size, schema, encoding and endpoint data.
- Bound retries, timeouts and concurrency; support cancellation.
- Keep network I/O off the UI thread.
- Require confirmation for activation, network changes, reboot, batch actions and firmware upgrades.
- Firmware upgrades require verified model/hardware/region compatibility and integrity checks.
- Batch operations report each target's result and never hide partial failures.
- Avoid intrusive scanning; scan only user-selected local networks/ranges.

## Current implementation status

Implemented in the repository: Qt Widgets shell, ONVIF and project-protocol discovery, bounded subnet candidate scanning, ONVIF device information/capability/network-interface reads, SQLite inventory, manual entry, search/filter, labels, CSV export, reachability checks and guarded ONVIF static IPv4 configuration.

Not yet complete: normalized capability model, fully asynchronous operation orchestration, vendor-specific management adapters, range IP assignment, preview/snapshot, reboot/password workflows, firmware update, bulk image configuration, NVR channels, packaging and exact-model hardware validation.
