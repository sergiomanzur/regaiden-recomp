#ifndef RE_GAME_STATE_H
#define RE_GAME_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "gbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Telling gameplay apart from the game's own UI screens.
 *
 * Resident Evil Gaiden runs its full-screen UI - the inventory, the item info
 * panel, the PDA/map, the title screen and the save/load menu - with CGB WRAM
 * bank 2 selected, and ordinary gameplay with bank 1. Measured, not assumed:
 *
 *   75/75 snapshots taken while walking around a room  -> bank 1
 *   inventory, item info, PDA, title menu, save menu   -> bank 2 (every sample)
 *
 * This replaced a set of hardcoded WRAM addresses ($C800/$C900) that the
 * disassembly does not corroborate - the only ROM code touching them is a
 * 0x50-byte save/restore memcpy, and they read as all zeroes even during
 * gameplay.
 *
 * Known limit: the intro cutscene also runs in bank 1, so it is not separated
 * from exploration by this test.
 */
static inline bool gb_state_is_ui_screen(const GBContext* ctx) {
    if (!ctx) {
        return false;
    }
    uint8_t bank = (uint8_t)(ctx->wram_bank & 0x07);
    if (bank == 0) {
        bank = 1; /* SVBK 0 and 1 both select bank 1 */
    }
    return bank == 2;
}

/*
 * Current music track id.
 *
 * The sound driver keeps its state in $CE80-$CEFF (bank 1 holds the only
 * genuine APU code in the ROM, and every RAM address it touches falls in that
 * block). $CE8C is its current-song byte. Measured:
 *
 *   93/93 snapshots roaming one area  -> 2
 *   75/75 snapshots, separate session -> 2
 *   title screen                      -> 8
 *   intro / attract sequence          -> 16 (11 earlier in the intro)
 *
 * Stable for as long as a piece of music is playing and changes exactly when
 * the music does, including holding steady when the inventory is opened over
 * the top of gameplay.
 */
static inline int gb_state_music_track(const GBContext* ctx) {
    if (!ctx || !ctx->wram) {
        return -1;
    }
    return (int)ctx->wram[0x0E8C]; /* $CE8C, always WRAM bank 0 */
}

#ifdef __cplusplus
}
#endif

#endif // RE_GAME_STATE_H
