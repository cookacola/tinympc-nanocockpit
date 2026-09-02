FLASH_FILES += hex/gap8_BNReluConvolution0_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution1_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution2_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution3_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution4_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution5_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution6_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution8_weights.hex
FLASH_FILES += hex/gap8_BNReluConvolution9_weights.hex
FLASH_FILES += hex/gap8_inputs.hex

READFS_FILES := $(FLASH_FILES)
APP_CFLAGS += -DFS_READ_FS
#PLPBRIDGE_FLAGS += -f