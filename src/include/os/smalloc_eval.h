#ifndef _SMALLOC_EVAL_H_
#define _SMALLOC_EVAL_H_

#include <stdio.h>
#include <stdbool.h>

struct test_settings;

void *region_zero_smalloc(size_t size);
void region_zero_sfree(void *payload_start);
long test_with_threads(void (*test_func)(void *),
                       struct test_settings *settings,
                       unsigned int num_threads);
void alloc_test(void *settings);
void do_test(unsigned int num_threads, unsigned int num_allocations);

#endif // _SMALLOC_EVAL_H_