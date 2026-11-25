# Camera compatibility and hardware test matrix

This file is the source of truth for tested model-level compatibility. Do not mark a vendor or model as supported without evidence.

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
