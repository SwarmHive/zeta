#ifndef TIMESKIP_MCAP_RECORDER_ADAPTER_H
#define TIMESKIP_MCAP_RECORDER_ADAPTER_H

#include "../backend_interface.h"

extern "C" {
#include "recorder_mcap.h"
}

namespace timeskip {
namespace mcap {

class McapRecorderAdapter : public IRecorder {
public:
    McapRecorderAdapter(const char* nats_url, const char* topic, 
                        const char* output_file, size_t buffer_size);
    ~McapRecorderAdapter() override;

    int start() override;
    void pause() override;
    void resume() override;
    bool is_paused() const override;
    void stop() override;
    RecorderStats get_stats() const override;

private:
    timeskip_recorder_mcap_t* recorder_;
};

} // namespace mcap
} // namespace timeskip

#endif // TIMESKIP_MCAP_RECORDER_ADAPTER_H
