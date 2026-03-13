#include "recorder_mcap.h"
#include "../../../bus/c/bus.h"
#include <mcap/writer.hpp>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>
#include <fstream>
#include <memory>
#include <unordered_map>

#define DEFAULT_BUFFER_SIZE 100000
#define BATCH_SIZE 1000

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

// MCAP writer wrapper
struct McapWriter {
    std::unique_ptr<std::ofstream> file;
    std::unique_ptr<mcap::McapWriter> writer;
    std::unordered_map<std::string, mcap::ChannelId> channels;
    mcap::SchemaId schema_id;
};

// Recorder context
struct timeskip_recorder_mcap_s {
    zetabus_t* bus;
    zetabus_subscriber_t* subscriber;
    McapWriter* mcap_writer;
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

// Circular buffer functions (same as ZET)
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
    
    if (write - read >= buf->capacity) {
        atomic_store(&buf->overflow, true);
        return false;
    }
    
    buffered_message_t* slot = &buf->messages[write % buf->capacity];
    slot->sent_ns = msg->sent_ns;
    slot->received_ns = msg->received_ns;
    slot->topic = msg->topic;
    slot->data = msg->data;
    slot->size = msg->size;
    
    atomic_store(&buf->write_idx, write + 1);
    return true;
}

static bool buffer_pop(circular_buffer_t* buf, buffered_message_t* msg) {
    size_t read = atomic_load(&buf->read_idx);
    size_t write = atomic_load(&buf->write_idx);
    
    if (read == write) {
        return false;
    }
    
    buffered_message_t* slot = &buf->messages[read % buf->capacity];
    msg->sent_ns = slot->sent_ns;
    msg->received_ns = slot->received_ns;
    msg->topic = slot->topic;
    msg->data = slot->data;
    msg->size = slot->size;
    
    slot->topic = NULL;
    slot->data = NULL;
    
    atomic_store(&buf->read_idx, read + 1);
    return true;
}

static bool buffer_is_empty(circular_buffer_t* buf) {
    return atomic_load(&buf->read_idx) == atomic_load(&buf->write_idx);
}

// Global recorder for callback
static timeskip_recorder_mcap_t* g_current_recorder_mcap = NULL;

// Subscriber callback
static void recording_callback_mcap(const char* topic, const void* data, size_t size) {
    timeskip_recorder_mcap_t* recorder = g_current_recorder_mcap;
    if (!recorder) return;
    
    atomic_fetch_add(&recorder->messages_received, 1);
    
    if (atomic_load(&recorder->paused)) {
        atomic_fetch_add(&recorder->messages_dropped, 1);
        return;
    }
    
    buffered_message_t msg;
    msg.sent_ns = 0;
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
    
    if (!buffer_push(recorder->buffer, &msg)) {
        free(msg.topic);
        free(msg.data);
        atomic_fetch_add(&recorder->messages_dropped, 1);
    }
}

// Writer thread function
static void* writer_thread_func_mcap(void* arg) {
    timeskip_recorder_mcap_t* recorder = (timeskip_recorder_mcap_t*)arg;
    buffered_message_t batch[BATCH_SIZE];
    
    atomic_store(&recorder->writer_running, true);
    
    while (atomic_load(&recorder->recording) || !buffer_is_empty(recorder->buffer)) {
        size_t count = 0;
        
        while (count < BATCH_SIZE && buffer_pop(recorder->buffer, &batch[count])) {
            count++;
        }
        
        if (count > 0) {
            for (size_t i = 0; i < count; i++) {
                // Get or create channel for this topic
                mcap::ChannelId channel_id;
                auto it = recorder->mcap_writer->channels.find(batch[i].topic);
                if (it == recorder->mcap_writer->channels.end()) {
                    mcap::Channel channel(batch[i].topic, "raw", recorder->mcap_writer->schema_id);
                    recorder->mcap_writer->writer->addChannel(channel);
                    channel_id = channel.id;
                    recorder->mcap_writer->channels[batch[i].topic] = channel_id;
                } else {
                    channel_id = it->second;
                }
                
                // Write message
                mcap::Message msg;
                msg.channelId = channel_id;
                msg.sequence = 0;
                msg.publishTime = batch[i].sent_ns;
                msg.logTime = batch[i].received_ns;
                msg.data = reinterpret_cast<const std::byte*>(batch[i].data);
                msg.dataSize = batch[i].size;
                
                recorder->mcap_writer->writer->write(msg);
                
                // Track bytes
                size_t msg_size = batch[i].size + strlen(batch[i].topic) + 32; // approximate
                atomic_fetch_add(&recorder->bytes_written, msg_size);
                
                free(batch[i].topic);
                free(batch[i].data);
                
                atomic_fetch_add(&recorder->messages_written, 1);
            }
            
            recorder->mcap_writer->file->flush();
        } else {
            usleep(1000);
        }
    }
    
    atomic_store(&recorder->writer_running, false);
    return NULL;
}

// Public API
extern "C" {

timeskip_recorder_mcap_t* timeskip_recorder_mcap_create(const char* nats_url,
                                                         const char* topic,
                                                         const char* output_file,
                                                         size_t buffer_size) {
    if (!nats_url || !topic || !output_file) return NULL;
    
    timeskip_recorder_mcap_t* recorder = (timeskip_recorder_mcap_t*)calloc(1, sizeof(timeskip_recorder_mcap_t));
    if (!recorder) return NULL;
    
    atomic_init(&recorder->recording, false);
    atomic_init(&recorder->writer_running, false);
    atomic_init(&recorder->paused, false);
    atomic_init(&recorder->messages_received, 0);
    atomic_init(&recorder->messages_written, 0);
    atomic_init(&recorder->messages_dropped, 0);
    atomic_init(&recorder->bytes_written, 0);
    
    recorder->topic = strdup(topic);
    recorder->output_file = strdup(output_file);
    
    size_t cap = buffer_size > 0 ? buffer_size : DEFAULT_BUFFER_SIZE;
    recorder->buffer = buffer_create(cap);
    if (!recorder->buffer) {
        free(recorder->topic);
        free(recorder->output_file);
        free(recorder);
        return NULL;
    }
    
    // Create MCAP writer
    recorder->mcap_writer = new McapWriter();
    recorder->mcap_writer->file = std::make_unique<std::ofstream>(output_file, std::ios::binary);
    if (!recorder->mcap_writer->file->is_open()) {
        delete recorder->mcap_writer;
        buffer_destroy(recorder->buffer);
        free(recorder->topic);
        free(recorder->output_file);
        free(recorder);
        return NULL;
    }
    
    recorder->mcap_writer->writer = std::make_unique<mcap::McapWriter>();
    mcap::McapWriterOptions opts("zetabus");
    opts.compression = mcap::Compression::Zstd;
    opts.compressionLevel = mcap::CompressionLevel::Default;
    recorder->mcap_writer->writer->open(*recorder->mcap_writer->file, opts);
    
    // Create schema
    mcap::Schema schema("", "raw", "");
    recorder->mcap_writer->writer->addSchema(schema);
    recorder->mcap_writer->schema_id = schema.id;
    
    // Connect to NATS
    recorder->bus = zetabus_create(nats_url);
    if (!recorder->bus) {
        recorder->mcap_writer->writer->close();
        delete recorder->mcap_writer;
        buffer_destroy(recorder->buffer);
        free(recorder->topic);
        free(recorder->output_file);
        free(recorder);
        return NULL;
    }
    
    return recorder;
}

int timeskip_recorder_mcap_start(timeskip_recorder_mcap_t* recorder) {
    if (!recorder) return -1;
    
    g_current_recorder_mcap = recorder;
    
    recorder->subscriber = zetabus_subscriber_create(recorder->bus, recorder->topic, recording_callback_mcap);
    if (!recorder->subscriber) {
        return -1;
    }
    
    atomic_store(&recorder->recording, true);
    
    if (pthread_create(&recorder->writer_thread, NULL, writer_thread_func_mcap, recorder) != 0) {
        atomic_store(&recorder->recording, false);
        zetabus_subscriber_destroy(recorder->subscriber);
        return -1;
    }
    
    return 0;
}

void timeskip_recorder_mcap_stop(timeskip_recorder_mcap_t* recorder) {
    if (!recorder) return;
    
    atomic_store(&recorder->recording, false);
    
    if (g_current_recorder_mcap == recorder) {
        g_current_recorder_mcap = NULL;
    }
    
    if (atomic_load(&recorder->writer_running)) {
        pthread_join(recorder->writer_thread, NULL);
    }
    
    if (recorder->subscriber) {
        zetabus_subscriber_destroy(recorder->subscriber);
        recorder->subscriber = NULL;
    }
}

void timeskip_recorder_mcap_pause(timeskip_recorder_mcap_t* recorder) {
    if (recorder) {
        atomic_store(&recorder->paused, true);
    }
}

void timeskip_recorder_mcap_resume(timeskip_recorder_mcap_t* recorder) {
    if (recorder) {
        atomic_store(&recorder->paused, false);
    }
}

bool timeskip_recorder_mcap_is_paused(timeskip_recorder_mcap_t* recorder) {
    return recorder ? atomic_load(&recorder->paused) : false;
}

void timeskip_recorder_mcap_destroy(timeskip_recorder_mcap_t* recorder) {
    if (!recorder) return;
    
    if (recorder->mcap_writer) {
        recorder->mcap_writer->writer->close();
        delete recorder->mcap_writer;
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

void timeskip_recorder_mcap_get_stats(timeskip_recorder_mcap_t* recorder, timeskip_recorder_mcap_stats_t* stats) {
    if (!recorder || !stats) return;
    
    stats->messages_received = atomic_load(&recorder->messages_received);
    stats->messages_written = atomic_load(&recorder->messages_written);
    stats->messages_dropped = atomic_load(&recorder->messages_dropped);
    stats->bytes_written = atomic_load(&recorder->bytes_written);
    stats->buffer_overflow = atomic_load(&recorder->buffer->overflow);
}

} // extern "C"
