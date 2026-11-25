# Network device discovery and classification

## Discovery sources

The application combines three local-link discovery mechanisms:

- ONVIF WS-Discovery probes for ONVIF Network Video Transmitters (commonly cameras and some recorders).
- Oxita Contract B UDP discovery on port 9988 for devices implementing the documented project discovery contract.
- SSDP/UPnP M-SEARCH on 239.255.255.250:1900 for devices that advertise UPnP services, including Internet Gateway Devices and some printers.

The selected interface is used for the probes. Choosing “All active IPv4 interfaces” sends discovery probes across each eligible local interface; subnet scanning still requires one specific interface. These multicast/broadcast protocols are link-local by design and do not cross routers or VLAN boundaries. A host that suppresses discovery responses may still be found by the manual TCP subnet scan.

## Subnet scan

The opt-in IPv4 TCP scan checks common management and device service ports:

`22, 23, 53, 80, 135, 139, 443, 445, 515, 554, 631, 1900, 2869, 3389, 5000, 5357, 7547, 8000, 8080, 8443, 8899, 9100, 34567`.

A successful connection creates or enriches an **unverified candidate**. It is not proof of a particular device type, and the scanner does not authenticate to services. It normally scans up to 256 host addresses. For subnets containing 257–4096 hosts, the UI offers a full scan or a 256-address window around the selected interface address. Subnets larger than 4096 hosts remain windowed to avoid unexpectedly huge scans. The scanner is TCP-only; UDP-only services may not be found by this fallback.

## Classification and confidence

The inventory classifies devices using explicit protocol identity, model/vendor descriptors and conservative service fingerprints:

| Type | Evidence |
|---|---|
| Camera | ONVIF or Oxita discovery identity (verified); RTSP plus web service ports (probable) |
| NVR | Explicit NVR/DVR/XVR/video-recorder model or identity descriptor |
| Router | UPnP Internet Gateway Device identity; explicit router/gateway model; or DNS plus management-port fingerprint (probable) |
| Printer | UPnP Printer identity; explicit printer model; or multiple printing service ports (probable) |
| Computer | Explicit workstation/PC model; or file-sharing/remote-desktop service fingerprint (probable) |
| Switch | Explicit switch model/vendor descriptor |
| Unknown | Insufficient evidence; the application does not infer a type from one generic open port |

Confidence labels are `Verified`, `High`, `Probable` and `Unverified`. Port-based classification is a hint for filtering and operator triage, not a vendor-certified identification. Switches and devices that do not expose recognizable services may remain Unknown. There is not yet an integrated MAC-vendor database, SNMP inventory, mDNS inventory, LLDP topology discovery or vendor-specific identification SDK.

## Filters

Device type, manufacturer/discovery source, reachability/activation status and free-text search combine as AND filters. The type column displays the classification; its tooltip shows confidence and the evidence used. Unknown remains a first-class filter so unclassified hosts are not hidden.

## Safety and limitations

- SSDP LOCATION values are parsed only as untrusted metadata and are never fetched by discovery.
- Discovery responses do not establish that the device is safe, authenticated or manageable.
- Manual inventory entry is not identity verification.
- Static IPv4 UI preflight rejects addresses already used by another inventory device, reported gateway or local interface. It does not perform an active ARP duplicate-address probe before every write.
- Reachability is based on TCP checks; a failed check is not definitive proof that a host is offline.
- Cross-VLAN discovery requires a routed discovery design or scanning each authorized VLAN from a host attached to that VLAN.
- Do not claim that the application finds every device on every network. Firewalls, ACLs, VLANs, disabled services, host isolation and unsupported protocols can hide devices.
