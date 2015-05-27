CC = gcc

# Files to include
C_SRC  = $(wildcard src/*.c)
C_SRC += $(wildcard lib/lux/src/*.c)

C_INC  = $(wildcard src/*.h)
C_INC += $(wildcard lib/lux/inc/*.h)

OBJECTS = $(patsubst %.c,%.o,$(C_SRC))
DEPS = $(OBJECTS:.o=.d)

INC  = -I. -Isrc/ -Ilib/lux/inc -Ilib -L/usr/local/lib -L/usr/lib 
LIB  = -lczmq

# Assembler, compiler, and linker flags
CFLAGS  = -g -O0 $(INC) -Wall -Wextra -Werror -DLUX_WRITE_ONLY -std=c99
LFLAGS  = $(CXXFLAGS)

-include $(DEPS)
%.d : %.c
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.d : %.cpp
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@

# Targets
.PHONY: all
all: flux

.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(DEPS) flux

flux: $(OBJECTS)
	$(CC) $(LFLAGS) -g -o flux $(OBJECTS) $(LIB)


#%.o: %.c $(C_INC)
#	gcc $(CFLAGS) -std=c99 -c -o $@ $<

.DEFAULT_GOAL := flux
