#ifndef TIMESKIP_MCAP_PLAYER_ADAPTER_H
#define TIMESKIP_MCAP_PLAYER_ADAPTER_H

#include "../backend_interface.h"

extern "C" {
#include "player_mcap.h"
}

namespace timeskip {
namespace mcap {

class McapPlayerAdapter : public IPlayer {
public:
    McapPlayerAdapter(const char* nats_url, const char* input_file, double speed);
    ~McapPlayerAdapter() override;

    void set_speed(double speed) override;
    void pause() override;
    void resume() override;
    bool is_paused() const override;
    void skip_next() override;
    void seek(int64_t delta_messages) override;
    bool is_finished() const override;
    void step() override;
    PlayerStats get_stats() const override;

private:
    timeskip_player_mcap_t* player_;
    double speed_;
    bool paused_;
    bool finished_;
};

} // namespace mcap
} // namespace timeskip

#endif // TIMESKIP_MCAP_PLAYER_ADAPTER_H
