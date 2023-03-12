//
// Created by Finnian Fallowfield on 25/01/2023.
//

#ifndef _SMALLOC_TEST_H_
#define _SMALLOC_TEST_H_

#include "smalloc.h"

void print_region(int region_id);
void print_regions();
void write_ones_to_payload(void *start, size_t size);
bool check_payload_contains_only_ones(void *start, size_t size);
void *smalloc_and_write_ones(size_t size, int region_id);
bool sfree_and_check_ones(void *start, size_t size, int region_id);

#endif // _SMALLOC_TEST_H_
