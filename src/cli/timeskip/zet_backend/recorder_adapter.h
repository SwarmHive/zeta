#ifndef TIMESKIP_ZET_RECORDER_ADAPTER_H
#define TIMESKIP_ZET_RECORDER_ADAPTER_H

#include "../backend_interface.h"

extern "C" {
#include "recorder_zet.h"
}

namespace timeskip {
namespace zet {

class ZetRecorderAdapter : public IRecorder {
public:
    ZetRecorderAdapter(const char* nats_url, const char* topic, 
                       const char* output_file, size_t buffer_size);
    ~ZetRecorderAdapter() override;

    int start() override;
    void pause() override;
    void resume() override;
    bool is_paused() const override;
    void stop() override;
    RecorderStats get_stats() const override;

private:
    timeskip_recorder_zet_t* recorder_;
};

} // namespace zet
} // namespace timeskip

#endif // TIMESKIP_ZET_RECORDER_ADAPTER_H
