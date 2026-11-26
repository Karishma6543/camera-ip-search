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
- [x] Stable device identity matching across MAC, ONVIF endpoint, serial/manufacturer and guarded IP fallback.
- [x] Require protocol evidence before promoting a TCP-only candidate.
- [x] Add conservative device-type classification and confidence labels for Camera, NVR, Router, Switch, Printer, Computer and Unknown.
- [x] Add combined device-type, manufacturer/discovery-source, reachability/activation and text filters; expose classification in the inventory.
- [ ] Separate application coordinator and normalized observation/error models.
- [ ] Move all persistence out of the UI layer into a testable repository.
- [x] Add rotating application logs with best-effort secret redaction and regression tests.
- [x] Add user-confirmed diagnostic export with OS/application/interface details, recent operation history and recent logs.
- [ ] Complete broader secret-redaction review and validate support-bundle privacy on each OS.
- [ ] Move all potentially blocking work off the GUI thread and prove responsiveness under load.

## Milestone 2 — Discovery
- [x] ONVIF WS-Discovery probe and ProbeMatch parser.
- [x] Project UDP 9988 Contract B discovery codec.
- [x] Opt-in IPv4 TCP candidate scanner across common camera, management, printer and computer ports; user can choose full scans up to 4096 hosts or a 256-address window on larger subnets.
- [x] Allow ONVIF, Oxita and SSDP discovery across all active IPv4 interfaces or one selected interface.
- [x] SSDP/UPnP M-SEARCH discovery with bounded response parsing; LOCATION URLs are not fetched.
- [x] Basic duplicate merging and discovery provenance.
- [x] Add unit tests for invalid interfaces, IPv4 mask/prefix mismatch and unusable /31-/32 scan ranges.
- [x] Add cancellation for in-flight ONVIF enrichment and reject stale replies after cancellation/restart.
- [ ] Add end-to-end integration tests for socket lifecycle, cancellation, multiple interfaces, duplicate responses and timeouts. SSDP response parsing has focused unit tests; multicast delivery still requires physical-network validation.
- [x] Add a local Contract B/ONVIF simulator and deterministic Python fixture tests; full C++ socket integration remains pending.
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
- [x] Run SQLite quick_check before inventory schema changes; corrupted databases are left untouched and reported.
- [ ] Expand UI tests for device selection, filters, import/export, persistence and empty/error states.
- [x] Add a desktop startup smoke test for the Operation History workspace.
- [ ] Validate inventory behavior with large device lists and repeated discovery.

See [vendor protocol research](vendor-protocol-research.md) for model-specific public evidence and the adapter boundaries.

## Milestone 4 — Vendor integrations
- [ ] Research official SDK/protocol options for CP Plus.
- [ ] Research official SDK/protocol options for Hikvision.
- [ ] Research official SDK/protocol options for Prama.
- [ ] Implement adapters only for exact models/firmware with authorized documentation and licensing.
- [ ] Add model-level hardware evidence to the compatibility matrix.

## Milestone 5 — Safe provisioning
- [x] Implement guarded ONVIF static IPv4 request with local validation, confirmation and post-change verification path.
- [x] Gate ONVIF network writes through the adapter's implemented-operation list.
- [x] Persist a verified new IP and report partial failure when a subsequent gateway update fails.
- [ ] Add guided recovery/reconnect instructions for partial network-configuration failures.
- [ ] Verify ONVIF write behavior, authentication and recovery on supported physical models.
- [ ] Add robust partial-failure/recovery handling when IP changes succeed but gateway or verification fails.
- [ ] Define and version the project's activation/network-configuration contract before implementation.
- [x] Add local operation history for discovery, scans, reachability, manual inventory changes, exports and static-IPv4 outcomes.
- [ ] Expand audit coverage to every request/response stage and partial failure.
- [ ] Add batch preview/confirmation only after single-device flows are validated.

## Milestone 6 — Firmware management
- [ ] Define vendor firmware package metadata and compatibility rules.
- [ ] Verify package integrity and model/hardware/region compatibility.
- [ ] Implement only vendor-authorized upgrade transports and recovery behavior.
- [ ] Add progress, cancellation policy and per-device results.

## Milestone 7 — Production release
- [ ] Fix and pass CI on Windows, macOS and Linux.
- [ ] Complete unit, integration, UI and network simulation coverage (simulator fixtures and a startup smoke test now exist; full socket/service/UI coverage remains).
- [ ] Complete model/firmware hardware validation matrix.
- [ ] Package installers and portable builds; test install, upgrade and uninstall.
- [ ] Sign/notarize releases where applicable and define update distribution.
- [ ] Complete security, privacy, dependency/license and support review using [security-and-privacy-review.md](security-and-privacy-review.md).
- [ ] Publish supported models, known limitations, troubleshooting and release notes.

## Evidence levels

- **Implemented:** code exists and automated tests pass.
- **Protocol-tested:** validated against a simulator or protocol fixture.
- **Hardware-tested:** validated against the exact documented physical model/firmware.
- **Vendor-certified:** only if the vendor explicitly certifies it.

Do not mark a whole brand as supported based on one model.


## Compatibility reporting

- [x] Report ONVIF service categories returned by GetServices, with same-host endpoint validation.
- [x] Show a per-device operation compatibility matrix separating application implementation, observed device responses and advertised services.
- [x] Add loopback ONVIF HTTP/SOAP integration tests for HTTP Basic challenge/retry, GetServices request/response, SOAP faults and malformed service XML.
- [x] Implement ONVIF WS-Security UsernameToken PasswordDigest with fresh nonce/Created values, in-memory session credentials and challenge/retry.
- [x] Add deterministic digest unit tests and loopback SOAP challenge/retry integration coverage.
- [ ] Validate WS-Security interoperability and clock/nonce behavior on representative physical camera models.
- [ ] Expand integration tests for timeouts, redirects, malformed responses and read-back mismatches.
- [ ] Validate representative CP Plus and Prama camera models and firmware versions on physical hardware.
- [ ] Add vendor-specific adapters only where protocol behavior is documented and testable.
