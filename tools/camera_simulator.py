#!/usr/bin/env python3
"""Deterministic local camera simulator for Oxita Camera Device Manager.

This simulator implements only the documented Contract B discovery exchange and
a deliberately small ONVIF device-service subset. It is for isolated test labs;
it does not emulate vendor-specific APIs or certify physical-camera behavior.
"""
from __future__ import annotations

import argparse
import json
import logging
import signal
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

LOG = logging.getLogger("oxita-camera-simulator")
MAX_DATAGRAM = 65507
ONVIF_GROUP = "239.255.255.250"
ONVIF_PORT = 3702
PROJECT_PORT = 9988

class Scenario:
    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.http_port = args.http_port
        self.stop = threading.Event()
        self.http: ThreadingHTTPServer | None = None
        self.udp_threads: list[threading.Thread] = []

    def project_payload(self, request: dict) -> bytes | None:
        if request.get("magic") != "CAM_DISCOVER" or request.get("version") != 1:
            return None
        if request.get("cmd") != "PROBE_REQUEST":
            return None
        seq = request.get("seq")
        if not isinstance(seq, int) or isinstance(seq, bool) or seq < 0:
            return None
        if self.args.scenario == "malformed-json":
            return b'{"magic":"CAM_DISCOVER","version":1,'
        if self.args.scenario == "wrong-sequence":
            seq += 1
        ip = self.args.advertised_ip
        if self.args.scenario == "advertised-mismatch":
            ip = "192.0.2.253"
        device = {
            "mac": self.args.mac,
            "ipv4": ip,
            "netmask": "255.255.255.0",
            "gateway": "192.0.2.1",
            "dns": "1.1.1.1",
            "dhcp": self.args.scenario == "dhcp",
            "http_port": self.http_port,
            "rtsp_port": 554,
            "soc": "SIMULATED-SOC",
            "sensor": "SIMULATED-SENSOR",
            "firmware_version": "sim-1.0.0",
            "activated": True,
        }
        if self.args.scenario == "missing-mac":
            device.pop("mac")
        if self.args.scenario == "invalid-port":
            device["http_port"] = 70000
        return json.dumps({
            "magic": "CAM_DISCOVER", "version": 1, "seq": seq,
            "cmd": "PROBE_RESPONSE", "device": device,
        }, separators=(",", ":")).encode()

    def start_project_udp(self) -> None:
        def worker() -> None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                sock.bind((self.args.bind, PROJECT_PORT))
                sock.settimeout(0.5)
                LOG.info("Contract B simulator listening on %s:%d", self.args.bind, PROJECT_PORT)
                while not self.stop.is_set():
                    try:
                        packet, peer = sock.recvfrom(MAX_DATAGRAM)
                    except socket.timeout:
                        continue
                    if len(packet) > MAX_DATAGRAM:
                        continue
                    try:
                        request = json.loads(packet.decode("utf-8"))
                    except (UnicodeDecodeError, json.JSONDecodeError):
                        continue
                    response = self.project_payload(request)
                    if response is not None:
                        try:
                            sock.sendto(response, peer)
                            LOG.info("Contract B response sent to %s:%d", peer[0], peer[1])
                        except OSError as exc:
                            LOG.warning("UDP response failed: %s", exc)
            except OSError as exc:
                LOG.error("Cannot bind Contract B UDP port %d: %s", PROJECT_PORT, exc)
            finally:
                sock.close()
        thread = threading.Thread(target=worker, name="contract-b-udp", daemon=True)
        thread.start()
        self.udp_threads.append(thread)

    def start_onvif_udp(self) -> None:
        def worker() -> None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            try:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                sock.bind(("", ONVIF_PORT))
                group = socket.inet_aton(ONVIF_GROUP) + socket.inet_aton(self.args.interface_ip)
                sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, group)
                sock.settimeout(0.5)
                LOG.info("ONVIF WS-Discovery simulator joined %s on %s", ONVIF_GROUP, self.args.interface_ip)
                while not self.stop.is_set():
                    try:
                        packet, peer = sock.recvfrom(MAX_DATAGRAM)
                    except socket.timeout:
                        continue
                    if b"Probe" not in packet or b"NetworkVideoTransmitter" not in packet:
                        continue
                    if self.args.scenario == "silent":
                        continue
                    response = self.probe_match(peer[0])
                    try:
                        sock.sendto(response, peer)
                    except OSError as exc:
                        LOG.warning("ONVIF response failed: %s", exc)
            except OSError as exc:
                LOG.error("ONVIF multicast listener unavailable: %s", exc)
            finally:
                sock.close()
        thread = threading.Thread(target=worker, name="onvif-udp", daemon=True)
        thread.start()
        self.udp_threads.append(thread)

    def probe_match(self, sender_ip: str) -> bytes:
        address = "urn:uuid:simulated-oxita-camera-0001"
        xaddr = f"http://{self.args.advertised_ip}:{self.http_port}/onvif/device_service"
        if self.args.scenario == "unsafe-xaddr":
            xaddr = f"http://user:pass@{sender_ip}:{self.http_port}/onvif/device_service?token=bad"
        if self.args.scenario == "multiple-xaddrs":
            xaddrs = f"http://198.51.100.7/onvif/device_service {xaddr}"
        else:
            xaddrs = xaddr
        return f'''<?xml version="1.0" encoding="UTF-8"?>
<e:Envelope xmlns:e="http://www.w3.org/2003/05/soap-envelope"
 xmlns:w="http://schemas.xmlsoap.org/ws/2004/08/addressing"
 xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery">
 <e:Header><w:MessageID>urn:uuid:simulated-response</w:MessageID></e:Header>
 <e:Body><d:ProbeMatches><d:ProbeMatch>
  <w:EndpointReference><w:Address>{address}</w:Address></w:EndpointReference>
  <d:Types>dn:NetworkVideoTransmitter</d:Types>
  <d:Scopes>onvif://www.onvif.org/name/Oxita%20Simulator onvif://www.onvif.org/manufacturer/Oxita onvif://www.onvif.org/hardware/Simulated</d:Scopes>
  <d:XAddrs>{xaddrs}</d:XAddrs><d:MetadataVersion>1</d:MetadataVersion>
 </d:ProbeMatch></d:ProbeMatches></e:Body></e:Envelope>'''.encode()

    def start_http(self) -> None:
        scenario = self
        class Handler(BaseHTTPRequestHandler):
            server_version = "OxitaCameraSimulator/1.0"
            def log_message(self, fmt: str, *args: object) -> None:
                LOG.info("HTTP %s - %s", self.client_address[0], fmt % args)
            def do_POST(self) -> None:
                if self.path != "/onvif/device_service":
                    self.send_error(404)
                    return
                length = int(self.headers.get("Content-Length", "0"))
                if length < 0 or length > 1024 * 1024:
                    self.send_error(413)
                    return
                body = self.rfile.read(length)
                if scenario.args.scenario == "auth-required" and self.headers.get("Authorization") is None:
                    self.send_response(401)
                    self.send_header("WWW-Authenticate", 'Basic realm="Oxita Simulator"')
                    self.end_headers()
                    return
                if scenario.args.scenario == "soap-fault":
                    payload = b'''<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body><s:Fault><s:Reason><s:Text>Simulated device fault</s:Text></s:Reason></s:Fault></s:Body></s:Envelope>'''
                elif b"GetDeviceInformation" in body:
                    payload = b'''<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><s:Body><tds:GetDeviceInformationResponse><tds:Manufacturer>Oxita</tds:Manufacturer><tds:Model>Simulator</tds:Model><tds:FirmwareVersion>sim-1.0.0</tds:FirmwareVersion><tds:SerialNumber>SIM-0001</tds:SerialNumber><tds:HardwareId>SIM-HW-1</tds:HardwareId></tds:GetDeviceInformationResponse></s:Body></s:Envelope>'''
                elif b"GetCapabilities" in body:
                    payload = b'''<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body><GetCapabilitiesResponse/></s:Body></s:Envelope>'''
                elif b"GetNetworkInterfaces" in body:
                    payload = f'''<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema"><s:Body><tds:GetNetworkInterfacesResponse><tds:NetworkInterfaces token="eth0"><tt:Enabled>true</tt:Enabled><tt:Info><tt:Name>eth0</tt:Name></tt:Info><tt:IPv4><tt:Enabled>true</tt:Enabled><tt:Config><tt:Manual><tt:Address>{scenario.args.advertised_ip}</tt:Address><tt:PrefixLength>24</tt:PrefixLength></tt:Manual><tt:DHCP><tt:Enabled>false</tt:Enabled></tt:DHCP></tt:Config></tt:IPv4></tds:NetworkInterfaces></tds:GetNetworkInterfacesResponse></s:Body></s:Envelope>'''.encode()
                elif b"GetDNS" in body:
                    payload = b'''<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body><GetDNSResponse/></s:Body></s:Envelope>'''
                elif b"SetNetworkInterfaces" in body or b"SetNetworkDefaultGateway" in body:
                    # A simulator must never silently claim a state-changing operation
                    # succeeded. This fixture explicitly rejects writes by default.
                    payload = b'''<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body><s:Fault><s:Reason><s:Text>Writes disabled in simulator</s:Text></s:Reason></s:Fault></s:Body></s:Envelope>'''
                else:
                    self.send_error(400, "Unsupported simulator operation")
                    return
                self.send_response(200)
                self.send_header("Content-Type", "application/soap+xml; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
        self.http = ThreadingHTTPServer((self.args.bind, self.http_port), Handler)
        thread = threading.Thread(target=self.http.serve_forever, name="onvif-http", daemon=True)
        thread.start()
        LOG.info("ONVIF device service listening on %s:%d", self.args.bind, self.http_port)

    def run(self) -> None:
        self.start_http()
        self.start_project_udp()
        if not self.args.no_onvif:
            self.start_onvif_udp()
        LOG.info("Scenario=%s; Ctrl+C to stop", self.args.scenario)
        try:
            while not self.stop.wait(0.5):
                pass
        finally:
            self.shutdown()

    def shutdown(self) -> None:
        self.stop.set()
        if self.http:
            self.http.shutdown()
            self.http.server_close()
        for thread in self.udp_threads:
            thread.join(timeout=1.0)

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="0.0.0.0", help="Local bind address")
    parser.add_argument("--interface-ip", default="127.0.0.1", help="Interface IPv4 used for multicast membership")
    parser.add_argument("--advertised-ip", default="127.0.0.1", help="IP advertised by simulated camera")
    parser.add_argument("--http-port", type=int, default=18080)
    parser.add_argument("--mac", default="02:00:00:00:00:01")
    parser.add_argument("--no-onvif", action="store_true", help="Run only the Contract B UDP simulator")
    parser.add_argument("--scenario", choices=[
        "normal", "silent", "malformed-json", "wrong-sequence", "missing-mac",
        "invalid-port", "advertised-mismatch", "dhcp", "unsafe-xaddr",
        "multiple-xaddrs", "oversized", "auth-required", "soap-fault",
    ], default="normal")
    args = parser.parse_args()
    if not 1 <= args.http_port <= 65535:
        parser.error("--http-port must be between 1 and 65535")
    return args

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    simulator = Scenario(parse_args())
    signal.signal(signal.SIGINT, lambda *_: simulator.stop.set())
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, lambda *_: simulator.stop.set())
    simulator.run()
