# Implementation roadmap

This roadmap tracks repository work and evidence. A UI control is not a completed feature unless it performs a supported operation, reports per-device results and has tests.

## Milestone 0 — Product baseline
- [x] Define capability-based, multi-vendor compatibility strategy.
- [x] Document security boundaries and unknown/unsupported states.
- [x] Capture Swatak screenshot-based feature parity baseline.
- [ ] Obtain official Swatak manual/installer or additional screenshots to verify hidden workflows and exact terminology.

## Milestone 1 — Application foundations
- [x] Qt desktop shell and CMake/CTest build.
- [x] SQLite persistence for non-sensitive inventory.
- [x] Active IPv4 interface enumeration.
- [ ] Normalized device, observation and structured error models.
- [x] Explicit typed device-operation capability contract with conservative unsupported defaults.
- [ ] Explicit interface selection throughout discovery (not just subnet scanning).
- [x] Capability-gated batch-operation preflight with per-device outcomes.
- [x] Sequential batch runner with per-device success/failure results, progress signals and cancellation between targets; concrete protocol handlers remain gated until verified.
- [ ] Move all potentially blocking network work off the GUI thread.
- [ ] Centralized redacted logging and diagnostic export.

## Milestone 2 — Discovery and device inventory
- [x] ONVIF WS-Discovery and ProbeMatch parsing.
- [x] Project UDP 9988 Contract B discovery.
- [x] Bounded opt-in IPv4 subnet candidate scanning.
- [x] ONVIF GetDeviceInformation, GetCapabilities and GetNetworkInterfaces read operations.
- [x] Manual entry, inventory, search/filter, labels, CSV export and reachability checks.
- [ ] Add verified vendor discovery adapters where official protocol/SDK and license permit.
- [ ] Hardware-test discovery on exact CP Plus, Hikvision, Prama, Securus and generic ONVIF models.

## Milestone 3 — Network configuration
- [x] Validate static IPv4 address, subnet mask, gateway and DNS input.
- [x] ONVIF static IPv4 interface configuration with explicit confirmation.
- [x] Verify the configured address after reconnecting.
- [x] Implement a dry-run IP range planner with reserved-address exclusions and stable selected-device mapping (live collision checks remain pending).
- [ ] Add safe automatic/range assignment only for adapters that advertise verified write support.
- [ ] Add batch preview, confirmation, cancellation policy, per-device result and recovery guidance.
- [ ] Implement DHCP, DNS and gateway writes only when supported by the exact adapter.
- [ ] Hardware-test each write operation and document loss-of-connectivity recovery.

## Milestone 4 — Device maintenance and preview
- [ ] Camera preview using a supported RTSP/HTTP stream path; never assume credentials or codecs.
- [ ] Snapshot capture with explicit destination and user action.
- [ ] Reboot only through a verified authenticated operation with confirmation.
- [ ] Password/activation workflows only through official supported mechanisms; never log or persist plaintext secrets.
- [ ] Add device time, service/port diagnostics and operation history where supported.

## Milestone 5 — Bulk camera configuration
- [ ] Define normalized image/day-night/IR settings and capability mapping.
- [ ] Implement read-before-write and preview of current versus proposed values.
- [ ] Apply only settings supported by every selected target or clearly partition results by adapter.
- [ ] Provide per-device success/failure, cancellation and audit history.
- [ ] Add protocol fixtures and exact-model hardware tests.

## Milestone 6 — Firmware management
- [ ] Define firmware package metadata (vendor, product series, model, hardware revision, region, version).
- [ ] Verify package hash/signature and compatibility before enabling an update.
- [ ] Implement vendor-authorized update transport only.
- [ ] Show progress and device-specific outcomes; handle power-loss and recovery warnings.
- [ ] Never offer a generic firmware flash operation for unknown devices.

## Milestone 7 — NVR channel management
- [ ] Discover recorder/channel inventory through a verified ONVIF or vendor interface.
- [ ] Display channel, source, status and model information.
- [ ] Add supported channel add/edit/remove workflows with preview and confirmation.
- [ ] Hardware-test recorder models and channel limits.

## Milestone 8 — Release readiness
- [ ] Complete Windows, macOS and Linux packaging/signing.
- [ ] Complete automated tests and hardware validation matrix.
- [ ] Security, privacy, licensing and dependency review.
- [ ] Publish supported model/firmware combinations, limitations and recovery instructions.
- [ ] User acceptance test against the supplied Swatak workflows.

## Evidence labels

- **Implemented:** code exists and automated tests pass.
- **Protocol-tested:** validated against a simulator or protocol fixture.
- **Hardware-tested:** validated against the exact documented physical model/firmware.
- **Vendor-certified:** only if the vendor explicitly certifies it.

Do not mark a whole brand as supported based on one model. Do not mark a workflow complete because its button exists.

- [x] Extract ONVIF network-interface response parsing into a bounded, testable parser with coverage for interface metadata, manual/DHCP address precedence, gateway and malformed/oversized XML.
