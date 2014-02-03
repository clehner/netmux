BIN = netmux
SRC = netmux.c
PREFIX ?= /usr/local

all: $(BIN)

$(BIN): $(SRC)

install: all
	cp -f $(BIN) $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clean:
	rm $(BIN)

.PHONY: all clean install uninstall
