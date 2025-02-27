CC ?= gcc
PKG_CONFIG := pkg-config

LIBS := libpng
CFLAGS := -Wall -Wextra -std=c11 -MMD -MP
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIBS))
LDLIBS := $(shell $(PKG_CONFIG) --libs $(LIBS))

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

TARGET := ngtiles

# Build Rules

$(TARGET): $(OBJS)
	$(CC) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: clean
