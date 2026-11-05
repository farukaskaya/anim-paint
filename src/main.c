#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "canvas.h"
#include "tools.h"
#include "ui.h"
#include "export.h"

#define DEFAULT_WINDOW_W 1080
#define DEFAULT_WINDOW_H 740
#define DEFAULT_CANVAS_W 640
#define DEFAULT_CANVAS_H 480

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Faruk Paint - Classic MS Paint Animation Studio",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DEFAULT_WINDOW_W,
        DEFAULT_WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    CanvasProject *proj = canvas_create(DEFAULT_CANVAS_W, DEFAULT_CANVAS_H);
    ToolState ts;
    tools_init(&ts);

    UIState ui;
    ui_init(&ui, DEFAULT_WINDOW_W, DEFAULT_WINDOW_H);

    Uint32 last_frame_time = SDL_GetTicks();
    bool running = true;
    SDL_Event e;

    while (running) {
        if (ui.should_quit) {
            running = false;
            break;
        }

        Uint32 now = SDL_GetTicks();
        Uint32 delta_ms = now - last_frame_time;
        last_frame_time = now;

        /* Playback Frame Update */
        if (proj->is_playing && proj->frame_count > 1) {
            proj->play_accum_ms += delta_ms;
            Uint32 frame_delay = 1000 / (Uint32)proj->fps;
            if (proj->play_accum_ms >= frame_delay) {
                proj->play_accum_ms = 0;
                int next = proj->current_frame + 1;
                if (next >= proj->frame_count) {
                    if (proj->loop) next = 0;
                    else {
                        next = proj->frame_count - 1;
                        proj->is_playing = false;
                    }
                }
                canvas_set_current_frame(proj, next);
            }
        }

        /* Event Handling */
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        ui_resize(&ui, e.window.data1, e.window.data2);
                    }
                    break;

                case SDL_TEXTINPUT:
                    if (ts.text_active) {
                        tool_handle_text_input(&ts, e.text.text);
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    ui_handle_mouse_down(&ui, &proj, &ts, renderer, e.button.button, e.button.x, e.button.y);
                    break;

                case SDL_MOUSEBUTTONUP:
                    ui_handle_mouse_up(&ui, proj, &ts, e.button.button, e.button.x, e.button.y);
                    break;

                case SDL_MOUSEMOTION:
                    ui_handle_mouse_move(&ui, proj, &ts, e.motion.x, e.motion.y);
                    break;

                case SDL_MOUSEWHEEL:
                    ui_handle_mouse_wheel(&ui, e.wheel.y);
                    break;

                case SDL_KEYDOWN: {
                    /* If typing text in Text tool */
                    if (ts.text_active) {
                        if (e.key.keysym.sym == SDLK_BACKSPACE) {
                            tool_handle_text_backspace(&ts);
                        } else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                            tool_commit_text(&ts, proj);
                        } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                            tool_cancel_text(&ts);
                        }
                        break;
                    }

                    /* If active polygon drawing */
                    if (ts.poly_active) {
                        if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                            tool_commit_polygon(&ts, proj);
                            break;
                        } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                            tool_cancel_polygon(&ts);
                            break;
                        }
                    }

                    SDL_Keymod mod = SDL_GetModState();
                    bool ctrl = (mod & KMOD_CTRL);

                    if (ctrl && e.key.keysym.sym == SDLK_z) {
                        canvas_undo(proj);
                        ui_notify(&ui, "Undo");
                    } else if (ctrl && (e.key.keysym.sym == SDLK_y || ((mod & KMOD_SHIFT) && e.key.keysym.sym == SDLK_z))) {
                        canvas_redo(proj);
                        ui_notify(&ui, "Redo");
                    } else if (ctrl && e.key.keysym.sym == SDLK_s) {
                        bool ok = project_save(proj, "project.fpaint");
                        ui_notify(&ui, ok ? "Saved project.fpaint!" : export_get_last_error());
                    } else if (ctrl && e.key.keysym.sym == SDLK_o) {
                        CanvasProject *loaded = project_load("project.fpaint");
                        if (loaded) {
                            canvas_destroy(proj);
                            proj = loaded;
                            ui_notify(&ui, "Loaded project.fpaint!");
                        } else {
                            ui_notify(&ui, export_get_last_error());
                        }
                    } else if (ctrl && e.key.keysym.sym == SDLK_n) {
                        CanvasProject *fresh = canvas_create(proj->width, proj->height);
                        canvas_destroy(proj);
                        proj = fresh;
                        ui.pan_x = 0; ui.pan_y = 0;
                        ui_notify(&ui, "New Animation");
                    } else if (ctrl && e.key.keysym.sym == SDLK_e) {
                        ui_notify(&ui, "Exporting to output.mp4...");
                        SDL_RenderPresent(renderer);
                        bool ok = export_to_mp4(proj, "output.mp4", proj->fps);
                        ui_notify(&ui, ok ? "Exported output.mp4!" : export_get_last_error());
                    } else if (ctrl && e.key.keysym.sym == SDLK_g) {
                        ui_notify(&ui, "Exporting to output.gif...");
                        SDL_RenderPresent(renderer);
                        bool ok = export_to_gif(proj, "output.gif", proj->fps);
                        ui_notify(&ui, ok ? "Exported output.gif!" : export_get_last_error());
                    } else {
                        switch (e.key.keysym.sym) {
                            case SDLK_SPACE:
                                proj->is_playing = !proj->is_playing;
                                proj->canvas_dirty = true;
                                break;
                            case SDLK_LEFT:
                                if (proj->current_frame > 0) canvas_set_current_frame(proj, proj->current_frame - 1);
                                break;
                            case SDLK_RIGHT:
                                if (proj->current_frame < proj->frame_count - 1) canvas_set_current_frame(proj, proj->current_frame + 1);
                                break;
                            case SDLK_TAB:
                                canvas_toggle_active_layer(proj);
                                ui_notify(&ui, proj->active_layer == LAYER_BG ? "Editing Layer 0: Background" : "Editing Layer 1: Animation");
                                break;
                            case SDLK_o:
                                proj->onion_skin_enabled = !proj->onion_skin_enabled;
                                proj->canvas_dirty = true;
                                ui_notify(&ui, proj->onion_skin_enabled ? "Onion Skin: ON" : "Onion Skin: OFF");
                                break;
                            case SDLK_b: ts.lasso_active = false; ts.active_tool = TOOL_BRUSH; break;
                            case SDLK_p: ts.lasso_active = false; ts.active_tool = TOOL_PENCIL; break;
                            case SDLK_e: ts.lasso_active = false; ts.active_tool = TOOL_ERASER; break;
                            case SDLK_g: ts.lasso_active = false; ts.active_tool = TOOL_BUCKET; break;
                            case SDLK_i: ts.lasso_active = false; ts.active_tool = TOOL_EYEDROPPER; break;
                            case SDLK_l: ts.lasso_active = false; ts.active_tool = TOOL_LINE; break;
                            case SDLK_r: ts.lasso_active = false; ts.active_tool = TOOL_RECTANGLE; break;
                            case SDLK_c: ts.lasso_active = false; ts.active_tool = TOOL_ELLIPSE; break;
                            case SDLK_s: ts.lasso_active = false; ts.active_tool = TOOL_SELECT; break;
                            case SDLK_f: ts.lasso_active = false; ts.active_tool = TOOL_FREE_SELECT; break;
                            case SDLK_ESCAPE:
                                if (ui.active_menu >= 0) {
                                    ui.active_menu = -1;
                                } else if (proj->has_selection || ts.lasso_active) {
                                    ts.lasso_active = false;
                                    tool_cancel_selection(&ts, proj);
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                }
            }
        }

        ui_render(&ui, proj, &ts, renderer);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    canvas_destroy(proj);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
