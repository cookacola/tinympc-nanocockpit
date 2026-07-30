import unittest
from pathlib import Path

from tutorial.tutorial_impl.actions_or_control.reply import make_inference_reply
from tutorial.tutorial_impl.evaluation.source_audit import load_cpx_constants, load_frontnet_contract
from tutorial.tutorial_impl.execution.deployment_plan import build_frontnet_deployment_plan
from tutorial.tutorial_impl.sensing_or_observations.contracts import StreamerMetadata


REPO_ROOT = Path(__file__).resolve().parents[2]


class FrontnetTutorialTests(unittest.TestCase):
    def test_frontnet_contract_matches_original_header(self):
        contract = load_frontnet_contract(REPO_ROOT, "frontnet-160x32-bgaug")
        self.assertEqual(contract.input_count, 15360)
        self.assertEqual(contract.input_shape, (160, 32, 3))
        self.assertEqual(contract.output_count, 4)
        self.assertEqual(contract.l2_buffer_size, 352000)

    def test_cpx_constants_match_streamer_route(self):
        constants = load_cpx_constants(REPO_ROOT)
        self.assertEqual(constants["CPX_VERSION"], 0)
        self.assertEqual(constants["CPX_T_WIFI_HOST"], 3)
        self.assertEqual(constants["CPX_T_GAP"], 4)
        self.assertEqual(constants["CPX_F_STREAMER"], 6)

    def test_inference_reply_preserves_state_timestamp(self):
        metadata = StreamerMetadata(
            metadata_version=1,
            frame_id=3,
            frame_gap8_timestamp_us=2000,
            state_gap8_timestamp_us=1500,
            state_stm32_timestamp=99,
        )
        reply = make_inference_reply(metadata, [1.0, 2.0, 3.0, 4.0])
        self.assertEqual(reply.reply_frame_id, 3)
        self.assertEqual(reply.inference.stm32_timestamp, 99)
        self.assertEqual(reply.inference.as_tuple(), (1.0, 2.0, 3.0, 4.0))

    def test_empty_reply_has_zero_timestamp_for_onboard_inference_ack(self):
        metadata = StreamerMetadata(
            metadata_version=1,
            frame_id=3,
            frame_gap8_timestamp_us=2000,
            state_gap8_timestamp_us=1500,
            state_stm32_timestamp=99,
        )
        reply = make_inference_reply(metadata, None)
        self.assertEqual(reply.inference.stm32_timestamp, 0)

    def test_deployment_plan_keeps_component_order(self):
        steps = build_frontnet_deployment_plan(REPO_ROOT, "aideck.local", 5000, "radio://0/100/2M/E7E7E7E7E7")
        self.assertEqual(
            [step.component for step in steps],
            ["host", "stm32", "stm32", "gap8", "gap8", "nina", "nina", "host", "host"],
        )
        self.assertIn("./gap8.sh examples/pulp-frontnet", steps[3].command)
        self.assertIn("./flash-jtag.sh", steps[6].command)


if __name__ == "__main__":
    unittest.main()
