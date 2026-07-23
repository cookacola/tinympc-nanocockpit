#!/usr/bin/env python3

import importlib.util
import pathlib
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("parse_combined_profile.py")
SPEC = importlib.util.spec_from_file_location("parse_combined_profile", MODULE_PATH)
PARSER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PARSER)


class CombinedProfileParserTest(unittest.TestCase):
    def test_quantile_window(self):
        row = PARSER.parse_quantile(
            "quantile,447,21250,21500,447,55500,55750"
        )
        self.assertEqual(
            row,
            {
                "flow_count": 447,
                "flow_total_p95_us_upper": 21250,
                "flow_total_p99_us_upper": 21500,
                "cnn_count": 447,
                "cnn_p95_us_upper": 55500,
                "cnn_p99_us_upper": 55750,
            },
        )

    def test_malformed_quantile_is_ignored(self):
        self.assertIsNone(PARSER.parse_quantile("quantile,1,2,3"))
        self.assertIsNone(PARSER.parse_quantile("profile,1,2,3,4,5,6"))


if __name__ == "__main__":
    unittest.main()
