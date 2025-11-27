#include "player_mcap.h"
#include "../../../bus/c/bus.h"
#include <mcap/reader.hpp>
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
#include <fstream>
#include <memory>
#include <vector>

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

// MCAP reader wrapper
struct McapReader {
    std::unique_ptr<std::ifstream> file;
    std::unique_ptr<mcap::McapReader> reader;
    std::vector<mcap::MessageView> messages;
};

// Player context
struct timeskip_player_mcap_s {
    zetabus_t* bus;
    zetabus_publisher_t** publishers;
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
    McapReader* mcap_reader;
};

// Terminal helpers (same as ZET player)
static int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
}

extern "C" {

timeskip_player_mcap_t* timeskip_player_mcap_create(const char* nats_url,
                                                     const char* input_file,
                                                     double speed) {
    if (!nats_url || !input_file) return NULL;
    
    timeskip_player_mcap_t* player = (timeskip_player_mcap_t*)calloc(1, sizeof(timeskip_player_mcap_t));
    if (!player) return NULL;
    
    player->input_file = strdup(input_file);
    player->speed = speed;
    atomic_init(&player->messages_published, 0);
    
    // Create MCAP reader
    player->mcap_reader = new McapReader();
    player->mcap_reader->file = std::make_unique<std::ifstream>(input_file, std::ios::binary);
    if (!player->mcap_reader->file->is_open()) {
        delete player->mcap_reader;
        free(player->input_file);
        free(player);
        return NULL;
    }
    
    player->mcap_reader->reader = std::make_unique<mcap::McapReader>();
    player->mcap_reader->reader->open(*player->mcap_reader->file);
    
    // Get start time
    auto summary = player->mcap_reader->reader->statistics();
    if (summary.has_value()) {
        player->start_time_ns = summary->messageStartTime;
    } else {
        player->start_time_ns = 0;
    }
    
    // Load all messages
    auto messageView = player->mcap_reader->reader->readMessages();
    for (const auto& msg : messageView) {
        player->mcap_reader->messages.push_back(msg);
    }
    
    // Convert to playback_message_t array
    player->message_count = player->mcap_reader->messages.size();
    player->messages = (playback_message_t*)calloc(player->message_count, sizeof(playback_message_t));
    
    for (size_t i = 0; i < player->message_count; i++) {
        const auto& msgView = player->mcap_reader->messages[i];
        const auto& message = msgView.message;
        const auto& channel = msgView.channel;
        
        player->messages[i].sent_ns = message.publishTime;
        player->messages[i].received_ns = message.logTime;
        player->messages[i].topic = strdup(channel->topic.c_str());
        player->messages[i].data = malloc(message.dataSize);
        memcpy(player->messages[i].data, message.data, message.dataSize);
        player->messages[i].size = message.dataSize;
    }
    
    // Calculate duration
    if (player->message_count > 0) {
        player->duration_ns = player->messages[player->message_count - 1].received_ns - 
                             player->messages[0].received_ns;
    }
    
    // Connect to NATS
    player->bus = zetabus_create(nats_url);
    if (!player->bus) {
        for (size_t i = 0; i < player->message_count; i++) {
            free(player->messages[i].topic);
            free(player->messages[i].data);
        }
        free(player->messages);
        delete player->mcap_reader;
        free(player->input_file);
        free(player);
        return NULL;
    }
    
    // Create publishers for each unique topic
    // (simplified - create on first use in playback)
    player->publishers = NULL;
    player->topics = NULL;
    player->topic_count = 0;
    
    return player;
}

int timeskip_player_mcap_start(timeskip_player_mcap_t* player) {
    if (!player) return -1;
    
    printf("Playing %zu messages...\n", player->message_count);
    
    uint64_t playback_start = get_monotonic_ns();
    uint64_t recording_start = player->messages[0].received_ns;
    
    for (size_t i = 0; i < player->message_count; i++) {
        playback_message_t* msg = &player->messages[i];
        
        // Find or create publisher for this topic
        zetabus_publisher_t* pub = NULL;
        for (size_t j = 0; j < player->topic_count; j++) {
            if (strcmp(player->topics[j], msg->topic) == 0) {
                pub = player->publishers[j];
                break;
            }
        }
        
        if (!pub) {
            pub = zetabus_publisher_create(player->bus, msg->topic);
            if (!pub) continue;
            
            player->topics = (char**)realloc(player->topics, (player->topic_count + 1) * sizeof(char*));
            player->publishers = (zetabus_publisher_t**)realloc(player->publishers, 
                                                                (player->topic_count + 1) * sizeof(zetabus_publisher_t*));
            player->topics[player->topic_count] = strdup(msg->topic);
            player->publishers[player->topic_count] = pub;
            player->topic_count++;
        }
        
        // Wait for correct timing
        if (player->speed > 0) {
            uint64_t msg_offset = msg->received_ns - recording_start;
            uint64_t target_time = playback_start + (uint64_t)(msg_offset / player->speed);
            
            while (get_monotonic_ns() < target_time) {
                usleep(100);
            }
        }
        
        // Publish message
        zetabus_publish(pub, msg->data, msg->size);
        atomic_fetch_add(&player->messages_published, 1);
        player->current_index = i + 1;
    }
    
    printf("\nPlayback complete!\n");
    return 0;
}

int timeskip_player_mcap_start_interactive(timeskip_player_mcap_t* player) {
    // TODO: Implement interactive mode (keyboard controls, progress bar)
    // For now, just call non-interactive
    return timeskip_player_mcap_start(player);
}

void timeskip_player_mcap_destroy(timeskip_player_mcap_t* player) {
    if (!player) return;
    
    // Cleanup publishers
    for (size_t i = 0; i < player->topic_count; i++) {
        zetabus_publisher_destroy(player->publishers[i]);
        free(player->topics[i]);
    }
    free(player->publishers);
    free(player->topics);
    
    // Cleanup messages
    for (size_t i = 0; i < player->message_count; i++) {
        free(player->messages[i].topic);
        free(player->messages[i].data);
    }
    free(player->messages);
    
    if (player->bus) {
        zetabus_destroy(player->bus);
    }
    
    if (player->mcap_reader) {
        player->mcap_reader->reader->close();
        delete player->mcap_reader;
    }
    
    free(player->input_file);
    free(player);
}

void timeskip_player_mcap_get_stats(timeskip_player_mcap_t* player, timeskip_player_mcap_stats_t* stats) {
    if (!player || !stats) return;
    
    stats->total_messages = player->message_count;
    stats->current_message = player->current_index;
    stats->messages_published = atomic_load(&player->messages_published);
    stats->current_speed = player->speed;
    stats->duration_ns = player->duration_ns;
    stats->position_ns = (player->current_index > 0 && player->message_count > 0) 
        ? player->messages[player->current_index - 1].received_ns - player->messages[0].received_ns 
        : 0;
}

} // extern "C"
