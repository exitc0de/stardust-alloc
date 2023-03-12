#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

#include <os/xmalloc.h>
#include <os/smalloc.h>
#include <os/smalloc_test.h>
#include <os/smalloc_eval.h>
#include <os/mm.h>
#include <os/time.h>

struct test_settings {
    void*(*alloc_func)(size_t size);
    void (*free_func)(void * payload_start);
    unsigned int num_allocs;
};

void *region_zero_smalloc(size_t size) {
    return smalloc(size, 0);
}

void region_zero_sfree(void* payload_start) {
    return region_sfree(payload_start, 0);
}

void *my_xmalloc(size_t size) {
    return xmalloc_align(size, 4);
}

long test_with_threads(
    void (*test_func)(void *), 
    struct test_settings* settings,
    unsigned int num_threads) 
{
    printf("Running a test...\n");
    struct timeval start;
    struct timeval end;
    pthread_t *threads = thread_local_smalloc(sizeof(pthread_t) * num_threads);
    pthread_t test;
    pthread_t test2;

    gettimeofday(&start);
    for(int i = 0; i < num_threads; i++) {
        printf("Creating thread %i...\n", i);
        pthread_create(&threads[i], NULL, test_func, (void*) settings);
    }
    for(int i = 0; i < num_threads; i++) {
        printf("Joining thread %i...\n", i);
        pthread_join(threads[i], NULL);
    }
    gettimeofday(&end);

    unsigned long long end_us = (end.tv_sec * 1000000) + end.tv_usec;
    unsigned long long start_us = (start.tv_sec * 1000000) + start.tv_usec;
    return end_us - start_us;
}

void alloc_test(void *settings)
{
    unsigned long total = 0;
    struct timeval start;
    struct timeval end;
    printf("alloc_test starting\n");
    struct test_settings* _settings = (struct test_settings*) settings;
    gettimeofday(&start);
    void **allocations = _settings->alloc_func(sizeof(void *) * _settings->num_allocs);
    gettimeofday(&end);
    unsigned long long end_us = (end.tv_sec * 1000000) + end.tv_usec;
    unsigned long long start_us = (start.tv_sec * 1000000) + start.tv_usec;
    printf("Initial array creation took %llu us\n", end_us - start_us);

    for(int i = 0; i < _settings->num_allocs; i++) {
        size_t size;
        int n = i % 100;
        if(n == 0) {
            size = 100000; // 100kb
        }
        else if(n <= 4) {
            size = 25000;
        } 
        else if(n <= 19) {
            size = 10000;
        }
        else if(n <= 39) {
            size = 5000;
        }
        else if(n <= 59) {
            size = 1000;
        }
        else if(n <= 79) {
            size = 500;
        }
        else {
            size = 100;
        }
        gettimeofday(&start);
        allocations[i] = _settings->alloc_func(size);
        gettimeofday(&end);
        end_us = (end.tv_sec * 1000000) + end.tv_usec;
        start_us = (start.tv_sec * 1000000) + start.tv_usec;
        //printf("%llu us, size = %lu\n", end_us - start_us, size);
        total += end_us - start_us;
    }

    //print_regions();
    gettimeofday(&start);
    for(int i = 0; i < _settings->num_allocs; i++) {
        _settings->free_func(allocations[i]);
    }
    gettimeofday(&end);
    end_us = (end.tv_sec * 1000000) + end.tv_usec;
    start_us = (start.tv_sec * 1000000) + start.tv_usec;


    printf("alloc_test complete\n");
    printf("Allocation total time: %lu us\n", total);
    printf("Freeing total time: %lu us\n", end_us - start_us);
}

void do_test(unsigned int num_threads, unsigned int num_allocations) {
    printf("Running tests!!\n");
    struct test_settings region_smalloc_settings = {
        thread_local_smalloc,
        thread_local_sfree,
        num_allocations
    };

    struct test_settings smalloc_settings = {
        region_zero_smalloc,
        region_zero_sfree,
        num_allocations
    };

    struct test_settings xmalloc_settings = {
        my_xmalloc,
        xfree,
        num_allocations
    };

    //long region_smalloc_elapsed = test_with_threads(alloc_test, &region_smalloc_settings, num_threads);
    //long smalloc_elapsed = test_with_threads(alloc_test, &smalloc_settings, num_threads);
    long xmalloc_elapsed = test_with_threads(alloc_test, &xmalloc_settings, num_threads);

    //printf("Region-based Smalloc: %u allocations took %ld us\n", num_allocations, region_smalloc_elapsed);
    //printf("Single-region Smalloc: %u allocations took %ld us\n", num_allocations, smalloc_elapsed);
    printf("xmalloc: %u allocations took %ld us\n", num_allocations, xmalloc_elapsed);
}