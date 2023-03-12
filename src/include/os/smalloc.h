//
// Created by Finnian Fallowfield on 03/01/2023.
//

#ifndef _SMALLOC_H_
#define _SMALLOC_H_

#include <pthread.h>
#include <stdbool.h>

#define NUM_BINS 10
//#define PAGE_SIZE 4096
#define HEADER_SIZE sizeof(struct mem_blk_header)
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define MIN_BLK_SIZE ALIGN(HEADER_SIZE + sizeof(struct free_blk_header))

// TODO: Should this include region 0?
#define NUM_REGIONS 1000

struct region
{
    unsigned int id; // TODO: we might not need this
    // 0  1   2   3   4    5    6    7    8     9
    // up to:
    // 64 128 256 512 1024 2048 4096 8192 16384 MAX
    struct free_blk_header *first_frees[NUM_BINS];
    pthread_mutex_t free_list_lock;
    struct page_list_node *page_list_head;
    struct page_list_node *page_list_tail;
};

// Singly linked list of page allocations
// List nodes dynamically allocated in region 0
// Each node corresponds to one contiguous allocation of pages
// If user mallocs several pages worth of mem, this is one allocation
// and therefore one node with num_pages > 1
struct page_list_node
{
    void* page_start;
    int num_pages;
    struct page_list_node* next;
};

struct mem_blk_header
{
    size_t size;
};

struct free_blk_header
{
    size_t size;
    struct free_blk_header* prev;
    struct free_blk_header* next;
};

// Global statically declared array of region pointers
// TODO: could be a dynamic hashmap
struct region *regions[NUM_REGIONS];

unsigned int get_bin_index_from_size(size_t size);
void* fetch_page(unsigned int pages);
size_t ptr_distance(char* p1, char* p2);
bool is_alloc(struct mem_blk_header* header);
void init_region(unsigned int region_id);
void* smalloc(size_t size, unsigned int region_id);
void set_headers(struct mem_blk_header* blk_head, bool alloc, size_t size);
size_t actual_blk_size(struct mem_blk_header *blk_head);
struct free_blk_header* extend_region_heap(size_t blk_size, unsigned int region_id);
struct mem_blk_header* alloc_region_block(struct free_blk_header* free_blk, size_t blk_size, unsigned int region_id);
void region_coalesce(struct free_blk_header** free_blk, unsigned int region_id);
void region_sfree(void* payload_start, unsigned int region_id);
void *thread_local_smalloc(size_t size);
void thread_local_sfree(void* payload_start);
void check_and_init_regions(unsigned int region_id);
void put_blk_on_free_list(struct free_blk_header *blk, unsigned int region_id);
unsigned long long get_thread_id();

#endif //_SMALLOC_H_
