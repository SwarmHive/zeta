#include "recorder_adapter.h"

namespace timeskip {
namespace zet {

ZetRecorderAdapter::ZetRecorderAdapter(const char* nats_url, const char* topic,
                                       const char* output_file, size_t buffer_size) {
    recorder_ = timeskip_recorder_zet_create(nats_url, topic, output_file, buffer_size);
}

ZetRecorderAdapter::~ZetRecorderAdapter() {
    if (recorder_) {
        timeskip_recorder_zet_destroy(recorder_);
    }
}

int ZetRecorderAdapter::start() {
    if (!recorder_) return -1;
    return timeskip_recorder_zet_start(recorder_);
}

void ZetRecorderAdapter::pause() {
    if (recorder_) {
        timeskip_recorder_zet_pause(recorder_);
    }
}

void ZetRecorderAdapter::resume() {
    if (recorder_) {
        timeskip_recorder_zet_resume(recorder_);
    }
}

bool ZetRecorderAdapter::is_paused() const {
    if (!recorder_) return false;
    return timeskip_recorder_zet_is_paused(recorder_);
}

void ZetRecorderAdapter::stop() {
    if (recorder_) {
        timeskip_recorder_zet_stop(recorder_);
    }
}

RecorderStats ZetRecorderAdapter::get_stats() const {
    RecorderStats stats = {};
    if (recorder_) {
        timeskip_recorder_zet_stats_t zet_stats;
        timeskip_recorder_zet_get_stats(recorder_, &zet_stats);
        
        stats.messages_received = zet_stats.messages_received;
        stats.messages_written = zet_stats.messages_written;
        stats.messages_dropped = zet_stats.messages_dropped;
        stats.bytes_written = zet_stats.bytes_written;
        stats.buffer_overflow = zet_stats.buffer_overflow;
    }
    return stats;
}

} // namespace zet
} // namespace timeskip
