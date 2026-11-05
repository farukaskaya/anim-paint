#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ffmpeg is invoked through a shell pipe. Windows needs _popen/_pclose and NUL. */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define FP_POPEN  _popen
#define FP_PCLOSE _pclose
#define FP_NULL_DEVICE "NUL"
#else
#define FP_POPEN  popen
#define FP_PCLOSE pclose
#define FP_NULL_DEVICE "/dev/null"
#endif

/* Last export failure, shown in the status bar. */
static char g_export_error[512] = "";
static char g_ffmpeg_path[1024] = "";

const char *export_get_last_error(void) {
    return g_export_error;
}

static void export_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_export_error, sizeof(g_export_error), fmt, ap);
    va_end(ap);
}

static void export_clear_error(void) {
    g_export_error[0] = '\0';
}

/* Windows _popen defaults to text mode, which rewrites 0x0A as 0x0D 0x0A and
   corrupts raw RGBA frame data. Always force the pipe into binary mode. */
static void export_pipe_binary(FILE *pipe) {
#ifdef _WIN32
    _setmode(_fileno(pipe), _O_BINARY);
#else
    (void)pipe;
#endif
}

/* Check that ffmpeg can be executed at all, so we can report "not found"
   instead of a generic export failure. */
static bool export_ffmpeg_available(void) {
    if (g_ffmpeg_path[0] != '\0') return true;

    FILE *probe = FP_POPEN("ffmpeg -version", "r");
    if (probe) {
        export_pipe_binary(probe);
        char buf[16];
        size_t got = fread(buf, 1, sizeof(buf), probe);
        int status = FP_PCLOSE(probe);
        if (got > 0 && status == 0) {
            strncpy(g_ffmpeg_path, "ffmpeg", sizeof(g_ffmpeg_path) - 1);
            return true;
        }
    }

    /* Windows: PATH changes made while the app was open are not inherited by
       already-running processes, so also try the usual install locations. */
#ifdef _WIN32
    static const char *candidates[] = {
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files (x86)\\ffmpeg\\bin\\ffmpeg.exe",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (GetFileAttributesA(candidates[i]) != INVALID_FILE_ATTRIBUTES) {
            snprintf(g_ffmpeg_path, sizeof(g_ffmpeg_path), "\"%s\"", candidates[i]);
            return true;
        }
    }
#endif

    export_set_error("FFmpeg not found. Install FFmpeg or add ffmpeg.exe to PATH, then restart Faruk Paint.");
    return false;
}

/* Build the stderr log path so failed exports leave a readable diagnostic. */
static void export_log_path(const char *filepath, char *out, size_t out_size) {
    snprintf(out, out_size, "%s.ffmpeg.log", filepath);
}

/* Helper to composite a single frame (Layer 0 + Layer 1) into an RGBA buffer */
static void composite_frame_to_buffer(CanvasProject *proj, int frame_idx, uint32_t *out_buffer) {
    int total_pixels = proj->width * proj->height;
    Frame *f = proj->frames[frame_idx];

    /* Start with background layer */
    memcpy(out_buffer, f->layers[LAYER_BG], total_pixels * sizeof(uint32_t));

    /* Alpha blend animation layer on top */
    uint32_t *anim = f->layers[LAYER_ANIM];
    for (int i = 0; i < total_pixels; i++) {
        uint32_t src = anim[i];
        uint8_t sa = (uint8_t)((src >> 24) & 0xFF);
        if (sa == 0) continue;

        if (sa == 255) {
            out_buffer[i] = src;
        } else {
            uint8_t sr = (uint8_t)(src & 0xFF);
            uint8_t sg = (uint8_t)((src >> 8) & 0xFF);
            uint8_t sb = (uint8_t)((src >> 16) & 0xFF);

            uint32_t dst = out_buffer[i];
            uint8_t dr = (uint8_t)(dst & 0xFF);
            uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
            uint8_t db = (uint8_t)((dst >> 16) & 0xFF);

            float a_src = sa / 255.0f;
            float a_dst = 1.0f - a_src;

            uint8_t r = (uint8_t)(sr * a_src + dr * a_dst);
            uint8_t g = (uint8_t)(sg * a_src + dg * a_dst);
            uint8_t b = (uint8_t)(sb * a_src + db * a_dst);

            out_buffer[i] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}

/* Shared encode driver: run ffmpeg, stream every frame into it, report errors. */
static bool export_run(CanvasProject *proj, const char *filepath, int fps,
                       const char *ffmpeg_args, const char *format_name) {
    export_clear_error();

    if (!proj || !filepath || proj->frame_count <= 0) {
        export_set_error("Nothing to export (no frames).");
        return false;
    }
    if (fps <= 0) fps = 12;

    if (!export_ffmpeg_available()) return false;

    int w = proj->width;
    int h = proj->height;
    int total_pixels = w * h;

    uint32_t *frame_buffer = (uint32_t *)malloc((size_t)total_pixels * sizeof(uint32_t));
    if (!frame_buffer) {
        export_set_error("Out of memory while preparing %s export.", format_name);
        return false;
    }

    char log_path[1024];
    export_log_path(filepath, log_path, sizeof(log_path));

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "%s -y -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - %s \"%s\" 2> \"%s\"",
        g_ffmpeg_path, w, h, fps, ffmpeg_args, filepath, log_path);

    FILE *pipe = FP_POPEN(cmd, "w");
    if (!pipe) {
        free(frame_buffer);
        export_set_error("Could not start FFmpeg. Check that it is installed and on PATH.");
        return false;
    }
    export_pipe_binary(pipe);

    for (int i = 0; i < proj->frame_count; i++) {
        composite_frame_to_buffer(proj, i, frame_buffer);
        size_t written = fwrite(frame_buffer, sizeof(uint32_t), (size_t)total_pixels, pipe);
        if (written < (size_t)total_pixels) {
            FP_PCLOSE(pipe);
            free(frame_buffer);
            export_set_error("Failed while writing frames to FFmpeg. See %s", log_path);
            return false;
        }
    }

    free(frame_buffer);
    int status = FP_PCLOSE(pipe);

    if (status != 0) {
        export_set_error("FFmpeg failed (exit code %d). See %s", status, log_path);
        return false;
    }

    remove(log_path);
    return true;
}

bool export_to_mp4(CanvasProject *proj, const char *filepath, int fps) {
    return export_run(proj, filepath, fps,
        "-c:v libx264 -pix_fmt yuv420p -movflags +faststart",
        "MP4");
}

bool export_to_gif(CanvasProject *proj, const char *filepath, int fps) {
    return export_run(proj, filepath, fps,
        "-filter_complex \"[0:v] split [a][b];[a] palettegen=reserve_transparent=0 [p];[b][p] paletteuse\"",
        "GIF");
}

bool export_image_sequence(CanvasProject *proj, const char *pattern_path, int fps) {
    return export_run(proj, pattern_path, fps, "", "image sequence");
}

/* Save project format:
   Magic "FPNT" (4 bytes)
   width (int32)
   height (int32)
   frame_count (int32)
   fps (int32)
   For each frame:
     Layer 0 pixels (width * height * 4)
     Layer 1 pixels (width * height * 4)
*/
bool project_save(CanvasProject *proj, const char *filepath) {
    export_clear_error();
    if (!proj || !filepath) {
        export_set_error("Invalid project or file path.");
        return false;
    }
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        export_set_error("Could not open %s for writing.", filepath);
        return false;
    }

    const char magic[4] = { 'F', 'P', 'N', 'T' };
    fwrite(magic, 1, 4, f);

    int32_t header[4] = {
        (int32_t)proj->width,
        (int32_t)proj->height,
        (int32_t)proj->frame_count,
        (int32_t)proj->fps
    };
    fwrite(header, sizeof(int32_t), 4, f);

    int total_pixels = proj->width * proj->height;
    for (int i = 0; i < proj->frame_count; i++) {
        fwrite(proj->frames[i]->layers[LAYER_BG], sizeof(uint32_t), total_pixels, f);
        fwrite(proj->frames[i]->layers[LAYER_ANIM], sizeof(uint32_t), total_pixels, f);
    }

    fclose(f);
    return true;
}

CanvasProject *project_load(const char *filepath) {
    export_clear_error();
    if (!filepath) {
        export_set_error("No file path given.");
        return NULL;
    }
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        export_set_error("Could not open %s.", filepath);
        return NULL;
    }

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "FPNT", 4) != 0) {
        fclose(f);
        export_set_error("%s is not a valid .fpaint project.", filepath);
        return NULL;
    }

    int32_t header[4];
    if (fread(header, sizeof(int32_t), 4, f) != 4) {
        fclose(f);
        export_set_error("%s is truncated or corrupted.", filepath);
        return NULL;
    }

    int w = header[0];
    int h = header[1];
    int frame_count = header[2];
    int fps = header[3];

    if (w <= 0 || h <= 0 || frame_count <= 0) {
        fclose(f);
        export_set_error("%s has invalid dimensions or frame count.", filepath);
        return NULL;
    }

    CanvasProject *proj = canvas_create(w, h);
    proj->fps = fps;

    /* Add additional frames up to frame_count */
    for (int i = 1; i < frame_count; i++) {
        canvas_add_frame(proj, i);
    }

    int total_pixels = w * h;
    for (int i = 0; i < frame_count; i++) {
        fread(proj->frames[i]->layers[LAYER_BG], sizeof(uint32_t), total_pixels, f);
        fread(proj->frames[i]->layers[LAYER_ANIM], sizeof(uint32_t), total_pixels, f);
        proj->frames[i]->thumb_dirty = true;
    }

    proj->current_frame = 0;
    proj->canvas_dirty = true;

    fclose(f);
    return proj;
}
