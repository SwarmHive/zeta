#ifndef TIMESKIP_RECORDER_MCAP_H
#define TIMESKIP_RECORDER_MCAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct timeskip_recorder_mcap_s timeskip_recorder_mcap_t;

// Create MCAP recorder
// nats_url: NATS server URL (e.g., "nats://localhost:4222")
// topic: Subject to record (supports wildcards like "sensor.*")
// output_file: Path to output .mcap file
// buffer_size: Size of circular buffer in number of messages (0 = default 10000)
timeskip_recorder_mcap_t* timeskip_recorder_mcap_create(const char* nats_url,
                                                         const char* topic,
                                                         const char* output_file,
                                                         size_t buffer_size);

// Start recording (spawns writer thread)
int timeskip_recorder_mcap_start(timeskip_recorder_mcap_t* recorder);

// Pause recording (stops writing, keeps receiving)
void timeskip_recorder_mcap_pause(timeskip_recorder_mcap_t* recorder);

// Resume recording
void timeskip_recorder_mcap_resume(timeskip_recorder_mcap_t* recorder);

// Check if paused
bool timeskip_recorder_mcap_is_paused(timeskip_recorder_mcap_t* recorder);

// Stop recording (waits for writer thread to flush buffer)
void timeskip_recorder_mcap_stop(timeskip_recorder_mcap_t* recorder);

// Destroy recorder (must call stop first)
void timeskip_recorder_mcap_destroy(timeskip_recorder_mcap_t* recorder);

// Get statistics
typedef struct {
    uint64_t messages_received;
    uint64_t messages_written;
    uint64_t messages_dropped;
    uint64_t bytes_written;
    bool buffer_overflow;
} timeskip_recorder_mcap_stats_t;

void timeskip_recorder_mcap_get_stats(timeskip_recorder_mcap_t* recorder, timeskip_recorder_mcap_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // TIMESKIP_RECORDER_MCAP_H