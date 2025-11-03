#include "bus.h"
#include "bus_internal.h"
#include <stdlib.h>
#include <string.h>

zetabus_publisher_t* zetabus_publisher_create(zetabus_t* bus, const char* topic) {
    if (!bus || !topic) return NULL;
    
    zetabus_publisher_t* pub = (zetabus_publisher_t*)malloc(sizeof(zetabus_publisher_t));
    if (!pub) return NULL;
    
    pub->bus = bus;
    pub->topic = strdup(topic);
    if (!pub->topic) {
        free(pub);
        return NULL;
    }
    
    return pub;
}

void zetabus_publisher_destroy(zetabus_publisher_t* pub) {
    if (pub) {
        free(pub->topic);
        free(pub);
    }
}

int zetabus_publish(zetabus_publisher_t* pub, const void* data, size_t size) {
    if (!pub || !pub->bus || !data) return -1;
    
    natsStatus s = natsConnection_Publish(pub->bus->nc, pub->topic, data, size);
    return (s == NATS_OK) ? 0 : -1;
}