# Pre-production simulator and integration test guide

## Scope and safety

The local simulator implements the documented Contract B discovery exchange (UDP 9988) and a deliberately limited ONVIF WS-Discovery/device-service fixture. It is a deterministic test peer, not a camera emulator certification suite. It does not implement CP Plus, Hikvision, Prama or undocumented Oxita commands.

State-changing ONVIF requests are rejected by the simulator. Never use the simulator as evidence that static-IP changes, activation, reboot or firmware update work on physical devices.

## Requirements

- Python 3.10 or newer (standard library only).
- The desktop application built from the same working branch.
- A dedicated isolated test network is recommended.
- Firewall permission for UDP 3702, UDP 9988 and the selected HTTP test port.

## Start the simulator

Use the IPv4 address assigned to the test PC's selected Ethernet/Wi-Fi adapter. The advertised address must match the sender address observed by the application.

Windows PowerShell:

```powershell
py -3 tools/camera_simulator.py --interface-ip 192.168.10.20 --advertised-ip 192.168.10.20
```

Linux/macOS:

```bash
python3 tools/camera_simulator.py --interface-ip 192.168.10.20 --advertised-ip 192.168.10.20
```

The simulator listens on UDP 9988 for Contract B, joins ONVIF multicast 239.255.255.250:3702 on the requested interface and serves the ONVIF device endpoint at:

`http://192.168.10.20:18080/onvif/device_service`

Use the same interface in the desktop application and start discovery. The simulator must be on the same local network; multicast/broadcast discovery is not routed.

## Deterministic negative scenarios

Pass one of the following values with `--scenario`:

| Scenario | Expected test purpose |
|---|---|
| `normal` | Valid Contract B and ONVIF fixture |
| `silent` | No ONVIF response; timeout behavior |
| `malformed-json` | Truncated Contract B JSON |
| `wrong-sequence` | Stale/mismatched request sequence |
| `missing-mac` | Missing optional identity field |
| `invalid-port` | Port range validation |
| `advertised-mismatch` | Advertised address differs from UDP source; must be rejected |
| `dhcp` | DHCP field handling |
| `unsafe-xaddr` | XAddr with credentials/query; must be rejected |
| `multiple-xaddrs` | First unsafe/wrong-host XAddr followed by a same-host endpoint |
| `auth-required` | HTTP Basic challenge path |
| `soap-fault` | SOAP fault/error handling |
| `oversized` | 2 MiB SOAP body; client must abort after the 1 MiB response budget |

Example:

```bash
python3 tools/camera_simulator.py --interface-ip 192.168.10.20 --advertised-ip 192.168.10.20 --scenario unsafe-xaddr
```

Some scenarios intentionally produce a malformed or rejected response. A missing device in those scenarios can be the expected result; inspect the application logs and test case rather than treating all negative fixtures as simulator failures.

## Automated fixture tests

```bash
python -m unittest discover -s tests -p "test_*.py" -v
```

These tests validate simulator response construction and negative fixtures. They do not replace the C++ parser tests or an end-to-end application test. The cross-platform workflow runs them before CMake/Qt.

## Manual acceptance matrix

For each scenario, record:
- OS, application commit, simulator command and selected interface.
- Expected result and actual result.
- Discovery latency and whether the UI remained responsive.
- Whether cancellation stopped outstanding work.
- Sanitized logs (never include credentials, authorization headers or tokens).

Do not mark a physical camera model as supported based on simulator results.

## C++ loopback wire integration tests

The CTest target `discovery-wire-integration-tests` exercises actual Qt UDP and TCP sockets on the loopback interface:

- A Contract B request/response datagram round trip, including response sequence validation and source-address parsing.
- An ONVIF Probe/ProbeMatch datagram round trip and same-host device-service XAddr parsing.
- A TCP connection to a local listening service, matching the basic connectability primitive used by subnet scanning.

These tests require no camera, external network, multicast route or privileged port. They validate the socket transport and parser boundary deterministically.

Additional C++ wire cases now attempt real Qt multicast delivery to the ONVIF group on loopback and directed-broadcast delivery through an available non-loopback interface. They explicitly skip when the host OS/container disables those delivery modes. A skipped multicast/broadcast case is not a pass and must be followed by the physical-interface packet-capture procedure below.

**Scope limitation:** this is not yet a full `DiscoveryService` integration test. It does not exercise OS multicast group delivery, broadcast delivery on multiple physical NICs, the service's interface binding/selection, in-flight ONVIF HTTP enrichment, cancellation races or real camera behavior. Those remain separate CI/manual and hardware acceptance gates. A passing loopback test must not be described as proof of end-to-end camera discovery.

## ONVIF response resource limit

The desktop client now monitors ONVIF SOAP replies, including enrichment and static-IPv4 configuration stages, as data arrives and aborts a response when more than 1 MiB is buffered. This prevents an untrusted camera from making the client buffer an arbitrarily large SOAP response before checking its size. A local HTTP peer regression test sends a 2 MiB SOAP body and expects the service to report the 1 MiB rejection.

The existing post-read size checks remain as defense in depth. This guard is not a substitute for testing fragmented delivery, slow-drip responses, TLS behavior and all supported Qt/OS combinations.


## Physical-interface discovery acceptance

The Qt service lifecycle regression test now starts discovery on an available non-loopback IPv4 interface, confirms the service enters its active state after socket setup/probe submission, and cancels it. This validates the service's interface-selection and lifecycle path on CI hosts that expose a usable NIC; it does not prove a camera received either datagram.

For a physical acceptance run, use a dedicated isolated Ethernet segment with one test camera and one application host. Capture traffic on the selected adapter and verify:
- ONVIF Probe leaves the selected NIC for 239.255.255.250:3702.
- Contract B probe leaves the selected NIC as IPv4 limited broadcast to UDP 9988.
- Replies arrive on the same selected NIC and are associated with the actual UDP sender address.
- A second active NIC does not receive or contribute replies when a specific interface is selected.
- Duplicate replies are merged without duplicate inventory rows.
- Cancellation closes discovery sockets and stops subsequent UI updates.
- No response after the configured timeout produces a clean zero/partial result, not a stuck scanning state.

Record the OS, adapter name, IPv4/prefix, camera model/firmware, packet capture and sanitized application diagnostics. Multicast/broadcast are link-local; repeat this test on every supported network topology.

## Static IPv4 hardware acceptance — mandatory before enabling a model

Use a camera on an isolated bench network, with a documented recovery path and a second way to reach the device (vendor reset procedure or direct local access). Never test first on a production camera or a remote camera.

For each exact model, hardware revision and firmware:
1. Record the current DHCP/static mode, address, subnet mask, gateway and DNS.
2. Confirm the app discovered the same camera interface address and that the ONVIF interface token belongs to that interface.
3. Preview and validate a new address/mask/gateway in the same isolated subnet. Verify invalid network/broadcast addresses and mismatched gateway subnets are rejected.
4. Apply a reserved, unused static address. Capture each ONVIF SetNetworkInterfaces response and subsequent GetNetworkInterfaces verification.
5. Reconnect using the new address and verify mask, gateway and DNS independently; do not treat an HTTP success response alone as proof.
6. Repeat with a deliberately unreachable gateway or rejected write and verify the UI reports partial failure without claiming success.
7. Power-cycle the camera and confirm the setting persists.
8. Test a multi-interface camera and verify no other interface is modified.
9. Record authentication mode, operator role, firmware response, elapsed time and sanitized logs.

The application does not automatically roll back a verified IP change. If a test fails, use the camera's documented local recovery process. A model remains unsupported until the full matrix passes on its actual hardware.

## ONVIF HTTP negative-path fixture coverage

The Python fixture suite also exercises the simulator's HTTP service, not just its response builders:
- Basic-auth challenge and the authorized retry path.
- SOAP fault response handling.
- Explicit rejection of state-changing network requests (the simulator never claims a write succeeded).
- Unknown device-service paths.
- Oversized request-body rejection (413).
- Oversized response-body generation for the desktop client's response-size guard.

These tests verify that the simulator presents the intended deterministic fixtures. They do not prove the desktop client handles each case correctly unless paired with a corresponding C++ service-level test, and they do not establish physical-camera interoperability.
