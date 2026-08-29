#ifndef RE_MUSIC_PACK_H
#define RE_MUSIC_PACK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_PACK_MAX_TRACKS 64
#define MUSIC_PACK_NO_TRACK (-1)

typedef struct {
    bool enabled;
    char pack_dir[256];
    int volume;         /* 0 - 100, replacement music level        */
    int duck_percent;   /* 0 - 100, emulated APU level while music plays */
    bool loop;
    int loaded_count;   /* populated by music_pack_reload()        */
} MusicPackConfig;

extern MusicPackConfig g_music_pack_config;

/**
 * @brief Point the mixer at the SDL audio device so track swaps can be made
 *        safely with respect to the audio callback thread.
 */
void music_pack_set_audio_device(uint32_t device_id);

void music_pack_init(const char* pack_dir);
void music_pack_reload(void);
void music_pack_shutdown(void);

int  music_pack_track_count(void);
const char* music_pack_track_name(int index);
/** @brief Track id parsed from the filename (track_07.ogg -> 7), or -1. */
int  music_pack_track_id(int index);

/**
 * @brief Ask for the replacement track matching a guest music id.
 *
 * Safe to call every frame; swapping only happens when the id actually
 * changes. Pass MUSIC_PACK_NO_TRACK to fade out and fall back to the
 * emulated soundtrack.
 */
void music_pack_request_track(int guest_track_id);

int  music_pack_current_track(void);
bool music_pack_is_playing(void);

/* --- audio thread --- */

/**
 * @brief Level the emulated APU should be played at, 0-100.
 *        Drops to duck_percent while a replacement track is audible.
 */
int  music_pack_apu_level_percent(void);

/** @brief Mix the active track into an already-filled stereo S16 buffer. */
void music_pack_mix(int16_t* out, int frames);

#ifdef __cplusplus
}
#endif

#endif // RE_MUSIC_PACK_H
