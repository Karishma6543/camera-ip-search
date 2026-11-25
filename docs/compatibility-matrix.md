# Camera compatibility and hardware test matrix

This file is the source of truth for tested model-level compatibility. Do not mark a vendor or model as supported without evidence.

## Multi-brand ONVIF support

The application uses standards-based ONVIF WS-Discovery and Device-service requests. A CP Plus, Prama, Hikvision or other manufacturer's camera can be discovered and queried when its model/firmware exposes compatible ONVIF services and the network permits discovery traffic. Manufacturer names are taken from the camera's ONVIF GetDeviceInformation response; they are not inferred from an IP address or port fingerprint.

| Operation | Current implementation | Compatibility qualification |
|---|---|---|
| Discovery | ONVIF WS-Discovery | Depends on ONVIF being enabled and multicast being permitted |
| Manufacturer/model/firmware/serial | ONVIF GetDeviceInformation | Fields depend on the device response |
| Device capabilities | ONVIF GetCapabilities | Service categories are reported; not every operation is guaranteed |
| Read network interfaces | ONVIF GetNetworkInterfaces | Interface token and IPv4 match required |
| Static IPv4 and gateway | ONVIF SetNetworkInterfaces / SetNetworkDefaultGateway | Requires authenticated permission and verified interface; model/firmware validation still required |
| DHCP configuration | Not implemented in this release | Not advertised as supported |
| DNS configuration | Read via ONVIF GetDNS; write not implemented | Not advertised as supported |
| Reboot, time and user management | Not implemented in this release | Not advertised as supported |
| Vendor-specific APIs | Adapter extension point exists; no CP Plus/Prama/Hikvision proprietary adapter is currently validated | Add only against official SDK/protocol documentation and physical model tests |
| Factory reset | Request code exists, but UI action is disabled pending model/firmware support verification | ONVIF discovery alone is not sufficient proof |

No physical CP Plus, Prama or Hikvision model/firmware combination is certified by CI. The compatibility matrix must be updated only after testing exact hardware and firmware versions.

## Status legend

- Not assessed
- Researching
- Implemented
- Protocol-tested
- Hardware-tested
- Unsupported / blocked

## Current matrix

| Manufacturer / family | Model | Hardware revision | Firmware | Discovery | Device info | Network config | Activation | Firmware upgrade | Evidence / notes |
|---|---|---|---|---|---|---|---|---|---|
| Project camera | TBD | TBD | Contract B implementation TBD | Not assessed | Not assessed | Not assessed | Not assessed | Not assessed | Own UDP 9988 protocol; see protocol contract |
| CP Plus | TBD | TBD | TBD | Not assessed | Not assessed | Not assessed | Not assessed | Not assessed | Determine ONVIF support and official vendor options per model |
| Hikvision | TBD | TBD | TBD | Not assessed | Not assessed | Not assessed | Not assessed | Not assessed | Determine ONVIF support and official SDK/licensing per model |
| Prama | TBD | TBD | TBD | Not assessed | Not assessed | Not assessed | Not assessed | Not assessed | Determine ONVIF support and official vendor options per model |
| Other ONVIF camera | TBD | TBD | TBD | Not assessed | Not assessed | Not assessed | Not assessed | Not assessed | Generic interoperability target |
| Other / non-ONVIF | TBD | TBD | TBD | Not assessed | Not assessed | Not assessed | Not assessed | Not assessed | Validate candidate scanning and unknown-device behavior |

## How to add a test result

For each tested device, record:
- Exact model and hardware revision from the physical label or authenticated device information.
- Firmware version and region where relevant.
- Connection topology (same subnet, multiple interfaces, VLAN, routed subnet).
- Discovery provider and timestamp.
- Each operation attempted, its result and any prerequisites.
- Tool build/commit and adapter version.
- Sanitized logs or packet fixtures, with secrets removed.

A successful ONVIF probe does not prove activation, network configuration or firmware upgrade support. A successful port probe does not prove that the endpoint is a camera.

## In-progress protocol foundations (not user-facing support)

The development branch `feat/onvif-operations-foundation` adds validated SOAP request builders and unit tests for ONVIF DHCP enable/disable, manual/DHCP DNS selection, and SystemReboot. These builders are not yet connected to the desktop operation workflow, do not establish model-level support, and must not be exposed as working UI actions until authenticated request execution, capability checks, response verification, error handling, and hardware tests are complete. Date/time and user-management operations remain unimplemented.
