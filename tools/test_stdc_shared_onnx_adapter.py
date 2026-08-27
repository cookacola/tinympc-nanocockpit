import numpy as np

from stdc_shared_onnx_adapter import _validate_or_recover_gate


def test_all_four_missing_corner_positions_recover_the_square():
    expected = np.asarray(
        [[40, 20], [120, 20], [120, 100], [40, 100]], dtype=np.float32
    )
    for missing in range(4):
        corners = expected.copy()
        corners[missing] = (2, 2)
        confident = np.ones(4, dtype=bool)
        confident[missing] = False
        recovered, recovered_corner, reason = _validate_or_recover_gate(
            corners, confident, 0
        )
        assert recovered_corner == missing
        assert reason == "accepted_three_corners"
        assert np.allclose(recovered, expected)


def test_two_missing_or_out_of_bounds_recovery_is_rejected_without_mutation():
    corners = np.asarray(
        [[40, 20], [120, 20], [20, 80], [2, 2]], dtype=np.float32
    )
    result, recovered_corner, reason = _validate_or_recover_gate(
        corners, np.asarray([True, True, True, False]), 0
    )
    assert recovered_corner is None
    assert reason == "recovered_out_of_bounds"
    assert np.array_equal(result, corners)

    result, recovered_corner, reason = _validate_or_recover_gate(
        corners, np.asarray([True, False, False, True]), 0
    )
    assert recovered_corner is None
    assert reason == "confidence"
    assert np.array_equal(result, corners)
