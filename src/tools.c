#include "tools.h"
#include "font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void tools_init(ToolState *ts) {
    memset(ts, 0, sizeof(ToolState));
    ts->active_tool = TOOL_PENCIL;
    ts->primary_color = make_rgba(0, 0, 0, 255);       /* Black */
    ts->secondary_color = make_rgba(255, 255, 255, 255); /* White */
    ts->line_width = 1;
    ts->eraser_size = 8;
    ts->brush_size = 4;
    ts->brush_shape = BRUSH_CIRCLE;
    ts->airbrush_size = 12;
    ts->shape_fill = FILL_OUTLINE_ONLY;
}

const char *tool_get_name(ToolType t) {
    switch (t) {
        case TOOL_FREE_SELECT: return "Free-Form Select";
        case TOOL_SELECT: return "Select";
        case TOOL_ERASER: return "Eraser / Color Eraser";
        case TOOL_BUCKET: return "Fill With Color";
        case TOOL_EYEDROPPER: return "Pick Color";
        case TOOL_MAGNIFIER: return "Magnifier";
        case TOOL_PENCIL: return "Pencil";
        case TOOL_BRUSH: return "Brush";
        case TOOL_AIRBRUSH: return "Airbrush";
        case TOOL_TEXT: return "Text";
        case TOOL_LINE: return "Line";
        case TOOL_CURVE: return "Curve";
        case TOOL_RECTANGLE: return "Rectangle";
        case TOOL_POLYGON: return "Polygon";
        case TOOL_ELLIPSE: return "Ellipse";
        case TOOL_ROUNDED_RECT: return "Rounded Rectangle";
        default: return "Tool";
    }
}

/* Stamp brush shape onto canvas */
static void stamp_brush(CanvasProject *proj, int x, int y, int size, BrushShape shape, uint32_t color) {
    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;
    int half = size / 2;

    switch (shape) {
        case BRUSH_CIRCLE: {
            int r2 = half * half;
            for (int dy = -half; dy <= half; dy++) {
                for (int dx = -half; dx <= half; dx++) {
                    if (dx * dx + dy * dy <= r2) {
                        canvas_set_pixel(proj, cur_f, cur_l, x + dx, y + dy, color);
                    }
                }
            }
            break;
        }
        case BRUSH_SQUARE: {
            for (int dy = -half; dy <= half; dy++) {
                for (int dx = -half; dx <= half; dx++) {
                    canvas_set_pixel(proj, cur_f, cur_l, x + dx, y + dy, color);
                }
            }
            break;
        }
        case BRUSH_SLASH_RIGHT: {
            for (int d = -half; d <= half; d++) {
                canvas_set_pixel(proj, cur_f, cur_l, x + d, y - d, color);
                if (size > 3) canvas_set_pixel(proj, cur_f, cur_l, x + d + 1, y - d, color);
            }
            break;
        }
        case BRUSH_SLASH_LEFT: {
            for (int d = -half; d <= half; d++) {
                canvas_set_pixel(proj, cur_f, cur_l, x + d, y + d, color);
                if (size > 3) canvas_set_pixel(proj, cur_f, cur_l, x + d + 1, y + d, color);
            }
            break;
        }
    }
}

/* Bresenham line drawing with optional brush stamping */
static void draw_line_bresenham(CanvasProject *proj, int x0, int y0, int x1, int y1, uint32_t color, int width, BrushShape shape, bool use_brush) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;

    while (1) {
        if (use_brush) {
            stamp_brush(proj, x0, y0, width, shape, color);
        } else if (width <= 1) {
            canvas_set_pixel(proj, cur_f, cur_l, x0, y0, color);
        } else {
            stamp_brush(proj, x0, y0, width, BRUSH_CIRCLE, color);
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* Eraser stamp */
static void apply_eraser(ToolState *ts, CanvasProject *proj, int x, int y, bool color_replace) {
    int half = ts->eraser_size / 2;
    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;

    uint32_t target_match = ts->primary_color;
    uint32_t erase_color = (cur_l == LAYER_ANIM) ? make_rgba(0, 0, 0, 0) : ts->secondary_color;

    for (int dy = -half; dy <= half; dy++) {
        for (int dx = -half; dx <= half; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px < 0 || px >= proj->width || py < 0 || py >= proj->height) continue;

            if (color_replace) {
                uint32_t cur = canvas_get_pixel(proj, cur_f, cur_l, px, py);
                if (cur == target_match) {
                    canvas_set_pixel(proj, cur_f, cur_l, px, py, ts->secondary_color);
                }
            } else {
                canvas_set_pixel(proj, cur_f, cur_l, px, py, erase_color);
            }
        }
    }
}

/* Flood fill queue */
typedef struct { int x, y; } Point;

static void flood_fill(CanvasProject *proj, int start_x, int start_y, uint32_t fill_color) {
    int w = proj->width;
    int h = proj->height;
    if (start_x < 0 || start_x >= w || start_y < 0 || start_y >= h) return;

    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;
    uint32_t target_color = canvas_get_pixel(proj, cur_f, cur_l, start_x, start_y);
    if (target_color == fill_color) return;

    int capacity = w * h;
    Point *queue = (Point *)malloc(capacity * sizeof(Point));
    if (!queue) return;

    uint8_t *visited = (uint8_t *)calloc(capacity, sizeof(uint8_t));
    if (!visited) { free(queue); return; }

    int head = 0;
    int tail = 0;

    queue[tail++] = (Point){ start_x, start_y };
    visited[start_y * w + start_x] = 1;

    while (head < tail) {
        Point p = queue[head++];
        canvas_set_pixel(proj, cur_f, cur_l, p.x, p.y, fill_color);

        const int dx[4] = { 0, 0, 1, -1 };
        const int dy[4] = { 1, -1, 0, 0 };

        for (int i = 0; i < 4; i++) {
            int nx = p.x + dx[i];
            int ny = p.y + dy[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int idx = ny * w + nx;
                if (!visited[idx] && canvas_get_pixel(proj, cur_f, cur_l, nx, ny) == target_color) {
                    visited[idx] = 1;
                    if (tail < capacity) {
                        queue[tail++] = (Point){ nx, ny };
                    }
                }
            }
        }
    }

    free(visited);
    free(queue);
}

/* Airbrush spray */
static void apply_airbrush(ToolState *ts, CanvasProject *proj, int x, int y, uint32_t color) {
    int r = ts->airbrush_size;
    int drops = r * 2;
    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;

    for (int i = 0; i < drops; i++) {
        float angle = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159f;
        float dist = ((float)rand() / (float)RAND_MAX) * (float)r;
        int px = x + (int)(cosf(angle) * dist);
        int py = y + (int)(sinf(angle) * dist);
        if (px >= 0 && px < proj->width && py >= 0 && py < proj->height) {
            canvas_set_pixel(proj, cur_f, cur_l, px, py, color);
        }
    }
}

/* Shapes: Rectangle, Ellipse, Rounded Rect */
static void draw_rect(CanvasProject *proj, int x0, int y0, int x1, int y1, uint32_t stroke_col, uint32_t fill_col, ShapeFillMode mode, int stroke_w) {
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 > x1) ? x0 : x1;
    int min_y = (y0 < y1) ? y0 : y1;
    int max_y = (y0 > y1) ? y0 : y1;

    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;

    if (mode == FILL_OUTLINE_AND_FILL || mode == FILL_SOLID_ONLY) {
        uint32_t f_col = (mode == FILL_SOLID_ONLY) ? stroke_col : fill_col;
        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                canvas_set_pixel(proj, cur_f, cur_l, x, y, f_col);
            }
        }
    }

    if (mode == FILL_OUTLINE_ONLY || mode == FILL_OUTLINE_AND_FILL) {
        for (int sw = 0; sw < stroke_w; sw++) {
            for (int x = min_x + sw; x <= max_x - sw; x++) {
                canvas_set_pixel(proj, cur_f, cur_l, x, min_y + sw, stroke_col);
                canvas_set_pixel(proj, cur_f, cur_l, x, max_y - sw, stroke_col);
            }
            for (int y = min_y + sw; y <= max_y - sw; y++) {
                canvas_set_pixel(proj, cur_f, cur_l, min_x + sw, y, stroke_col);
                canvas_set_pixel(proj, cur_f, cur_l, max_x - sw, y, stroke_col);
            }
        }
    }
}

static void draw_ellipse(CanvasProject *proj, int x0, int y0, int x1, int y1, uint32_t stroke_col, uint32_t fill_col, ShapeFillMode mode, int stroke_w) {
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 > x1) ? x0 : x1;
    int min_y = (y0 < y1) ? y0 : y1;
    int max_y = (y0 > y1) ? y0 : y1;

    int cx = (min_x + max_x) / 2;
    int cy = (min_y + max_y) / 2;
    int rx = (max_x - min_x) / 2;
    int ry = (max_y - min_y) / 2;
    if (rx <= 0 || ry <= 0) return;

    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;

    float rx2 = (float)(rx * rx);
    float ry2 = (float)(ry * ry);

    if (mode == FILL_OUTLINE_AND_FILL || mode == FILL_SOLID_ONLY) {
        uint32_t f_col = (mode == FILL_SOLID_ONLY) ? stroke_col : fill_col;
        for (int y = -ry; y <= ry; y++) {
            for (int x = -rx; x <= rx; x++) {
                if (((x * x) / rx2) + ((y * y) / ry2) <= 1.0f) {
                    canvas_set_pixel(proj, cur_f, cur_l, cx + x, cy + y, f_col);
                }
            }
        }
    }

    if (mode == FILL_OUTLINE_ONLY || mode == FILL_OUTLINE_AND_FILL) {
        for (int y = -ry; y <= ry; y++) {
            for (int x = -rx; x <= rx; x++) {
                float val = ((x * x) / rx2) + ((y * y) / ry2);
                float inner_rx = (float)(rx - stroke_w);
                float inner_ry = (float)(ry - stroke_w);
                float inner_val = (inner_rx > 0 && inner_ry > 0) ?
                    ((x * x) / (inner_rx * inner_rx)) + ((y * y) / (inner_ry * inner_ry)) : 2.0f;

                if (val <= 1.0f && inner_val >= 1.0f) {
                    canvas_set_pixel(proj, cur_f, cur_l, cx + x, cy + y, stroke_col);
                }
            }
        }
    }
}

/* Cubic Bezier curve drawer */
static void draw_bezier_curve(CanvasProject *proj, int p0x, int p0y, int p1x, int p1y, int p2x, int p2y, int p3x, int p3y, uint32_t color, int width) {
    int prev_x = p0x, prev_y = p0y;
    int steps = 100;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;

        int cur_x = (int)(uuu * p0x + 3 * uu * t * p1x + 3 * u * tt * p2x + ttt * p3x);
        int cur_y = (int)(uuu * p0y + 3 * uu * t * p1y + 3 * u * tt * p2y + ttt * p3y);

        draw_line_bresenham(proj, prev_x, prev_y, cur_x, cur_y, color, width, BRUSH_CIRCLE, false);
        prev_x = cur_x;
        prev_y = cur_y;
    }
}

/* Mouse Event Handlers */
void tool_on_mouse_down(ToolState *ts, CanvasProject *proj, int btn, int cx, int cy) {
    if (cx < 0 || cx >= proj->width || cy < 0 || cy >= proj->height) return;

    ts->is_drawing = true;
    ts->mouse_button = btn;
    ts->start_x = cx;
    ts->start_y = cy;
    ts->prev_x = cx;
    ts->prev_y = cy;
    ts->curr_x = cx;
    ts->curr_y = cy;

    uint32_t draw_color = (btn == SDL_BUTTON_RIGHT) ? ts->secondary_color : ts->primary_color;

    /* Push undo snapshot before stroke starts */
    if (ts->active_tool != TOOL_EYEDROPPER && ts->active_tool != TOOL_MAGNIFIER && ts->active_tool != TOOL_SELECT) {
        if (ts->active_tool != TOOL_CURVE || ts->curve_step == 0) {
            canvas_push_undo(proj);
        }
    }

    switch (ts->active_tool) {
        case TOOL_PENCIL:
            canvas_set_pixel(proj, proj->current_frame, proj->active_layer, cx, cy, draw_color);
            break;
        case TOOL_BRUSH:
            stamp_brush(proj, cx, cy, ts->brush_size, ts->brush_shape, draw_color);
            break;
        case TOOL_ERASER:
            apply_eraser(ts, proj, cx, cy, btn == SDL_BUTTON_RIGHT);
            break;
        case TOOL_BUCKET:
            flood_fill(proj, cx, cy, draw_color);
            ts->is_drawing = false;
            break;
        case TOOL_EYEDROPPER: {
            uint32_t picked = canvas_get_pixel(proj, proj->current_frame, proj->active_layer, cx, cy);
            if ((picked & 0xFF000000) == 0 && proj->active_layer == LAYER_ANIM) {
                /* If transparent in anim, pick background pixel */
                picked = canvas_get_pixel(proj, proj->current_frame, LAYER_BG, cx, cy);
            }
            if (btn == SDL_BUTTON_RIGHT) ts->secondary_color = picked;
            else ts->primary_color = picked;
            ts->is_drawing = false;
            break;
        }
        case TOOL_AIRBRUSH:
            apply_airbrush(ts, proj, cx, cy, draw_color);
            break;
        case TOOL_TEXT:
            ts->text_active = true;
            ts->text_x = cx;
            ts->text_y = cy;
            ts->text_cursor = 0;
            ts->text_buffer[0] = '\0';
            ts->is_drawing = false;
            break;
        case TOOL_SELECT:
            /* If clicking inside existing selection, start moving it */
            if (proj->has_selection && cx >= proj->sel_x && cx < proj->sel_x + proj->sel_w &&
                cy >= proj->sel_y && cy < proj->sel_y + proj->sel_h) {
                proj->selection_floating = true;
            } else {
                /* Start new selection marquee */
                if (proj->has_selection && proj->selection_floating) {
                    /* Commit previous selection */
                    proj->selection_floating = false;
                    proj->has_selection = false;
                }
                proj->sel_x = cx;
                proj->sel_y = cy;
                proj->sel_w = 0;
                proj->sel_h = 0;
                proj->has_selection = false;
            }
            break;
        case TOOL_CURVE:
            if (ts->curve_step == 0) {
                ts->curve_p0_x = cx; ts->curve_p0_y = cy;
                ts->curve_p3_x = cx; ts->curve_p3_y = cy;
                ts->curve_p1_x = cx; ts->curve_p1_y = cy;
                ts->curve_p2_x = cx; ts->curve_p2_y = cy;
            } else if (ts->curve_step == 1) {
                ts->curve_p1_x = cx; ts->curve_p1_y = cy;
            } else if (ts->curve_step == 2) {
                ts->curve_p2_x = cx; ts->curve_p2_y = cy;
            }
            break;
        default:
            break;
    }
}

void tool_on_mouse_move(ToolState *ts, CanvasProject *proj, int cx, int cy) {
    if (!ts->is_drawing) return;

    ts->curr_x = cx;
    ts->curr_y = cy;
    uint32_t draw_color = (ts->mouse_button == SDL_BUTTON_RIGHT) ? ts->secondary_color : ts->primary_color;

    switch (ts->active_tool) {
        case TOOL_PENCIL:
            draw_line_bresenham(proj, ts->prev_x, ts->prev_y, cx, cy, draw_color, 1, BRUSH_CIRCLE, false);
            break;
        case TOOL_BRUSH:
            draw_line_bresenham(proj, ts->prev_x, ts->prev_y, cx, cy, draw_color, ts->brush_size, ts->brush_shape, true);
            break;
        case TOOL_ERASER:
            draw_line_bresenham(proj, ts->prev_x, ts->prev_y, cx, cy, draw_color, ts->eraser_size, BRUSH_SQUARE, false);
            apply_eraser(ts, proj, cx, cy, ts->mouse_button == SDL_BUTTON_RIGHT);
            break;
        case TOOL_AIRBRUSH:
            apply_airbrush(ts, proj, cx, cy, draw_color);
            break;
        case TOOL_SELECT:
            if (proj->has_selection && proj->selection_floating) {
                /* Move floating selection */
                int dx = cx - ts->prev_x;
                int dy = cy - ts->prev_y;
                proj->sel_x += dx;
                proj->sel_y += dy;
                proj->canvas_dirty = true;
            } else {
                /* Dragging selection box */
                proj->sel_x = (ts->start_x < cx) ? ts->start_x : cx;
                proj->sel_y = (ts->start_y < cy) ? ts->start_y : cy;
                proj->sel_w = abs(cx - ts->start_x);
                proj->sel_h = abs(cy - ts->start_y);
            }
            break;
        case TOOL_CURVE:
            if (ts->curve_step == 0) {
                ts->curve_p3_x = cx; ts->curve_p3_y = cy;
                ts->curve_p1_x = (ts->curve_p0_x + cx) / 2;
                ts->curve_p1_y = (ts->curve_p0_y + cy) / 2;
                ts->curve_p2_x = ts->curve_p1_x;
                ts->curve_p2_y = ts->curve_p1_y;
            } else if (ts->curve_step == 1) {
                ts->curve_p1_x = cx; ts->curve_p1_y = cy;
            } else if (ts->curve_step == 2) {
                ts->curve_p2_x = cx; ts->curve_p2_y = cy;
            }
            break;
        default:
            break;
    }

    ts->prev_x = cx;
    ts->prev_y = cy;
}

void tool_on_mouse_up(ToolState *ts, CanvasProject *proj, int btn, int cx, int cy) {
    if (!ts->is_drawing) return;
    ts->is_drawing = false;
    ts->curr_x = cx;
    ts->curr_y = cy;

    uint32_t draw_color = (btn == SDL_BUTTON_RIGHT) ? ts->secondary_color : ts->primary_color;
    uint32_t fill_color = (btn == SDL_BUTTON_RIGHT) ? ts->primary_color : ts->secondary_color;

    switch (ts->active_tool) {
        case TOOL_LINE:
            draw_line_bresenham(proj, ts->start_x, ts->start_y, cx, cy, draw_color, ts->line_width, BRUSH_CIRCLE, false);
            break;
        case TOOL_RECTANGLE:
            draw_rect(proj, ts->start_x, ts->start_y, cx, cy, draw_color, fill_color, ts->shape_fill, ts->line_width);
            break;
        case TOOL_ROUNDED_RECT:
            draw_rect(proj, ts->start_x, ts->start_y, cx, cy, draw_color, fill_color, ts->shape_fill, ts->line_width);
            break;
        case TOOL_ELLIPSE:
            draw_ellipse(proj, ts->start_x, ts->start_y, cx, cy, draw_color, fill_color, ts->shape_fill, ts->line_width);
            break;
        case TOOL_SELECT:
            if (!proj->has_selection && proj->sel_w > 1 && proj->sel_h > 1) {
                proj->has_selection = true;
                /* Lift selection pixels from active layer */
                if (proj->selection_pixels) free(proj->selection_pixels);
                proj->selection_pixels = (uint32_t *)malloc(proj->sel_w * proj->sel_h * sizeof(uint32_t));
                for (int sy = 0; sy < proj->sel_h; sy++) {
                    for (int sx = 0; sx < proj->sel_w; sx++) {
                        proj->selection_pixels[sy * proj->sel_w + sx] = canvas_get_pixel(proj, proj->current_frame, proj->active_layer, proj->sel_x + sx, proj->sel_y + sy);
                    }
                }
            }
            break;
        case TOOL_CURVE:
            if (ts->curve_step == 0) {
                ts->curve_step = 1;
            } else if (ts->curve_step == 1) {
                ts->curve_step = 2;
            } else if (ts->curve_step == 2) {
                /* Commit curve to canvas */
                draw_bezier_curve(proj, ts->curve_p0_x, ts->curve_p0_y, ts->curve_p1_x, ts->curve_p1_y,
                                  ts->curve_p2_x, ts->curve_p2_y, ts->curve_p3_x, ts->curve_p3_y,
                                  draw_color, ts->line_width);
                ts->curve_step = 0;
            }
            break;
        default:
            break;
    }
}

/* Render on-screen previews (like rubber-banding line or rectangle while dragging) */
void tool_render_preview(ToolState *ts, CanvasProject *proj, SDL_Renderer *renderer, int canvas_screen_x, int canvas_screen_y, float zoom) {
    if (!ts->is_drawing && ts->active_tool != TOOL_CURVE && !proj->has_selection) return;

    if (ts->is_drawing) {
        int x0 = canvas_screen_x + (int)(ts->start_x * zoom);
        int y0 = canvas_screen_y + (int)(ts->start_y * zoom);
        int x1 = canvas_screen_x + (int)(ts->curr_x * zoom);
        int y1 = canvas_screen_y + (int)(ts->curr_y * zoom);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        if (ts->active_tool == TOOL_LINE) {
            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        } else if (ts->active_tool == TOOL_RECTANGLE || ts->active_tool == TOOL_ROUNDED_RECT) {
            int rx = (x0 < x1) ? x0 : x1;
            int ry = (y0 < y1) ? y0 : y1;
            int rw = abs(x1 - x0);
            int rh = abs(y1 - y0);
            SDL_Rect r = { rx, ry, rw, rh };
            SDL_RenderDrawRect(renderer, &r);
        } else if (ts->active_tool == TOOL_ELLIPSE) {
            int rx = (x0 < x1) ? x0 : x1;
            int ry = (y0 < y1) ? y0 : y1;
            int rw = abs(x1 - x0);
            int rh = abs(y1 - y0);
            SDL_Rect r = { rx, ry, rw, rh };
            SDL_RenderDrawRect(renderer, &r);
        }
    }

    /* Selection marquee dashed/dotted box */
    if (proj->has_selection) {
        int sx = canvas_screen_x + (int)(proj->sel_x * zoom);
        int sy = canvas_screen_y + (int)(proj->sel_y * zoom);
        int sw = (int)(proj->sel_w * zoom);
        int sh = (int)(proj->sel_h * zoom);

        SDL_SetRenderDrawColor(renderer, 0, 0, 180, 255);
        SDL_Rect r = { sx, sy, sw, sh };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* Curve handles */
    if (ts->active_tool == TOOL_CURVE && ts->curve_step > 0) {
        int p0x = canvas_screen_x + (int)(ts->curve_p0_x * zoom);
        int p0y = canvas_screen_y + (int)(ts->curve_p0_y * zoom);
        int p1x = canvas_screen_x + (int)(ts->curve_p1_x * zoom);
        int p1y = canvas_screen_y + (int)(ts->curve_p1_y * zoom);
        int p2x = canvas_screen_x + (int)(ts->curve_p2_x * zoom);
        int p2y = canvas_screen_y + (int)(ts->curve_p2_y * zoom);
        int p3x = canvas_screen_x + (int)(ts->curve_p3_x * zoom);
        int p3y = canvas_screen_y + (int)(ts->curve_p3_y * zoom);

        SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255);
        SDL_RenderDrawLine(renderer, p0x, p0y, p1x, p1y);
        SDL_RenderDrawLine(renderer, p2x, p2y, p3x, p3y);
    }
}
