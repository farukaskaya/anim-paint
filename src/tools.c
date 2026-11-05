#include "tools.h"
#include "font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void tools_init(ToolState *ts) {
    memset(ts, 0, sizeof(ToolState));
    ts->active_tool = TOOL_PENCIL;
    ts->primary_color = make_rgba(0, 0, 0, 255);         /* Black */
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

/* Bresenham line drawing */
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

/* Eraser stamp: Left-click = erase to transparent (Anim) or secondary color (BG); Right-click = replace primary with secondary */
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

/* Interpolate eraser along a line */
static void apply_eraser_line(ToolState *ts, CanvasProject *proj, int x0, int y0, int x1, int y1, bool color_replace) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        apply_eraser(ts, proj, x0, y0, color_replace);
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

/* Shapes: Rectangle */
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

/* Distinctly Rounded Rectangle */
static void draw_rounded_rect(CanvasProject *proj, int x0, int y0, int x1, int y1, uint32_t stroke_col, uint32_t fill_col, ShapeFillMode mode, int stroke_w) {
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 > x1) ? x0 : x1;
    int min_y = (y0 < y1) ? y0 : y1;
    int max_y = (y0 > y1) ? y0 : y1;
    int w = max_x - min_x;
    int h = max_y - min_y;
    if (w <= 0 || h <= 0) return;

    /* Distinctly rounded radius: ~25% of smaller edge, clamped to 8..32 px */
    int r = (w < h ? w : h) / 4;
    if (r < 8) r = (w < h ? w : h) / 2;
    if (r > 32) r = 32;
    if (r < 2) r = 2;

    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;

    int r2 = r * r;
    int inner_r = r - stroke_w;
    if (inner_r < 0) inner_r = 0;
    int inner_r2 = inner_r * inner_r;

    uint32_t f_col = (mode == FILL_SOLID_ONLY) ? stroke_col : fill_col;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            bool in_outer = false;
            bool in_inner = false;

            int dx = 0, dy = 0;
            if (x < min_x + r && y < min_y + r) { /* Top-left corner */
                dx = x - (min_x + r); dy = y - (min_y + r);
            } else if (x > max_x - r && y < min_y + r) { /* Top-right corner */
                dx = x - (max_x - r); dy = y - (min_y + r);
            } else if (x < min_x + r && y > max_y - r) { /* Bottom-left corner */
                dx = x - (min_x + r); dy = y - (max_y - r);
            } else if (x > max_x - r && y > max_y - r) { /* Bottom-right corner */
                dx = x - (max_x - r); dy = y - (max_y - r);
            } else {
                in_outer = true;
                if (x >= min_x + stroke_w && x <= max_x - stroke_w &&
                    y >= min_y + stroke_w && y <= max_y - stroke_w) {
                    in_inner = true;
                }
            }

            if (!in_outer) {
                int dist2 = dx * dx + dy * dy;
                if (dist2 <= r2) in_outer = true;
                if (dist2 <= inner_r2) in_inner = true;
            }

            if (in_outer) {
                if (mode == FILL_SOLID_ONLY) {
                    canvas_set_pixel(proj, cur_f, cur_l, x, y, f_col);
                } else if (mode == FILL_OUTLINE_AND_FILL) {
                    if (in_inner) {
                        canvas_set_pixel(proj, cur_f, cur_l, x, y, f_col);
                    } else {
                        canvas_set_pixel(proj, cur_f, cur_l, x, y, stroke_col);
                    }
                } else if (mode == FILL_OUTLINE_ONLY) {
                    if (!in_inner) {
                        canvas_set_pixel(proj, cur_f, cur_l, x, y, stroke_col);
                    }
                }
            }
        }
    }
}

/* Ellipse */
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

/* Polygon Fill (Scanline algorithm) */
static void fill_polygon(CanvasProject *proj, const int *px, const int *py, int n, uint32_t fill_col) {
    if (n < 3) return;
    int min_y = py[0], max_y = py[0];
    for (int i = 1; i < n; i++) {
        if (py[i] < min_y) min_y = py[i];
        if (py[i] > max_y) max_y = py[i];
    }
    if (min_y < 0) min_y = 0;
    if (max_y >= proj->height) max_y = proj->height - 1;

    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;
    int node_x[128];

    for (int y = min_y; y <= max_y; y++) {
        int nodes = 0;
        int j = n - 1;
        for (int i = 0; i < n; i++) {
            if ((py[i] < y && py[j] >= y) || (py[j] < y && py[i] >= y)) {
                if (nodes < 128) {
                    node_x[nodes++] = px[i] + (y - py[i]) * (px[j] - px[i]) / (py[j] - py[i]);
                }
            }
            j = i;
        }
        for (int i = 0; i < nodes - 1; i++) {
            for (int k = i + 1; k < nodes; k++) {
                if (node_x[i] > node_x[k]) {
                    int tmp = node_x[i];
                    node_x[i] = node_x[k];
                    node_x[k] = tmp;
                }
            }
        }
        for (int i = 0; i < nodes; i += 2) {
            if (i + 1 >= nodes) break;
            int x_start = node_x[i];
            int x_end = node_x[i + 1];
            if (x_start < 0) x_start = 0;
            if (x_end >= proj->width) x_end = proj->width - 1;
            for (int x = x_start; x <= x_end; x++) {
                canvas_set_pixel(proj, cur_f, cur_l, x, y, fill_col);
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

/* Stamp text onto canvas directly */
static void draw_text_to_canvas(CanvasProject *proj, const char *text, int x, int y, uint32_t color, int scale) {
    if (!text || scale <= 0) return;
    int cur_f = proj->current_frame;
    int cur_l = proj->active_layer;
    int cur_x = x;
    int cur_y = y;
    int len = (int)strlen(text);

    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n') {
            cur_x = x;
            cur_y += 9 * scale;
            continue;
        }
        if ((unsigned char)c < 32 || (unsigned char)c > 127) c = '?';
        const uint8_t *bitmap = font8x8_basic[(unsigned char)c - 32];
        for (int row = 0; row < 8; row++) {
            uint8_t byte = bitmap[row];
            for (int col = 0; col < 8; col++) {
                if (byte & (0x80 >> col)) {
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            canvas_set_pixel(proj, cur_f, cur_l, cur_x + col * scale + dx, cur_y + row * scale + dy, color);
                        }
                    }
                }
            }
        }
        cur_x += 8 * scale;
    }
}

/* Text tool management */
void tool_handle_text_input(ToolState *ts, const char *text) {
    if (!ts->text_active || !text) return;
    int cur_len = (int)strlen(ts->text_buffer);
    int add_len = (int)strlen(text);
    if (cur_len + add_len < (int)sizeof(ts->text_buffer) - 1) {
        strcat(ts->text_buffer, text);
    }
}

void tool_handle_text_backspace(ToolState *ts) {
    if (!ts->text_active) return;
    int len = (int)strlen(ts->text_buffer);
    if (len > 0) {
        ts->text_buffer[len - 1] = '\0';
    }
}

void tool_commit_text(ToolState *ts, CanvasProject *proj) {
    if (!ts->text_active) return;
    if (strlen(ts->text_buffer) > 0) {
        canvas_push_undo(proj);
        draw_text_to_canvas(proj, ts->text_buffer, ts->text_x, ts->text_y, ts->primary_color, 1);
        proj->canvas_dirty = true;
    }
    ts->text_active = false;
    ts->text_buffer[0] = '\0';
    SDL_StopTextInput();
}

void tool_cancel_text(ToolState *ts) {
    ts->text_active = false;
    ts->text_buffer[0] = '\0';
    SDL_StopTextInput();
}

/* Polygon tool management */
void tool_commit_polygon(ToolState *ts, CanvasProject *proj) {
    if (!ts->poly_active || ts->poly_point_count < 2) {
        ts->poly_active = false;
        ts->poly_point_count = 0;
        return;
    }

    uint32_t stroke_col = ts->primary_color;
    uint32_t fill_col = ts->secondary_color;

    if (ts->shape_fill == FILL_OUTLINE_AND_FILL || ts->shape_fill == FILL_SOLID_ONLY) {
        uint32_t f_col = (ts->shape_fill == FILL_SOLID_ONLY) ? stroke_col : fill_col;
        fill_polygon(proj, ts->poly_points_x, ts->poly_points_y, ts->poly_point_count, f_col);
    }

    if (ts->shape_fill == FILL_OUTLINE_ONLY || ts->shape_fill == FILL_OUTLINE_AND_FILL) {
        for (int i = 0; i < ts->poly_point_count; i++) {
            int next_i = (i + 1) % ts->poly_point_count;
            draw_line_bresenham(proj, ts->poly_points_x[i], ts->poly_points_y[i],
                                ts->poly_points_x[next_i], ts->poly_points_y[next_i],
                                stroke_col, ts->line_width, BRUSH_CIRCLE, false);
        }
    }

    ts->poly_active = false;
    ts->poly_point_count = 0;
    proj->canvas_dirty = true;
}

void tool_cancel_polygon(ToolState *ts) {
    ts->poly_active = false;
    ts->poly_point_count = 0;
}

/* ------------------------------------------------------------------ */
/* Selection helpers (rectangular + free-form lasso)                  */
/* ------------------------------------------------------------------ */

static uint32_t blend_src_over(uint32_t dst, uint32_t src) {
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

/* Free all selection buffers and reset selection state. */
static void selection_free_buffers(CanvasProject *proj) {
    if (proj->selection_pixels) {
        free(proj->selection_pixels);
        proj->selection_pixels = NULL;
    }
    if (proj->selection_mask) {
        free(proj->selection_mask);
        proj->selection_mask = NULL;
    }
    proj->sel_x = 0;
    proj->sel_y = 0;
    proj->sel_w = 0;
    proj->sel_h = 0;
    proj->has_selection = false;
    proj->selection_floating = false;
    proj->sel_outline_count = 0;
}

void tool_clear_selection(CanvasProject *proj) {
    selection_free_buffers(proj);
    proj->canvas_dirty = true;
}

/* Point-in-selection test (uses the mask for lasso selections). */
static bool selection_contains(CanvasProject *proj, int x, int y) {
    if (!proj->has_selection) return false;
    int lx = x - proj->sel_x;
    int ly = y - proj->sel_y;
    if (lx < 0 || lx >= proj->sel_w || ly < 0 || ly >= proj->sel_h) return false;
    if (proj->selection_mask) {
        return proj->selection_mask[ly * proj->sel_w + lx] != 0;
    }
    return true;
}

/* Erase the selected pixels from the source layer (used when a move starts). */
static void selection_clear_source(CanvasProject *proj) {
    int f = proj->current_frame;
    int l = proj->active_layer;
    uint32_t clear_col = (l == LAYER_BG) ? make_rgba(255, 255, 255, 255)
                                         : make_rgba(0, 0, 0, 0);
    for (int sy = 0; sy < proj->sel_h; sy++) {
        for (int sx = 0; sx < proj->sel_w; sx++) {
            int idx = sy * proj->sel_w + sx;
            if (proj->selection_mask && !proj->selection_mask[idx]) continue;
            canvas_set_pixel(proj, f, l, proj->sel_x + sx, proj->sel_y + sy, clear_col);
        }
    }
    proj->canvas_dirty = true;
}

/* Lift the selection off the canvas so it can be dragged. */
static void selection_begin_move(CanvasProject *proj) {
    if (!proj->has_selection || proj->selection_floating) return;
    canvas_push_undo(proj);

    /* Refresh the buffer in case the selection area was painted since it was made. */
    if (proj->selection_pixels) {
        int f = proj->current_frame;
        int l = proj->active_layer;
        for (int sy = 0; sy < proj->sel_h; sy++) {
            for (int sx = 0; sx < proj->sel_w; sx++) {
                int idx = sy * proj->sel_w + sx;
                if (proj->selection_mask && !proj->selection_mask[idx]) continue;
                proj->selection_pixels[idx] = canvas_get_pixel(
                    proj, f, l, proj->sel_x + sx, proj->sel_y + sy);
            }
        }
    }

    selection_clear_source(proj);
    proj->selection_floating = true;
    proj->canvas_dirty = true;
}

/* Copy the selected layer pixels into the selection buffer. */
static void selection_extract(CanvasProject *proj, uint8_t *mask) {
    int w = proj->sel_w;
    int h = proj->sel_h;
    if (w <= 0 || h <= 0) {
        free(mask);
        return;
    }

    uint32_t *pixels = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    if (!pixels) {
        free(mask);
        return;
    }

    int f = proj->current_frame;
    int l = proj->active_layer;
    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            int idx = sy * w + sx;
            if (mask && !mask[idx]) {
                pixels[idx] = 0;
            } else {
                pixels[idx] = canvas_get_pixel(proj, f, l, proj->sel_x + sx, proj->sel_y + sy);
            }
        }
    }

    free(proj->selection_pixels);
    free(proj->selection_mask);
    proj->selection_pixels = pixels;
    proj->selection_mask = mask;
    proj->has_selection = true;
    proj->selection_floating = false;
    proj->canvas_dirty = true;
}

/* Paint the floating selection back onto the active layer. */
void tool_commit_selection(ToolState *ts, CanvasProject *proj) {
    (void)ts;
    if (!proj->has_selection || !proj->selection_floating || !proj->selection_pixels) return;

    int f = proj->current_frame;
    int l = proj->active_layer;
    for (int sy = 0; sy < proj->sel_h; sy++) {
        for (int sx = 0; sx < proj->sel_w; sx++) {
            int idx = sy * proj->sel_w + sx;
            if (proj->selection_mask && !proj->selection_mask[idx]) continue;
            uint32_t src = proj->selection_pixels[idx];
            if ((src & 0xFF000000) == 0) continue;
            uint32_t dst = canvas_get_pixel(proj, f, l, proj->sel_x + sx, proj->sel_y + sy);
            canvas_set_pixel(proj, f, l, proj->sel_x + sx, proj->sel_y + sy,
                             blend_src_over(dst, src));
        }
    }
    proj->selection_floating = false;
    proj->canvas_dirty = true;
}

/* Deselect: bake any floating pixels, then drop the selection. */
void tool_cancel_selection(ToolState *ts, CanvasProject *proj) {
    if (proj->selection_floating) {
        tool_commit_selection(ts, proj);
    }
    selection_free_buffers(proj);
    proj->canvas_dirty = true;
}

/* Bresenham line into a local selection mask. */
static void mask_draw_line(uint8_t *mask, int w, int h, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            mask[y0 * w + x0] = 1;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static int compare_ints(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

/* Rasterize the lasso polygon into a mask covering the bounding box. */
static bool polygon_to_mask(const int *px, const int *py, int n,
                            int ox, int oy, int w, int h, uint8_t *mask) {
    if (n < 3 || w <= 0 || h <= 0) return false;
    memset(mask, 0, (size_t)w * h);

    int *nodes = (int *)malloc(sizeof(int) * (size_t)(n + 2));
    if (!nodes) return false;

    for (int y = 0; y < h; y++) {
        int gy = oy + y;
        int count = 0;
        int j = n - 1;
        for (int i = 0; i < n; i++) {
            if ((py[i] < gy && py[j] >= gy) || (py[j] < gy && py[i] >= gy)) {
                nodes[count++] = px[i] + (gy - py[i]) * (px[j] - px[i]) / (py[j] - py[i]);
            }
            j = i;
        }
        if (count > 1) {
            qsort(nodes, (size_t)count, sizeof(int), compare_ints);
        }
        for (int i = 0; i + 1 < count; i += 2) {
            int x_start = nodes[i] - ox;
            int x_end = nodes[i + 1] - ox;
            if (x_start > x_end) { int tmp = x_start; x_start = x_end; x_end = tmp; }
            if (x_start < 0) x_start = 0;
            if (x_end >= w) x_end = w - 1;
            for (int x = x_start; x <= x_end; x++) {
                mask[y * w + x] = 1;
            }
        }
    }

    /* Include the outline itself so very thin lassos still select pixels. */
    for (int i = 0; i < n; i++) {
        int next_i = (i + 1) % n;
        mask_draw_line(mask, w, h,
                       px[i] - ox, py[i] - oy,
                       px[next_i] - ox, py[next_i] - oy);
    }

    free(nodes);
    return true;
}

/* Mouse Event Handlers */
void tool_on_mouse_down(ToolState *ts, CanvasProject *proj, int btn, int cx, int cy) {
    if (cx < 0 || cx >= proj->width || cy < 0 || cy >= proj->height) return;

    /* If text is active and user clicked outside, commit previous text */
    if (ts->text_active && ts->active_tool != TOOL_TEXT) {
        tool_commit_text(ts, proj);
    }

    /* If polygon active and tool switched, commit polygon */
    if (ts->poly_active && ts->active_tool != TOOL_POLYGON) {
        tool_commit_polygon(ts, proj);
    }

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
    if (ts->active_tool != TOOL_EYEDROPPER && ts->active_tool != TOOL_MAGNIFIER &&
        ts->active_tool != TOOL_SELECT && ts->active_tool != TOOL_FREE_SELECT &&
        ts->active_tool != TOOL_TEXT && ts->active_tool != TOOL_POLYGON) {
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
            canvas_push_undo(proj);
            flood_fill(proj, cx, cy, draw_color);
            ts->is_drawing = false;
            break;
        case TOOL_EYEDROPPER: {
            uint32_t picked = canvas_get_pixel(proj, proj->current_frame, proj->active_layer, cx, cy);
            if ((picked & 0xFF000000) == 0 && proj->active_layer == LAYER_ANIM) {
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
            if (ts->text_active) {
                tool_commit_text(ts, proj);
            }
            ts->text_active = true;
            ts->text_x = cx;
            ts->text_y = cy;
            ts->text_cursor = 0;
            ts->text_buffer[0] = '\0';
            SDL_StartTextInput();
            ts->is_drawing = false;
            break;
        case TOOL_POLYGON:
            if (!ts->poly_active) {
                canvas_push_undo(proj);
                ts->poly_active = true;
                ts->poly_point_count = 0;
                ts->poly_points_x[ts->poly_point_count] = cx;
                ts->poly_points_y[ts->poly_point_count] = cy;
                ts->poly_point_count++;
            } else {
                int dx = abs(cx - ts->poly_points_x[0]);
                int dy = abs(cy - ts->poly_points_y[0]);
                if (btn == SDL_BUTTON_RIGHT || (dx <= 8 && dy <= 8 && ts->poly_point_count >= 3)) {
                    tool_commit_polygon(ts, proj);
                } else if (ts->poly_point_count < 63) {
                    ts->poly_points_x[ts->poly_point_count] = cx;
                    ts->poly_points_y[ts->poly_point_count] = cy;
                    ts->poly_point_count++;
                }
            }
            ts->is_drawing = false;
            break;
        case TOOL_FREE_SELECT:
            if (proj->has_selection && selection_contains(proj, cx, cy)) {
                selection_begin_move(proj);
            } else {
                if (proj->selection_floating) tool_commit_selection(ts, proj);
                selection_free_buffers(proj);
                ts->lasso_active = true;
                ts->lasso_point_count = 0;
                ts->lasso_points_x[ts->lasso_point_count] = cx;
                ts->lasso_points_y[ts->lasso_point_count] = cy;
                ts->lasso_point_count++;
            }
            break;
        case TOOL_SELECT:
            if (proj->has_selection && cx >= proj->sel_x && cx < proj->sel_x + proj->sel_w &&
                cy >= proj->sel_y && cy < proj->sel_y + proj->sel_h) {
                selection_begin_move(proj);
            } else {
                if (proj->selection_floating) tool_commit_selection(ts, proj);
                selection_free_buffers(proj);
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
    if (!ts->is_drawing) {
        ts->curr_x = cx;
        ts->curr_y = cy;
        return;
    }

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
            apply_eraser_line(ts, proj, ts->prev_x, ts->prev_y, cx, cy, ts->mouse_button == SDL_BUTTON_RIGHT);
            break;
        case TOOL_AIRBRUSH:
            apply_airbrush(ts, proj, cx, cy, draw_color);
            break;
        case TOOL_FREE_SELECT:
            if (proj->has_selection && proj->selection_floating) {
                int dx = cx - ts->prev_x;
                int dy = cy - ts->prev_y;
                proj->sel_x += dx;
                proj->sel_y += dy;
                proj->canvas_dirty = true;
            } else if (ts->lasso_active && ts->lasso_point_count < MAX_SELECTION_POINTS) {
                int last = ts->lasso_point_count - 1;
                if (ts->lasso_points_x[last] != cx || ts->lasso_points_y[last] != cy) {
                    ts->lasso_points_x[ts->lasso_point_count] = cx;
                    ts->lasso_points_y[ts->lasso_point_count] = cy;
                    ts->lasso_point_count++;
                }
            }
            break;
        case TOOL_SELECT:
            if (proj->has_selection && proj->selection_floating) {
                int dx = cx - ts->prev_x;
                int dy = cy - ts->prev_y;
                proj->sel_x += dx;
                proj->sel_y += dy;
                proj->canvas_dirty = true;
            } else {
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
            draw_rounded_rect(proj, ts->start_x, ts->start_y, cx, cy, draw_color, fill_color, ts->shape_fill, ts->line_width);
            break;
        case TOOL_ELLIPSE:
            draw_ellipse(proj, ts->start_x, ts->start_y, cx, cy, draw_color, fill_color, ts->shape_fill, ts->line_width);
            break;
        case TOOL_FREE_SELECT:
            if (ts->lasso_active) {
                ts->lasso_active = false;

                int min_x = ts->lasso_points_x[0];
                int max_x = ts->lasso_points_x[0];
                int min_y = ts->lasso_points_y[0];
                int max_y = ts->lasso_points_y[0];
                for (int i = 1; i < ts->lasso_point_count; i++) {
                    if (ts->lasso_points_x[i] < min_x) min_x = ts->lasso_points_x[i];
                    if (ts->lasso_points_x[i] > max_x) max_x = ts->lasso_points_x[i];
                    if (ts->lasso_points_y[i] < min_y) min_y = ts->lasso_points_y[i];
                    if (ts->lasso_points_y[i] > max_y) max_y = ts->lasso_points_y[i];
                }

                int w = max_x - min_x + 1;
                int h = max_y - min_y + 1;
                if (ts->lasso_point_count >= 3 && w > 1 && h > 1) {
                    uint8_t *mask = (uint8_t *)malloc((size_t)w * h);
                    if (mask && polygon_to_mask(ts->lasso_points_x, ts->lasso_points_y,
                                                ts->lasso_point_count, min_x, min_y, w, h, mask)) {
                        proj->sel_x = min_x;
                        proj->sel_y = min_y;
                        proj->sel_w = w;
                        proj->sel_h = h;

                        /* Keep the lasso outline relative to the selection box. */
                        int count = ts->lasso_point_count;
                        if (count > MAX_SELECTION_POINTS) count = MAX_SELECTION_POINTS;
                        proj->sel_outline_count = count;
                        for (int i = 0; i < count; i++) {
                            proj->sel_outline_x[i] = ts->lasso_points_x[i] - min_x;
                            proj->sel_outline_y[i] = ts->lasso_points_y[i] - min_y;
                        }

                        selection_extract(proj, mask);
                    } else {
                        free(mask);
                    }
                }
            } else if (proj->selection_floating) {
                tool_commit_selection(ts, proj);
            }
            break;
        case TOOL_SELECT:
            if (!proj->has_selection && proj->sel_w > 1 && proj->sel_h > 1) {
                proj->sel_outline_count = 0;
                selection_extract(proj, NULL);
            }
            break;
        case TOOL_CURVE:
            if (ts->curve_step == 0) {
                ts->curve_step = 1;
            } else if (ts->curve_step == 1) {
                ts->curve_step = 2;
            } else if (ts->curve_step == 2) {
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

/* Render on-screen previews */
void tool_render_preview(ToolState *ts, CanvasProject *proj, SDL_Renderer *renderer, int canvas_screen_x, int canvas_screen_y, float zoom) {
    if (ts->is_drawing) {
        int x0 = canvas_screen_x + (int)(ts->start_x * zoom);
        int y0 = canvas_screen_y + (int)(ts->start_y * zoom);
        int x1 = canvas_screen_x + (int)(ts->curr_x * zoom);
        int y1 = canvas_screen_y + (int)(ts->curr_y * zoom);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        if (ts->active_tool == TOOL_LINE) {
            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        } else if (ts->active_tool == TOOL_RECTANGLE) {
            int rx = (x0 < x1) ? x0 : x1;
            int ry = (y0 < y1) ? y0 : y1;
            int rw = abs(x1 - x0);
            int rh = abs(y1 - y0);
            SDL_Rect r = { rx, ry, rw, rh };
            SDL_RenderDrawRect(renderer, &r);
        } else if (ts->active_tool == TOOL_ROUNDED_RECT) {
            int rx = (x0 < x1) ? x0 : x1;
            int ry = (y0 < y1) ? y0 : y1;
            int rw = abs(x1 - x0);
            int rh = abs(y1 - y0);
            int r_corner = (rw < rh ? rw : rh) / 4;
            if (r_corner < 4) r_corner = 4;
            if (r_corner > 24) r_corner = 24;

            /* Outline preview with corner ticks */
            SDL_RenderDrawLine(renderer, rx + r_corner, ry, rx + rw - r_corner, ry);
            SDL_RenderDrawLine(renderer, rx + r_corner, ry + rh, rx + rw - r_corner, ry + rh);
            SDL_RenderDrawLine(renderer, rx, ry + r_corner, rx, ry + rh - r_corner);
            SDL_RenderDrawLine(renderer, rx + rw, ry + r_corner, rx + rw, ry + rh - r_corner);
            /* Corner diagonals */
            SDL_RenderDrawLine(renderer, rx, ry + r_corner, rx + r_corner, ry);
            SDL_RenderDrawLine(renderer, rx + rw - r_corner, ry, rx + rw, ry + r_corner);
            SDL_RenderDrawLine(renderer, rx, ry + rh - r_corner, rx + r_corner, ry + rh);
            SDL_RenderDrawLine(renderer, rx + rw - r_corner, ry + rh, rx + rw, ry + rh - r_corner);
        } else if (ts->active_tool == TOOL_ELLIPSE) {
            int rx = (x0 < x1) ? x0 : x1;
            int ry = (y0 < y1) ? y0 : y1;
            int rw = abs(x1 - x0);
            int rh = abs(y1 - y0);
            SDL_Rect r = { rx, ry, rw, rh };
            SDL_RenderDrawRect(renderer, &r);
        }
    }

    /* Polygon interactive preview */
    if (ts->active_tool == TOOL_POLYGON && ts->poly_active && ts->poly_point_count > 0) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 180, 255);
        for (int i = 0; i < ts->poly_point_count - 1; i++) {
            int px0 = canvas_screen_x + (int)(ts->poly_points_x[i] * zoom);
            int py0 = canvas_screen_y + (int)(ts->poly_points_y[i] * zoom);
            int px1 = canvas_screen_x + (int)(ts->poly_points_x[i+1] * zoom);
            int py1 = canvas_screen_y + (int)(ts->poly_points_y[i+1] * zoom);
            SDL_RenderDrawLine(renderer, px0, py0, px1, py1);
        }
        int last_x = canvas_screen_x + (int)(ts->poly_points_x[ts->poly_point_count - 1] * zoom);
        int last_y = canvas_screen_y + (int)(ts->poly_points_y[ts->poly_point_count - 1] * zoom);
        int cur_x = canvas_screen_x + (int)(ts->curr_x * zoom);
        int cur_y = canvas_screen_y + (int)(ts->curr_y * zoom);
        SDL_RenderDrawLine(renderer, last_x, last_y, cur_x, cur_y);

        /* Starting point box (click here to close) */
        int start_x = canvas_screen_x + (int)(ts->poly_points_x[0] * zoom);
        int start_y = canvas_screen_y + (int)(ts->poly_points_y[0] * zoom);
        SDL_Rect sbox = { start_x - 3, start_y - 3, 7, 7 };
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &sbox);
    }

    /* Text interactive preview */
    if (ts->text_active) {
        int tx = canvas_screen_x + (int)(ts->text_x * zoom);
        int ty = canvas_screen_y + (int)(ts->text_y * zoom);
        int scale = (int)zoom > 0 ? (int)zoom : 1;
        int text_w = font_text_width(ts->text_buffer, scale);
        if (text_w < 40) text_w = 40;
        int text_h = 10 * scale;

        SDL_SetRenderDrawColor(renderer, 0, 0, 180, 255);
        SDL_Rect tr = { tx - 2, ty - 2, text_w + 8, text_h + 4 };
        SDL_RenderDrawRect(renderer, &tr);

        uint8_t pr, pg, pb, pa;
        unpack_rgba(ts->primary_color, &pr, &pg, &pb, &pa);
        font_draw_text(renderer, ts->text_buffer, tx, ty, (SDL_Color){ pr, pg, pb, 255 }, scale);

        /* Blinking text cursor */
        if ((SDL_GetTicks() / 400) % 2 == 0) {
            int cur_x = tx + font_text_width(ts->text_buffer, scale);
            SDL_RenderDrawLine(renderer, cur_x + 1, ty, cur_x + 1, ty + text_h);
        }
    }

    /* Free-form lasso path while the user is drawing it */
    if (ts->active_tool == TOOL_FREE_SELECT && ts->lasso_active && ts->lasso_point_count > 1) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 180, 255);
        for (int i = 0; i < ts->lasso_point_count - 1; i++) {
            int x0 = canvas_screen_x + (int)(ts->lasso_points_x[i] * zoom);
            int y0 = canvas_screen_y + (int)(ts->lasso_points_y[i] * zoom);
            int x1 = canvas_screen_x + (int)(ts->lasso_points_x[i + 1] * zoom);
            int y1 = canvas_screen_y + (int)(ts->lasso_points_y[i + 1] * zoom);
            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        }
        int first_x = canvas_screen_x + (int)(ts->lasso_points_x[0] * zoom);
        int first_y = canvas_screen_y + (int)(ts->lasso_points_y[0] * zoom);
        int last_x = canvas_screen_x + (int)(ts->lasso_points_x[ts->lasso_point_count - 1] * zoom);
        int last_y = canvas_screen_y + (int)(ts->lasso_points_y[ts->lasso_point_count - 1] * zoom);
        SDL_RenderDrawLine(renderer, last_x, last_y, first_x, first_y);
    }

    /* Selection marquee: lasso outline when available, otherwise a box */
    if (proj->has_selection) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 180, 255);
        if (proj->sel_outline_count > 1) {
            for (int i = 0; i < proj->sel_outline_count; i++) {
                int next_i = (i + 1) % proj->sel_outline_count;
                int x0 = canvas_screen_x + (int)((proj->sel_x + proj->sel_outline_x[i]) * zoom);
                int y0 = canvas_screen_y + (int)((proj->sel_y + proj->sel_outline_y[i]) * zoom);
                int x1 = canvas_screen_x + (int)((proj->sel_x + proj->sel_outline_x[next_i]) * zoom);
                int y1 = canvas_screen_y + (int)((proj->sel_y + proj->sel_outline_y[next_i]) * zoom);
                SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
            }
        } else {
            int sx = canvas_screen_x + (int)(proj->sel_x * zoom);
            int sy = canvas_screen_y + (int)(proj->sel_y * zoom);
            int sw = (int)(proj->sel_w * zoom);
            int sh = (int)(proj->sel_h * zoom);
            SDL_Rect r = { sx, sy, sw, sh };
            SDL_RenderDrawRect(renderer, &r);
        }
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
