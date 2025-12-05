#include "memarena.h"

/* ======================== */
/* FUNCTION IMPLEMENTATIONS */
/* ======================== */

static size_t get_page_size(void)
{
	static size_t page_size = 0;
	if (page_size == 0)
	{
		long res = sysconf(_SC_PAGESIZE);
		page_size = (res > 0) ? (size_t)res : 4096;
	}
	return (page_size);
}

static size_t align_to_page(size_t size)
{
	size_t page_size = get_page_size();
	return (size + page_size - 1) & ~(page_size - 1);
}

static uintptr_t align_forward(uintptr_t ptr, size_t align)
{
	uintptr_t a = (uintptr_t)align;
	uintptr_t modulo = ptr & (a - 1);
	if (modulo != 0)
		ptr += a - modulo;
	return (ptr);
}

static ArenaBlock* arena_create_block(size_t capacity, int prot)
{
	size_t total_needed = capacity + sizeof(ArenaBlock);
	size_t total_size = align_to_page(total_needed);

	void *base = mmap(NULL, total_size, prot, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (base == MAP_FAILED)
		return NULL;
	
	ArenaBlock* block = (ArenaBlock *)base;
	block->prev = NULL;
	block->size = total_size;
	block->offset = sizeof(ArenaBlock);

	ASAN_POISON_MEMORY_REGION((char*)base + sizeof(ArenaBlock), total_size - sizeof(ArenaBlock));

	return(block);
}

Arena arena_init(int prot)
{
	Arena a = {0};
	a.prot = prot;
	return (a);
}

void* arena_alloc_aligned(Arena *a, size_t size, size_t align)
{
	if (size == 0)
		return (NULL);
	if (!is_power_of_two(align))
		return (NULL);
	if (a->curr == NULL)
	{
		size_t block_size = (size > ARENA_DEFAULT_SIZE) ? size : ARENA_DEFAULT_SIZE;
		a->curr = arena_create_block(block_size, a->prot);
		if (!a->curr)
			return (NULL);
	}

	uintptr_t base_addr = (uintptr_t)a->curr;
	uintptr_t current_addr = base_addr + a->curr->offset;
	uintptr_t aligned_addr = align_forward(current_addr, align);
	size_t padding = aligned_addr - current_addr;

	if (a->curr->offset + padding + size > a->curr->size)
	{
		size_t needed = size + align;
		size_t next_size = (needed > ARENA_DEFAULT_SIZE) ? needed : ARENA_DEFAULT_SIZE;
		ArenaBlock *new_block = arena_create_block(next_size, a->prot);
		if (!new_block)
			return (NULL);
		new_block->prev = a->curr;
		a->curr = new_block;

		base_addr = (uintptr_t)a->curr;
		current_addr = base_addr + a->curr->offset;
		aligned_addr = align_forward(current_addr, align);
		padding = aligned_addr - current_addr;
	}

	a->curr->offset += padding;
	void *ptr = (void *)(base_addr + a->curr->offset);
	ASAN_UNPOISON_MEMORY_REGION(ptr, size);

	a->curr->offset += size;
	return (ptr);
}

void* arena_alloc(Arena *a, size_t size)
{
	return (arena_alloc_aligned(a, size, DEFAULT_ALIGNMENT));
}

void* arena_alloc_zeroed(Arena *a, size_t size)
{
   void *ptr = arena_alloc(a, size);
    if (ptr)
        memset(ptr, 0, size);
    return (ptr);
}

void arena_reset(Arena *a)
{
	if (!a->curr)
		return;
	ArenaBlock *curr = a->curr;
	while (curr->prev != NULL)
	{
		ArenaBlock *prev = curr->prev;
		munmap(curr, curr->size);
		curr = prev;
	}
	a->curr = curr;
	a->curr->offset = sizeof(ArenaBlock);

	ASAN_POISON_MEMORY_REGION(
			(char*)a->curr + sizeof(ArenaBlock),
			a->curr->size - sizeof(ArenaBlock));
}

void arena_free(Arena *a)
{
	ArenaBlock *curr = a->curr;
	while (curr)
	{
		ArenaBlock *prev = curr->prev;
		munmap(curr, curr->size);
		curr = prev;
	}
	a->curr = NULL;
}

bool arena_set_prot(Arena *a, int prot)
{
	ArenaBlock *curr = a->curr;
	while (curr)
	{
		if (mprotect(curr, curr->size, prot) == -1)
			return (false);
		curr = curr->prev;
	}

	a->prot = prot;
	return (true);
}

ArenaTemp arena_temp_begin(Arena *a)
{
	ArenaTemp temp = {0};
	temp.arena = a;
	temp.pos.block = a->curr;
	temp.pos.offset = a->curr ? a->curr->offset : 0;
	return temp;
}

void arena_temp_end(ArenaTemp temp)
{
	if (!temp.arena->curr)
		return;
	ArenaBlock *curr = temp.arena->curr;
	while (curr != temp.pos.block)
	{
		if (curr->prev == NULL)
			return;
		ArenaBlock *prev = curr->prev;
		munmap(curr, curr->size);
		curr = prev;
	}

	temp.arena->curr = temp.pos.block;
	if (temp.arena->curr)
	{
		size_t old_offset = temp.arena->curr->offset;
		temp.arena->curr->offset = temp.pos.offset;
		ASAN_POISON_MEMORY_REGION(
				(char*)temp.arena->curr + temp.pos.offset,
				old_offset - temp.pos.offset);
	}
}

size_t arena_total_used(Arena *a)
{
	size_t total = 0;
	ArenaBlock *curr = a->curr;
	while (curr)
	{
		total += curr->offset;
		curr = curr->prev;
	}
	return (total);
}

void arena_print_stats(Arena *a)
{
    size_t total_capacity = 0;
    size_t total_used = 0;
    size_t block_count = 0;
    
    ArenaBlock *curr = a->curr;
    while (curr)
	{
        total_capacity += curr->size;
        total_used += curr->offset;
        block_count++;
        curr = curr->prev;
    }

    printf("Arena Stats:\n");
	printf("  OS Page size: %zuKiB\n", get_page_size() / 1024);
    printf("  Blocks:   %zu\n", block_count);
    printf("  Capacity: %zu MB (%zu KiB)\n", 
			total_capacity / (1024 * 1024),
			total_capacity / 1024);
    printf("  Used:     %zu MB (%zu KiB) [%zu bytes]\n",
			total_used / (1024 * 1024),
			total_used / 1024,
			total_used);
}

char* arena_sprintf(Arena *a, const char *fmt, ...)
{
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    
    int size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (size < 0)
		return (NULL);
    char *buffer = arena_alloc(a, size + 1);
    vsnprintf(buffer, size + 1, fmt, args_copy);
    va_end(args_copy);
    
    return (buffer);
}

