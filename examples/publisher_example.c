#include "src/bus/c/bus.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Publisher Example ===\n\n");

    zetabus_t* bus = zetabus_create("nats://localhost:4222");
    if (!bus) {
        fprintf(stderr, "Failed to create zetabus\n");
        return 1;
    }

    zetabus_publisher_t* publisher = zetabus_publisher_create(bus, "example.topic");
    if (!publisher) {
        fprintf(stderr, "Failed to create publisher\n");
        zetabus_destroy(bus);
        return 1;
    }

    printf("Publishing messages to topic 'example.topic'...\n");
    for (int i = 0; i < 5; i++) {
        char message[50];
        snprintf(message, sizeof(message), "Hello, Zetabus! Message %d", i + 1);
        if (zetabus_publish(publisher, message, strlen(message)) != 0) {
            fprintf(stderr, "Failed to publish message %d\n", i + 1);
        } else {
            printf("Published: %s\n", message);
        }
    }

    printf("\nAll messages published successfully!\n");

    zetabus_publisher_destroy(publisher);
    zetabus_destroy(bus);
    return 0;
}
