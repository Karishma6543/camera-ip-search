# Swatak Device Manager feature parity baseline

## Source and confidence

This baseline is derived from the three-page screenshot document supplied for this project (Doc2.docx, pages 1–3). It records only visible controls and information. It is not an official Swatak manual and does not establish the protocol, exact behavior, complete feature set or model compatibility.

The objective is to reproduce the useful device-management workflows in our own multi-vendor application, not to copy proprietary implementation details or assume that Securus-specific commands work on other brands.

## Visible workflows in the supplied screenshots

| Area | Visible evidence | Product requirement |
|---|---|---|
| Device inventory | List columns include IP address, description, product series and model; second screenshot also shows MAC, device ID and HTTP/RTSP-related port information | Searchable inventory, multi-select, stable device identity and per-device status |
| Maintenance | Maintenance panel shows Reboot, Password and additional maintenance actions | Authenticated, capability-gated maintenance operations with confirmation and audit results |
| Network configuration | IP address, subnet mask, gateway and Submit Changes | Read current settings, validate proposed values, preview, confirm, apply and verify |
| IP assignment | Auto IP Assign and Range IP Assign; range workflow shows a target device and IP range | Safe IP planner, collision checks, explicit device selection and per-device results |
| Device details | IP, MAC, model, firmware, ports, device ID and release date | Structured details panel with source/provenance and unknown values represented honestly |
| Camera preview | Camera Preview image appears in details panel | Live preview and snapshot where supported and authorized |
| Firmware update | Firmware file selection, product-series selector, Update and Smart Update | Package metadata, integrity/compatibility checks and vendor-authorized transport |
| Bulk configuration | Day/Night, ICR, Easy Mode and Apply Image/IR controls | Read-before-write, capability-aware multi-device settings and per-device results |
| NVR channels | NVR Channels tab/button | Recorder channel discovery and supported channel operations |

## Current project status against this baseline

The repository currently has device discovery/inventory, device information, guarded single-device ONVIF static IPv4 configuration, reachability, CSV export and a desktop UI shell with placeholder sections for some future workflows.

The following screenshot workflows are not implemented as end-to-end device operations yet: automatic/range IP assignment, preview/snapshot, reboot, password management, firmware update, bulk image/day-night/IR settings and NVR channel management. A visible placeholder control does not count as implemented functionality.

## Engineering rules

1. **Capability-driven:** each operation is exposed only when an adapter confirms support for the exact device/model/firmware.
2. **Protocol evidence:** prefer ONVIF standard operations where applicable; use vendor SDKs/protocols only with official documentation, permission and license compliance.
3. **No guessed commands:** do not infer undocumented commands from screenshots or packet guesses.
4. **Safe bulk changes:** preview all targets, detect conflicts, require explicit confirmation and report every device's result.
5. **Credentials:** request credentials only when needed, never log them, and do not store plaintext credentials.
6. **Firmware safety:** validate vendor, product series, model, hardware revision, region, version and package integrity before enabling updates.
7. **Hardware evidence:** automated fixtures are not a substitute for tests on exact physical models and firmware.

## Acceptance criteria for parity

A workflow is complete only when it has:
- a real operation service (not a decorative/enabled button);
- capability and authorization checks;
- input validation and a confirmation step for state-changing actions;
- bounded timeout/retry and cancellation behavior;
- structured per-device success/failure and useful recovery guidance;
- unit/protocol tests;
- documented exact-model hardware test evidence before being labelled hardware-supported.

## Still needed to verify Swatak behavior

The screenshots do not reveal authentication transport, supported brands/models, firmware signature rules, preview protocol, exact bulk-setting semantics, NVR channel behavior, export/logging, device activation, or the full maintenance menu. An official manual or additional screenshots/video are needed before treating these as Swatak requirements.
