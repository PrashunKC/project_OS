# Graphics Demo Makefile

NBOS_SDK ?= ../..
include $(NBOS_SDK)/tools/nbos.mk

TARGET = demo.bin
SOURCES = demo.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(NBOS_CC) -o $@ $^

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
