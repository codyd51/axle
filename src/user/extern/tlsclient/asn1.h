#ifndef ASN1_H
#define ASN1_H

#include <stdint.h>
#include "tls.h"

void asn1_cert_parse(tls_conn_t* conn, uint32_t cert_len);

#endif
