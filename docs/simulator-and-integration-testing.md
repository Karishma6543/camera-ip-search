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
| `unconfigured` | Explicit unconfigured-camera fallback fixture |
| `dhcp` | DHCP field handling |
| `unsafe-xaddr` | XAddr with credentials/query; must be rejected |
| `multiple-xaddrs` | First unsafe/wrong-host XAddr followed by a same-host endpoint |
| `auth-required` | HTTP Basic challenge path |
| `soap-fault` | SOAP fault/error handling |

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
