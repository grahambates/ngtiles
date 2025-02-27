# Detect OS
OS := $(shell uname -s)
PKG_CONFIG := pkg-config

# Target binary
TARGET := bin/ngtiles

# Libraries
LIBS := libpng

# Compiler settings
CC ?= gcc
CFLAGS := -Wall -Wextra -std=c11 -MMD -MP -Iinclude
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIBS))
LDLIBS := $(shell $(PKG_CONFIG) --libs $(LIBS)) -lm

# Source and object files
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c, build/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

# Windows-specific settings
ifeq ($(OS), Windows_NT)
    TARGET := bin/ngtiles.exe
    RM := del /Q
    MKDIR := if not exist build mkdir
    PKG_CONFIG := pkg-config --msvc-syntax
else
    RM := rm -f
    MKDIR := mkdir -p
endif

# Build rules
$(TARGET): $(OBJS)
	$(MKDIR) bin
	$(CC) $^ $(LDLIBS) -o $@

build/%.o: src/%.c
	$(MKDIR) build
	$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: clean
