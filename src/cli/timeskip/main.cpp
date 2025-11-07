#include <iostream>
#include <cstdlib>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "CLI11.hpp"
#include <time.h>
#include <chrono>

extern "C" {
#include "recorder.h"
#include "player.h"
}

// Global recorder for signal handling
static timeskip_recorder_t* g_recorder = nullptr;

// Terminal handling
static struct termios orig_termios;
static bool raw_mode_enabled = false;

void disable_raw_mode() {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = false;
    }
}

void enable_raw_mode() {
    if (raw_mode_enabled) return;
    
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    
    // Set non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    raw_mode_enabled = true;
}

int kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

char read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return -1;
}

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }
    
    char buf[64];
    if (unit == 0) {
        snprintf(buf, sizeof(buf), "%.0f %s", size, units[unit]);
    } else {
        snprintf(buf, sizeof(buf), "%.2f %s", size, units[unit]);
    }
    return std::string(buf);
}

int get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default fallback
}

std::string format_recording_status(bool paused, const timeskip_stats_t& stats, int max_width) {
    const char* status = paused ? "⏸️  PAUSED  " : "🔴 REC";
    
    // Build full string first
    char full[256];
    snprintf(full, sizeof(full), "%s | Rcv: %lu | Wr: %lu | Drop: %lu | Size: %s",
             status,
             (unsigned long)stats.messages_received,
             (unsigned long)stats.messages_written,
             (unsigned long)stats.messages_dropped,
             format_bytes(stats.bytes_written).c_str());
    
    std::string result = full;
    
    // If too long, use abbreviated version
    if ((int)result.length() > max_width - 5) {
        snprintf(full, sizeof(full), "%s | %lu/%lu | %s",
                 status,
                 (unsigned long)stats.messages_written,
                 (unsigned long)stats.messages_received,
                 format_bytes(stats.bytes_written).c_str());
        result = full;
    }
    
    // If still too long, just show essentials
    if ((int)result.length() > max_width - 5) {
        snprintf(full, sizeof(full), "%s | %lu | %s",
                 status,
                 (unsigned long)stats.messages_written,
                 format_bytes(stats.bytes_written).c_str());
        result = full;
    }
    
    return result;
}

void signal_handler(int signum) {
    if (g_recorder) {
        disable_raw_mode();
        std::cout << "\n📊 Stopping recording...\n";
        timeskip_recorder_stop(g_recorder);
        
        timeskip_stats_t stats;
        timeskip_recorder_get_stats(g_recorder, &stats);
        
        std::cout << "\n📈 Recording Statistics:\n";
        std::cout << "  Messages received: " << stats.messages_received << "\n";
        std::cout << "  Messages written:  " << stats.messages_written << "\n";
        std::cout << "  Messages dropped:  " << stats.messages_dropped << "\n";
        std::cout << "  File size:         " << format_bytes(stats.bytes_written) << "\n";
        if (stats.buffer_overflow) {
            std::cout << "  ⚠️  Buffer overflow occurred!\n";
        }
        
        timeskip_recorder_destroy(g_recorder);
        g_recorder = nullptr;
    }
    exit(0);
}

int main(int argc, char** argv) {
    CLI::App app{"timeskip - Zetabus recording and playback tool"};

    CLI::App* record = app.add_subcommand("record", "Record Zetabus subject(s) into a file");

    // Format current date and time as YYYY_MM_DD_HHMMSS
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm buf;
    localtime_r(&in_time_t, &buf);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "%Y_%m_%d_%H%M%S", &buf);

    std::string subject;
    std::string output_file = std::string("timeskip_") + timestamp + ".zet";
    std::string nats_url;
    
    record->add_option("subject", subject, "Specify the Zetabus subject(s) to record")->required();
    record->add_option("-o,--output", output_file, "Specify the output file");
    record->add_option("-s,--server", nats_url, "NATS server URL (default: env NATS_URL or nats://localhost:4222)");

    CLI::App* play = app.add_subcommand("play", "Play back a recorded Zetabus file");
    std::string playback_file;
    std::string play_nats_url;
    double speed = 1.0;
    bool interactive = true;
    
    play->add_option("file", playback_file, "The recorded file to play back")->required();
    play->add_option("-s,--server", play_nats_url, "NATS server URL (default: env NATS_URL or nats://localhost:4222)");
    play->add_option("--speed", speed, "Playback speed multiplier (1.0=real-time, 2.0=2x, 0=max)")->default_val(1.0);
    play->add_flag("--no-interactive,!--interactive", interactive, "Disable interactive controls")->default_val(true);

    CLI11_PARSE(app, argc, argv);

    if (record->parsed()) {
        // Determine NATS server URL
        std::string server_url;
        if (!nats_url.empty()) {
            server_url = nats_url;
        } else {
            const char* env_url = std::getenv("NATS_URL");
            if (env_url) {
                server_url = env_url;
            } else {
                server_url = "nats://localhost:4222";
            }
        }
        
        std::cout << "🔴 Recording Zetabus subject: " << subject << "\n";
        std::cout << "📁 Output file: " << output_file << "\n";
        std::cout << "🌐 NATS server: " << server_url << "\n";
        std::cout << "\n🎮 Controls:\n";
        std::cout << "  p      : Pause/Resume recording\n";
        std::cout << "  Ctrl+C : Stop and save\n\n";
        
        // Create recorder
        g_recorder = timeskip_recorder_create(server_url.c_str(), subject.c_str(), 
                                              output_file.c_str(), 0);
        if (!g_recorder) {
            std::cerr << "❌ Failed to create recorder\n";
            return 1;
        }
        
        // Set up signal handler
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Enable raw mode for keyboard input
        enable_raw_mode();
        
        // Start recording
        if (timeskip_recorder_start(g_recorder) != 0) {
            disable_raw_mode();
            std::cerr << "❌ Failed to start recording\n";
            timeskip_recorder_destroy(g_recorder);
            return 1;
        }
        
        // Display stats periodically and handle keyboard
        bool paused = false;
        while (true) {
            // Check for keyboard input
            if (kbhit()) {
                char key = read_key();
                if (key == 'p' || key == 'P') {
                    paused = !paused;
                    if (paused) {
                        timeskip_recorder_pause(g_recorder);
                    } else {
                        timeskip_recorder_resume(g_recorder);
                    }
                }
            }
            
            sleep(1);
            timeskip_stats_t stats;
            timeskip_recorder_get_stats(g_recorder, &stats);
            
            int term_width = get_terminal_width();
            std::string status_line = format_recording_status(paused, stats, term_width);
            
            std::cout << "\r\033[K" << status_line << std::flush;
        }
        
    } else if (play->parsed()) {
        // Determine NATS server URL
        std::string server_url;
        if (!play_nats_url.empty()) {
            server_url = play_nats_url;
        } else {
            const char* env_url = std::getenv("NATS_URL");
            if (env_url) {
                server_url = env_url;
            } else {
                server_url = "nats://localhost:4222";
            }
        }
        
        std::cout << "▶️  Playing back recorded Zetabus file\n";
        std::cout << "📁 Input file: " << playback_file << "\n";
        std::cout << "🌐 NATS server: " << server_url << "\n";
        std::cout << "⚡ Speed: " << (speed == 0 ? "MAX" : std::to_string(speed)) << "x\n";
        if (interactive) {
            std::cout << "🎮 Interactive mode: enabled\n";
        }
        std::cout << "\n";
        
        // Create player
        timeskip_player_t* player = timeskip_player_create(server_url.c_str(), 
                                                           playback_file.c_str(), 
                                                           speed);
        if (!player) {
            std::cerr << "❌ Failed to create player (file not found or invalid format)\n";
            return 1;
        }
        
        // Start playback
        int result;
        if (interactive) {
            result = timeskip_player_start_interactive(player);
        } else {
            result = timeskip_player_start(player);
        }
        
        if (result != 0) {
            std::cerr << "❌ Playback failed\n";
            timeskip_player_destroy(player);
            return 1;
        }
        
        // Get final stats
        timeskip_player_stats_t stats;
        timeskip_player_get_stats(player, &stats);
        
        std::cout << "\n📊 Playback Statistics:\n";
        std::cout << "  Messages published: " << stats.messages_published << "/" << stats.total_messages << "\n";
        std::cout << "  Duration: " << (stats.duration_ns / 1e9) << "s\n";
        
        timeskip_player_destroy(player);
    }

    return 0;
}
