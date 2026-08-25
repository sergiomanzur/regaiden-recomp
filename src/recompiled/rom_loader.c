#include "rom_loader.h"
#include "gbrt_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

uint8_t* g_rom_data = NULL;
size_t g_rom_size = 0;

static const char* s_candidate_paths[] = {
    "Resident Evil Gaiden (USA).gbc",
    "rom/Resident Evil Gaiden (USA).gbc",
    "../rom/Resident Evil Gaiden (USA).gbc",
    "../../rom/Resident Evil Gaiden (USA).gbc",
    "rom.gbc",
    "Resident_Evil_Gaiden__USA_.gbc",
    NULL
};

static uint8_t* read_file_bytes(const char* path, size_t* out_size) {
    if (!path || !out_size) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 32 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }

    uint8_t* buf = (uint8_t*)malloc(sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, sz, f);
    fclose(f);

    if (read_bytes != (size_t)sz) {
        free(buf);
        return NULL;
    }

    *out_size = (size_t)sz;
    return buf;
}

static bool validate_and_load(const char* path) {
    if (!path) return false;
    size_t sz = 0;
    uint8_t* buf = read_file_bytes(path, &sz);
    if (!buf) return false;

    if (sz != RE_GAIDEN_ROM_SIZE) {
        fprintf(stderr, "[ROM Loader] File '%s' has incorrect size (%zu bytes, expected %u bytes)\n",
                path, sz, RE_GAIDEN_ROM_SIZE);
        free(buf);
        return false;
    }

    if (!gbrt_sha256_matches_hex(buf, sz, RE_GAIDEN_EXPECTED_SHA256)) {
        fprintf(stderr, "[ROM Loader] File '%s' SHA256 checksum mismatch!\n", path);
        free(buf);
        return false;
    }

    if (g_rom_data) {
        free(g_rom_data);
    }
    g_rom_data = buf;
    g_rom_size = sz;
    printf("[ROM Loader] Successfully verified and loaded: %s\n", path);
    return true;
}

static void cache_rom_copy(const char* source_path) {
    const char* dest_path = "Resident Evil Gaiden (USA).gbc";
    if (strcmp(source_path, dest_path) == 0) {
        return;
    }

    FILE* in = fopen(source_path, "rb");
    if (!in) return;
    FILE* out = fopen(dest_path, "wb");
    if (!out) {
        fclose(in);
        return;
    }

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, bytes, out);
    }

    fclose(in);
    fclose(out);
    printf("[ROM Loader] Cached ROM locally to '%s'\n", dest_path);
}

#ifdef _WIN32
static bool prompt_user_for_rom_win32(char* out_path, size_t max_len) {
    OPENFILENAMEA ofn;
    char file_buffer[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Game Boy Color ROM (*.gbc;*.gb;*.bin)\0*.gbc;*.gb;*.bin\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = sizeof(file_buffer);
    ofn.lpstrTitle = "Select 'Resident Evil Gaiden (USA)' Game Boy Color ROM";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        strncpy(out_path, file_buffer, max_len - 1);
        out_path[max_len - 1] = '\0';
        return true;
    }
    return false;
}
#endif

bool rom_loader_acquire_rom(const char* explicit_path) {
    // 1. Try explicit path if provided
    if (explicit_path && explicit_path[0]) {
        if (validate_and_load(explicit_path)) {
            return true;
        }
        fprintf(stderr, "[ROM Loader] Explicit path '%s' is not a valid Resident Evil Gaiden (USA) ROM.\n", explicit_path);
    }

    // 2. Search default candidate paths
    for (int i = 0; s_candidate_paths[i] != NULL; ++i) {
        if (validate_and_load(s_candidate_paths[i])) {
            return true;
        }
    }

    // 3. Prompt the user via GUI file dialog or interactive prompt
    printf("\n===================================================================\n");
    printf("  Resident Evil Gaiden (GBC) - ROM Required\n");
    printf("===================================================================\n");
    printf("  To run this recompilation, please provide your legitimate\n");
    printf("  'Resident Evil Gaiden (USA).gbc' ROM image.\n\n");
    printf("  Expected SHA256: %s\n", RE_GAIDEN_EXPECTED_SHA256);
    printf("===================================================================\n\n");

#ifdef _WIN32
    char picked_path[MAX_PATH] = "";
    while (prompt_user_for_rom_win32(picked_path, sizeof(picked_path))) {
        if (validate_and_load(picked_path)) {
            cache_rom_copy(picked_path);
            return true;
        } else {
            MessageBoxA(NULL,
                "The selected file does not match the expected 'Resident Evil Gaiden (USA)' ROM.\n\n"
                "Expected file: Resident Evil Gaiden (USA).gbc\n"
                "Expected size: 2,097,152 bytes\n"
                "Expected SHA256: " RE_GAIDEN_EXPECTED_SHA256,
                "Invalid ROM Image",
                MB_OK | MB_ICONERROR);
        }
    }
#endif

    fprintf(stderr, "[ROM Loader] No valid ROM provided. Aborting.\n");
    return false;
}
