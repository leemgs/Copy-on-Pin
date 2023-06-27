CC = gcc
CFLAGS = -O2 -MMD -MP
CFLAGS += -Werror -Wall -Wextra
CFLAGS += -Wmissing-declarations -Wredundant-decls -Wpointer-arith
CFLAGS += -Wwrite-strings -Wswitch-default -Wmissing-include-dirs -Wundef

SRC = $(wildcard *.c)
DEP = $(SRC:.c=.d)
OBJ = $(SRC:.c=.o)

.PHONY: all
all: stream

stream: $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -rf stream $(OBJ) $(DEP)

-include $(DEP)
