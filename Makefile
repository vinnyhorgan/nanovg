CC ?= cc
AR ?= ar
BUILD_DIR ?= build
OBJ_DIR := $(BUILD_DIR)/obj
MODE ?= debug

UNAME_S := $(shell uname -s)

WARNINGS := -Wall -Wextra
CPPFLAGS := -Isrc -Iexample -Isokol -MMD -MP
CFLAGS := -std=c11 $(WARNINGS)
LDFLAGS :=
LDLIBS :=

ifeq ($(MODE),release)
CPPFLAGS += -DNDEBUG
CFLAGS += -O2
else
CPPFLAGS += -DDEBUG
CFLAGS += -O0 -g
endif

ifeq ($(UNAME_S),Linux)
	CPPFLAGS += -D_POSIX_C_SOURCE=200809L -DSOKOL_GLCORE
	LDLIBS += -lX11 -lXi -lXcursor -ldl -lpthread -lm -lGL
else ifeq ($(UNAME_S),Darwin)
$(error This Makefile currently supports Linux only)
else
$(error Unsupported platform: $(UNAME_S))
endif

LIB := $(BUILD_DIR)/libnanovg.a
EXAMPLE := $(BUILD_DIR)/example

LIB_SRCS := src/nanovg.c
LIB_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))

EXAMPLE_SRCS := example/example.c example/demo.c example/perf.c
EXAMPLE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(EXAMPLE_SRCS))

DEPS := $(LIB_OBJS:.o=.d) $(EXAMPLE_OBJS:.o=.d)

.PHONY: all lib example clean run

all: example

lib: $(LIB)

example: $(EXAMPLE)

run: $(EXAMPLE)
	$(EXAMPLE)

$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(EXAMPLE): $(EXAMPLE_OBJS) $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(EXAMPLE_OBJS) $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
