// hunk.h -- primary game memory and allocators based on it

#ifndef HUNK_H
#define HUNK_H

#include "common.h"

/*
=========================================================================================================================

Hunk memory allocator is usually a very large piece of memory provided by system layer.
Hunk memory is captured by Hunk memory allocator, which is a double-ended stack-like container,
and it is used for temporary allocations, permanent allocations to stay till the program ends,
and allocations for being a blocks for other memory allocators, such as Zone memory allocator.

Hunk memory allocator is good for large allocations, does low fragmentation, and is the most
efficient and speedy allocator. It always zero-initialize allocated memory chunks, and these
allocations are always memory aligned. The only downside is a stack-like alloc/pop order requirement.

=========================================================================================================================
*/
void Hunk_Init(void *base, size_t size, double zpiece, size_t zminfrag);

void Hunk_Reset(void);
void Hunk_Clear(void);

size_t Hunk_Size(void);

size_t Hunk_LowMark(void);
size_t Hunk_HighMark(void);

void * Hunk_LowAlloc(size_t size);
void * Hunk_LowAllocNamed(size_t size, const char *name);

void * Hunk_HighAlloc(size_t size);
void * Hunk_HighAllocNamed(size_t size, const char *name);

void Hunk_LowPop(void);
void Hunk_LowPopToMark(size_t mark);

void Hunk_HighPop(void);
void Hunk_HighPopToMark(size_t mark);

void Hunk_Check(void);
void Hunk_Print(qboolean_t everyalloc);

/*
=========================================================================================================================

Zone memory allocator uses 30% of hunk memory as a generic-purpose heap.
So it is internally a two linked lists of free blocks and allocated blocks,
which lead to memory fragmentation with large size requests. The memory
can be allocated and deallocated in any imaginable order.

=========================================================================================================================
*/
void Zone_Init(size_t size, size_t zminfrag);    // zminfrag is a minimally required size of space between two blocks to marge them, UGLYPARAM means defaults

void * Zone_Alloc(size_t size);
void Zone_Free(void *addr);

void Zone_Check(void);
void Zone_Print(void);

/*
=========================================================================================================================

Cache memory allocator primary goal is to minimize used memory size
and avoid same data repeats, load and store.

=========================================================================================================================
*/
typedef void * cacheid_t;

void Cache_Init(void);

void * Cache_Alloc(cacheid_t *id, size_t size);
void Cache_Free(cacheid_t *id, void *addr);
void Cache_Flush(cacheid_t *id);

void Cache_Check(cacheid_t *id);
void Cache_Print(cacheid_t *id);

#endif // #ifndef HUNK_H
