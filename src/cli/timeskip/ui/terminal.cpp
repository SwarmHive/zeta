#include "terminal.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace timeskip {
namespace ui {

Terminal::Terminal() : raw_mode_enabled_(false), termios_orig_(nullptr) {
    termios_orig_ = new struct termios();
}

Terminal::~Terminal() {
    disable_raw_mode();
    delete static_cast<struct termios*>(termios_orig_);
}

void Terminal::enable_raw_mode() {
    if (raw_mode_enabled_) return;

    struct termios* orig = static_cast<struct termios*>(termios_orig_);
    tcgetattr(STDIN_FILENO, orig);

    struct termios raw = *orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Set non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    raw_mode_enabled_ = true;
}

void Terminal::disable_raw_mode() {
    if (!raw_mode_enabled_) return;

    struct termios* orig = static_cast<struct termios*>(termios_orig_);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
    raw_mode_enabled_ = false;
}

bool Terminal::kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

char Terminal::read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return -1;
}

int Terminal::get_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default fallback
}

void Terminal::clear_line() {
    std::cout << "\r\033[K" << std::flush;
}

void Terminal::print_status(const char* status) {
    std::cout << "\r\033[K" << status << std::flush;
}

} // namespace ui
} // namespace timeskip
