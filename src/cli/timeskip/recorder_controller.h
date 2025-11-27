#ifndef TIMESKIP_RECORDER_CONTROLLER_H
#define TIMESKIP_RECORDER_CONTROLLER_H

#include "backend_interface.h"
#include "ui/terminal.h"
#include <memory>
#include <csignal>

namespace timeskip {

class RecorderController {
public:
    RecorderController(std::unique_ptr<IRecorder> recorder, bool interactive);
    ~RecorderController();

    // Run the recording session (blocking)
    int run();

    // Stop recording (called by signal handler)
    void stop();

private:
    void handle_keyboard_input();
    void update_display();
    void print_final_stats();

    std::unique_ptr<IRecorder> recorder_;
    bool interactive_;
    ui::Terminal terminal_;
    bool running_;
    bool stopped_;
};

} // namespace timeskip

#endif // TIMESKIP_RECORDER_CONTROLLER_H
