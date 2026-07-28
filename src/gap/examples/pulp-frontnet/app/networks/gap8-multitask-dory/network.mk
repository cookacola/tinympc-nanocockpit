# Generated NanoCockpit DORY network package.
CORE ?= 7
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM

APP_SRCS += $(wildcard $(NETWORK_DIR)/src/*.c)
APP_CFLAGS += -I$(NETWORK_DIR)/inc
APP_CFLAGS += -DNUM_CORES=$(CORE) -DGAP8_MULTITASK_NETWORK=1
APP_CFLAGS += -Wno-error -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -lm -flto
APP_CFLAGS += -DGAP_SDK=1
APP_CFLAGS += -DFLASH_TYPE=$(FLASH_TYPE) -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS -DFS_READ_FS

FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution10_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution12_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution13_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution15_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution16_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution18_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution19_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution21_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution22_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution24_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution25_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution27_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution28_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution30_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution31_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution33_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution34_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution36_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution37_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution39_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution40_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution42_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution43_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution45_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution46_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution47_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution4_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution5_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution6_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution8_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/gap8_BNReluConvolution9_weights.hex
READFS_FILES += $(FLASH_FILES)
