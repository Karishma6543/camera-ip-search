# Implementation roadmap

This roadmap is the working sequence for contributors. Checkboxes represent repository work, not promises that a vendor or device has passed hardware validation.

## Milestone 0 — Documentation and architecture baseline
- [x] Define universal, capability-based compatibility strategy.
- [x] Separate ONVIF, vendor-specific, project firmware and fallback discovery paths.
- [x] Define contributor requirements for adapters and hardware validation.
- [x] Record security boundaries and unsupported/unknown states.

## Milestone 1 — Application foundations
- [ ] Introduce normalized device, observation, capability and structured error models.
- [ ] Add network-interface enumeration and explicit interface selection.
- [ ] Add a discovery coordinator with cancellation and progress reporting.
- [ ] Add deterministic unit-test fixtures and logging redaction.
- [ ] Keep UI responsive by running network work outside the GUI thread.

## Milestone 2 — Universal discovery
- [x] Implement ONVIF WS-Discovery Probe/ProbeMatch parsing and endpoint extraction.
- [x] Implement the project's UDP 9988 Contract B probe/response codec.
- [x] Add bounded opt-in IPv4 subnet candidate scanning.
- [x] Merge duplicate observations and preserve discovery provenance.
- [x] Add unknown-device and false-positive handling.
- [ ] Validate Windows, macOS and Linux behavior with hardware.

## Milestone 3 — Device information and inventory
- [x] Query ONVIF GetDeviceInformation when a same-device HTTP(S) endpoint is advertised.
- [x] Populate manufacturer, model, firmware, serial and hardware ID where returned.
- [x] Add a selectable device-details view from the inventory.
- [x] Query ONVIF capabilities and network interfaces (read-only; device/model hardware validation remains outstanding).
- [x] Persist non-sensitive device inventory and last-seen metadata in local SQLite (no credentials stored).
- [x] Add manual device entry and open-web-interface action.

## Milestone 4 — Vendor integrations
- [ ] Research and document official SDK/protocol options for CP Plus.
- [ ] Research and document official SDK/protocol options for Hikvision.
- [ ] Research and document official SDK/protocol options for Prama.
- [ ] Add vendor adapters only after exact models, licenses and wire behavior are verified.
- [ ] Add hardware results to the compatibility matrix.

## Milestone 5 — Safe provisioning
- [ ] Implement ONVIF network configuration only for devices advertising support.
- [ ] Implement project-camera activation and network configuration after firmware command contracts are frozen.
- [ ] Implement verified vendor-specific activation/configuration where authorized.
- [ ] Add batch preview, explicit confirmation, per-device result and rollback/recovery guidance.

## Milestone 6 — Firmware management
- [ ] Define per-vendor firmware package metadata and compatibility rules.
- [ ] Verify package integrity and model/hardware/region compatibility.
- [ ] Implement only vendor-authorized upgrade transports and recovery behavior.
- [ ] Add progress, cancellation policy and per-device results.

## Milestone 7 — Release readiness
- [ ] Complete Windows, macOS and Linux packaging.
- [ ] Complete automated tests and hardware validation matrix.
- [ ] Publish known limitations and supported model/firmware combinations.
- [ ] Review security, privacy, licensing and support documentation.

## Release policy

A feature is not “supported” merely because it compiles or passes a mock test. Use these labels:
- **Implemented:** code exists and automated tests pass.
- **Protocol-tested:** validated against a simulator or protocol fixture.
- **Hardware-tested:** validated against the exact documented physical model/firmware.
- **Vendor-certified:** only if the vendor has explicitly certified it.

Do not mark a whole brand as supported based on one model.
