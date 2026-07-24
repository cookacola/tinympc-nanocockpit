#
# Makefile
# Elia Cereda <elia.cereda@idsia.ch>
#
# Copyright (C) 2022-2025 IDSIA, USI-SUPSI
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

APP = pulp_frontnet

APP_CFLAGS  += -I$(CURDIR) -I$(CURDIR)/../../lib
APP_CFLAGS  += -Werror
FLOW_FEATURE_COUNT ?= 27
APP_CFLAGS  += -DFLOW_MAX_FEATURES=$(FLOW_FEATURE_COUNT)
FLOW_BUILD_ID ?= unknown
APP_CFLAGS  += -DFLOW_BUILD_ID=\"$(FLOW_BUILD_ID)\"
CAMERA_FRAME_RATE_HZ ?= 30
PIPELINE_PERIOD_US ?= 65000
APP_CFLAGS  += -DHIMAX_FRAME_RATE=$(CAMERA_FRAME_RATE_HZ).0f
APP_CFLAGS  += -DVISION_PERIOD_US=$(PIPELINE_PERIOD_US)u
FLOW_DIAGNOSTIC_CAPTURE ?= 0
APP_CFLAGS  += -DFLOW_DIAGNOSTIC_CAPTURE=$(FLOW_DIAGNOSTIC_CAPTURE)
FLOW_LEGACY_SECTORS ?= 0
APP_CFLAGS  += -DFLOW_LEGACY_SECTORS=$(FLOW_LEGACY_SECTORS)

# -O2 with -fno-indirect-inlining is just as fast as -O3 and reduces code size considerably
# by not inlining of small functions in the management code
APP_CFLAGS  += -g -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -g -Wl,--print-memory-usage -flto

APP_SRCS += main.c flow_obstacle_uart.c
APP_SRCS += ../../lib/camera.c ../../lib/camera/himax.c ../../lib/cluster.c ../../lib/crc32.c ../../lib/debug.c ../../lib/rng.c ../../lib/soc.c ../../lib/streamer.c ../../lib/time.c ../../lib/trace.c ../../lib/queue.c
APP_SRCS += ../../lib/cpx/cpx.c ../../lib/cpx/cpx_spi.c
APP_SRCS += ../../lib/uart.c ../../lib/uart_protocol.c

include app/app.mk

# GVSOC configuration
# override config_args += --config-opt=camera/image-stream=$(CURDIR)/images/fullimage.pgm
# override config_args += --config-opt=uart/baudrate=115200 --config-opt=uart/stdout=true --config-opt=uart/stdin=true

include $(RULES_DIR)/pmsis_rules.mk
