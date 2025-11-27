#include "player_controller.h"
#include "ui/formatting.h"
#include <iostream>
#include <unistd.h>

namespace timeskip {

PlayerController::PlayerController(std::unique_ptr<IPlayer> player, bool interactive)
    : player_(std::move(player)), 
      interactive_(interactive),
      running_(true) {
}

PlayerController::~PlayerController() {
    if (interactive_) {
        terminal_.disable_raw_mode();
    }
}

int PlayerController::run() {
    // Enable raw mode if interactive
    if (interactive_) {
        terminal_.enable_raw_mode();
    }

    // Main playback loop
    while (running_ && !player_->is_finished()) {
        if (interactive_) {
            handle_keyboard_input();
            update_display();
        }

        // Step through playback
        if (!player_->is_paused()) {
            player_->step();
        }

        // Small sleep to prevent busy-waiting
        usleep(10000); // 10ms
    }

    if (interactive_) {
        terminal_.disable_raw_mode();
        std::cout << "\n";
    }

    print_final_stats();
    return 0;
}

void PlayerController::handle_keyboard_input() {
    if (!terminal_.kbhit()) return;

    char key = terminal_.read_key();
    
    // Handle arrow keys (ANSI escape sequences)
    if (key == 27) { // ESC
        if (terminal_.kbhit() && terminal_.read_key() == '[') {
            if (terminal_.kbhit()) {
                char arrow = terminal_.read_key();
                switch (arrow) {
                    case 'A': // Up arrow - speed up
                        player_->set_speed(player_->get_stats().current_speed + 0.5);
                        break;
                    case 'B': // Down arrow - slow down
                        player_->set_speed(player_->get_stats().current_speed - 0.5);
                        break;
                    case 'C': // Right arrow - seek forward
                        player_->seek(10);
                        break;
                    case 'D': // Left arrow - seek backward
                        player_->seek(-10);
                        break;
                }
            }
        }
    } else {
        switch (key) {
            case 'p':
            case 'P':
            case ' ': // Space
                if (player_->is_paused()) {
                    player_->resume();
                } else {
                    player_->pause();
                }
                break;
            case 'n':
            case 'N':
                player_->skip_next();
                break;
            case 'q':
            case 'Q':
                running_ = false;
                break;
        }
    }
}

void PlayerController::update_display() {
    auto stats = player_->get_stats();
    
    ui::PlaybackStats ui_stats;
    ui_stats.paused = player_->is_paused();
    ui_stats.current_message = stats.current_message;
    ui_stats.total_messages = stats.total_messages;
    ui_stats.position_ns = stats.position_ns;
    ui_stats.duration_ns = stats.duration_ns;
    ui_stats.speed = stats.current_speed;

    int term_width = terminal_.get_width();
    std::string status_line = ui::format_playback_status(ui_stats, term_width);

    terminal_.print_status(status_line.c_str());
}

void PlayerController::print_final_stats() {
    auto stats = player_->get_stats();

    std::cout << "\n📊 Playback Statistics:\n";
    std::cout << "  Messages published: " << stats.messages_published << "/"
              << stats.total_messages << "\n";
    std::cout << "  Duration: " << ui::format_duration(stats.duration_ns) << "\n";
}

} // namespace timeskip
