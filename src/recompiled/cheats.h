#ifndef CHEATS_H
#define CHEATS_H

#include "gbrt.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[64];
    char code[32]; // e.g. "016404C8"
    bool enabled;
} CustomCheat;

#define MAX_CUSTOM_CHEATS 32
extern CustomCheat g_custom_cheats[MAX_CUSTOM_CHEATS];
extern int g_custom_cheat_count;

/**
 * @brief Apply active cheats to the game context (called every frame).
 */
void cheats_apply_frame(GBContext* ctx);

/**
 * @brief Add a custom GameShark cheat code (format: 01XXYYZZ -> writes XX to address 0xZZYY).
 */
bool cheats_add_gameshark_code(const char* name, const char* code_str, bool enabled);

/**
 * @brief Remove a custom cheat by index.
 */
void cheats_remove_custom(int index);

#ifdef __cplusplus
}
#endif

#endif /* CHEATS_H */
