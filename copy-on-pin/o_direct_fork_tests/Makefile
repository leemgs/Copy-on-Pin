CFLAGS += -MMD -MP -O2 -Wall
LDFLAGS += -lpthread

EXECS := test1
EXECS += test2
EXECS += test3

OBJS := test1.o
OBJS += test2.o
OBJS += test3.o
DEPS := $(OBJS:.o=.d)

.PHONY: default
default: $(EXECS)

test1: test1.o

test2: test2.o

test3: test3.o

.PHONY: clean
clean:
	rm -rf $(DEPS) $(OBJS) $(EXECS)

-include $(DEPS)
