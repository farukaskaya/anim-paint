#ifndef TOOLS_H
#define TOOLS_H

#include "canvas.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TOOL_FREE_SELECT = 0,
    TOOL_SELECT,
    TOOL_ERASER,
    TOOL_BUCKET,
    TOOL_EYEDROPPER,
    TOOL_MAGNIFIER,
    TOOL_PENCIL,
    TOOL_BRUSH,
    TOOL_AIRBRUSH,
    TOOL_TEXT,
    TOOL_LINE,
    TOOL_CURVE,
    TOOL_RECTANGLE,
    TOOL_POLYGON,
    TOOL_ELLIPSE,
    TOOL_ROUNDED_RECT,
    TOOL_COUNT
} ToolType;

typedef enum {
    FILL_OUTLINE_ONLY = 0,
    FILL_OUTLINE_AND_FILL,
    FILL_SOLID_ONLY
} ShapeFillMode;

typedef enum {
    BRUSH_CIRCLE = 0,
    BRUSH_SQUARE,
    BRUSH_SLASH_RIGHT,
    BRUSH_SLASH_LEFT
} BrushShape;

typedef struct {
    ToolType active_tool;
    uint32_t primary_color;      /* Left-click color */
    uint32_t secondary_color;    /* Right-click color */

    /* Tool Options */
    int line_width;              /* 1, 2, 3, 4, 6, 8 */
    int eraser_size;             /* 4, 8, 12, 16, 24 */
    int brush_size;              /* 2, 4, 7, 10 */
    BrushShape brush_shape;
    int airbrush_size;           /* 6, 12, 20 */
    ShapeFillMode shape_fill;

    /* Interactive state */
    bool is_drawing;
    int mouse_button;            /* 1 = Left, 3 = Right */
    int start_x, start_y;
    int prev_x, prev_y;
    int curr_x, curr_y;

    /* Curve tool state machine */
    int curve_step;              /* 0: drag line, 1: drag bend 1, 2: drag bend 2 */
    int curve_p0_x, curve_p0_y;
    int curve_p1_x, curve_p1_y;
    int curve_p2_x, curve_p2_y;
    int curve_p3_x, curve_p3_y;

    /* Polygon tool state machine */
    bool poly_active;
    int poly_points_x[64];
    int poly_points_y[64];
    int poly_point_count;

    /* Text tool state */
    bool text_active;
    int text_x, text_y;
    char text_buffer[256];
    int text_cursor;

    /* Preview overlay on canvas */
    bool has_preview;
} ToolState;

void tools_init(ToolState *ts);
const char *tool_get_name(ToolType t);

/* Tool mouse event handlers */
void tool_on_mouse_down(ToolState *ts, CanvasProject *proj, int btn, int cx, int cy);
void tool_on_mouse_move(ToolState *ts, CanvasProject *proj, int cx, int cy);
void tool_on_mouse_up(ToolState *ts, CanvasProject *proj, int btn, int cx, int cy);

/* Render interactive previews (shapes, curve, text, selection box) */
void tool_render_preview(ToolState *ts, CanvasProject *proj, SDL_Renderer *renderer, int canvas_screen_x, int canvas_screen_y, float zoom);

#endif /* TOOLS_H */
