#ifndef UI_H
#define UI_H

#include "canvas.h"
#include "tools.h"
#include "audio.h"
#include <SDL2/SDL.h>

#define UI_BG_COLOR       0xC0, 0xC0, 0xC0, 0xFF
#define UI_LIGHT_COLOR    0xFF, 0xFF, 0xFF, 0xFF
#define UI_SHADOW_COLOR   0x80, 0x80, 0x80, 0xFF
#define UI_DARK_COLOR     0x00, 0x00, 0x00, 0xFF
#define UI_TITLE_ACTIVE   0x00, 0x00, 0x80, 0xFF /* Classic Win95 Navy */

#define TOOLBOX_WIDTH     60
#define TOOLBOX_X         4
#define TOOLBOX_Y         28

#define PALETTE_X         TOOLBOX_X + TOOLBOX_WIDTH + 8
#define PALETTE_HEIGHT    48

#define TIMELINE_HEIGHT   72
#define STATUSBAR_HEIGHT  22
#define MENUBAR_HEIGHT    22

typedef struct {
    int screen_w;
    int screen_h;

    /* Canvas viewport in center */
    int canvas_view_x;
    int canvas_view_y;
    int canvas_view_w;
    int canvas_view_h;
    float zoom;
    int pan_x, pan_y;

    /* Panning drag state */
    bool is_panning;
    int pan_drag_start_x, pan_drag_start_y;
    int pan_start_pan_x, pan_start_pan_y;

    /* Dialog / Modal states */
    bool show_export_modal;
    bool show_new_modal;
    char modal_message[320];
    uint32_t modal_message_time;

    /* Menu dropdown */
    int active_menu;     /* -1: none, 0: File, 1: Edit, 2: View, 3: Animation, 4: Help, 5: Voiceover */
    int hovered_menu_item;
    bool should_quit;

    /* Voiceover track (owned by main). NULL disables the Sound menu. */
    Voiceover *vo;

    /* Playback state saved while a voiceover take is recording. */
    bool rec_loop_saved;
    bool rec_was_loop;

    /* Timeline scroll */
    int timeline_scroll_x;
} UIState;

void ui_init(UIState *ui, int window_w, int window_h);
void ui_resize(UIState *ui, int window_w, int window_h);

/* Classic 3D beveled borders */
void ui_draw_bevel_raised(SDL_Renderer *r, int x, int y, int w, int h);
void ui_draw_bevel_sunken(SDL_Renderer *r, int x, int y, int w, int h);
void ui_draw_button(SDL_Renderer *r, int x, int y, int w, int h, bool pressed, const char *label);

/* Render entire interface */
void ui_render(UIState *ui, CanvasProject *proj, ToolState *ts, SDL_Renderer *r);

/* Event handling */
bool ui_handle_mouse_down(UIState *ui, CanvasProject **proj_ptr, ToolState *ts, SDL_Renderer *r, int btn, int mx, int my);
bool ui_handle_mouse_up(UIState *ui, CanvasProject *proj, ToolState *ts, int btn, int mx, int my);
bool ui_handle_mouse_move(UIState *ui, CanvasProject *proj, ToolState *ts, int mx, int my);
bool ui_handle_mouse_wheel(UIState *ui, int dy);

/* Convert screen coordinates to canvas pixel coordinates */
bool ui_screen_to_canvas(UIState *ui, CanvasProject *proj, int mx, int my, int *cx, int *cy);

/* Notification banner */
void ui_notify(UIState *ui, const char *msg);

/* Voiceover recording helpers (keep playback and audio in sync). */
double ui_animation_duration(const CanvasProject *proj);
void ui_voiceover_toggle_record(UIState *ui, CanvasProject *proj);
void ui_voiceover_stop_recording(UIState *ui, CanvasProject *proj, bool auto_completed);

#endif /* UI_H */
