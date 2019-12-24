// com_alloc.c -- systems to alloc and manage memory

#include "common.h"
#include "mathlib.h"

/*
============================================================================================================

Hunk Memory Stack

============================================================================================================
*/
#define MAXHUNKNAME   32
#define HUNKCHECKPOW  1024
#define HUNKALIGNMENT 16
#define HUNKSENTINAL  0x4fba8fcd
typedef struct {
	unsigned sentinal;
	size_t   size;
	char     name[MAXHUNKNAME];
} hunkheader_t;
static byte_t *hunk_base, *hunk_end;
static size_t hunk_size, hunk_used_low, hunk_used_high;
static unsigned hunk_counter;
static criticalcode_t hunkcriticalcode;

void Hunk_CheckGeneral(void);
void Cache_FreeLow(size_t mark);   // these are in cache code way down below
void Cache_FreeHigh(size_t mark);

/*
=================
Hunk_Init

membase must point to a preallocated block of contiguous memory that the program will be able to use only
memsize must contain an exact size in bytes of the membase pointed memory block described above

zpiece is a percent of size to take for the zone memory allocator, 0 means defaults
zminfrag is a minimally required size of space between two blocks to marge them, UGLYPARAM means defaults
=================
*/
void Hunk_Init(void *membase, size_t memsize, double zpiece, size_t zminfrag)
{
	if (!base || size == 0)
		Sys_Error("Hunk_Init: bad params");
		
	//
	// process heap check
	//
	Sys_HeapCheck();
	
	//
	// hunk memory init
	//
	hunk_size = size;
	hunk_base = base;
	hunk_end = hunk_base + hunk_size;

	//
	// hunk using memory systems init
	//	
	Zone_Init((double)Hunk_Size() * zpiece, zminfrag);
	
	Cache_Init();

	//
	// done
	//
	COM_DevPrintf("Hunk and subsystems initialized\n");
}

/*
=================
Hunk_Reset
=================
*/
void Hunk_Reset(void)
{
	Hunk_LowPopToMark(0);
	Hunk_HighPopToMark(0);
	Hunk_Clear();
}

/*
=================
Hunk_Clear
=================
*/
void Hunk_Clear(void)
{
	EnterCriticalCode(&hunkcriticalcode);
	
	Q_memset(hunk_base, 0, hunk_size);
	
	LeaveCriticalCode(&hunkcriticalcode);
}

/*
=================
Hunk_Size
=================
*/
size_t Hunk_Size(void)
{
	MemoryBarrierForWrite();
	return hunk_size;
}

/*
=================
Hunk_LowMark
=================
*/
size_t Hunk_LowMark(void)
{
	MemoryBarrierForWrite();
	return hunk_used_low;
}

/*
=================
Hunk_HighMark
=================
*/
size_t Hunk_HighMark(void)
{
	MemoryBarrierForWrite();
	return hunk_used_high;
}

/*
=================
Hunk_LowAlloc
=================
*/
void * Hunk_LowAlloc(size_t size)
{
	return Hunk_LowAlloc(size, "unknown");
}

/*
=================
Hunk_LowAllocNamed
=================
*/
void Cache_FreeLow(size_t mark);
void * Hunk_LowAllocNamed(size_t size, const char *name)
{
	hunkheader_t *h;
	void *out;

#ifdedf PARANOID
	if (size == 0 || !name || !name[0])
		Sys_Error("Hunk_LowAllocNamed: bad params");

	// validate hunk every HUNKCHECKPOW alloc
	if (Q_upow(hunk_counter, HUNKCHECKPOW)) {
		Hunk_CheckGeneral();
		hunk_counter = 0;
	} else {
		hunk_counter++;
	}
#endif

	EnterCriticalCode(&hunkcriticalcode);

	size = sizeof(hunkheader_t) + ((size + (HUNKALIGNMENT - )) & ~(HUNKALIGNMENT - 1));
	if (hunk_size - hunk_used_low - hunk_used_high < size)
		Sys_Error("Hunk_LowAllocNamed: not enough space allocated, try starting with -megs on the command line");

	h = (hunkheader_t *)(hunk_base + hunk_used_low);
	hunk_used_low += size;
	Cache_FreeLow(hunk_used_low);
	
	Q_memset(h, 0, size);
	h->sentinal = HUNKSENTINAL;
	h->size = size;
	Q_strncpy(h->name, name, MAXHUNKNAME);

	out = (void *)((byte_t *)h + sizeof(hunkheader_t));
	LeaveCriticalCode(&hunkcriticalcode);

	return out;
}

/*
=================
Hunk_HighAlloc
=================
*/
void * Hunk_HighAlloc(size_t size)
{
	return Hunk_HighAlloc(size, "unknown");
}

/*
=================
Hunk_HighAllocNamed
=================
*/
void * Hunk_HighAllocNamed(size_t size, const char *name)
{
	hunkheader_t *h;
	void *out;
	
#ifdef PARANOID
	if (size == 0 || !name || !name[0])
		Sys_Error("Hunk_HighAllocNamed: bad params");

	// validate hunk every HUNKCHECKPOW alloc
	if (upow(hunk_counter, HUNKCHECKPOW)) {
		Hunk_CheckGeneral();
		hunk_counter = 0;
	} else {
		hunk_counter++;
	}
#endif

	EnterCriticalCode(&hunkcriticalcode);

	size = sizeof(hunkheader_t) + ((size + (HUNKALIGNMENT - 1)) & ~(HUNKALIGNMENT - 1));
	if (hunk_size - hunk_used_low - hunk_used_high < size)
		Sys_Error("Hunk_HighAllocNamed: not enough space allocated, try using -megs <hunksize> on the command line");

	h = (hunkheader_t *)(hunk_base + hunk_size - hunk_used_high);
	hunk_used_high += size;
	Cache_FreeHigh(hunk_used_high);
	
	Q_memset(h, 0, size);
	h->sentinal = HUNKSENTINAL;
	h->size = size;
	Q_strncpy(h->name, name, MAXHUNKNAME);

	out = (void *)((byte_t *)h + sizeof(hunkheader_t));
	LeaveCriticalCode(&hunkcriticalcode);

	return out;
}

/*
=================
Hunk_LowPop
=================
*/
void Hunk_LowPop(void)
{
	hunkheader_t *h;

	EnterCriticalCode(&hunkcriticalcode);
	
	h = hunk_base;
	hunk_used_low -= h->size;

	LeaveCriticalCode(&hunkcriticalcode);
}

/*
=================
Hunk_LowPopToMark
=================
*/
void Hunk_LowPopToMark(size_t mark)
{
#ifdef PARANOID	
	if (mark > hunk_used_low)
		Sys_Error("Hunk_LowPopToMark: bad mark %d", mark);
#endif

	EnterCriticalCode(&hunkcriticalcode);
	hunk_used_low = mark;
	LeaveCriticalCode(&hunkcriticalcode);
}

/*
=================
Hunk_HighPop
=================
*/
void Hunk_LowPop(void)
{
	hunkheader_t *h;

	EnterCriticalCode(&hunkcriticalcode);
	
	h = hunk_base + hunk_size - hunk_used_high;
	hunk_used_high -= h->size;
	
	LeaveCriticalCode(&hunkcriticalcode);
}

/*
=================
Hunk_HighPopToMark
=================
*/
void Hunk_HighPopToMark(size_t mark)
{
#ifdef PARANOID	
	if (mark > hunk_used_high)
		Sys_Error("Hunk_HighPopToMark: bad mark %d", mark);
#endif

	EnterCriticalCode(&hunkcriticalcode);
	hunk_used_high = mark;
	LeaveCriticalCode(&hunkcriticalcode);
}

/*
==================
Hunk_CheckGeneral
==================
*/
static void Hunk_CheckGeneral(void)
{
	hunkheader_t *h;
	
	EnterCriticalCode(&hunkcriticalcode);
	
	for (h = (hunkheader_t *)hunk_base; (byte_t *)h != hunk_base + hunk_used_low;) {
		if (h->sentinal != HUNKSENTINAL)
			Sys_Error("Hunk_CheckGeneral: trashed sentinal");
		if (h->size < HUNKALIGNMENT || h->size + (byte_t *)h - hunk_base > hunk_size)
			Sys_Error("Hunk_CheckGeneral: bad size");

		h = (hunkheader_t *)((byte_t *)h + h->size);
	}

	LeaveCriticalCode(&hunkcriticalcode);
}

/*
==================
Hunk_Check

Runs consistency and sentinal trashing checks
==================
*/
void Hunk_Check(void)
{
	//
	// hunk general checks
	//
	Hunk_CheckGeneral();

	//
	// subsystems checks
	//
	Zone_Check();
	
	Cache_Check();
}

/*
==================
Hunk_Print

Prints out hunk statistics
==================
*/
void Hunk_Print(qboolean_t everyalloc)
{
	EnterCriticalCode(&hunkcriticalcode);

	LeaveCriticalCode(&hunkcriticalcode);
}

/*
============================================================================================================

Zone Memory Allocator

============================================================================================================
*/
#define DEF_ZPIECE    0.3
#define DEF_ZMINFRAG  64
#define ZONECHECKPOW  HUNKCHECKPOW
#define ZONEALIGNMENT 8
#define ZONESENTINAL  0xff0e1377
typedef struct zoneblock_s {
	size_t sentinel;
	size_t size;
	struct zoneblock_s *next;
	struct zoneblock_s *prev;
} zoneblock_t;
typedef struct zone_s {
	size_t size;
	struct zone_s *next;
	struct zone_s *prev;
} zone_t;
static void * zone_base;
static size_t zone_size, zone_mark;
static size_t zone_minfrag;
static zone_t *zone0;
static unsigned zone_counter;
static criticalcode_t zonecriticalcode;

/*
==================
Zone_Init
==================
*/
void Zone_Init(size_t size, size_t zminfrag)
{
	char *p;
	
	p = COM_CheckArgValue("-zmegs");
	if (p) {
		zone_size = Q_strtoull(p, 0, 10);
		zone_size *= (1024 * 1024);                      // megs to bytes
	} else if (size) {
		zone_size = size;
	} else {
		zone_size = (size_t)((double)hunk_size * DEF_ZPIECE);
	}
	zone_base = Hunk_AllocNamed(zone_size);
	zone_mark = Hunk_LowMark();
	zone_end = zone_base + zone_size;
	zone_minfrag = (zminfrag == UGLYPARAM) ? DEF_ZMINFRAG : zminfrag;
}

/*
==================
Zone_Alloc
==================
*/
void * Zone_Alloc(size_t size)
{
	size_t extra;
	zoneblock_t *start, *rover, *new, *base;
	void *out;
	
#ifdef PARANOID	
	if (size == 0)
		Sys_Error("Zone_Alloc: bad size");

	// validate zone every ZONECHECKPOW alloc
	if (upow(zone_counter, ZONECHECKPOW)) {
		Zone_Check();
		zone_counter = 0;
	} else {
		zone_counter++;
	}
#endif

	EnterCriticalCode(&zonecriticalcode);

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof(zoneblock_t);
	size += sizeof(int);             // space for memory trash tester
	size += (size + (ZONEALIGNMENT - 1)) & ~(ZONEALIGNMENT - 1);

	base = rover = zone0->rover;
	start = base->prev;
	do {
		if (rover == start)          // scanned all the way around the list
			return 0;

		rover = rover->next;
	} while (base->size < size);

	//
	// found a block big enough
	//
	extra = base->size - size;
	if (extra > MIN_ZONE_FRAGMENTATION) {
		// there will be a free fragment after the allocated block
		new =(zoneblock_t *)((byte_t *)base + size);
		new->size = extra;
		new->sentinal = ZONESENTINAL;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->size = size;
	}

	zone0->rover = base->next;       // next allocation will start looking here

	// marker for memory trash testing
	*(int *)((byte_t *)base + base->size - sizeof(int)) = ZONESENTINAL;
	base->sentinal = ZONESENTINAL;

	out = (void *)((byte_t *)base + sizeof(zoneblock_t));

	LeaveCriticalCode(&zonecriticalcode);
	return out;
}

/*
==================
Zone_Free
==================
*/
void Zone_Free(void *addr)
{
	zoneblock_t *block, *other;

	EnterCriticalCode(&zonecriticalcode);
	
#ifdef PARANOID
	if (!addr)
		Sys_Error("Zone_Free: null addr");
#endif

	block = (zoneblock_t *)((byte_t *)addr - sizeof(zoneblock_t));
#ifdef PARANOID	
	if (block->sentinal != ZONESENTINAL)
		Sys_Error("Zone_Free: freeing a pointer without ZONESENTINAL");
#endif	

	other = block->prev;
	other->size += block->siz;
	other->next = block->next;
	other->next->prev = other;
	if (block == zone0->rover)
		zone0->rover = other;
	block = other;

	other = block->next;
	block->size += other->size;
	block->next = other->next;
	block->next->prev = block;
	if (block == zone0->rover)
		zone0->rover = block;

	LeaveCriticalCode(&zonecriticalcode);
}

/*
==================
Zone_Check
==================
*/
void Zone_Check(void)
{
	zoneblock_t *block;

	EnterCriticalCode(&zonecriticalcode);
	
	for (block = zone0->blocklist.next; ; block = block->next) {
		if (block->next == &zone0->blocklist)
			break;                           // all blocks have been hit
		if ((byte_t *)block + block->size != (byte_t *)block->next)
			Sys_Error("Zone_Check: block size doesn't touch the next block");
		if (block->next->prev != block)
			Sys_Error("Zone_Check: next block doesn't have a proper back link");
	}

	LeaveCriticalCode(&zonecriticalcode);
}

/*
==================
Zone_Print
==================
*/
void Zone_Print(void)
{
	EnterCriticalCode(&zonecriticalcode);

	LeaveCriticalCode(&zonecriticalcode);
}

/*
============================================================================================================

Cache Manager

============================================================================================================
*/
#define MAXCACHENAME 32
typedef struct cache_s {
	int size;                                // including this header
	cacheid_t id;
	char name[MAXCACHENAME];
	struct cache_s *next, *prev;
	struct cache_s *lru_next, *lru_prev;     // for LRU flushing
} cache_t;
static cache_t cachechain;
static criticalcode_t cachecriticalcode;

void Cache_MakeLRU(cacheid_t *id);
void Cache_UnlinkLRU(cacheid_t *id);

/*
==================
Cache_Init
==================
*/
void Cache_Init(void)
{
	Q_strcpy(chachechain.name, "dummy");
	cachechain.next = cachechain.prev = &cachechain;
	cachechain.lru_next = cachechain.lru_prev = &cachechain;
}

/*
==================
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks,
size should already include the header and padding
==================
*/
static cache_t * Cache_TryAlloc(size_t size, qboolean_t nobottom)
{
	cache_t *cache, new;

#ifdef PARANOID	
	if (size == 0)
		Sys_Error("Cache_TryAlloc: bad size");
#endif

	//
	// is the cache completely empty?
	//
	if (!nobottom && cachechain.prev == &cachechain) {
		if (hunk_size - hunk_used_low - hunk_used_high < size)
			Sys_Error("Cache_TryAlloc: size %d is greater than free chunk", size);

		new = (cache_t *)(hunk_base + hunk_used_low);
		Q_memset(new, 0, sizeof(cache_t));
		new->size = size;

		cachechain.prev = cachechain.next = neww;
		new->prev = new->next = &cachechain;

		Cache_MakeLRU(new);
		return new;
	}

	//
	// search from the bottom up for space
	//
	new = (cache_t *)(hunk_base + hunk_used_low);
	cache = cachechain.next;
	do {
		if (!nobottom || cache != cachechain.next) {
			if ((byte_t *)cache - (byte_t *)new >= size) {
				//
				// found space
				//
				Q_memset(new, 0, sizeof(cache_t));
				new->size = size;

				new->next = cache;
				new->prev = cache->prev;
				cache->prev->next = new;
				cache->prev = new;

				Cache_MakeLRU(new);
				return new;
			}
		}

		//
		// continue looking
		//
		new = (cache_t *)((byte_t *)cache + cache->size);
		cache = cache->next;
	} while (cache != &cachechain);

	//
	// try to allocate one at the very end
	//
	if (hunk_base + hunk_size - hunk_used_high - (byte_t *)new >= size) {
		Q_memset(new, 0, sizeof(cache_t));
		new->size = size;

		new->next = &cachechain;
		new->prev = cachechain.prev;
		cachechain.prev->next = new;
		cachechain.prev = new;

		Cache_MakeLRU(new);
		return new;
	}

	return 0;            // couldn't allocate
}

/*
==================
Cache_Move
==================
*/
static void Cache_Move(cache_t *cache)
{
	cache_t *new;
	
#ifdef PARANOID
	if (!cache)
		Sys_Error("Cache_Move: null cache");
#endif

	//
	// clear up space at the bottom, so only allocate it late
	//
	new = Cache_TryAlloc(cache->size, true);
	if (new) {
		Q_memcpy(new + 1, cache + 1, cache->size - sizeof(cache_t));
		Q_strcpy(new->name, cache->name);
		new->id = cache->id;

		Cache_Free(cache->id);
		*new->id = (void *)(new + 1);
	} else {
		Cache_Free(cache->id);
	}
}

/*
==================
Cache_FreeLow

Throws things out until the hunk can be expanded to the given point
==================
*/
static void Cache_FreeLow(size_t mark)
{	
	cache_t *cache;

	EnterCriticalCode(&cachecriticalcode);

	while (true) {
		cache = &cachechain.next;
		if (cache == &cachechain)
			break;                      // nothing in cache at all
		if ((byte_t *)cache >= hunk_base + mark)
			break;                      // there is space to grow the hunk
		Cache_Move(cache);              // reclaim the space
	}

	LeaveCriticalCode(&cachecriticalcode);
}

/*
==================
Cache_FreeHigh

Throws things out until the hunk can be expanded to the given point
==================
*/
static void Cache_FreeHigh(size_t mark)
{
	cache_t *cache, *prev = 0;

	EnterCriticalCode(&cachecriticalcode);
	
	while (true) {
		cache = cachechain.prev;
		if (cache == &cache_stat)
			break;                      // nothing in cache at all
		if ((byte_t *)cache + cache->size <= hunk_base + hunk_size - mark)
			break;                      // there is space to grow the hunk
		if (cache == prev) {
			Cache_Free(cache->id);      // didn't move out of the way
		} else {
			Cache_Move(cache->id);      // try to move it...
			prev = cache;
		}
	}

	LeaveCriticalCode(&cachecriticalcode);
}

/*
==================
Cache_MakeLRU
==================
*/
static void Cache_MakeLRU(cache_t *cache)
{	
#ifdef PARANOID
	if (cache->lru_next || cache->lru_prev)
		Sys_Error("Cache_MakeLRU: active cache links");
#endif

	cachechain.lru_next->lru_prev = cache;
	cache->lru_next = cachechain.lru_next;
	cache->lru_prev = &cachechain;
	cachechain.lru_next = cache;
}

/*
==================
Cache_UnlinkLRU
==================
*/
static void Cache_UnlinkLRU(cache_t *cache)
{
#ifdef PARANOID	
	if (!cache)
		Sys_Error("Cache_UnlinkLRU: null cache");
	if (!cache->lru_next || !cache->lru_prev)
		Sys_Error("Cache_UnlinkLRU: null cache links");
#endif

	cache->lru_next->lru_prev = cache->lru_prev;
	cache->lru_prev->lru_next = cache->lru_next;
}

/*
==================
Cache_Alloc
==================
*/
void * Cache_Alloc(cacheid_t *id, size_t size)
{
	cache_t *cache;

	EnterCriticalCode(&cachecriticalcode);

#ifdef PARANOID
	if (*id)
		Sys_Error("Cache_Alloc: already allocated");
	if (!id || size == 0)
		Sys_Error("Cache_Alloc: bad params");
#endif

	size = (size + sizeof(cache_t) + (HUNKALIGNMENT - 1)) & ~(HUNKALIGNMENT - 1);

	//
	// find memory for it
	//
	while (true) {
		cache = Cache_TryAlloc(size, false);
		if (cache) {
			Q_strncpy(cache->name, name, MAXCACHENAME);
			*id = (void *)(cache + 1);
			*cache->id = *id;
			break;
		}

		//
		// find the least recently used cahedat
		//
		if (cachechain.lru_prev == &cachechain)
			Sys_Error("Cache_Alloc: out of memory");

		Cache_Free(cachechain.lru_prev->id);
	}

	//
	// move the the head of LRU
	//
	Cache_UnlinkLRU(cache);
	Cache_MakeLRU(cache);

	LeaveCriticalCode(&cachecriticalcode);
	return *id;
}

/*
==================
Cache_Free
==================
*/
void Cache_Free(cacheid_t *id)
{
	cache_t *cache;

	EnterCriticalCode(&cachecriticalcode);

#ifdef PARANOID
	if (!id)
		Sys_Error("Cache_Free: null id");
#endif

	cache = ((cache_t *)*id) - 1;
	cache->prev->next = cache->next;
	cache->next->prev = cache->prev;
	cache->next = cache->prev = 0;

	*id = 0;
	Cache_UnlinkLRU(cache);

	LeaveCriticalCode(&cachecriticalcode);
}

/*
=================
Cache_Flush

Flush everything, so new data wil be demand cached
=================
*/
void Cache_Flush(void)
{
	while (cachechain.next != &cachechain)
		Cache_Free(cachechain.next->id);               // reclaim the space
}

/*
=================
Cache_Check

Runs cache validation
=================
*/
void Cache_Check(void)
{}

/*
=================
Cache_Print

Prints out cache statistics
=================
*/
void Cache_Print(void)
{
	cache_t *cache;

	EnterCriticalCode(&cachecriticalcode);

	for (cache = cachechain.next; cache != &cachechain; cache = cache->next)
		COM_Printf("%s: %d\n", cache->name, cache->size);

	LeaveCriticalCode(&cachecriticalcode);
}
