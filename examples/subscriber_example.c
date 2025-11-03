#include "src/bus/c/bus.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void message_callback(const char* topic, const void* data, size_t size) {
    printf("Received message on topic '%s': %.*s\n", topic, (int)size, (const char*)data);
}

int main(void) {
    printf("=== Subscriber Example ===\n\n");

    // Set up signal handler for clean shutdown
    signal(SIGINT, signal_handler);

    zetabus_t* bus = zetabus_create("nats://localhost:4222");
    if (!bus) {
        fprintf(stderr, "Failed to create zetabus\n");
        return 1;
    }

    zetabus_subscriber_t* subscriber = zetabus_subscriber_create(bus, "example.topic", message_callback);
    if (!subscriber) {
        fprintf(stderr, "Failed to create subscriber\n");
        zetabus_destroy(bus);
        return 1;
    }

    printf("Subscribed to topic 'example.topic'\n");
    printf("Waiting for messages... (Press Ctrl+C to exit)\n\n");

    while (running) {
        sleep(1);
    }

    printf("\nShutting down...\n");

    zetabus_subscriber_destroy(subscriber);
    zetabus_destroy(bus);
    return 0;
}
