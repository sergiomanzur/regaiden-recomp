#ifndef RE_ASSET_DUMP_H
#define RE_ASSET_DUMP_H
#include <stdbool.h>
#include <stddef.h>
#include "gbrt.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Capture current CGB VRAM/palettes and native frame into a new capture_NNNN
 * directory. Call on the emulation thread between frames. Returns false on any
 * invalid input or write error; status always describes the result when supplied.
 * output_dir's parent must exist. Partial captures can remain after write errors. */
bool asset_dump_capture(const GBContext* ctx, const char* output_dir,
                        char* status, size_t status_size);
#ifdef __cplusplus
}
#endif
#endif
