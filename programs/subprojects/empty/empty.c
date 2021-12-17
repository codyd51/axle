#include <stdint.h>

int main(int argc, char** argv) {
	char buf[64];
	snprintf(buf, 64, "com.axle.empty-%d", ms_since_boot());
	amc_register_service(buf);
	return 0;
}
