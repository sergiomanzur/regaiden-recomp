/* Main entry point */
#include "Resident_Evil_Gaiden__USA_.h"
#include "rom_loader.h"
#include "gbrt.h"
#include "gbrt_data_mod.h"
#include "gbrt_port.h"
#include "audio.h"
#include "audio_stats.h"
#ifdef GB_HAS_SDL2
#include <SDL.h>
#include "platform_sdl.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define GB_DUP2 _dup2
#define GB_FILENO _fileno
#else
#include <unistd.h>
#define GB_DUP2 dup2
#define GB_FILENO fileno
#endif

static bool gb_redirect_logs(const char* path) {
    if (!path || !path[0]) {
        return true;
    }
    if (!freopen(path, "w", stdout)) {
        fprintf(stderr, "Failed to open log file '%s' for stdout redirection\n", path);
        return false;
    }
    if (GB_DUP2(GB_FILENO(stdout), GB_FILENO(stderr)) < 0) {
        fprintf(stdout, "Failed to open log file '%s' for stderr redirection\n", path);
        fflush(stdout);
        return false;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "[LOG] Redirecting runtime output to %s\n", path);
    return true;
}

static bool gbrt_serial_verdict_seen = false;
static char gbrt_serial_verdict_window[7] = {0};

static void gbrt_serial_stdout(GBContext* ctx, uint8_t byte) {
    (void)ctx;
    fputc((int)byte, stdout);
    fflush(stdout);
    memmove(gbrt_serial_verdict_window, gbrt_serial_verdict_window + 1, 5);
    gbrt_serial_verdict_window[5] = (char)byte;
    if (memcmp(gbrt_serial_verdict_window, "Passed", 6) == 0 ||
        memcmp(gbrt_serial_verdict_window, "Failed", 6) == 0) {
        gbrt_serial_verdict_seen = true;
    }
}

#ifdef GB_HAS_SDL2
static double gb_profile_now_ms(void) {
    uint64_t ticks = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    return freq ? ((double)ticks * 1000.0) / (double)freq : 0.0;
}
#endif

static void gbrt_print_interpreter_summary(const GBContext* ctx, unsigned max_hotspots) {
    if (!ctx) {
        return;
    }
    fprintf(stderr,
            "[INTERP] Fallback inventory: sites=%u dropped=%llu complete=%s\n",
            (unsigned)ctx->dispatch_fallback_site_count,
            (unsigned long long)ctx->dispatch_fallback_sites_dropped,
            ctx->dispatch_fallback_sites_dropped == 0 ? "yes" : "no");
    for (uint16_t i = 0; i < ctx->dispatch_fallback_site_count; ++i) {
        const GBDispatchFallbackSite* site = &ctx->dispatch_fallback_sites[i];
        if (!site->valid) continue;
        fprintf(stderr,
                "[INTERP] Fallback site #%u %03X:%04X reason=%s entries=%llu instructions=%llu cycles=%llu first_frame=%llu last_frame=%llu compiled_bank_variants=%u\n",
                (unsigned)(i + 1),
                site->bank,
                site->addr,
                gbrt_dispatch_fallback_reason_name(site->reason),
                (unsigned long long)site->entries,
                (unsigned long long)site->instructions,
                (unsigned long long)site->cycles,
                (unsigned long long)site->first_frame,
                (unsigned long long)site->last_frame,
                (unsigned)site->compiled_bank_variants);
    }
    if (ctx->total_dispatch_fallbacks == 0 &&
        ctx->total_interpreter_entries == 0 &&
        !ctx->has_unimplemented_interpreter_opcode) {
        fprintf(stderr, "[INTERP] No interpreter fallback recorded.\n");
        return;
    }
    fprintf(stderr,
            "[INTERP] Summary: fallbacks=%llu interpreter_entries=%llu interpreter_instructions=%llu interpreter_cycles=%llu\n",
            (unsigned long long)ctx->total_dispatch_fallbacks,
            (unsigned long long)ctx->total_interpreter_entries,
            (unsigned long long)ctx->total_interpreter_instructions,
            (unsigned long long)ctx->total_interpreter_cycles);
    unsigned printed = 0;
    for (size_t i = 0; i < GBRT_INTERPRETER_HOTSPOT_CAPACITY && printed < max_hotspots; i++) {
        const GBInterpreterHotspot* hotspot = &ctx->interpreter_hotspots[i];
        if (!hotspot->valid || hotspot->entries == 0) {
            continue;
        }
        fprintf(stderr,
                "[INTERP] Hotspot #%u %03X:%04X entries=%llu instructions=%llu cycles=%llu last_frame=%llu\n",
                printed + 1,
                hotspot->bank,
                hotspot->addr,
                (unsigned long long)hotspot->entries,
                (unsigned long long)hotspot->instructions,
                (unsigned long long)hotspot->cycles,
                (unsigned long long)hotspot->last_frame);
        printed++;
    }
    if (ctx->has_unimplemented_interpreter_opcode) {
        fprintf(stderr,
                "[INTERP] Coverage gap: opcode=%02X at %03X:%04X\n",
                ctx->last_unimplemented_opcode,
                ctx->last_unimplemented_bank,
                ctx->last_unimplemented_addr);
    }
}

static const GBContext* gbrt_interpreter_summary_ctx = NULL;
static unsigned gbrt_interpreter_summary_limit = 8;
static int gbrt_interpreter_summary_enabled = 0;
static int gbrt_interpreter_summary_atexit_registered = 0;

static void gbrt_flush_interpreter_summary(void) {
    if (!gbrt_interpreter_summary_enabled || !gbrt_interpreter_summary_ctx) {
        return;
    }
    gbrt_print_interpreter_summary(gbrt_interpreter_summary_ctx, gbrt_interpreter_summary_limit);
    gbrt_interpreter_summary_ctx = NULL;
    gbrt_interpreter_summary_enabled = 0;
    if (gbrt_instruction_limit_callback == gbrt_flush_interpreter_summary) {
        gbrt_instruction_limit_callback = NULL;
    }
}

static void gbrt_enable_interpreter_summary(const GBContext* ctx, unsigned max_hotspots) {
    gbrt_interpreter_summary_ctx = ctx;
    gbrt_interpreter_summary_limit = max_hotspots;
    gbrt_interpreter_summary_enabled = 1;
    gbrt_instruction_limit_callback = gbrt_flush_interpreter_summary;
    if (!gbrt_interpreter_summary_atexit_registered) {
        atexit(gbrt_flush_interpreter_summary);
        gbrt_interpreter_summary_atexit_registered = 1;
    }
}

static void gbrt_disable_interpreter_summary(void) {
    gbrt_interpreter_summary_ctx = NULL;
    gbrt_interpreter_summary_enabled = 0;
    if (gbrt_instruction_limit_callback == gbrt_flush_interpreter_summary) {
        gbrt_instruction_limit_callback = NULL;
    }
}

int Resident_Evil_Gaiden__USA__main(int argc, char* argv[]) {
    bool debug_audio = false;
    bool debug_audio_trace = false;
    bool audio_stats_console = false;
    bool no_audio = false;
    unsigned debug_audio_seconds = 10;
    bool differential_mode = false;
    unsigned long long differential_steps = 10000;
    bool differential_steps_explicit = false;
    unsigned long long differential_frames = 0;
    unsigned long long differential_log_interval = 1000;
    bool differential_compare_memory = true;
    bool differential_log_fallbacks = false;
    bool differential_fail_on_fallback = false;
    unsigned long long differential_inject_mismatch_step = 0;
    bool debug_performance = false;
    const char* input_script = NULL;
    const char* persistence_dir = NULL;
    const char* log_file = NULL;
    const char* state_dump_file = NULL;
    const char* save_state_file = NULL;
    const char* load_state_file = NULL;
    const char* data_mod_file = NULL;
    const char* differential_state_file = NULL;
    unsigned long long frame_limit = 0;
    bool rtc_unix_time_override_enabled = false;
    unsigned long long rtc_unix_time_override = 0;
    bool ignore_rtc_persistence = false;
    double slow_frame_ms = 0.0;
    double slow_vsync_ms = 0.0;
    bool log_frame_fallbacks = false;
    bool log_lcd_transitions = false;
    bool report_interpreter_hotspots = false;
    bool report_performance_counters = false;
    bool estimate_visibility_regions = false;
    bool force_scalar_timer = false;
    bool force_eager_audio = false;
    unsigned long interpreter_hotspot_limit = 8;
    int smooth_lcd_transitions_override = -1;
    bool benchmark_mode = false;
    bool headless_mode = false;
    bool serial_stdout = false;
    bool stop_on_serial_verdict = false;
    bool native_presentation_enabled = false;
#ifdef GBRT_ENABLE_PORT_MODULE
    bool port_ui_open = false;
    bool port_module_disabled = false;
    unsigned long long port_toggle_frames[64] = {0};
    size_t port_toggle_frame_count = 0;
    unsigned long long port_input_frames[64] = {0};
    GBPortInputAction port_input_actions[64] = {0};
    size_t port_input_frame_count = 0;
    const char* port_state_file = NULL;
#endif
    const char* explicit_rom_path = NULL;
    const char* model_override = "auto";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            log_file = argv[++i];
        } else if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            explicit_rom_path = argv[++i];
        }
    }
    if (!gb_redirect_logs(log_file)) {
        return 1;
    }
    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            explicit_rom_path = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0) {
            gbrt_trace_enabled = true;
            printf("Trace enabled\n");
        } else if (strcmp(argv[i], "--trace-entries") == 0 && i + 1 < argc) {
            gbrt_set_trace_file(argv[++i]);
        } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            gbrt_instruction_limit = strtoull(argv[++i], NULL, 10);
            printf("Instruction limit: %llu\n", (unsigned long long)gbrt_instruction_limit);
        } else if (strcmp(argv[i], "--limit-frames") == 0 && i + 1 < argc) {
            frame_limit = strtoull(argv[++i], NULL, 10);
            printf("Frame limit: %llu\n", (unsigned long long)frame_limit);
        } else if (strcmp(argv[i], "--stop-on-test-breakpoint") == 0) {
            gbrt_test_breakpoint_enabled = true;
        } else if (strcmp(argv[i], "--dump-state") == 0 && i + 1 < argc) {
            state_dump_file = argv[++i];
        } else if (strcmp(argv[i], "--save-state-file") == 0 && i + 1 < argc) {
            save_state_file = argv[++i];
        } else if (strcmp(argv[i], "--load-state-file") == 0 && i + 1 < argc) {
            load_state_file = argv[++i];
        } else if (strcmp(argv[i], "--data-mod") == 0 && i + 1 < argc) {
            data_mod_file = argv[++i];
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_script = argv[++i];
            if (!gb_platform_set_input_script(input_script)) {
                fprintf(stderr, "Invalid input script\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--save-dir") == 0 && i + 1 < argc) {
            persistence_dir = argv[++i];
        } else if (strcmp(argv[i], "--rtc-unix-time") == 0 && i + 1 < argc) {
            const char* rtc_arg = argv[++i];
            char* rtc_end = NULL;
            rtc_unix_time_override = strtoull(rtc_arg, &rtc_end, 10);
            if (!rtc_arg[0] || !rtc_end || rtc_end[0]) {
                fprintf(stderr, "Invalid RTC Unix time: %s\n", rtc_arg);
                return 1;
            }
            rtc_unix_time_override_enabled = true;
        } else if (strcmp(argv[i], "--ignore-rtc-persistence") == 0) {
            ignore_rtc_persistence = true;
        } else if (strcmp(argv[i], "--record-input") == 0 && i + 1 < argc) {
            gb_platform_set_input_record_file(argv[++i]);
        } else if (strcmp(argv[i], "--dump-frames") == 0 && i + 1 < argc) {
            gb_platform_set_dump_frames(argv[++i]);
        } else if (strcmp(argv[i], "--dump-present-frames") == 0 && i + 1 < argc) {
            gb_platform_set_dump_present_frames(argv[++i]);
        } else if (strcmp(argv[i], "--screenshot-prefix") == 0 && i + 1 < argc) {
            gb_platform_set_screenshot_prefix(argv[++i]);
        } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(argv[i], "--debug-performance") == 0) {
            debug_performance = true;
        } else if (strcmp(argv[i], "--debug-audio") == 0) {
            debug_audio = true;
        } else if (strcmp(argv[i], "--debug-audio-seconds") == 0 && i + 1 < argc) {
            debug_audio_seconds = (unsigned)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--debug-audio-trace") == 0) {
            debug_audio_trace = true;
        } else if (strcmp(argv[i], "--audio-stats") == 0) {
            audio_stats_console = true;
        } else if (strcmp(argv[i], "--no-audio") == 0) {
            no_audio = true;
        } else if (strcmp(argv[i], "--log-slow-frames") == 0 && i + 1 < argc) {
            slow_frame_ms = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--log-slow-vsync") == 0 && i + 1 < argc) {
            slow_vsync_ms = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--log-frame-fallbacks") == 0) {
            log_frame_fallbacks = true;
        } else if (strcmp(argv[i], "--log-lcd-transitions") == 0) {
            log_lcd_transitions = true;
        } else if (strcmp(argv[i], "--report-interpreter-hotspots") == 0) {
            report_interpreter_hotspots = true;
        } else if (strcmp(argv[i], "--report-performance-counters") == 0) {
            report_performance_counters = true;
        } else if (strcmp(argv[i], "--estimate-visibility-regions") == 0) {
            estimate_visibility_regions = true;
        } else if (strcmp(argv[i], "--scalar-timer") == 0) {
            force_scalar_timer = true;
        } else if (strcmp(argv[i], "--eager-audio") == 0) {
            force_eager_audio = true;
        } else if (strcmp(argv[i], "--interpreter-hotspot-limit") == 0 && i + 1 < argc) {
            interpreter_hotspot_limit = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--smooth-lcd-transitions") == 0) {
            smooth_lcd_transitions_override = 1;
        } else if (strcmp(argv[i], "--no-smooth-lcd-transitions") == 0) {
            smooth_lcd_transitions_override = 0;
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_override = argv[++i];
        } else if (strcmp(argv[i], "--differential") == 0) {
            differential_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                differential_steps = strtoull(argv[++i], NULL, 10);
                differential_steps_explicit = true;
            }
        } else if (strcmp(argv[i], "--differential-frames") == 0 && i + 1 < argc) {
            differential_frames = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--differential-state") == 0 && i + 1 < argc) {
            differential_state_file = argv[++i];
        } else if (strcmp(argv[i], "--differential-log") == 0 && i + 1 < argc) {
            differential_log_interval = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--differential-no-memory") == 0) {
            differential_compare_memory = false;
        } else if (strcmp(argv[i], "--differential-log-fallbacks") == 0) {
            differential_log_fallbacks = true;
        } else if (strcmp(argv[i], "--differential-fail-on-fallback") == 0) {
            differential_fail_on_fallback = true;
        } else if (strcmp(argv[i], "--differential-inject-mismatch") == 0 && i + 1 < argc) {
            differential_inject_mismatch_step = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            benchmark_mode = true;
        } else if (strcmp(argv[i], "--headless") == 0) {
            headless_mode = true;
        } else if (strcmp(argv[i], "--native-presentation") == 0 && i + 1 < argc) {
            const char* presentation_arg = argv[++i];
            if (strcmp(presentation_arg, "native") == 0) {
                native_presentation_enabled = true;
            } else if (strcmp(presentation_arg, "original") == 0) {
                native_presentation_enabled = false;
            } else {
                fprintf(stderr, "Unknown native presentation '%s' (expected native or original)\n", presentation_arg);
                return 1;
            }
#ifdef GBRT_ENABLE_PORT_MODULE
        } else if (strcmp(argv[i], "--port-ui-open") == 0) {
            port_ui_open = true;
        } else if (strcmp(argv[i], "--disable-port-module") == 0) {
            port_module_disabled = true;
        } else if (strcmp(argv[i], "--port-toggle-frame") == 0 && i + 1 < argc) {
            const char* toggle_arg = argv[++i];
            char* toggle_end = NULL;
            unsigned long long toggle_frame = strtoull(toggle_arg, &toggle_end, 10);
            if (!toggle_arg[0] || !toggle_end || toggle_end[0] || toggle_frame == 0 ||
                port_toggle_frame_count >= 64 ||
                (port_toggle_frame_count > 0 &&
                 toggle_frame <= port_toggle_frames[port_toggle_frame_count - 1])) {
                fprintf(stderr, "Invalid or unordered port toggle frame: %s\n", toggle_arg);
                return 1;
            }
            port_toggle_frames[port_toggle_frame_count++] = toggle_frame;
        } else if (strcmp(argv[i], "--port-input-frame") == 0 && i + 1 < argc) {
            const char* input_arg = argv[++i];
            const char* colon = strchr(input_arg, ':');
            char* input_end = NULL;
            unsigned long long input_frame = strtoull(input_arg, &input_end, 10);
            GBPortInputAction input_action = GB_PORT_INPUT_TOGGLE_UI;
            bool input_action_valid = colon != NULL && input_end == colon;
            if (input_action_valid && strcmp(colon + 1, "toggle") == 0) input_action = GB_PORT_INPUT_TOGGLE_UI;
            else if (input_action_valid && strcmp(colon + 1, "close") == 0) input_action = GB_PORT_INPUT_CLOSE_UI;
            else if (input_action_valid && strcmp(colon + 1, "up") == 0) input_action = GB_PORT_INPUT_UP;
            else if (input_action_valid && strcmp(colon + 1, "down") == 0) input_action = GB_PORT_INPUT_DOWN;
            else if (input_action_valid && strcmp(colon + 1, "left") == 0) input_action = GB_PORT_INPUT_LEFT;
            else if (input_action_valid && strcmp(colon + 1, "right") == 0) input_action = GB_PORT_INPUT_RIGHT;
            else if (input_action_valid && strcmp(colon + 1, "accept") == 0) input_action = GB_PORT_INPUT_ACCEPT;
            else if (input_action_valid && strcmp(colon + 1, "back") == 0) input_action = GB_PORT_INPUT_BACK;
            else if (input_action_valid && strcmp(colon + 1, "open-pc") == 0) input_action = GB_PORT_INPUT_OPEN_PC;
            else if (input_action_valid && strcmp(colon + 1, "encounters") == 0) input_action = GB_PORT_INPUT_TOGGLE_ENCOUNTERS;
            else input_action_valid = false;
            if (!input_arg[0] || !input_action_valid || input_frame == 0 ||
                port_input_frame_count >= 64 ||
                (port_input_frame_count > 0 &&
                 input_frame <= port_input_frames[port_input_frame_count - 1])) {
                fprintf(stderr, "Invalid or unordered port input frame: %s\n", input_arg);
                return 1;
            }
            port_input_frames[port_input_frame_count] = input_frame;
            port_input_actions[port_input_frame_count] = input_action;
            port_input_frame_count++;
        } else if (strcmp(argv[i], "--port-state") == 0 && i + 1 < argc) {
            port_state_file = argv[++i];
#endif
        } else if (strcmp(argv[i], "--serial-stdout") == 0) {
            serial_stdout = true;
        } else if (strcmp(argv[i], "--stop-on-serial-verdict") == 0) {
            stop_on_serial_verdict = true;
        }
    }

    if (!rom_loader_acquire_rom(explicit_rom_path)) {
        fprintf(stderr, "Error: Valid Resident Evil Gaiden (USA) GBC ROM was not provided. Exiting.\n");
        return 1;
    }

    gbrt_visibility_estimator_enabled = estimate_visibility_regions;

    if (persistence_dir && !gb_platform_set_persistence_dir(persistence_dir)) {
        fprintf(stderr, "Invalid save directory: %s\n", persistence_dir);
        return 1;
    }

    gbrt_force_scalar_timer = force_scalar_timer;

    gbrt_force_eager_audio = force_eager_audio;

    if (debug_performance) {
        audio_stats_console = true;
        log_frame_fallbacks = true;
        log_lcd_transitions = true;
        report_interpreter_hotspots = true;
        if (slow_frame_ms <= 0.0) slow_frame_ms = 1.0;
        if (slow_vsync_ms <= 0.0) slow_vsync_ms = 0.1;
        fprintf(stderr,
                "[PERF] Enabled performance debug logging (frame>=%.3fms, vsync>=%.3fms, fallbacks, LCD transitions, audio stats)\n",
                slow_frame_ms,
                slow_vsync_ms);
    }

    GBConfig runtime_config = *Resident_Evil_Gaiden__USA__default_config();
    runtime_config.rtc_unix_time_override_enabled = rtc_unix_time_override_enabled;
    runtime_config.rtc_unix_time_override = rtc_unix_time_override;
    runtime_config.ignore_rtc_persistence = ignore_rtc_persistence;
    runtime_config.native_presentation_enabled = native_presentation_enabled;
    if (rtc_unix_time_override_enabled) {
        printf("[GBRT] RTC Unix time override: %llu\n", rtc_unix_time_override);
    }
    if (benchmark_mode || no_audio) {
        runtime_config.enable_audio = false;
    }
    gbrt_rgb_framebuffer_enabled = !benchmark_mode;
    gbrt_benchmark_fast_tick_enabled = benchmark_mode;
    if (strcmp(model_override, "auto") == 0) {
        runtime_config.model = runtime_config.cartridge_supports_cgb ? GB_MODEL_CGB : GB_MODEL_DMG;
        runtime_config.cgb_compatibility_mode = false;
    } else if (strcmp(model_override, "dmg") == 0) {
        if (runtime_config.cartridge_requires_cgb) {
            fprintf(stderr, "CGB-only ROMs cannot run with --model dmg\n");
            return 1;
        }
        runtime_config.model = GB_MODEL_DMG;
        runtime_config.cgb_compatibility_mode = false;
    } else if (strcmp(model_override, "cgb") == 0) {
        runtime_config.model = GB_MODEL_CGB;
        runtime_config.cgb_compatibility_mode = !runtime_config.cartridge_supports_cgb;
    } else {
        fprintf(stderr, "Unknown model '%s' (expected auto, dmg, or cgb)\n", model_override);
        return 1;
    }

    if (differential_mode && differential_frames > 0 && !differential_steps_explicit) {
        differential_steps = 0;
    }

    if (differential_mode && load_state_file) {
        fprintf(stderr, "--load-state-file is for normal execution; use --differential-state in differential mode\n");
        return 1;
    }

    if (differential_mode) {
        GBContext* generated_ctx = gb_context_create(&runtime_config);
        GBContext* interpreted_ctx = gb_context_create(&runtime_config);
        if (!generated_ctx || !interpreted_ctx) {
            fprintf(stderr, "Failed to create differential contexts\n");
            gb_context_destroy(generated_ctx);
            gb_context_destroy(interpreted_ctx);
            return 1;
        }
        gb_context_set_save_id(generated_ctx, "Resident_Evil_Gaiden__USA_");
        gb_context_set_save_id(interpreted_ctx, "Resident_Evil_Gaiden__USA_");
        if (serial_stdout) {
            generated_ctx->callbacks.on_serial_byte = gbrt_serial_stdout;
        }
        Resident_Evil_Gaiden__USA__init(generated_ctx);
        Resident_Evil_Gaiden__USA__init(interpreted_ctx);
        if (data_mod_file) {
            GBDataModStatus generated_mod_status = gbrt_data_mod_load_file(generated_ctx, data_mod_file);
            GBDataModStatus interpreted_mod_status = gbrt_data_mod_load_file(interpreted_ctx, data_mod_file);
            if (generated_mod_status != GB_DATA_MOD_OK || interpreted_mod_status != GB_DATA_MOD_OK) {
                fprintf(stderr, "Data-mod activation failed: generated=%s interpreted=%s\n",
                        gbrt_data_mod_status_string(generated_mod_status),
                        gbrt_data_mod_status_string(interpreted_mod_status));
                gb_context_destroy(generated_ctx);
                gb_context_destroy(interpreted_ctx);
                return 1;
            }
            fprintf(stderr, "[DATA-MOD] Active entries=%zu\n",
                    gbrt_data_mod_entry_count(generated_ctx));
        }
        if (differential_state_file) {
            if (!gb_context_load_state_file(generated_ctx, differential_state_file) ||
                !gb_context_load_state_file(interpreted_ctx, differential_state_file)) {
                fprintf(stderr, "Failed to load differential state: %s\n", differential_state_file);
                gb_context_destroy(generated_ctx);
                gb_context_destroy(interpreted_ctx);
                return 1;
            }
            fprintf(stderr, "[DIFF] Loaded comparison state: %s\n", differential_state_file);
        }
        GBDifferentialOptions diff_options = {
            .max_steps = differential_steps,
            .max_frames = differential_frames,
            .log_interval = differential_log_interval,
            .compare_memory = differential_compare_memory,
            .log_fallbacks = differential_log_fallbacks,
            .fail_on_fallback = differential_fail_on_fallback,
            .inject_mismatch_step = differential_inject_mismatch_step,
            .input_script = input_script,
        };
        GBDifferentialResult diff_result;
        bool matched = gb_run_differential(generated_ctx, interpreted_ctx, &diff_options, &diff_result);
        if (report_performance_counters) {
            gbrt_report_performance_counters(generated_ctx);
        }
        if (state_dump_file && !gb_context_write_state_json(generated_ctx, state_dump_file)) {
            fprintf(stderr, "Failed to write state snapshot: %s\n", state_dump_file);
            matched = false;
        }
        gb_context_destroy(generated_ctx);
        gb_context_destroy(interpreted_ctx);
        return matched ? 0 : 1;
    }

    GBContext* ctx = gb_context_create(&runtime_config);
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }
    gb_context_set_save_id(ctx, "Resident_Evil_Gaiden__USA_");
    gbrt_log_lcd_transitions = log_lcd_transitions;
    gbrt_dispatch_fallback_tracking_enabled = log_frame_fallbacks || report_interpreter_hotspots;
    if (debug_audio) gb_audio_set_debug(true);
    gb_audio_set_debug_capture_seconds(debug_audio_seconds);
    if (debug_audio_trace) gb_audio_set_debug_trace(true);
    audio_stats_set_log_to_console(audio_stats_console);
#ifdef GB_HAS_SDL2
    // Initialize SDL2 platform with 5x scaling
    if (benchmark_mode || headless_mode) {
        gb_platform_set_benchmark_mode(true);
    }
    if (!gb_platform_init(5)) {
        fprintf(stderr, "Failed to initialize platform\n");
        gb_context_destroy(ctx);
        return 1;
    }
    gb_platform_register_context(ctx);
    if (smooth_lcd_transitions_override >= 0) {
        gb_platform_set_smooth_lcd_transitions(smooth_lcd_transitions_override != 0);
    }
#endif
    if (serial_stdout) {
        ctx->callbacks.on_serial_byte = gbrt_serial_stdout;
    }
    Resident_Evil_Gaiden__USA__init(ctx);
    if (data_mod_file) {
        GBDataModStatus data_mod_status = gbrt_data_mod_load_file(ctx, data_mod_file);
        if (data_mod_status != GB_DATA_MOD_OK) {
            fprintf(stderr, "Data-mod activation failed: %s\n",
                    gbrt_data_mod_status_string(data_mod_status));
#ifdef GB_HAS_SDL2
            gb_platform_shutdown();
#endif
            gb_context_destroy(ctx);
            return 1;
        }
        fprintf(stderr, "[DATA-MOD] Active entries=%zu\n",
                gbrt_data_mod_entry_count(ctx));
    }
    if (load_state_file) {
        if (!gb_context_load_state_file(ctx, load_state_file)) {
            fprintf(stderr, "Failed to load execution state\n");
#ifdef GB_HAS_SDL2
            gb_platform_shutdown();
#endif
            gb_context_destroy(ctx);
            return 1;
        }
        fprintf(stderr, "[GBRT] Loaded execution state\n");
    }
#ifdef GBRT_ENABLE_PORT_MODULE
    const GBPortMetadata port_metadata = {
        .abi_version = GB_PORT_ABI_VERSION,
        .game_id = "Resident_Evil_Gaiden__USA_",
        .game_title = "Resident_Evil_Gaiden__USA_",
        .rom_sha256 = "9a97678cbd8da02c8763e977674e17f460c06ea8b73bad35c52fe6817f506d44",
        .rom_size = 2097152u,
    };
    const GBPortHost port_host = {
        .abi_version = GB_PORT_ABI_VERSION,
        .headless = headless_mode || benchmark_mode,
#ifdef GB_HAS_SDL2
        .submit_frame = gb_platform_submit_port_frame,
#endif
    };
    GBPortStatus port_status = GB_PORT_OK;
    if (!port_module_disabled) {
        port_status = gbrt_port_attach(
            ctx, gb_port_module_get(), &port_metadata, &port_host);
    }
    if (port_status != GB_PORT_OK) {
        fprintf(stderr, "Port module activation failed: %d\n", (int)port_status);
#ifdef GB_HAS_SDL2
        gb_platform_shutdown();
#endif
        gb_context_destroy(ctx);
        return 1;
    }
    if (port_ui_open && !port_module_disabled) {
        const GBPortInputEvent event = {GB_PORT_INPUT_TOGGLE_UI, true};
        gbrt_port_input(ctx, &event);
    }
#endif
    int exit_code = 0;

#ifdef GB_HAS_SDL2
    if (report_interpreter_hotspots) {
        gbrt_enable_interpreter_summary(ctx, (unsigned)interpreter_hotspot_limit);
    }

    // Run the game loop
    unsigned long long frame_index = 0;
    if (benchmark_mode) {
        while ((frame_limit == 0 || frame_index < frame_limit) &&
               !(stop_on_serial_verdict && gbrt_serial_verdict_seen)) {
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            while (!ctx->frame_done) {
                gb_run_cycles(ctx, 0xFFFFFFFFu);
            }
            frame_index++;
#ifdef GBRT_ENABLE_PORT_MODULE
            for (size_t toggle_index = 0; toggle_index < port_toggle_frame_count; ++toggle_index) {
                if (frame_index == port_toggle_frames[toggle_index]) {
                    const GBPortInputEvent event = {GB_PORT_INPUT_TOGGLE_UI, true};
                    gbrt_port_input(ctx, &event);
                    break;
                }
            }
            for (size_t input_index = 0; input_index < port_input_frame_count; ++input_index) {
                if (frame_index == port_input_frames[input_index]) {
                    const GBPortInputEvent event = {port_input_actions[input_index], true};
                    gbrt_port_input(ctx, &event);
                    break;
                }
            }
            gbrt_port_update(ctx, frame_index, ctx->frame_cycles);
            gbrt_port_render(ctx, 1280, 720);
#endif
        }
        fprintf(stderr, "[LIMIT] Reached frame limit %llu\n", (unsigned long long)frame_limit);
        if (state_dump_file && !gb_context_write_state_json(ctx, state_dump_file)) {
            fprintf(stderr, "Failed to write state snapshot: %s\n", state_dump_file);
            exit_code = 1;
        }
        if (save_state_file && !gb_context_save_state_file(ctx, save_state_file)) {
            fprintf(stderr, "Failed to write savestate: %s\n", save_state_file);
            exit_code = 1;
        }
        if (report_performance_counters) {
            gbrt_report_performance_counters(ctx);
        }
#ifdef GBRT_ENABLE_PORT_MODULE
        if (port_state_file && !gbrt_port_write_state_json(ctx, port_state_file)) {
            fprintf(stderr, "Failed to write port state: %s\n", port_state_file);
            exit_code = 1;
        }
#endif
        gb_platform_shutdown();
        gb_context_destroy(ctx);
        return exit_code;
    }
    const uint32_t lcd_smooth_slice_cycles = 70224u;
    bool running = true;
    while (running) {
        double emu_ms = 0.0;
        double render_ms = 0.0;
        double upload_ms = 0.0;
        double compose_ms = 0.0;
        double present_ms = 0.0;
        double vsync_ms = 0.0;
        uint32_t paced_cycles = 0;
        GBPlatformTimingInfo timing_info = {0};
        gb_reset_frame(ctx);
        ctx->stopped = 0;
        while (!ctx->frame_done) {
            bool smooth_lcd_transitions = gb_platform_get_smooth_lcd_transitions();
            uint32_t slice_budget = smooth_lcd_transitions ? lcd_smooth_slice_cycles : 0xFFFFFFFFu;
            uint32_t slice_start_cycles = ctx->frame_cycles;
            double slice_start_ms = gb_profile_now_ms();
            gb_run_cycles(ctx, slice_budget);
            emu_ms += gb_profile_now_ms() - slice_start_ms;
            uint32_t slice_cycles = ctx->frame_cycles - slice_start_cycles;
            if (stop_on_serial_verdict && gbrt_serial_verdict_seen) {
                running = false;
                break;
            }
            if (!gb_platform_poll_events(ctx)) {
                running = false;
                break;
            }
            if (smooth_lcd_transitions && !ctx->frame_done && slice_cycles >= lcd_smooth_slice_cycles) {
                if (ctx->lcd_off_active || !(ctx->io[0x40] & 0x80)) {
                    gb_platform_render_lcd_off_frame();
                } else {
                    const uint32_t* slice_fb = gb_get_framebuffer(ctx);
                    if (slice_fb) gb_platform_present_framebuffer(slice_fb);
                }
                gb_platform_get_timing_info(&timing_info);
                render_ms += timing_info.total_render_ms;
                upload_ms += timing_info.upload_ms;
                compose_ms += timing_info.compose_ms;
                present_ms += timing_info.present_ms;
                gb_platform_vsync(slice_cycles);
                paced_cycles += slice_cycles;
            }
        }
        if (!running) break;
        if (ctx->frame_done) {
            uint32_t completed_frame_cycles = ctx->frame_cycles;
            uint32_t final_pacing_cycles = (completed_frame_cycles > paced_cycles) ? (completed_frame_cycles - paced_cycles) : 0;
            frame_index++;
#ifdef GBRT_ENABLE_PORT_MODULE
            for (size_t toggle_index = 0; toggle_index < port_toggle_frame_count; ++toggle_index) {
                if (frame_index == port_toggle_frames[toggle_index]) {
                    const GBPortInputEvent event = {GB_PORT_INPUT_TOGGLE_UI, true};
                    gbrt_port_input(ctx, &event);
                    break;
                }
            }
            for (size_t input_index = 0; input_index < port_input_frame_count; ++input_index) {
                if (frame_index == port_input_frames[input_index]) {
                    const GBPortInputEvent event = {port_input_actions[input_index], true};
                    gbrt_port_input(ctx, &event);
                    break;
                }
            }
            gbrt_port_update(ctx, frame_index, completed_frame_cycles);
            gbrt_port_render(ctx, 1280, 720);
#endif
            const uint32_t* fb = gb_get_framebuffer(ctx);
            if (fb) gb_platform_render_frame(fb);
            gb_platform_get_timing_info(&timing_info);
            render_ms += timing_info.total_render_ms;
            upload_ms += timing_info.upload_ms;
            compose_ms += timing_info.compose_ms;
            present_ms += timing_info.present_ms;
            if ((slow_frame_ms > 0.0 && (emu_ms + render_ms) >= slow_frame_ms) ||
                (log_frame_fallbacks && ctx->frame_dispatch_fallbacks > 0)) {
                fprintf(stderr,
                        "[FRAME] #%llu emu=%.3fms render=%.3fms upload=%.3fms compose=%.3fms present=%.3fms cycles=%u fallbacks=%u interp_instr=%llu interp_cycles=%llu first=%03X:%04X last=%03X:%04X total_fallbacks=%llu lcd_off_cycles=%u lcd_transitions=%u lcd_spans=%u last_lcd_off_span=%u\n",
                        frame_index,
                        emu_ms,
                        render_ms,
                        upload_ms,
                        compose_ms,
                        present_ms,
                        completed_frame_cycles,
                        ctx->frame_dispatch_fallbacks,
                        (unsigned long long)ctx->frame_interpreter_instructions,
                        (unsigned long long)ctx->frame_interpreter_cycles,
                        (unsigned)ctx->frame_first_fallback_bank,
                        ctx->frame_first_fallback_addr,
                        (unsigned)ctx->frame_last_fallback_bank,
                        ctx->frame_last_fallback_addr,
                        (unsigned long long)ctx->total_dispatch_fallbacks,
                        ctx->frame_lcd_off_cycles,
                        ctx->frame_lcd_transition_count,
                        ctx->frame_lcd_off_span_count,
                        ctx->last_lcd_off_span_cycles);
            }
            if (final_pacing_cycles > 0) {
                gb_platform_vsync(final_pacing_cycles);
                gb_platform_get_timing_info(&timing_info);
                vsync_ms = timing_info.pacing_ms;
            }
            if (slow_vsync_ms > 0.0 && vsync_ms >= slow_vsync_ms) {
                fprintf(stderr,
                        "[VSYNC] #%llu wait=%.3fms cycles=%u\n",
                        frame_index,
                        vsync_ms,
                        final_pacing_cycles);
            }
            if (frame_limit > 0 && frame_index >= frame_limit) {
                fprintf(stderr,
                        "[LIMIT] Reached frame limit %llu\n",
                        (unsigned long long)frame_limit);
                break;
            }
        }
    }
    if (gb_platform_get_exit_action() == GB_PLATFORM_EXIT_RETURN_TO_LAUNCHER) {
        exit_code = GB_PLATFORM_RETURN_TO_LAUNCHER_EXIT_CODE;
    }
    gb_platform_shutdown();
#else
    // No SDL2 - just run for testing
    if (report_interpreter_hotspots) {
        gbrt_enable_interpreter_summary(ctx, (unsigned)interpreter_hotspot_limit);
    }
    Resident_Evil_Gaiden__USA__run(ctx);
    printf("Recompiled code executed successfully!\n");
    printf("Registers: A=%02X B=%02X C=%02X\n", ctx->a, ctx->b, ctx->c);
#endif

    if (report_interpreter_hotspots) {
        gbrt_flush_interpreter_summary();
    }
    gbrt_disable_interpreter_summary();
    if (report_performance_counters) {
        gbrt_report_performance_counters(ctx);
    }
    if (state_dump_file && !gb_context_write_state_json(ctx, state_dump_file)) {
        fprintf(stderr, "Failed to write state snapshot: %s\n", state_dump_file);
        exit_code = 1;
    }
    if (save_state_file && !gb_context_save_state_file(ctx, save_state_file)) {
        fprintf(stderr, "Failed to write savestate: %s\n", save_state_file);
        exit_code = 1;
    }
#ifdef GBRT_ENABLE_PORT_MODULE
    if (port_state_file && !gbrt_port_write_state_json(ctx, port_state_file)) {
        fprintf(stderr, "Failed to write port state: %s\n", port_state_file);
        exit_code = 1;
    }
#endif
    gb_context_destroy(ctx);
    return exit_code;
}

int main(int argc, char* argv[]) {
    return Resident_Evil_Gaiden__USA__main(argc, argv);
}
