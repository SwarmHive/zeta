#include "recorder_adapter.h"

namespace timeskip {
namespace mcap {

McapRecorderAdapter::McapRecorderAdapter(const char* nats_url, const char* topic,
                                         const char* output_file, size_t buffer_size) {
    recorder_ = timeskip_recorder_mcap_create(nats_url, topic, output_file, buffer_size);
}

McapRecorderAdapter::~McapRecorderAdapter() {
    if (recorder_) {
        timeskip_recorder_mcap_destroy(recorder_);
    }
}

int McapRecorderAdapter::start() {
    if (!recorder_) return -1;
    return timeskip_recorder_mcap_start(recorder_);
}

void McapRecorderAdapter::pause() {
    if (recorder_) {
        timeskip_recorder_mcap_pause(recorder_);
    }
}

void McapRecorderAdapter::resume() {
    if (recorder_) {
        timeskip_recorder_mcap_resume(recorder_);
    }
}

bool McapRecorderAdapter::is_paused() const {
    if (!recorder_) return false;
    return timeskip_recorder_mcap_is_paused(recorder_);
}

void McapRecorderAdapter::stop() {
    if (recorder_) {
        timeskip_recorder_mcap_stop(recorder_);
    }
}

RecorderStats McapRecorderAdapter::get_stats() const {
    RecorderStats stats = {};
    if (recorder_) {
        timeskip_recorder_mcap_stats_t mcap_stats;
        timeskip_recorder_mcap_get_stats(recorder_, &mcap_stats);
        
        stats.messages_received = mcap_stats.messages_received;
        stats.messages_written = mcap_stats.messages_written;
        stats.messages_dropped = mcap_stats.messages_dropped;
        stats.bytes_written = mcap_stats.bytes_written;
        stats.buffer_overflow = mcap_stats.buffer_overflow;
    }
    return stats;
}

} // namespace mcap
} // namespace timeskip
