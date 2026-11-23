# Implementation roadmap

Checkboxes distinguish code present from validation evidence. A checked implementation item is not proof of successful hardware interoperability.

## Milestone 0 — Architecture and safety baseline
- [x] Define capability-based compatibility strategy.
- [x] Separate ONVIF, project firmware, vendor and fallback discovery paths.
- [x] Define contributor and hardware-evidence requirements.
- [x] Record security boundaries and unsupported states.

## Milestone 1 — Application foundations
- [x] Qt desktop shell and active IPv4 interface enumeration/selection.
- [x] Basic device record and adapter registry.
- [ ] Separate application coordinator and normalized observation/error models.
- [ ] Move all persistence out of the UI layer into a testable repository.
- [ ] Add structured, redacted logging and diagnostic export.
- [ ] Move all potentially blocking work off the GUI thread and prove responsiveness under load.

## Milestone 2 — Discovery
- [x] ONVIF WS-Discovery probe and ProbeMatch parser.
- [x] Project UDP 9988 Contract B discovery codec.
- [x] Bounded opt-in IPv4 TCP candidate scanner (maximum 256 host addresses).
- [x] Basic duplicate merging and discovery provenance.
- [x] Add unit tests for invalid interfaces, IPv4 mask/prefix mismatch and unusable /31-/32 scan ranges.
- [ ] Add integration tests for socket lifecycle, cancellation, multiple interfaces, duplicate responses and timeouts.
- [ ] Verify broadcast/multicast behavior on Windows, macOS and Linux.
- [ ] Test VLAN, multiple NIC, Wi-Fi/Ethernet, firewall and routed-network limitations.

## Milestone 3 — Device information and inventory
- [x] ONVIF GetDeviceInformation request/response handling.
- [x] ONVIF GetCapabilities, GetNetworkInterfaces and GetDNS request/response handling.
- [x] Select the ONVIF network interface matching the discovered camera IPv4 address before exposing its token for configuration; cover multi-interface selection in parser tests.
- [x] Validate advertised ONVIF endpoints against the responding host and reject embedded credentials/query strings.
- [x] Bind project discovery addresses to the UDP source except for explicitly unconfigured 0.0.0.0 responses.
- [x] Local SQLite inventory, manual device entry, labels and CSV export.
- [ ] Add schema-versioned migrations and database recovery tests.
- [ ] Add UI tests for device selection, filters, import/export, persistence and empty/error states.
- [ ] Validate inventory behavior with large device lists and repeated discovery.

## Milestone 4 — Vendor integrations
- [ ] Research official SDK/protocol options for CP Plus.
- [ ] Research official SDK/protocol options for Hikvision.
- [ ] Research official SDK/protocol options for Prama.
- [ ] Implement adapters only for exact models/firmware with authorized documentation and licensing.
- [ ] Add model-level hardware evidence to the compatibility matrix.

## Milestone 5 — Safe provisioning
- [x] Implement guarded ONVIF static IPv4 request with local validation, confirmation and post-change verification path.
- [x] Gate ONVIF network writes through the adapter's implemented-operation list.
- [ ] Verify ONVIF write behavior, authentication and recovery on supported physical models.
- [ ] Add robust partial-failure/recovery handling when IP changes succeed but gateway or verification fails.
- [ ] Define and version the project's activation/network-configuration contract before implementation.
- [ ] Add operation audit history and per-device outcomes.
- [ ] Add batch preview/confirmation only after single-device flows are validated.

## Milestone 6 — Firmware management
- [ ] Define vendor firmware package metadata and compatibility rules.
- [ ] Verify package integrity and model/hardware/region compatibility.
- [ ] Implement only vendor-authorized upgrade transports and recovery behavior.
- [ ] Add progress, cancellation policy and per-device results.

## Milestone 7 — Production release
- [ ] Fix and pass CI on Windows, macOS and Linux.
- [ ] Add unit, integration, UI and network simulation coverage.
- [ ] Complete model/firmware hardware validation matrix.
- [ ] Package installers and portable builds; test install, upgrade and uninstall.
- [ ] Sign/notarize releases where applicable and define update distribution.
- [ ] Complete security, privacy, dependency/license and support review.
- [ ] Publish supported models, known limitations, troubleshooting and release notes.

## Evidence levels

- **Implemented:** code exists and automated tests pass.
- **Protocol-tested:** validated against a simulator or protocol fixture.
- **Hardware-tested:** validated against the exact documented physical model/firmware.
- **Vendor-certified:** only if the vendor explicitly certifies it.

Do not mark a whole brand as supported based on one model.
