#ifndef SHIMS_H
#define SHIMS_H

#include <stdint.h>

#include <libutils/assert.h>
#include <libutils/array.h>
#include <agx/lib/shapes.h>
#include <libport/libport.h>

uint32_t html_read_tcp_stream(uint32_t conn_descriptor, uint8_t* buf, uint32_t len);

#endif