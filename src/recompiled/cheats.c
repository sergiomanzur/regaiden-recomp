#include "cheats.h"
#include "config_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CustomCheat g_custom_cheats[MAX_CUSTOM_CHEATS];
int g_custom_cheat_count = 0;

static inline void write_memory_byte(GBContext* ctx, uint16_t addr, uint8_t val) {
    if (!ctx) return;
    if (addr >= 0xC000 && addr <= 0xCFFF) {
        ctx->wram[addr - 0xC000] = val;
    } else if (addr >= 0xD000 && addr <= 0xDFFF) {
        uint8_t bank = (ctx->wram_bank == 0) ? 1 : (ctx->wram_bank & 0x07);
        ctx->wram[bank * 0x1000 + (addr - 0xD000)] = val;
    } else if (addr >= 0xFF80 && addr <= 0xFFFE) {
        ctx->hram[addr - 0xFF80] = val;
    }
}

static inline uint8_t read_memory_byte(GBContext* ctx, uint16_t addr) {
    if (!ctx) return 0;
    if (addr >= 0xC000 && addr <= 0xCFFF) {
        return ctx->wram[addr - 0xC000];
    } else if (addr >= 0xD000 && addr <= 0xDFFF) {
        uint8_t bank = (ctx->wram_bank == 0) ? 1 : (ctx->wram_bank & 0x07);
        return ctx->wram[bank * 0x1000 + (addr - 0xD000)];
    } else if (addr >= 0xFF80 && addr <= 0xFFFE) {
        return ctx->hram[addr - 0xFF80];
    }
    return 0;
}

static void apply_gameshark_code(GBContext* ctx, const char* code_str) {
    // Format: 01XXYYZZ -> write byte XX to 0xZZYY
    if (!code_str || strlen(code_str) < 8) return;
    if (code_str[0] != '0' || code_str[1] != '1') return;

    char byte_str[3] = { code_str[2], code_str[3], '\0' };
    char addr_lo_str[3] = { code_str[4], code_str[5], '\0' };
    char addr_hi_str[3] = { code_str[6], code_str[7], '\0' };

    uint8_t val = (uint8_t)strtoul(byte_str, NULL, 16);
    uint8_t lo = (uint8_t)strtoul(addr_lo_str, NULL, 16);
    uint8_t hi = (uint8_t)strtoul(addr_hi_str, NULL, 16);

    uint16_t addr = (uint16_t)((hi << 8) | lo);
    write_memory_byte(ctx, addr, val);
}

void cheats_apply_frame(GBContext* ctx) {
    if (!ctx || !ctx->wram) return;

    // 1. Infinite Health for Barry, Leon, Lucia
    if (g_app_config.cheat_infinite_health) {
        // Character 1 (Barry)
        write_memory_byte(ctx, 0xC804, 100); // Current HP
        write_memory_byte(ctx, 0xC805, 100); // Max HP
        // Character 2 (Leon)
        write_memory_byte(ctx, 0xC824, 100);
        write_memory_byte(ctx, 0xC825, 100);
        // Character 3 (Lucia)
        write_memory_byte(ctx, 0xC844, 100);
        write_memory_byte(ctx, 0xC845, 100);
    }

    // 2. Infinite Ammo for all weapons
    if (g_app_config.cheat_infinite_ammo) {
        write_memory_byte(ctx, 0xC80B, 99); // Handgun ammo
        write_memory_byte(ctx, 0xC80D, 99); // Shotgun ammo
        write_memory_byte(ctx, 0xC80F, 99); // Grenade Launcher ammo
        write_memory_byte(ctx, 0xC811, 99); // Assault Rifle ammo
    }

    // 3. One-Hit Kill in Battle
    if (g_app_config.cheat_one_hit_kill) {
        uint8_t enemy1_hp = read_memory_byte(ctx, 0xC920);
        if (enemy1_hp > 0 && enemy1_hp < 100) {
            write_memory_byte(ctx, 0xC920, 0);
        }
        uint8_t enemy2_hp = read_memory_byte(ctx, 0xC922);
        if (enemy2_hp > 0 && enemy2_hp < 100) {
            write_memory_byte(ctx, 0xC922, 0);
        }
        uint8_t enemy3_hp = read_memory_byte(ctx, 0xC924);
        if (enemy3_hp > 0 && enemy3_hp < 100) {
            write_memory_byte(ctx, 0xC924, 0);
        }
    }

    // 4. Freeze Combat Reticle / Always Perfect Center Hit
    if (g_app_config.cheat_freeze_reticle) {
        uint8_t target_center = read_memory_byte(ctx, 0xC914);
        if (target_center > 0) {
            write_memory_byte(ctx, 0xC910, target_center); // Align reticle to target center
        }
    }

    // 5. Unlock All Weapons
    if (g_app_config.cheat_all_weapons) {
        write_memory_byte(ctx, 0xC80A, 1); // Handgun equipped/owned
        write_memory_byte(ctx, 0xC80C, 1); // Shotgun
        write_memory_byte(ctx, 0xC80E, 1); // Grenade Launcher
        write_memory_byte(ctx, 0xC810, 1); // Assault Rifle
    }

    // 6. Infinite First Aid Sprays & Items
    if (g_app_config.cheat_infinite_items) {
        write_memory_byte(ctx, 0xC813, 9); // First Aid Sprays
        write_memory_byte(ctx, 0xC815, 9); // Herbs
        write_memory_byte(ctx, 0xC817, 9); // Armor
    }

    // 7. Custom User GameShark Codes
    for (int i = 0; i < g_custom_cheat_count; ++i) {
        if (g_custom_cheats[i].enabled) {
            apply_gameshark_code(ctx, g_custom_cheats[i].code);
        }
    }
}

bool cheats_add_gameshark_code(const char* name, const char* code_str, bool enabled) {
    if (!code_str || g_custom_cheat_count >= MAX_CUSTOM_CHEATS) return false;
    CustomCheat* c = &g_custom_cheats[g_custom_cheat_count++];
    snprintf(c->name, sizeof(c->name), "%s", (name && name[0]) ? name : "Custom Cheat");
    snprintf(c->code, sizeof(c->code), "%s", code_str);
    c->enabled = enabled;
    return true;
}

void cheats_remove_custom(int index) {
    if (index < 0 || index >= g_custom_cheat_count) return;
    for (int i = index; i < g_custom_cheat_count - 1; ++i) {
        g_custom_cheats[i] = g_custom_cheats[i + 1];
    }
    g_custom_cheat_count--;
}
