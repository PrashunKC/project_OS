# Pong Game Makefile

NBOS_SDK ?= ../..
include $(NBOS_SDK)/tools/nbos.mk

TARGET = pong.bin
SOURCES = pong.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(NBOS_CC) -o $@ $^

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
