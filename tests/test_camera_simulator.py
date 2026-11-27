import importlib.util
import http.client
import pathlib
import unittest
from argparse import Namespace

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("camera_simulator", ROOT / "tools" / "camera_simulator.py")
simulator_module = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(simulator_module)

def make_simulator(scenario="normal"):
    args = Namespace(
        scenario=scenario, advertised_ip="192.0.2.50", http_port=18080,
        mac="02:00:00:00:00:01"
    )
    return simulator_module.Scenario(args)

class ContractBSimulatorTests(unittest.TestCase):
    def test_emits_documented_contract_b_response(self):
        payload = make_simulator().project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 123, "cmd": "PROBE_REQUEST"
        })
        self.assertIsNotNone(payload)
        import json
        response = json.loads(payload)
        self.assertEqual(response["magic"], "CAM_DISCOVER")
        self.assertEqual(response["version"], 1)
        self.assertEqual(response["seq"], 123)
        self.assertEqual(response["cmd"], "PROBE_RESPONSE")
        self.assertEqual(response["device"]["ipv4"], "192.0.2.50")
        self.assertEqual(response["device"]["mac"], "02:00:00:00:00:01")

    def test_rejects_unknown_or_invalid_requests(self):
        sim = make_simulator()
        self.assertIsNone(sim.project_payload({"magic": "OTHER", "version": 1, "seq": 1, "cmd": "PROBE_REQUEST"}))
        self.assertIsNone(sim.project_payload({"magic": "CAM_DISCOVER", "version": 2, "seq": 1, "cmd": "PROBE_REQUEST"}))
        self.assertIsNone(sim.project_payload({"magic": "CAM_DISCOVER", "version": 1, "seq": True, "cmd": "PROBE_REQUEST"}))
        self.assertIsNone(sim.project_payload({"magic": "CAM_DISCOVER", "version": 1, "seq": 1, "cmd": "OTHER"}))

    def test_wrong_sequence_scenario_is_deterministic(self):
        payload = make_simulator("wrong-sequence").project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 77, "cmd": "PROBE_REQUEST"
        })
        import json
        self.assertEqual(json.loads(payload)["seq"], 78)

    def test_malformed_scenario_returns_truncated_json(self):
        payload = make_simulator("malformed-json").project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 7, "cmd": "PROBE_REQUEST"
        })
        import json
        with self.assertRaises(json.JSONDecodeError):
            json.loads(payload)

    def test_invalid_port_scenario_exercises_range_validation(self):
        payload = make_simulator("invalid-port").project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 7, "cmd": "PROBE_REQUEST"
        })
        import json
        self.assertEqual(json.loads(payload)["device"]["http_port"], 70000)

    def test_onvif_probe_contains_same_host_device_service(self):
        payload = make_simulator().probe_match("192.0.2.50")
        self.assertIn(b"192.0.2.50:18080/onvif/device_service", payload)
        self.assertIn(b"ProbeMatches", payload)

    def test_unsafe_xaddr_fixture_contains_credentials_and_query(self):
        payload = make_simulator("unsafe-xaddr").probe_match("192.0.2.50")
        self.assertIn(b"user:pass@", payload)
        self.assertIn(b"?token=bad", payload)

    def test_multiple_xaddr_fixture_contains_wrong_host_then_valid_host(self):
        payload = make_simulator("multiple-xaddrs").probe_match("192.0.2.50")
        self.assertIn(b"198.51.100.7", payload)
        self.assertIn(b"192.0.2.50:18080", payload)

    def test_advertised_ip_mismatch_fixture_is_explicit(self):
        payload = make_simulator("advertised-mismatch").project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 19, "cmd": "PROBE_REQUEST"
        })
        import json
        self.assertEqual(json.loads(payload)["device"]["ipv4"], "192.0.2.253")

    def test_oversized_scenario_serves_more_than_one_mib_soap_body(self):
        args = Namespace(
            scenario="oversized", advertised_ip="127.0.0.1", http_port=0,
            mac="02:00:00:00:00:01", bind="127.0.0.1", interface_ip="127.0.0.1"
        )
        simulator = simulator_module.Scenario(args)
        try:
            simulator.start_http()
            port = simulator.http.server_address[1]
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
            connection.request(
                "POST", "/onvif/device_service", body=b"<GetDeviceInformation/>",
                headers={"Content-Type": "application/soap+xml"}
            )
            response = connection.getresponse()
            body = response.read()
            self.assertEqual(response.status, 200)
            self.assertGreater(len(body), 1024 * 1024)
            connection.close()
        finally:
            simulator.shutdown()

    def test_missing_mac_is_allowed_as_a_negative_fixture(self):
        payload = make_simulator("missing-mac").project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 7, "cmd": "PROBE_REQUEST"
        })
        import json
        self.assertNotIn("mac", json.loads(payload)["device"])


    def _post_simulator_request(self, scenario, path="/onvif/device_service", body=b"<GetDeviceInformation/>", headers=None):
        args = Namespace(
            scenario=scenario, advertised_ip="127.0.0.1", http_port=0,
            mac="02:00:00:00:00:01", bind="127.0.0.1", interface_ip="127.0.0.1"
        )
        simulator = simulator_module.Scenario(args)
        try:
            simulator.start_http()
            port = simulator.http.server_address[1]
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
            connection.request(
                "POST", path, body=body,
                headers=headers or {"Content-Type": "application/soap+xml"}
            )
            response = connection.getresponse()
            payload = response.read()
            result = (response.status, dict(response.getheaders()), payload)
            connection.close()
            return result
        finally:
            simulator.shutdown()

    def test_auth_required_scenario_returns_basic_challenge(self):
        status, headers, _ = self._post_simulator_request("auth-required")
        self.assertEqual(status, 401)
        self.assertIn("Basic", headers.get("WWW-Authenticate", ""))

    def test_auth_required_scenario_accepts_followup_authorization_header(self):
        status, _, payload = self._post_simulator_request(
            "auth-required", headers={
                "Content-Type": "application/soap+xml",
                "Authorization": "Basic dXNlcjpwYXNz",
            }
        )
        self.assertEqual(status, 200)
        self.assertIn(b"GetDeviceInformationResponse", payload)

    def test_soap_fault_scenario_returns_a_soap_fault(self):
        status, _, payload = self._post_simulator_request("soap-fault")
        self.assertEqual(status, 200)
        self.assertIn(b"<s:Fault>", payload)
        self.assertIn(b"Simulated device fault", payload)

    def test_simulator_explicitly_rejects_state_changing_network_requests(self):
        status, _, payload = self._post_simulator_request(
            "normal", body=b"<SetNetworkInterfaces/>"
        )
        self.assertEqual(status, 200)
        self.assertIn(b"Writes disabled in simulator", payload)

    def test_simulator_rejects_unknown_device_service_path(self):
        status, _, _ = self._post_simulator_request("normal", path="/admin")
        self.assertEqual(status, 404)

    def test_simulator_rejects_oversized_soap_request_body(self):
        args = Namespace(
            scenario="normal", advertised_ip="127.0.0.1", http_port=0,
            mac="02:00:00:00:00:01", bind="127.0.0.1", interface_ip="127.0.0.1"
        )
        simulator = simulator_module.Scenario(args)
        connection = None
        try:
            simulator.start_http()
            port = simulator.http.server_address[1]
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
            connection.putrequest("POST", "/onvif/device_service")
            connection.putheader("Content-Type", "application/soap+xml")
            connection.putheader("Content-Length", str(1024 * 1024 + 1))
            connection.endheaders()
            response = connection.getresponse()
            self.assertEqual(response.status, 413)
            response.read()
        finally:
            if connection is not None:
                connection.close()
            simulator.shutdown()

if __name__ == "__main__":
    unittest.main(verbosity=2)
