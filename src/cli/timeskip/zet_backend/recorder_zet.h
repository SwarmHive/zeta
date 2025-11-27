#ifndef TIMESKIP_RECORDER_ZET_H
#define TIMESKIP_RECORDER_ZET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct timeskip_recorder_zet_s timeskip_recorder_zet_t;

// Create ZET recorder
// nats_url: NATS server URL (e.g., "nats://localhost:4222")
// topic: Subject to record (supports wildcards like "sensor.*")
// output_file: Path to output .zet file
// buffer_size: Size of circular buffer in number of messages (0 = default 10000)
timeskip_recorder_zet_t* timeskip_recorder_zet_create(const char* nats_url,
                                                       const char* topic,
                                                       const char* output_file,
                                                       size_t buffer_size);

// Start recording (spawns writer thread)
int timeskip_recorder_zet_start(timeskip_recorder_zet_t* recorder);

// Pause recording (stops writing, keeps receiving)
void timeskip_recorder_zet_pause(timeskip_recorder_zet_t* recorder);

// Resume recording
void timeskip_recorder_zet_resume(timeskip_recorder_zet_t* recorder);

// Check if paused
bool timeskip_recorder_zet_is_paused(timeskip_recorder_zet_t* recorder);

// Stop recording (waits for writer thread to flush buffer)
void timeskip_recorder_zet_stop(timeskip_recorder_zet_t* recorder);

// Destroy recorder (must call stop first)
void timeskip_recorder_zet_destroy(timeskip_recorder_zet_t* recorder);

// Get statistics
typedef struct {
    uint64_t messages_received;
    uint64_t messages_written;
    uint64_t messages_dropped;
    uint64_t bytes_written;
    bool buffer_overflow;
} timeskip_recorder_zet_stats_t;

void timeskip_recorder_zet_get_stats(timeskip_recorder_zet_t* recorder, timeskip_recorder_zet_stats_t* stats);

#endif // TIMESKIP_RECORDER_ZET_H
