#include "player.h"
#include "zet_format.h"
#include "../../bus/c/bus.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdatomic.h>
#include <sys/ioctl.h>

// Get monotonic time in nanoseconds
static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Message buffer for playback
typedef struct {
    uint64_t sent_ns;
    uint64_t received_ns;
    char* topic;
    void* data;
    size_t size;
} playback_message_t;

// Player context
struct timeskip_player_s {
    zetabus_t* bus;
    zetabus_publisher_t** publishers; // One publisher per topic
    char** topics;
    size_t topic_count;
    
    playback_message_t* messages;
    size_t message_count;
    size_t current_index;
    
    uint64_t start_time_ns;
    uint64_t duration_ns;
    double speed;
    
    atomic_uint_fast64_t messages_published;
    
    char* input_file;
};

// Get terminal width
static int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default fallback
}

// Display progress bar
static void display_progress_bar(timeskip_player_t* player, 
                                  uint64_t recording_start,
                                  playback_message_t* msg,
                                  bool paused) {
    int term_width = get_terminal_width();
    
    // Calculate progress
    double progress = (double)player->current_index / player->message_count;
    uint64_t position = msg->received_ns - recording_start;
    double position_sec = position / 1e9;
    double duration_sec = player->duration_ns / 1e9;
    
    // Format time strings
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%.1f/%.1fs", position_sec, duration_sec);
    
    // Format speed string
    char speed_str[16];
    if (player->speed == 0) {
        snprintf(speed_str, sizeof(speed_str), "MAX");
    } else {
        snprintf(speed_str, sizeof(speed_str), "%.1fx", player->speed);
    }
    
    // Format message count
    char msg_str[32];
    snprintf(msg_str, sizeof(msg_str), "%zu/%zu", player->current_index + 1, player->message_count);
    
    // Calculate space for progress bar
    // Format: "⏸️  PLAYBACK [===|   ] 12.3/45.6s 2.0x 123/456"
    const char* icon = paused ? "⏸️ " : "▶️ ";
    int fixed_width = strlen(icon) + strlen("PLAYBACK [] ") + 
                      strlen(time_str) + 1 + strlen(speed_str) + 1 + strlen(msg_str);
    int bar_width = term_width - fixed_width - 2; // -2 for safety margin
    
    if (bar_width < 10) bar_width = 10; // Minimum bar width
    if (bar_width > 60) bar_width = 60; // Maximum bar width
    
    // Build progress bar
    int filled = (int)(progress * bar_width);
    if (filled > bar_width) filled = bar_width;
    
    printf("\r\033[K%sPLAYBACK [", icon);
    
    // Draw filled portion
    for (int i = 0; i < filled; i++) {
        printf("=");
    }
    
    // Draw position marker
    if (filled < bar_width) {
        printf("|");
        
        // Draw empty portion
        for (int i = filled + 1; i < bar_width; i++) {
            printf(" ");
        }
    }
    
    printf("] %s %s %s", time_str, speed_str, msg_str);
    fflush(stdout);
}

// Terminal handling for keyboard input
static struct termios orig_termios;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
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
}

// Check for keyboard input
static int kbhit(void) {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

// Read a key (non-blocking)
static int read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        // Handle escape sequences (arrow keys)
        if (c == '\x1b') {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return c;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return c;
            
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': return 'A'; // Up arrow
                    case 'B': return 'B'; // Down arrow
                    case 'C': return 'R'; // Right arrow
                    case 'D': return 'L'; // Left arrow
                }
            }
        }
        return c;
    }
    return -1;
}

// Find or create publisher for topic
static zetabus_publisher_t* get_publisher_for_topic(timeskip_player_t* player, const char* topic) {
    // Check if we already have a publisher for this topic
    for (size_t i = 0; i < player->topic_count; i++) {
        if (strcmp(player->topics[i], topic) == 0) {
            return player->publishers[i];
        }
    }
    
    // Create new publisher
    zetabus_publisher_t* pub = zetabus_publisher_create(player->bus, topic);
    if (!pub) return NULL;
    
    // Resize arrays
    player->topic_count++;
    player->topics = realloc(player->topics, player->topic_count * sizeof(char*));
    player->publishers = realloc(player->publishers, player->topic_count * sizeof(zetabus_publisher_t*));
    
    player->topics[player->topic_count - 1] = strdup(topic);
    player->publishers[player->topic_count - 1] = pub;
    
    return pub;
}

// Load all messages from file
static int load_messages(timeskip_player_t* player) {
    zet_reader_t* reader = zet_reader_create(player->input_file);
    if (!reader) return -1;
    
    player->start_time_ns = zet_reader_get_start_time(reader);
    
    // Count messages first
    size_t count = 0;
    zet_message_t msg;
    while (zet_reader_read_message(reader, &msg) == 0) {
        count++;
        zet_message_free(&msg);
    }
    
    // Allocate message buffer
    player->messages = calloc(count, sizeof(playback_message_t));
    if (!player->messages) {
        zet_reader_destroy(reader);
        return -1;
    }
    
    // Reload messages
    zet_reader_destroy(reader);
    reader = zet_reader_create(player->input_file);
    if (!reader) {
        free(player->messages);
        return -1;
    }
    
    size_t idx = 0;
    uint64_t first_timestamp = 0;
    uint64_t last_timestamp = 0;
    
    while (zet_reader_read_message(reader, &msg) == 0 && idx < count) {
        player->messages[idx].sent_ns = msg.sent_ns;
        player->messages[idx].received_ns = msg.received_ns;
        player->messages[idx].topic = msg.topic;  // Transfer ownership
        player->messages[idx].data = msg.data;    // Transfer ownership
        player->messages[idx].size = msg.size;
        
        if (idx == 0) first_timestamp = msg.received_ns;
        last_timestamp = msg.received_ns;
        
        idx++;
    }
    
    player->message_count = idx;
    player->duration_ns = last_timestamp - first_timestamp;
    
    zet_reader_destroy(reader);
    return 0;
}

// Public API implementation
timeskip_player_t* timeskip_player_create(const char* nats_url,
                                          const char* input_file,
                                          double speed) {
    if (!nats_url || !input_file) return NULL;
    
    timeskip_player_t* player = calloc(1, sizeof(timeskip_player_t));
    if (!player) return NULL;
    
    player->input_file = strdup(input_file);
    player->speed = speed > 0 ? speed : 0; // 0 = max speed
    player->current_index = 0;
    atomic_init(&player->messages_published, 0);
    
    // Connect to NATS
    player->bus = zetabus_create(nats_url);
    if (!player->bus) {
        free(player->input_file);
        free(player);
        return NULL;
    }
    
    // Load messages from file
    if (load_messages(player) != 0) {
        zetabus_destroy(player->bus);
        free(player->input_file);
        free(player);
        return NULL;
    }
    
    return player;
}

int timeskip_player_start(timeskip_player_t* player) {
    if (!player || player->message_count == 0) return -1;
    
    uint64_t playback_start = get_monotonic_ns();
    uint64_t recording_start = player->messages[0].received_ns;
    
    for (size_t i = 0; i < player->message_count; i++) {
        playback_message_t* msg = &player->messages[i];
        
        // Calculate when this message should be sent
        uint64_t msg_offset = msg->received_ns - recording_start;
        
        if (player->speed > 0) {
            uint64_t target_time = playback_start + (uint64_t)(msg_offset / player->speed);
            
            // Wait until target time
            uint64_t now = get_monotonic_ns();
            if (now < target_time) {
                uint64_t wait_ns = target_time - now;
                struct timespec ts;
                ts.tv_sec = wait_ns / 1000000000ULL;
                ts.tv_nsec = wait_ns % 1000000000ULL;
                nanosleep(&ts, NULL);
            }
        }
        
        // Get or create publisher for this topic
        zetabus_publisher_t* pub = get_publisher_for_topic(player, msg->topic);
        if (pub) {
            zetabus_publish(pub, msg->data, msg->size);
            atomic_fetch_add(&player->messages_published, 1);
        }
        
        player->current_index = i;
    }
    
    return 0;
}

int timeskip_player_start_interactive(timeskip_player_t* player) {
    if (!player || player->message_count == 0) return -1;
    
    enable_raw_mode();
    
    printf("\n🎮 Interactive Playback Controls:\n");
    printf("  ← → : Seek backward/forward (10 messages, works while paused)\n");
    printf("  ↑ ↓ : Speed up/down (works while paused)\n");
    printf("  n   : Next message\n");
    printf("  p   : Pause/Resume\n");
    printf("  q   : Quit\n\n");
    
    uint64_t playback_start = get_monotonic_ns();
    uint64_t recording_start = player->messages[0].received_ns;
    uint64_t pause_time = 0;
    bool paused = false;
    bool skip_wait = false; // Flag to skip waiting for next message
    uint64_t last_display_time = 0; // For throttling display updates
    
    player->current_index = 0;
    
    while (player->current_index < player->message_count) {
        playback_message_t* msg = &player->messages[player->current_index];
        
        // Calculate when this message should be sent
        uint64_t msg_offset = msg->received_ns - recording_start;
        
        if (player->speed > 0 && !skip_wait) {
            uint64_t target_time = playback_start + (uint64_t)(msg_offset / player->speed);
            uint64_t now = get_monotonic_ns();
            
            if (now < target_time) {
                // Wait until target time, checking for keyboard input
                uint64_t wait_time = target_time - now;
                uint64_t end_time = now + wait_time;
                
                while (get_monotonic_ns() < end_time && !skip_wait) {
                    // Handle keyboard input during wait
                    if (kbhit()) {
                        int key = read_key();
                        
                        switch (key) {
                            case 'q':
                            case 'Q':
                                printf("\n\n▶️  Playback stopped by user\n");
                                disable_raw_mode();
                                return 0;
                            
                            case 'p':
                            case 'P':
                            case ' ':
                                paused = !paused;
                                if (paused) {
                                    pause_time = get_monotonic_ns();
                                    printf("\r⏸️  Paused  ");
                                } else {
                                    uint64_t pause_duration = get_monotonic_ns() - pause_time;
                                    playback_start += pause_duration;
                                    end_time += pause_duration;
                                    printf("\r▶️  Playing ");
                                }
                                fflush(stdout);
                                break;
                            
                            case 'L': // Left arrow
                                if (player->current_index >= 10) {
                                    player->current_index -= 10;
                                    playback_start = get_monotonic_ns() - 
                                        (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                                    skip_wait = true;
                                }
                                break;
                            
                            case 'R': // Right arrow
                                if (player->current_index + 10 < player->message_count) {
                                    player->current_index += 10;
                                    playback_start = get_monotonic_ns() - 
                                        (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                                    skip_wait = true;
                                }
                                break;
                            
                            case 'A': // Up arrow - speed up
                                if (player->speed == 0) {
                                    player->speed = 1.0;
                                } else if (player->speed < 10.0) {
                                    player->speed += 0.5;
                                }
                                playback_start = get_monotonic_ns() - 
                                    (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                                skip_wait = true;
                                break;
                            
                            case 'B': // Down arrow - slow down
                                if (player->speed > 0.5) {
                                    player->speed -= 0.5;
                                } else if (player->speed > 0) {
                                    player->speed = 0; // Max speed
                                }
                                playback_start = get_monotonic_ns() - 
                                    (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                                skip_wait = true;
                                break;
                            
                            case 'n':
                            case 'N':
                                // Skip to next message immediately
                                skip_wait = true;
                                break;
                        }
                        
                        if (skip_wait) break;
                    }
                    
                    if (paused) {
                        usleep(10000); // 10ms
                        continue;
                    }
                    
                    // Sleep in small chunks to remain responsive
                    usleep(1000); // 1ms
                }
            }
        }
        
        // Reset skip flag
        skip_wait = false;
        
        if (paused) {
            // Display progress bar while paused
            playback_message_t* pause_msg = &player->messages[player->current_index];
            display_progress_bar(player, recording_start, pause_msg, true);
            
            // Handle keyboard input while paused
            if (kbhit()) {
                int key = read_key();
                
                switch (key) {
                    case 'q':
                    case 'Q':
                        printf("\n\n▶️  Playback stopped by user\n");
                        disable_raw_mode();
                        return 0;
                    
                    case 'p':
                    case 'P':
                    case ' ': {
                        paused = false;
                        uint64_t pause_duration = get_monotonic_ns() - pause_time;
                        playback_start += pause_duration;
                        break;
                    }
                    
                    case 'L': // Left arrow - seek backward
                        if (player->current_index >= 10) {
                            player->current_index -= 10;
                        } else {
                            player->current_index = 0;
                        }
                        playback_start = get_monotonic_ns() - 
                            (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                        skip_wait = true;
                        break;
                    
                    case 'R': // Right arrow - seek forward
                        if (player->current_index + 10 < player->message_count) {
                            player->current_index += 10;
                        } else if (player->current_index < player->message_count - 1) {
                            player->current_index = player->message_count - 1;
                        }
                        playback_start = get_monotonic_ns() - 
                            (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                        skip_wait = true;
                        break;
                    
                    case 'A': // Up arrow - speed up
                        if (player->speed == 0) {
                            player->speed = 1.0;
                        } else if (player->speed < 10.0) {
                            player->speed += 0.5;
                        }
                        playback_start = get_monotonic_ns() - 
                            (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                        skip_wait = true;
                        break;
                    
                    case 'B': // Down arrow - slow down
                        if (player->speed > 0.5) {
                            player->speed -= 0.5;
                        } else if (player->speed > 0) {
                            player->speed = 0; // Max speed
                        }
                        playback_start = get_monotonic_ns() - 
                            (uint64_t)((player->messages[player->current_index].received_ns - recording_start) / player->speed);
                        skip_wait = true;
                        break;
                    
                    case 'n':
                    case 'N': {
                        // Skip to next message immediately
                        skip_wait = true;
                        paused = false;
                        uint64_t pause_dur = get_monotonic_ns() - pause_time;
                        playback_start += pause_dur;
                        break;
                    }
                }
            }
            
            if (paused) {
                usleep(10000); // 10ms
                continue;
            }
        }
        
        // Get or create publisher for this topic
        zetabus_publisher_t* pub = get_publisher_for_topic(player, msg->topic);
        if (pub) {
            zetabus_publish(pub, msg->data, msg->size);
            atomic_fetch_add(&player->messages_published, 1);
        }
        
        // Display progress (throttle to avoid spam, update every 100ms or so)
        uint64_t now = get_monotonic_ns();
        if (now - last_display_time > 100000000ULL || player->current_index == 0) { // 100ms
            last_display_time = now;
            display_progress_bar(player, recording_start, msg, paused);
        }
        
        player->current_index++;
    }
    
    printf("\n\n✅ Playback complete!\n");
    disable_raw_mode();
    return 0;
}

void timeskip_player_destroy(timeskip_player_t* player) {
    if (!player) return;
    
    // Destroy publishers
    for (size_t i = 0; i < player->topic_count; i++) {
        zetabus_publisher_destroy(player->publishers[i]);
        free(player->topics[i]);
    }
    free(player->publishers);
    free(player->topics);
    
    // Free messages
    for (size_t i = 0; i < player->message_count; i++) {
        free(player->messages[i].topic);
        free(player->messages[i].data);
    }
    free(player->messages);
    
    // Destroy bus
    if (player->bus) {
        zetabus_destroy(player->bus);
    }
    
    free(player->input_file);
    free(player);
}

void timeskip_player_get_stats(timeskip_player_t* player, timeskip_player_stats_t* stats) {
    if (!player || !stats) return;
    
    stats->total_messages = player->message_count;
    stats->current_message = player->current_index;
    stats->messages_published = atomic_load(&player->messages_published);
    stats->current_speed = player->speed;
    stats->duration_ns = player->duration_ns;
    
    if (player->current_index > 0 && player->current_index < player->message_count) {
        stats->position_ns = player->messages[player->current_index].received_ns - 
                            player->messages[0].received_ns;
    } else {
        stats->position_ns = 0;
    }
}
