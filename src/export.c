#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool export_to_mp4(CanvasProject *proj, const char *filepath, int fps) {
    if (!proj || !filepath || proj->frame_count <= 0) return false;
    if (fps <= 0) fps = 12;

    int w = proj->width;
    int h = proj->height;
    int total_pixels = w * h;

    uint32_t *frame_buffer = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
    if (!frame_buffer) return false;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - "
        "-c:v libx264 -pix_fmt yuv420p -movflags +faststart \"%s\" > /dev/null 2>&1",
        w, h, fps, filepath);

    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        free(frame_buffer);
        return false;
    }

    for (int i = 0; i < proj->frame_count; i++) {
        composite_frame_to_buffer(proj, i, frame_buffer);
        size_t written = fwrite(frame_buffer, sizeof(uint32_t), total_pixels, pipe);
        if (written < (size_t)total_pixels) {
            pclose(pipe);
            free(frame_buffer);
            return false;
        }
    }

    free(frame_buffer);
    int status = pclose(pipe);
    return (status == 0);
}

bool export_to_gif(CanvasProject *proj, const char *filepath, int fps) {
    if (!proj || !filepath || proj->frame_count <= 0) return false;
    if (fps <= 0) fps = 12;

    int w = proj->width;
    int h = proj->height;
    int total_pixels = w * h;

    uint32_t *frame_buffer = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
    if (!frame_buffer) return false;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - "
        "-filter_complex \"[0:v] split [a][b];[a] palettegen=reserve_transparent=0 [p];[b][p] paletteuse\" \"%s\" > /dev/null 2>&1",
        w, h, fps, filepath);

    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        free(frame_buffer);
        return false;
    }

    for (int i = 0; i < proj->frame_count; i++) {
        composite_frame_to_buffer(proj, i, frame_buffer);
        size_t written = fwrite(frame_buffer, sizeof(uint32_t), total_pixels, pipe);
        if (written < (size_t)total_pixels) {
            pclose(pipe);
            free(frame_buffer);
            return false;
        }
    }

    free(frame_buffer);
    int status = pclose(pipe);
    return (status == 0);
}

bool export_image_sequence(CanvasProject *proj, const char *pattern_path, int fps) {
    if (!proj || !pattern_path || proj->frame_count <= 0) return false;
    if (fps <= 0) fps = 12;

    int w = proj->width;
    int h = proj->height;
    int total_pixels = w * h;

    uint32_t *frame_buffer = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
    if (!frame_buffer) return false;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - \"%s\" > /dev/null 2>&1",
        w, h, fps, pattern_path);

    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        free(frame_buffer);
        return false;
    }

    for (int i = 0; i < proj->frame_count; i++) {
        composite_frame_to_buffer(proj, i, frame_buffer);
        fwrite(frame_buffer, sizeof(uint32_t), total_pixels, pipe);
    }

    free(frame_buffer);
    int status = pclose(pipe);
    return (status == 0);
}

/* Save project format:
   Magic "ANP1" (4 bytes)
   width (int32)
   height (int32)
   frame_count (int32)
   fps (int32)
   For each frame:
     Layer 0 pixels (width * height * 4)
     Layer 1 pixels (width * height * 4)
*/
bool project_save(CanvasProject *proj, const char *filepath) {
    if (!proj || !filepath) return false;
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    const char magic[4] = { 'A', 'N', 'P', '1' };
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
    if (!filepath) return NULL;
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "ANP1", 4) != 0) {
        fclose(f);
        return NULL;
    }

    int32_t header[4];
    if (fread(header, sizeof(int32_t), 4, f) != 4) {
        fclose(f);
        return NULL;
    }

    int w = header[0];
    int h = header[1];
    int frame_count = header[2];
    int fps = header[3];

    if (w <= 0 || h <= 0 || frame_count <= 0) {
        fclose(f);
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
