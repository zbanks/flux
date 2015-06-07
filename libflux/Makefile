PREFIX = /usr/local
CC = gcc

#C_SRC = $(wildcard lib/*.c)
C_SRC = server.c client.c

OBJECTS = $(C_SRC:.c=.o)
DEPS = $(OBJECTS:.o=.d)

INC = -I. -L/usr/local/lib -L/usr/lib
LIB = -lnanomsg

-include $(DEPS)
%.d : %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

# Assembler, compiler, and linker flags
PEDANTIC = -Wpedantic
CFLAGS  = -g -O3 $(INC) -Wall -Wextra -Werror -std=c99 -fPIC
LFLAGS  = $(CFLAGS)
CFLAGS += $(PEDANTIC)

# Targets
.PHONY: all
all: libflux.so

.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(DEPS) libflux.so

libflux.so: $(OBJECTS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LIB)

.PHONY: install
install: libflux.so
	cp $< $(PREFIX)/lib/$<
	cp flux.h $(PREFIX)/include/flux.h
	chmod 0755 $(PREFIX)/lib/$<
	ldconfig

.DEFAULT_GOAL := all
