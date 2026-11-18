# Camera communication contract

This document tracks the protocol boundary between camera firmware and the desktop utility.

## Proprietary discovery

- Transport: UDP
- Port: 9988
- Broadcast/multicast behavior: as defined by the firmware contract
- Protocol version: 1

The engineering specification currently defines this probe request:

```json
{
  "magic": "CAM_DISCOVER",
  "version": 1,
  "seq": 1001,
  "cmd": "PROBE_REQUEST"
}
```

The sequence value must be generated per request by the implementation; the value above is illustrative.

## ONVIF distinction

ONVIF WS-Discovery uses UDP port 3702. It is a separate discovery mechanism and must not be conflated with the proprietary port-9988 protocol.

## Items to freeze before implementing the codec

- Exact response JSON/binary schema and required fields
- MAC address representation and normalization
- Activation request, response and password policy
- Network configuration request/response schema
- Batch operation semantics and per-device acknowledgement
- Firmware upgrade request, transfer protocol, integrity metadata and rollback behavior
- Authentication, replay protection and error codes

Until these items are confirmed against the firmware implementation, the application must not invent packet formats or send activation/configuration commands. A mock transport may be used for UI development.
