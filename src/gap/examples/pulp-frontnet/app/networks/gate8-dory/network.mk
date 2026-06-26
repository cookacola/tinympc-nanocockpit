# network.mk — app-layer build wiring for the standalone gate8-dory DORY package.
# Mirrors frontnet-160x32-bgaug/network.mk, but with gate8-dory's FLAT hex names
# and filtering out src/main.c (the standalone gvsoc test harness has its own
# main(); the app provides main in examples/pulp-frontnet/main.c).

CORE ?= 8
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM

# All network sources EXCEPT the standalone gvsoc test harness main.c.
APP_SRCS += $(filter-out %/main.c, $(wildcard $(NETWORK_DIR)/src/*.c))
APP_CFLAGS += -I$(NETWORK_DIR)/inc
APP_LDFLAGS += -lm

APP_CFLAGS += -DNUM_CORES=$(CORE)

# Stock-DORY generated code trips -Werror (mchan.h macro redefinition, implicit
# log2 builtin in dory_dma.c). The IDSIA-template (bgaug) package didn't; until we
# regenerate gate8 with that template, relax -Werror for this build. (mchan's macro
# redefinition has no specific -W name, so this must be the blanket form.)
APP_CFLAGS += -Wno-error
APP_CFLAGS += -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -Wl,--print-memory-usage -flto
APP_CFLAGS += -DGAP_SDK=1

ifeq '$(FLASH_TYPE)' 'MRAM'
    READFS_FLASH = target/chip/soc/mram
endif

APP_CFLAGS += -DFLASH_TYPE=$(FLASH_TYPE) -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS

# gate8-dory uses FLAT hex names; must match network.c's L3_weights_files[].
# (inputs.hex is intentionally omitted — the deploy feeds the camera, not a file.)
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution4_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution5_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution6_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/BNReluConvolution7_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/FullyConnected8_weights.hex

READFS_FILES += $(FLASH_FILES)
APP_CFLAGS += -DFS_READ_FS
