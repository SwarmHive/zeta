#include "player_adapter.h"
#include <algorithm>

namespace timeskip {
namespace zet {

ZetPlayerAdapter::ZetPlayerAdapter(const char* nats_url, const char* input_file, double speed)
    : player_(nullptr), speed_(speed), paused_(false), finished_(false) {
    player_ = timeskip_player_zet_create(nats_url, input_file, speed);
}

ZetPlayerAdapter::~ZetPlayerAdapter() {
    if (player_) {
        timeskip_player_zet_destroy(player_);
    }
}

void ZetPlayerAdapter::set_speed(double speed) {
    // Clamp speed between 0.5x and 10x
    speed_ = std::max(0.0, std::min(10.0, speed));
    // Note: ZET backend doesn't support runtime speed change
    // This would need to be added to the C backend
}

void ZetPlayerAdapter::pause() {
    paused_ = true;
}

void ZetPlayerAdapter::resume() {
    paused_ = false;
}

bool ZetPlayerAdapter::is_paused() const {
    return paused_;
}

void ZetPlayerAdapter::skip_next() {
    // Skip to next message by stepping once
    if (!paused_ && player_) {
        step();
    }
}

void ZetPlayerAdapter::seek(int64_t delta_messages) {
    // Note: ZET backend doesn't support seeking
    // This would need to be added to the C backend
    // For now, this is a no-op
}

bool ZetPlayerAdapter::is_finished() const {
    return finished_;
}

void ZetPlayerAdapter::step() {
    if (!player_ || finished_) return;
    
    // The original ZET player runs in blocking mode
    // For now, we mark as finished after start
    // This needs refactoring in the C backend to support step-by-step playback
    if (timeskip_player_zet_start(player_) != 0) {
        finished_ = true;
    } else {
        finished_ = true;
    }
}

PlayerStats ZetPlayerAdapter::get_stats() const {
    PlayerStats stats = {};
    if (player_) {
        timeskip_player_zet_stats_t zet_stats;
        timeskip_player_zet_get_stats(player_, &zet_stats);
        
        stats.total_messages = zet_stats.total_messages;
        stats.current_message = zet_stats.current_message;
        stats.messages_published = zet_stats.messages_published;
        stats.current_speed = speed_;
        stats.duration_ns = zet_stats.duration_ns;
        stats.position_ns = zet_stats.position_ns;
    }
    return stats;
}

} // namespace zet
} // namespace timeskip
