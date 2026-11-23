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
- [ ] Complete the security and privacy review in [security-and-privacy-review.md](security-and-privacy-review.md), including TLS, WS-Security, OS credential vault, redaction tests and malformed-input testing.
- [ ] Fill the compatibility matrix with exact model, hardware revision, firmware, topology and operation results. Until then, do not advertise named brands as supported.

### P1 — Required for a dependable general release

- [ ] Complete end-to-end discovery-service integration tests for selected interface, multiple IPv4 addresses, multicast/broadcast delivery, timeouts, duplicate responses and cancellation. [x] Add a service lifecycle test that starts probes on an available physical IPv4 interface and cancels them. C++ loopback UDP/TCP tests exercise datagram round trips and ONVIF parsing; neither proves multicast/broadcast delivery on physical NICs.
- [ ] Add scanner tests for full-subnet and capped-window selection, progress, cancellation, real host probing and no-camera false positives. [x] Add validation coverage for network/broadcast boundaries and /30 host boundaries. [x] Add deterministic multi-port aggregation and duplicate suppression coverage; live multi-port host probing still requires a controlled LAN fixture.
- [ ] Expand UI tests beyond the startup smoke test: [x] verify all five workspaces, stable automation selectors, empty-inventory reachability error, search filtering and ONVIF/Oxita manufacturer filtering. Still required: device selection/detail rendering, manual-entry and rename dialogs, CSV export, static-IP confirmation/recovery dialogs, scan cancellation and persistence/error states.
- [ ] Add SQLite migration/recovery tests for read-only directory, disk-full, backup/restore and process interruption. [x] Add legacy-schema migration, forced-failure rollback and corrupt-database preservation tests. The startup migration is transactional and versioned with SQLite `user_version`.
- [ ] Add an explicit scan-range/CIDR control or clearly retain the current 256-address window limitation in the UI. A bounded scan is not a full scan of a larger subnet.
- [x] Add a local operation-history view for discovery, scan, reachability, inventory and static-IPv4 outcomes; retention is capped at 1,000 records. Configuration audit details still need broader stage-by-stage coverage.
- [x] Track and cancel in-flight ONVIF enrichment requests when discovery is cancelled or restarted; generation checks prevent stale replies from updating a newer scan.
- [x] Add per-device progress and user-visible cancellation for reachability checks.
- [ ] Add automated tests for reachability timeout and cancellation. [x] Test cancellation and ensure a second start cannot replace an active reachability batch.
- [x] Persist the verified new camera IP and report a partial outcome when the later gateway update fails.
- [x] Add guided operator recovery/reconnect instructions when the new IP is verified but a subsequent gateway update is not confirmed.
- [x] Add rotating application log files and regression tests for common key/value, Bearer, URL user-info and XML secret redaction. [x] Add an archive-rotation/three-file-retention regression test. Redaction remains best-effort and needs broader security review.
- [x] Add a user-confirmed diagnostics text export with OS/application/interface summary, recent operation history and redacted rotating logs; it excludes the inventory database and ONVIF interface tokens.
- [x] Distinguish completed and cancelled discovery/subnet scan outcomes in the activity history.
- [x] Request cancellation of discovery, subnet scanning and reachability work during MainWindow shutdown.
- [x] Add a window-shutdown regression test that closes an active ONVIF enrichment socket, plus a live subnet-scan cancellation test. [ ] Run under all CI platforms and verify no pending replies/sockets or worker activity remain.

### P2 — Distribution and maintainability

- [ ] Produce Windows installer/portable package, macOS app bundle and Linux package/AppImage.
- [ ] Test clean install, upgrade, uninstall, application-data preservation and rollback.
- [ ] Sign Windows artifacts and notarize macOS artifacts where distribution requires it.
- [ ] Add dependency/license inventory, privacy notice, support policy and release notes.
- [ ] Refactor database access out of MainWindow. [x] Introduce transactional, versioned SQLite schema migration (`user_version = 1`) with future-version rejection.
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

## New pre-production hardening work

- [x] Add a local simulator for the documented Contract B discovery exchange and a limited ONVIF WS-Discovery/device-service fixture.
- [x] Add deterministic Python tests for simulator response construction and malformed/stale/invalid fixtures.
- [x] Run simulator fixture tests in the Windows/macOS/Linux CI matrix before the Qt build.
- [x] Add a Qt startup smoke-test target that checks the Operation History workspace and SQLite schema version are initialized.
- [x] Add an in-flight ONVIF SOAP response limit (1 MiB) and a regression test using a 2 MiB local HTTP peer.
- [x] Exercise `DiscoveryService` ONVIF device-information enrichment against a local HTTP peer, including manufacturer/model/firmware/serial parsing.
- [x] Add an operation-history table and a bounded recent-activity view.
- [x] Add user-confirmed diagnostic text export with OS/application/interface summary and recent operation records.
- [x] Check SQLite integrity before schema changes and leave an invalid database untouched.
- [x] Make the initial legacy-schema upgrade transactional and record the schema version atomically.
- [x] Add a startup assertion for the schema version.

These are source changes only until the workflow runs successfully. Simulator fixtures do not certify real cameras or the physical network stack.

## Release decision

**Current status: NOT PRODUCTION READY.** The code has a useful pre-production feature set, but cross-platform builds/tests, real-device interoperability, security review and distribution packaging remain unproven or incomplete. A release may be described as a development/beta build only after the P0 build and test gates pass; supported model claims require hardware evidence.
