CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c99
TARGET  = aria-safety
SRC     = aria_safety.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: clean install
