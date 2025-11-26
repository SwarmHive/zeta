#ifndef ZET_FORMAT_H
#define ZET_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// .zet file format header
typedef struct {
    char magic[4];           // "ZET\0"
    uint32_t version;        // File format version (1)
    uint64_t start_time_ns;  // Recording start time
    uint8_t reserved[16];    // Future use
} zet_header_t;

// Message record in .zet file
typedef struct {
    uint64_t sent_ns;        // When publisher sent (0 if unknown)
    uint64_t received_ns;    // When timeskip received
    uint16_t topic_len;      // Length of topic string (including null terminator)
    uint32_t payload_size;   // Size of payload in bytes
    // Followed by:
    // - topic (topic_len bytes, null-terminated)
    // - payload (payload_size bytes)
} zet_message_header_t;

// Writer API
typedef struct zet_writer_s zet_writer_t;

zet_writer_t* zet_writer_create(const char* filename);
void zet_writer_destroy(zet_writer_t* writer);
int zet_writer_write_message(zet_writer_t* writer, 
                              uint64_t sent_ns,
                              uint64_t received_ns,
                              const char* topic,
                              const void* data,
                              size_t size);
void zet_writer_flush(zet_writer_t* writer);

// Reader API (for future playback)
typedef struct zet_reader_s zet_reader_t;

typedef struct {
    uint64_t sent_ns;
    uint64_t received_ns;
    char* topic;
    void* data;
    size_t size;
} zet_message_t;

zet_reader_t* zet_reader_create(const char* filename);
void zet_reader_destroy(zet_reader_t* reader);
int zet_reader_read_message(zet_reader_t* reader, zet_message_t* msg);
void zet_message_free(zet_message_t* msg);
uint64_t zet_reader_get_start_time(zet_reader_t* reader);

#endif // ZET_FORMAT_H
