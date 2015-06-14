PREFIX = /usr/local
CC = gcc

C_SRC = main.c

OBJECTS = $(C_SRC:.c=.o)
DEPS = $(OBJECTS:.o=.d)

INC = -I. -L/usr/local/lib -L/usr/lib
LIB = -lnanomsg

-include $(DEPS)
%.d : %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

# Assembler, compiler, and linker flags
PEDANTIC = -Wpedantic
CFLAGS  = -g -O0 $(INC) -Wall -Wextra -Werror -std=c99 -fPIC
LFLAGS  = $(CFLAGS)
#CFLAGS += $(PEDANTIC)

# Targets
.PHONY: all
all: flux-broker

.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(DEPS) flux-broker

flux-broker: $(OBJECTS)
	$(CC) $(LFLAGS) -o $@ $^ $(LIB)

.PHONY: install
install: flux-broker
	cp $< $(PREFIX)/bin/$<

.DEFAULT_GOAL := all
