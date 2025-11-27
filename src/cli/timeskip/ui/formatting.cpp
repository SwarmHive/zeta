#include "formatting.h"
#include <cstdio>
#include <sstream>

namespace timeskip {
namespace ui {

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

std::string format_duration(uint64_t duration_ns) {
    double seconds = duration_ns / 1e9;
    if (seconds < 60) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fs", seconds);
        return std::string(buf);
    } else if (seconds < 3600) {
        int mins = static_cast<int>(seconds / 60);
        double secs = seconds - (mins * 60);
        char buf[32];
        snprintf(buf, sizeof(buf), "%dm%.1fs", mins, secs);
        return std::string(buf);
    } else {
        int hours = static_cast<int>(seconds / 3600);
        int mins = static_cast<int>((seconds - hours * 3600) / 60);
        char buf[32];
        snprintf(buf, sizeof(buf), "%dh%dm", hours, mins);
        return std::string(buf);
    }
}

std::string format_recording_status(const RecordingStats& stats, int max_width) {
    const char* status = stats.paused ? "⏸️  PAUSED  " : "🔴 REC";

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
    if (static_cast<int>(result.length()) > max_width - 5) {
        snprintf(full, sizeof(full), "%s | %lu/%lu | %s",
                 status,
                 (unsigned long)stats.messages_written,
                 (unsigned long)stats.messages_received,
                 format_bytes(stats.bytes_written).c_str());
        result = full;
    }

    // If still too long, just show essentials
    if (static_cast<int>(result.length()) > max_width - 5) {
        snprintf(full, sizeof(full), "%s | %lu | %s",
                 status,
                 (unsigned long)stats.messages_written,
                 format_bytes(stats.bytes_written).c_str());
        result = full;
    }

    return result;
}

std::string format_playback_status(const PlaybackStats& stats, int max_width) {
    const char* status_icon = stats.paused ? "⏸️ " : "▶️ ";
    
    // Calculate progress bar
    double progress = stats.total_messages > 0 
        ? static_cast<double>(stats.current_message) / stats.total_messages 
        : 0.0;
    
    int bar_width = max_width / 3;
    if (bar_width < 10) bar_width = 10;
    if (bar_width > 40) bar_width = 40;
    
    int filled = static_cast<int>(progress * bar_width);
    
    std::ostringstream oss;
    oss << status_icon << "PLAYBACK [";
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            oss << "=";
        } else if (i == filled) {
            oss << "|";
        } else {
            oss << " ";
        }
    }
    oss << "] ";
    
    // Time and speed info
    oss << format_duration(stats.position_ns) << "/"
        << format_duration(stats.duration_ns) << " "
        << stats.speed << "x "
        << stats.current_message << "/" << stats.total_messages;
    
    if (stats.paused) {
        oss << " (paused)";
    }
    
    return oss.str();
}

} // namespace ui
} // namespace timeskip
