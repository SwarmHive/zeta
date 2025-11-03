#ifndef ZETA_BUS_H
#define ZETA_BUS_H

#include <stddef.h>

typedef struct zetabus_s zetabus_t;
typedef struct zetabus_publisher_s zetabus_publisher_t;
typedef struct zetabus_subscriber_s zetabus_subscriber_t;

// Bus operations
zetabus_t* zetabus_create(const char* url);
void zetabus_destroy(zetabus_t* bus);

// Publisher operations
zetabus_publisher_t* zetabus_publisher_create(zetabus_t* bus, const char* topic);
void zetabus_publisher_destroy(zetabus_publisher_t* publisher);
int zetabus_publish(zetabus_publisher_t* publisher, const void* data, size_t size);

zetabus_subscriber_t* zetabus_subscriber_create(zetabus_t* bus, const char* topic, void (*callback)(const char* topic, const void* data, size_t size));
void zetabus_subscriber_destroy(zetabus_subscriber_t* subscriber);

#endif // ZETA_BUS_H
