//
// Created by Finnian Fallowfield on 25/01/2023.
//

#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

#include <os/xmalloc.h>
#include <os/smalloc.h>
#include <os/smalloc_test.h>
#include <os/mm.h>

void print_region(int region_id)
{
    // TODO: Should we lock free/page lists for this?
    if (regions[region_id] == NULL)
    {
        return;
    }
    printf("\nREGION: %i\n", region_id);

    pthread_mutex_lock(&(regions[region_id]->free_list_lock));

    // Get first allocation in page list
    struct page_list_node *next_alloc = regions[region_id]->page_list_head;
    int alloc_num = 1;

    while (next_alloc != NULL)
    {
        printf("Allocation %i: %i pages, totalling %i bytes\n", alloc_num, next_alloc->num_pages, PAGE_SIZE * next_alloc->num_pages);
        void *end = next_alloc->page_start + PAGE_SIZE * next_alloc->num_pages;
        struct mem_blk_header *next_blk = ((struct mem_blk_header *)(next_alloc->page_start));

        while (((void *)next_blk) != end)
        {
            if (is_alloc(next_blk))
            {
                printf("Used block: ");
            }
            else
            {
                printf("Free block: ");
            }

            size_t size = actual_blk_size(next_blk);

            printf("%lu bytes, \t\t@%p", size, next_blk);

            if (!is_alloc(next_blk))
            {
                printf(" -> %p", ((struct free_blk_header *)next_blk)->next);
            }

            printf("\n");

            next_blk = (struct mem_blk_header *)(((char *)next_blk) + size);
        }

        next_alloc = next_alloc->next;
        alloc_num++;
    }

    pthread_mutex_unlock(&(regions[region_id]->free_list_lock));
}

void print_regions()
{
    for (int i = 0; i < NUM_REGIONS; i++)
    {
        // printf("\nREGION %i\n", i);
        print_region(i);
    }
}

void write_ones_to_payload(void *start, size_t size)
{
    char *p = start;
    for (int i = 0; i < size; i++)
    {
        *p = '1';
        p++;
    }
}

void *smalloc_and_write_ones(size_t size, int region_id)
{
    void *ret = smalloc(size, region_id);
    // void *ret = thread_local_smalloc(size);
    write_ones_to_payload(ret, size);
    return ret;
}

void *smalloc_local_and_write_ones(size_t size)
{
    void *ret = thread_local_smalloc(size);
    write_ones_to_payload(ret, size);
    return ret;
}

bool check_payload_contains_only_ones(void *start, size_t size)
{
    char *p = start;
    for (int i = 0; i < size; i++)
    {
        if (*p != '1')
        {
            return false;
        }
        p++;
    }
    return true;
}

bool sfree_and_check_ones(void *start, size_t size, int region_id)
{
    bool ret = check_payload_contains_only_ones(start, size);
    region_sfree(start, region_id);
    //thread_local_sfree(start);
    return ret;
}

bool sfree_local_and_check_ones(void *start, size_t size) {
    bool ret = check_payload_contains_only_ones(start, size);
    thread_local_sfree(start);
    return ret;
}

int tests()
{
    printf("Running tests!\n");
    bool checks[12];

    size_t SEG1_SIZE = 2048;
    size_t SEG2_SIZE = 73;
    size_t SEG3_SIZE = 173;
    size_t SEG4_SIZE = 2000;
    size_t SEG5_SIZE = 2000;
    size_t SEG2_1_SIZE = 7000;
    size_t SEG2_2_SIZE = 2000;
    size_t SEG2_3_SIZE = 3000;
    size_t SEG2_4_SIZE = 50;
    size_t SEG3_1_SIZE = 10000;
    size_t SEG3_2_SIZE = 8;
    size_t SEG6_SIZE = 1000000; 

    void *seg1 = smalloc_and_write_ones(SEG1_SIZE, 0);
    void *seg2 = smalloc_and_write_ones(SEG2_SIZE, 0);
    checks[0] = sfree_and_check_ones(seg1, SEG1_SIZE, 0);
    void *seg3 = smalloc_and_write_ones(SEG3_SIZE, 0);
    void *seg4 = smalloc_and_write_ones(SEG4_SIZE, 0);
    void *seg5 = smalloc_and_write_ones(SEG5_SIZE, 0);
    void *seg2_1 = smalloc_and_write_ones(SEG2_1_SIZE, 0);
    void *seg2_2 = smalloc_and_write_ones(SEG2_2_SIZE, 0);
    checks[1] = sfree_and_check_ones(seg2_2, SEG2_2_SIZE, 0);
    void *seg2_3 = smalloc_and_write_ones(SEG2_3_SIZE, 0);
    void *seg2_4 = smalloc_and_write_ones(SEG2_4_SIZE, 0);
    void *seg3_1 = smalloc_and_write_ones(SEG3_1_SIZE, 0);
    void *seg3_2 = smalloc_and_write_ones(SEG3_2_SIZE, 0);

    // print_regions();
    checks[2] = sfree_and_check_ones(seg3, SEG3_SIZE, 0);
    //print_regions();
    checks[3] = sfree_and_check_ones(seg2, SEG2_SIZE, 0);
    //print_regions();
    checks[4] = sfree_and_check_ones(seg4, SEG4_SIZE, 0);
    checks[5] = sfree_and_check_ones(seg2_1, SEG2_1_SIZE, 0);
    //print_regions();
    checks[6] = sfree_and_check_ones(seg5, SEG5_SIZE, 0);
    //print_regions();
    checks[7] = sfree_and_check_ones(seg2_3, SEG2_3_SIZE, 0);
    checks[8] = sfree_and_check_ones(seg2_4, SEG2_4_SIZE, 0); 
    checks[9] = sfree_and_check_ones(seg3_1, SEG3_1_SIZE, 0);
    checks[10] = sfree_and_check_ones(seg3_2, SEG3_2_SIZE, 0);

    //print_regions();

    void *seg6 = smalloc_and_write_ones(SEG6_SIZE, 0);
    char **str = (char **)seg6;
    // *str = "Hello World";

    //print_regions();
    checks[11] = sfree_and_check_ones(seg6, SEG6_SIZE, 0);
    
    printf("Checks complete!\n");
    //print_regions();

    for (int i = 0; i < 12; i++)
    {
        printf("Check %d: %d\n", i, checks[i]);
    }
    
    return 0;
}