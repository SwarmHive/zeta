#ifndef TIMESKIP_UI_FORMATTING_H
#define TIMESKIP_UI_FORMATTING_H

#include <cstdint>
#include <string>

namespace timeskip {
namespace ui {

// Format bytes with units (B, KB, MB, GB)
std::string format_bytes(uint64_t bytes);

// Format duration in nanoseconds to human-readable string
std::string format_duration(uint64_t duration_ns);

// Format recording status line
struct RecordingStats {
    bool paused;
    uint64_t messages_received;
    uint64_t messages_written;
    uint64_t messages_dropped;
    uint64_t bytes_written;
};

std::string format_recording_status(const RecordingStats& stats, int max_width);

// Format playback status line
struct PlaybackStats {
    bool paused;
    uint64_t current_message;
    uint64_t total_messages;
    uint64_t position_ns;
    uint64_t duration_ns;
    double speed;
};

std::string format_playback_status(const PlaybackStats& stats, int max_width);

} // namespace ui
} // namespace timeskip

#endif // TIMESKIP_UI_FORMATTING_H
