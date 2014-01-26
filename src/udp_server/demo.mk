#
# demo.mk
#
# Copyright (C) 2011 Creytiv.com
#

DEMO		:= udp_server
$(DEMO)_SRCS	+= udp_server.c
$(DEMO)_LFLAGS	+=

include mk/dmo.mk
