# New-camera Ethernet onboarding and initialization plan

Status: engineering plan opened 2026-11-28. This document is the acceptance checklist for the end-to-end workflow. A checked software item means implemented and tested in code; it does not mean a camera model is hardware-certified.

## Product outcome

A user connects a supported IP camera to the PC by Ethernet, opens Oxita Device Manager, and can discover it, identify whether it is uninitialized, initialize it through a documented and authorized mechanism when supported, configure its network, reconnect to its new address, and verify the result. The workflow must also explain clearly when a device is unreachable, non-ONVIF, already initialized, unsupported, or blocked by permissions.

The application must not claim universal activation support merely because a camera is discoverable. ONVIF standardizes device discovery and selected management operations; it does not standardize every manufacturer's factory activation workflow.


## Browser-first universal initialization experience

The user-facing contract is browser-like: selecting a discovered camera and pressing **Initialize** must take the user into that camera's own setup experience inside Oxita, without requiring the user to choose a manufacturer or model.

- [x] Add a single **Initialize / open camera setup** action to the selected-device action panel. (Live-row shortcut remains pending.)
- [ ] Resolve the camera's observed address and available web endpoints; try only safe, bounded HTTP/HTTPS endpoint checks and do not guess vendor CGI/API paths.
- [ ] Prefer HTTPS when the device advertises a usable HTTPS web endpoint; explain certificate warnings and never silently weaken TLS validation for protocol/API traffic.
- [x] Embed the camera's web UI in an in-app browser surface with navigation, back/forward, reload, loading/error states and a visible device address/origin.
- [ ] Keep the user inside the Oxita workflow; do not make the normal Initialize action silently launch an unrelated external browser.
- [x] Handle HTTP authentication challenges with session-only prompts; never persist credentials or log passwords. (Camera HTML login forms remain the camera's own UI.)
- [x] Restrict top-level navigation to HTTP/HTTPS on the selected device host; block cross-host navigation and pop-up windows.
- [ ] Detect and explain browser limitations such as obsolete ActiveX/NPAPI plugins, unsupported TLS/ciphers, self-signed certificates, mixed content and vendor pages requiring legacy browser components; never claim these pages are supported merely because they load in a browser.
- [ ] Do not inject scripts, bypass login, defeat certificate checks, or automate password creation through undocumented page internals.
- [ ] Provide a guided fallback when no web UI is reachable: standard protocol adapter, authorized vendor adapter, or a clear unsupported/manual recovery state.
- [ ] Complete **Verify camera**: the current flow starts fresh discovery and attempts a strong identity match using prior MAC, serial number or ONVIF endpoint evidence. Explicit reachability checks, network/device read-back and activation-state verification remain pending. (Current first slice starts fresh discovery; explicit identity/read-back verification is pending.)
- [ ] Detect address changes after setup and guide rediscovery/reconnection without assuming the previous IP remains valid.
- [ ] Keep browser-page success separate from verified device initialization; only mark Initialized/Ready after supported evidence confirms it.
- [x] Select Qt WebEngine as the embedded browser implementation; add the module to build and release CI. (Runtime packaging/security/license/binary-size validation remains pending.)
- [ ] Add browser navigation/auth/redirect/error tests plus simulated camera web pages for initialized, activation-required, login-required and unsupported cases.
- [ ] Add physical tests against multiple camera web interfaces, including CP PLUS and Prama, and record model/firmware/browser compatibility separately.

**Important boundary:** a browser-like in-app setup page is the universal user experience, not a universal vendor activation protocol. A camera's own web page remains responsible for its proprietary activation behavior. Some legacy camera pages may depend on browser plugins or TLS behavior that a modern embedded browser cannot support; Oxita must disclose that limitation rather than claim universal compatibility.

## Workstream A — Ethernet and interface readiness

- [x] Detect adapter link/address changes with a non-blocking 3-second Qt interface inventory refresh. OS event-driven notifications remain a future optimization.
- [ ] Show interface name, adapter description, link state, IPv4/IPv6 addresses, prefix, gateway, DHCP state and metric. Current selected-interface summary shows name, hardware address, link state, IPv4/prefix and broadcast; gateway, DHCP and route metric remain pending.
- [ ] Distinguish physical Ethernet from Wi-Fi, VPN, loopback, virtual adapters and disconnected interfaces.
- [ ] Detect when a newly connected camera causes a new neighbor/ARP observation or subnet change.
- [x] Refresh interface inventory on link/address changes via polling; manual refresh remains available through the existing interface refresh workflow.
- [ ] Handle multiple NICs, multiple IPv4 addresses and USB Ethernet adapters. Multiple-address display exists; route metric handling and hardware validation remain pending.
- [ ] Never assume the PC and camera share a subnet merely because an Ethernet cable is connected.
- [x] Show disabled/link-down/link-up-without-IPv4 states and explain cable, power/PoE and DHCP/link-local checks. Full guided addressing remains pending.
- [ ] Detect likely self-assigned/link-local and common factory subnet situations without guessing camera addresses.
- [ ] Provide safe guided instructions when PC and camera subnets do not match.
- [ ] Do not silently change the PC's IP address, route, firewall or adapter settings.
- [ ] Add Windows, macOS and Linux interface/link integration tests and hardware validation.

## Workstream B — Discovery coverage

- [ ] Run ONVIF WS-Discovery on selected/all eligible interfaces with correct interface binding.
- [ ] Add directed IPv4 broadcast discovery where protocol documentation supports it.
- [ ] Add passive neighbor-table/ARP observation using supported OS APIs; label MAC-only results as candidates.
- [ ] Add bounded, cancellable TCP candidate scanning with preview, rate limits and clear scope.
- [ ] Add vendor-specific discovery only from authorized official protocol/SDK documentation.
- [ ] Keep the Oxita Contract B probe isolated to Oxita devices; never send it to unrelated vendors.
- [ ] Record source interface, source protocol, observed address, MAC, timestamps and provenance for every observation.
- [ ] Merge duplicate observations while preserving all sources and last-seen evidence.
- [ ] Distinguish Live Scan Results from Saved Inventory; a fresh scan must never show stale records as live.
- [ ] Handle multicast blocked by host firewall, switch, VLAN, Wi-Fi isolation or camera configuration.
- [ ] Add repeated-scan, missing-device, duplicate-packet, malformed-packet, timeout, cancellation and multi-NIC tests.
- [ ] Validate broadcast/multicast on Windows, macOS and Linux; test VLAN and routed network limitations.

## Workstream C — Device identity and onboarding classification

- [ ] Add explicit states: Unknown, Candidate, Reachable, Identified, AlreadyInitialized, InitializationRequired, InitializationInProgress, Initialized, ConfigurationRequired, Configuring, VerificationPending, Ready, Unsupported and Failed.
- [ ] Track evidence and confidence separately from the displayed state.
- [ ] Identify manufacturer/model/hardware/firmware from authenticated or protocol-defined responses only.
- [ ] Detect ONVIF endpoint, profiles/services and per-operation evidence.
- [ ] Distinguish camera, NVR, encoder, doorbell and unknown network endpoint without port-only false positives.
- [ ] Detect "activation required" only through a documented response/SDK result; do not infer it from blank credentials or open ports.
- [ ] Distinguish unauthenticated, authentication rejected, authorization denied and unsupported operation.
- [ ] Show a clear reason and next safe action for every state.
- [ ] Preserve exact device identity across address changes using verified MAC/serial/endpoint evidence; avoid unsafe IP-only rebinding.
- [ ] Add unit tests for every state transition, invalid transition and evidence downgrade.

## Workstream D — Initialization / activation engine

- [ ] Define a vendor-neutral initialization request/result contract with typed outcomes and structured diagnostics.
- [ ] Define adapter capability declarations for activation detection, activation, password policy, initial network setup and post-activation verification.
- [ ] Implement an ONVIF generic path only for operations actually standardized and supported; never label ONVIF discovery as activation.
- [x] Research CP PLUS public IP Finder catalogue/manual evidence: official IP Finder V4.0 exists and is described for LAN discovery/configuration. Authorized protocol/SDK documentation and exact model/family contract remain pending.
- [ ] Confirm CP PLUS SDK/API licensing, redistribution and supported OS constraints before integration.
- [ ] Implement CP PLUS adapter only after an authorized protocol contract and simulator fixture exist.
- [x] Research Prama public local-discovery evidence: official Prama Online Pro describes same-network device search. A documented desktop activation API/SDK and exact model/family contract remain pending.
- [ ] Implement Prama adapter only after an authorized protocol contract and simulator fixture exist.
- [ ] Keep vendor adapters isolated; no guessed CGI paths, undocumented ports, default passwords or reverse-engineered authentication bypass.
- [ ] Support user-created initial password only when the device's documented protocol supports it.
- [ ] Enforce device-reported password policy; validate locally and explain policy failures without logging secrets.
- [ ] Require explicit user confirmation before activation/password creation.
- [ ] Never ship or try default credentials; never brute-force or bypass activation.
- [ ] Handle duplicate activation requests, timeouts, lost responses and safe retry/idempotency rules.
- [ ] Verify activation by a documented follow-up query or authenticated session before marking complete.
- [ ] Add protocol simulator fixtures and negative tests for unsupported, unauthorized, expired and malformed responses.

## Workstream E — First network configuration

- [ ] Provide guided DHCP and static IPv4 setup after activation or for already initialized cameras.
- [ ] Read current interface/address/prefix/gateway/DNS before proposing changes.
- [ ] Offer safe suggested addressing based on the selected PC interface; never choose an unverified address.
- [ ] Validate IPv4 address, subnet mask/prefix, gateway reachability and DNS before sending writes.
- [ ] Probe for IP conflicts before applying a static address.
- [ ] Require explicit confirmation with before/after values and target device identity.
- [ ] Bind each write to the selected camera's verified identity and adapter.
- [ ] Verify each write by read-back; distinguish accepted, applied, verified and failed.
- [ ] Handle address change, reboot-required, transport loss, timeout and partial success.
- [ ] Re-discover the camera at the new address and verify identity before updating the record.
- [ ] Provide guided recovery when the camera disappears or the new address is unreachable.
- [ ] Never modify the PC network configuration automatically.
- [ ] Add tests for conflict, wrong subnet, wrong gateway, duplicate IP, DHCP address change, partial failure and reconnect.

## Workstream F — Security, consent and audit

- [ ] Keep credentials in memory only until an OS credential vault is implemented and explicitly opted into.
- [ ] Implement Windows Credential Manager, macOS Keychain and Linux Secret Service adapters with secure fallback.
- [ ] Redact passwords, tokens, WS-Security nonces, authorization headers and sensitive packet fields from logs/exports.
- [ ] Require confirmation for activation, network writes, reboot, reset and firmware operations.
- [ ] Show exact target identity, source interface, requested change and expected connectivity impact.
- [ ] Prevent cross-device credential/operation mix-ups and re-check identity immediately before writes.
- [ ] Record audit events for request, response, verification, reconnect and partial failure without secrets.
- [ ] Validate TLS certificate behavior and secure transport on all supported operating systems.
- [ ] Add threat-model review for spoofed discovery replies, hostile endpoints, redirects, XML/parser abuse and LAN scanning boundaries.

## Workstream G — UI/UX

- [ ] Add a guided "Add camera" / "Initialize camera" workflow accessible from Live Scan Results.
- [ ] Show interface selection and connection readiness before scanning.
- [ ] Show separate states for initializing, already initialized, authentication required and unsupported.
- [ ] Show actionable reasons instead of generic "not found" errors.
- [ ] Provide a clear stepper: Connect → Discover → Identify → Initialize → Configure IP → Verify → Ready.
- [ ] Keep advanced network settings available without overwhelming the default flow.
- [ ] Show progress, timeout, cancellation and per-device outcomes.
- [ ] Provide a recovery panel with last known address, selected NIC and safe reconnect instructions.
- [ ] Ensure keyboard navigation, screen-reader labels, high-DPI layouts and localization-ready strings.
- [ ] Add UI acceptance tests for every state, dialog, validation error, cancellation and recovery branch.

## Workstream H — Compatibility and release evidence

- [ ] Build a model-level test matrix for CP PLUS, Prama, generic ONVIF and non-ONVIF devices.
- [ ] Record exact model, hardware revision, firmware, account role, protocol/adapter version and tested operations.
- [ ] Test at least one CP PLUS model with ONVIF and one exact model requiring an authorized proprietary initialization path if available.
- [ ] Test at least one Prama model with ONVIF and any authorized proprietary path if available.
- [ ] Test generic ONVIF camera and non-ONVIF device to prove honest unsupported behavior.
- [ ] Add simulator tests for every documented vendor protocol before physical testing.
- [ ] Run Windows, macOS and Linux build/test/sanitizer/security workflows.
- [ ] Test installer install/upgrade/uninstall and signed release artifacts.
- [ ] Publish supported-model evidence and known limitations; never claim "all cameras" from brand-level assumptions.

## Current implementation baseline (2026-11-28)

- [x] Existing ONVIF WS-Discovery and GetDeviceInformation / capabilities / services / network reads.
- [x] Existing bounded TCP candidate scanner and SSDP discovery.
- [x] Existing interface selection and per-interface discovery socket architecture.
- [x] Existing ONVIF static IPv4, DHCP, DNS and reboot request paths with varying levels of read-back verification.
- [x] Existing capability/adapter registry and explicit write-operation gating.
- [x] Existing IP conflict and assignment preflight helpers.
- [x] Existing live-scan vs saved-inventory separation (PR #36 work; merge/CI status tracked separately).
- [ ] Dedicated Ethernet link-state and hot-plug detection.
- [ ] Explicit onboarding state model and transition tests.
- [ ] Generic activation contract and UI workflow.
- [ ] Authorized CP PLUS/Prama activation adapters.
- [ ] Hardware validation on exact models.

## Implementation order

1. Ethernet/interface readiness and a typed onboarding state model.
2. Discovery observation/provenance and safe candidate classification.
3. Generic initialization contract and adapter capability gates.
4. ONVIF first-network configuration and reconnect/recovery hardening.
5. Authorized CP PLUS and Prama adapters after protocol evidence is available.
6. Guided UI workflow, audit/security hardening and full acceptance coverage.
7. Physical model matrix and release sign-off.

## Definition of done

The feature is complete only when a supported physical camera can be connected over Ethernet, discovered on the correct NIC, identified, initialized through a documented authorized adapter when required, configured, re-discovered at its resulting address, and verified. Unsupported devices must fail safely with a clear explanation. CI success alone is not hardware certification.
