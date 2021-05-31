#include <stdint.h>

int main(int argc, char** argv) {
	//int a = atoi(argv[0]);
	//printf("Empty running %s %d\n", argv[0], a);
	//printf("Empty running %d 0x%08x\n", argc, argv);
	char buf[64];
	snprintf(buf, 64, "com.axle.empty-%d", ms_since_boot());
	amc_register_service(buf);
	return 0;
}
