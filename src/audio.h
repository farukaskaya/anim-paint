#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Voiceover track: a single mono S16 clip that can be recorded from the
 * microphone, imported from a WAV file, previewed, and muxed into the MP4
 * export by ffmpeg.
 */

#define VO_MAX_SECONDS     600          /* 10 minute safety cap          */
#define VO_DEFAULT_PATH    "voiceover.wav"
#define VO_ERROR_LEN       256

typedef struct {
    /* Loaded clip (mono, signed 16-bit, native sample rate). */
    bool loaded;
    bool enabled;                       /* Include in MP4 export?        */
    char path[512];
    int16_t *samples;
    int sample_count;
    int sample_rate;

    /* Preview playback */
    bool playing;
    uint32_t play_dev;

    /* Microphone recording */
    bool recording;
    uint32_t rec_dev;                   /* SDL_AudioDeviceID             */
    void *rec_stream;                   /* SDL_AudioStream*              */
    int16_t *rec_samples;
    int rec_count;
    int rec_capacity;
    uint32_t rec_start_ticks;
    int rec_max_samples;                /* 0 = use the 10 minute cap     */
    double rec_target_seconds;          /* Animation length to fill      */
    bool rec_limit_reached;             /* True once the take is full    */
    char rec_error[VO_ERROR_LEN];
} Voiceover;

void vo_init(Voiceover *vo);
void vo_shutdown(Voiceover *vo);

/* Load a WAV file (PCM 8/16/24/32-bit or 32-bit float, mono/stereo). */
bool vo_load_wav(Voiceover *vo, const char *path, char *err, size_t err_size);

/* Write the current clip back out as 16-bit mono WAV. */
bool vo_save_wav(const Voiceover *vo, const char *path, char *err, size_t err_size);

/* Recording lifecycle. Poll must be called regularly while recording.
   duration_seconds sets how much audio to capture (typically frames / fps);
   recording stops automatically once that much has been captured. */
bool vo_start_recording(Voiceover *vo, double duration_seconds, char *err, size_t err_size);
bool vo_poll_recording(Voiceover *vo);
void vo_stop_recording(Voiceover *vo);

/* Preview playback of the loaded clip. */
bool vo_play(Voiceover *vo, char *err, size_t err_size);
void vo_stop_playback(Voiceover *vo);
void vo_poll_playback(Voiceover *vo);

/* Drop the loaded clip. */
void vo_clear(Voiceover *vo);

/* Seconds of audio currently available (recorded or loaded). */
double vo_duration_seconds(const Voiceover *vo);

/* Path ffmpeg should read, or NULL when there is no usable voiceover. */
const char *vo_export_path(const Voiceover *vo);

#endif /* AUDIO_H */
