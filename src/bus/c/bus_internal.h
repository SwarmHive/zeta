#ifndef ZETA_BUS_INTERNAL_H
#define ZETA_BUS_INTERNAL_H

#include "bus.h"
#include <nats/nats.h>

// Internal struct definitions shared across implementation files

struct zetabus_s {
    natsConnection* nc;
    natsOptions* opts;
    char* url;
};

struct zetabus_publisher_s {
    zetabus_t* bus;
    char* topic;
};

struct zetabus_subscriber_s {
    zetabus_t* bus;
    char* topic;
    natsSubscription* sub;
    void (*callback)(const char* topic, const void* data, size_t size);
};

#endif // ZETA_BUS_INTERNAL_H
