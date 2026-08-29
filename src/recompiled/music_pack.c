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

int music_pack_current_track(void) { return s_current_track; }

bool music_pack_is_playing(void) {
    return g_music_pack_config.enabled && s_pcm != NULL && s_pcm_frames > 0;
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

static void add_track(const char* dir, const char* filename) {
    if (s_track_count >= MUSIC_PACK_MAX_TRACKS) return;
    if (!has_extension(filename, ".ogg") && !has_extension(filename, ".wav")) return;

    MusicTrackEntry* t = &s_tracks[s_track_count++];
    snprintf(t->name, sizeof(t->name), "%s", filename);
    snprintf(t->path, sizeof(t->path), "%s/%s", dir, filename);
    t->track_id = parse_track_id(filename);
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

    if (guest_track_id == MUSIC_PACK_NO_TRACK) {
        lock_audio();
        stop_playback_locked();
        s_current_track = MUSIC_PACK_NO_TRACK;
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
    unlock_audio();

    printf("[Music Pack] Track %d -> %s (%.1fs)\n",
           guest_track_id, s_tracks[entry].name, (double)frames / MUSIC_SAMPLE_RATE);
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
        if (s_pcm_pos >= s_pcm_frames) {
            if (!g_music_pack_config.loop) {
                break;
            }
            s_pcm_pos = 0;
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
