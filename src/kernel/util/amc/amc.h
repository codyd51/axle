#ifndef AMC_H
#define AMC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum amc_message_type {
    KEYSTROKE = 0,
    STDOUT = 1,
} amc_message_type_t;

typedef struct amc_message {
    const char* source;
    const char* dest; // May be null if the message is globally broadcast
    amc_message_type_t type;
    char data[64];
    int len;
} amc_message_t;

// Register the running process as the provided service name
void amc_register_service(const char* name);

// Construct an amc message
amc_message_t* amc_message_construct(amc_message_type_t type, const char* data, int len);
// Construct an amc message, with a hint that the source service is the "kernel core" (i.e. an interrupt handler)
amc_message_t* amc_message_construct__from_core(amc_message_type_t type, const char* data, int len);

// Asynchronously send the message to the provided destination service
// Returns whether the message was successfully routed to the service
bool amc_message_send(const char* destination_service, amc_message_t* msg);

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg);

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t* out);
// Block until a message has been received from any of the provided source services
void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t* out);


#endif