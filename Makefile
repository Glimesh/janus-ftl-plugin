JANUS_PATH=/opt/janus
CC=gcc
RM=rm -f
CFLAGS=-Wall -fPIC $(shell pkg-config --cflags glib-2.0 libsrtp2) -I$(JANUS_PATH)/include/janus -DHAVE_SRTP_2=1
LDFLAGS=-fPIC
LDLIBS=$(shell pkg-config --libs glib-2.0) -L$(JANUS_PATH)/lib
DESTINATION_PATH=$(JANUS_PATH)/lib/janus/plugins

SRCS=janus_ftl.c
OBJS=$(subst .c,.lo,$(SRCS))

all: libjanus_ftl.la

libjanus_ftl.la: $(OBJS)
	libtool --mode=link $(CC) $(LDFLAGS) -o libjanus_ftl.la $(OBJS) $(LDLIBS) -rpath $(DESTINATION_PATH)

%.lo: %.c
	libtool --mode=compile $(CC) $(CFLAGS) -c $<

install: libjanus_ftl.la
	libtool --mode=install install -m 644 libjanus_ftl.la $(DESTINATION_PATH)/libjanus_ftl.la