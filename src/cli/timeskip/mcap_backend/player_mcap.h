#ifndef TIMESKIP_PLAYER_MCAP_H
#define TIMESKIP_PLAYER_MCAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct timeskip_player_mcap_s timeskip_player_mcap_t;

// Create MCAP player
// nats_url: NATS server URL (e.g., "nats://localhost:4222")
// input_file: Path to .mcap file to play
// speed: Playback speed multiplier (1.0 = real-time, 2.0 = 2x, 0 = max speed)
timeskip_player_mcap_t* timeskip_player_mcap_create(const char* nats_url,
                                                     const char* input_file,
                                                     double speed);

// Start playback in interactive mode (blocking, handles keyboard input)
int timeskip_player_mcap_start_interactive(timeskip_player_mcap_t* player);

// Start playback in non-interactive mode (blocking)
int timeskip_player_mcap_start(timeskip_player_mcap_t* player);

// Destroy player
void timeskip_player_mcap_destroy(timeskip_player_mcap_t* player);

// Get playback statistics
typedef struct {
    uint64_t total_messages;
    uint64_t current_message;
    uint64_t messages_published;
    double current_speed;
    uint64_t duration_ns;
    uint64_t position_ns;
} timeskip_player_mcap_stats_t;

void timeskip_player_mcap_get_stats(timeskip_player_mcap_t* player, timeskip_player_mcap_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // TIMESKIP_PLAYER_MCAP_H
