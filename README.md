# Custom Memory Allocator (`smalloc`)

This project implements a simple **dynamic memory allocator** using a **first-fit strategy**.  
It provides functionality similar to `malloc` and `free` by managing a memory region obtained via `mmap` and tracking allocations through a doubly linked list of memory segments.

---

## 📖 Overview

The allocator manages memory using the following key concepts:

- **Memory Segments (`mem_seg_t`)**  
  Each segment contains:
  - `seg_size`: Total size of the segment (header + payload).
  - `in_use`: Boolean flag (1 = allocated, 0 = free).
  - `next` / `prev`: Links for maintaining a doubly linked list of free segments.

- **Free List**  
  A doubly linked list tracking all free segments. Segments are kept in ascending address order.

- **Allocation Strategy**  
  First-fit: The allocator searches the free list for the first segment large enough to satisfy the allocation.

- **Splitting and Coalescing**  
  - If a free segment is larger than needed, it is split into allocated and free parts.  
  - When freeing, adjacent free segments are merged (coalesced) to reduce fragmentation.

---

## 🛠 Functions

### 1. `int my_init(int region_size)`
Initializes the allocator by mapping a memory region using `mmap`.

- Rounds the requested region size up to the nearest page size (4 KB).  
- Creates a single initial free segment spanning the entire region.  
- Returns:
  - `0` on success  
  - `-1` on failure

---

### 2. `void *smalloc(int payload_size, Malloc_Status *stat)`
Allocates a block of memory.

- **Inputs:**
  - `payload_size`: Number of bytes requested.  
  - `stat`: Pointer to a status structure that records:
    - `success`: `1` if allocation succeeded, `0` otherwise.  
    - `payload_offset`: Offset of the allocated memory from the base address.  
    - `hops`: Number of steps taken in the free list search.  

- **Behavior:**
  - Aligns requested size to an 8-byte boundary.  
  - Uses first-fit to find a suitable segment.  
  - Splits a free segment if it is larger than needed.  
  - Returns pointer to usable memory, or `NULL` if allocation fails.

---

### 3. `void sfree(void *ptr)`
Frees a previously allocated block.

- Retrieves the segment header from the given pointer.  
- Marks the segment as free and reinserts it into the free list.  
- Calls `merge_adjacent()` to coalesce with neighboring free segments if possible.  

---

## ⚙️ Internal Helpers

- **`align_value(size, factor)`**  
  Rounds `size` up to the nearest multiple of `factor`.

- **`search_for_fit(needed_size, steps)`**  
  Traverses the free list to find the first fitting segment.  
  Tracks number of steps taken.

- **`add_to_free_list(seg)`**  
  Inserts a segment into the free list in sorted order (by memory address).

- **`remove_from_free_list(seg)`**  
  Disconnects a segment from the free list.

- **`merge_adjacent(seg)`**  
  Coalesces a free segment with its neighbors (if adjacent and free).  

---

## 🔍 Example Usage

```c
#include "smalloc.h"

int main() {
    Malloc_Status stat;
    
    // Initialize allocator with 64KB
    if (my_init(65536) < 0) {
        perror("Allocator initialization failed");
        return 1;
    }

    // Allocate 128 bytes
    void *ptr = smalloc(128, &stat);
    if (ptr != NULL && stat.success) {
        printf("Allocated 128 bytes at offset %lu\n", stat.payload_offset);
    }

    // Free memory
    sfree(ptr);

    return 0;
}
