#!/usr/bin/env python3
"""Package the promoted five-partition ESPNet v8 release for GAP8 bring-up."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil

GRAPHS = ("encoder", "global_head", "gate_encoder", "gate_head", "corner_head")
SIZES = dict(encoder=51200, global_head=3, gate_encoder=51200, gate_head=5, corner_head=1600)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def norm(text):
    return "\n".join(x.rstrip() for x in text.splitlines()).rstrip() + "\n"

def cf(value):
    return repr(float(value)) + "f"

def package(source, destination):
    if destination.exists():
        raise FileExistsError(destination)
    apps = {g: source / (g + "_gap8_application") for g in GRAPHS}
    reports = {}
    for filename in ("integer_symw8_gw7/nemo_report.json", "integer_gate_symw8_gw7_cw7/nemo_report.json"):
        reports.update({g["graph"]: g for g in json.loads((source / filename).read_text())["partitions"]})
    assert set(reports) == set(GRAPHS)
    for g in GRAPHS:
        assert math.prod(reports[g]["integer_output_shape"]) == SIZES[g]
        assert json.loads((source / (g + "_dory_report.json")).read_text())["gap8_final_output_bytes"] == SIZES[g]
    # Every non-graph kernel/header must be identical; do not silently choose
    # one graph's implementation for another graph.
    common = {}
    for sub, pattern in (("src", "*.c"), ("inc", "*.h")):
        sets = [{p.name for p in (a/sub).glob(pattern) if not p.name.startswith("gap8_")} for a in apps.values()]
        assert all(s == sets[0] for s in sets), (sub, sets)
        for name in sorted(sets[0]):
            texts = [norm((a/sub/name).read_text()) for a in apps.values()]
            assert all(t == texts[0] for t in texts), "common source mismatch: " + name
            common[sub + "/" + name] = texts[0]
    memory = {}
    for g,a in apps.items():
        c = (a/"src/gap8_network.c").read_text()
        alloc = {k:int(v) for k,v in re.findall(r"#define (L3_\w+_SIZE) (\d+)", c)}
        weights = sum(p.stat().st_size for p in (a/"hex").glob("gap8_*_weights.hex"))
        assert weights <= alloc["L3_WEIGHTS_SIZE"], (g, weights, alloc)
        assert max(map(int,re.findall(r"pi_l2_malloc\((\d+)\)", (a/"src/gap8_main.c").read_text()))) <= 260000
        memory[g] = dict(alloc, serialized_weights=weights)
    for sub in ("src", "inc", "hex"):
        (destination/sub).mkdir(parents=True)
    for name,text in common.items():
        (destination/name).write_text(text)
    for g,a in apps.items():
        for sub,pattern in (("src","gap8_*.c"),("inc","gap8_*.h"),("hex","gap8_*_weights.hex")):
            for p in sorted((a/sub).glob(pattern)):
                if p.name in ("gap8_main.c", "gap8_checksum_input.c"):
                    continue
                name = p.name.replace("gap8_", "espnet_" + g + "_")
                if sub == "hex":
                    shutil.copy2(p, destination/sub/name)
                else:
                    text = p.read_text().replace("gap8_", "espnet_" + g + "_").replace("GAP8_", "ESPNET_" + g.upper() + "_")
                    # Keep each verified allocation, including >256k heads.
                    (destination/sub/name).write_text(norm(text))
    header = """#ifndef ESPNET_OUTPUT_H
#define ESPNET_OUTPUT_H
#include <stdint.h>
#define ESPNET_INPUT_BYTES 76800
#define ESPNET_FEATURE_BYTES 51200
#define ESPNET_CORNER_BYTES 1600
#define ESPNET_GATE_OFFSET 1600
#define ESPNET_COLLISION_OFFSET 1605
#define ESPNET_OUTPUT_BYTES 1608
#define ESPNET_WORKSPACE_BYTES 260000
/* Input HWC [160,160,3]: previous, current, (current-previous+255)/2.
 * Corners HWC [20,20,4]: LT, RT, LB, RB.
 * Gate: none, opening_left, opening_right, left_rail, right_rail.
 * Collision: left, center, right (native model order).
 * Teacher logit = uint8 * EPSILON - offset[channel].
 * offset below folds NeMO offset - learned_bias + teacher_logit_offset.
 */
"""
    affine = {}
    for label,g in (("corner","corner_head"),("gate","gate_head"),("collision","global_head")):
        r=reports[g]
        offsets=[o-b+t for o,b,t in zip(r["output_offset"],r["learned_bias"],r["teacher_logit_offset"])]
        affine[label]=dict(epsilon=r["output_epsilon"],offset=offsets)
        header += "#define ESPNET_" + label.upper() + "_EPSILON " + cf(r["output_epsilon"]) + "\n"
        header += "static const float espnet_" + label + "_offset[" + str(len(offsets)) + "] = {" + ", ".join(map(cf,offsets)) + "};\n"
    (destination/"inc/espnet_output.h").write_text(header + "#endif\n")
    (destination/"inc/network.h").write_text("""#ifndef ESPNET_NETWORK_H
#define ESPNET_NETWORK_H
#include <stddef.h>
#include "pmsis.h"
/* One in-flight inference; input/output may alias workspace. */
void network_initialize(void);
void network_terminate(void);
void network_run_async_cl(void *, size_t, void *, int, int, pi_device_t *, pi_task_t *);
#endif
""")
    wrapper = """#include "network.h"
#include "espnet_output.h"
#include "mem.h"
#include <string.h>
#include <stdio.h>
"""
    wrapper += "".join('#include "espnet_' + g + '_network.h"\n' for g in GRAPHS)
    wrapper += """
static void *saved_input;
static PI_L2 uint8_t features[ESPNET_FEATURE_BYTES];
static PI_L2 uint8_t outputs[ESPNET_OUTPUT_BYTES];
static pi_task_t step_done[5];
static void *workspace, *final_output;
static size_t workspace_size;
static int run_exec, run_dir, busy;
static pi_device_t *run_cluster;
static pi_task_t *user_done;

static void finished(void *arg) {
    (void)arg;
    memcpy(final_output, outputs, ESPNET_OUTPUT_BYTES);
    busy = 0;
    pi_task_push(user_done);
}
static void gate_finished(void *arg) {
    (void)arg;
    memcpy(workspace, features, ESPNET_FEATURE_BYTES);
    espnet_corner_head_network_run_async_cl(workspace, workspace_size,
        outputs, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[4], finished, NULL));
}
static void gate_encoder_finished(void *arg) {
    (void)arg;
    memcpy(workspace, features, ESPNET_FEATURE_BYTES);
    espnet_gate_head_network_run_async_cl(workspace, workspace_size,
        outputs + ESPNET_GATE_OFFSET, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[3], gate_finished, NULL));
}
static void collision_finished(void *arg) {
    (void)arg;
    ram_read(workspace, saved_input, ESPNET_INPUT_BYTES);
    espnet_gate_encoder_network_run_async_cl(workspace, workspace_size,
        features, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[2], gate_encoder_finished, NULL));
}
static void encoder_finished(void *arg) {
    (void)arg;
    memcpy(workspace, features, ESPNET_FEATURE_BYTES);
    espnet_global_head_network_run_async_cl(workspace, workspace_size,
        outputs + ESPNET_COLLISION_OFFSET, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[1], collision_finished, NULL));
}
void network_initialize(void) {
    saved_input = ram_malloc(ESPNET_INPUT_BYTES);
    if (!saved_input) { printf("ESPNET input preservation allocation failed\\n"); pmsis_exit(-1); }
"""
    wrapper += "".join("    espnet_" + g + "_network_initialize();\n" for g in GRAPHS)
    wrapper += "}\nvoid network_terminate(void) {\n"
    wrapper += "".join("    espnet_" + g + "_network_terminate();\n" for g in GRAPHS)
    wrapper += """    ram_free(saved_input, ESPNET_INPUT_BYTES);
    saved_input = NULL;
}
void network_run_async_cl(void *buffer, size_t size, void *output,
                          int exec, int initial_dir,
                          pi_device_t *cluster, pi_task_t *done) {
    if (busy || !saved_input || size < ESPNET_WORKSPACE_BYTES || initial_dir != 1) {
        printf("ESPNET invalid inference state/workspace/direction\\n");
        pmsis_exit(-1);
    }
    busy = 1;
    workspace = buffer; workspace_size = size; final_output = output;
    run_exec = exec; run_dir = initial_dir; run_cluster = cluster; user_done = done;
    ram_write(saved_input, buffer, ESPNET_INPUT_BYTES);
    espnet_encoder_network_run_async_cl(buffer, size, features, exec, initial_dir,
        cluster, pi_task_callback(&step_done[0], encoder_finished, NULL));
}
"""
    (destination/"src/espnet_network.c").write_text(wrapper)
    mk="""# Five-partition ESPNet v8; no legacy map ABI.
CORE ?= 8
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM
APP_SRCS += $(wildcard $(NETWORK_DIR)/src/*.c)
APP_CFLAGS += -I$(NETWORK_DIR)/inc -DNUM_CORES=$(CORE)
APP_CFLAGS += -Wno-error -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -lm -flto
APP_CFLAGS += -DGAP_SDK=1 -DFLASH_TYPE=$(FLASH_TYPE)
APP_CFLAGS += -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS -DFS_READ_FS
"""
    mk += "".join("READFS_FILES += $(NETWORK_DIR)/hex/" + p.name + "\n" for p in sorted((destination/"hex").glob("*")))
    (destination/"network.mk").write_text(mk)
    manifest=dict(format="nanocockpit-espnet-task-branched-v1",source_release=str(source),
        graphs=list(GRAPHS),input=dict(layout="HWC",shape=[160,160,3]),
        output=dict(bytes=1608,corner_shape_hwc=[20,20,4],corner_order=["LT","RT","LB","RB"],gate_offset=1600,collision_offset=1605),
        affine=affine,memory=dict(l2_workspace=260000,l2_wrapper_buffers=52808,l3_saved_input=76800,partitions=memory),
        lifecycle="bringup_only_physical_hardware_unvalidated",
        collision_quality_override=True)
    manifest["files_sha256"]={str(p.relative_to(destination)):digest(p) for p in sorted(destination.rglob("*")) if p.is_file()}
    (destination/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(json.dumps(manifest["memory"],indent=2))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source",type=Path,required=True)
    p.add_argument("--release",type=Path,required=True)
    p.add_argument("--destination",type=Path,required=True)
    a=p.parse_args()
    source=a.source.resolve()
    original={str(p.relative_to(source)):digest(p) for p in sorted(source.rglob("*")) if p.is_file()}
    if a.release.exists():
        raise FileExistsError(a.release)
    shutil.copytree(source,a.release,symlinks=False)
    copied={str(p.relative_to(a.release)):digest(p) for p in sorted(a.release.rglob("*")) if p.is_file()}
    assert original == copied, "release copy failed integrity verification"
    (a.release/"COPY_INTEGRITY.json").write_text(json.dumps(dict(source=str(source),files_sha256=copied),indent=2)+"\n")
    package(a.release.resolve(),a.destination.resolve())
if __name__=="__main__":
    main()
