CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 $(shell pkg-config --cflags sdl2)
LIBS ?= $(shell pkg-config --libs sdl2) -lm
VERSION ?= 1.0.0

SRC = src/main.c src/canvas.c src/tools.c src/ui.c src/font.c src/export.c
OBJ = $(SRC:src/%.c=build/%.o)
TARGET = animpaint

all: build $(TARGET)

build:
	mkdir -p build

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

clean:
	rm -rf build $(TARGET) output.mp4 output.gif project.anp

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	$(CC) $(CFLAGS) tests/test_anim.c src/canvas.c src/tools.c src/font.c src/export.c -o tests/test_anim $(LIBS)
	cd tests && ./test_anim

appimage: $(TARGET)
	VERSION=$(VERSION) packaging/build-appimage.sh

windows:
	VERSION=$(VERSION) packaging/build-windows.sh

.PHONY: all clean run test appimage windows
