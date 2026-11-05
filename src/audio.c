#include "audio.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define VO_REC_RATE 44100

static void set_error(char *err, size_t err_size, const char *fmt, ...) {
    if (!err || err_size == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

void vo_init(Voiceover *vo) {
    memset(vo, 0, sizeof(*vo));
    vo->enabled = true;
}

void vo_clear(Voiceover *vo) {
    if (!vo) return;
    vo_stop_playback(vo);
    free(vo->samples);
    vo->samples = NULL;
    vo->sample_count = 0;
    vo->sample_rate = 0;
    vo->loaded = false;
    vo->path[0] = '\0';
}

void vo_shutdown(Voiceover *vo) {
    if (!vo) return;
    if (vo->recording) vo_stop_recording(vo);
    vo_clear(vo);
    free(vo->rec_samples);
    vo->rec_samples = NULL;
    vo->rec_count = 0;
    vo->rec_capacity = 0;
    vo_stop_playback(vo);
}

double vo_duration_seconds(const Voiceover *vo) {
    if (!vo) return 0.0;
    if (vo->recording) {
        if (vo->rec_count <= 0) return 0.0;
        return (double)vo->rec_count / (double)VO_REC_RATE;
    }
    if (!vo->loaded || vo->sample_rate <= 0) return 0.0;
    return (double)vo->sample_count / (double)vo->sample_rate;
}

const char *vo_export_path(const Voiceover *vo) {
    if (!vo || !vo->loaded || !vo->enabled) return NULL;
    if (vo->path[0] == '\0') return NULL;
    return vo->path;
}

/* ------------------------------------------------------------------ */
/* WAV reading                                                         */
/* ------------------------------------------------------------------ */

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int16_t clamp_s16(float v) {
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)lrintf(v);
}

/* Convert interleaved sample data to mono S16. */
static int16_t *wav_to_mono_s16(const uint8_t *data, size_t data_size,
                                int format, int bits, int channels,
                                int *out_count) {
    *out_count = 0;
    if (!data || data_size == 0 || channels <= 0) return NULL;

    int bytes_per_sample = bits / 8;
    if (bytes_per_sample <= 0) return NULL;
    size_t frame_bytes = (size_t)bytes_per_sample * (size_t)channels;
    if (frame_bytes == 0) return NULL;

    size_t frames = data_size / frame_bytes;
    if (frames == 0) return NULL;

    int16_t *out = (int16_t *)malloc(frames * sizeof(int16_t));
    if (!out) return NULL;

    for (size_t f = 0; f < frames; f++) {
        float acc = 0.0f;
        for (int c = 0; c < channels; c++) {
            const uint8_t *p = data + f * frame_bytes + (size_t)c * (size_t)bytes_per_sample;
            float v = 0.0f;

            if (format == 3 && bits == 32) {
                uint32_t raw = rd_u32(p);
                float fv;
                memcpy(&fv, &raw, sizeof(fv));
                v = fv;
            } else if (format == 1) {
                switch (bits) {
                    case 8:  v = ((int)p[0] - 128) / 128.0f; break;
                    case 16: v = (float)(int16_t)rd_u16(p) / 32768.0f; break;
                    case 24: {
                        int32_t s = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24);
                        v = (float)(s >> 8) / 8388608.0f;
                        break;
                    }
                    case 32: {
                        int32_t s = (int32_t)rd_u32(p);
                        v = (float)(s >> 16) / 32768.0f;
                        break;
                    }
                    default: v = 0.0f; break;
                }
            }
            acc += v;
        }
        acc /= (float)channels;
        out[f] = clamp_s16(acc * 32767.0f);
    }

    *out_count = (int)frames;
    return out;
}

bool vo_load_wav(Voiceover *vo, const char *path, char *err, size_t err_size) {
    if (!vo || !path) {
        set_error(err, err_size, "No voiceover file given.");
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(err, err_size, "Could not open %s.", path);
        return false;
    }

    uint8_t riff[12];
    if (fread(riff, 1, sizeof(riff), f) != sizeof(riff) ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        fclose(f);
        set_error(err, err_size, "%s is not a WAV file.", path);
        return false;
    }

    int format = 0;
    int channels = 0;
    int bits = 0;
    int sample_rate = 0;
    uint8_t *data = NULL;
    size_t data_size = 0;

    uint8_t chunk[8];
    while (fread(chunk, 1, sizeof(chunk), f) == sizeof(chunk)) {
        uint32_t size = rd_u32(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            if (size < 16) break;
            uint8_t fmt[40];
            size_t to_read = size < sizeof(fmt) ? size : sizeof(fmt);
            if (fread(fmt, 1, to_read, f) != to_read) break;
            format = rd_u16(fmt);
            channels = rd_u16(fmt + 2);
            sample_rate = (int)rd_u32(fmt + 4);
            bits = rd_u16(fmt + 14);
            if (format == 0xFFFE && to_read >= 26) {
                /* WAVE_FORMAT_EXTENSIBLE: real format lives in the sub-format GUID. */
                format = rd_u16(fmt + 24);
            }
            if (size > to_read) fseek(f, (long)(size - to_read), SEEK_CUR);
        } else if (memcmp(chunk, "data", 4) == 0) {
            data = (uint8_t *)malloc(size ? size : 1);
            if (!data) {
                fclose(f);
                set_error(err, err_size, "Out of memory reading %s.", path);
                return false;
            }
            data_size = fread(data, 1, size, f);
            break;
        } else {
            fseek(f, (long)size + (size & 1), SEEK_CUR);
        }
        if (size & 1) fseek(f, 1, SEEK_CUR);
    }
    fclose(f);

    if (!data) {
        set_error(err, err_size, "%s has no audio data.", path);
        return false;
    }

    if ((format != 1 && format != 3) || channels <= 0 || bits <= 0) {
        free(data);
        set_error(err, err_size, "%s uses an unsupported WAV encoding.", path);
        return false;
    }

    int count = 0;
    int16_t *mono = wav_to_mono_s16(data, data_size, format, bits, channels, &count);
    free(data);

    if (!mono || count <= 0) {
        free(mono);
        set_error(err, err_size, "%s contained no usable samples.", path);
        return false;
    }

    if (sample_rate <= 0) sample_rate = VO_REC_RATE;

    vo_stop_playback(vo);
    free(vo->samples);
    vo->samples = mono;
    vo->sample_count = count;
    vo->sample_rate = sample_rate;
    vo->loaded = true;
    vo->enabled = true;
    strncpy(vo->path, path, sizeof(vo->path) - 1);
    vo->path[sizeof(vo->path) - 1] = '\0';
    return true;
}

/* ------------------------------------------------------------------ */
/* WAV writing                                                         */
/* ------------------------------------------------------------------ */

static void wr_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void wr_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static bool write_wav(const int16_t *samples, int count, int sample_rate, const char *path,
                      char *err, size_t err_size) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_error(err, err_size, "Could not write %s.", path);
        return false;
    }

    uint32_t data_bytes = (uint32_t)count * 2u;
    uint8_t header[44];
    memcpy(header, "RIFF", 4);
    wr_u32(header + 4, 36 + data_bytes);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    wr_u32(header + 16, 16);
    wr_u16(header + 20, 1);                 /* PCM */
    wr_u16(header + 22, 1);                 /* mono */
    wr_u32(header + 24, (uint32_t)sample_rate);
    wr_u32(header + 28, (uint32_t)sample_rate * 2u);
    wr_u16(header + 32, 2);
    wr_u16(header + 34, 16);
    memcpy(header + 36, "data", 4);
    wr_u32(header + 40, data_bytes);

    bool ok = fwrite(header, 1, sizeof(header), f) == sizeof(header);
    if (ok && count > 0) {
        ok = fwrite(samples, sizeof(int16_t), (size_t)count, f) == (size_t)count;
    }
    fclose(f);

    if (!ok) {
        set_error(err, err_size, "Failed while writing %s.", path);
        return false;
    }
    return true;
}

bool vo_save_wav(const Voiceover *vo, const char *path, char *err, size_t err_size) {
    if (!vo || !vo->loaded || vo->sample_count <= 0) {
        set_error(err, err_size, "There is no voiceover to save.");
        return false;
    }
    return write_wav(vo->samples, vo->sample_count, vo->sample_rate, path, err, err_size);
}

/* ------------------------------------------------------------------ */
/* Recording                                                           */
/* ------------------------------------------------------------------ */

static bool rec_reserve(Voiceover *vo, int extra) {
    int need = vo->rec_count + extra;
    if (need <= vo->rec_capacity) return true;

    int hard_cap = VO_REC_RATE * VO_MAX_SECONDS;
    int limit = (vo->rec_max_samples > 0 && vo->rec_max_samples < hard_cap)
              ? vo->rec_max_samples : hard_cap;

    if (need > limit) need = limit;
    if (need <= vo->rec_capacity) return true;

    int cap = vo->rec_capacity > 0 ? vo->rec_capacity : VO_REC_RATE;
    while (cap < need) cap *= 2;
    if (cap > limit) cap = limit;
    if (cap < need) cap = need;

    int16_t *grown = (int16_t *)realloc(vo->rec_samples, (size_t)cap * sizeof(int16_t));
    if (!grown) return false;
    vo->rec_samples = grown;
    vo->rec_capacity = cap;
    return true;
}

bool vo_start_recording(Voiceover *vo, double duration_seconds, char *err, size_t err_size) {
    if (!vo) return false;
    if (vo->recording) return true;

    /* Capture exactly one animation pass: frames / fps seconds. */
    if (duration_seconds <= 0.05) duration_seconds = 1.0;

    int64_t want_samples = (int64_t)(duration_seconds * (double)VO_REC_RATE + 0.5);
    int hard_cap = VO_REC_RATE * VO_MAX_SECONDS;
    if (want_samples > hard_cap) want_samples = hard_cap;
    if (want_samples < 1) want_samples = 1;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        set_error(err, err_size, "Audio unavailable: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    want.freq = VO_REC_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = NULL;

    vo->rec_dev = SDL_OpenAudioDevice(NULL, 1, &want, &have,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE |
        SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (vo->rec_dev == 0) {
        set_error(err, err_size, "No microphone available: %s", SDL_GetError());
        return false;
    }

    SDL_AudioStream *stream = SDL_NewAudioStream(
        have.format, have.channels, have.freq,
        AUDIO_S16SYS, 1, VO_REC_RATE);
    if (!stream) {
        SDL_CloseAudioDevice(vo->rec_dev);
        vo->rec_dev = 0;
        set_error(err, err_size, "Could not prepare microphone stream: %s", SDL_GetError());
        return false;
    }
    vo->rec_stream = stream;

    vo->rec_count = 0;
    vo->rec_start_ticks = SDL_GetTicks();
    vo->rec_max_samples = (int)want_samples;
    vo->rec_target_seconds = (double)want_samples / (double)VO_REC_RATE;
    vo->rec_limit_reached = false;
    vo->recording = true;
    vo->rec_error[0] = '\0';
    SDL_PauseAudioDevice(vo->rec_dev, 0);
    return true;
}

bool vo_poll_recording(Voiceover *vo) {
    if (!vo || !vo->recording || vo->rec_dev == 0) return true;

    uint8_t buf[4096];
    uint32_t queued;
    while ((queued = SDL_DequeueAudio(vo->rec_dev, buf, sizeof(buf))) > 0) {
        SDL_AudioStream *stream = (SDL_AudioStream *)vo->rec_stream;
        if (!stream) break;

        if (SDL_AudioStreamPut(stream, buf, (int)queued) < 0) {
            snprintf(vo->rec_error, sizeof(vo->rec_error), "Recording error: %s", SDL_GetError());
            return false;
        }

        for (;;) {
            int avail = SDL_AudioStreamAvailable(stream);
            if (avail < (int)sizeof(int16_t)) break;

            int chunk = avail;
            if (chunk > 8192) chunk = 8192;
            chunk -= chunk % (int)sizeof(int16_t);

            int16_t tmp[4096];
            if (chunk > (int)sizeof(tmp)) chunk = (int)sizeof(tmp);

            int got = SDL_AudioStreamGet(stream, tmp, chunk);
            if (got <= 0) break;

            int n = got / (int)sizeof(int16_t);
            if (n <= 0) break;

            if (!rec_reserve(vo, n)) {
                snprintf(vo->rec_error, sizeof(vo->rec_error),
                         "Recording stopped: ran out of memory.");
                return false;
            }

            int room = vo->rec_max_samples - vo->rec_count;
            if (room <= 0) {
                vo->rec_limit_reached = true;
                return false;
            }
            if (n > room) n = room;

            memcpy(vo->rec_samples + vo->rec_count, tmp, (size_t)n * sizeof(int16_t));
            vo->rec_count += n;

            /* Stop as soon as the animation-length take is complete. */
            if (vo->rec_count >= vo->rec_max_samples) {
                vo->rec_count = vo->rec_max_samples;
                vo->rec_limit_reached = true;
                return false;
            }
        }
    }

    return true;
}

void vo_stop_recording(Voiceover *vo) {
    if (!vo || !vo->recording) return;

    /* Drain whatever is still buffered before closing. */
    vo_poll_recording(vo);

    SDL_PauseAudioDevice(vo->rec_dev, 1);
    SDL_CloseAudioDevice(vo->rec_dev);
    vo->rec_dev = 0;

    if (vo->rec_stream) {
        SDL_FreeAudioStream((SDL_AudioStream *)vo->rec_stream);
        vo->rec_stream = NULL;
    }

    vo->recording = false;

    if (vo->rec_count <= 0) {
        snprintf(vo->rec_error, sizeof(vo->rec_error),
                 "Nothing was recorded. Check that a microphone is connected and allowed.");
        return;
    }

    /* An auto-completed take is padded/trimmed to the animation length so the
       voiceover lines up frame-for-frame with the exported video. */
    if (vo->rec_limit_reached && vo->rec_max_samples > 0) {
        if (vo->rec_count < vo->rec_max_samples) {
            if (rec_reserve(vo, vo->rec_max_samples - vo->rec_count)) {
                memset(vo->rec_samples + vo->rec_count, 0,
                       (size_t)(vo->rec_max_samples - vo->rec_count) * sizeof(int16_t));
                vo->rec_count = vo->rec_max_samples;
            }
        } else {
            vo->rec_count = vo->rec_max_samples;
        }
    }

    char err[VO_ERROR_LEN];
    err[0] = '\0';
    if (!write_wav(vo->rec_samples, vo->rec_count, VO_REC_RATE, VO_DEFAULT_PATH, err, sizeof(err))) {
        snprintf(vo->rec_error, sizeof(vo->rec_error), "%s", err);
        return;
    }

    /* Adopt the recording as the active voiceover clip. */
    int16_t *copy = (int16_t *)malloc((size_t)vo->rec_count * sizeof(int16_t));
    if (!copy) {
        snprintf(vo->rec_error, sizeof(vo->rec_error), "Out of memory after recording.");
        return;
    }
    memcpy(copy, vo->rec_samples, (size_t)vo->rec_count * sizeof(int16_t));

    vo_stop_playback(vo);
    free(vo->samples);
    vo->samples = copy;
    vo->sample_count = vo->rec_count;
    vo->sample_rate = VO_REC_RATE;
    vo->loaded = true;
    vo->enabled = true;
    strncpy(vo->path, VO_DEFAULT_PATH, sizeof(vo->path) - 1);
    vo->path[sizeof(vo->path) - 1] = '\0';
}

/* ------------------------------------------------------------------ */
/* Playback preview                                                    */
/* ------------------------------------------------------------------ */

void vo_stop_playback(Voiceover *vo) {
    if (!vo || vo->play_dev == 0) return;
    SDL_ClearQueuedAudio(vo->play_dev);
    SDL_CloseAudioDevice(vo->play_dev);
    vo->play_dev = 0;
    vo->playing = false;
}

bool vo_play(Voiceover *vo, char *err, size_t err_size) {
    if (!vo || !vo->loaded || vo->sample_count <= 0) {
        set_error(err, err_size, "No voiceover loaded.");
        return false;
    }

    vo_stop_playback(vo);

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        set_error(err, err_size, "Audio unavailable: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    want.freq = vo->sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;

    vo->play_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE |
        SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (vo->play_dev == 0) {
        set_error(err, err_size, "Could not open audio output: %s", SDL_GetError());
        return false;
    }

    SDL_AudioStream *stream = SDL_NewAudioStream(
        AUDIO_S16SYS, 1, vo->sample_rate,
        have.format, have.channels, have.freq);
    if (!stream) {
        vo_stop_playback(vo);
        set_error(err, err_size, "Could not prepare playback: %s", SDL_GetError());
        return false;
    }

    bool ok = true;
    if (SDL_AudioStreamPut(stream, vo->samples,
                           vo->sample_count * (int)sizeof(int16_t)) < 0) {
        ok = false;
    }
    if (ok) SDL_AudioStreamFlush(stream);

    if (ok) {
        for (;;) {
            int avail = SDL_AudioStreamAvailable(stream);
            if (avail < (int)sizeof(int16_t)) break;

            uint8_t chunk[8192];
            int want_bytes = avail < (int)sizeof(chunk) ? avail : (int)sizeof(chunk);
            int got = SDL_AudioStreamGet(stream, chunk, want_bytes);
            if (got <= 0) break;
            if (SDL_QueueAudio(vo->play_dev, chunk, (Uint32)got) < 0) {
                ok = false;
                break;
            }
        }
    }

    SDL_FreeAudioStream(stream);

    if (!ok) {
        vo_stop_playback(vo);
        set_error(err, err_size, "Playback failed: %s", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(vo->play_dev, 0);
    vo->playing = true;
    return true;
}

void vo_poll_playback(Voiceover *vo) {
    if (!vo || !vo->playing || vo->play_dev == 0) return;
    if (SDL_GetQueuedAudioSize(vo->play_dev) == 0) {
        vo_stop_playback(vo);
    }
}
