#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>

int main(int argc, char** argv) {
	amc_register_service("com.axle.tty");

	while (true) {
		amc_message_t msg = {0};
		amc_message_await("com.axle.core", &msg);
		amc_message_t* forwarded_msg = amc_message_construct((const char*)&msg.data, msg.len);
		amc_message_send("com.axle.awm", forwarded_msg);
	}
	
	return 0;
}
