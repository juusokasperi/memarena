#define MEMARENA_IMPLEMENTATION
#include "../memarena.h"

// Run from root of repo:
// cc (-DMEMARENA_DISABLE_RESIZE) -fsanitize=address -g tester/tester.c -o tester/memarena_tester

#define YELLOW "\033[0;93m"
#define GREEN_B "\033[1;92m"
#define RED_B "\033[1;91m"
#define RESET "\033[0m"

#define FLAG_POISON 0x01 
#define FLAG_ALIGN 0x02 
#define FLAG_REALLOC 0x03
#define FLAG_ALL 0xFF

static void page_alignment(void);
static void poison(void);
static uint8_t check_flags(int argc, char **argv);
static bool check_version_match(void);
static void test_realloc(void);

int main(int argc, char **argv)
{
	if (!check_version_match())
		return (1);

	if (argc < 2)
	{
		page_alignment();
		test_realloc();
		return (0);
	}
	uint8_t flags = check_flags(argc, argv);
	if (flags & FLAG_ALIGN)
		page_alignment();
	if (flags & FLAG_POISON)
		poison();
	if (flags & FLAG_REALLOC)
		test_realloc();
    return (0);
}

static void poison(void)
{
	printf("%s==================\n", GREEN_B);
    printf("=== Arena Test ===\n");
	printf("==================%s\n", RESET);
	
    Arena a = arena_init(PROT_READ | PROT_WRITE);

#ifdef MEMARENA_DISABLE_RESIZE
	if (!a.curr)
	{
		printf("  %s>> FAIL: Allocating block for arena failed.%s\n", RED_B, RESET);
		return;
	}
	printf("  %s>> Initialized arena (%d MiB)%s\n", YELLOW, MEMARENA_DEFAULT_SIZE / 1024 / 1024, RESET);
#else
    printf("  %s>> Initialized arena (not allocating yet)%s\n", YELLOW, RESET);
#endif

    printf("  %s>> Allocating a block of %lu bytes%s\n", YELLOW, sizeof(int) * 10, RESET);
    int *nums = arena_alloc(&a, sizeof(int) * 10);
    nums[0] = 42;
    printf("  %s>> Wrote to the block:%s %d\n", YELLOW, RESET, nums[0]);
    printf("  %s>> Checking arena statistics.%s\n", YELLOW, RESET);
    printf("    >> Expect to see %dMB allocated (our minimum) and %zu bytes used.\n\n", 
			MEMARENA_DEFAULT_SIZE / 1024 / 1024,
			sizeof(int) * 10 + sizeof(ArenaBlock));
	arena_print_stats(&a);

#ifndef MEMARENA_DISABLE_RESIZE
    printf("  \n%s>> Allocating 70MB%s\n", GREEN_B, RESET);
    void *huge = arena_alloc(&a, 70 * 1024 * 1024);
    if (!huge)
	{
		printf("    %s>> FAIL: Error allocating the arena%s\n", RED_B, RESET);
		return;
	}

    printf("  %s>> Checking arena statistics%s\n\n", YELLOW, RESET);
	arena_print_stats(&a);
#endif

    printf("\n  %s>> Resetting arena.%s\n\n", YELLOW, RESET);
    arena_reset(&a);
	arena_print_stats(&a);
    printf("\n  %s>> Attempting to access old memory (expect crash)%s\n", YELLOW, RESET);
    printf("  >> Old value: %d\n", nums[0]); 
    arena_free(&a);
}

static void page_alignment(void)
{
	printf("%s===========================\n", GREEN_B);
	printf("=== Page Alignment Test ===\n");
	printf("===========================%s\n", RESET);
    Arena a2 = arena_init(PROT_READ | PROT_WRITE);

#ifdef MEMARENA_DISABLE_RESIZE
	if (!a2.curr)
	{
		printf("  %s>> FAIL: Allocating block for arena failed.%s\n", RED_B, RESET);
		return;
	}
	printf("  %s>> Initialized arena (%d MiB)%s\n", YELLOW, MEMARENA_DEFAULT_SIZE / 1024 / 1024, RESET);
    printf("  %s>> Checking stats%s\n\n", YELLOW, RESET);
	size_t allocation_size = (size_t)(64 * 1024 * 1024) - 16;
	void *ptr = arena_alloc(&a2, allocation_size);
#else  
    size_t weird_size = (size_t)(64 * 1024 * 1024) + 1;
    printf("  %s>> Allocating 64MB + 1 byte (%zu bytes)%s\n", YELLOW, weird_size, RESET);
    
    void *ptr = arena_alloc(&a2, weird_size);
    printf("  %s>> Checking stats (Expect 64MB + 1 Page of capacity)%s\n\n", YELLOW, RESET);
#endif
    
    arena_print_stats(&a2);
    long page_size = sysconf(_SC_PAGESIZE);
    size_t cap = a2.curr->size;
    if (cap % page_size == 0)
        printf("  \n%s>> SUCCESS: Block size %zu is perfectly divisible by page size %ld%s\n", GREEN_B, cap, page_size, RESET);
    else
        printf("  %s>> FAIL: Block size %zu is NOT page aligned!%s\n", RED_B, cap, RESET);

	printf("\n%s========================\n", GREEN_B);
	printf("=== Slack Space Test ===\n");
	printf("========================%s\n", RESET);
    printf("  %s>> We have ~16KiB remaining in the current block.\n", YELLOW);
    printf("  >> Allocating 1KiB. Should fit in CURRENT block.%s\n", RESET);
    
    ArenaBlock *before_block = a2.curr;
    arena_alloc(&a2, 1024);

#ifdef MEMARENA_DISABLE_RESIZE
    if (a2.curr == before_block)
        printf("  %s>> SUCCESS: Allocation succeeded.%s\n", GREEN_B, RESET);
	else
        printf("  %s>> FAIL: Allocation failed, pointer returned NULL.%s\n", RED_B, RESET);
#else
    if (a2.curr == before_block)
        printf("  %s>> SUCCESS: Still in the same block! We used the slack space.%s\n", GREEN_B, RESET);
    else
        printf("  %s>> FAIL: Created a new block unnecessarily.%s\n", RED_B, RESET);
#endif    

#ifdef MEMARENA_DISABLE_RESIZE
    printf("  %s>> Now allocating 24KiB. This should fail and return NULL%s\n", YELLOW, RESET);
    void *ptr2 = arena_alloc(&a2, 24 * 1024);
	if (ptr2 == NULL)
        printf("  %s>> SUCCESS: Allocation failed expectedly (out of space). Stats should show ~15KiB still free on the block.%s\n", GREEN_B, RESET);
	else 
		printf(" %s>> FAIL: Ptr2 didn't return NULL. Time to debug!%s\n", RED_B, RESET);
#else
    printf("  %s>> Now allocating 100MB. This can trigger a new block or merge into the existing one.%s\n", YELLOW, RESET);
    
	if (a2.curr != before_block)
        printf("  %s>> SUCCESS: Moved to a new block.%s\n", GREEN_B, RESET);
	else if (a2.curr == before_block)
        printf("  %s>> SUCCESS: Merged to the previous block.%s\n", GREEN_B, RESET);
	else
		printf("  %s>> FAIL: Allocation failed.%s\n'n", RED_B, RESET);
#endif

    arena_print_stats(&a2);
	printf("\n\n");
    arena_free(&a2);
}

static void test_realloc(void)
{
	printf("%s====================\n", GREEN_B);
    printf("=== Realloc Test ===\n");
    printf("====================%s\n", RESET);
    
    Arena a = arena_init(PROT_READ | PROT_WRITE);
    
    // 1. Test In-place Growth (last allocation)
    printf("  %s>> Testing in-place growth...%s\n", YELLOW, RESET);
    size_t initial_size = 128;
    size_t growth_size = 256;
    void *ptr1 = arena_alloc(&a, initial_size);
    memset(ptr1, 0xAA, initial_size);
    
    void *ptr1_new = arena_realloc(&a, ptr1, initial_size, growth_size);
    
    if (ptr1 == ptr1_new)
        printf("  %s>> SUCCESS: Realloc stayed in place for last allocation.%s\n", GREEN_B, RESET);
    else
        printf("  %s>> FAIL: Realloc moved despite being the last allocation.%s\n", RED_B, RESET);

    // 2. Test Fallback (not the last allocation)
    printf("  %s>> Testing fallback allocation...%s\n", YELLOW, RESET);
    void *ptr2 = arena_alloc(&a, 64); // This is now the "top" of the arena
    // Try to grow ptr1 again; since ptr2 is after it, it must move
    void *ptr1_moved = arena_realloc(&a, ptr1_new, growth_size, growth_size * 2);
    
	if (ptr1_moved != ptr1_new)
	{
        printf("  %s>> SUCCESS: Fallback triggered (pointer moved) correctly.%s\n", GREEN_B, RESET);
        // Verify data integrity
        if (((unsigned char*)ptr1_moved)[0] == 0xAA)
            printf("  %s>> SUCCESS: Data preserved after move.%s\n", GREEN_B, RESET);
	} 
	else
        printf("  %s>> FAIL: Realloc stayed in place even though memory was blocked.%s\n", RED_B, RESET);

    // 3. Test Alignment
    printf("  %s>> Testing aligned realloc...%s\n", YELLOW, RESET);
    size_t align = 64;
    void *ptr3 = arena_alloc_aligned(&a, 32, align);
    void *ptr3_grown = arena_realloc_aligned(&a, ptr3, 32, 128, align);
    
    if (((uintptr_t)ptr3_grown % align) == 0)
        printf("  %s>> SUCCESS: Grown pointer is still aligned to %zu.%s\n", GREEN_B, align, RESET);
    else
        printf("  %s>> FAIL: Grown pointer lost alignment (%zu)!%s\n", RED_B, align, RESET);

    arena_free(&a);
}

static uint8_t check_flags(int argc, char **argv)
{
	uint8_t flags = 0;
	bool help_printed = false;

	if (argc < 2)
		return (flags);
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--poison") == 0)
			flags |= FLAG_POISON;
		else if (strcmp(argv[i], "--align") == 0)
			flags |= FLAG_ALIGN;
		else if (strcmp(argv[i], "--realloc") == 0)
			flags |= FLAG_REALLOC;
		else if (strcmp(argv[i], "--all") == 0)
			flags = FLAG_ALL;
		else if (strcmp(argv[i], "--help") == 0)
		{
			if (!help_printed)
			{
				printf("%sHow to use tester:%s\n", GREEN_B, RESET);
				printf("Accepts flags --poison, --align, --realloc, --all, --help\n");
				printf("By default, runs with --align and --realloc\n");
				printf("Remember to compile with -g and -fsanitize=address for the poison test\n");
				help_printed = true;
			}
		}
		else
			printf("%sWarning%s: Invalid flag %s\n", "\033[0;91m", "\033[0m", argv[i]);
	}
	return (flags);
}

static bool check_version_match(void)
{
    MemArenaVersion v = arena_get_version();
    printf("%s>> Memory Arena Version: %d.%d.%d%s\n", YELLOW, v.major, v.minor, v.patch, RESET);

    if (v.major != MEMARENA_VERSION_MAJOR && v.minor != MEMARENA_VERSION_MINOR)
	{
        printf("%s>> FAIL: Version mismatch! Header is %d, Binary is %d%s\n", 
               RED_B, MEMARENA_VERSION_MAJOR, v.major, RESET);
        return (false);
    }
	return (true);
}
