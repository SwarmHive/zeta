#include "zet_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Test helper to create a temp file path
static const char* get_test_filename(void) {
    return "/tmp/test_zet_format.zet";
}

// Test basic write and read
void test_write_read_basic(void) {
    printf("Running test_write_read_basic...\n");
    
    const char* filename = get_test_filename();
    unlink(filename);  // Remove if exists
    
    // Write some messages
    zet_writer_t* writer = zet_writer_create(filename);
    assert(writer != NULL);
    
    const char* topic1 = "test/topic1";
    const char* data1 = "Hello, World!";
    int ret = zet_writer_write_message(writer, 1000, 2000, topic1, data1, strlen(data1));
    assert(ret == 0);
    
    const char* topic2 = "test/topic2";
    const char* data2 = "Second message";
    ret = zet_writer_write_message(writer, 3000, 4000, topic2, data2, strlen(data2));
    assert(ret == 0);
    
    zet_writer_destroy(writer);
    
    // Read back the messages
    zet_reader_t* reader = zet_reader_create(filename);
    assert(reader != NULL);
    
    uint64_t start_time = zet_reader_get_start_time(reader);
    assert(start_time > 0);
    
    // Read first message
    zet_message_t msg1;
    ret = zet_reader_read_message(reader, &msg1);
    assert(ret == 0);
    assert(msg1.sent_ns == 1000);
    assert(msg1.received_ns == 2000);
    assert(strcmp(msg1.topic, topic1) == 0);
    assert(msg1.size == strlen(data1));
    assert(memcmp(msg1.data, data1, msg1.size) == 0);
    zet_message_free(&msg1);
    
    // Read second message
    zet_message_t msg2;
    ret = zet_reader_read_message(reader, &msg2);
    assert(ret == 0);
    assert(msg2.sent_ns == 3000);
    assert(msg2.received_ns == 4000);
    assert(strcmp(msg2.topic, topic2) == 0);
    assert(msg2.size == strlen(data2));
    assert(memcmp(msg2.data, data2, msg2.size) == 0);
    zet_message_free(&msg2);
    
    // Try to read past end
    zet_message_t msg3;
    ret = zet_reader_read_message(reader, &msg3);
    assert(ret != 0);  // Should fail at EOF
    
    zet_reader_destroy(reader);
    unlink(filename);
    
    printf("test_write_read_basic PASSED\n");
}

// Test empty messages
void test_empty_message(void) {
    printf("Running test_empty_message...\n");
    
    const char* filename = get_test_filename();
    unlink(filename);
    
    zet_writer_t* writer = zet_writer_create(filename);
    assert(writer != NULL);
    
    const char* topic = "empty/topic";
    const char* data = "";
    int ret = zet_writer_write_message(writer, 0, 0, topic, data, 0);
    assert(ret == 0);
    
    zet_writer_destroy(writer);
    
    zet_reader_t* reader = zet_reader_create(filename);
    assert(reader != NULL);
    
    zet_message_t msg;
    ret = zet_reader_read_message(reader, &msg);
    assert(ret == 0);
    assert(msg.size == 0);
    assert(strcmp(msg.topic, topic) == 0);
    zet_message_free(&msg);
    
    zet_reader_destroy(reader);
    unlink(filename);
    
    printf("test_empty_message PASSED\n");
}

// Test binary data
void test_binary_data(void) {
    printf("Running test_binary_data...\n");
    
    const char* filename = get_test_filename();
    unlink(filename);
    
    zet_writer_t* writer = zet_writer_create(filename);
    assert(writer != NULL);
    
    const char* topic = "binary/data";
    uint8_t binary_data[] = {0x00, 0xFF, 0xAB, 0xCD, 0xEF, 0x00, 0x12, 0x34};
    size_t data_size = sizeof(binary_data);
    
    int ret = zet_writer_write_message(writer, 5000, 6000, topic, binary_data, data_size);
    assert(ret == 0);
    
    zet_writer_destroy(writer);
    
    zet_reader_t* reader = zet_reader_create(filename);
    assert(reader != NULL);
    
    zet_message_t msg;
    ret = zet_reader_read_message(reader, &msg);
    assert(ret == 0);
    assert(msg.size == data_size);
    assert(memcmp(msg.data, binary_data, data_size) == 0);
    zet_message_free(&msg);
    
    zet_reader_destroy(reader);
    unlink(filename);
    
    printf("test_binary_data PASSED\n");
}

// Test multiple messages with same topic
void test_multiple_messages_same_topic(void) {
    printf("Running test_multiple_messages_same_topic...\n");
    
    const char* filename = get_test_filename();
    unlink(filename);
    
    zet_writer_t* writer = zet_writer_create(filename);
    assert(writer != NULL);
    
    const char* topic = "repeated/topic";
    const int num_messages = 100;
    
    for (int i = 0; i < num_messages; i++) {
        char data[32];
        snprintf(data, sizeof(data), "Message %d", i);
        int ret = zet_writer_write_message(writer, i * 1000, i * 1000 + 500, topic, data, strlen(data));
        assert(ret == 0);
    }
    
    zet_writer_destroy(writer);
    
    zet_reader_t* reader = zet_reader_create(filename);
    assert(reader != NULL);
    
    for (int i = 0; i < num_messages; i++) {
        zet_message_t msg;
        int ret = zet_reader_read_message(reader, &msg);
        assert(ret == 0);
        assert(msg.sent_ns == (uint64_t)(i * 1000));
        assert(msg.received_ns == (uint64_t)(i * 1000 + 500));
        assert(strcmp(msg.topic, topic) == 0);
        
        char expected_data[32];
        snprintf(expected_data, sizeof(expected_data), "Message %d", i);
        assert(msg.size == strlen(expected_data));
        assert(memcmp(msg.data, expected_data, msg.size) == 0);
        
        zet_message_free(&msg);
    }
    
    zet_reader_destroy(reader);
    unlink(filename);
    
    printf("test_multiple_messages_same_topic PASSED\n");
}

// Test flush functionality
void test_flush(void) {
    printf("Running test_flush...\n");
    
    const char* filename = get_test_filename();
    unlink(filename);
    
    zet_writer_t* writer = zet_writer_create(filename);
    assert(writer != NULL);
    
    const char* topic = "flush/test";
    const char* data = "Test flush";
    int ret = zet_writer_write_message(writer, 0, 0, topic, data, strlen(data));
    assert(ret == 0);
    
    zet_writer_flush(writer);
    
    // Write another message after flush
    ret = zet_writer_write_message(writer, 1000, 2000, topic, data, strlen(data));
    assert(ret == 0);
    
    zet_writer_destroy(writer);
    
    // Verify both messages are readable
    zet_reader_t* reader = zet_reader_create(filename);
    assert(reader != NULL);
    
    zet_message_t msg1;
    ret = zet_reader_read_message(reader, &msg1);
    assert(ret == 0);
    zet_message_free(&msg1);
    
    zet_message_t msg2;
    ret = zet_reader_read_message(reader, &msg2);
    assert(ret == 0);
    zet_message_free(&msg2);
    
    zet_reader_destroy(reader);
    unlink(filename);
    
    printf("test_flush PASSED\n");
}

// Test invalid file operations
void test_invalid_operations(void) {
    printf("Running test_invalid_operations...\n");
    
    // Try to open non-existent file for reading
    zet_reader_t* reader = zet_reader_create("/tmp/nonexistent_file_12345.zet");
    assert(reader == NULL);
    
    // Try to create writer in non-existent directory
    zet_writer_t* writer = zet_writer_create("/nonexistent_dir_12345/test.zet");
    assert(writer == NULL);
    
    printf("test_invalid_operations PASSED\n");
}

int main(void) {
    printf("Starting ZET format tests...\n\n");
    
    test_write_read_basic();
    test_empty_message();
    test_binary_data();
    test_multiple_messages_same_topic();
    test_flush();
    test_invalid_operations();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}
