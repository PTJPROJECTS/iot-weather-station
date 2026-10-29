# Host build for the shared C code and its tests.
# Firmware itself is built with PlatformIO (see README).

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O1 -g
CFLAGS  += -Ilib/wx_core
LDLIBS  += -lm

BUILD   := build

.PHONY: all test clean

all: $(BUILD)/test_wx_core

$(BUILD):
	mkdir -p $@

$(BUILD)/test_wx_core: host_tests/test_main.c lib/wx_core/wx_core.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: $(BUILD)/test_wx_core
	./$(BUILD)/test_wx_core

clean:
	rm -rf $(BUILD)
