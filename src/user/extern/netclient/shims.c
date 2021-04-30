#include "shims.h"
#include "mock-request.h"

uint32_t html_read_tcp_stream(uint32_t conn_descriptor, uint8_t* buf, uint32_t len) {
	//return net_tcp_conn_read(conn_descriptor, buf, len);
    memcpy(buf, mocked_request, sizeof(mocked_request));
	return sizeof(mocked_request);
}
