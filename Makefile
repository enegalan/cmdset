CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = cmdset
SOURCE = cmdset.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

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
