#ifndef TIMESKIP_UI_TERMINAL_H
#define TIMESKIP_UI_TERMINAL_H

namespace timeskip {
namespace ui {

// Terminal control utilities
class Terminal {
public:
    Terminal();
    ~Terminal();

    // Enable/disable raw mode for keyboard input
    void enable_raw_mode();
    void disable_raw_mode();
    
    // Check if a key has been pressed (non-blocking)
    bool kbhit();
    
    // Read a single key (returns -1 if no key available)
    char read_key();
    
    // Get terminal width in columns
    int get_width();
    
    // Clear the current line
    void clear_line();
    
    // Print status on current line (with carriage return)
    void print_status(const char* status);

private:
    bool raw_mode_enabled_;
    void* termios_orig_; // Store original termios as opaque pointer
};

} // namespace ui
} // namespace timeskip

#endif // TIMESKIP_UI_TERMINAL_H
