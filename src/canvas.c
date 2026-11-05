#include "canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Frame *frame_create(int width, int height) {
    Frame *f = (Frame *)calloc(1, sizeof(Frame));
    if (!f) return NULL;
    f->width = width;
    f->height = height;

    /* Layer 0: Background (solid white default) */
    f->layers[LAYER_BG] = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (f->layers[LAYER_BG]) {
        uint32_t white = make_rgba(255, 255, 255, 255);
        for (int i = 0; i < width * height; i++) {
            f->layers[LAYER_BG][i] = white;
        }
    }

    /* Layer 1: Animation (transparent default) */
    f->layers[LAYER_ANIM] = (uint32_t *)calloc(width * height, sizeof(uint32_t));

    f->thumb_texture = NULL;
    f->thumb_dirty = true;
    return f;
}

static void frame_destroy(Frame *f) {
    if (!f) return;
    if (f->layers[LAYER_BG]) free(f->layers[LAYER_BG]);
    if (f->layers[LAYER_ANIM]) free(f->layers[LAYER_ANIM]);
    if (f->thumb_texture) SDL_DestroyTexture(f->thumb_texture);
    free(f);
}

CanvasProject *canvas_create(int width, int height) {
    CanvasProject *proj = (CanvasProject *)calloc(1, sizeof(CanvasProject));
    if (!proj) return NULL;

    proj->width = width;
    proj->height = height;
    proj->frame_capacity = 16;
    proj->frames = (Frame **)malloc(sizeof(Frame *) * proj->frame_capacity);
    proj->frame_count = 0;

    /* Add initial frame */
    Frame *initial = frame_create(width, height);
    proj->frames[proj->frame_count++] = initial;
    proj->current_frame = 0;

    proj->active_layer = LAYER_ANIM; /* Default active is animation layer */
    proj->layer_visible[LAYER_BG] = true;
    proj->layer_visible[LAYER_ANIM] = true;

    proj->onion_skin_enabled = true;
    proj->onion_prev_frames = 1;
    proj->onion_next_frames = 1;
    proj->onion_opacity = 0.38f;
    proj->onion_tint_mode = true; /* Red for past, Blue for future */

    proj->is_playing = false;
    proj->fps = 12;
    proj->loop = true;
    proj->play_accum_ms = 0;

    proj->undo_count = 0;
    proj->redo_count = 0;

    proj->selection_pixels = NULL;
    proj->has_selection = false;
    proj->selection_floating = false;
    proj->transparent_selection = true;

    proj->composite_buffer = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    proj->canvas_texture = NULL;
    proj->canvas_dirty = true;

    return proj;
}

void canvas_destroy(CanvasProject *proj) {
    if (!proj) return;
    for (int i = 0; i < proj->frame_count; i++) {
        frame_destroy(proj->frames[i]);
    }
    free(proj->frames);

    for (int i = 0; i < proj->undo_count; i++) {
        free(proj->undo_stack[i].pixels);
    }
    for (int i = 0; i < proj->redo_count; i++) {
        free(proj->redo_stack[i].pixels);
    }

    if (proj->selection_pixels) free(proj->selection_pixels);
    if (proj->selection_mask) free(proj->selection_mask);
    if (proj->composite_buffer) free(proj->composite_buffer);
    if (proj->canvas_texture) SDL_DestroyTexture(proj->canvas_texture);
    free(proj);
}

int canvas_add_frame(CanvasProject *proj, int insert_at_index) {
    if (proj->frame_count >= proj->frame_capacity) {
        proj->frame_capacity *= 2;
        proj->frames = (Frame **)realloc(proj->frames, sizeof(Frame *) * proj->frame_capacity);
    }

    Frame *f = frame_create(proj->width, proj->height);

    if (insert_at_index < 0 || insert_at_index >= proj->frame_count) {
        insert_at_index = proj->frame_count;
        proj->frames[insert_at_index] = f;
    } else {
        for (int i = proj->frame_count; i > insert_at_index; i--) {
            proj->frames[i] = proj->frames[i - 1];
        }
        proj->frames[insert_at_index] = f;
    }

    proj->frame_count++;
    proj->current_frame = insert_at_index;
    proj->canvas_dirty = true;
    return insert_at_index;
}

int canvas_duplicate_frame(CanvasProject *proj, int source_index) {
    if (source_index < 0 || source_index >= proj->frame_count) return -1;
    if (proj->frame_count >= proj->frame_capacity) {
        proj->frame_capacity *= 2;
        proj->frames = (Frame **)realloc(proj->frames, sizeof(Frame *) * proj->frame_capacity);
    }

    Frame *src = proj->frames[source_index];
    Frame *dst = frame_create(proj->width, proj->height);
    memcpy(dst->layers[LAYER_BG], src->layers[LAYER_BG], proj->width * proj->height * sizeof(uint32_t));
    memcpy(dst->layers[LAYER_ANIM], src->layers[LAYER_ANIM], proj->width * proj->height * sizeof(uint32_t));

    int insert_pos = source_index + 1;
    for (int i = proj->frame_count; i > insert_pos; i--) {
        proj->frames[i] = proj->frames[i - 1];
    }
    proj->frames[insert_pos] = dst;
    proj->frame_count++;
    proj->current_frame = insert_pos;
    proj->canvas_dirty = true;
    return insert_pos;
}

bool canvas_delete_frame(CanvasProject *proj, int index) {
    if (proj->frame_count <= 1) return false; /* Keep at least 1 frame */
    if (index < 0 || index >= proj->frame_count) return false;

    frame_destroy(proj->frames[index]);
    for (int i = index; i < proj->frame_count - 1; i++) {
        proj->frames[i] = proj->frames[i + 1];
    }
    proj->frame_count--;

    if (proj->current_frame >= proj->frame_count) {
        proj->current_frame = proj->frame_count - 1;
    }
    proj->canvas_dirty = true;
    return true;
}

void canvas_set_current_frame(CanvasProject *proj, int index) {
    if (index < 0 || index >= proj->frame_count) return;
    proj->current_frame = index;
    proj->canvas_dirty = true;
}

void canvas_move_frame(CanvasProject *proj, int from_index, int to_index) {
    if (from_index < 0 || from_index >= proj->frame_count) return;
    if (to_index < 0 || to_index >= proj->frame_count) return;
    if (from_index == to_index) return;

    Frame *tmp = proj->frames[from_index];
    if (from_index < to_index) {
        for (int i = from_index; i < to_index; i++) {
            proj->frames[i] = proj->frames[i + 1];
        }
    } else {
        for (int i = from_index; i > to_index; i--) {
            proj->frames[i] = proj->frames[i - 1];
        }
    }
    proj->frames[to_index] = tmp;
    proj->current_frame = to_index;
    proj->canvas_dirty = true;
}

void canvas_set_active_layer(CanvasProject *proj, int layer) {
    if (layer == LAYER_BG || layer == LAYER_ANIM) {
        proj->active_layer = layer;
        proj->canvas_dirty = true;
    }
}

void canvas_toggle_active_layer(CanvasProject *proj) {
    proj->active_layer = (proj->active_layer == LAYER_BG) ? LAYER_ANIM : LAYER_BG;
    proj->canvas_dirty = true;
}

/* Undo / Redo */
void canvas_clear_redo(CanvasProject *proj) {
    for (int i = 0; i < proj->redo_count; i++) {
        free(proj->redo_stack[i].pixels);
    }
    proj->redo_count = 0;
}

void canvas_push_undo(CanvasProject *proj) {
    canvas_clear_redo(proj);

    Frame *cur = proj->frames[proj->current_frame];
    uint32_t *layer_pixels = cur->layers[proj->active_layer];
    int size = proj->width * proj->height;

    if (proj->undo_count >= MAX_UNDO_LEVELS) {
        free(proj->undo_stack[0].pixels);
        for (int i = 0; i < MAX_UNDO_LEVELS - 1; i++) {
            proj->undo_stack[i] = proj->undo_stack[i + 1];
        }
        proj->undo_count = MAX_UNDO_LEVELS - 1;
    }

    UndoStep *step = &proj->undo_stack[proj->undo_count++];
    step->frame_index = proj->current_frame;
    step->layer_index = proj->active_layer;
    step->width = proj->width;
    step->height = proj->height;
    step->pixels = (uint32_t *)malloc(size * sizeof(uint32_t));
    memcpy(step->pixels, layer_pixels, size * sizeof(uint32_t));
}

void canvas_undo(CanvasProject *proj) {
    if (proj->undo_count <= 0) return;

    UndoStep *step = &proj->undo_stack[--proj->undo_count];
    if (step->frame_index >= proj->frame_count) {
        free(step->pixels);
        return;
    }

    Frame *f = proj->frames[step->frame_index];
    int size = proj->width * proj->height;

    /* Push current into redo */
    if (proj->redo_count < MAX_UNDO_LEVELS) {
        UndoStep *rstep = &proj->redo_stack[proj->redo_count++];
        rstep->frame_index = step->frame_index;
        rstep->layer_index = step->layer_index;
        rstep->width = proj->width;
        rstep->height = proj->height;
        rstep->pixels = (uint32_t *)malloc(size * sizeof(uint32_t));
        memcpy(rstep->pixels, f->layers[step->layer_index], size * sizeof(uint32_t));
    }

    /* Restore */
    memcpy(f->layers[step->layer_index], step->pixels, size * sizeof(uint32_t));
    free(step->pixels);

    proj->current_frame = step->frame_index;
    proj->active_layer = step->layer_index;
    f->thumb_dirty = true;
    proj->canvas_dirty = true;
}

void canvas_redo(CanvasProject *proj) {
    if (proj->redo_count <= 0) return;

    UndoStep *step = &proj->redo_stack[--proj->redo_count];
    if (step->frame_index >= proj->frame_count) {
        free(step->pixels);
        return;
    }

    Frame *f = proj->frames[step->frame_index];
    int size = proj->width * proj->height;

    /* Push current into undo */
    if (proj->undo_count < MAX_UNDO_LEVELS) {
        UndoStep *ustep = &proj->undo_stack[proj->undo_count++];
        ustep->frame_index = step->frame_index;
        ustep->layer_index = step->layer_index;
        ustep->width = proj->width;
        ustep->height = proj->height;
        ustep->pixels = (uint32_t *)malloc(size * sizeof(uint32_t));
        memcpy(ustep->pixels, f->layers[step->layer_index], size * sizeof(uint32_t));
    }

    /* Restore */
    memcpy(f->layers[step->layer_index], step->pixels, size * sizeof(uint32_t));
    free(step->pixels);

    proj->current_frame = step->frame_index;
    proj->active_layer = step->layer_index;
    f->thumb_dirty = true;
    proj->canvas_dirty = true;
}

uint32_t canvas_get_pixel(CanvasProject *proj, int frame_idx, int layer_idx, int x, int y) {
    if (frame_idx < 0 || frame_idx >= proj->frame_count) return 0;
    if (layer_idx < 0 || layer_idx >= NUM_LAYERS) return 0;
    if (x < 0 || x >= proj->width || y < 0 || y >= proj->height) return 0;
    return proj->frames[frame_idx]->layers[layer_idx][y * proj->width + x];
}

void canvas_set_pixel(CanvasProject *proj, int frame_idx, int layer_idx, int x, int y, uint32_t color) {
    if (frame_idx < 0 || frame_idx >= proj->frame_count) return;
    if (layer_idx < 0 || layer_idx >= NUM_LAYERS) return;
    if (x < 0 || x >= proj->width || y < 0 || y >= proj->height) return;
    proj->frames[frame_idx]->layers[layer_idx][y * proj->width + x] = color;
    proj->frames[frame_idx]->thumb_dirty = true;
    proj->canvas_dirty = true;
}

void canvas_clear_layer(CanvasProject *proj, int frame_idx, int layer_idx, uint32_t color) {
    if (frame_idx < 0 || frame_idx >= proj->frame_count) return;
    if (layer_idx < 0 || layer_idx >= NUM_LAYERS) return;
    Frame *f = proj->frames[frame_idx];
    int size = proj->width * proj->height;
    for (int i = 0; i < size; i++) {
        f->layers[layer_idx][i] = color;
    }
    f->thumb_dirty = true;
    proj->canvas_dirty = true;
}

/* Alpha blend src over dst */
static inline uint32_t blend_pixel(uint32_t dst, uint32_t src) {
    uint8_t sr, sg, sb, sa;
    unpack_rgba(src, &sr, &sg, &sb, &sa);
    if (sa == 0) return dst;
    if (sa == 255) return src;

    uint8_t dr, dg, db, da;
    unpack_rgba(dst, &dr, &dg, &db, &da);

    float a_src = sa / 255.0f;
    float a_dst = (da / 255.0f) * (1.0f - a_src);
    float a_out = a_src + a_dst;

    if (a_out <= 0.001f) return 0;

    uint8_t r = (uint8_t)((sr * a_src + dr * a_dst) / a_out);
    uint8_t g = (uint8_t)((sg * a_src + dg * a_dst) / a_out);
    uint8_t b = (uint8_t)((sb * a_src + db * a_dst) / a_out);
    uint8_t a = (uint8_t)(a_out * 255.0f);

    return make_rgba(r, g, b, a);
}

/* Blend pixel with custom tint & opacity for onion skinning */
static inline uint32_t blend_onion_pixel(uint32_t dst, uint32_t src, float opacity, bool tint_red, bool tint_blue) {
    uint8_t sr, sg, sb, sa;
    unpack_rgba(src, &sr, &sg, &sb, &sa);
    if (sa == 0) return dst;

    uint8_t dr, dg, db, da;
    unpack_rgba(dst, &dr, &dg, &db, &da);

    /* Tint color */
    if (tint_red) {
        /* Tint previous frame to red / warm orange */
        sr = 230; sg = 40; sb = 40;
    } else if (tint_blue) {
        /* Tint next frame to cool cyan / blue */
        sr = 30; sg = 100; sb = 230;
    }

    float a_src = (sa / 255.0f) * opacity;
    float a_dst = (da / 255.0f) * (1.0f - a_src);
    float a_out = a_src + a_dst;
    if (a_out <= 0.001f) return dst;

    uint8_t r = (uint8_t)((sr * a_src + dr * a_dst) / a_out);
    uint8_t g = (uint8_t)((sg * a_src + dg * a_dst) / a_out);
    uint8_t b = (uint8_t)((sb * a_src + db * a_dst) / a_out);
    uint8_t a = (uint8_t)(a_out * 255.0f);

    return make_rgba(r, g, b, a);
}

void canvas_update_composite(CanvasProject *proj, SDL_Renderer *renderer) {
    if (!proj->canvas_dirty && proj->canvas_texture) return;

    int total_pixels = proj->width * proj->height;
    Frame *cur = proj->frames[proj->current_frame];

    /* 1. Base layer: Layer 0 (BG) of current frame */
    if (proj->layer_visible[LAYER_BG] && cur->layers[LAYER_BG]) {
        memcpy(proj->composite_buffer, cur->layers[LAYER_BG], total_pixels * sizeof(uint32_t));
    } else {
        /* White canvas if BG layer hidden */
        uint32_t white = make_rgba(255, 255, 255, 255);
        for (int i = 0; i < total_pixels; i++) proj->composite_buffer[i] = white;
    }

    /* 2. Onion Skinning (only active when not playing) */
    if (proj->onion_skin_enabled && !proj->is_playing) {
        /* Previous frames (warm red/orange tint) */
        for (int p = 1; p <= proj->onion_prev_frames; p++) {
            int prev_idx = proj->current_frame - p;
            if (prev_idx >= 0) {
                Frame *prev_f = proj->frames[prev_idx];
                uint32_t *prev_anim = prev_f->layers[LAYER_ANIM];
                float op = proj->onion_opacity / (float)p;
                for (int i = 0; i < total_pixels; i++) {
                    uint32_t px = prev_anim[i];
                    if ((px & 0xFF000000) != 0) {
                        proj->composite_buffer[i] = blend_onion_pixel(proj->composite_buffer[i], px, op, proj->onion_tint_mode, false);
                    }
                }
            }
        }

        /* Next frames (cool blue tint) */
        for (int n = 1; n <= proj->onion_next_frames; n++) {
            int next_idx = proj->current_frame + n;
            if (next_idx < proj->frame_count) {
                Frame *next_f = proj->frames[next_idx];
                uint32_t *next_anim = next_f->layers[LAYER_ANIM];
                float op = proj->onion_opacity / (float)n;
                for (int i = 0; i < total_pixels; i++) {
                    uint32_t px = next_anim[i];
                    if ((px & 0xFF000000) != 0) {
                        proj->composite_buffer[i] = blend_onion_pixel(proj->composite_buffer[i], px, op, false, proj->onion_tint_mode);
                    }
                }
            }
        }
    }

    /* 3. Current frame animation layer (Layer 1) */
    if (proj->layer_visible[LAYER_ANIM] && cur->layers[LAYER_ANIM]) {
        uint32_t *cur_anim = cur->layers[LAYER_ANIM];
        for (int i = 0; i < total_pixels; i++) {
            uint32_t px = cur_anim[i];
            if ((px & 0xFF000000) != 0) {
                proj->composite_buffer[i] = blend_pixel(proj->composite_buffer[i], px);
            }
        }
    }

    /* 4. Floating selection overlay if active */
    if (proj->has_selection && proj->selection_floating && proj->selection_pixels) {
        for (int sy = 0; sy < proj->sel_h; sy++) {
            int cy = proj->sel_y + sy;
            if (cy < 0 || cy >= proj->height) continue;
            for (int sx = 0; sx < proj->sel_w; sx++) {
                int cx = proj->sel_x + sx;
                if (cx < 0 || cx >= proj->width) continue;
                uint32_t sp = proj->selection_pixels[sy * proj->sel_w + sx];
                if ((sp & 0xFF000000) != 0) {
                    int cidx = cy * proj->width + cx;
                    proj->composite_buffer[cidx] = blend_pixel(proj->composite_buffer[cidx], sp);
                }
            }
        }
    }

    /* Upload to SDL texture */
    if (!proj->canvas_texture) {
        proj->canvas_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STREAMING,
            proj->width,
            proj->height
        );
    }
    SDL_UpdateTexture(proj->canvas_texture, NULL, proj->composite_buffer, proj->width * sizeof(uint32_t));
    proj->canvas_dirty = false;
}

void canvas_update_thumbnail(CanvasProject *proj, int frame_idx, SDL_Renderer *renderer) {
    if (frame_idx < 0 || frame_idx >= proj->frame_count) return;
    Frame *f = proj->frames[frame_idx];
    if (!f->thumb_dirty && f->thumb_texture) return;

    int total_pixels = proj->width * proj->height;
    uint32_t *thumb_buf = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
    if (!thumb_buf) return;

    /* Composite BG + ANIM for the thumbnail */
    memcpy(thumb_buf, f->layers[LAYER_BG], total_pixels * sizeof(uint32_t));
    uint32_t *anim = f->layers[LAYER_ANIM];
    for (int i = 0; i < total_pixels; i++) {
        uint32_t px = anim[i];
        if ((px & 0xFF000000) != 0) {
            thumb_buf[i] = blend_pixel(thumb_buf[i], px);
        }
    }

    if (!f->thumb_texture) {
        f->thumb_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STREAMING,
            proj->width,
            proj->height
        );
    }
    SDL_UpdateTexture(f->thumb_texture, NULL, thumb_buf, proj->width * sizeof(uint32_t));
    free(thumb_buf);
    f->thumb_dirty = false;
}
