/**
 * @file audio_stats.c
 * @brief Audio performance statistics implementation
 */

#include "audio_stats.h"
#include <stdio.h>
#include <string.h>

/* Global stats instance */
AudioStats g_audio_stats = {0};

/* Buffer for formatted output */
static char g_stats_buffer[512];

void audio_stats_init(void) {
    memset(&g_audio_stats, 0, sizeof(AudioStats));
    g_audio_stats.enabled = true;
    g_audio_stats.log_to_console = false;  /* Disabled by default */
}

void audio_stats_set_log_to_console(bool enabled) {
    g_audio_stats.log_to_console = enabled;
}

void audio_stats_tick(uint64_t current_time_ms) {
    if (!g_audio_stats.enabled) return;

    if (g_audio_stats.last_reset_time_ms == 0) {
        g_audio_stats.last_reset_time_ms = current_time_ms;
        return;
    }
    
    uint64_t elapsed = current_time_ms - g_audio_stats.last_reset_time_ms;
    
    /* Update once per second */
    if (elapsed >= 1000) {
        /* Calculate actual sample rate */
        if (elapsed > 0) {
            g_audio_stats.sample_rate_actual = 
                (float)g_audio_stats.samples_generated * 1000.0f / (float)elapsed;
        }
        
        /* Save snapshots */
        g_audio_stats.last_samples_generated = g_audio_stats.samples_generated;
        g_audio_stats.last_samples_queued = g_audio_stats.samples_queued;
        g_audio_stats.last_samples_dropped = g_audio_stats.samples_dropped;
        
        /* Log to console if enabled */
        if (g_audio_stats.log_to_console) {
            audio_stats_print();
        }
        
        /* Reset per-second counters */
        g_audio_stats.samples_generated = 0;
        g_audio_stats.samples_queued = 0;
        g_audio_stats.samples_dropped = 0;
        g_audio_stats.last_buffer_underruns = 0;
        g_audio_stats.last_reset_time_ms = current_time_ms;
    }
}

void audio_stats_update_buffer(uint32_t fill_samples, uint32_t capacity_samples, uint32_t sample_rate) {
    g_audio_stats.buffer_fill_samples = fill_samples;
    g_audio_stats.buffer_capacity_samples = capacity_samples;
    g_audio_stats.device_sample_rate = sample_rate;
    
    if (sample_rate > 0) {
        g_audio_stats.buffer_latency_ms =
            (float)fill_samples * 1000.0f / (float)sample_rate;
    }

    if (fill_samples > g_audio_stats.buffer_high_water_samples) {
        g_audio_stats.buffer_high_water_samples = fill_samples;
    }
}

void audio_stats_print(void) {
    printf("[AUDIO] Rate: %.0f Hz (expected: 44100) | "
           "Generated: %u | Queued: %u | Dropped: %u | "
           "Buffer: %u/%u samples (%.1f ms) | Underruns: %u\n",
           g_audio_stats.sample_rate_actual,
           g_audio_stats.last_samples_generated,
           g_audio_stats.last_samples_queued,
           g_audio_stats.last_samples_dropped,
           g_audio_stats.buffer_fill_samples,
           g_audio_stats.buffer_capacity_samples,
           g_audio_stats.buffer_latency_ms,
           g_audio_stats.last_buffer_underruns);
    
    if (g_audio_stats.last_samples_dropped > 0) {
        float drop_percent = 100.0f * (float)g_audio_stats.last_samples_dropped / 
                            (float)(g_audio_stats.last_samples_generated + 1);
        printf("[AUDIO] WARNING: %.1f%% samples dropped!\n", drop_percent);
    }
    
    if (g_audio_stats.buffer_latency_ms < 10.0f) {
        printf("[AUDIO] WARNING: Low buffer (%.1f ms) - risk of underrun!\n",
               g_audio_stats.buffer_latency_ms);
    }
}

const char* audio_stats_get_summary(void) {
    float drop_percent = 0.0f;
    if (g_audio_stats.last_samples_generated > 0) {
        drop_percent = 100.0f * (float)g_audio_stats.last_samples_dropped /
                      (float)g_audio_stats.last_samples_generated;
    }
    
    const char* status = "OK";
    if (g_audio_stats.last_samples_dropped > 0) {
        status = "DROPS!";
    } else if (g_audio_stats.last_buffer_underruns > 0) {
        status = "XRUN";
    } else if (g_audio_stats.buffer_latency_ms < 15.0f) {
        status = "LOW BUF";
    }
    
    snprintf(g_stats_buffer, sizeof(g_stats_buffer),
        "Audio: %.0f Hz | Buf: %.0f ms | Fill: %u/%u | %s\n"
        "Dropped: %llu | Underruns: %llu | Drop Rate: %.2f%%",
        g_audio_stats.sample_rate_actual,
        g_audio_stats.buffer_latency_ms,
        g_audio_stats.buffer_fill_samples,
        g_audio_stats.buffer_capacity_samples,
        status,
        (unsigned long long)g_audio_stats.total_samples_dropped,
        (unsigned long long)g_audio_stats.total_buffer_underruns,
        drop_percent);
    
    return g_stats_buffer;
}
