#ifndef _smalloc_H_
#define _smalloc_H_



typedef struct Malloc_Status{
    int success;            // 1 if success. 0 if failed.
    int payload_offset;     // Offset of the payload in the heap (in Bytes).
                            //   This should be (unsigned long)payload_pointer - (unsigned long)start_of_heap.
                            //   If malloc failed, then this should be -1.
    int hops;               // Number of hops it takes to find the first-fit block.
                            //   e.g. hops = 0 if the first free block (i.e. the head) of the free list satisfies.
                            //        hops = 1 if the second free block of the free list satisfies.
                            //   hops = -1 if malloc failed.
} Malloc_Status;

int my_init(int size_of_region);
void *smalloc(int size_of_payload, Malloc_Status* status);
void sfree(void *ptr);

#endif
