# Adapter development guide

**Status:** Contributor guide  
**Related:** [Universal compatibility strategy](universal-compatibility.md), [Architecture](architecture.md), [Protocol contract](protocol-contract.md).

## 1. Adapter responsibilities

An adapter translates one discovery or management protocol into the application's normalized device and capability model. It must not own UI widgets or directly write presentation state.

Expected conceptual boundaries:

- **Discovery provider:** performs a bounded probe and emits device observations.
- **Device adapter:** enriches a known device and exposes verified capabilities/actions.
- **Transport:** handles UDP/TCP/HTTP with deadlines, cancellation and bounded payload sizes.
- **Normalizer:** converts protocol-specific fields into common types while retaining source/provenance.
- **Coordinator:** runs enabled providers, merges observations and publishes results.

The exact C++ interfaces will be introduced incrementally; this guide defines behavior, not a claim that all interfaces already exist.

## 2. Required behavior

Every adapter must:
- Declare protocol name, version and supported platform constraints.
- Declare which operations it supports and any model/firmware restrictions.
- Validate all inbound data (size, encoding, required fields, numeric ranges and endpoint addresses).
- Use explicit timeouts, bounded retries and cancellation.
- Avoid blocking the UI thread.
- Return structured errors; do not silently swallow failures.
- Never log credentials or sensitive payloads.
- Avoid sending state-changing commands during discovery.
- Include unit tests with valid, malformed, truncated, oversized and unexpected responses.
- Document hardware/models and firmware versions actually tested.

## 3. Vendor integration process

For each new vendor/model family:

1. Identify exact model, hardware revision and firmware version.
2. Check official vendor documentation and SDK availability/licensing.
3. Determine whether ONVIF already supplies the required discovery and management features.
4. Implement the smallest useful capability set first (usually discovery and device information).
5. Test on isolated lab equipment and capture expected request/response behavior.
6. Add negative tests and record unsupported operations.
7. Enable provisioning actions only after authentication, authorization, error handling and recovery are verified.
8. Update the compatibility matrix and release notes.

Do not assume CP Plus, Hikvision or Prama share one protocol across all product families. Do not copy undocumented packet formats from unrelated models.

## 4. Error and capability semantics

Use explicit outcomes such as:
- unsupported
- authentication_required
- authentication_failed
- timeout
- malformed_response
- network_unreachable
- device_busy
- firmware_incompatible
- operation_cancelled

A timeout is not evidence that a device is unsupported. Preserve the distinction between “not discovered”, “discovered but not identified”, “unsupported operation” and “authentication required”.

## 5. Test strategy

- Unit tests: codecs, normalization, identity merging, capability calculation.
- Protocol tests: deterministic local UDP/TCP fixtures and malformed input cases.
- Integration tests: ONVIF simulator and project firmware simulator.
- Hardware tests: physical cameras across the supported vendor/model matrix.
- UI tests: progress, cancellation, partial batch failure, unknown devices and unsupported actions.
- Platform tests: Windows, macOS and Linux network-interface behavior and firewall permissions.

Tests must not require real cameras for the normal CI pipeline. Hardware tests are a separate, explicitly labeled validation job.
