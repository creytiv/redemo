#
# demo.mk
#
# Copyright (C) 2011 Creytiv.com
#

DEMO		:= websocket_server
$(DEMO)_SRCS	+= websocket_server.c
$(DEMO)_LFLAGS	+=

include mk/dmo.mk
