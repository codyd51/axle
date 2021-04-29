#ifndef SHIMS_H
#define SHIMS_H

#include <stdint.h>

#include <stdlibadd/assert.h>
#include <stdlibadd/array.h>
#include <agx/lib/shapes.h>

uint32_t html_read_tcp_stream(uint32_t conn_descriptor, uint8_t* buf, uint32_t len);

#endif