#ifndef ROM_LOADER_H
#define ROM_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RE_GAIDEN_EXPECTED_SHA256 "9a97678cbd8da02c8763e977674e17f460c06ea8b73bad35c52fe6817f506d44"
#define RE_GAIDEN_ROM_SIZE 2097152u

extern uint8_t* g_rom_data;
extern size_t g_rom_size;

/**
 * @brief Find, validate, or prompt the user for their Resident Evil Gaiden GBC ROM.
 * @param explicit_path Optional path passed via command-line arguments (--rom)
 * @return true if a valid ROM was loaded, false otherwise.
 */
bool rom_loader_acquire_rom(const char* explicit_path);

#ifdef __cplusplus
}
#endif

#endif /* ROM_LOADER_H */
