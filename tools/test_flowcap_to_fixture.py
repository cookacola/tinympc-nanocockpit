#!/usr/bin/env python3

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from flowcap_to_fixture import parse_capture, write_fixture


CAPTURE = """noise
FLOWCAP_BEGIN,1,100,167,2,2,1
FLOWCAP_FRAME,prev,4,00010203
FLOWCAP_FRAME,cur,4,04050607
FLOWCAP_TRACK,0,0.5,1.5,0.75,1.25,2.0,0.25
FLOWCAP_END,1
noise
"""


class FlowcapTest(unittest.TestCase):
    def test_parse_and_write(self):
        metadata, frames, tracks = parse_capture(CAPTURE)
        self.assertEqual(metadata["width"], 2)
        self.assertEqual(frames["cur"], bytes((4, 5, 6, 7)))
        self.assertEqual(tracks[0].fb_error, 0.25)
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "pair"
            write_fixture(prefix, metadata, frames, tracks)
            self.assertTrue((prefix.parent / "pair_prev.pgm").read_bytes().endswith(
                bytes((0, 1, 2, 3))))
            self.assertEqual(json.loads(prefix.with_suffix(".json").read_text())[
                "dt_us"], 67)
            with prefix.with_suffix(".csv").open() as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 1)

    def test_rejects_truncated_capture(self):
        with self.assertRaises(ValueError):
            parse_capture(CAPTURE.replace("FLOWCAP_END,1", ""))


if __name__ == "__main__":
    unittest.main()
