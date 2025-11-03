#include "bus.h"
#include "bus_internal.h"
#include <stdlib.h>
#include <string.h>

// NATS callback wrapper that converts to our callback signature
static void _nats_message_handler(natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure) {
    zetabus_subscriber_t* subscriber = (zetabus_subscriber_t*)closure;
    
    if (subscriber && subscriber->callback) {
        const char* data = natsMsg_GetData(msg);
        int data_len = natsMsg_GetDataLength(msg);
        const char* subject = natsMsg_GetSubject(msg);
        
        subscriber->callback(subject, data, (size_t)data_len);
    }
    
    natsMsg_Destroy(msg);
}

zetabus_subscriber_t* zetabus_subscriber_create(zetabus_t* bus, const char* topic, 
                                                  void (*callback)(const char* topic, const void* data, size_t size)) {
    if (!bus || !topic || !callback) return NULL;
    
    zetabus_subscriber_t* subscriber = (zetabus_subscriber_t*)malloc(sizeof(zetabus_subscriber_t));
    if (!subscriber) return NULL;
    
    subscriber->bus = bus;
    subscriber->topic = strdup(topic);
    subscriber->callback = callback;
    subscriber->sub = NULL;
    
    if (!subscriber->topic) {
        free(subscriber);
        return NULL;
    }
    
    // Subscribe with callback
    natsStatus s = natsConnection_Subscribe(&subscriber->sub, bus->nc, topic, 
                                             _nats_message_handler, subscriber);
    if (s != NATS_OK) {
        free(subscriber->topic);
        free(subscriber);
        return NULL;
    }
    
    return subscriber;
}

void zetabus_subscriber_destroy(zetabus_subscriber_t* subscriber) {
    if (subscriber) {
        if (subscriber->sub) {
            natsSubscription_Unsubscribe(subscriber->sub);
            natsSubscription_Destroy(subscriber->sub);
        }
        free(subscriber->topic);
        free(subscriber);
    }
}
