CC = gcc

# Files to include
C_SRC_COMMON  = $(wildcard core/*.c)
C_SRC_COMMON += $(wildcard lib/*.c)

C_SRC_SERVER = $(wildcard server/*.c)
C_SRC_SERVER += $(wildcard serial/*.c)
C_SRC_SERVER += $(wildcard lib/lux/src/*.c)

C_SRC_CLIENT = $(wildcard client/*.c)

C_SRC = $(C_SRC_COMMON) $(C_SRC_SERVER) $(C_SRC_CLIENT)

C_SRC_SERVER += $(C_SRC_COMMON)
C_SRC_CLIENT += $(C_SRC_COMMON)

OBJECTS_SERVER = $(patsubst %.c,%.o,$(C_SRC_SERVER)) 
OBJECTS_CLIENT = $(patsubst %.c,%.o,$(C_SRC_CLIENT))

OBJECTS_ALL = $(C_SRC:.c=.o)
DEPS = $(OBJECTS_ALL:.o=.d)

INC  = -I.  -Ilib/lux/inc -Ilibflux -Llibflux -L/usr/local/lib -L/usr/lib
LIB  = -lflux -lnanomsg

# Assembler, compiler, and linker flags
PEDANTIC = -Wpedantic
CFLAGS  = -g -O0 $(INC) -Wall -Wextra -Werror -std=gnu99 # -DLUX_WRITE_ONLY
LFLAGS  = $(CFLAGS)
CFLAGS += $(PEDANTIC)

-include $(DEPS)
%.d : %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

# Targets
.PHONY: all
all: libflux flux-broker flux-server flux-client

.PHONY: clean
clean:
	-$(MAKE) -C libflux clean
	-$(MAKE) -C broker clean
	-rm -f $(OBJECTS_ALL) $(DEPS) flux-server flux-broker flux-client

.PHONY: install
install:
	$(MAKE) -C libflux install
	$(MAKE) -C broker install

.PHONY: libflux
libflux:
	$(MAKE) -C libflux

.PHONY: flux-broker
flux-broker:
	$(MAKE) -C broker

flux-server: libflux $(OBJECTS_SERVER)
	$(CC) $(LFLAGS) -o $@ $(OBJECTS_SERVER) $(LIB)

flux-client: libflux $(OBJECTS_CLIENT)
	$(CC) $(LFLAGS) -o $@ $(OBJECTS_CLIENT) $(LIB)

#%.o: %.c $(C_INC)
#	gcc $(CFLAGS) -std=c99 -c -o $@ $<

.DEFAULT_GOAL := all
