#
# demo.mk
#
# Copyright (C) 2011 Creytiv.com
#

DEMO		:= tcp_server
$(DEMO)_SRCS	+= tcp_server.c
$(DEMO)_LFLAGS	+=

include mk/dmo.mk
