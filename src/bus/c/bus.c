#include "bus.h"
#include "bus_internal.h"
#include <stdlib.h>
#include <string.h>

// Zetabus Creation and Destruction

zetabus_t* zetabus_create(const char* url) {
    zetabus_t* bus = (zetabus_t*)malloc(sizeof(zetabus_t));
    if (!bus) return NULL;

    bus->url = strdup(url);
    natsOptions_Create(&bus->opts);
    natsOptions_SetURL(bus->opts, url);

    natsConnection* nc = NULL;
    natsStatus s = natsConnection_Connect(&nc, bus->opts);
    if (s != NATS_OK) {
        free(bus->url);
        free(bus);
        return NULL;
    }

    bus->nc = nc;
    return bus;
}

void zetabus_destroy(zetabus_t* bus) {
    if (bus) {
        natsConnection_Destroy(bus->nc);
        natsOptions_Destroy(bus->opts);
        free(bus->url);
        free(bus);
    }
}