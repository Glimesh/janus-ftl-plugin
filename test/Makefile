JANUS_PATH=/opt/janus
CC=g++
RM=rm -f
CFLAGS=-g -Wall -Wno-unknown-pragmas -fPIC $(shell pkg-config --cflags criterion glib-2.0 openssl jansson) -I../ -I$(JANUS_PATH)/include/janus
LDFLAGS=-g -fPIC
LDLIBS=$(shell pkg-config --libs criterion glib-2.0 openssl jansson) -Wl,-R -Wl,../.libs -L../.libs -ljanus_ftl -L$(JANUS_PATH)/lib

SRCS=JanusFtlTest.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: test

test: $(OBJS)
	$(CC) $(LDFLAGS) -o test $(OBJS) $(LDLIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean: 
	rm -Rf $(OBJS) test