#ifndef TIMESKIP_BACKEND_INTERFACE_H
#define TIMESKIP_BACKEND_INTERFACE_H

#include <cstdint>

namespace timeskip {

// Statistics for recording
struct RecorderStats {
    uint64_t messages_received;
    uint64_t messages_written;
    uint64_t messages_dropped;
    uint64_t bytes_written;
    bool buffer_overflow;
};

// Statistics for playback
struct PlayerStats {
    uint64_t total_messages;
    uint64_t current_message;
    uint64_t messages_published;
    double current_speed;
    uint64_t duration_ns;
    uint64_t position_ns;
};

// Abstract interface for recording backends
class IRecorder {
public:
    virtual ~IRecorder() = default;

    // Start recording (spawns writer thread)
    virtual int start() = 0;

    // Pause recording (stops writing, keeps receiving)
    virtual void pause() = 0;

    // Resume recording
    virtual void resume() = 0;

    // Check if paused
    virtual bool is_paused() const = 0;

    // Stop recording (waits for writer thread to flush buffer)
    virtual void stop() = 0;

    // Get statistics
    virtual RecorderStats get_stats() const = 0;
};

// Abstract interface for playback backends
class IPlayer {
public:
    virtual ~IPlayer() = default;

    // Set playback speed
    virtual void set_speed(double speed) = 0;

    // Pause playback
    virtual void pause() = 0;

    // Resume playback
    virtual void resume() = 0;

    // Check if paused
    virtual bool is_paused() const = 0;

    // Skip to next message
    virtual void skip_next() = 0;

    // Seek forward/backward by N messages
    virtual void seek(int64_t delta_messages) = 0;

    // Check if playback is finished
    virtual bool is_finished() const = 0;

    // Publish next message (for manual stepping)
    virtual void step() = 0;

    // Get statistics
    virtual PlayerStats get_stats() const = 0;
};

} // namespace timeskip

#endif // TIMESKIP_BACKEND_INTERFACE_H
