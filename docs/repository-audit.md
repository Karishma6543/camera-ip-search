# Repository audit — 2026-11-27

## Reviewed revision

- Repository: `Karishma6543/camera-ip-search`
- Base revision: `3c4583e1d2e6003c3389dcfae1d31777725c254a` (the merge commit for PR #16)
- Default branch: `main`
- Repository inventory at review time: 135 tracked tree entries.

## Review coverage

The review mapped the complete tracked tree and inspected the build configuration, CI workflows, application entry point, UI coordination, adapter registry and capability contract, discovery and ONVIF services, parsers, identity merging, network validation and assignment planning, SQLite backup/restore and migration paths, diagnostics, firmware package validation, unit/integration tests, simulator fixtures, and the product/developer/release documentation.

The repository contains a C++20 / Qt 6 Widgets desktop application. It is separate from the Flutter mobile app and from the camera firmware web UI. The application currently uses ONVIF WS-Discovery, SSDP/UPnP, the project's own UDP 9988 Contract B discovery message, and an opt-in bounded TCP candidate scan. It has no verified proprietary CP PLUS, Prama or Hikvision adapter.

## Defect found and corrected in this audit

### Discovery UDP socket lifetime

The discovery timeout and explicit-cancel paths closed each per-interface `QUdpSocket` and cleared the tracking vector, but did not schedule the QObject children for deletion. Because the sockets are parented to `DiscoveryService`, repeated discovery runs could retain closed socket objects until the service was destroyed.

The fix now calls `deleteLater()` after closing each socket in both timeout and cancellation paths. The selected-interface cancellation test also asserts that the service's child UDP sockets are released after the event loop processes deferred deletion.

## Documentation corrections

- Updated the README to record PR #28's completed static IPv4 verification: address, subnet prefix, DHCP-disabled state and configured gateway read-back.
- Removed stale "PR #28 pending CI" language and marked the merged PR's test evidence accurately.
- Corrected the UI guide's duplicate "Operation Operation History" label.
- Corrected the UI guide's feature list: DHCP, DNS and reboot are implemented ONVIF operations; factory-reset request code is present but its UI action remains disabled pending hardware evidence.
- Corrected development and architecture descriptions that incorrectly listed reboot as unavailable.
- Corrected the diagnostics log retention description to match source: approximately 5 MiB active log plus one rotated archive.
- Preserved the distinction between implementation, protocol/CI tests and physical-camera validation.

## Validation evidence and limitations

PR #16's exact head commit passed the Windows, Ubuntu and macOS build/test jobs, Ubuntu ASan/UBSan, Windows/macOS/Ubuntu release packaging, and CodeQL C++/Python. Dependency review was skipped because it is conditional on the repository dependency-graph setting. Those checks ran on the PR head before merge; no separate workflow run was returned for the merge commit during this audit.

The code change and documentation edits in this audit require their own CI run on the new PR head. No physical CP PLUS, Prama, Hikvision or project-firmware camera was available for this review, so no hardware compatibility claim is made.

## Remaining engineering work

1. Add end-to-end static-IP service tests for changed-address reconnect, stale address, gateway mismatch/partial failure and post-write transport loss.
2. Expand UI acceptance tests for CSV export, static-IP confirmation/recovery, scan cancellation and persistence/error states.
3. Complete database failure tests for read-only directories, disk-full and abrupt process interruption.
4. Complete cross-platform TLS/certificate, credential-redaction, OS credential-vault and dependency/license reviews.
5. Validate discovery and every state-changing ONVIF operation on exact physical models/firmware; maintain the compatibility matrix at model level.
6. Complete signed installers, install/upgrade/uninstall testing, SBOM and release evidence before any production release.

**Audit conclusion:** the repository has a substantial pre-production implementation and broad automated test coverage, but it is not production-ready or universally compatible with all camera models. A passing CI matrix does not replace model-specific hardware validation.
