import importlib.util
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

    def test_missing_mac_is_allowed_as_a_negative_fixture(self):
        payload = make_simulator("missing-mac").project_payload({
            "magic": "CAM_DISCOVER", "version": 1, "seq": 7, "cmd": "PROBE_REQUEST"
        })
        import json
        self.assertNotIn("mac", json.loads(payload)["device"])

if __name__ == "__main__":
    unittest.main(verbosity=2)
