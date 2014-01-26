#
# dmo.mk
#
# Copyright (C) 2011 Creytiv.com
#

$(DEMO)_OBJS     := $(patsubst %.c,$(BUILD)/$(DEMO)/%.o,$($(DEMO)_SRCS))

-include $($(DEMO)_OBJS:.o=.d)

$(DEMO)$(BIN_SUFFIX): $($(DEMO)_OBJS)
	@echo "  LD      $@"
	@$(LD) $(LFLAGS) $($(basename $@)_OBJS) \
		$($(basename $@)_LFLAGS) -L$(LIBRE_SO) -lre $(LIBS) -o $@

$(BUILD)/$(DEMO)/%.o: src/$(DEMO)/%.c $(BUILD) Makefile mk/dmo.mk \
				src/$(DEMO)/demo.mk
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c $< -o $@ $(DFLAGS)
