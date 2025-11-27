#ifndef TIMESKIP_PLAYER_ZET_H
#define TIMESKIP_PLAYER_ZET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct timeskip_player_zet_s timeskip_player_zet_t;

// Create ZET player
// nats_url: NATS server URL (e.g., "nats://localhost:4222")
// input_file: Path to .zet file to play
// speed: Playback speed multiplier (1.0 = real-time, 2.0 = 2x, 0 = max speed)
timeskip_player_zet_t* timeskip_player_zet_create(const char* nats_url,
                                                   const char* input_file,
                                                   double speed);

// Start playback in interactive mode (blocking, handles keyboard input)
int timeskip_player_zet_start_interactive(timeskip_player_zet_t* player);

// Start playback in non-interactive mode (blocking)
int timeskip_player_zet_start(timeskip_player_zet_t* player);

// Destroy player
void timeskip_player_zet_destroy(timeskip_player_zet_t* player);

// Get playback statistics
typedef struct {
    uint64_t total_messages;
    uint64_t current_message;
    uint64_t messages_published;
    double current_speed;
    uint64_t duration_ns;
    uint64_t position_ns;
} timeskip_player_zet_stats_t;

void timeskip_player_zet_get_stats(timeskip_player_zet_t* player, timeskip_player_zet_stats_t* stats);

#endif // TIMESKIP_PLAYER_ZET_H
