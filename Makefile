CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lcrypto -ljson-c
TARGET = cmdset
SOURCE = cmdset.c

SO_EXT = so
SHARED_LDFLAGS = -shared

UNAME_S := $(shell uname -s)

SHARED_TARGET = libcmdset.$(SO_EXT)

ifeq ($(UNAME_S),Darwin)
    ifneq ($(wildcard /opt/homebrew/opt/openssl@3/include),)
        CFLAGS += -I/opt/homebrew/opt/openssl@3/include
        LDFLAGS += -L/opt/homebrew/opt/openssl@3/lib
    else ifneq ($(wildcard /usr/local/opt/openssl/include),)
        CFLAGS += -I/usr/local/opt/openssl/include
        LDFLAGS += -L/usr/local/opt/openssl/lib
    endif
    ifneq ($(wildcard /opt/homebrew/Cellar/json-c),)
        CFLAGS += -I/opt/homebrew/Cellar/json-c/0.18/include
        LDFLAGS += -L/opt/homebrew/Cellar/json-c/0.18/lib
    endif
else ifeq ($(UNAME_S),Linux)
    ifneq ($(wildcard /usr/include/openssl),)
    else ifneq ($(wildcard /usr/local/include/openssl),)
        CFLAGS += -I/usr/local/include
        LDFLAGS += -L/usr/local/lib
    endif
	ifneq ($(wildcard /usr/include/json-c),)
		CFLAGS += -I/usr/include/json-c
		LDFLAGS += -L/usr/lib/json-c
	endif
else ifeq ($(OS),Windows_NT)
    CFLAGS += $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
    LDFLAGS += $(shell pkg-config --libs openssl 2>/dev/null || echo "-lcrypto")
    CFLAGS += $(shell pkg-config --cflags json-c 2>/dev/null || echo "")
    LDFLAGS += $(shell pkg-config --libs json-c 2>/dev/null || echo "-ljson-c")
endif
all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

shared: $(SHARED_TARGET)

$(SHARED_TARGET): $(SOURCE) cmdset.h
	$(CC) $(CFLAGS) -fPIC $(SHARED_LDFLAGS) -DCMDSET_BUILD_LIB -o $(SHARED_TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(SHARED_TARGET)

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
