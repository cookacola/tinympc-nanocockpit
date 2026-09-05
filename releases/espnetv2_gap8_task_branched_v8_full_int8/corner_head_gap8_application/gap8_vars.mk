FLASH_FILES += hex/gap8_ReluConvolution0_weights.hex
FLASH_FILES += hex/gap8_ReluConvolution1_weights.hex
FLASH_FILES += hex/gap8_ReluConvolution2_weights.hex
FLASH_FILES += hex/gap8_ReluConvolution3_weights.hex
FLASH_FILES += hex/gap8_ReluConvolution4_weights.hex
FLASH_FILES += hex/gap8_inputs.hex

READFS_FILES := $(FLASH_FILES)
APP_CFLAGS += -DFS_READ_FS
#PLPBRIDGE_FLAGS += -f