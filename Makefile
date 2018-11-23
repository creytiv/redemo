#
# Makefile
#
# Copyright (C) 2011 Creytiv.com
#

PROJECT   := redemo
VERSION   := 0.6.0

DEMOS	  += sip_ua
DEMOS	  += tcp_server
DEMOS	  += udp_server
DEMOS	  += websocket_server


LIBRE_MK  := $(shell [ -f ../re/mk/re.mk ] && \
	echo "../re/mk/re.mk")
ifeq ($(LIBRE_MK),)
LIBRE_MK  := $(shell [ -f /usr/share/re/re.mk ] && \
	echo "/usr/share/re/re.mk")
endif
ifeq ($(LIBRE_MK),)
LIBRE_MK  := $(shell [ -f /usr/local/share/re/re.mk ] && \
	echo "/usr/local/share/re/re.mk")
endif

include $(LIBRE_MK)

CFLAGS	+= -I$(LIBRE_INC)


DEMO_MK  := $(patsubst %,src/%/demo.mk,$(DEMOS))
DEMO_BLD := $(patsubst %,$(BUILD)/%,$(DEMOS))
BINS     := $(patsubst %,%$(BIN_SUFFIX),$(DEMOS))

include $(DEMO_MK)


all: $(BINS)

$(BUILD): Makefile
	@mkdir -p $(DEMO_BLD)
	@touch $@

clean:
	@rm -rf $(BINS) $(BUILD)
