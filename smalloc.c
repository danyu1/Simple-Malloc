#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "smalloc.h"

/*
 * mem_seg_t represents one contiguous memory segment.
 * It holds the total size of the segment (including header),
 * a flag indicating if it is currently in use, and pointers
 * for linking free segments.
 */
typedef struct mem_seg {
    int seg_size;             //header + payload = size of seg
    int in_use;
    struct mem_seg *next;     //point to next free seg
    struct mem_seg *prev;     //point to prev free seg
} mem_seg_t;

//global vars
static void *base_address = NULL;
static mem_seg_t *free_list_head = NULL;
static int total_region = 0;

//round to nearest multiple factor
static int align_value(int size, int factor) {
    return ((size + factor - 1) / factor) * factor;
}

//implement first fit
static mem_seg_t *search_for_fit(int needed_size, int *steps) {
    mem_seg_t *current = free_list_head;
    *steps = 0;
    while (current != NULL) {
        if (current->seg_size >= needed_size) {
            return current;
        }
        current = current->next;
        (*steps)++;
    }
    return NULL;
}

//insert a seg into the free list. insertion is done so that the list remains ordered by addresses
static void add_to_free_list(mem_seg_t *seg) {
    seg->in_use = 0;
if (free_list_head == NULL) {
        free_list_head = seg;
        seg->next = seg->prev = NULL;
        return;
    }
    //mew seg comes before the current head
    if (seg < free_list_head) {
        seg->next = free_list_head;
        seg->prev = NULL;
        free_list_head->prev = seg;
        free_list_head = seg;
        return;
    }
    //locate proper position based on mem address
    mem_seg_t *runner = free_list_head;
    while (runner->next != NULL && runner->next < seg) {
        runner = runner->next;
    }
    seg->next = runner->next;
    if (runner->next != NULL) {
        runner->next->prev = seg;
    }
    runner->next = seg;
    seg->prev = runner;
}

//disconnect a seg from the free list
static void remove_from_free_list(mem_seg_t *seg) {
    if (seg == free_list_head) {
        free_list_head = seg->next;
        if (free_list_head != NULL) {
            free_list_head->prev = NULL;
        }
    } else {
        if (seg->prev != NULL) {
            seg->prev->next = seg->next;
        }
        if (seg->next != NULL) {
            seg->next->prev = seg->prev;
        }
    }
    seg->next = seg->prev = NULL;
}

//merge neighboring free segments, then scans the free list to see if a seg precedes the current one
static mem_seg_t *merge_adjacent(mem_seg_t *seg) {
    mem_seg_t *result = seg;
    mem_seg_t *next_seg = (mem_seg_t *)((char *)seg + seg->seg_size);

    //merge with next seg if you can
    if ((char *)next_seg < (char *)base_address + total_region &&
        next_seg->in_use == 0) {
        remove_from_free_list(next_seg);
        seg->seg_size += next_seg->seg_size;
    }

    //Look for a free segment immediately preceding seg
    mem_seg_t *prev_seg = NULL;
    mem_seg_t *temp = free_list_head;
    while (temp != NULL) {
        mem_seg_t *possible_next = (mem_seg_t *)((char *)temp + temp->seg_size);
        if (possible_next == seg) {
            prev_seg = temp;
            break;
        }
        temp = temp->next;
    }

    //merge
    if (prev_seg != NULL) {
        remove_from_free_list(prev_seg);
        remove_from_free_list(seg);
        prev_seg->seg_size += seg->seg_size;
        add_to_free_list(prev_seg);
        result = prev_seg;
    }

    return result;
}


int my_init(int region_size) {
    int page_sz = 4096;
    int adjusted = align_value(region_size, page_sz);

    int fd = open("/dev/zero", O_RDWR);
    if (fd < 0) {
        return -1;
    }

    base_address = mmap(NULL, adjusted, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
  
    if (base_address == MAP_FAILED) {
        return -1;
    }

    /* Establish one initial free segment covering the entire region */
    mem_seg_t *initial_seg = (mem_seg_t *)base_address;
    initial_seg->seg_size = adjusted;
    initial_seg->in_use = 0;
    initial_seg->next = initial_seg->prev = NULL;

    free_list_head = initial_seg;
    total_region = adjusted;

    return 0;
}

void *smalloc(int payload_size, Malloc_Status *stat) {
    if (base_address == NULL) {
        stat->success = 0;
        stat->payload_offset = -1;
        stat->hops = -1;
        return NULL;
    }

    int hdr_size = sizeof(mem_seg_t);
    int total_required = hdr_size + payload_size;
    total_required = align_value(total_required, 8);

    int step_counter = 0;
    mem_seg_t *target = search_for_fit(total_required, &step_counter);

    if (target == NULL) {
        stat->success = 0;
        stat->payload_offset = -1;
        stat->hops = -1;
        return NULL;
    }

    int remaining_space = target->seg_size - total_required;
    remove_from_free_list(target);

    //if there is a large enough amount of extra space, split the seg into allocated and free part
    if (remaining_space >= (int)sizeof(mem_seg_t)) {
        mem_seg_t *free_seg = (mem_seg_t *)((char *)target + total_required);
        free_seg->seg_size = remaining_space;
        free_seg->in_use = 0;
        free_seg->next = free_seg->prev = NULL;

        target->seg_size = total_required;
        add_to_free_list(free_seg);
    }

    target->in_use = 1;
    void *usable_mem = (char *)target + hdr_size;

    stat->success = 1;
    stat->payload_offset = (unsigned long)usable_mem - (unsigned long)base_address;
    stat->hops = step_counter;
                                                                                      return usable_mem;
}                                                                                 
void sfree(void *ptr) {
    if (ptr == NULL || base_address == NULL) {
        return;
    }

    //retrieve the seg header using pointer arithmetic
    mem_seg_t *seg = (mem_seg_t *)((char *)ptr - sizeof(mem_seg_t));

    if (seg->in_use == 0) {
        return;
    }

    add_to_free_list(seg);
    merge_adjacent(seg);
}
