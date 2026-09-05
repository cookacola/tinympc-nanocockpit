APP_DIR = $(CURDIR)/app
NETWORK_NAME ?= espnetv2-v8-full-int8
NETWORK_DIR = $(APP_DIR)/networks/$(NETWORK_NAME)
include $(NETWORK_DIR)/network.mk
