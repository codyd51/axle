#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

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

// Asynchronously send the message to the provided destination service
void amc_message_send(const char* destination_service, amc_message_t* msg);

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg);

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t* out);

int main(int argc, char** argv) {
	amc_register_service("com.axle.postman");
	while (true) {
		amc_message_t msg = {0};
		printf("~~~ Msg 0x%08x 0x%08x\n", msg, &msg);
		amc_message_await("com.axle.mailman", &msg);
		printf("-- Received message 0x%08x 0x%08x\n", msg, &msg);
		printf("---- source: %s\n", msg.source);
		printf("---- dest:   %s\n", msg.dest);
		printf("---- type:   %d\n", msg.type);
		printf("---- len:    %d\n", msg.len);
		printf("---- data:   ");
		for (int i = 0; i < msg.len; i++) {
			putchar(msg.data[i]);
		}
		printf("\n");
		fflush(stdout);
		printf("--------------\n");
	}
	
	return 0;
}
