# network.mk — app-layer build wiring for the gate8-dory DORY package.
# Mirrors frontnet-160x32-bgaug/network.mk with gate8-dory's flat hex names, and
# drops src/main.c so the app's main is used.

CORE ?= 8
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM

# All network sources except the standalone gvsoc test harness main.c.
APP_SRCS += $(filter-out %/main.c, $(wildcard $(NETWORK_DIR)/src/*.c))
APP_CFLAGS += -I$(NETWORK_DIR)/inc
APP_LDFLAGS += -lm

APP_CFLAGS += -DNUM_CORES=$(CORE)

# Stock-DORY code trips -Werror on an mchan.h macro redefinition and an implicit
# log2 in dory_dma.c. Relax -Werror until we regenerate with the IDSIA template.
# The macro redefinition has no specific -W name, so use blanket -Wno-error.
APP_CFLAGS += -Wno-error
APP_CFLAGS += -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -Wl,--print-memory-usage -flto
APP_CFLAGS += -DGAP_SDK=1

ifeq '$(FLASH_TYPE)' 'MRAM'
    READFS_FLASH = target/chip/soc/mram
endif

APP_CFLAGS += -DFLASH_TYPE=$(FLASH_TYPE) -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS

# Flat hex names, must match network.c's L3_weights_files. inputs.hex omitted;
# the deploy feeds the camera, not a file.
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
