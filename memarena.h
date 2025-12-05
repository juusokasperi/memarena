#ifndef MEMARENA_H
# define MEMARENA_H

# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <sys/mman.h>
# include <unistd.h>
# include <string.h>
# include <stdbool.h>
# include <stdarg.h>

// Memory poisoning for fsanitize
#if defined(__SANITIZE_ADDRESS__) || defined(ADDRESS_SANITIZER) \
	|| (defined(__has_feature) && __has_feature(address_sanitizer))
  #include <sanitizer/asan_interface.h>
  #ifndef ASAN_POISON_MEMORY_REGION
    #define ASAN_POISON_MEMORY_REGION(addr, size) \
	  __asan_poison_memory_region((void*)(addr), (size))
  #endif

  #ifndef ASAN_UNPOISON_MEMORY_REGION 
    #define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
	  __asan_unpoison_memory_region((void*)(addr), (size))
  #endif 

#else
  #define ASAN_POISON_MEMORY_REGION(addr, size) 	((void)0)
  #define ASAN_UNPOISON_MEMORY_REGION(addr, size)  ((void)0)
#endif 

// Default block size 64MB
#define ARENA_DEFAULT_SIZE (64 * 1024 * 1024)
#define DEFAULT_ALIGNMENT 8

#define is_power_of_two(x) ((x != 0) && ((x & (x - 1)) == 0))

/* ======= */
/* STRUCTS */
/* ======= */

typedef struct ArenaBlock ArenaBlock;

struct ArenaBlock
{
	ArenaBlock	*prev;
	size_t 		size;
	size_t 		offset;
};

typedef struct {
	ArenaBlock 	*curr;
	int 		prot;
} Arena;

typedef struct {
	ArenaBlock 	*block;
	size_t 		offset;
} ArenaPos;

typedef struct {
	Arena* arena;
	ArenaPos pos;
} ArenaTemp;

/* =================== */
/* FUNCTION PROTOTYPES */
/* =================== */

Arena arena_init(int prot);
void arena_free(Arena *a);
void arena_reset(Arena *a);

void* arena_alloc(Arena *a, size_t size);
void* arena_alloc_aligned(Arena *a, size_t size, size_t align);
void* arena_alloc_zeroed(Arena *a, size_t size);

ArenaTemp arena_temp_begin(Arena *a);
void arena_temp_end(ArenaTemp temp);

size_t arena_total_used(Arena *a);
bool arena_set_prot(Arena *a, int prot);

// For debugging arena efficiency
void arena_print_stats(Arena *a);

/* =================== */
/* STRING MANIPULATION */
/* =================== */

char* arena_sprintf(Arena *a, const char *fmt, ...);

#endif
