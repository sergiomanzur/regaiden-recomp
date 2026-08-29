#ifndef RE_STATE_DUMP_H
#define RE_STATE_DUMP_H

#include <stdbool.h>
#include <stddef.h>
#include "gbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Directory snapshots are written to. Desktop uses the working
 *        directory; Android must pass a writable app-storage path.
 */
void state_dump_set_output_dir(const char* dir);

/**
 * @brief Capture a full guest memory snapshot for offline analysis.
 *
 * Writes a raw .bin (WRAM/HRAM/IO/OAM, see the header inside) plus a
 * human-readable .txt summary. Used to locate real game-state addresses by
 * diffing snapshots taken in different game states.
 *
 * @param ctx   Live guest context.
 * @param label Optional short tag recorded in the snapshot, may be NULL.
 * @param out_status Optional buffer receiving a user-facing status line.
 * @param status_size Size of out_status.
 * @return true if the snapshot was written.
 */
bool state_dump_capture(GBContext* ctx, const char* label,
                        char* out_status, size_t status_size);

/** @brief Number of snapshots captured this session. */
int state_dump_count(void);

/** @brief Directory snapshots are written to, for display in the UI. */
const char* state_dump_output_dir(void);

#ifdef __cplusplus
}
#endif

#endif // RE_STATE_DUMP_H
