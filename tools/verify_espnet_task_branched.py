#!/usr/bin/env python3
"""Generate deterministic uint8 fixtures from the five ESPNet integer ONNX partitions.

Run with /home/cchen/isaacsim-env/bin/python. Inputs are HWC temporal channels;
ONNX tensors use NCHW float containers for integer values. No model is changed.
An optional GVSOC log must contain: PARITY output crc32=<8 hexadecimal digits>.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zlib

import numpy as np
import onnxruntime as ort

DEFAULT_BUNDLE = Path(__file__).resolve().parents[1] / 'releases/espnetv2_gap8_task_branched_v8_full_int8'
DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / 'releases/espnet-task-branched-validation'
PARTITIONS = {
    'encoder': ('integer_symw8_gw7/encoder/encoder_int.onnx', (1, 32, 40, 40)),
    'global_head': ('integer_symw8_gw7/global_head/global_head_int.onnx', (1, 3, 1, 1)),
    'gate_encoder': ('integer_gate_symw8_gw7_cw7/gate_encoder/gate_encoder_int.onnx', (1, 32, 40, 40)),
    'corner_head': ('integer_gate_symw8_gw7_cw7/corner_head/corner_head_int.onnx', (1, 4, 20, 20)),
    'gate_head': ('integer_gate_symw8_gw7_cw7/gate_head/gate_head_int.onnx', (1, 5, 1, 1)),
}


def integer_u8(value, shape, label):
    if value.shape != shape:
        raise ValueError(f'{label}: shape {value.shape}, expected {shape}')
    if not np.isfinite(value).all() or not np.array_equal(value, np.rint(value)):
        raise ValueError(f'{label}: noninteger or nonfinite values')
    if value.min() < 0 or value.max() > 255:
        raise ValueError(f'{label}: outside uint8 range')
    return value.astype(np.uint8)


def read_dump(path):
    return np.fromstring(path.read_text().replace(',', ' '), sep=' ', dtype=np.float32)


def crc(data):
    return f'{zlib.crc32(data) & 0xffffffff:08x}'


def verify_decoder(bundle, output_dir, packed):
    target = Path(__file__).resolve().parents[1] / 'src/gap/examples/espnet-task-branched'
    header = target / 'app/networks/espnetv2-v8-full-int8/inc/espnet_output.h'
    harness = output_dir / 'host_decoder_check.c'
    executable = output_dir / 'host_decoder_check'
    harness.write_text(r'''#include <stdio.h>
#include "espnet_decode.h"
int main(int argc, char **argv) {
    unsigned char packed[1608]; espnet_decoded_t d;
    if (argc != 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(packed, 1, sizeof packed, f) != sizeof packed) return 2;
    fclose(f); espnet_decode(packed, &d);
    for (int i=0;i<3;i++) printf("%.9g ",d.collision[i]);
    for (int i=0;i<3;i++) printf("%.9g ",d.affordance[i]);
    for (int i=0;i<2;i++) printf("%.9g ",d.visibility[i]);
    for (int i=0;i<8;i++) printf("%.9g ",d.corners[i]);
    for (int i=0;i<2;i++) printf("%u ",d.visible[i]);
    puts(""); return 0;
}
''')
    # GCC temporary files also remain inside the remote validation directory.
    import os
    env = dict(os.environ, TMPDIR=str(output_dir.resolve()))
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-I', str(target), '-I', str(header.parent), str(harness), str(target / 'espnet_decode.c'), '-lm', '-o', str(executable)], check=True, env=env)
    actual = np.fromstring(subprocess.check_output([str(executable), str(output_dir / 'expected_output.bin')], text=True), sep=' ')
    metadata = {}
    for relative in ('integer_symw8_gw7/nemo_report.json', 'integer_gate_symw8_gw7_cw7/nemo_report.json'):
        for partition in json.loads((bundle / relative).read_text())['partitions']:
            metadata[partition['graph']] = partition
    def logits(name, values):
        p = metadata[name]
        # Independent reference: preserve the original four-term formula,
        # rather than trusting generated header constants or folding twice.
        return (np.asarray(values, dtype=np.float64) * p['output_epsilon']
                - np.asarray(p['output_offset']) + np.asarray(p['learned_bias'])
                - np.asarray(p['teacher_logit_offset']))
    collision = 1 / (1 + np.exp(-logits('global_head', list(packed[1605:1608]))))
    gate = logits('gate_head', list(packed[1600:1605]))
    affordance = np.exp(gate[:3] - gate[:3].max())
    affordance /= affordance.sum()
    visibility = 1 / (1 + np.exp(-gate[3:]))
    peaks = np.argmax(np.frombuffer(packed[:1600], dtype=np.uint8).reshape(400, 4), axis=0)
    corners = np.stack((peaks % 20, peaks // 20), axis=1).reshape(-1) * (159.0 / 19.0)
    expected = np.concatenate((collision, affordance, visibility, corners, visibility >= 0.5))
    np.testing.assert_allclose(actual, expected, rtol=2e-6, atol=1e-5)
    report = {'passed': True, 'max_absolute_error': float(np.max(np.abs(actual - expected))),
              'collision': actual[:3].tolist(), 'affordance': actual[3:6].tolist(),
              'visibility': actual[6:8].tolist(), 'corners': actual[8:16].reshape(4,2).tolist(),
              'visible': actual[16:18].astype(int).tolist(),
              'reference': 'uint8*output_epsilon-output_offset+learned_bias-teacher_logit_offset; NumPy sigmoid, softmax and HWC argmax',
              'decoder_sha256': hashlib.sha256((target / 'espnet_decode.c').read_bytes()).hexdigest(),
              'output_header_sha256': hashlib.sha256(header.read_bytes()).hexdigest()}
    (output_dir / 'host_decoder_report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, default=DEFAULT_BUNDLE)
    parser.add_argument('--output-dir', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--gvsoc-log', type=Path)
    parser.add_argument('--verify-decoder', action='store_true', help='Compile the firmware decoder on host and compare source-metadata NumPy decoding')
    args = parser.parse_args()
    source = args.bundle / 'integer_symw8_gw7/encoder/input.txt'
    flat = read_dump(source)
    hwc = integer_u8(flat, (76800,), 'input').reshape(160, 160, 3)
    input_tensor = hwc.transpose(2, 0, 1)[None].astype(np.float32)
    models = {}
    outputs = {}
    for name, (relative, shape) in PARTITIONS.items():
        path = args.bundle / relative
        session = ort.InferenceSession(str(path), providers=['CPUExecutionProvider'])
        x = input_tensor if name in ('encoder', 'gate_encoder') else outputs['encoder' if name == 'global_head' else 'gate_encoder'].astype(np.float32)
        if list(x.shape) != session.get_inputs()[0].shape:
            raise ValueError(f'{name}: input shape mismatch')
        results = session.run(None, {session.get_inputs()[0].name: x})
        if len(results) != 1:
            raise ValueError(f'{name}: expected one output')
        outputs[name] = integer_u8(results[0], shape, name)
        models[name] = {'path': relative, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'input_shape': list(x.shape), 'output_shape': list(shape), 'output_min': int(outputs[name].min()), 'output_max': int(outputs[name].max()), 'output_hwc_crc32': crc(outputs[name].transpose(0, 2, 3, 1).tobytes())}
    # This exact equality establishes the source text is HWC, not CHW.
    golden = read_dump(source.parent / 'out_layer10.txt')
    if not np.array_equal(outputs['encoder'].transpose(0, 2, 3, 1).reshape(-1), golden):
        raise ValueError('Source encoder golden mismatch: input layout or model changed')
    corner = outputs['corner_head'].transpose(0, 2, 3, 1).tobytes()
    gate = outputs['gate_head'].tobytes()
    collision = outputs['global_head'].tobytes()
    packed = corner + gate + collision
    if len(packed) != 1608:
        raise ValueError('Packed result must have 1608 bytes')
    report = {
        'bundle': str(args.bundle.resolve()), 'onnxruntime_version': ort.__version__,
        'input_source': str(source), 'input_source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'input_shape_hwc': [160, 160, 3], 'input_bytes': hwc.nbytes, 'input_crc32': crc(hwc.tobytes()),
        'source_encoder_golden_exact_match': True,
        'output_bytes': len(packed), 'output_crc32': crc(packed),
        'packing': [{'name': 'corners_hwc', 'offset': 0, 'bytes': 1600, 'shape': [20, 20, 4], 'crc32': crc(corner)}, {'name': 'gate', 'offset': 1600, 'bytes': 5, 'values': list(gate), 'crc32': crc(gate)}, {'name': 'collision', 'offset': 1605, 'bytes': 3, 'values': list(collision), 'crc32': crc(collision)}],
        'models': models,
    }
    if args.gvsoc_log:
        matches = re.findall(r'PARITY[^\r\n]*?\boutput(?:\s+|_)crc32\s*[=:]\s*(?:0x)?([0-9a-fA-F]{8})\b', args.gvsoc_log.read_text(errors='replace'))
        if not matches or any(value.lower() != report['output_crc32'] for value in matches):
            raise ValueError(f'GVSOC output CRC mismatch or missing PARITY line: expected {report["output_crc32"]}, found {matches}')
        report['gvsoc'] = {'log': str(args.gvsoc_log.resolve()), 'output_crc32_matches': True, 'matching_lines': len(matches)}
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / 'input_hwc.raw').write_bytes(hwc.tobytes())
    (args.output_dir / 'expected_output.bin').write_bytes(packed)
    if args.verify_decoder:
        report['host_decoder'] = verify_decoder(args.bundle, args.output_dir, packed)
    (args.output_dir / 'onnx_parity_report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'input_crc32': report['input_crc32'], 'output_crc32': report['output_crc32'], 'output_bytes': len(packed), 'gate': list(gate), 'collision': list(collision), 'gvsoc': report.get('gvsoc'), 'output_dir': str(args.output_dir)}, indent=2))


if __name__ == '__main__':
    main()
