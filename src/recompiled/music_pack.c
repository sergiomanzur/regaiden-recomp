#include "music_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>

/* Platform headers must come first: stb_vorbis defines short macro names that
 * collide with member names in winnt.h if it is included ahead of them. */
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define STB_VORBIS_NO_PUSHDATA_API
#include "runtime/vendor/stb/stb_vorbis.c"

/*
 * Replacement soundtrack.
 *
 * Mirrors hd_pack: the user drops audio into music_pack/ and it overrides the
 * emulated music. Nothing ships with the project - the files are user supplied,
 * exactly like the ROM.
 *
 * Files are named track_<id>.ogg (or .wav), where <id> is the guest music id
 * the track replaces, so the music follows the game rather than playing as a
 * detached playlist.
 *
 * Game Boy music and sound effects share the same four APU channels, so there
 * is no way to silence only the music. Instead the emulated APU is ducked to
 * MusicPackConfig::duck_percent while a replacement track is audible, which
 * keeps gunshots, doors and menu blips underneath the new track.
 */

#define MUSIC_SAMPLE_RATE 44100
#define FADE_FRAMES (MUSIC_SAMPLE_RATE / 4)   /* 250 ms fade-in */
/* Tracks are decoded to PCM up front so the audio callback never decodes. */
#define MAX_DECODED_BYTES (96u * 1024u * 1024u)

typedef struct {
    char name[64];
    int  track_id;      /* parsed from the filename, -1 if absent */
    char path[512];
    bool has_loop;
    uint32_t loop_start; /* in 44.1 kHz stereo sample frames */
    uint32_t loop_end;   /* in 44.1 kHz stereo sample frames (0 = full length) */
} MusicTrackEntry;

MusicPackConfig g_music_pack_config = {
    .enabled = false,
    .pack_dir = "music_pack",
    .volume = 85,
    .duck_percent = 25,
    .loop = true,
    .loaded_count = 0
};

static MusicTrackEntry s_tracks[MUSIC_PACK_MAX_TRACKS];
static int s_track_count = 0;
static SDL_AudioDeviceID s_audio_device = 0;

/* Active stream. Only mutated with the audio device locked. */
static int16_t* s_pcm = NULL;          /* interleaved stereo, 44100 Hz */
static uint32_t s_pcm_frames = 0;
static uint32_t s_pcm_pos = 0;
static int  s_current_track = MUSIC_PACK_NO_TRACK;
static int  s_fade_in_remaining = 0;
static bool s_has_loop = false;
static uint32_t s_loop_start = 0;
static uint32_t s_loop_end = 0;

static void lock_audio(void)   { if (s_audio_device) SDL_LockAudioDevice(s_audio_device); }
static void unlock_audio(void) { if (s_audio_device) SDL_UnlockAudioDevice(s_audio_device); }

void music_pack_set_audio_device(uint32_t device_id) {
    s_audio_device = (SDL_AudioDeviceID)device_id;
}

int music_pack_track_count(void) { return s_track_count; }

const char* music_pack_track_name(int index) {
    return (index >= 0 && index < s_track_count) ? s_tracks[index].name : "";
}

int music_pack_track_id(int index) {
    return (index >= 0 && index < s_track_count) ? s_tracks[index].track_id : -1;
}

bool music_pack_track_has_loop(int index) {
    return (index >= 0 && index < s_track_count) ? s_tracks[index].has_loop : false;
}

uint32_t music_pack_track_loop_start(int index) {
    return (index >= 0 && index < s_track_count) ? s_tracks[index].loop_start : 0;
}

uint32_t music_pack_track_loop_end(int index) {
    return (index >= 0 && index < s_track_count) ? s_tracks[index].loop_end : 0;
}

int music_pack_current_track(void) { return s_current_track; }

bool music_pack_is_playing(void) {
    return g_music_pack_config.enabled && s_pcm != NULL && s_pcm_frames > 0;
}

bool music_pack_current_has_loop(void) {
    return s_has_loop;
}

uint32_t music_pack_current_loop_start(void) {
    return s_loop_start;
}

uint32_t music_pack_current_loop_end(void) {
    return s_loop_end;
}

void music_pack_format_timestamp(uint32_t frames, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) return;
    uint32_t total_sec = frames / MUSIC_SAMPLE_RATE;
    uint32_t min = total_sec / 60;
    uint32_t sec = total_sec % 60;
    uint32_t ms = (uint32_t)(((uint64_t)(frames % MUSIC_SAMPLE_RATE) * 1000u) / MUSIC_SAMPLE_RATE);
    snprintf(buf, buf_size, "%02u:%02u.%03u", min, sec, ms);
}

const char* music_pack_get_track_description(int track_id) {
    switch (track_id) {
        case 0: return "Silence / Transition";
        case 1: return "Battle";
        case 2: return "Introduction (Part 1 - Capcom)";
        case 3: return "Introduction (Part 2 - Virgin & M4)";
        case 4: return "Introduction (Part 3 - In-game Cutscene)";
        case 5: return "Restaurant";
        case 6: return "Restaurant (Alternate Loop)";
        case 7: return "Restaurant X";
        case 8: return "Prologue / Cutscenes";
        case 9: return "Unused / Silence";
        case 10: return "Title Screen";
        case 11: return "Side Deck";
        case 12: return "Side Deck X";
        case 13: return "Main Hall X (Variation)";
        case 14: return "Side Deck X (Mild Drums)";
        case 15: return "Main Hall";
        case 16: return "Main Hall (Slower, No Bass)";
        case 17: return "Main Hall X";
        case 18: return "Title (Battle Version)";
        case 19: return "Zombie Attack";
        case 20: return "Class Cabin";
        case 21: return "Class Cabin X";
        case 22: return "Class Cabin (With Bass)";
        case 23: return "Corridor";
        case 24: return "Corridor X";
        case 25: return "Corridor X (Variation)";
        case 26: return "Corridor X (No Drums, Slower)";
        case 27: return "Class Cabin (With Bass & Drums)";
        default: return "Unknown";
    }
}

/* ------------------------------------------------------------------ */
/* Decoding                                                            */
/* ------------------------------------------------------------------ */

/* Linear resample and fold to stereo 44100 Hz, so the audio callback never
 * has to do any conversion work. */
static int16_t* to_stereo_44k(const short* src, int src_frames, int src_channels,
                              int src_rate, uint32_t* out_frames) {
    if (!src || src_frames <= 0 || src_channels <= 0 || src_rate <= 0) {
        return NULL;
    }

    const double ratio = (double)MUSIC_SAMPLE_RATE / (double)src_rate;
    uint64_t dst_frames = (uint64_t)((double)src_frames * ratio);
    if (dst_frames == 0) dst_frames = 1;
    if (dst_frames * 2ull * sizeof(int16_t) > MAX_DECODED_BYTES) {
        return NULL;
    }

    int16_t* dst = (int16_t*)malloc((size_t)dst_frames * 2u * sizeof(int16_t));
    if (!dst) return NULL;

    for (uint64_t i = 0; i < dst_frames; i++) {
        const double pos = (double)i / ratio;
        uint32_t i0 = (uint32_t)pos;
        uint32_t i1 = i0 + 1;
        if (i1 >= (uint32_t)src_frames) i1 = (uint32_t)src_frames - 1;
        const double frac = pos - (double)i0;

        for (int ch = 0; ch < 2; ch++) {
            /* mono feeds both outputs; more than two channels takes the first two */
            const int sc = (src_channels == 1) ? 0 : ch;
            const int a = src[(size_t)i0 * src_channels + sc];
            const int b = src[(size_t)i1 * src_channels + sc];
            const double v = a + (b - a) * frac;
            int s = (int)v;
            if (s < -32768) s = -32768;
            if (s > 32767) s = 32767;
            dst[i * 2 + ch] = (int16_t)s;
        }
    }

    *out_frames = (uint32_t)dst_frames;
    return dst;
}

static int16_t* decode_ogg(const char* path, uint32_t* out_frames) {
    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    if (!rw) return NULL;
    Sint64 size = SDL_RWsize(rw);
    if (size <= 0 || size > 64 * 1024 * 1024) { SDL_RWclose(rw); return NULL; }

    unsigned char* raw = (unsigned char*)malloc((size_t)size);
    if (!raw) { SDL_RWclose(rw); return NULL; }
    const size_t got = SDL_RWread(rw, raw, 1, (size_t)size);
    SDL_RWclose(rw);
    if (got != (size_t)size) { free(raw); return NULL; }

    int channels = 0, rate = 0;
    short* pcm = NULL;
    const int frames = stb_vorbis_decode_memory(raw, (int)size, &channels, &rate, &pcm);
    free(raw);
    if (frames <= 0 || !pcm) {
        if (pcm) free(pcm);
        return NULL;
    }

    int16_t* out = to_stereo_44k(pcm, frames, channels, rate, out_frames);
    free(pcm);
    return out;
}

static int16_t* decode_wav(const char* path, uint32_t* out_frames) {
    SDL_AudioSpec spec;
    Uint8* buf = NULL;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path, &spec, &buf, &len)) return NULL;

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                          AUDIO_S16SYS, 2, MUSIC_SAMPLE_RATE) < 0) {
        SDL_FreeWAV(buf);
        return NULL;
    }

    if (cvt.needed) {
        cvt.len = (int)len;
        cvt.buf = (Uint8*)malloc((size_t)cvt.len * cvt.len_mult);
        if (!cvt.buf) { SDL_FreeWAV(buf); return NULL; }
        memcpy(cvt.buf, buf, len);
        SDL_FreeWAV(buf);
        if (SDL_ConvertAudio(&cvt) < 0) { free(cvt.buf); return NULL; }
        *out_frames = (uint32_t)(cvt.len_cvt / 4);
        return (int16_t*)cvt.buf;
    }

    int16_t* out = (int16_t*)malloc(len);
    if (!out) { SDL_FreeWAV(buf); return NULL; }
    memcpy(out, buf, len);
    SDL_FreeWAV(buf);
    *out_frames = len / 4u;
    return out;
}

/* ------------------------------------------------------------------ */
/* Pack scanning                                                       */
/* ------------------------------------------------------------------ */

static bool has_extension(const char* name, const char* ext) {
    const size_t n = strlen(name), e = strlen(ext);
    if (n <= e) return false;
    for (size_t i = 0; i < e; i++) {
        if (tolower((unsigned char)name[n - e + i]) != tolower((unsigned char)ext[i])) {
            return false;
        }
    }
    return true;
}

/* track_07.ogg maps to guest music id 7; anything else is index-only. */
static int parse_track_id(const char* filename) {
    const char* p = strstr(filename, "track_");
    if (!p) p = strstr(filename, "TRACK_");
    if (!p) return -1;
    p += 6;
    if (!isdigit((unsigned char)*p)) return -1;
    return (int)strtol(p, NULL, 10);
}

/* ------------------------------------------------------------------ */
/* Loop Point Detection (WAV smpl chunk, OGG Vorbis comment, and INI)  */
/* ------------------------------------------------------------------ */

static int str_case_prefix(const char* str, const char* prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        str++;
        prefix++;
    }
    return 1;
}

static int str_case_cmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static uint32_t parse_loop_point_value(const char* str, int sample_rate) {
    if (!str || !*str) return 0;
    while (*str == ' ' || *str == '\t' || *str == '=' || *str == ':') str++;

    /* Check for mm:ss or mm:ss.ms */
    const char* colon = strchr(str, ':');
    if (colon) {
        double minutes = strtod(str, NULL);
        double seconds = strtod(colon + 1, NULL);
        double total_sec = minutes * 60.0 + seconds;
        if (total_sec < 0) total_sec = 0;
        return (uint32_t)(total_sec * MUSIC_SAMPLE_RATE);
    }

    /* Check for seconds with decimal or trailing 's' */
    if (strchr(str, '.') != NULL || strchr(str, 's') != NULL || strchr(str, 'S') != NULL) {
        double sec = strtod(str, NULL);
        if (sec < 0) sec = 0;
        return (uint32_t)(sec * MUSIC_SAMPLE_RATE);
    }

    /* Integer samples in source sample rate -> scale to 44.1 kHz */
    long long samples = strtoll(str, NULL, 10);
    if (samples <= 0) return 0;
    if (sample_rate <= 0) sample_rate = MUSIC_SAMPLE_RATE;
    double ratio = (double)MUSIC_SAMPLE_RATE / (double)sample_rate;
    return (uint32_t)((double)samples * ratio);
}

static void parse_vorbis_comments(const stb_vorbis_comment* comment, int sample_rate,
                                  uint32_t* out_loop_start, uint32_t* out_loop_end, bool* out_has_loop) {
    if (!comment || !comment->comment_list) return;

    bool has_start = false;
    uint32_t start_val = 0;
    uint32_t end_val = 0;
    uint32_t length_val = 0;

    for (int i = 0; i < comment->comment_list_length; i++) {
        const char* c = comment->comment_list[i];
        if (!c) continue;

        if (str_case_prefix(c, "LOOPSTART=") || str_case_prefix(c, "LOOP_START=")) {
            const char* val = strchr(c, '=') + 1;
            start_val = parse_loop_point_value(val, sample_rate);
            has_start = true;
        } else if (str_case_prefix(c, "LOOPEND=") || str_case_prefix(c, "LOOP_END=")) {
            const char* val = strchr(c, '=') + 1;
            end_val = parse_loop_point_value(val, sample_rate);
        } else if (str_case_prefix(c, "LOOPLENGTH=") || str_case_prefix(c, "LOOP_LENGTH=")) {
            const char* val = strchr(c, '=') + 1;
            length_val = parse_loop_point_value(val, sample_rate);
        }
    }

    if (has_start) {
        *out_has_loop = true;
        *out_loop_start = start_val;
        if (end_val > 0) {
            *out_loop_end = end_val;
        } else if (length_val > 0) {
            *out_loop_end = start_val + length_val;
        } else {
            *out_loop_end = 0;
        }
    }
}

static bool read_ogg_loop_points(const char* path, uint32_t* out_loop_start, uint32_t* out_loop_end, bool* out_has_loop) {
    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    if (!rw) return false;
    Sint64 size = SDL_RWsize(rw);
    if (size <= 0 || size > 64 * 1024 * 1024) { SDL_RWclose(rw); return false; }

    unsigned char* raw = (unsigned char*)malloc((size_t)size);
    if (!raw) { SDL_RWclose(rw); return false; }
    const size_t got = SDL_RWread(rw, raw, 1, (size_t)size);
    SDL_RWclose(rw);
    if (got != (size_t)size) { free(raw); return false; }

    int err = 0;
    stb_vorbis* v = stb_vorbis_open_memory(raw, (int)size, &err, NULL);
    if (!v) { free(raw); return false; }

    stb_vorbis_info info = stb_vorbis_get_info(v);
    stb_vorbis_comment comment = stb_vorbis_get_comment(v);
    parse_vorbis_comments(&comment, (int)info.sample_rate, out_loop_start, out_loop_end, out_has_loop);
    stb_vorbis_close(v);
    free(raw);
    return *out_has_loop;
}

static bool read_wav_loop_points(const char* path, uint32_t* out_loop_start, uint32_t* out_loop_end, bool* out_has_loop) {
    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    if (!rw) return false;

    uint8_t header[12];
    if (SDL_RWread(rw, header, 1, 12) != 12) {
        SDL_RWclose(rw);
        return false;
    }

    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        SDL_RWclose(rw);
        return false;
    }

    uint32_t sample_rate = 44100;
    bool found_loop = false;

    while (1) {
        uint8_t chunk_hdr[8];
        if (SDL_RWread(rw, chunk_hdr, 1, 8) != 8) break;

        uint32_t chunk_size = (uint32_t)chunk_hdr[4] |
                              ((uint32_t)chunk_hdr[5] << 8) |
                              ((uint32_t)chunk_hdr[6] << 16) |
                              ((uint32_t)chunk_hdr[7] << 24);

        if (memcmp(chunk_hdr, "fmt ", 4) == 0 && chunk_size >= 16) {
            uint8_t fmt_data[16];
            if (SDL_RWread(rw, fmt_data, 1, 16) == 16) {
                sample_rate = (uint32_t)fmt_data[4] |
                              ((uint32_t)fmt_data[5] << 8) |
                              ((uint32_t)fmt_data[6] << 16) |
                              ((uint32_t)fmt_data[7] << 24);
                if (sample_rate == 0) sample_rate = 44100;
                if (chunk_size > 16) {
                    SDL_RWseek(rw, (Sint64)(chunk_size - 16), RW_SEEK_CUR);
                }
            } else {
                break;
            }
        } else if (memcmp(chunk_hdr, "smpl", 4) == 0 && chunk_size >= 36) {
            uint8_t smpl_data[36];
            if (SDL_RWread(rw, smpl_data, 1, 36) == 36) {
                uint32_t num_loops = (uint32_t)smpl_data[28] |
                                     ((uint32_t)smpl_data[29] << 8) |
                                     ((uint32_t)smpl_data[30] << 16) |
                                     ((uint32_t)smpl_data[31] << 24);
                if (num_loops > 0 && chunk_size >= 36 + 24) {
                    uint8_t loop_data[24];
                    if (SDL_RWread(rw, loop_data, 1, 24) == 24) {
                        uint32_t start_samp = (uint32_t)loop_data[8] |
                                              ((uint32_t)loop_data[9] << 8) |
                                              ((uint32_t)loop_data[10] << 16) |
                                              ((uint32_t)loop_data[11] << 24);
                        uint32_t end_samp = (uint32_t)loop_data[12] |
                                            ((uint32_t)loop_data[13] << 8) |
                                            ((uint32_t)loop_data[14] << 16) |
                                            ((uint32_t)loop_data[15] << 24);
                        double ratio = (double)MUSIC_SAMPLE_RATE / (double)sample_rate;
                        *out_loop_start = (uint32_t)((double)start_samp * ratio);
                        *out_loop_end = (uint32_t)((double)(end_samp + 1) * ratio);
                        *out_has_loop = true;
                        found_loop = true;
                    }
                }
                uint32_t read_bytes = 36 + (num_loops > 0 && chunk_size >= 60 ? 24 : 0);
                if (chunk_size > read_bytes) {
                    SDL_RWseek(rw, (Sint64)(chunk_size - read_bytes), RW_SEEK_CUR);
                }
            } else {
                break;
            }
        } else {
            SDL_RWseek(rw, (Sint64)chunk_size, RW_SEEK_CUR);
        }

        if (chunk_size & 1) {
            SDL_RWseek(rw, 1, RW_SEEK_CUR);
        }
    }

    SDL_RWclose(rw);
    return found_loop;
}

static bool read_ini_loop_points(const char* dir, const char* filename, int track_id,
                                 uint32_t* out_loop_start, uint32_t* out_loop_end, bool* out_has_loop) {
    char ini_path[512];
    snprintf(ini_path, sizeof(ini_path), "%s/music_pack.ini", dir);

    char section_target1[64] = {0};
    char section_target2[64] = {0};
    if (track_id >= 0) {
        snprintf(section_target1, sizeof(section_target1), "[track_%d]", track_id);
    }
    if (filename && filename[0]) {
        snprintf(section_target2, sizeof(section_target2), "[%s]", filename);
    }

    FILE* f = fopen(ini_path, "r");
    if (!f && track_id >= 0) {
        snprintf(ini_path, sizeof(ini_path), "%s/track_%d.ini", dir, track_id);
        f = fopen(ini_path, "r");
    }

    if (!f) return false;

    char line[256];
    bool in_matching_section = (section_target1[0] == '\0' && section_target2[0] == '\0');
    bool found = false;

    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;

        if (*p == '[') {
            in_matching_section = false;
            char* end_bracket = strchr(p, ']');
            if (end_bracket) {
                *(end_bracket + 1) = '\0';
                if ((section_target1[0] && str_case_cmp(p, section_target1) == 0) ||
                    (section_target2[0] && str_case_cmp(p, section_target2) == 0)) {
                    in_matching_section = true;
                }
            }
            continue;
        }

        if (in_matching_section) {
            char* eq = strchr(p, '=');
            if (!eq) continue;
            *eq = '\0';
            char* key = p;
            char* val = eq + 1;

            char* kend = key + strlen(key) - 1;
            while (kend >= key && (*kend == ' ' || *kend == '\t')) *kend-- = '\0';

            while (*val == ' ' || *val == '\t') val++;
            char* vend = val + strlen(val) - 1;
            while (vend >= val && (*vend == ' ' || *vend == '\t' || *vend == '\r' || *vend == '\n')) *vend-- = '\0';

            if (str_case_cmp(key, "loop_start") == 0 || str_case_cmp(key, "loopstart") == 0) {
                *out_loop_start = parse_loop_point_value(val, MUSIC_SAMPLE_RATE);
                *out_has_loop = true;
                found = true;
            } else if (str_case_cmp(key, "loop_end") == 0 || str_case_cmp(key, "loopend") == 0) {
                *out_loop_end = parse_loop_point_value(val, MUSIC_SAMPLE_RATE);
                found = true;
            } else if (str_case_cmp(key, "loop_length") == 0 || str_case_cmp(key, "looplength") == 0) {
                uint32_t len = parse_loop_point_value(val, MUSIC_SAMPLE_RATE);
                *out_loop_end = *out_loop_start + len;
                found = true;
            }
        }
    }

    fclose(f);
    return found;
}

static void detect_track_loop_points(MusicTrackEntry* t, const char* dir) {
    t->has_loop = false;
    t->loop_start = 0;
    t->loop_end = 0;

    if (has_extension(t->name, ".ogg")) {
        read_ogg_loop_points(t->path, &t->loop_start, &t->loop_end, &t->has_loop);
    } else if (has_extension(t->name, ".wav")) {
        read_wav_loop_points(t->path, &t->loop_start, &t->loop_end, &t->has_loop);
    }

    read_ini_loop_points(dir, t->name, t->track_id, &t->loop_start, &t->loop_end, &t->has_loop);

    if (t->has_loop) {
        char sbuf[32], ebuf[32];
        music_pack_format_timestamp(t->loop_start, sbuf, sizeof(sbuf));
        if (t->loop_end > 0) {
            music_pack_format_timestamp(t->loop_end, ebuf, sizeof(ebuf));
            printf("[Music Pack] Track '%s' loop points: %s -> %s (samples %u -> %u)\n",
                   t->name, sbuf, ebuf, t->loop_start, t->loop_end);
        } else {
            printf("[Music Pack] Track '%s' loop point start: %s (sample %u)\n",
                   t->name, sbuf, t->loop_start);
        }
    }
}

static void add_track(const char* dir, const char* filename) {
    if (s_track_count >= MUSIC_PACK_MAX_TRACKS) return;
    if (!has_extension(filename, ".ogg") && !has_extension(filename, ".wav")) return;

    MusicTrackEntry* t = &s_tracks[s_track_count++];
    snprintf(t->name, sizeof(t->name), "%s", filename);
    snprintf(t->path, sizeof(t->path), "%s/%s", dir, filename);
    t->track_id = parse_track_id(filename);
    detect_track_loop_points(t, dir);
}

static void scan_directory(const char* dir) {
#ifdef _WIN32
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%s/*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            add_track(dir, fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        add_track(dir, e->d_name);
    }
    closedir(d);
#endif
}

static void ensure_directory(const char* dir) {
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static void stop_playback_locked(void) {
    free(s_pcm);
    s_pcm = NULL;
    s_pcm_frames = 0;
    s_pcm_pos = 0;
    s_fade_in_remaining = 0;
    s_has_loop = false;
    s_loop_start = 0;
    s_loop_end = 0;
}

void music_pack_init(const char* pack_dir) {
    if (pack_dir && pack_dir[0]) {
        snprintf(g_music_pack_config.pack_dir, sizeof(g_music_pack_config.pack_dir), "%s", pack_dir);
    }
    ensure_directory(g_music_pack_config.pack_dir);
    music_pack_reload();
}

void music_pack_reload(void) {
    lock_audio();
    stop_playback_locked();
    s_current_track = MUSIC_PACK_NO_TRACK;
    unlock_audio();

    s_track_count = 0;
    scan_directory(g_music_pack_config.pack_dir);
    g_music_pack_config.loaded_count = s_track_count;
    printf("[Music Pack] %d track(s) found in '%s'\n",
           s_track_count, g_music_pack_config.pack_dir);
}

void music_pack_shutdown(void) {
    lock_audio();
    stop_playback_locked();
    s_current_track = MUSIC_PACK_NO_TRACK;
    unlock_audio();
    s_track_count = 0;
    g_music_pack_config.loaded_count = 0;
}

/* ------------------------------------------------------------------ */
/* Track selection                                                     */
/* ------------------------------------------------------------------ */

static int find_entry_for_track(int guest_track_id) {
    for (int i = 0; i < s_track_count; i++) {
        if (s_tracks[i].track_id == guest_track_id) return i;
    }
    return -1;
}

void music_pack_request_track(int guest_track_id) {
    if (!g_music_pack_config.enabled) {
        if (s_pcm) {
            lock_audio();
            stop_playback_locked();
            s_current_track = MUSIC_PACK_NO_TRACK;
            unlock_audio();
        }
        return;
    }

    if (guest_track_id == s_current_track) {
        return; /* already on the right track */
    }

    if (guest_track_id <= 0 || guest_track_id == MUSIC_PACK_NO_TRACK) {
        lock_audio();
        stop_playback_locked();
        s_current_track = guest_track_id;
        unlock_audio();
        return;
    }

    const int entry = find_entry_for_track(guest_track_id);
    if (entry < 0) {
        /* No replacement supplied for this id: fall back to the original music
         * rather than leaving the previous track running over the wrong scene. */
        lock_audio();
        stop_playback_locked();
        s_current_track = guest_track_id;
        unlock_audio();
        return;
    }

    uint32_t frames = 0;
    int16_t* pcm = has_extension(s_tracks[entry].path, ".ogg")
        ? decode_ogg(s_tracks[entry].path, &frames)
        : decode_wav(s_tracks[entry].path, &frames);

    if (!pcm || frames == 0) {
        free(pcm);
        printf("[Music Pack] Failed to decode '%s'\n", s_tracks[entry].path);
        return;
    }

    lock_audio();
    stop_playback_locked();
    s_pcm = pcm;
    s_pcm_frames = frames;
    s_pcm_pos = 0;
    s_current_track = guest_track_id;
    s_fade_in_remaining = FADE_FRAMES;
    s_has_loop = s_tracks[entry].has_loop;
    s_loop_start = s_tracks[entry].loop_start;
    s_loop_end = (s_tracks[entry].loop_end > 0 && s_tracks[entry].loop_end <= frames)
                 ? s_tracks[entry].loop_end : frames;
    unlock_audio();

    if (s_has_loop) {
        char sbuf[32], ebuf[32];
        music_pack_format_timestamp(s_loop_start, sbuf, sizeof(sbuf));
        music_pack_format_timestamp(s_loop_end, ebuf, sizeof(ebuf));
        printf("[Music Pack] Track %d -> %s (%.1fs, loop: %s -> %s)\n",
               guest_track_id, s_tracks[entry].name, (double)frames / MUSIC_SAMPLE_RATE,
               sbuf, ebuf);
    } else {
        printf("[Music Pack] Track %d -> %s (%.1fs)\n",
               guest_track_id, s_tracks[entry].name, (double)frames / MUSIC_SAMPLE_RATE);
    }
}

/* ------------------------------------------------------------------ */
/* Audio thread                                                        */
/* ------------------------------------------------------------------ */

int music_pack_apu_level_percent(void) {
    if (!music_pack_is_playing()) {
        return 100;
    }
    int duck = g_music_pack_config.duck_percent;
    if (duck < 0) duck = 0;
    if (duck > 100) duck = 100;
    return duck;
}

void music_pack_mix(int16_t* out, int frames) {
    if (!out || frames <= 0 || !music_pack_is_playing()) {
        return;
    }

    int volume = g_music_pack_config.volume;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (volume == 0) return;

    for (int i = 0; i < frames; i++) {
        const uint32_t end_point = (s_has_loop && s_loop_end > 0 && s_loop_end <= s_pcm_frames)
                                   ? s_loop_end : s_pcm_frames;
        if (s_pcm_pos >= end_point) {
            if (!g_music_pack_config.loop) {
                break;
            }
            s_pcm_pos = (s_has_loop && s_loop_start < end_point) ? s_loop_start : 0;
        }

        int gain = volume;
        if (s_fade_in_remaining > 0) {
            gain = volume * (FADE_FRAMES - s_fade_in_remaining) / FADE_FRAMES;
            s_fade_in_remaining--;
        }

        for (int ch = 0; ch < 2; ch++) {
            int32_t mixed = out[i * 2 + ch] +
                            (int32_t)s_pcm[(size_t)s_pcm_pos * 2 + ch] * gain / 100;
            if (mixed < -32768) mixed = -32768;
            if (mixed > 32767) mixed = 32767;
            out[i * 2 + ch] = (int16_t)mixed;
        }
        s_pcm_pos++;
    }
}
