CC = gcc

# Files to include
C_SRC_COMMON  = $(wildcard core/*.c)
C_SRC_COMMON += $(wildcard lib/*.c)

C_SRC_BROKER = $(wildcard broker/*.c)

C_SRC_SERVER = $(wildcard server/*.c)
C_SRC_SERVER += $(wildcard serial/*.c)
C_SRC_SERVER += $(wildcard lib/lux/src/*.c)

C_SRC_CLIENT = $(wildcard client/*.c)

C_SRC = $(C_SRC_COMMON) $(C_SRC_BROKER) $(C_SRC_SERVER) $(C_SRC_CLIENT)
OBJECTS_COMMON = $(patsubst %.c,%.o,$(C_SRC_COMMON))
OBJECTS_BROKER = $(patsubst %.c,%.o,$(C_SRC_BROKER))
OBJECTS_SERVER = $(patsubst %.c,%.o,$(C_SRC_SERVER))
OBJECTS_CLIENT = $(patsubst %.c,%.o,$(C_SRC_CLIENT))
OBJECTS_LIB = $(patsubst %.c,%.o,$(C_SRC_LIB))
OBJECTS_COMMON += $(OBJECTS_LIB)
OBJECTS_ALL = $(OBJECTS_COMMON) $(OBJECTS_BROKER) $(OBJECTS_SERVER) $(OBJECTS_CLIENT) 
DEPS = $(OBJECTS_ALL:.o=.d)

INC  = -I. -Isrc/ -Ilib/lux/inc -Ilib -L/usr/local/lib -L/usr/lib -Llibflux
LIB  = -lczmq -lzmq -lflux

# Assembler, compiler, and linker flags
PEDANTIC = -Wpedantic
CFLAGS  = -g -O0 $(INC) -Wall -Wextra -Werror -std=c99  -DLUX_WRITE_ONLY
LFLAGS  = $(CFLAGS)

#CFLAGS += $(PEDANTIC)

-include $(DEPS)
%.d : %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

# Targets
.PHONY: all
all: libflux flux-server flux-broker flux-client

.PHONY: clean
clean:
	-rm -f $(OBJECTS_ALL) $(DEPS) flux-server flux-broker flux-client

.PHONY: libflux
	$(MAKE) -C libflux

flux-server: $(OBJECTS_COMMON) $(OBJECTS_SERVER)
	$(CC) $(LFLAGS) -o $@ $^ $(LIB)

flux-broker: $(OBJECTS_COMMON) $(OBJECTS_BROKER)
	$(CC) $(LFLAGS) -o $@ $^ $(LIB)

flux-client: $(OBJECTS_COMMON) $(OBJECTS_CLIENT)
	$(CC) $(LFLAGS) -o $@ $^ $(LIB)

#%.o: %.c $(C_INC)
#	gcc $(CFLAGS) -std=c99 -c -o $@ $<

.DEFAULT_GOAL := all
