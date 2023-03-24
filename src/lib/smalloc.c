#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

#include <os/smalloc.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/time.h>

// #define DEBUG
#define STARDUST
#define FAST_COALESCING

#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

struct region region_zero = {
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    PTHREAD_MUTEX_INITIALIZER,
    NULL,
    NULL}; // globally declared region_zero

pthread_mutex_t regions_lock = PTHREAD_MUTEX_INITIALIZER;

unsigned int steps = 0;
unsigned int extends = 0;
unsigned int allocs = 0;

unsigned long long get_thread_id()
{
#ifdef STARDUST
    return (unsigned long long)sched_current_thread()->id;
#else
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return (unsigned long long)tid;
#endif
}

// Return a pointer to a new empty 4k page
void *fetch_page(unsigned int pages)
{
#ifdef STARDUST
    // alloc_pages takes in a power i, where 2^(i) = number of pages to fetch
    // we therefore have to compute the ceiling(log2(pages)) to pass to it
    // If we had access to C math header could do:
    // int order = ceil(log(pages) / log(2));
    int order = 1;
    if (pages != 1)
    {
        // when pages = 1, builtin_clz(pages - 1) will be undefined (pages - 1 = 0)
        // TODO: WARNING: GCC ONLY
        order = 31 - __builtin_clz(pages - 1) + 1; // builtin_clz is gcc function for counting leading zeroes in variable
    }
    void *ptr = alloc_pages(order);
    return ptr;
#else
    void *ptr = malloc(PAGE_SIZE * pages);
    return ptr;
#endif
}
unsigned int get_bin_index_from_size(size_t size)
{
    if (size <= 256)
    {
        int i = ((int)(size - 1) / 16) - 2;
        if (i <= 0) {
            return 0;
        }
        return i;
    }
    if (size <= 512)
    {
        return 14;
    }
    else if (size <= 1024)
    {
        return 15;
    }
    else if (size <= 2048)
    {
        return 16;
    }
    else if (size <= 4096)
    {
        return 17;
    }
    else if (size <= 8192)
    {
        return 18;
    }
    return 19;
}

size_t ptr_distance(char *p1, char *p2)
{
    return p2 - p1;
}

// Check if mem_blk_header is allocated (check if final bit of size is set)
bool is_alloc(struct mem_blk_header *header)
{
    return ((header->size & 1) == 1);
}

void put_blk_on_free_list(struct free_blk_header *blk, unsigned int region_id)
{
    unsigned int index = get_bin_index_from_size(blk->size);
    DEBUG_PRINT("Region %i: Placing blk %p on free list, bin index = %u, size = %lu!\n", region_id, blk, index, blk->size);

    blk->prev = NULL;

    DEBUG_PRINT("Region %i: Attaching blk to prev first_free: %p\n", region_id, regions[region_id]->first_frees[index]);
    blk->next = regions[region_id]->first_frees[index];

    if (regions[region_id]->first_frees[index])
    {
        DEBUG_PRINT("Region %i: Placing at start of free list, replacing first_free!\n", region_id);
        regions[region_id]->first_frees[index]->prev = blk;
    }

    regions[region_id]->first_frees[index] = blk;
}

void check_and_init_regions(unsigned int region_id)
{
    if (regions[0] == NULL)
    {
        pthread_mutex_lock(&regions_lock);
        if (regions[0] == NULL)
        {
            init_region(0);
        }
        pthread_mutex_unlock(&regions_lock);
    }
    if (regions[region_id] == NULL && region_id != 0)
    {
        pthread_mutex_lock(&regions_lock);
        if (regions[region_id] == NULL)
        {
            init_region(region_id);
        }
        pthread_mutex_unlock(&regions_lock);
    }
}

// If block is set i.e. size = actual_size + 1, return actual_size
size_t actual_blk_size(struct mem_blk_header *blk_head)
{
    // clear last bit as allocated
    if (is_alloc(blk_head))
    {
        return blk_head->size & ~1L;
    }
    else
    {
        return blk_head->size;
    }
}

// Set both headers (at start and end of block) with given size and alloc
void set_headers(struct mem_blk_header *blk_head, bool alloc, size_t size)
{
    size_t new_size;
    size_t actual_size;

    // If block is currently allocated, actual size = size with last bit cleared
    if (is_alloc(blk_head))
    {
        actual_size = size & ~1L; // clear last bit
    }
    else
    {
        actual_size = size;
    }

    if (alloc)
    {
        new_size = size | 1; // set last bit
    }
    else
    {
        new_size = size & ~1L; // clear last bit
    }

    blk_head->size = new_size;
    ((struct mem_blk_header *)((char *)blk_head + actual_size - HEADER_SIZE))->size = new_size;
}

// Initiate region
void init_region(unsigned int region_id)
{
    unsigned long long tid = get_thread_id();
    DEBUG_PRINT("Region %i: Entering init, thread id = %llu\n", region_id, tid);
    if (region_id == 0)
    {
        regions[region_id] = &region_zero;
    }
    else
    {
        regions[region_id] = smalloc(sizeof(struct region), 0); // Region struct is put in region 0 mem
    }
    int status = pthread_mutex_init(&(regions[region_id]->free_list_lock), NULL);
    regions[region_id]->id = region_id;

    for (int i = 0; i < NUM_BINS; i++)
    {
        regions[region_id]->first_frees[i] = NULL;
    }
    regions[region_id]->page_list_head = NULL;
    regions[region_id]->page_list_tail = NULL;
    DEBUG_PRINT("Region %i: Exiting init, thread id = %llu, mutex init val = %i\n", region_id, tid, status);
}

// Extend heap of given region by fetching more pages and maintaining correct block and free/page list structures
struct free_blk_header *extend_region_heap(size_t blk_size, unsigned int region_id)
{
    extends++;
    unsigned int num_pages = (blk_size + (PAGE_SIZE - 1)) / PAGE_SIZE; // TODO: Replace with ceil function from math.h
    struct page_list_node *new_page_node;
    struct free_blk_header *new_free_blk;
    size_t page_list_node_blk_size = ALIGN(sizeof(struct page_list_node) + 2 * sizeof(struct mem_blk_header));

    if (region_id == 0)
    {
        size_t ext_blk_size = ALIGN(blk_size + page_list_node_blk_size);
        num_pages = (ext_blk_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
    }

    DEBUG_PRINT("Region %i: Fetching %i pages\n", region_id, num_pages);
    new_free_blk = fetch_page(num_pages);
    set_headers((struct mem_blk_header *)new_free_blk, false, PAGE_SIZE * num_pages);
    new_free_blk->region_id = region_id;

    // Place block in correct place on free list
    put_blk_on_free_list(new_free_blk, region_id);

    // If we're extending region 0s heap, we need to extend it a bit more so
    // there is space for the new page's page list node which is also stored in
    // region 0
    if (region_id == 0)
    {
        DEBUG_PRINT("Region %i: Creating new page list node from existing block!\n", region_id);
        struct free_blk_header *extra_free_blk = new_free_blk;
        // assign extra space in free block to a new page node
        new_page_node = (struct page_list_node *)((char *)
                                                      alloc_region_block(extra_free_blk, page_list_node_blk_size, 0) +
                                                  HEADER_SIZE);
        new_free_blk = (struct free_blk_header *)((char *)extra_free_blk + page_list_node_blk_size);
        new_page_node->page_start = extra_free_blk;
    }
    else
    {
        DEBUG_PRINT("Region %i: Allocating space for new page list node!\n", region_id);
        // otherwise use smalloc to allocate mem for page list node
        new_page_node = smalloc(sizeof(struct page_list_node), 0);
        new_page_node->page_start = new_free_blk;
    }

    new_page_node->num_pages = num_pages;
    new_page_node->next = NULL;

    // If no pages assigned to region create page list
    if (regions[region_id]->page_list_head == NULL)
    {
        regions[region_id]->page_list_head = new_page_node;
    }
    else
    {
        // Otherwise add to tail of page list
        // Page list is not in memory address order, just in order of allocation time
        regions[region_id]->page_list_tail->next = new_page_node;
    }
    regions[region_id]->page_list_tail = new_page_node; // update tail

    DEBUG_PRINT("Region %i: Heap extended!\n", region_id);

    return new_free_blk;
}

// From a given free block, mark allocated, update free list and region structures and return
struct mem_blk_header *alloc_region_block(struct free_blk_header *free_blk, size_t blk_size, unsigned int region_id)
{
    DEBUG_PRINT("Region %i: Allocating %lu bytes to block %p of size %lu!\n", region_id, blk_size, free_blk, free_blk->size);
    void *end_of_free_blk = (char *)free_blk + free_blk->size;
    size_t extra_size = ptr_distance((char *)free_blk + blk_size, end_of_free_blk); // Should already be aligned
    unsigned int bin_index = get_bin_index_from_size(free_blk->size);

    // remove block from free list
    if (free_blk->prev)
    {
        (free_blk->prev)->next = free_blk->next;
    }
    if (free_blk->next)
    {
        (free_blk->next)->prev = free_blk->prev;
    }
    // If this block was the first in free list
    if (regions[region_id]->first_frees[bin_index] == free_blk)
    {
        regions[region_id]->first_frees[bin_index] = free_blk->next;
    }

    if (extra_size > MIN_BLK_SIZE)
    {
        struct free_blk_header *new_free_blk = (struct free_blk_header *)((char *)free_blk + blk_size);
        new_free_blk->size = extra_size;
        new_free_blk->region_id = region_id;
        ((struct mem_blk_header *)((char *)new_free_blk + extra_size - HEADER_SIZE))->size = extra_size;
        DEBUG_PRINT("Region %i: Split block %p, created new block of %lu bytes at %p!\n", region_id, free_blk, extra_size, new_free_blk);
        put_blk_on_free_list(new_free_blk, region_id);
    }
    else
    {
        blk_size += extra_size;
    }

    DEBUG_PRINT("Region %i: Allocated block %p!\n", region_id, free_blk);
    set_headers((struct mem_blk_header *)free_blk, true, blk_size); // Mark allocated

    return (struct mem_blk_header *)free_blk;
}

// Coalesce a free block with its left and right neighbour where appropriate
void region_coalesce(struct free_blk_header **free_blk, unsigned int region_id)
{
    struct free_blk_header *left_blk = NULL;
    struct free_blk_header *right_blk = NULL;
    unsigned int left_bin;
    unsigned int right_bin;

    struct mem_blk_header *left_tail = (struct mem_blk_header *)((char *)*free_blk - HEADER_SIZE);
    if (!is_alloc(left_tail) && left_tail->size != 0)
    {
        left_bin = get_bin_index_from_size(left_tail->size);
#ifdef FAST_COALESCING
        struct free_blk_header *left_head = (struct free_blk_header*)((char*) left_tail + HEADER_SIZE - left_tail->size);
        if ((unsigned)*free_blk % PAGE_SIZE != 0 && left_head->region_id == region_id)
        {
            DEBUG_PRINT("Fast coalesce left successful! ptr = %p\n", *free_blk);
            left_blk = left_head;
        }
        else {
            DEBUG_PRINT("Fast coalesce left failed! ptr = %p\n", *free_blk);
        }
#else
        DEBUG_PRINT("Region %i: Traversing free list with bin index = %u looking for left_blk!\n", region_id, left_bin);
        struct free_blk_header *next_free = regions[region_id]->first_frees[left_bin];

        while (next_free)
        {
            if ((char *)next_free + next_free->size == (char *)left_tail)
            {
                left_blk = next_free;
                DEBUG_PRINT("Region %i: Found left blk, address = %p\n", region_id, left_blk);
            }
            next_free = next_free->next;
        }
#endif
    }

    struct mem_blk_header *right_head = (struct mem_blk_header *)((char *)*free_blk + (*free_blk)->size);
    if (!is_alloc(right_head) && right_head->size != 0)
    {
        right_bin = get_bin_index_from_size(right_head->size);
#ifdef FAST_COALESCING
        if ((unsigned)right_head % PAGE_SIZE != 0 && ((struct free_blk_header*) right_head)->region_id == region_id)
        {
            DEBUG_PRINT("Fast coalesce right successful! ptr = %p\n", right_head);
            right_blk = (struct free_blk_header*) right_head;
        }
        else {
            DEBUG_PRINT("Fast coalesce right failed! right_head address = %p\n", right_head);
        }
#else
        DEBUG_PRINT("Region %i: Traversing free list with bin index = %u looking for right_blk!\n", region_id, right_bin);
        struct free_blk_header *next_free = regions[region_id]->first_frees[right_bin];

        while (next_free)
        {
            if ((char *)next_free == (char *)right_head)
            {
                right_blk = (struct free_blk_header *)right_head;
                DEBUG_PRINT("Region %i: Found right blk, address = %p\n", region_id, right_blk);
            }
            next_free = next_free->next;
        }
#endif
    }

    size_t new_blk_size = (*free_blk)->size;

    if (left_blk)
    {
        DEBUG_PRINT("Region %i: Coalescing left with blk: %p\n", region_id, left_blk);
        // left_blk must have a prev
        // Remove left blk
        if (left_blk->prev)
        {
            left_blk->prev->next = left_blk->next;
        }
        if (left_blk->next)
        {
            left_blk->next->prev = left_blk->prev;
        }
        if (regions[region_id]->first_frees[left_bin] == left_blk)
        {
            regions[region_id]->first_frees[left_bin] = regions[region_id]->first_frees[left_bin]->next;
        }

        // TODO: should we do left_blk->prev = NULL, ->next = NULL
        *free_blk = left_blk;
        new_blk_size += left_blk->size;
    }

    if (right_blk)
    {
        DEBUG_PRINT("Region %i: Coalescing right with blk: %p\n", region_id, right_blk);
        // right_blk must have prev cos free_blk is first_free so guaranteed comes before
        // Remove right blk
        if (right_blk->prev)
        {
            right_blk->prev->next = right_blk->next;
        }
        if (right_blk->next)
        {
            right_blk->next->prev = right_blk->prev;
        }
        if (regions[region_id]->first_frees[right_bin] == right_blk)
        {
            regions[region_id]->first_frees[right_bin] = regions[region_id]->first_frees[right_bin]->next;
        }

        DEBUG_PRINT("Region %i: Increasing block size to coalesce right\n", region_id);
        new_blk_size += right_blk->size;
    }

    set_headers((struct mem_blk_header *)*free_blk, false, new_blk_size);
}

void *smalloc(size_t size, unsigned int region_id)
{
    unsigned long long tid = get_thread_id();
    check_and_init_regions(region_id);
    pthread_mutex_lock(&(regions[region_id]->free_list_lock));
    allocs++;

    void *return_mem = NULL;
    size_t blk_size = ALIGN(size + 2 * sizeof(struct mem_blk_header));
    if (blk_size < MIN_BLK_SIZE)
        blk_size = MIN_BLK_SIZE; // If size is below min, set to min
    DEBUG_PRINT("Region %i: Entering smalloc -> allocating %lu bytes from thread id = %llu\n", region_id, blk_size, tid);

    unsigned int bin_index = get_bin_index_from_size(blk_size);

    // Find a block large enough in free list to allocate to
    for (unsigned int i = bin_index; i < NUM_BINS && !return_mem; i++)
    {
        // DEBUG_PRINT("Region %i: Traversing free list with bin index = %u!\n", region_id, i);
        struct free_blk_header *next_free = regions[region_id]->first_frees[i];

        while (next_free)
        {
            steps++;
            // DEBUG_PRINT("Region %i: Looking at block at %p of size %lu\n", region_id, next_free, next_free->size);
            if (next_free->size > blk_size)
            {
                DEBUG_PRINT("Region %i: Found block at %p of size %lu in bin %u\n", region_id, next_free, next_free->size, i);
                return_mem = alloc_region_block(next_free, blk_size, region_id);
                break;
            }
            next_free = next_free->next;
        }
    }

    if (!return_mem)
    {
        // We didn't find a free block large enough, must extend heap and alloc from result
        DEBUG_PRINT("Region %i: No block found! Extending heap!\n", region_id);
        return_mem = alloc_region_block(extend_region_heap(blk_size, region_id), blk_size, region_id);
    }
    // found:

    pthread_mutex_unlock(&(regions[region_id]->free_list_lock));
    DEBUG_PRINT("Region %i: Exiting smalloc, thread id = %llu\n", region_id, tid);

    // Return pointer to the start of payload, i.e. skipping over header
    return (void *)(((char *)return_mem) + HEADER_SIZE);
}

void *thread_local_smalloc(size_t size)
{
    return smalloc(size, get_thread_id());
}

void thread_local_sfree(void *payload_start)
{
    region_sfree(payload_start, get_thread_id());
}

// Free a block
void region_sfree(void *payload_start, unsigned int region_id)
{
    unsigned long long tid = get_thread_id();
    pthread_mutex_lock(&(regions[region_id]->free_list_lock));
    DEBUG_PRINT("Region %i: Entering sfree, thread id = %llu, freeing %p\n", region_id, tid, payload_start);
    // Get the start of block header from *payload with pointer arithmetic
    struct free_blk_header *blk_to_free = (struct free_blk_header *)(((char *)payload_start) - HEADER_SIZE);
    // Mark unallocated
    set_headers((struct mem_blk_header *)blk_to_free, false, blk_to_free->size);

    region_coalesce(&blk_to_free, region_id); // Now coalesce the free block with blocks to left and right
    blk_to_free->region_id = region_id;
    put_blk_on_free_list(blk_to_free, region_id);

    pthread_mutex_unlock(&(regions[region_id]->free_list_lock));
    DEBUG_PRINT("Region %i: Exiting sfree, thread id = %llu\n", region_id, tid);
}

void print_info() {
    printf("steps: %i extends: %i allocs: %i\n", steps, extends, allocs);
}