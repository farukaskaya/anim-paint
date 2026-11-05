#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define MAX_UNDO_LEVELS 30
#define NUM_LAYERS 2
#define LAYER_BG 0
#define LAYER_ANIM 1

/* A single frame of the animation with 2 layers */
typedef struct {
    int width;
    int height;
    /* Layer 0: Background (RGB / RGBA)
       Layer 1: Animation (RGBA with transparency) */
    uint32_t *layers[NUM_LAYERS];
    
    /* Thumbnail texture cache */
    SDL_Texture *thumb_texture;
    bool thumb_dirty;
} Frame;

/* Undo snapshot */
typedef struct {
    int frame_index;
    int layer_index;
    uint32_t *pixels;
    int width;
    int height;
} UndoStep;

/* Main Animation Canvas & Project */
typedef struct {
    int width;
    int height;
    
    Frame **frames;
    int frame_count;
    int frame_capacity;
    int current_frame;
    
    int active_layer;          /* LAYER_BG (0) or LAYER_ANIM (1) */
    bool layer_visible[NUM_LAYERS];
    
    /* Onion Skinning */
    bool onion_skin_enabled;
    int onion_prev_frames;     /* Number of previous frames to show (1 or 2) */
    int onion_next_frames;     /* Number of next frames to show (0 or 1) */
    float onion_opacity;       /* e.g. 0.35f */
    bool onion_tint_mode;      /* Tint prev frames red/orange, next frames blue */

    /* Playback */
    bool is_playing;
    int fps;                   /* Default 12, range 1 - 30 */
    bool loop;                 /* Loop animation */
    uint32_t play_accum_ms;    /* Milliseconds accumulator for frame advance */

    /* Undo / Redo stacks */
    UndoStep undo_stack[MAX_UNDO_LEVELS];
    int undo_count;
    UndoStep redo_stack[MAX_UNDO_LEVELS];
    int redo_count;

    /* Selection Buffer (for cut/copy/paste/move) */
    uint32_t *selection_pixels;
    int sel_x, sel_y, sel_w, sel_h;
    bool has_selection;
    bool selection_floating;   /* True if currently being dragged/moved */
    bool transparent_selection;/* Classic MS Paint feature: secondary color is key/transparent */

    /* Composited preview buffer for rendering */
    uint32_t *composite_buffer;
    SDL_Texture *canvas_texture;
    bool canvas_dirty;

} CanvasProject;

/* RGBA helper utilities */
static inline uint32_t make_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

static inline void unpack_rgba(uint32_t color, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a) {
    *r = (uint8_t)(color & 0xFF);
    *g = (uint8_t)((color >> 8) & 0xFF);
    *b = (uint8_t)((color >> 16) & 0xFF);
    *a = (uint8_t)((color >> 24) & 0xFF);
}

/* Lifecycle */
CanvasProject *canvas_create(int width, int height);
void canvas_destroy(CanvasProject *proj);

/* Frame management */
int canvas_add_frame(CanvasProject *proj, int insert_at_index);
int canvas_duplicate_frame(CanvasProject *proj, int source_index);
bool canvas_delete_frame(CanvasProject *proj, int index);
void canvas_set_current_frame(CanvasProject *proj, int index);
void canvas_move_frame(CanvasProject *proj, int from_index, int to_index);

/* Layer management */
void canvas_set_active_layer(CanvasProject *proj, int layer);
void canvas_toggle_active_layer(CanvasProject *proj);

/* Undo / Redo */
void canvas_push_undo(CanvasProject *proj);
void canvas_undo(CanvasProject *proj);
void canvas_redo(CanvasProject *proj);
void canvas_clear_redo(CanvasProject *proj);

/* Pixel access */
uint32_t canvas_get_pixel(CanvasProject *proj, int frame_idx, int layer_idx, int x, int y);
void canvas_set_pixel(CanvasProject *proj, int frame_idx, int layer_idx, int x, int y, uint32_t color);

/* Rendering & Compositing */
void canvas_update_composite(CanvasProject *proj, SDL_Renderer *renderer);
void canvas_update_thumbnail(CanvasProject *proj, int frame_idx, SDL_Renderer *renderer);

/* Clear layer or frame */
void canvas_clear_layer(CanvasProject *proj, int frame_idx, int layer_idx, uint32_t color);

#endif /* CANVAS_H */
