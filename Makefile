CC ?= gcc
VERSION ?= 1.0.0

PKG_CONFIG ?= pkg-config
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)

# Fallback for environments without pkg-config (e.g. some MinGW setups).
ifeq ($(strip $(SDL_LIBS)),)
  ifeq ($(OS),Windows_NT)
    SDL_CFLAGS := -IC:/msys64/mingw64/include/SDL2
    SDL_LIBS := -lmingw32 -lSDL2main -lSDL2 -mwindows
  else
    SDL_LIBS := -lSDL2
  endif
endif

CFLAGS ?= -Wall -Wextra -O2 -std=c99 $(SDL_CFLAGS)
LIBS ?= $(SDL_LIBS) -lm

SRC = src/main.c src/canvas.c src/tools.c src/ui.c src/font.c src/export.c
OBJ = $(SRC:src/%.c=build/%.o)
TARGET = farukpaint

all: build $(TARGET)

build:
	mkdir -p build

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

clean:
	rm -rf build $(TARGET) output.mp4 output.gif project.fpaint

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	$(CC) $(CFLAGS) tests/test_farukpaint.c src/canvas.c src/tools.c src/font.c src/export.c -o tests/test_farukpaint $(LIBS)
	cd tests && ./test_farukpaint

appimage: $(TARGET)
	VERSION=$(VERSION) packaging/build-appimage.sh

windows:
	VERSION=$(VERSION) packaging/build-windows.sh

.PHONY: all clean run test appimage windows
