# Camera communication contracts

This document separates the project's own firmware protocol from third-party camera interoperability.

## 1. Project proprietary discovery (Contract B)

**Between:** Project camera firmware and Camera IP Search  
**Transport:** UDP broadcast/multicast, destination port 9988  
**Broadcast address in the engineering manual:** `255.255.255.255`  
**Protocol version:** 1

Request:

```json
{
  "magic": "CAM_DISCOVER",
  "version": 1,
  "seq": 1001,
  "cmd": "PROBE_REQUEST"
}
```

The sequence value is illustrative and must be generated per request.

Response:

```json
{
  "magic": "CAM_DISCOVER",
  "version": 1,
  "seq": 1001,
  "cmd": "PROBE_RESPONSE",
  "device": {
    "mac": "AA:BB:CC:DD:EE:01",
    "ipv4": "192.168.1.253",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1",
    "dns": "8.8.8.8",
    "dhcp": false,
    "http_port": 80,
    "rtsp_port": 554,
    "soc": "NT98566",
    "sensor": "SC401AI",
    "firmware_version": "v1.0.0-rc1",
    "activated": false
  }
}
```

The response fields above are copied from the project's engineering manual (Contract B). The codec must validate magic, version, sequence, command and field types/ranges. Do not trust a response solely because it arrived on UDP 9988.

### Still undefined in the source contract

The engineering manual defines the discovery request/response but does not define wire formats for activation, network changes, batch acknowledgements or firmware-upgrade commands. Those commands must not be invented. Freeze and version those contracts with the firmware team before implementation.

## 2. ONVIF WS-Discovery

ONVIF WS-Discovery is a standards-based third-party interoperability path and is separate from Contract B:
- Multicast address: `239.255.255.250`
- UDP port: `3702`
- SOAP/XML Probe and ProbeMatch messages.

The implementation must follow the applicable WS-Discovery/ONVIF specifications and handle multiple responses, endpoint references, scopes and XAddrs. ONVIF discovery does not imply every ONVIF service or management operation is supported.

## 3. Vendor-specific protocols

CP Plus, Hikvision, Prama and other manufacturers may provide proprietary tools, SDKs or protocols for some models. No vendor packet schema is assumed by this document. Implementations require exact model/firmware evidence, official documentation or authorized SDK use, licensing review and hardware validation.

Do not send a proprietary vendor command to a device based only on a guessed brand fingerprint.

## 4. Network scanning

Fallback scanning is not a wire contract for camera management. It is a user-initiated candidate-finding feature. It must be bounded to selected local interfaces/CIDR ranges, rate-limited, cancellable and non-intrusive. Open ports are not proof of camera identity.

## 5. Security requirements

- Never use default credentials or credential guessing.
- Authenticate before management operations.
- Validate and bound all received data.
- Do not log credentials, tokens or sensitive payloads.
- Require explicit confirmation for high-impact operations.
- Use per-device result reporting for batch operations.
