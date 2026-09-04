APP = olgmd_obstacle

APP_CFLAGS += -I$(CURDIR) -I$(CURDIR)/../../lib
APP_CFLAGS += -g -O2 -fno-indirect-inlining -flto -Werror
APP_CFLAGS += -Wno-error=multichar -Wno-error=int-conversion
APP_CFLAGS += -Wno-error=implicit-function-declaration
APP_CFLAGS += -Wno-error=incompatible-pointer-types
APP_CFLAGS += -Wno-error=discarded-qualifiers -Wno-error=attributes
APP_LDFLAGS += -g -flto -Wl,--print-memory-usage

APP_SRCS += main.c olgmd1.c olgmd_uart.c
APP_SRCS += ../../lib/camera.c ../../lib/camera/himax.c ../../lib/cluster.c
APP_SRCS += ../../lib/crc32.c ../../lib/debug.c ../../lib/soc.c
APP_SRCS += ../../lib/time.c ../../lib/trace.c ../../lib/uart.c ../../lib/uart_protocol.c

include $(RULES_DIR)/pmsis_rules.mk
