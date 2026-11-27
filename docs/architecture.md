# Architecture

## Product goal

Oxita Camera Device Manager is a cross-platform desktop utility for discovering IP cameras, inspecting supported device information and performing only explicitly verified operations. Compatibility is capability-driven: discovery or an open TCP port does not prove that a device supports a management action.

See:
- [Universal compatibility strategy](universal-compatibility.md)
- [Adapter development guide](adapter-development.md)
- [Implementation roadmap](implementation-roadmap.md)
- [Production-readiness checklist](production-readiness.md)
- [Compatibility and hardware test matrix](compatibility-matrix.md)
- [Firmware communication contract](protocol-contract.md)
- [Simulator and integration testing](simulator-and-integration-testing.md)

## Technology

- C++20 and Qt 6 Widgets for the desktop UI
- Qt Network for UDP discovery, TCP reachability and ONVIF HTTP/SOAP requests
- Qt SQL with SQLite for local, non-sensitive inventory and operation history
- Qt Concurrent for bounded asynchronous application work
- CMake and Qt Test
- Python 3.12+ for deterministic simulator fixture tests

## Current source layout

~~~text
src/
  adapters/       DeviceAdapter contract, registry, ONVIF adapter and IPv4 validation
  app/            Batch-operation and IP-assignment preflight/runner foundations
  core/           Device identity, diagnostics and logging
  discovery/      ONVIF/project discovery, enrichment, subnet scan, reachability,
                  IP conflict probing, assignment planning and protocol parsers
  firmware/       Firmware package metadata and integrity candidate validation
  ui/             Qt Widgets desktop application and Oxita theme
tests/            Qt unit, protocol, safety and desktop smoke tests
tools/            Local Contract B / limited ONVIF simulator
docs/             Architecture, security, compatibility and release guidance
~~~

The camera-core static library compiles shared implementation once and is linked by the desktop executable and focused test targets. The UI, application coordination and some persistence logic still share responsibilities; a dedicated repository/service layer and a more explicit application coordinator remain future refactoring work.

## Runtime boundaries

- **UI:** presents discovery, inventory, details, operation history and user-confirmed actions. It must not invent device capabilities.
- **Application safety layer:** validates target identity, duplicate targets, address ranges and operation capability before any batch action can run.
- **Adapters:** identify devices and declare verified protocol operations. Unsupported write operations default to disabled.
- **Discovery providers and transports:** perform bounded UDP/TCP/HTTP exchanges with timeouts, cancellation and response-size limits.
- **Normalization:** converts protocol-specific observations into a common device record while retaining source/provenance.
- **Persistence:** stores non-sensitive inventory and bounded operation history. Credentials and ONVIF interface tokens are not included in diagnostic exports.
- **Firmware validation:** checks package metadata and integrity candidates; it does not verify a vendor signature or perform firmware flashing.

## Discovery strategy

1. ONVIF WS-Discovery (UDP 3702 multicast) for standards-based devices.
2. Project firmware discovery (UDP 9988) strictly for the project's own documented Contract B firmware.
3. Vendor-specific providers only when official protocol/SDK behavior is verified and licensing permits use.
4. Bounded, opt-in local IPv4 TCP candidate scanning for devices that do not respond to discovery.

Discovery is local-link IPv4 only; multicast and limited broadcast do not cross routers. Candidate scanning is bounded and does not prove a candidate is a camera or ONVIF device.

## Operation safety

- Adapter capability and authorization checks precede management actions.
- IP assignment uses planning and conflict preflight. Unknown conflict results block writes.
- Static IPv4 changes require validation and explicit user confirmation, then post-change verification.
- A successful new-address check followed by a failed gateway stage is reported as a partial outcome with recovery guidance; automatic rollback is not guaranteed.
- Batch operations require stable device identity, duplicate detection, explicit target results and preflight before execution.
- Firmware candidate checks are not signature verification. Firmware flashing remains unavailable.
- No undocumented vendor commands are guessed. ONVIF reboot is implemented with response validation; activation/password changes, firmware updates, stream preview/snapshot, imaging/PTZ, event management and NVR operations remain unavailable unless a verified adapter implements them. Factory reset request code is present but the UI control remains disabled pending hardware validation.

## Reliability and security

- Bound network response sizes and use explicit timeouts/cancellation.
- Treat malformed, stale, oversized and ambiguous responses as failures or Unknown.
- Do not log credentials, tokens or raw sensitive SOAP payloads.
- SQLite integrity is checked before schema migration; migrations are transactional and versioned.
- Simulator and loopback tests supplement, but do not replace, physical-network and hardware acceptance.

## Implementation status

The repository includes the pre-production discovery, inventory, diagnostics, safety-preflight and automated test foundations described above. It is not production-certified. Exact-model camera interoperability, physical-interface multicast/broadcast, credential security, TLS/WS-Security, backup/restore and interrupted/disk-full recovery, release packaging and hardware-specific state-changing operations still require validation or implementation.
