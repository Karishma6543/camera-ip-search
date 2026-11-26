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
| DHCP configuration | ONVIF SetNetworkInterfaces request, then GetNetworkInterfaces read-back | Requires interface token, camera authorization and model/firmware support; address changes may require rediscovery |
| DNS configuration | ONVIF SetDNS request, then GetDNS read-back | Manual primary IPv4 DNS is compared; automatic DNS is accepted when the camera returns a valid IPv4 DNS address |
| Reboot | ONVIF SystemReboot request with response validation | Camera may disconnect; successful response means accepted, not proof that reboot completed |\n| Time and user management | Not implemented | Not advertised as supported |
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

The development branch `feat/onvif-camera-network-configuration` connects ONVIF DHCP, DNS and reboot requests to the desktop workflow, with response validation, read-back checks for DHCP/DNS, confirmation dialogs, and adapter-level operation declarations. These are protocol-level implementations, not model-level certification. A camera can reject an operation because of firmware, account permissions or incomplete ONVIF support. No CP Plus or Prama physical camera has been hardware-tested by CI. DHCP address changes and reboot may require rediscovery. Date/time and user-management operations remain unimplemented.
