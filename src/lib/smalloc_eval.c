#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

#include <os/xmalloc.h>
#include <os/smalloc.h>
#include <os/smalloc_test.h>
#include <os/smalloc_eval.h>
#include <os/mm.h>
#include <os/time.h>

struct test_settings
{
    void *(*alloc_func)(size_t size);
    void (*free_func)(void *payload_start);
    unsigned int num_allocs;
};

void *region_zero_smalloc(size_t size)
{
    return smalloc(size, 0);
}

void region_zero_sfree(void *payload_start)
{
    return region_sfree(payload_start, 0);
}

void *my_xmalloc(size_t size)
{
    return xmalloc_align(size, 4);
}

long test_with_threads(
    void (*test_func)(void *),
    struct test_settings *settings,
    unsigned int num_threads)
{
    printf("Running a test...\n");
    struct timeval start;
    struct timeval end;
    pthread_t *threads = thread_local_smalloc(sizeof(pthread_t) * num_threads);
    pthread_t test;
    pthread_t test2;

    gettimeofday(&start);
    for (int i = 0; i < num_threads; i++)
    {
        printf("Creating thread %i...\n", i);
        pthread_create(&threads[i], NULL, test_func, (void *)settings);
    }
    for (int i = 0; i < num_threads; i++)
    {
        printf("Joining thread %i...\n", i);
        pthread_join(threads[i], NULL);
    }
    gettimeofday(&end);

    unsigned long long end_us = (end.tv_sec * 1000000) + end.tv_usec;
    unsigned long long start_us = (start.tv_sec * 1000000) + start.tv_usec;
    return end_us - start_us;
}

void alloc_many_different_size_then_free(void *settings)
{
    unsigned long total = 0;
    printf("alloc_many_different_size_then_free starting\n");
    struct test_settings *_settings = (struct test_settings *)settings;
    void **allocations = _settings->alloc_func(sizeof(void *) * _settings->num_allocs);

    for (int i = 0; i < _settings->num_allocs; i++)
    {
        size_t size;
        int n = i % 100;
        if (n == 0)
        {
            size = 100000; // 100kb
        }
        else if (n <= 4)
        {
            size = 25000;
        }
        else if (n <= 19)
        {
            size = 10000;
        }
        else if (n <= 39)
        {
            size = 5000;
        }
        else if (n <= 59)
        {
            size = 1000;
        }
        else if (n <= 79)
        {
            size = 500;
        }
        else
        {
            size = 100;
        }
        allocations[i] = _settings->alloc_func(size);
    }

    // print_regions();

    for (int i = 0; i < _settings->num_allocs; i++)
    {
        _settings->free_func(allocations[i]);
    }

    printf("alloc_many_different_size_then_free complete\n");
}

void alloc_different_size_then_free(void *settings)
{
    printf("alloc_different_size_then_free starting\n");
    struct test_settings *_settings = (struct test_settings *)settings;
    void **allocations = _settings->alloc_func(sizeof(void *) * _settings->num_allocs);

    for (int i = 0; i < _settings->num_allocs; i++)
    {
        size_t size;
        int n = i % 100;
        if (n == 0)
        {
            size = 100000; // 100kb
        }
        else if (n <= 4)
        {
            size = 25000;
        }
        else if (n <= 19)
        {
            size = 10000;
        }
        else if (n <= 39)
        {
            size = 5000;
        }
        else if (n <= 59)
        {
            size = 1000;
        }
        else if (n <= 79)
        {
            size = 500;
        }
        else
        {
            size = 100;
        }
        allocations[i] = _settings->alloc_func(size);
        _settings->free_func(allocations[i]);
    }

    printf("alloc_different_size_then_free complete\n");
}

// Allocate all same size, then free all
void alloc_many_same_size_then_free(void *settings)
{
    printf("alloc_many_same_size_then_free starting\n");
    size_t size = 100;

    struct test_settings *_settings = (struct test_settings *)settings;
    void **allocations = _settings->alloc_func(sizeof(void *) * _settings->num_allocs);

    for (int i = 0; i < _settings->num_allocs; i++)
    {
        allocations[i] = _settings->alloc_func(size);
    }

    for (int i = 0; i < _settings->num_allocs; i++)
    {
        _settings->free_func(allocations[i]);
    }

    printf("alloc_many_different_size_then_free complete\n");
}

// Do 10 allocations, free, repeat
void alloc_several_free_several(void *settings)
{
    printf("alloc_several_free_several starting\n");

    struct test_settings *_settings = (struct test_settings *)settings;
    void **allocations = _settings->alloc_func(sizeof(void *) * _settings->num_allocs);

    allocations[0] = _settings->alloc_func(50);

    for (int i = 1; i < _settings->num_allocs; i++)
    {
        size_t size = 100;
        if (i % 10 == 0)
        {
            for (int j = i - 10; j < i; j++)
            {
                _settings->free_func(allocations[j]);
            }
            allocations[i] = _settings->alloc_func(50);

            continue;
        }

        if (i % 10 <= 2)
        {
            size = 100;
        }
        else if (i % 10 <= 4)
        {
            size = 200;
        }
        else if (i % 10 <= 6)
        {
            size = 400;
        }
        else if (i % 10 <= 8)
        {
            size = 800;
        }
        else if (i % 10 <= 10)
        {
            size = 1600;
        }

        allocations[i] = _settings->alloc_func(size);
    }
}

void do_test(unsigned int num_threads, unsigned int num_allocations)
{
    printf("Running tests!!\n");
    struct test_settings region_smalloc_settings = {
        thread_local_smalloc,
        thread_local_sfree,
        num_allocations};

    struct test_settings smalloc_settings = {
        region_zero_smalloc,
        region_zero_sfree,
        num_allocations};

    struct test_settings xmalloc_settings = {
        my_xmalloc,
        xfree,
        num_allocations};

    long region_smalloc_elapsed = test_with_threads(alloc_many_different_size_then_free, &region_smalloc_settings, num_threads);
    printf("Region-based Smalloc: %u allocations took %ld us\n", num_allocations, region_smalloc_elapsed);

    // long xmalloc_elapsed = test_with_threads(alloc_many_different_size_then_free, &xmalloc_settings, num_threads);

    // long smalloc_elapsed = test_with_threads(alloc_many_different_size_then_free, &smalloc_settings, num_threads);
    // printf("Smalloc: %u allocations took %ld us\n", num_allocations, smalloc_elapsed);
   

    // printf("Singleregion Smalloc: %u allocations took %ld us\n", num_allocations, smalloc_elapsed);
    // printf("xmalloc: %u allocations took %ld us\n", num_allocations, xmalloc_elapsed);
    print_info();
}