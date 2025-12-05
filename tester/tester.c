#include "../memarena.h"

// cc -fsanitize=address -g tester/tester.c memarena.c -o tester 

#define YELLOW "\033[0;93m"
#define GREEN_B "\033[1;92m"
#define RED_B "\033[1;91m"
#define RESET "\033[0m"

#define FLAG_POISON 0x01 
#define FLAG_ALIGN 0x02 
#define FLAG_ALL 0xFF

static void page_alignment(void);
static void poison(void);
static uint8_t check_flags(int argc, char **argv);

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		page_alignment();
		return (0);
	}
	uint8_t flags = check_flags(argc, argv);
	if (flags & FLAG_ALIGN)
		page_alignment();
	if (flags & FLAG_POISON)
		poison();
    return (1);
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

#ifdef MEMARENA_DISABLE_RESIZE

#else
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

#ifdef MEMARENA_DEFAULT_SIZE
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

#ifdef MEMARENA_DEFAULT_SIZE
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
		else if (strcmp(argv[i], "--all") == 0)
			flags = FLAG_ALL;
		else if (strcmp(argv[i], "--help") == 0)
		{
			if (!help_printed)
			{
				printf("%sHow to use tester:%s\n", GREEN_B, RESET);
				printf("Accepts flags --poison, --align, --all, --help\n");
				printf("By default, runs with --align\n");
				printf("Remember to compile with -g and -fsanitize=address for the poison test\n");
				help_printed = true;
			}
		}
		else
			printf("%sWarning%s: Invalid flag %s\n", "\033[0;91m", "\033[0m", argv[i]);
	}
	return (flags);
}
