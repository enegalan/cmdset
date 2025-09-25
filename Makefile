CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lcrypto
TARGET = cmdset
SOURCE = cmdset.c

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    ifneq ($(wildcard /opt/homebrew/opt/openssl@3/include),)
        CFLAGS += -I/opt/homebrew/opt/openssl@3/include
        LDFLAGS += -L/opt/homebrew/opt/openssl@3/lib
    else ifneq ($(wildcard /usr/local/opt/openssl/include),)
        CFLAGS += -I/usr/local/opt/openssl/include
        LDFLAGS += -L/usr/local/opt/openssl/lib
    endif
else ifeq ($(UNAME_S),Linux)
    ifneq ($(wildcard /usr/include/openssl),)
    else ifneq ($(wildcard /usr/local/include/openssl),)
        CFLAGS += -I/usr/local/include
        LDFLAGS += -L/usr/local/lib
    endif
else ifeq ($(OS),Windows_NT)
    CFLAGS += $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
    LDFLAGS += $(shell pkg-config --libs openssl 2>/dev/null || echo "-lcrypto")
endif
all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@echo "Installing cmdset globally..."
	@sudo ./setup.sh install

uninstall:
	@echo "Uninstalling cmdset..."
	@sudo ./setup.sh uninstall

status:
	@./setup.sh status

help:
	./$(TARGET) help

test: $(TARGET)
	@echo "Testing cmdset..."
	@echo "Adding test presets..."
	./$(TARGET) add test-echo "echo 'Hello from cmdset!'"
	./$(TARGET) add test-ls "ls -la"
	@echo "Listing presets..."
	./$(TARGET) list
	@echo "Executing test-echo..."
	./$(TARGET) exec test-echo
	@echo "Cleaning up test presets..."
	./$(TARGET) remove test-echo
	./$(TARGET) remove test-ls

usage: $(TARGET)
	./$(TARGET) help

.PHONY: all clean install uninstall help test usage
