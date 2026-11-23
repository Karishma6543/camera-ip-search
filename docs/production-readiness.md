# Production-readiness assessment

This checklist is an engineering release gate for the current Oxita Camera Device Manager implementation. It distinguishes code present from behavior proven on real cameras.

## Current release scope

Implemented in source:
- Qt 6 desktop UI and Oxita theme.
- Active IPv4 interface selection.
- ONVIF WS-Discovery and project Contract B UDP 9988 discovery.
- ONVIF device information, capability, network-interface and DNS enrichment.
- Bounded TCP candidate scan (up to 256 addresses around the selected interface address on larger subnets).
- TCP reachability checks.
- Local SQLite inventory, manual entry, labels and CSV export.
- Guarded ONVIF static IPv4 request, confirmation and post-change verification path.
- Unsupported activation, firmware, reboot, NVR and batch operations remain unavailable.

Not implemented:
- CP Plus, Hikvision or Prama vendor adapters.
- Project-camera activation or network configuration commands.
- Firmware upload/update and recovery.
- DHCP toggle, DNS configuration and general network provisioning.
- Secure OS credential-vault integration and ONVIF WS-Security authentication.
- Cross-platform release installers and signed update distribution.

## Release gates

### P0 — Must pass before any production release

- [ ] A clean Release build succeeds on Windows, Ubuntu and macOS using the documented Qt version.
- [ ] All CTest targets pass on all three operating systems.
- [ ] GitHub Actions runner provisioning is restored; the previous workflow attempts failed before job steps ran and therefore provide no compile/test result.
- [ ] Run discovery against real ONVIF cameras and the project's own Contract B firmware; attach sanitized logs/packet evidence.
- [ ] Validate ONVIF static IPv4 changes on each specifically supported camera model/firmware, including authentication, rejected writes, address change, gateway failure and reboot-required response.
- [ ] Verify multi-NIC cameras never expose a configuration token unless the interface IPv4 address matches the discovered camera address.
- [ ] Complete a security review of network parsing, endpoint handling, credential prompts, local database contents and high-impact actions.
- [ ] Fill the compatibility matrix with exact model, hardware revision, firmware, topology and operation results. Until then, do not advertise named brands as supported.

### P1 — Required for a dependable general release

- [ ] Add discovery-service integration tests for selected interface, multiple IPv4 addresses, multicast/broadcast behavior, timeouts, duplicate responses and cancellation.
- [ ] Add scanner tests for full-subnet and capped-window selection, progress, cancellation, multiple open ports per host, and no-camera false positives.
- [ ] Add UI tests for startup, empty inventory, device selection, filters, scan cancellation, manual entry, CSV export and error states.
- [ ] Add SQLite migration/recovery tests, including old schema, interrupted migration, read-only directory, corrupt DB and disk-full behavior.
- [ ] Add an explicit scan-range/CIDR control or clearly retain the current 256-address window limitation in the UI. A bounded scan is not a full scan of a larger subnet.
- [ ] Add an operation history/audit record for configuration attempts and partial failures.
- [ ] Add per-device progress and tests for cancellation/overlapping operations.
- [x] Track and cancel in-flight ONVIF enrichment requests when discovery is cancelled or restarted; generation checks prevent stale replies from updating a newer scan.
- [ ] Add per-device progress and automated cancellation tests for reachability.
- [x] Persist the verified new camera IP and report a partial outcome when the later gateway update fails.
- [ ] Add guided operator recovery/reconnect instructions for partial network-configuration failures.
- [ ] Add structured, redacted logs and a support bundle with secrets removed.
- [ ] Verify application shutdown while network requests/scans are active.

### P2 — Distribution and maintainability

- [ ] Produce Windows installer/portable package, macOS app bundle and Linux package/AppImage.
- [ ] Test clean install, upgrade, uninstall, application-data preservation and rollback.
- [ ] Sign Windows artifacts and notarize macOS artifacts where distribution requires it.
- [ ] Add dependency/license inventory, privacy notice, support policy and release notes.
- [ ] Refactor database access out of MainWindow and introduce versioned migrations.
- [ ] Separate discovery providers and application coordination from the UI.
- [ ] Add localization/accessibility and high-DPI visual validation.

## Known operational limitations

- Discovery is local-link IPv4 only; multicast and limited broadcast do not cross routers.
- The fallback scanner is user-initiated, tests TCP ports only, and reports unverified candidates. On a subnet larger than 256 host addresses it checks a 256-address window around the selected interface address, not every host.
- An open port does not prove a device is a camera or that ONVIF is supported.
- ONVIF operation availability and write permissions vary by model, firmware and account role.
- Static IPv4 changes can disconnect a device. There is no automatic rollback.
- Project Contract B currently defines discovery only; activation/configuration/firmware commands must not be guessed.
- No CP Plus, Hikvision or Prama vendor-specific adapter is present in this build.

## Release decision

**Current status: NOT PRODUCTION READY.** The code has a useful pre-production feature set, but cross-platform builds/tests, real-device interoperability, security review and distribution packaging remain unproven or incomplete. A release may be described as a development/beta build only after the P0 build and test gates pass; supported model claims require hardware evidence.
