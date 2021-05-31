#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libamc/libamc.h>
#include <stdlibadd/assert.h>

#include <libnet/libnet.h>
#include <libport/libport.h>

#include "tls.h"

#define SEQUENCE_TAG				0x10
#define CONSTRUCTED_SEQUENCE_TAG	SEQUENCE_TAG | (1 << 6)

#define ASN1_UNIVERSAL_TAG_INTEGER 	0x2
#define X509_CERT_VERSION_TAG	0

typedef enum asn1_ident_class {
	ASN1_IDENT_UNIVERSAL = 0,
	ASN1_IDENT_APPLICATION = 1,
	ASN1_IDENT_CONTEXT_SPECIFIC = 2,
	ASN1_IDENT_PRIVATE = 3,
} asn1_ident_class_enum_t;

typedef struct asn1_ident {
	asn1_ident_class_enum_t class;
	bool is_constructed;
	uint32_t tag_value;
} asn1_ident_t;

static void _print_tabs(uint32_t depth) {
	for (uint32_t i = 0; i < depth; i++) {
		putchar('\t');
	}
}

static void _match(tls_conn_t* state, uint8_t* expected, uint32_t expected_len) {
	uint8_t* b = tls_read(state, expected_len);
	assert(memcmp(b, expected, expected_len), "Match failed");
	free(b);
}

static void _match_byte(tls_conn_t* state, uint8_t byte) {
	assert(tls_read_byte(state) == byte, "Match byte failed");
}

static asn1_ident_t* _parse_definite_length_ident(tls_conn_t* state) {
	asn1_ident_t* ident = calloc(1, sizeof(asn1_ident_t));
	// See section 3 "Basic Encoding Rules"
	// https://luca.ntop.org/Teaching/Appunti/asn1.html
	uint8_t ident_byte1 = tls_read_byte(state);
	// Class selector is bits 7-8
	uint8_t class_selector = (ident_byte1 >> 6) & 0b11;
	switch (class_selector) {
		case 0b00:
			ident->class = ASN1_IDENT_UNIVERSAL;
			break;
		case 0b01:
			ident->class = ASN1_IDENT_APPLICATION;
			break;
		case 0b10:
			ident->class = ASN1_IDENT_CONTEXT_SPECIFIC;
			break;
		case 0b11:
		default:
			ident->class = ASN1_IDENT_PRIVATE;
			break;
	}

	ident->is_constructed = ident_byte1 & (1 << 5);
	
	uint8_t tag_value = ident_byte1 & 0b11111;
	// If all 5 bits are set, this identifier has high-tag-number form
	if (tag_value == 0b11111) {
		assert(0, "High-tag-number form is unsupported");
	}
	ident->tag_value = tag_value;

	return ident;
}

static void _asn1_ident_print(asn1_ident_t* ident) {
	const char* class = NULL;
	switch (ident->class) {
		case ASN1_IDENT_UNIVERSAL:
			class = "UNI";
			break;
		case ASN1_IDENT_APPLICATION:
			class = "APP";
			break;
		case ASN1_IDENT_CONTEXT_SPECIFIC:
			class = "CTX";
			break;
		case ASN1_IDENT_PRIVATE:
		default:
			class = "PRV";
			break;
	}
	const char* type = ident->is_constructed ? "Struct" : "Prim";
	printf("[Ident %s %s: 0x%02x]", class, type, ident->tag_value);
}

static uint32_t _parse_definite_length_length(tls_conn_t* state) {
	uint8_t len_byte1 = tls_read_byte(state);
	if (!(len_byte1 & (1 << 7))) {
		// Short form number
		// Bit 8 is zero, bits 7-1 hold the length
		// We can return it as-is
		return len_byte1;
	}
	// Long form number
	uint8_t bytes = len_byte1 & ~(1 << 7);
	assert(bytes >= 2 && bytes <= 127, "Invalid long-form size");
	if (bytes > 4) {
		assert(0, "Must support BigInt");
	}
	uint32_t accumulator = 0;
	for (uint8_t i = bytes; i > 0; i--) {
		uint8_t len_byte = tls_read_byte(state);
		accumulator |= (len_byte << ((i - 1) * 8));
	}
	return accumulator;
}

static uint32_t parse_integer(tls_conn_t* state, uint32_t depth) {
	_print_tabs(depth);

	asn1_ident_t* integer = _parse_definite_length_ident(state);
	assert(integer->class == ASN1_IDENT_UNIVERSAL, "Expecetd universal integer tag");
	assert(integer->tag_value == ASN1_UNIVERSAL_TAG_INTEGER, "Expecetd universal integer tag");

	uint8_t bytes = tls_read_byte(state);
	//assert(bytes >= 2 && bytes <= 127, "Invalid long-form size");
	if (bytes > 4) {
		assert(0, "Must support BigInt");
	}
	// TODO(PT): This is signed, two's complement... need to handle
	uint32_t accumulator = 0;
	for (uint8_t i = bytes; i > 0; i--) {
		uint8_t len_byte = tls_read_byte(state);
		accumulator |= (len_byte << ((i - 1) * 8));
	}

	printf("[INTEGER %d]\n", accumulator);
	return accumulator;
}

static void parse_certificate_version(tls_conn_t* state, uint32_t depth) {
	_print_tabs(depth);

	asn1_ident_t* version = _parse_definite_length_ident(state);
	assert(version->tag_value == X509_CERT_VERSION_TAG, "Version tag was wrong");
	printf("[Certificate version ");
	_asn1_ident_print(version);

	uint32_t length = _parse_definite_length_length(state);
	printf(" len=0x%04x]\n", length);

	parse_integer(state, depth + 1);
}

static void parse_certificate(tls_conn_t* state, uint32_t depth) {
	_print_tabs(depth);

	asn1_ident_t* ident = _parse_definite_length_ident(state);
	printf("[Certificate ");
	_asn1_ident_print(ident);
	uint32_t length = _parse_definite_length_length(state);
	printf(" len=0x%04x]\n", length);

	parse_certificate_version(state, depth + 1);
}

static void parse_certificate_sequence(tls_conn_t* state, uint32_t data_length, uint32_t depth) {
	_print_tabs(depth);

	asn1_ident_t* ident = _parse_definite_length_ident(state);
	printf("[Certificate sequence ");
	_asn1_ident_print(ident);
	uint32_t length = _parse_definite_length_length(state);
	printf(" len=0x%04x]\n", length);

	parse_certificate(state, depth+1);
}

void asn1_cert_parse(tls_conn_t* state, uint32_t cert_len) {
	printf("asn1_cert_parse %d %d\n", state->read_off, cert_len);

	parse_certificate_sequence(state, cert_len, 0);
	/*   Certificate ::= SIGNED SEQUENCE{
           version [0]     Version DEFAULT v1988,
           serialNumber    CertificateSerialNumber,
           signature       AlgorithmIdentifier,
           issuer          Name,
           validity        Validity,
           subject         Name,
           subjectPublicKeyInfo    SubjectPublicKeyInfo}
	*/
	// See "RFC1422 Certificate-Based Key Management" for more
	// Expect "constructed sequence"

}