"""Validate actual ESPHome configurations, including rejected transport conflicts."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ConfigTests(unittest.TestCase):
    def check(self, text, expected=None):
        text = text.replace("../esphome/components", str(ROOT / "esphome/components"))
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / "test.yaml"
            config.write_text(text)
            result = subprocess.run(
                [sys.executable, "-m", "esphome", "config", str(config)],
                capture_output=True,
                check=False,
                text=True,
            )
        output = result.stdout + result.stderr
        if expected is None:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertNotEqual(result.returncode, 0, output)
            self.assertIn(expected, output)
        return output

    def test_defaults_and_versions(self):
        v1 = self.check((ROOT / "tests/v1.yaml").read_text())
        self.assertIn("enable_polling: false", v1)
        self.assertIn("status_timeout: 0s", v1)
        v2 = self.check((ROOT / "tests/v2.yaml").read_text())
        self.assertIn("protocol: v2", v2)
        self.assertIn("enable_polling: true", v2)
        self.assertIn("status_timeout: 30s", v2)

    def test_invalid_v1_transports(self):
        text = (ROOT / "tests/v1.yaml").read_text()
        cases = [
            (
                text.replace("baud_rate: 250000", "baud_rate: 9600"),
                "requires baud rate 250000",
            ),
            (
                text.replace("  rx_full_threshold: 1", "  rx_full_threshold: 8"),
                "rx_full_threshold: 1",
            ),
            (
                text.replace("  flow_control_pin: GPIO4\n", ""),
                "requires zoneswitch.flow_control_pin",
            ),
            (
                text.replace(
                    "  rx_full_threshold: 1",
                    "  rx_full_threshold: 1\n  flow_control_pin: GPIO7",
                ),
                "only one owner",
            ),
            (
                text.replace(
                    "  rx_full_threshold: 1",
                    "  rx_full_threshold: 1\n  debug:\n    dummy_receiver: true",
                ),
                "dummy_receiver: false",
            ),
            (text.replace("type: esp-idf", "type: arduino"), "framework type esp-idf"),
            (
                text.replace(
                    "flow_control_pin: GPIO4",
                    "flow_control_pin:\n    number: GPIO4\n    inverted: true",
                ),
                "non-inverted",
            ),
        ]
        for config, error in cases:
            with self.subTest(error=error):
                self.check(config, error)

    def test_listen_only_and_protocol_options(self):
        for version in ("v1", "v2"):
            text = (ROOT / f"tests/{version}.yaml").read_text()
            passive = text.replace(
                "  id: test_zs", "  id: test_zs\n  listen_only: true"
            )
            output = self.check(passive)
            self.assertIn("enable_polling: false", output)
            self.check(
                passive.replace(
                    "  listen_only: true", "  listen_only: true\n  enable_polling: true"
                ),
                "listen_only cannot",
            )
        v1 = (ROOT / "tests/v1.yaml").read_text()
        for key, value in {
            "tx_node_addr": "0x48",
            "restore_node": "false",
            "node_confirmations": "3",
            "node_mismatch_threshold": "5",
            "offline_miss_threshold": "5",
        }.items():
            with self.subTest(option=key):
                self.check(
                    v1.replace("  id: test_zs", f"  id: test_zs\n  {key}: {value}"),
                    f"{key} applies only to protocol v2",
                )
        self.check(
            v1.replace("metric: transaction_result", "metric: node_address"),
            "node_address applies only to protocol v2",
        )
        self.check(
            v1.replace("metric: rx_ok", "metric: node_address"),
            "node_address applies only to protocol v2",
        )
        v2 = (ROOT / "tests/v2.yaml").read_text()
        self.check(
            v2.replace("baud_rate: 9600", "baud_rate: 250000"),
            "requires baud rate 9600",
        )
        for metric in ("ack_timeouts", "rejected_busy"):
            self.check(
                v2.replace("metric: rx_ok", f"metric: {metric}"),
                f"{metric} applies only to protocol v1",
            )

    def test_uart_framing(self):
        for version in ("v1", "v2"):
            text = (ROOT / f"tests/{version}.yaml").read_text()
            for setting in ("data_bits: 7", "parity: EVEN", "stop_bits: 2"):
                with self.subTest(version=version, setting=setting):
                    self.check(
                        text.replace(
                            "  id: test_uart", f"  id: test_uart\n  {setting}"
                        ),
                        "zoneswitch requires",
                    )

    def test_complete_carrier_example(self):
        text = (ROOT / "esphome/examples/v1-xiao-all-in-one.yaml").read_text()
        start = text.index("external_components:")
        end = text.index("uart:", start)
        source = f"external_components:\n  - source:\n      type: local\n      path: {ROOT / 'esphome/components'}\n    components: [zoneswitch]\n"
        text = text[:start] + source + text[end:]
        replacements = {
            "wifi_ssid": "test-network",
            "wifi_password": "test-password",
            "xiao_485_zoneswitch_api_key": "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
            "xiao_485_zoneswitch_ota_password": "test-password",
        }
        for key, value in replacements.items():
            text = text.replace(f"!secret {key}", f'"{value}"')
        self.check(text)

    def test_examples(self):
        prefix = "esphome:\n  name: test-example\nesp32:\n  variant: esp32s3\n  framework:\n    type: esp-idf\nlogger:\n  hardware_uart: USB_SERIAL_JTAG\n"
        for version in ("v1", "v2"):
            text = (ROOT / f"esphome/examples/{version}.yaml").read_text()
            start = text.index("external_components:")
            end = text.index("uart:", start)
            source = f"external_components:\n  - source:\n      type: local\n      path: {ROOT / 'esphome/components'}\n    components: [zoneswitch]\n"
            self.check(prefix + text[:start] + source + text[end:])


if __name__ == "__main__":
    unittest.main()
