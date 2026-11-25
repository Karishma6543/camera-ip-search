# User guide: screens, buttons and options

This guide describes the controls currently wired into the desktop UI source. Labels may move slightly as the interface evolves. A button being visible does not mean every camera supports the corresponding operation.

## 1. First scan

1. Open the application.
2. In the network-interface selector, choose **All active IPv4 interfaces** to probe every active IPv4 adapter, or select one adapter such as Ethernet or Wi-Fi. Subnet scanning uses the selected interface.
3. Click **Discover cameras**. This sends ONVIF WS-Discovery and the supported Oxita Contract B discovery probe. During a scan, the same button becomes **Cancel discovery**.
4. Wait for the results in **Network inventory**. Discovery is local-link IPv4; it does not automatically cross routers or VLANs.
5. If devices may not answer discovery, click **Scan subnet**. Select the scan scope in its dialog, review the host count and start the scan. It checks a defined set of TCP ports and adds unverified candidates. During the scan, the button becomes **Cancel subnet scan**.

Use discovery first. Subnet scanning can take longer and a TCP response does not prove that a host is a camera.

## 2. Top bar and inventory filters

| Control | What it does / how to use it |
|---|---|
| **Help** | Opens in-app guidance about discovery, subnet scans, device information and risky operations. |
| **Settings** | Shows the local application-data folder, SQLite inventory path, database connection state and selected scan interface. **Copy location** copies the folder path; **Open data folder** opens it in the OS file manager. These are local-storage settings, not camera-network settings. |
| **Search devices by IP, name, MAC or model** | Filters the visible inventory by matching device fields. Use the clear icon to remove the search. |
| **Network interface selector** | Chooses which active IPv4 interface discovery uses. **All active IPv4 interfaces** is available for discovery; choose a specific interface for subnet scanning. |
| **All device types / Cameras / NVRs / Routers / Switches / Printers / Computers / Unknown** | Filters by the app's evidence-based classification. **Unknown** is useful when investigating devices that were found but not confidently classified. |
| **All manufacturers / ONVIF devices / Oxita devices / Other / unknown** | Filters by manufacturer/discovery source. This is not a claim of vendor certification. |
| **All statuses / Online / Offline / Activated / Not activated** | Filters the stored status fields. Reachability is based on TCP checks; activation can remain Unknown when the device does not report it. |
| **Add device** | Manually adds a device record to the local inventory. It does not test or verify that the IP belongs to a camera. |
| **Export CSV** | Exports the inventory fields to a UTF-8 CSV file. Review the file before sharing it. |
| **Rename** | Sets a friendly local label for the selected device; it does not rename the camera itself. |
| **Check reachability** | Checks checked rows first; if none are checked, checks highlighted/selected rows; if neither exists, checks the full inventory. While running, the button becomes **Cancel reachability check**. |
| **Scan subnet** | Starts an opt-in TCP candidate scan on the selected IPv4 interface. Candidate results are not confirmed cameras. |
| **Discover cameras** | Starts protocol discovery using ONVIF and supported Oxita discovery. While running, it becomes **Cancel discovery**. |

## 3. Inventory table options

- **Checkbox in the first column:** explicitly marks devices for reachability checks and bulk static-IPv4 configuration.
- **Select all:** checks every row for the next multi-device operation. It does not select every row for editing.
- **Clear:** unchecks all device checkboxes.
- **Click a row:** selects one device and updates the lower device workspace.
- **Click a column heading:** sorts the inventory by that column; click again to reverse sort order.
- **Right-click a column heading:** opens a column-visibility menu. Tick/untick fields such as MAC, firmware, discovery source, serial number and hardware ID to show or hide them. Hidden columns remain searchable and are included in CSV export.
- **Device type tooltip:** hover over a type to see its confidence and the evidence used. A probable classification is not a verified identity.

## 4. Lower device workspace

The top workspace navigation has three tabs: **Device Info**, **Configuration** and **Operation History**. Selecting a tab updates the right-hand inspector while keeping the camera inventory visible. Configure is further divided into **Device**, **Network** and **Storage**, so camera access/maintenance, IP provisioning and local inventory recovery do not compete in one long panel.

### Device details

Select a row in the inventory to see the device-reported profile, identity, discovery evidence, services, network configuration and capabilities. Empty values usually mean the camera did not report that field during the latest query; they do not automatically mean a fault.

### Configure → Device / Network / Storage

| Action / panel | Purpose and safe usage |
|---|---|
| **Refresh device info** | Queries the selected device's reachable ONVIF device service for information such as manufacturer, model, firmware, serial number, capabilities and network interfaces. Select a device first. A compatible endpoint and any required authentication are necessary. |
| **Open web interface** | Opens the selected device's HTTP web page in the computer's default browser. The camera may use HTTPS or a non-standard port; availability depends on the device. |
| **Factory reset camera** | Sends ONVIF SetSystemFactoryDefault only when a verified ONVIF service is available and the camera authorizes it. **Soft** restores user-configuration defaults; **Hard** requests factory defaults. This can erase settings/users, reboot the camera or change its IP. Use only when you have physical/network recovery access. |
| **Preview IP configuration** | Validates a proposed IPv4 address, subnet mask, optional gateway and DNS locally. It sends no network request and does not change the camera. This is the recommended first step before an IP change. |
| **Apply static IPv4 (ONVIF)** | Sends an ONVIF static IPv4 configuration request for the selected camera. Requires refreshed ONVIF interface details and permissions. It validates values and checks known local/inventory reservations, then asks for confirmation. The camera can disconnect immediately. There is no automatic rollback; DNS is not changed by this single-device action. |
| **Configure checked cameras** | Sequential bulk static-IPv4 configuration. Check at least two cameras in the first inventory column (or select at least two rows), refresh every camera first, enter a unique new address/mask/gateway for each, review the list and confirm. Each camera is changed and verified one at a time. A partial failure can leave earlier cameras changed; no guaranteed rollback exists. |
| **Firmware information** | Displays firmware details reported by the selected camera. Firmware installation/update is not implemented in this release. |
| **Back up inventory** | Creates a consistent SQLite backup and validates its integrity. Save the backup somewhere safe before major changes. |
| **Restore inventory** | Replaces the current local inventory with a selected validated backup after confirmation. The application retains a separate pre-restore recovery snapshot beside the active database. This restores the app's inventory, not camera firmware or camera settings. |

### Operation history

- **Refresh history:** reloads the local activity list.
- **Export diagnostics:** creates a user-confirmed text diagnostic bundle with OS/application/interface summary, recent operation history and redacted logs. It does not include the inventory database or ONVIF interface tokens. Review the export before sharing it because it still contains system/network metadata.

## 5. Recommended safe operator workflow

For ordinary discovery:
1. Choose the correct interface.
2. Click **Discover cameras**.
3. Use **Scan subnet** only if you need additional TCP candidates.
4. Select a device and click **Refresh device info**.
5. Review the details and confidence evidence.
6. Run **Check reachability** when you need a current TCP reachability check.
7. Export CSV or back up the inventory if needed.

Before changing a camera IP:
1. Confirm the exact camera row and current address.
2. Click **Refresh device info** and confirm the ONVIF network interface details are present.
3. Use **Preview IP configuration** first.
4. Check that your computer can reach the proposed subnet and that the address is not already in use. The app's inventory checks are not a universal live-network reservation.
5. Apply only with a recovery plan and access to the camera's new subnet.
6. Re-discover or refresh after the change and verify the reported address.

Before factory reset:
1. Confirm the selected camera model/IP.
2. Back up or record its current configuration separately.
3. Ensure you can physically access it or recover its network address.
4. Choose Soft or Hard only after understanding the difference.
5. Confirm the final warning. A reset may make the camera unreachable.

## 6. Status and terminology

- **Discovered:** a supported protocol response was received.
- **Candidate / unverified:** a TCP port responded during subnet scanning; identity is not proven.
- **Reachable / Unreachable:** TCP reachability check result, not a complete health check.
- **Verified / High / Probable / Unverified:** confidence in the classification based on the available evidence, not vendor certification.
- **Activated / Not activated / Unknown:** activation state only when a source provides enough evidence; do not infer it from reachability.
- **All active IPv4 interfaces:** discovery attempts on each eligible active adapter. It does not mean every VLAN or routed subnet is scanned.
- **Bulk:** applies the same operation type to multiple selected devices, with per-device values. Current bulk provisioning is limited to static IPv4 assignments.

## 7. Not available in this release

The following are not currently exposed as working camera operations: camera activation, DHCP toggle, DNS configuration, firmware upload/update/recovery, vendor-specific reboot, CP Plus/Hikvision/Prama proprietary adapters, SNMP/LLDP/mDNS discovery and routed/VLAN topology discovery. Do not interpret a panel label or device-reported field as proof that the related write operation exists.
