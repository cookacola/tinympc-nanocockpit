TILED ?= 1
LIVE ?= 0
APP = ddnd
APP_SRCS := src/main.c ../runtime/ddnd_runtime.c
APP_CFLAGS += -Iinc -I../runtime -O2 -fno-indirect-inlining -DNUM_CORES=8 -DFS_READ_FS -DDDND_GAP8
APP_LDFLAGS += -lm -Wl,--print-memory-usage
READFS_FILES := $(wildcard hex/*.bin)
ifeq ($(LIVE),1)
APP = ddnd_live
APP_SRCS := src/live.c ../runtime/ddnd_runtime.c src/live_himax.c
APP_CFLAGS += -I../vendor/live_nanocockpit/lib
READFS_FILES := hex/weights.bin
endif
ifeq ($(TILED),1)
APP_CFLAGS += -DDDND_TILED
endif
ifeq ($(LIVE_DEBUG),1)
APP_CFLAGS += -DDDND_LIVE_DEBUG
endif
ifeq ($(RAW_UART),1)
APP_CFLAGS += -DDDND_RAW_UART
endif
include $(RULES_DIR)/pmsis_rules.mk
$(BUILDDIR)/src/main.o: inc/graph.h inc/ddnd_board.h ../runtime/ddnd_runtime.h
$(BUILDDIR)/src/live.o: inc/graph.h inc/ddnd_board.h inc/config.h inc/sdk_compat.h inc/depthgate_output.h ../runtime/ddnd_runtime.h

