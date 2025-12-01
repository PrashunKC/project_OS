# Hello World Example Makefile

NBOS_SDK ?= ../..
include $(NBOS_SDK)/tools/nbos.mk

TARGET = hello.bin
SOURCES = hello.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(NBOS_CC) -o $@ $^

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
