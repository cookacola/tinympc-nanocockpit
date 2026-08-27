# Generated STDC DORY package.
CORE ?= 7
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM
APP_SRCS += $(wildcard $(NETWORK_DIR)/src/*.c)
APP_CFLAGS += -I$(NETWORK_DIR)/inc
APP_CFLAGS += -DNUM_CORES=$(CORE) -DGAP8_MULTITASK_NETWORK=1
APP_CFLAGS += -DGAP8_STDC_PAIR_NETWORK=1
APP_CFLAGS += -DGAP8_STDC_SHARED_NETWORK=1
APP_CFLAGS += -Wno-error -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -lm -flto
APP_CFLAGS += -DGAP_SDK=1 -DFLASH_TYPE=$(FLASH_TYPE)
APP_CFLAGS += -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS -DFS_READ_FS
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution11_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution12_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution13_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution14_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution16_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution17_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution19_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution20_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution22_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution23_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution25_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution26_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution28_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution29_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution31_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution32_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution34_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution5_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution6_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution8_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_danger_head_BNReluConvolution9_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution4_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution5_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution6_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution8_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution9_weights.hex
READFS_FILES += $(FLASH_FILES)
