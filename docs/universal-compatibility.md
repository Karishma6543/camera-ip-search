# Universal camera compatibility strategy

**Status:** Architecture baseline (2026-11-18)  
**Audience:** Developers, reviewers, QA, field installers and future contributors.

## 1. Product objective

Camera IP Search is intended to be a cross-platform LAN discovery and provisioning utility for IP cameras from multiple manufacturers. The target includes CP Plus, Hikvision, Prama, the project's own camera firmware, ONVIF-compatible devices and other vendors.

Compatibility is capability-based, not a blanket promise that every operation works on every model. Camera vendors and even models within one brand may use different discovery, authentication, activation, configuration and firmware mechanisms.

The application must always distinguish:
- **Discovered:** a network endpoint responded to a supported probe.
- **Identified:** the application obtained a manufacturer/model identity with a stated source and confidence.
- **ONVIF-capable:** ONVIF services were positively detected.
- **Manageable:** one or more operations are supported by a verified adapter.
- **Unknown:** a device was found but its identity or management protocol is not known.

Never infer that a device is a camera solely because a common port is open.

## 2. Compatibility layers

The product uses multiple complementary discovery paths. They feed one normalized inventory model.

| Layer | Purpose | Scope / limitation |
|---|---|---|
| ONVIF WS-Discovery | Standards-based camera/service discovery | Multicast discovery; device must implement and enable ONVIF |
| Vendor adapters | Proprietary discovery and management for a specific vendor/model family | Only implement from verified public documentation, licensed SDKs or authorized protocol captures |
| Project firmware adapter | Discovery and management for our own camera firmware | Uses the project's frozen Contract B and later confirmed command contracts |
| Safe network scanner | Finds additional candidate endpoints that discovery probes miss | Scans only user-authorized interfaces/ranges; results are candidates, not confirmed cameras |
| Manual add | Allows an operator to enter a known address and optional credentials | Does not claim automatic identification |

Discovery paths must be independently switchable and report their source. Duplicate observations are merged into one device record using MAC address when trustworthy, otherwise stable service identifiers and endpoint data. Conflicts are retained as observations rather than silently overwritten.

## 3. Standards-first approach

### ONVIF

ONVIF WS-Discovery is a separate mechanism from the project's proprietary UDP discovery:
- WS-Discovery multicast address: `239.255.255.250`
- UDP port: `3702`
- Probe/ProbeMatch messages use the ONVIF/WS-Discovery SOAP XML model.
- Discovery should extract endpoint reference, scopes, XAddrs and advertised types where available.
- Follow-up device information and capability calls require the appropriate ONVIF service endpoint and user authentication.

ONVIF discovery does not guarantee that all configuration operations are supported. Before showing an operation as available, query device/service capabilities and handle unsupported actions cleanly.

### Project firmware protocol

The project's own camera firmware uses UDP port `9988`, not ONVIF port `3702`. Its request/response details are tracked in [protocol-contract.md](protocol-contract.md). The tool must not send the project-specific probe to third-party cameras or assume they understand it.

### Vendor protocols

Vendor integrations are separate adapters. CP Plus, Hikvision and Prama are priority validation targets, not proof that every model in those product families shares one protocol. An adapter may support discovery only at first; activation, network changes and firmware upgrades remain disabled until the exact model family, firmware range, authentication and command behavior are verified.

Do not reverse engineer access controls, bypass authentication, or distribute proprietary SDKs without authorization. Prefer official vendor documentation and SDK licensing terms.

## 4. Safe network scanning

The fallback scanner is opt-in and bounded:
- Enumerate active network interfaces and their IPv4 prefixes.
- Let the user choose interfaces and/or explicit CIDR ranges.
- Default to the directly connected local subnet; do not scan the Internet or arbitrary remote ranges.
- Use configurable, conservative concurrency, rate limits, timeouts and cancellation.
- Probe only a documented list of likely service ports and protocols; a port response is only a candidate signal.
- Do not attempt passwords, exploit services, or perform intrusive fingerprinting.
- Show the scan range and estimated number of addresses before a large scan.
- Respect local firewall/network restrictions and report when multicast/broadcast is blocked.

Limited broadcast `255.255.255.255` does not traverse routers. Routed-subnet discovery requires a per-interface directed broadcast/multicast strategy or a separately deployed relay. The UI must not promise subnet-independent discovery until tested in the intended network topology.

## 5. Normalized device record

The inventory should support partially known devices. Suggested fields:

- Stable internal ID
- MAC address (optional, normalized when available)
- IPv4 address and interface/source
- Manufacturer, model, hardware revision (optional)
- Firmware version (optional)
- ONVIF endpoint and advertised profiles/capabilities (optional)
- HTTP/HTTPS and RTSP endpoints/ports (optional)
- Discovery sources and last-seen timestamp
- Activation state (unknown / activated / not activated)
- Authentication state (not tested / required / authenticated / rejected)
- Supported operations and the adapter that provides each operation
- Per-field provenance and confidence where useful

Unknown values stay unknown; do not fill them with guesses.

## 6. Capability model

Every adapter reports capabilities explicitly, for example:
- discover
- read_device_information
- read_network_configuration
- set_network_configuration
- activate
- reboot
- firmware_upgrade
- open_web_ui
- get_stream_uris

The UI derives enabled actions from the capability set and the current authentication/authorization state. An operation unsupported by a device must be visibly unavailable with a reason.

## 7. Compatibility targets and validation

The initial validation matrix should include:
- CP Plus: at least one ONVIF-enabled model and one model using any verified proprietary integration.
- Hikvision: at least one ONVIF-enabled model; investigate official discovery SDK availability and licensing before vendor-specific work.
- Prama: at least one ONVIF-enabled model; proprietary behavior requires model-specific evidence.
- Project camera: the firmware build implementing Contract B.
- Generic ONVIF camera from another manufacturer.
- A non-ONVIF or ONVIF-disabled device to validate fallback scanning and honest unknown-device handling.

Record exact model, hardware revision, firmware version, network topology, adapter version and tested operations for every result. A brand-level pass must not be inferred from one model.

## 8. Security and privacy

- Never ship default passwords or attempt credential guessing.
- Do not persist credentials in plaintext. Use OS credential storage where available; otherwise ask for credentials per session and document limitations.
- Redact passwords, tokens, authorization headers and sensitive packet fields from logs.
- Require explicit confirmation for activation, network changes, reboot, batch actions and firmware upgrades.
- Firmware updates require model/hardware compatibility checks, integrity verification and a recoverability plan.
- Bind operations to the selected device identity and re-check the target before high-impact actions.
- Keep scanning local, bounded and user initiated.

## 9. Non-goals and limitations

- Universal support for every camera model cannot be guaranteed.
- Port scanning cannot reliably identify every camera.
- ONVIF does not standardize every vendor feature, activation workflow or firmware update.
- Cloud/P2P discovery outside the local network is outside this tool's initial scope.
- NVRs, doorbells and encoders may appear as compatible ONVIF/network devices, but product classification may remain generic.

## 10. Definition of done for universal discovery

The discovery milestone is complete only when:
1. ONVIF WS-Discovery works on supported Windows, macOS and Linux builds.
2. The project's Contract B probe works against the matching project firmware.
3. The fallback scanner is opt-in, bounded, cancellable and clearly labels candidates.
4. Multiple discovery paths merge duplicate observations without losing provenance.
5. Unknown/non-camera endpoints are not falsely labeled as confirmed cameras.
6. Tests cover malformed packets, timeouts, duplicate replies, cancellation and multiple interfaces.
7. The compatibility matrix records actual hardware tests and known limitations.
