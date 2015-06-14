PREFIX = /usr/local
CC = gcc

# Files to include
C_SRC_LUXSERVER = $(wildcard servers/*.c)
C_SRC_LUXSERVER += $(wildcard lib/serial/*.c)
C_SRC_LUXSERVER += $(wildcard lib/crc/*.c)
C_SRC_LUXSERVER += $(wildcard lib/lux/src/*.c)

C_SRC_CLIENT = $(wildcard client-example/*.c)

C_SRC = $(C_SRC_LUXSERVER) $(C_SRC_CLIENT)

OBJECTS_LUXSERVER = $(C_SRC_LUXSERVER:.c=.o)
OBJECTS_CLIENT = $(C_SRC_CLIENT:.c=.o)

OBJECTS_ALL = $(C_SRC:.c=.o)
DEPS = $(OBJECTS_ALL:.o=.d)

INC  = -I. -Ilibflux -Llibflux -L/usr/local/lib -L/usr/lib
LIB  = -lflux -lnanomsg

# Assembler, compiler, and linker flags
PEDANTIC = -Wpedantic
CFLAGS  = -g -O0 $(INC) -Wall -Wextra -Werror -std=gnu99
LFLAGS  = $(CFLAGS)
#CFLAGS += $(PEDANTIC)

-include $(DEPS)
%.d : %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

BINARIES = flux-broker flux-lux-server flux-example-client

# Targets
.PHONY: all
all: libflux $(BINARIES)

.PHONY: clean
clean:
	-$(MAKE) -C libflux clean
	-$(MAKE) -C broker clean
	-rm -f $(OBJECTS_ALL) $(DEPS) $(BINARIES)

.PHONY: install
install: libflux flux-broker install-flux-lux-server
	$(MAKE) -C libflux install
	$(MAKE) -C broker install

.PHONY: install-flux-lux-server
install-flux-lux-server: flux-lux-server
	cp $< $(PREFIX)/bin/$<

.PHONY: libflux
libflux:
	$(MAKE) -C libflux

.PHONY: flux-broker
flux-broker:
	$(MAKE) -C broker
	cp broker/flux-broker flux-broker

flux-lux-server: libflux $(OBJECTS_LUXSERVER)
	$(CC) $(LFLAGS) -o $@ $(OBJECTS_LUXSERVER) $(LIB)

flux-example-client: libflux $(OBJECTS_CLIENT)
	$(CC) $(LFLAGS) -o $@ $(OBJECTS_CLIENT) $(LIB)

#%.o: %.c $(C_INC)
#	gcc $(CFLAGS) -std=c99 -c -o $@ $<

.DEFAULT_GOAL := all
