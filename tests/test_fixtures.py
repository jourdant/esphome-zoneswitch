"""Check fixture provenance independently of the production parser replay."""

import csv
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CaptureMetadataTests(unittest.TestCase):
    def test_source_bytes_and_unique_ids(self):
        seen = set()
        with (ROOT / "tests/fixtures/captures.tsv").open() as stream:
            for row in csv.DictReader(stream, delimiter="\t"):
                with self.subTest(capture=row["id"]):
                    self.assertNotIn(row["id"], seen)
                    seen.add(row["id"])
                    path, number = row["source"].rsplit(":", 1)
                    line = (ROOT / path).read_text().splitlines()[int(number) - 1]
                    self.assertIn(row["timestamp"], line)
                    raw = (
                        line.split("<<<", 1)[1].strip().replace(":", " ")
                        if "<<<" in line
                        else line.split(" ", 1)[1]
                    )
                    self.assertEqual(bytes.fromhex(row["bytes"]), bytes.fromhex(raw))
                    self.assertIn(row["protocol"], ("v1", "v2"))
                    self.assertLessEqual(int(row["expected_mask"], 16), 63)
