CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99 -pedantic
CPPFLAGS += -Iinclude
LDFLAGS ?=
LDLIBS += -lm

# Detect environment prefix (e.g. Termux uses $PREFIX)
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1

SRCS = src/main.c src/terminal.c src/download.c src/render.c src/video.c
OBJS = $(SRCS:.c=.o)
TARGET = anif

.PHONY: all clean install uninstall fetch-ffmpeg test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c include/anif.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	@echo "Installing $(TARGET) to $(DESTDIR)$(BINDIR)..."
	@mkdir -p $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Installed successfully to $(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	@echo "Uninstalling $(TARGET) from $(DESTDIR)$(BINDIR)..."
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Uninstalled."

fetch-ffmpeg: $(TARGET)
	@./$(TARGET) --download-ffmpeg

prebuilt: $(TARGET)
	@mkdir -p bin
	@OS=$$(uname -s | tr '[:upper:]' '[:lower:]'); \
	ARCH=$$(uname -m); \
	if [ "$$OS" = "linux" ] && [ -n "$$PREFIX" ] && echo "$$PREFIX" | grep -q "com.termux"; then OS="android"; fi; \
	cp $(TARGET) bin/anif-$$OS-$$ARCH; \
	echo "Saved prebuilt binary to bin/anif-$$OS-$$ARCH"

clean:
	rm -f $(OBJS) $(TARGET)

distclean: clean
	rm -f bin/anif-*
