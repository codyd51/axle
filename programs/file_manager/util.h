#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlibadd/array.h>

#include "vfs.h"

bool str_ends_with(char* str, char* suffix);
bool str_ends_with_any(char* str, char* suffixes[]);
array_t* str_split(char* a_str, const char a_delim);

void print_tabs(uint32_t count);
void print_fs_tree(fs_node_t* node, uint32_t depth);

uint32_t depth_first_search__idx(fs_base_node_t* parent, fs_base_node_t* find, uint32_t sum, bool* out_found);

void launch_amc_service_if_necessary(const char* service_name);

#endif
