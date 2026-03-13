#ifndef TIMESKIP_PLAYER_CONTROLLER_H
#define TIMESKIP_PLAYER_CONTROLLER_H

#include "backend_interface.h"
#include "ui/terminal.h"
#include <memory>

namespace timeskip {

class PlayerController {
public:
    PlayerController(std::unique_ptr<IPlayer> player, bool interactive);
    ~PlayerController();

    // Run the playback session (blocking)
    int run();

private:
    void handle_keyboard_input();
    void update_display();
    void print_final_stats();

    std::unique_ptr<IPlayer> player_;
    bool interactive_;
    ui::Terminal terminal_;
    bool running_;
};

} // namespace timeskip

#endif // TIMESKIP_PLAYER_CONTROLLER_H
