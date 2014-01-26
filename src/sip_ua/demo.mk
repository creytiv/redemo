#
# demo.mk
#
# Copyright (C) 2011 Creytiv.com
#

DEMO		:= sip_ua
$(DEMO)_SRCS	+= sip_ua.c
$(DEMO)_LFLAGS	+=

include mk/dmo.mk
