#include "player_adapter.h"
#include <algorithm>

namespace timeskip {
namespace mcap {

McapPlayerAdapter::McapPlayerAdapter(const char* nats_url, const char* input_file, double speed)
    : player_(nullptr), speed_(speed), paused_(false), finished_(false) {
    player_ = timeskip_player_mcap_create(nats_url, input_file, speed);
}

McapPlayerAdapter::~McapPlayerAdapter() {
    if (player_) {
        timeskip_player_mcap_destroy(player_);
    }
}

void McapPlayerAdapter::set_speed(double speed) {
    // Clamp speed between 0.5x and 10x
    speed_ = std::max(0.0, std::min(10.0, speed));
    // Note: MCAP backend doesn't support runtime speed change yet
    // This would need to be added to the C backend
}

void McapPlayerAdapter::pause() {
    paused_ = true;
}

void McapPlayerAdapter::resume() {
    paused_ = false;
}

bool McapPlayerAdapter::is_paused() const {
    return paused_;
}

void McapPlayerAdapter::skip_next() {
    // Skip to next message by stepping once
    if (!paused_ && player_) {
        step();
    }
}

void McapPlayerAdapter::seek(int64_t delta_messages) {
    // Note: MCAP backend doesn't support seeking yet
    // This would need to be added to the C backend
    // For now, this is a no-op
}

bool McapPlayerAdapter::is_finished() const {
    return finished_;
}

void McapPlayerAdapter::step() {
    if (!player_ || finished_) return;
    
    // The original MCAP player runs in blocking mode
    // For now, we mark as finished after start
    // This needs refactoring in the C backend to support step-by-step playback
    if (timeskip_player_mcap_start(player_) != 0) {
        finished_ = true;
    } else {
        finished_ = true;
    }
}

PlayerStats McapPlayerAdapter::get_stats() const {
    PlayerStats stats = {};
    if (player_) {
        timeskip_player_mcap_stats_t mcap_stats;
        timeskip_player_mcap_get_stats(player_, &mcap_stats);
        
        stats.total_messages = mcap_stats.total_messages;
        stats.current_message = mcap_stats.current_message;
        stats.messages_published = mcap_stats.messages_published;
        stats.current_speed = speed_;
        stats.duration_ns = mcap_stats.duration_ns;
        stats.position_ns = mcap_stats.position_ns;
    }
    return stats;
}

} // namespace mcap
} // namespace timeskip
