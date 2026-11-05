#ifndef EXPORT_H
#define EXPORT_H

#include "canvas.h"
#include <stdbool.h>

/* Export animation to MP4 video using ffmpeg */
bool export_to_mp4(CanvasProject *proj, const char *filepath, int fps);

/* Export MP4 with a voiceover track muxed in (audio_path may be NULL). */
bool export_to_mp4_with_audio(CanvasProject *proj, const char *filepath, int fps,
                              const char *audio_path);

/* Export animation to high-quality GIF using ffmpeg */
bool export_to_gif(CanvasProject *proj, const char *filepath, int fps);

/* Export individual frames as PNG / image sequence using ffmpeg */
bool export_image_sequence(CanvasProject *proj, const char *pattern_path, int fps);

/* Save and load native Faruk Paint project file (.fpaint) */
bool project_save(CanvasProject *proj, const char *filepath);
CanvasProject *project_load(const char *filepath);

/* Human-readable reason for the most recent export/save/load failure. */
const char *export_get_last_error(void);

#endif /* EXPORT_H */
