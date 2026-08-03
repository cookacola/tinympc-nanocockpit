# Generated sequential QAT DORY network adapter for Tiny Racer.
CORE ?= 8
FLASH_TYPE ?= HYPERFLASH
RAM_TYPE ?= HYPERRAM

# These sources belong to DORY's standalone checksum application; Tiny Racer
# supplies its own application main and receives camera input directly.
APP_SRCS += $(filter-out $(NETWORK_DIR)/src/gap8_main.c $(NETWORK_DIR)/src/gap8_checksum_input.c,$(wildcard $(NETWORK_DIR)/src/*.c))
APP_CFLAGS += -I$(NETWORK_DIR)/inc
APP_CFLAGS += -DNUM_CORES=$(CORE) -DGAP8_SEQUENTIAL_NETWORK=1
APP_CFLAGS += -Wno-error -O2 -fno-indirect-inlining -flto
APP_LDFLAGS += -lm -flto
APP_CFLAGS += -DGAP_SDK=1 -DFLASH_TYPE=$(FLASH_TYPE)
APP_CFLAGS += -DUSE_$(FLASH_TYPE) -DUSE_$(RAM_TYPE)
APP_CFLAGS += -DALWAYS_BLOCK_DMA_TRANSFERS -DFS_READ_FS
FLASH_FILES += $(wildcard $(NETWORK_DIR)/hex/*)
READFS_FILES += $(FLASH_FILES)
