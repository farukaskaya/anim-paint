#include "ui.h"
#include "font.h"
#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Windows 95 classic 28 colors */
static const uint32_t PALETTE_COLORS[28] = {
    /* Row 1 */
    0xFF000000, /* Black */
    0xFF808080, /* Dark Gray */
    0xFF000080, /* Maroon (RGBA in memory: 0x800000) */
    0xFF008080, /* Olive */
    0xFF008000, /* Green */
    0xFF808000, /* Teal */
    0xFF800000, /* Navy */
    0xFF800080, /* Purple */
    0xFF408080, /* Khaki */
    0xFF404000, /* Dark Teal */
    0xFFFF8000, /* Sky Blue */
    0xFF804000, /* Steel Blue */
    0xFFFF0080, /* Violet */
    0xFF004080, /* Brown */

    /* Row 2 */
    0xFFFFFFFF, /* White */
    0xFFC0C0C0, /* Light Gray */
    0xFF0000FF, /* Red */
    0xFF00FFFF, /* Yellow */
    0xFF00FF00, /* Lime */
    0xFFFFFF00, /* Cyan */
    0xFFFF0000, /* Blue */
    0xFFFF00FF, /* Magenta */
    0xFF80FFFF, /* Light Yellow */
    0xFF80FF00, /* Mint */
    0xFFFFFF80, /* Aqua */
    0xFFFF8080, /* Periwinkle */
    0xFF8000FF, /* Pink */
    0xFF4080FF, /* Orange */
};

void ui_init(UIState *ui, int window_w, int window_h) {
    memset(ui, 0, sizeof(UIState));
    ui->screen_w = window_w;
    ui->screen_h = window_h;
    ui->zoom = 1.0f;
    ui->pan_x = 0;
    ui->pan_y = 0;
    ui->active_menu = -1;
    ui->timeline_scroll_x = 0;
    ui_resize(ui, window_w, window_h);
}

void ui_resize(UIState *ui, int window_w, int window_h) {
    ui->screen_w = window_w;
    ui->screen_h = window_h;

    /* Center canvas view area between toolbars and timeline */
    int left_margin = TOOLBOX_X + TOOLBOX_WIDTH + 8;
    int top_margin = MENUBAR_HEIGHT + 4;
    int bottom_margin = TIMELINE_HEIGHT + STATUSBAR_HEIGHT + PALETTE_HEIGHT + 14;

    ui->canvas_view_x = left_margin;
    ui->canvas_view_y = top_margin;
    ui->canvas_view_w = window_w - left_margin - 8;
    ui->canvas_view_h = window_h - top_margin - bottom_margin;
}

void ui_notify(UIState *ui, const char *msg) {
    strncpy(ui->modal_message, msg, sizeof(ui->modal_message) - 1);
    ui->modal_message_time = SDL_GetTicks();
}

/* 3D Bevel routines */
void ui_draw_bevel_raised(SDL_Renderer *r, int x, int y, int w, int h) {
    /* Outer highlight */
    SDL_SetRenderDrawColor(r, UI_LIGHT_COLOR);
    SDL_RenderDrawLine(r, x, y, x + w - 2, y);
    SDL_RenderDrawLine(r, x, y, x, y + h - 2);

    /* Inner dark shadow */
    SDL_SetRenderDrawColor(r, UI_SHADOW_COLOR);
    SDL_RenderDrawLine(r, x + 1, y + h - 2, x + w - 2, y + h - 2);
    SDL_RenderDrawLine(r, x + w - 2, y + 1, x + w - 2, y + h - 2);

    /* Outer dark border */
    SDL_SetRenderDrawColor(r, UI_DARK_COLOR);
    SDL_RenderDrawLine(r, x, y + h - 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(r, x + w - 1, y, x + w - 1, y + h - 1);
}

void ui_draw_bevel_sunken(SDL_Renderer *r, int x, int y, int w, int h) {
    /* Outer dark shadow */
    SDL_SetRenderDrawColor(r, UI_DARK_COLOR);
    SDL_RenderDrawLine(r, x, y, x + w - 1, y);
    SDL_RenderDrawLine(r, x, y, x, y + h - 1);

    /* Inner gray shadow */
    SDL_SetRenderDrawColor(r, UI_SHADOW_COLOR);
    SDL_RenderDrawLine(r, x + 1, y + 1, x + w - 2, y + 1);
    SDL_RenderDrawLine(r, x + 1, y + 1, x, y + h - 2);

    /* Outer white highlight */
    SDL_SetRenderDrawColor(r, UI_LIGHT_COLOR);
    SDL_RenderDrawLine(r, x, y + h - 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(r, x + w - 1, y, x + w - 1, y + h - 1);
}

void ui_draw_button(SDL_Renderer *r, int x, int y, int w, int h, bool pressed, const char *label) {
    /* Base fill */
    SDL_SetRenderDrawColor(r, UI_BG_COLOR);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);

    if (pressed) {
        ui_draw_bevel_sunken(r, x, y, w, h);
    } else {
        ui_draw_bevel_raised(r, x, y, w, h);
    }

    if (label && strlen(label) > 0) {
        int text_w = font_text_width(label, 1);
        int tx = x + (w - text_w) / 2 + (pressed ? 1 : 0);
        int ty = y + (h - 8) / 2 + (pressed ? 1 : 0);
        SDL_Color col = { 0, 0, 0, 255 };
        font_draw_text(r, label, tx, ty, col, 1);
    }
}

/* Procedural MS Paint tool icons */
static void draw_tool_icon(SDL_Renderer *r, ToolType t, int x, int y) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    switch (t) {
        case TOOL_FREE_SELECT: { /* Lasso star/loop */
            SDL_RenderDrawLine(r, x + 5, y + 4, x + 16, y + 7);
            SDL_RenderDrawLine(r, x + 16, y + 7, x + 18, y + 16);
            SDL_RenderDrawLine(r, x + 18, y + 16, x + 7, y + 18);
            SDL_RenderDrawLine(r, x + 7, y + 18, x + 5, y + 4);
            break;
        }
        case TOOL_SELECT: { /* Dotted rectangle */
            for (int i = 0; i < 16; i += 3) {
                SDL_RenderDrawPoint(r, x + 4 + i, y + 4);
                SDL_RenderDrawPoint(r, x + 4 + i, y + 18);
            }
            for (int i = 0; i < 14; i += 3) {
                SDL_RenderDrawPoint(r, x + 4, y + 4 + i);
                SDL_RenderDrawPoint(r, x + 20, y + 4 + i);
            }
            break;
        }
        case TOOL_ERASER: { /* Eraser block */
            SDL_SetRenderDrawColor(r, 255, 128, 128, 255);
            SDL_Rect er = { x + 6, y + 7, 12, 10 };
            SDL_RenderFillRect(r, &er);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            SDL_RenderDrawRect(r, &er);
            SDL_RenderDrawLine(r, x + 12, y + 7, x + 12, y + 16);
            break;
        }
        case TOOL_BUCKET: { /* Paint bucket pouring */
            SDL_RenderDrawLine(r, x + 6, y + 10, x + 14, y + 6);
            SDL_RenderDrawLine(r, x + 14, y + 6, x + 18, y + 14);
            SDL_RenderDrawLine(r, x + 18, y + 14, x + 10, y + 18);
            SDL_RenderDrawLine(r, x + 10, y + 18, x + 6, y + 10);
            /* Dripping drop */
            SDL_SetRenderDrawColor(r, 0, 0, 200, 255);
            SDL_RenderDrawPoint(r, x + 5, y + 14);
            SDL_RenderDrawPoint(r, x + 4, y + 16);
            break;
        }
        case TOOL_EYEDROPPER: { /* Eyedropper */
            SDL_RenderDrawLine(r, x + 16, y + 5, x + 18, y + 7);
            SDL_RenderDrawLine(r, x + 12, y + 9, x + 16, y + 5);
            SDL_RenderDrawLine(r, x + 8, y + 13, x + 12, y + 9);
            SDL_RenderDrawLine(r, x + 5, y + 18, x + 8, y + 13);
            break;
        }
        case TOOL_MAGNIFIER: { /* Magnifying glass */
            for (int a = 0; a < 8; a++) {
                SDL_RenderDrawLine(r, x + 8, y + 4, x + 14, y + 4);
                SDL_RenderDrawLine(r, x + 6, y + 6, x + 16, y + 6);
                SDL_RenderDrawLine(r, x + 6, y + 12, x + 16, y + 12);
                SDL_RenderDrawLine(r, x + 8, y + 14, x + 14, y + 14);
            }
            SDL_RenderDrawLine(r, x + 15, y + 13, x + 20, y + 18);
            SDL_RenderDrawLine(r, x + 16, y + 13, x + 21, y + 18);
            break;
        }
        case TOOL_PENCIL: { /* Pencil angled */
            SDL_RenderDrawLine(r, x + 17, y + 5, x + 19, y + 7);
            SDL_RenderDrawLine(r, x + 8, y + 14, x + 17, y + 5);
            SDL_RenderDrawLine(r, x + 10, y + 16, x + 19, y + 7);
            SDL_RenderDrawLine(r, x + 5, y + 19, x + 8, y + 14);
            SDL_RenderDrawLine(r, x + 5, y + 19, x + 10, y + 16);
            break;
        }
        case TOOL_BRUSH: { /* Paintbrush */
            SDL_RenderDrawLine(r, x + 16, y + 5, x + 19, y + 8);
            SDL_RenderDrawLine(r, x + 11, y + 10, x + 16, y + 5);
            SDL_RenderDrawLine(r, x + 14, y + 13, x + 19, y + 8);
            /* Bristles */
            SDL_SetRenderDrawColor(r, 160, 80, 0, 255);
            SDL_Rect br = { x + 6, y + 14, 6, 6 };
            SDL_RenderFillRect(r, &br);
            break;
        }
        case TOOL_AIRBRUSH: { /* Spray can */
            SDL_Rect can = { x + 9, y + 8, 8, 11 };
            SDL_RenderDrawRect(r, &can);
            SDL_RenderDrawLine(r, x + 11, y + 5, x + 15, y + 5);
            /* Sprays */
            SDL_RenderDrawPoint(r, x + 6, y + 4);
            SDL_RenderDrawPoint(r, x + 8, y + 2);
            SDL_RenderDrawPoint(r, x + 4, y + 7);
            break;
        }
        case TOOL_TEXT: { /* Capital 'A' */
            font_draw_text(r, "A", x + 8, y + 6, (SDL_Color){ 0, 0, 0, 255 }, 2);
            break;
        }
        case TOOL_LINE: { /* Diagonal line */
            SDL_RenderDrawLine(r, x + 5, y + 18, x + 19, y + 5);
            SDL_RenderDrawLine(r, x + 6, y + 18, x + 20, y + 5);
            break;
        }
        case TOOL_CURVE: { /* S-curve */
            SDL_RenderDrawLine(r, x + 5, y + 17, x + 10, y + 14);
            SDL_RenderDrawLine(r, x + 10, y + 14, x + 14, y + 9);
            SDL_RenderDrawLine(r, x + 14, y + 9, x + 19, y + 6);
            break;
        }
        case TOOL_RECTANGLE: { /* Rectangle outline */
            SDL_Rect rc = { x + 5, y + 6, 14, 11 };
            SDL_RenderDrawRect(r, &rc);
            break;
        }
        case TOOL_POLYGON: { /* Polygon shape */
            SDL_RenderDrawLine(r, x + 6, y + 16, x + 10, y + 6);
            SDL_RenderDrawLine(r, x + 10, y + 6, x + 18, y + 9);
            SDL_RenderDrawLine(r, x + 18, y + 9, x + 15, y + 17);
            SDL_RenderDrawLine(r, x + 15, y + 17, x + 6, y + 16);
            break;
        }
        case TOOL_ELLIPSE: { /* Ellipse outline */
            SDL_Rect el = { x + 5, y + 7, 14, 10 };
            SDL_RenderDrawRect(r, &el);
            break;
        }
        case TOOL_ROUNDED_RECT: { /* Rounded rect outline */
            SDL_RenderDrawLine(r, x + 7, y + 6, x + 17, y + 6);
            SDL_RenderDrawLine(r, x + 7, y + 17, x + 17, y + 17);
            SDL_RenderDrawLine(r, x + 5, y + 8, x + 5, y + 15);
            SDL_RenderDrawLine(r, x + 19, y + 8, x + 19, y + 15);
            break;
        }
        default:
            break;
    }
}

/* Render Menu Bar */
static void render_menubar(UIState *ui, CanvasProject *proj, SDL_Renderer *r) {
    (void)proj;
    SDL_SetRenderDrawColor(r, UI_BG_COLOR);
    SDL_Rect mb_rect = { 0, 0, ui->screen_w, MENUBAR_HEIGHT };
    SDL_RenderFillRect(r, &mb_rect);

    /* Bottom divider */
    SDL_SetRenderDrawColor(r, UI_SHADOW_COLOR);
    SDL_RenderDrawLine(r, 0, MENUBAR_HEIGHT - 1, ui->screen_w, MENUBAR_HEIGHT - 1);

    SDL_Color tc = { 0, 0, 0, 255 };
    const char *menus[] = { "File", "Edit", "View", "Animation", "Help" };
    int mx = 8;
    for (int i = 0; i < 5; i++) {
        font_draw_text(r, menus[i], mx, 6, tc, 1);
        mx += font_text_width(menus[i], 1) + 14;
    }

    /* Right quick export pills */
    int qx = ui->screen_w - 240;
    ui_draw_button(r, qx, 2, 85, 18, false, "Export MP4");
    ui_draw_button(r, qx + 90, 2, 75, 18, false, "Export GIF");
    ui_draw_button(r, qx + 170, 2, 60, 18, false, "Save .anp");
}

/* Render Toolbox & Options Box */
static void render_toolbox(UIState *ui, ToolState *ts, SDL_Renderer *r) {
    /* Toolbox Frame */
    ui_draw_bevel_raised(r, TOOLBOX_X, TOOLBOX_Y, TOOLBOX_WIDTH, 230);

    /* 16 Tool Buttons (2 cols x 8 rows) */
    int btn_w = 26;
    int btn_h = 24;
    for (int i = 0; i < TOOL_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int bx = TOOLBOX_X + 3 + col * (btn_w + 2);
        int by = TOOLBOX_Y + 4 + row * (btn_h + 2);
        bool active = (ts->active_tool == (ToolType)i);

        ui_draw_button(r, bx, by, btn_w, btn_h, active, "");
        draw_tool_icon(r, (ToolType)i, bx, by);
    }

    /* Lower Tool Option Box */
    int opt_y = TOOLBOX_Y + 235;
    int opt_h = 100;
    ui_draw_bevel_sunken(r, TOOLBOX_X, opt_y, TOOLBOX_WIDTH, opt_h);

    /* Options depending on active tool */
    if (ts->active_tool == TOOL_LINE || ts->active_tool == TOOL_CURVE) {
        /* Line thickness choices */
        int thicknesses[4] = { 1, 2, 4, 6 };
        for (int i = 0; i < 4; i++) {
            int line_y = opt_y + 12 + i * 20;
            bool sel = (ts->line_width == thicknesses[i]);
            if (sel) {
                SDL_SetRenderDrawColor(r, 0, 0, 128, 255);
                SDL_Rect sr = { TOOLBOX_X + 6, line_y - 4, TOOLBOX_WIDTH - 12, thicknesses[i] + 8 };
                SDL_RenderFillRect(r, &sr);
                SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            } else {
                SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            }
            SDL_Rect lr = { TOOLBOX_X + 10, line_y, TOOLBOX_WIDTH - 20, thicknesses[i] };
            SDL_RenderFillRect(r, &lr);
        }
    } else if (ts->active_tool == TOOL_ERASER) {
        /* Eraser sizes */
        int sizes[4] = { 4, 8, 12, 16 };
        for (int i = 0; i < 4; i++) {
            int ey = opt_y + 8 + i * 22;
            bool sel = (ts->eraser_size == sizes[i]);
            if (sel) {
                SDL_SetRenderDrawColor(r, 0, 0, 128, 255);
                SDL_Rect sr = { TOOLBOX_X + 4, ey - 2, TOOLBOX_WIDTH - 8, sizes[i] + 4 };
                SDL_RenderFillRect(r, &sr);
                SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            } else {
                SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            }
            SDL_Rect er = { TOOLBOX_X + (TOOLBOX_WIDTH - sizes[i]) / 2, ey, sizes[i], sizes[i] };
            SDL_RenderDrawRect(r, &er);
        }
    } else if (ts->active_tool == TOOL_RECTANGLE || ts->active_tool == TOOL_ELLIPSE || ts->active_tool == TOOL_ROUNDED_RECT) {
        /* Fill modes: Outline only, Outline+Fill, Fill only */
        for (int i = 0; i < 3; i++) {
            int fy = opt_y + 10 + i * 28;
            bool sel = (ts->shape_fill == (ShapeFillMode)i);
            if (sel) {
                SDL_SetRenderDrawColor(r, 0, 0, 128, 255);
                SDL_Rect sr = { TOOLBOX_X + 6, fy - 3, TOOLBOX_WIDTH - 12, 24 };
                SDL_RenderFillRect(r, &sr);
            }
            SDL_Rect fr = { TOOLBOX_X + 14, fy, 32, 18 };
            if (i == 0) {
                SDL_SetRenderDrawColor(r, sel ? 255 : 0, sel ? 255 : 0, sel ? 255 : 0, 255);
                SDL_RenderDrawRect(r, &fr);
            } else if (i == 1) {
                SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
                SDL_RenderFillRect(r, &fr);
                SDL_SetRenderDrawColor(r, sel ? 255 : 0, sel ? 255 : 0, sel ? 255 : 0, 255);
                SDL_RenderDrawRect(r, &fr);
            } else {
                SDL_SetRenderDrawColor(r, sel ? 255 : 128, sel ? 255 : 128, sel ? 255 : 128, 255);
                SDL_RenderFillRect(r, &fr);
            }
        }
    } else {
        /* Default: Zoom / Info */
        char zstr[32];
        snprintf(zstr, sizeof(zstr), "%.0fx", ui->zoom);
        font_draw_text(r, "ZOOM", TOOLBOX_X + 14, opt_y + 20, (SDL_Color){ 0, 0, 0, 255 }, 1);
        font_draw_text(r, zstr, TOOLBOX_X + 20, opt_y + 40, (SDL_Color){ 0, 0, 160, 255 }, 2);
    }
}

/* Render Color Palette */
static void render_palette(UIState *ui, ToolState *ts, SDL_Renderer *r) {
    int pal_y = ui->screen_h - STATUSBAR_HEIGHT - PALETTE_HEIGHT - 4;
    int pal_w = 420;

    ui_draw_bevel_raised(r, PALETTE_X, pal_y, pal_w, PALETTE_HEIGHT);

    /* Current Selected Colors (overlapping boxes) */
    /* Secondary box (bottom right) */
    uint8_t sr, sg, sb, sa;
    unpack_rgba(ts->secondary_color, &sr, &sg, &sb, &sa);
    SDL_SetRenderDrawColor(r, sr, sg, sb, 255);
    SDL_Rect s_rect = { PALETTE_X + 18, pal_y + 18, 20, 20 };
    SDL_RenderFillRect(r, &s_rect);
    ui_draw_bevel_sunken(r, s_rect.x, s_rect.y, s_rect.w, s_rect.h);

    /* Primary box (top left) */
    uint8_t pr, pg, pb, pa;
    unpack_rgba(ts->primary_color, &pr, &pg, &pb, &pa);
    SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
    SDL_Rect p_rect = { PALETTE_X + 8, pal_y + 8, 20, 20 };
    SDL_RenderFillRect(r, &p_rect);
    ui_draw_bevel_sunken(r, p_rect.x, p_rect.y, p_rect.w, p_rect.h);

    /* 28-color grid (2 rows x 14 cols) */
    int swatch_start_x = PALETTE_X + 50;
    int swatch_size = 18;
    for (int i = 0; i < 28; i++) {
        int col = i % 14;
        int row = i / 14;
        int sx = swatch_start_x + col * (swatch_size + 4);
        int sy = pal_y + 5 + row * (swatch_size + 3);

        uint8_t cr, cg, cb, ca;
        unpack_rgba(PALETTE_COLORS[i], &cr, &cg, &cb, &ca);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        SDL_Rect sr = { sx, sy, swatch_size, swatch_size };
        SDL_RenderFillRect(r, &sr);
        ui_draw_bevel_sunken(r, sx, sy, swatch_size, swatch_size);
    }
}

/* Render Timeline, Frame Reel & Animation Controls */
static void render_timeline(UIState *ui, CanvasProject *proj, SDL_Renderer *r) {
    int tl_y = ui->screen_h - STATUSBAR_HEIGHT - PALETTE_HEIGHT - TIMELINE_HEIGHT - 8;
    int tl_w = ui->screen_w - PALETTE_X - 8;

    ui_draw_bevel_raised(r, PALETTE_X, tl_y, tl_w, TIMELINE_HEIGHT);

    /* 1. Layer Selection Buttons (Only 2 Layers!) */
    int lx = PALETTE_X + 8;
    int ly = tl_y + 6;
    bool is_bg = (proj->active_layer == LAYER_BG);
    bool is_anim = (proj->active_layer == LAYER_ANIM);

    ui_draw_button(r, lx, ly, 85, 26, is_bg, "BG Layer");
    ui_draw_button(r, lx + 90, ly, 95, 26, is_anim, "Anim Layer");

    /* Onion Skinning toggle */
    char onion_label[32];
    snprintf(onion_label, sizeof(onion_label), "Onion: %s", proj->onion_skin_enabled ? "ON" : "OFF");
    ui_draw_button(r, lx + 190, ly, 85, 26, proj->onion_skin_enabled, onion_label);

    /* 2. Playback & Frame Actions */
    int ax = lx + 285;
    ui_draw_button(r, ax, ly, 55, 26, false, "< Prev");
    ui_draw_button(r, ax + 60, ly, 65, 26, proj->is_playing, proj->is_playing ? "Pause" : "> Play");
    ui_draw_button(r, ax + 130, ly, 55, 26, false, "Next >");

    /* FPS Control */
    char fps_str[32];
    snprintf(fps_str, sizeof(fps_str), "%d fps", proj->fps);
    ui_draw_button(r, ax + 190, ly, 22, 26, false, "-");
    font_draw_text(r, fps_str, ax + 218, ly + 9, (SDL_Color){ 0, 0, 0, 255 }, 1);
    ui_draw_button(r, ax + 268, ly, 22, 26, false, "+");

    /* Frame Management */
    ui_draw_button(r, ax + 300, ly, 60, 26, false, "+ Frame");
    ui_draw_button(r, ax + 365, ly, 50, 26, false, "Dup");
    ui_draw_button(r, ax + 420, ly, 50, 26, false, "Del");

    /* 3. Frame Reel Thumbnails Strip */
    int reel_x = PALETTE_X + 8;
    int reel_y = tl_y + 36;
    int reel_w = tl_w - 16;
    int reel_h = 30;

    ui_draw_bevel_sunken(r, reel_x, reel_y, reel_w, reel_h);

    /* Render thumbnail buttons for each frame */
    int thumb_w = 40;
    int thumb_h = 24;
    int tx = reel_x + 4 + ui->timeline_scroll_x;

    for (int i = 0; i < proj->frame_count; i++) {
        if (tx + thumb_w > reel_x + reel_w - 4) break;
        if (tx >= reel_x + 2) {
            bool active = (i == proj->current_frame);

            /* Frame Box */
            SDL_Rect f_rect = { tx, reel_y + 3, thumb_w, thumb_h };
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            SDL_RenderFillRect(r, &f_rect);

            /* Frame Thumbnail preview */
            canvas_update_thumbnail(proj, i, r);
            if (proj->frames[i]->thumb_texture) {
                SDL_RenderCopy(r, proj->frames[i]->thumb_texture, NULL, &f_rect);
            }

            /* Border */
            if (active) {
                SDL_SetRenderDrawColor(r, 0, 0, 220, 255);
                SDL_RenderDrawRect(r, &f_rect);
                SDL_Rect inner = { f_rect.x + 1, f_rect.y + 1, f_rect.w - 2, f_rect.h - 2 };
                SDL_RenderDrawRect(r, &inner);
            } else {
                SDL_SetRenderDrawColor(r, 128, 128, 128, 255);
                SDL_RenderDrawRect(r, &f_rect);
            }

            /* Frame number label */
            char fnum[16];
            snprintf(fnum, sizeof(fnum), "%d", i + 1);
            font_draw_text(r, fnum, tx + 2, reel_y + 4, (SDL_Color){ active ? 0 : 80, 0, active ? 220 : 80, 255 }, 1);
        }
        tx += thumb_w + 6;
    }
}

/* Render Status Bar */
static void render_statusbar(UIState *ui, CanvasProject *proj, ToolState *ts, SDL_Renderer *r) {
    int sb_y = ui->screen_h - STATUSBAR_HEIGHT;
    ui_draw_bevel_sunken(r, 0, sb_y, ui->screen_w, STATUSBAR_HEIGHT);

    SDL_Color tc = { 0, 0, 0, 255 };

    /* Tool name / Notification */
    if (SDL_GetTicks() - ui->modal_message_time < 4000 && strlen(ui->modal_message) > 0) {
        font_draw_text(r, ui->modal_message, 8, sb_y + 7, (SDL_Color){ 180, 0, 0, 255 }, 1);
    } else {
        char status_msg[128];
        snprintf(status_msg, sizeof(status_msg), "%s  |  Space: Play/Pause  |  Tab: Switch Layer", tool_get_name(ts->active_tool));
        font_draw_text(r, status_msg, 8, sb_y + 7, tc, 1);
    }

    /* Active Layer */
    int lx = ui->screen_w - 380;
    ui_draw_bevel_sunken(r, lx, sb_y + 2, 110, STATUSBAR_HEIGHT - 4);
    char layer_txt[32];
    snprintf(layer_txt, sizeof(layer_txt), "Layer: %s", proj->active_layer == LAYER_BG ? "BG" : "ANIM");
    font_draw_text(r, layer_txt, lx + 6, sb_y + 7, tc, 1);

    /* Frame Info */
    int fx = ui->screen_w - 260;
    ui_draw_bevel_sunken(r, fx, sb_y + 2, 120, STATUSBAR_HEIGHT - 4);
    char frame_txt[32];
    snprintf(frame_txt, sizeof(frame_txt), "Frame %d / %d", proj->current_frame + 1, proj->frame_count);
    font_draw_text(r, frame_txt, fx + 6, sb_y + 7, tc, 1);

    /* Canvas Size */
    int cx = ui->screen_w - 130;
    ui_draw_bevel_sunken(r, cx, sb_y + 2, 120, STATUSBAR_HEIGHT - 4);
    char size_txt[32];
    snprintf(size_txt, sizeof(size_txt), "%dx%d px", proj->width, proj->height);
    font_draw_text(r, size_txt, cx + 6, sb_y + 7, tc, 1);
}

void ui_render(UIState *ui, CanvasProject *proj, ToolState *ts, SDL_Renderer *r) {
    /* 1. Main background fill */
    SDL_SetRenderDrawColor(r, UI_BG_COLOR);
    SDL_RenderClear(r);

    /* 2. Canvas viewport frame (Sunken area) */
    ui_draw_bevel_sunken(r, ui->canvas_view_x, ui->canvas_view_y, ui->canvas_view_w, ui->canvas_view_h);

    /* Viewport inner dark gray area */
    SDL_Rect view_rect = { ui->canvas_view_x + 2, ui->canvas_view_y + 2, ui->canvas_view_w - 4, ui->canvas_view_h - 4 };
    SDL_SetRenderDrawColor(r, 128, 128, 128, 255);
    SDL_RenderFillRect(r, &view_rect);

    /* 3. Render Canvas */
    int c_screen_w = (int)(proj->width * ui->zoom);
    int c_screen_h = (int)(proj->height * ui->zoom);
    int c_screen_x = ui->canvas_view_x + (ui->canvas_view_w - c_screen_w) / 2 + ui->pan_x;
    int c_screen_y = ui->canvas_view_y + (ui->canvas_view_h - c_screen_h) / 2 + ui->pan_y;

    /* Drop shadow behind canvas */
    SDL_SetRenderDrawColor(r, 64, 64, 64, 255);
    SDL_Rect shadow_rect = { c_screen_x + 3, c_screen_y + 3, c_screen_w, c_screen_h };
    SDL_RenderFillRect(r, &shadow_rect);

    /* Composite canvas texture */
    canvas_update_composite(proj, r);
    if (proj->canvas_texture) {
        SDL_Rect dst_rect = { c_screen_x, c_screen_y, c_screen_w, c_screen_h };
        SDL_RenderCopy(r, proj->canvas_texture, NULL, &dst_rect);

        /* Canvas border */
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderDrawRect(r, &dst_rect);
    }

    /* Interactive preview (rubberband line/rect, curve, selection) */
    tool_render_preview(ts, proj, r, c_screen_x, c_screen_y, ui->zoom);

    /* 4. Chrome UI Elements */
    render_menubar(ui, proj, r);
    render_toolbox(ui, ts, r);
    render_palette(ui, ts, r);
    render_timeline(ui, proj, r);
    render_statusbar(ui, proj, ts, r);
}

bool ui_screen_to_canvas(UIState *ui, CanvasProject *proj, int mx, int my, int *cx, int *cy) {
    int c_screen_w = (int)(proj->width * ui->zoom);
    int c_screen_h = (int)(proj->height * ui->zoom);
    int c_screen_x = ui->canvas_view_x + (ui->canvas_view_w - c_screen_w) / 2 + ui->pan_x;
    int c_screen_y = ui->canvas_view_y + (ui->canvas_view_h - c_screen_h) / 2 + ui->pan_y;

    if (mx < c_screen_x || mx >= c_screen_x + c_screen_w ||
        my < c_screen_y || my >= c_screen_y + c_screen_h) {
        return false;
    }

    *cx = (int)((mx - c_screen_x) / ui->zoom);
    *cy = (int)((my - c_screen_y) / ui->zoom);

    if (*cx < 0) *cx = 0;
    if (*cx >= proj->width) *cx = proj->width - 1;
    if (*cy < 0) *cy = 0;
    if (*cy >= proj->height) *cy = proj->height - 1;

    return true;
}

/* Mouse Wheel Zoom */
bool ui_handle_mouse_wheel(UIState *ui, int dy) {
    if (dy > 0) {
        if (ui->zoom < 8.0f) ui->zoom += (ui->zoom >= 2.0f ? 1.0f : 0.5f);
        return true;
    } else if (dy < 0) {
        if (ui->zoom > 0.5f) ui->zoom -= (ui->zoom > 2.0f ? 1.0f : 0.5f);
        return true;
    }
    return false;
}

/* UI Click Event Dispatcher */
bool ui_handle_mouse_down(UIState *ui, CanvasProject *proj, ToolState *ts, SDL_Renderer *r, int btn, int mx, int my) {
    /* 1. Menu Bar Clicks */
    if (my < MENUBAR_HEIGHT) {
        int qx = ui->screen_w - 240;
        if (mx >= qx && mx < qx + 85) {
            /* Export MP4 */
            ui_notify(ui, "Exporting animation to output.mp4...");
            SDL_RenderPresent(r);
            bool ok = export_to_mp4(proj, "output.mp4", proj->fps);
            ui_notify(ui, ok ? "Exported output.mp4 successfully!" : "Failed to export MP4 (check ffmpeg)");
            return true;
        } else if (mx >= qx + 90 && mx < qx + 165) {
            /* Export GIF */
            ui_notify(ui, "Exporting animation to output.gif...");
            SDL_RenderPresent(r);
            bool ok = export_to_gif(proj, "output.gif", proj->fps);
            ui_notify(ui, ok ? "Exported output.gif successfully!" : "Failed to export GIF (check ffmpeg)");
            return true;
        } else if (mx >= qx + 170 && mx < qx + 230) {
            /* Save .anp */
            bool ok = project_save(proj, "project.anp");
            ui_notify(ui, ok ? "Saved project.anp!" : "Failed to save project.anp");
            return true;
        }
        return true;
    }

    /* 2. Toolbox 16 Tools */
    if (mx >= TOOLBOX_X && mx < TOOLBOX_X + TOOLBOX_WIDTH &&
        my >= TOOLBOX_Y && my < TOOLBOX_Y + 230) {
        int btn_w = 26;
        int btn_h = 24;
        for (int i = 0; i < TOOL_COUNT; i++) {
            int col = i % 2;
            int row = i / 2;
            int bx = TOOLBOX_X + 3 + col * (btn_w + 2);
            int by = TOOLBOX_Y + 4 + row * (btn_h + 2);
            if (mx >= bx && mx < bx + btn_w && my >= by && my < by + btn_h) {
                ts->active_tool = (ToolType)i;
                return true;
            }
        }
        return true;
    }

    /* 3. Tool Option Box Clicks */
    int opt_y = TOOLBOX_Y + 235;
    if (mx >= TOOLBOX_X && mx < TOOLBOX_X + TOOLBOX_WIDTH &&
        my >= opt_y && my < opt_y + 100) {
        if (ts->active_tool == TOOL_LINE || ts->active_tool == TOOL_CURVE) {
            int idx = (my - opt_y) / 25;
            int thicknesses[4] = { 1, 2, 4, 6 };
            if (idx >= 0 && idx < 4) ts->line_width = thicknesses[idx];
        } else if (ts->active_tool == TOOL_ERASER) {
            int idx = (my - opt_y) / 25;
            int sizes[4] = { 4, 8, 12, 16 };
            if (idx >= 0 && idx < 4) ts->eraser_size = sizes[idx];
        } else if (ts->active_tool == TOOL_RECTANGLE || ts->active_tool == TOOL_ELLIPSE || ts->active_tool == TOOL_ROUNDED_RECT) {
            int idx = (my - opt_y) / 32;
            if (idx >= 0 && idx < 3) ts->shape_fill = (ShapeFillMode)idx;
        }
        return true;
    }

    /* 4. Color Palette Clicks */
    int pal_y = ui->screen_h - STATUSBAR_HEIGHT - PALETTE_HEIGHT - 4;
    if (my >= pal_y && my < pal_y + PALETTE_HEIGHT && mx >= PALETTE_X && mx < PALETTE_X + 420) {
        int swatch_start_x = PALETTE_X + 50;
        int swatch_size = 18;
        for (int i = 0; i < 28; i++) {
            int col = i % 14;
            int row = i / 14;
            int sx = swatch_start_x + col * (swatch_size + 4);
            int sy = pal_y + 5 + row * (swatch_size + 3);
            if (mx >= sx && mx < sx + swatch_size && my >= sy && my < sy + swatch_size) {
                if (btn == SDL_BUTTON_RIGHT) {
                    ts->secondary_color = PALETTE_COLORS[i];
                } else {
                    ts->primary_color = PALETTE_COLORS[i];
                }
                return true;
            }
        }
        return true;
    }

    /* 5. Timeline & Animation Controls Clicks */
    int tl_y = ui->screen_h - STATUSBAR_HEIGHT - PALETTE_HEIGHT - TIMELINE_HEIGHT - 8;
    if (my >= tl_y && my < tl_y + TIMELINE_HEIGHT && mx >= PALETTE_X) {
        int lx = PALETTE_X + 8;
        int ly = tl_y + 6;

        /* Layer Switchers */
        if (mx >= lx && mx < lx + 85 && my >= ly && my < ly + 26) {
            canvas_set_active_layer(proj, LAYER_BG);
            ui_notify(ui, "Editing Layer 0: Background");
            return true;
        }
        if (mx >= lx + 90 && mx < lx + 185 && my >= ly && my < ly + 26) {
            canvas_set_active_layer(proj, LAYER_ANIM);
            ui_notify(ui, "Editing Layer 1: Animation");
            return true;
        }
        if (mx >= lx + 190 && mx < lx + 275 && my >= ly && my < ly + 26) {
            proj->onion_skin_enabled = !proj->onion_skin_enabled;
            proj->canvas_dirty = true;
            ui_notify(ui, proj->onion_skin_enabled ? "Onion Skinning: ON" : "Onion Skinning: OFF");
            return true;
        }

        /* Playback controls */
        int ax = lx + 285;
        if (mx >= ax && mx < ax + 55 && my >= ly && my < ly + 26) {
            /* Prev Frame */
            if (proj->current_frame > 0) canvas_set_current_frame(proj, proj->current_frame - 1);
            return true;
        }
        if (mx >= ax + 60 && mx < ax + 125 && my >= ly && my < ly + 26) {
            /* Play / Pause */
            proj->is_playing = !proj->is_playing;
            proj->canvas_dirty = true;
            return true;
        }
        if (mx >= ax + 130 && mx < ax + 185 && my >= ly && my < ly + 26) {
            /* Next Frame */
            if (proj->current_frame < proj->frame_count - 1) canvas_set_current_frame(proj, proj->current_frame + 1);
            return true;
        }

        /* FPS adjust */
        if (mx >= ax + 190 && mx < ax + 212 && my >= ly && my < ly + 26) {
            if (proj->fps > 1) proj->fps--;
            return true;
        }
        if (mx >= ax + 268 && mx < ax + 290 && my >= ly && my < ly + 26) {
            if (proj->fps < 30) proj->fps++;
            return true;
        }

        /* Frame actions */
        if (mx >= ax + 300 && mx < ax + 360 && my >= ly && my < ly + 26) {
            /* Add Frame */
            canvas_add_frame(proj, proj->current_frame + 1);
            ui_notify(ui, "Added new frame");
            return true;
        }
        if (mx >= ax + 365 && mx < ax + 415 && my >= ly && my < ly + 26) {
            /* Duplicate Frame */
            canvas_duplicate_frame(proj, proj->current_frame);
            ui_notify(ui, "Duplicated frame");
            return true;
        }
        if (mx >= ax + 420 && mx < ax + 470 && my >= ly && my < ly + 26) {
            /* Delete Frame */
            if (canvas_delete_frame(proj, proj->current_frame)) {
                ui_notify(ui, "Deleted frame");
            } else {
                ui_notify(ui, "Cannot delete only remaining frame");
            }
            return true;
        }

        /* Frame Reel Thumbnails Strip */
        int reel_x = PALETTE_X + 8;
        int reel_y = tl_y + 36;
        int reel_w = ui->screen_w - PALETTE_X - 24;
        int reel_h = 30;

        if (my >= reel_y && my < reel_y + reel_h && mx >= reel_x && mx < reel_x + reel_w) {
            int thumb_w = 40;
            int tx = reel_x + 4 + ui->timeline_scroll_x;
            for (int i = 0; i < proj->frame_count; i++) {
                if (mx >= tx && mx < tx + thumb_w) {
                    canvas_set_current_frame(proj, i);
                    return true;
                }
                tx += thumb_w + 6;
            }
        }

        return true;
    }

    /* 6. Canvas area clicks */
    int cx, cy;
    if (ui_screen_to_canvas(ui, proj, mx, my, &cx, &cy)) {
        if (ts->active_tool == TOOL_MAGNIFIER) {
            if (btn == SDL_BUTTON_RIGHT) {
                if (ui->zoom > 1.0f) ui->zoom /= 2.0f;
            } else {
                if (ui->zoom < 8.0f) ui->zoom *= 2.0f;
            }
            return true;
        }
        tool_on_mouse_down(ts, proj, btn, cx, cy);
        return true;
    }

    return false;
}

bool ui_handle_mouse_move(UIState *ui, CanvasProject *proj, ToolState *ts, int mx, int my) {
    int cx, cy;
    if (ui_screen_to_canvas(ui, proj, mx, my, &cx, &cy)) {
        tool_on_mouse_move(ts, proj, cx, cy);
        return true;
    }
    return false;
}

bool ui_handle_mouse_up(UIState *ui, CanvasProject *proj, ToolState *ts, int btn, int mx, int my) {
    int cx, cy;
    if (ui_screen_to_canvas(ui, proj, mx, my, &cx, &cy)) {
        tool_on_mouse_up(ts, proj, btn, cx, cy);
        return true;
    } else if (ts->is_drawing) {
        /* Mouse released outside canvas view */
        tool_on_mouse_up(ts, proj, btn, ts->curr_x, ts->curr_y);
        return true;
    }
    return false;
}
