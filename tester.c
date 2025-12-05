#include "memarena.h"
#include <stdio.h>
#include <sys/mman.h>

// cc -fsanitize=address -g tester.c memarena.c -o tester 

#define YELLOW "\033[0;93m"
#define GREEN_B "\033[1;92m"
#define RED_B "\033[1;91m"
#define RESET "\033[0m"

static void page_alignment(void);
static void poison(void);

int main()
{
	page_alignment();
	poison();
    return 0;
}

static void poison(void)
{
	printf("%s==================\n", GREEN_B);
    printf("=== Arena Test ===\n");
	printf("==================%s\n", RESET);
	
    printf("  %s>> Initializing arena (not allocating yet)%s\n", YELLOW, RESET);
    Arena a = arena_init(PROT_READ | PROT_WRITE);

    printf("  %s>> Allocating a block of %lu bytes%s\n", YELLOW, sizeof(int) * 10, RESET);
    int *nums = arena_alloc(&a, sizeof(int) * 10);
    nums[0] = 42;
    printf("  %s>> Wrote to the block:%s %d\n", YELLOW, RESET, nums[0]);
    printf("  %s>> Checking arena statistics.%s\n", YELLOW, RESET);
    printf("    >> Expect to see %dMB allocated (our minimum).\n\n", ARENA_DEFAULT_SIZE / 1024 / 1024);
	arena_print_stats(&a);

    printf("  \n%s>> Allocating 70MB%s\n", GREEN_B, RESET);
    void *huge = arena_alloc(&a, 70 * 1024 * 1024);
    if (!huge)
	{
		printf("    %s>> FAIL: Error allocating the arena%s\n", RED_B, RESET);
		return;
	}

    printf("  %s>> Checking arena statistics%s\n\n", YELLOW, RESET);
	arena_print_stats(&a);
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
    
    size_t weird_size = (size_t)(64 * 1024 * 1024) + 1;
    printf("  %s>> Allocating 64MB + 1 byte (%zu bytes)%s\n", YELLOW, weird_size, RESET);
    
    void *ptr = arena_alloc(&a2, weird_size);
    
    printf("  %s>> Checking stats (Expect 64MB + 1 Page of capacity)%s\n\n", YELLOW, RESET);
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
    printf("  %s>> We have ~16KB remaining in the current block.\n", YELLOW);
    printf("  >> Allocating 1KB. Should fit in CURRENT block (No new node).%s\n", RESET);
    
    ArenaBlock *before_block = a2.curr;
    
    arena_alloc(&a2, 1024);
    
    if (a2.curr == before_block)
        printf("  %s>> SUCCESS: Still in the same block! We used the slack space.%s\n", GREEN_B, RESET);
    else
        printf("  %s>> FAIL: Created a new block unnecessarily.%s\n", RED_B, RESET);
    

    printf("  %s>> Now allocating 100MB. Should trigger NEW block.%s\n", YELLOW, RESET);
    arena_alloc(&a2, 100 * 1024 * 1024);
    
    if (a2.curr != before_block)
        printf("  %s>> SUCCESS: Moved to a new block.%s\n", GREEN_B, RESET);
	else
		printf("  %s>> FAIL: Stayed in the same block.%s\n", RED_B, RESET);
	printf("\n");
    arena_free(&a2);
}

