CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 $(shell pkg-config --cflags sdl2)
LIBS ?= $(shell pkg-config --libs sdl2) -lm

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

.PHONY: all clean run
