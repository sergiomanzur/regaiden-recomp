#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "music_pack.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; \
} } while (0)

/* Fixtures contain generated constant PCM, never game or third-party audio. */
static void write_u32(FILE* f, unsigned n, int bytes) {
    for (int i = 0; i < bytes; i++) fputc((n >> (8 * i)) & 255, f);
}

static void write_wav(const char* dir, int id, int sample) {
    char path[512];
    snprintf(path, sizeof(path), "%s/track_%d.wav", dir, id);
    FILE* f = fopen(path, "wb");
    if (!f) exit(2);
    const unsigned data_size = 44100 * 4;
    fwrite("RIFF", 1, 4, f); write_u32(f, data_size + 36, 4);
    fwrite("WAVEfmt ", 1, 8, f); write_u32(f, 16, 4);
    write_u32(f, 1, 2); write_u32(f, 2, 2); write_u32(f, 44100, 4);
    write_u32(f, 176400, 4); write_u32(f, 4, 2); write_u32(f, 16, 2);
    fwrite("data", 1, 4, f); write_u32(f, data_size, 4);
    for (unsigned i = 0; i < 44100; i++) {
        int frame = sample;
        if (id == 11 && i < 1000) frame = -sample;
        if (id == 11 && i >= 20000) frame = sample / 2;
        write_u32(f, (unsigned)frame, 2);
        write_u32(f, (unsigned)frame, 2);
    }
    fclose(f);
}

static void mix_frames(int frames) {
    int16_t* samples = calloc((size_t)frames * 2, sizeof(*samples));
    if (!samples) exit(2);
    music_pack_mix(samples, frames);
    free(samples);
}

static void missing_variation_keeps_base_position(void) {
    music_pack_request_track(11);
    CHECK(music_pack_is_playing());
    mix_frames(12000); /* past fade-in; restarting would produce silence */
    music_pack_request_track(12);
    CHECK(music_pack_current_track() == 11);
    CHECK(music_pack_is_playing());
    int16_t samples[2] = {0};
    music_pack_mix(samples, 1);
    CHECK(samples[0] == 10000);
}

static void exact_match_and_stop_take_priority(void) {
    music_pack_request_track(11);
    music_pack_request_track(14);
    CHECK(music_pack_current_track() == 14);
    mix_frames(12000);
    int16_t samples[2] = {0};
    music_pack_mix(samples, 1);
    CHECK(samples[0] == 20000);
    music_pack_request_track(0);
    CHECK(music_pack_current_track() == 0);
    CHECK(!music_pack_is_playing());
}

static void unmapped_unrelated_scene_releases_replacement(void) {
    music_pack_request_track(11);
    music_pack_request_track(23);
    CHECK(!music_pack_is_playing());
    CHECK(music_pack_apu_level_percent() == 100);
}

static void corrupt_track_releases_previous_scene(void) {
    music_pack_request_track(11);
    music_pack_request_track(19);
    CHECK(!music_pack_is_playing());
    CHECK(music_pack_current_track() == 19);
    CHECK(music_pack_apu_level_percent() == 100);
}

static void looping_resumes_at_the_configured_start(void) {
    music_pack_request_track(0);
    music_pack_request_track(11);
    mix_frames(20000);
    int16_t samples[2] = {0};
    music_pack_mix(samples, 1);
    CHECK(samples[0] == 10000);
    music_pack_request_track(0);
}

static void completed_one_shot_releases_apu(void) {
    g_music_pack_config.loop = false;
    music_pack_request_track(11);
    mix_frames(22000); /* past the embedded loop end, before file end */
    int16_t samples[2] = {0};
    music_pack_mix(samples, 1);
    CHECK(samples[0] == 5000);
    mix_frames(22099); /* ends exactly at file end */
    CHECK(!music_pack_is_playing());
    CHECK(music_pack_apu_level_percent() == 100);
    g_music_pack_config.loop = true;
}

int main(void) {
    if (SDL_Init(0) != 0) return 2;
    char directory[128];
    snprintf(directory, sizeof(directory), "music-pack-test-%llu",
             (unsigned long long)SDL_GetPerformanceCounter());
#ifdef _WIN32
    if (_mkdir(directory) != 0) return 2;
#else
    if (mkdir(directory, 0700) != 0) return 2;
#endif
    music_pack_init(directory); /* creates isolated test directory */
    write_wav(directory, 11, 10000);
    write_wav(directory, 14, 20000);
    char ini_path[512];
    snprintf(ini_path, sizeof(ini_path), "%s/music_pack.ini", directory);
    FILE* ini = fopen(ini_path, "wb");
    if (!ini) return 2;
    fputs("[track_11]\nloop_start=1000\nloop_end=20000\n", ini); fclose(ini);
    char corrupt_path[512];
    snprintf(corrupt_path, sizeof(corrupt_path), "%s/track_19.wav", directory);
    FILE* corrupt = fopen(corrupt_path, "wb");
    if (!corrupt) return 2;
    fputs("invalid WAV", corrupt); fclose(corrupt);
    music_pack_reload();
    g_music_pack_config.enabled = true;
    g_music_pack_config.volume = 100;
    missing_variation_keeps_base_position();
    exact_match_and_stop_take_priority();
    unmapped_unrelated_scene_releases_replacement();
    corrupt_track_releases_previous_scene();
    music_pack_reload();
    looping_resumes_at_the_configured_start();
    completed_one_shot_releases_apu();
    music_pack_shutdown();
    char path[512];
    remove(ini_path);
    int ids[] = {11, 14, 19};
    for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        snprintf(path, sizeof(path), "%s/track_%d.wav", directory, ids[i]);
        remove(path);
    }
#ifdef _WIN32
    _rmdir(directory);
#else
    rmdir(directory);
#endif
    SDL_Quit();
    if (failures) return 1;
    puts("music pack tests passed");
    return 0;
}
