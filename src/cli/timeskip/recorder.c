#include "recorder.h"
#include "zet_format.h"
#include "../../bus/c/bus.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>

#define DEFAULT_BUFFER_SIZE 10000
#define BATCH_SIZE 100

// Get monotonic time in nanoseconds
static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Buffered message
typedef struct {
    uint64_t sent_ns;
    uint64_t received_ns;
    char* topic;
    void* data;
    size_t size;
} buffered_message_t;

// Lock-free circular buffer
typedef struct {
    buffered_message_t* messages;
    size_t capacity;
    atomic_size_t write_idx;
    atomic_size_t read_idx;
    atomic_bool overflow;
} circular_buffer_t;

// Recorder context
struct timeskip_recorder_s {
    zetabus_t* bus;
    zetabus_subscriber_t* subscriber;
    zet_writer_t* writer;
    circular_buffer_t* buffer;
    pthread_t writer_thread;
    atomic_bool recording;
    atomic_bool writer_running;
    atomic_bool paused;
    
    // Statistics
    atomic_uint_fast64_t messages_received;
    atomic_uint_fast64_t messages_written;
    atomic_uint_fast64_t messages_dropped;
    atomic_uint_fast64_t bytes_written;
    
    char* topic;
    char* output_file;
};

// Circular buffer functions
static circular_buffer_t* buffer_create(size_t capacity) {
    circular_buffer_t* buf = (circular_buffer_t*)malloc(sizeof(circular_buffer_t));
    if (!buf) return NULL;
    
    buf->messages = (buffered_message_t*)calloc(capacity, sizeof(buffered_message_t));
    if (!buf->messages) {
        free(buf);
        return NULL;
    }
    
    buf->capacity = capacity;
    atomic_init(&buf->write_idx, 0);
    atomic_init(&buf->read_idx, 0);
    atomic_init(&buf->overflow, false);
    
    return buf;
}

static void buffer_destroy(circular_buffer_t* buf) {
    if (buf) {
        // Free any remaining messages
        size_t read = atomic_load(&buf->read_idx);
        size_t write = atomic_load(&buf->write_idx);
        
        while (read != write) {
            buffered_message_t* msg = &buf->messages[read % buf->capacity];
            free(msg->topic);
            free(msg->data);
            read++;
        }
        
        free(buf->messages);
        free(buf);
    }
}

static bool buffer_push(circular_buffer_t* buf, const buffered_message_t* msg) {
    size_t write = atomic_load(&buf->write_idx);
    size_t read = atomic_load(&buf->read_idx);
    
    // Check if buffer is full
    if (write - read >= buf->capacity) {
        atomic_store(&buf->overflow, true);
        return false;
    }
    
    // Copy message to buffer
    buffered_message_t* slot = &buf->messages[write % buf->capacity];
    slot->sent_ns = msg->sent_ns;
    slot->received_ns = msg->received_ns;
    slot->topic = msg->topic;
    slot->data = msg->data;
    slot->size = msg->size;
    
    // Advance write index
    atomic_store(&buf->write_idx, write + 1);
    
    return true;
}

static bool buffer_pop(circular_buffer_t* buf, buffered_message_t* msg) {
    size_t read = atomic_load(&buf->read_idx);
    size_t write = atomic_load(&buf->write_idx);
    
    // Check if buffer is empty
    if (read == write) {
        return false;
    }
    
    // Copy message from buffer
    buffered_message_t* slot = &buf->messages[read % buf->capacity];
    msg->sent_ns = slot->sent_ns;
    msg->received_ns = slot->received_ns;
    msg->topic = slot->topic;
    msg->data = slot->data;
    msg->size = slot->size;
    
    // Clear slot
    slot->topic = NULL;
    slot->data = NULL;
    
    // Advance read index
    atomic_store(&buf->read_idx, read + 1);
    
    return true;
}

static bool buffer_is_empty(circular_buffer_t* buf) {
    return atomic_load(&buf->read_idx) == atomic_load(&buf->write_idx);
}

// Global recorder for callback (temporary workaround until we update zetabus API)
static timeskip_recorder_t* g_current_recorder = NULL;

// Subscriber callback (runs in NATS thread)
static void recording_callback(const char* topic, const void* data, size_t size) {
    timeskip_recorder_t* recorder = g_current_recorder;
    if (!recorder) return;
    
    // Always count received messages
    atomic_fetch_add(&recorder->messages_received, 1);
    
    // If paused, drop the message (don't buffer it)
    if (atomic_load(&recorder->paused)) {
        atomic_fetch_add(&recorder->messages_dropped, 1);
        return;
    }
    
    // Create buffered message
    buffered_message_t msg;
    msg.sent_ns = 0; // Not implemented yet
    msg.received_ns = get_monotonic_ns();
    msg.topic = strdup(topic);
    msg.data = malloc(size);
    if (!msg.topic || !msg.data) {
        free(msg.topic);
        free(msg.data);
        atomic_fetch_add(&recorder->messages_dropped, 1);
        return;
    }
    memcpy(msg.data, data, size);
    msg.size = size;
    
    // Push to buffer
    if (!buffer_push(recorder->buffer, &msg)) {
        // Buffer full - drop message
        free(msg.topic);
        free(msg.data);
        atomic_fetch_add(&recorder->messages_dropped, 1);
    }
}

// Writer thread function
static void* writer_thread_func(void* arg) {
    timeskip_recorder_t* recorder = (timeskip_recorder_t*)arg;
    buffered_message_t batch[BATCH_SIZE];
    
    atomic_store(&recorder->writer_running, true);
    
    while (atomic_load(&recorder->recording) || !buffer_is_empty(recorder->buffer)) {
        size_t count = 0;
        
        // Drain buffer in batches
        while (count < BATCH_SIZE && buffer_pop(recorder->buffer, &batch[count])) {
            count++;
        }
        
        if (count > 0) {
            // Write batch to file
            for (size_t i = 0; i < count; i++) {
                zet_writer_write_message(recorder->writer,
                                        batch[i].sent_ns,
                                        batch[i].received_ns,
                                        batch[i].topic,
                                        batch[i].data,
                                        batch[i].size);
                
                // Track bytes written (header + topic + payload)
                size_t msg_size = sizeof(uint64_t) * 2 + // timestamps
                                 sizeof(uint16_t) +      // topic_len
                                 sizeof(uint32_t) +      // payload_size
                                 strlen(batch[i].topic) + 1 + // topic with null
                                 batch[i].size;          // payload
                atomic_fetch_add(&recorder->bytes_written, msg_size);
                
                // Free message memory
                free(batch[i].topic);
                free(batch[i].data);
                
                atomic_fetch_add(&recorder->messages_written, 1);
            }
            
            // Flush to disk
            zet_writer_flush(recorder->writer);
        } else {
            // Buffer empty, sleep briefly
            usleep(1000); // 1ms
        }
    }
    
    atomic_store(&recorder->writer_running, false);
    return NULL;
}

// Public API implementation
timeskip_recorder_t* timeskip_recorder_create(const char* nats_url,
                                                const char* topic,
                                                const char* output_file,
                                                size_t buffer_size) {
    if (!nats_url || !topic || !output_file) return NULL;
    
    timeskip_recorder_t* recorder = (timeskip_recorder_t*)calloc(1, sizeof(timeskip_recorder_t));
    if (!recorder) return NULL;
    
    // Initialize atomics
    atomic_init(&recorder->recording, false);
    atomic_init(&recorder->writer_running, false);
    atomic_init(&recorder->paused, false);
    atomic_init(&recorder->messages_received, 0);
    atomic_init(&recorder->messages_written, 0);
    atomic_init(&recorder->messages_dropped, 0);
    atomic_init(&recorder->bytes_written, 0);
    
    // Copy strings
    recorder->topic = strdup(topic);
    recorder->output_file = strdup(output_file);
    
    // Create buffer
    size_t cap = buffer_size > 0 ? buffer_size : DEFAULT_BUFFER_SIZE;
    recorder->buffer = buffer_create(cap);
    if (!recorder->buffer) {
        free(recorder->topic);
        free(recorder->output_file);
        free(recorder);
        return NULL;
    }
    
    // Create writer
    recorder->writer = zet_writer_create(output_file);
    if (!recorder->writer) {
        buffer_destroy(recorder->buffer);
        free(recorder->topic);
        free(recorder->output_file);
        free(recorder);
        return NULL;
    }
    
    // Connect to NATS
    recorder->bus = zetabus_create(nats_url);
    if (!recorder->bus) {
        zet_writer_destroy(recorder->writer);
        buffer_destroy(recorder->buffer);
        free(recorder->topic);
        free(recorder->output_file);
        free(recorder);
        return NULL;
    }
    
    return recorder;
}

int timeskip_recorder_start(timeskip_recorder_t* recorder) {
    if (!recorder) return -1;
    
    // Set global recorder (workaround)
    g_current_recorder = recorder;
    
    // Create subscriber with callback
    recorder->subscriber = zetabus_subscriber_create(recorder->bus, recorder->topic, recording_callback);
    if (!recorder->subscriber) {
        return -1;
    }
    
    // Start recording
    atomic_store(&recorder->recording, true);
    
    // Start writer thread
    if (pthread_create(&recorder->writer_thread, NULL, writer_thread_func, recorder) != 0) {
        atomic_store(&recorder->recording, false);
        zetabus_subscriber_destroy(recorder->subscriber);
        return -1;
    }
    
    return 0;
}

void timeskip_recorder_stop(timeskip_recorder_t* recorder) {
    if (!recorder) return;
    
    // Stop recording
    atomic_store(&recorder->recording, false);
    
    // Clear global recorder
    if (g_current_recorder == recorder) {
        g_current_recorder = NULL;
    }
    
    // Wait for writer thread to finish draining buffer
    if (atomic_load(&recorder->writer_running)) {
        pthread_join(recorder->writer_thread, NULL);
    }
    
    // Destroy subscriber
    if (recorder->subscriber) {
        zetabus_subscriber_destroy(recorder->subscriber);
        recorder->subscriber = NULL;
    }
}

void timeskip_recorder_pause(timeskip_recorder_t* recorder) {
    if (recorder) {
        atomic_store(&recorder->paused, true);
    }
}

void timeskip_recorder_resume(timeskip_recorder_t* recorder) {
    if (recorder) {
        atomic_store(&recorder->paused, false);
    }
}

bool timeskip_recorder_is_paused(timeskip_recorder_t* recorder) {
    return recorder ? atomic_load(&recorder->paused) : false;
}

void timeskip_recorder_destroy(timeskip_recorder_t* recorder) {
    if (!recorder) return;
    
    // Clean up resources
    if (recorder->writer) {
        zet_writer_destroy(recorder->writer);
    }
    
    if (recorder->bus) {
        zetabus_destroy(recorder->bus);
    }
    
    if (recorder->buffer) {
        buffer_destroy(recorder->buffer);
    }
    
    free(recorder->topic);
    free(recorder->output_file);
    free(recorder);
}

void timeskip_recorder_get_stats(timeskip_recorder_t* recorder, timeskip_stats_t* stats) {
    if (!recorder || !stats) return;
    
    stats->messages_received = atomic_load(&recorder->messages_received);
    stats->messages_written = atomic_load(&recorder->messages_written);
    stats->messages_dropped = atomic_load(&recorder->messages_dropped);
    stats->bytes_written = atomic_load(&recorder->bytes_written);
    stats->buffer_overflow = atomic_load(&recorder->buffer->overflow);
}
