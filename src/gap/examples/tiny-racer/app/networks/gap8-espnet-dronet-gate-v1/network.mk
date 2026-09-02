# Generated ESPNet/DroNet/gate DORY package.
CORE ?= 8
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM
APP_SRCS += $(wildcard $(NETWORK_DIR)/src/*.c)
APP_CFLAGS += -I$(NETWORK_DIR)/inc -DNUM_CORES=$(CORE)
APP_CFLAGS += -DGAP8_MULTITASK_NETWORK=1 -DGAP8_TEMPORAL_NETWORK=1
APP_CFLAGS += -DGAP8_ESPNET_DRONET_GATE=1 -Wno-error -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -lm -flto
APP_CFLAGS += -DGAP_SDK=1 -DFLASH_TYPE=$(FLASH_TYPE)
APP_CFLAGS += -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS -DFS_READ_FS
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_corner_head_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution11_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution12_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution13_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution14_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution16_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution17_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution19_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution20_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution4_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution5_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution6_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution8_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_encoder_BNReluConvolution9_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_gate_head_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_gate_head_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_gate_head_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_gate_head_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution0_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution11_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution12_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution14_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution15_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution17_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution18_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution1_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution20_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution21_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution23_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution2_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution3_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution5_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution6_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution8_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_navigation_head_BNReluConvolution9_weights.hex
FLASH_FILES += $(NETWORK_DIR)/hex/stdc_presence_head_BNReluConvolution0_weights.hex
READFS_FILES += $(FLASH_FILES)
