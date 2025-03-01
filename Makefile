# Detect OS
OS := $(shell uname -s)
PKG_CONFIG := pkg-config

# Target binary
TARGET := bin/ngtiles
TEST_TARGET := bin/tests

# Libraries
LIBS := libpng

# Compiler settings
CC ?= gcc
CFLAGS := -Wall -Wextra -std=c11 -MMD -MP -Iinclude -Ilib
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIBS))
LDLIBS := $(shell $(PKG_CONFIG) --libs $(LIBS)) -lm

# Source and object files
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c, build/%.o, $(SRCS)) lib/xxhash.o
DEPS := $(OBJS:.o=.d)

# Test source and object files
TEST_SRCS := $(wildcard tests/*.c)
TEST_OBJS := $(patsubst tests/%.c, build/tests/%.o, $(TEST_SRCS)) lib/unity.o
TEST_DEPS := $(TEST_OBJS:.o=.d)

# Windows-specific settings
ifeq ($(OS), Windows_NT)
	TARGET := bin/ngtiles.exe
	TEST_TARGET := bin/tests.exe
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

$(TEST_TARGET): $(filter-out build/main.o, $(OBJS)) $(TEST_OBJS)
	$(MKDIR) bin
	$(CC) $^ $(LDLIBS) -o $@

build/%.o: src/%.c
	$(MKDIR) build
	$(CC) $(CFLAGS) -c $< -o $@

build/tests/%.o: tests/%.c
	$(MKDIR) build/tests
	$(CC) $(CFLAGS) -c $< -o $@

build/unity.o: lib/unity/unity.c
	$(MKDIR) build
	$(CC) $(CFLAGS) -c $< -o $@

# Test rules
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Cleanup
clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)
	$(RM) $(TEST_OBJS) $(TEST_DEPS) $(TEST_TARGET)

-include $(DEPS)
-include $(TEST_DEPS)

.PHONY: clean test
