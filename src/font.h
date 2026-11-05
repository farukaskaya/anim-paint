#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <SDL2/SDL.h>

/* Embedded classic 8x8 bitmap font (ASCII 32 to 127) */
extern const uint8_t font8x8_basic[96][8];

/* Draw a single character at (x, y) with color */
void font_draw_char(SDL_Renderer *renderer, char c, int x, int y, SDL_Color color, int scale);

/* Draw a string at (x, y) with color */
void font_draw_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, int scale);

/* Measure text width in pixels */
int font_text_width(const char *text, int scale);

#endif /* FONT_H */
