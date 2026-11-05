#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../src/canvas.h"
#include "../src/export.h"
#include "../src/tools.h"
#include "../src/audio.h"

int main() {
    printf("=== Running Faruk Paint Automated Tests ===\n");

    /* 1. Test Project Creation */
    CanvasProject *proj = canvas_create(320, 240);
    assert(proj != NULL);
    assert(proj->frame_count == 1);
    assert(proj->width == 320);
    assert(proj->height == 240);
    printf("[PASS] Project creation\n");

    /* 2. Test Layer 0 and Layer 1 default values */
    uint32_t bg_pixel = canvas_get_pixel(proj, 0, LAYER_BG, 10, 10);
    assert(bg_pixel == make_rgba(255, 255, 255, 255)); /* White BG */
    uint32_t anim_pixel = canvas_get_pixel(proj, 0, LAYER_ANIM, 10, 10);
    assert(anim_pixel == 0); /* Transparent ANIM */
    printf("[PASS] Layer default states (BG = white, ANIM = transparent)\n");

    /* 3. Test Drawing on Layers */
    canvas_set_pixel(proj, 0, LAYER_BG, 50, 50, make_rgba(100, 150, 200, 255));
    assert(canvas_get_pixel(proj, 0, LAYER_BG, 50, 50) == make_rgba(100, 150, 200, 255));

    canvas_set_pixel(proj, 0, LAYER_ANIM, 50, 50, make_rgba(255, 0, 0, 255));
    assert(canvas_get_pixel(proj, 0, LAYER_ANIM, 50, 50) == make_rgba(255, 0, 0, 255));
    printf("[PASS] Pixel access on both layers\n");

    /* 3b. Test free-form (lasso) selection and move */
    CanvasProject *sel_proj = canvas_create(64, 64);
    ToolState ts;
    tools_init(&ts);
    ts.active_tool = TOOL_FREE_SELECT;

    /* Draw a 10x10 red block on the animation layer */
    for (int y = 10; y < 20; y++) {
        for (int x = 10; x < 20; x++) {
            canvas_set_pixel(sel_proj, 0, LAYER_ANIM, x, y, make_rgba(255, 0, 0, 255));
        }
    }

    /* Lasso around the block */
    tool_on_mouse_down(&ts, sel_proj, SDL_BUTTON_LEFT, 5, 5);
    tool_on_mouse_move(&ts, sel_proj, 25, 5);
    tool_on_mouse_move(&ts, sel_proj, 25, 25);
    tool_on_mouse_move(&ts, sel_proj, 5, 25);
    tool_on_mouse_up(&ts, sel_proj, SDL_BUTTON_LEFT, 5, 5);

    assert(sel_proj->has_selection);
    assert(sel_proj->sel_w >= 20 && sel_proj->sel_h >= 20);
    assert(sel_proj->selection_mask != NULL);
    printf("[PASS] Free-form lasso creates a masked selection\n");

    /* Drag the selection +5,+5: source must be cut, destination receives pixels */
    tool_on_mouse_down(&ts, sel_proj, SDL_BUTTON_LEFT, 15, 15);
    tool_on_mouse_move(&ts, sel_proj, 20, 20);
    tool_on_mouse_up(&ts, sel_proj, SDL_BUTTON_LEFT, 20, 20);

    assert(canvas_get_pixel(sel_proj, 0, LAYER_ANIM, 11, 11) == 0);
    assert(canvas_get_pixel(sel_proj, 0, LAYER_ANIM, 21, 21) == make_rgba(255, 0, 0, 255));
    printf("[PASS] Lasso selection cuts and moves pixels\n");

    tool_cancel_selection(&ts, sel_proj);
    assert(!sel_proj->has_selection);
    canvas_destroy(sel_proj);

    /* 4. Test Adding and Duplicating Frames */
    int f1 = canvas_add_frame(proj, 1);
    assert(f1 == 1);
    assert(proj->frame_count == 2);

    /* Draw a moving red ball across 8 frames */
    for (int f = 1; f < 8; f++) {
        if (f > 1) canvas_add_frame(proj, f);
        /* Green ground on BG layer */
        for (int x = 0; x < 320; x++) {
            for (int y = 180; y < 240; y++) {
                canvas_set_pixel(proj, f, LAYER_BG, x, y, make_rgba(50, 180, 50, 255));
            }
        }
        /* Moving red ball on ANIM layer */
        int ball_x = 40 + f * 30;
        int ball_y = 100 + (f % 4) * 15;
        for (int dy = -10; dy <= 10; dy++) {
            for (int dx = -10; dx <= 10; dx++) {
                if (dx * dx + dy * dy <= 100) {
                    canvas_set_pixel(proj, f, LAYER_ANIM, ball_x + dx, ball_y + dy, make_rgba(230, 30, 30, 255));
                }
            }
        }
    }
    assert(proj->frame_count == 8);
    printf("[PASS] Multi-frame animation generated (8 frames)\n");

    /* 5. Test Project Save and Load */
    bool save_ok = project_save(proj, "test_proj.fpaint");
    assert(save_ok);

    CanvasProject *loaded = project_load("test_proj.fpaint");
    assert(loaded != NULL);
    assert(loaded->frame_count == 8);
    assert(loaded->width == 320);
    assert(loaded->height == 240);
    printf("[PASS] Binary project save and load (.fpaint)\n");
    canvas_destroy(loaded);
    remove("test_proj.fpaint");

    /* 6. Test Export to MP4 using ffmpeg */
    printf("Testing export to MP4...\n");
    bool mp4_ok = export_to_mp4(proj, "test_output.mp4", 12);
    assert(mp4_ok);
    printf("[PASS] Export to MP4 (test_output.mp4)\n");

    /* 7. Test Export to GIF using ffmpeg */
    printf("Testing export to GIF...\n");
    bool gif_ok = export_to_gif(proj, "test_output.gif", 12);
    assert(gif_ok);
    assert(strlen(export_get_last_error()) == 0); /* success clears the error */
    printf("[PASS] Export to GIF (test_output.gif)\n");

    /* 8. Voiceover: WAV round-trip */
    Voiceover vo;
    vo_init(&vo);

    const int rate = 44100;
    const int secs  = 2;
    const int count = rate * secs;
    int16_t *tone = (int16_t *)malloc((size_t)count * sizeof(int16_t));
    assert(tone != NULL);
    for (int i = 0; i < count; i++) {
        tone[i] = (int16_t)(12000.0 * sin(2.0 * 3.14159265358979 * 440.0 * i / rate));
    }
    vo.samples = tone;
    vo.sample_count = count;
    vo.sample_rate = rate;
    vo.loaded = true;
    vo.enabled = true;

    char err[VO_ERROR_LEN];
    err[0] = '\0';
    assert(vo_save_wav(&vo, "test_voiceover.wav", err, sizeof(err)));
    assert(vo_duration_seconds(&vo) > 1.99 && vo_duration_seconds(&vo) < 2.01);
    printf("[PASS] Voiceover WAV written (%.2fs)\n", vo_duration_seconds(&vo));

    Voiceover loaded_vo;
    vo_init(&loaded_vo);
    err[0] = '\0';
    assert(vo_load_wav(&loaded_vo, "test_voiceover.wav", err, sizeof(err)));
    assert(loaded_vo.loaded);
    assert(loaded_vo.sample_rate == rate);
    assert(loaded_vo.sample_count == count);
    assert(loaded_vo.samples[1000] == tone[1000]);
    printf("[PASS] Voiceover WAV loaded back with matching samples\n");

    /* 9. Export MP4 with the voiceover muxed in */
    printf("Testing MP4 export with voiceover...\n");
    const char *vo_path = vo_export_path(&loaded_vo);
    assert(vo_path != NULL);
    bool mp4_audio_ok = export_to_mp4_with_audio(proj, "test_audio.mp4", 12, vo_path);
    assert(mp4_audio_ok);
    assert(strlen(export_get_last_error()) == 0);

    /* Confirm ffmpeg actually wrote an audio stream into the container. */
    FILE *probe = popen("ffprobe -v error -select_streams a -show_entries stream=codec_type "
                        "-of csv=p=0 test_audio.mp4 2>/dev/null", "r");
    assert(probe != NULL);
    char probe_out[64] = {0};
    size_t got = fread(probe_out, 1, sizeof(probe_out) - 1, probe);
    pclose(probe);
    probe_out[got] = '\0';
    assert(strstr(probe_out, "audio") != NULL);
    printf("[PASS] MP4 export includes an audio track\n");

    /* 10. Voiceover toggle keeps the file out of the export when disabled */
    loaded_vo.enabled = false;
    assert(vo_export_path(&loaded_vo) == NULL);
    loaded_vo.enabled = true;
    printf("[PASS] Voiceover can be excluded from MP4 export\n");

    vo_shutdown(&loaded_vo);
    vo_shutdown(&vo);
    remove("test_voiceover.wav");
    remove("test_audio.mp4");

    /* 11. Error reporting */
    assert(!project_save(proj, "/nonexistent-dir/x.fpaint"));
    assert(strlen(export_get_last_error()) > 0);
    printf("[PASS] Failure reasons are reported\n");

    /* Cleanup */
    canvas_destroy(proj);
    printf("=== All Tests Passed Successfully! ===\n");
    return 0;
}
