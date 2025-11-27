#include "recorder_controller.h"
#include <iostream>
#include <unistd.h>

namespace timeskip {

// Global pointer for signal handling
static RecorderController* g_recorder_controller = nullptr;

void signal_handler(int signum) {
    if (g_recorder_controller) {
        g_recorder_controller->stop();
        exit(0);
    }
}

RecorderController::RecorderController(std::unique_ptr<IRecorder> recorder, bool interactive)
    : recorder_(std::move(recorder)), 
      interactive_(interactive),
      running_(true),
      stopped_(false) {
    g_recorder_controller = this;
}

RecorderController::~RecorderController() {
    if (g_recorder_controller == this) {
        g_recorder_controller = nullptr;
    }
}

int RecorderController::run() {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Enable raw mode if interactive
    if (interactive_) {
        terminal_.enable_raw_mode();
    }

    // Start recording
    if (recorder_->start() != 0) {
        if (interactive_) {
            terminal_.disable_raw_mode();
        }
        std::cerr << "❌ Failed to start recording\n";
        return 1;
    }

    // Main loop
    while (running_) {
        if (interactive_) {
            handle_keyboard_input();
        }

        sleep(1);
        update_display();
    }

    return 0;
}

void RecorderController::stop() {
    if (stopped_) return;
    stopped_ = true;

    if (interactive_) {
        terminal_.disable_raw_mode();
    }

    std::cout << "\n📊 Stopping recording...\n";
    recorder_->stop();
    print_final_stats();
    running_ = false;
}

void RecorderController::handle_keyboard_input() {
    if (terminal_.kbhit()) {
        char key = terminal_.read_key();
        if (key == 'p' || key == 'P') {
            if (recorder_->is_paused()) {
                recorder_->resume();
            } else {
                recorder_->pause();
            }
        }
    }
}

void RecorderController::update_display() {
    auto stats = recorder_->get_stats();
    
    ui::RecordingStats ui_stats;
    ui_stats.paused = recorder_->is_paused();
    ui_stats.messages_received = stats.messages_received;
    ui_stats.messages_written = stats.messages_written;
    ui_stats.messages_dropped = stats.messages_dropped;
    ui_stats.bytes_written = stats.bytes_written;

    int term_width = terminal_.get_width();
    std::string status_line = ui::format_recording_status(ui_stats, term_width);

    terminal_.print_status(status_line.c_str());
}

void RecorderController::print_final_stats() {
    auto stats = recorder_->get_stats();

    std::cout << "\n📈 Recording Statistics:\n";
    std::cout << "  Messages received: " << stats.messages_received << "\n";
    std::cout << "  Messages written:  " << stats.messages_written << "\n";
    std::cout << "  Messages dropped:  " << stats.messages_dropped << "\n";
    std::cout << "  File size:         " << ui::format_bytes(stats.bytes_written) << "\n";
    if (stats.buffer_overflow) {
        std::cout << "  ⚠️  Buffer overflow occurred!\n";
    }
}

} // namespace timeskip
