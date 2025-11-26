#include "zet_format.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Get monotonic time in nanoseconds
static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Writer implementation
struct zet_writer_s {
    FILE* file;
    uint64_t start_time_ns;
};

zet_writer_t* zet_writer_create(const char* filename) {
    zet_writer_t* writer = (zet_writer_t*)malloc(sizeof(zet_writer_t));
    if (!writer) return NULL;

    writer->file = fopen(filename, "wb");
    if (!writer->file) {
        free(writer);
        return NULL;
    }

    writer->start_time_ns = get_monotonic_ns();

    // Write header
    zet_header_t header = {
        .magic = {'Z', 'E', 'T', '\0'},
        .version = 1,
        .start_time_ns = writer->start_time_ns,
        .reserved = {0}
    };

    if (fwrite(&header, sizeof(zet_header_t), 1, writer->file) != 1) {
        fclose(writer->file);
        free(writer);
        return NULL;
    }

    return writer;
}

void zet_writer_destroy(zet_writer_t* writer) {
    if (writer) {
        if (writer->file) {
            fflush(writer->file);
            fclose(writer->file);
        }
        free(writer);
    }
}

int zet_writer_write_message(zet_writer_t* writer,
                              uint64_t sent_ns,
                              uint64_t received_ns,
                              const char* topic,
                              const void* data,
                              size_t size) {
    if (!writer || !writer->file || !topic || !data) return -1;

    uint16_t topic_len = (uint16_t)(strlen(topic) + 1); // Include null terminator
    uint32_t payload_size = (uint32_t)size;

    // Write message header
    if (fwrite(&sent_ns, sizeof(uint64_t), 1, writer->file) != 1) return -1;
    if (fwrite(&received_ns, sizeof(uint64_t), 1, writer->file) != 1) return -1;
    if (fwrite(&topic_len, sizeof(uint16_t), 1, writer->file) != 1) return -1;
    if (fwrite(&payload_size, sizeof(uint32_t), 1, writer->file) != 1) return -1;

    // Write topic
    if (fwrite(topic, 1, topic_len, writer->file) != topic_len) return -1;

    // Write payload
    if (fwrite(data, 1, size, writer->file) != size) return -1;

    return 0;
}

void zet_writer_flush(zet_writer_t* writer) {
    if (writer && writer->file) {
        fflush(writer->file);
    }
}

// Reader implementation
struct zet_reader_s {
    FILE* file;
    zet_header_t header;
};

zet_reader_t* zet_reader_create(const char* filename) {
    zet_reader_t* reader = (zet_reader_t*)malloc(sizeof(zet_reader_t));
    if (!reader) return NULL;

    reader->file = fopen(filename, "rb");
    if (!reader->file) {
        free(reader);
        return NULL;
    }

    // Read and validate header
    if (fread(&reader->header, sizeof(zet_header_t), 1, reader->file) != 1) {
        fclose(reader->file);
        free(reader);
        return NULL;
    }

    // Validate magic
    if (memcmp(reader->header.magic, "ZET", 3) != 0) {
        fclose(reader->file);
        free(reader);
        return NULL;
    }

    // Check version
    if (reader->header.version != 1) {
        fclose(reader->file);
        free(reader);
        return NULL;
    }

    return reader;
}

void zet_reader_destroy(zet_reader_t* reader) {
    if (reader) {
        if (reader->file) {
            fclose(reader->file);
        }
        free(reader);
    }
}

int zet_reader_read_message(zet_reader_t* reader, zet_message_t* msg) {
    if (!reader || !reader->file || !msg) return -1;

    uint64_t sent_ns, received_ns;
    uint16_t topic_len;
    uint32_t payload_size;

    // Read message header
    if (fread(&sent_ns, sizeof(uint64_t), 1, reader->file) != 1) return -1;
    if (fread(&received_ns, sizeof(uint64_t), 1, reader->file) != 1) return -1;
    if (fread(&topic_len, sizeof(uint16_t), 1, reader->file) != 1) return -1;
    if (fread(&payload_size, sizeof(uint32_t), 1, reader->file) != 1) return -1;

    // Allocate and read topic
    char* topic = (char*)malloc(topic_len);
    if (!topic) return -1;
    if (fread(topic, 1, topic_len, reader->file) != topic_len) {
        free(topic);
        return -1;
    }

    // Allocate and read payload
    void* data = malloc(payload_size);
    if (!data) {
        free(topic);
        return -1;
    }
    if (fread(data, 1, payload_size, reader->file) != payload_size) {
        free(topic);
        free(data);
        return -1;
    }

    // Fill message structure
    msg->sent_ns = sent_ns;
    msg->received_ns = received_ns;
    msg->topic = topic;
    msg->data = data;
    msg->size = payload_size;

    return 0;
}

void zet_message_free(zet_message_t* msg) {
    if (msg) {
        free(msg->topic);
        free(msg->data);
        msg->topic = NULL;
        msg->data = NULL;
        msg->size = 0;
    }
}

uint64_t zet_reader_get_start_time(zet_reader_t* reader) {
    return reader ? reader->header.start_time_ns : 0;
}
