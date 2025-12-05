# Memory Arena for C 

A growable Memory Arena (Region) allocator for C.

## Features

- O(1) allocation: allocation is just a pointer bump.
- Infinite growth: Uses a linked list of mmap'd blocks. Never runs out of memory until the OS does.
- Contiguous merging: Attemps to merge new memory blocks adjacent to the previous block. If successful, this extends the previous block's virtual memory range instead of appending to the linked list, reducing fragmentation.
- ASAN Integration: Manually "poisons" unused memory. If you access memory you haven't allocated (or after a reset), ASAN will crash your program with a precise error.
- Page aware: Automatically aligns large allocations to OS page boundaries (4KB/16KB) to eliminate internal fragmentation.
- Instant cleanup: Free millions of objects in O(1) time by freeing the arena or resetting the offset.
- Thread-Local ready: Designed to be used as thread-local storage (no internal mutexes for maximum speed).
- Fixed mode: Optional compile-time flag `MEM_ARENA_DISABLE_RESIZE` to disable growth and pre-allocate memory.

## Installation

This is a single-unit library. You do not need to compile a static library.

1. Copy memarena.c and memarena.h into your project.
2. Include memarena.h where needed.
3. Add memarena.c to your build sources.

## Usage

### Basic Allocation

```c

#include "memarena.h"

int main() {
    // 1. Initialize (Allocates nothing yet)
    Arena arena = arena_init(PROT_READ | PROT_WRITE);

    // 2. Allocate
    int *numbers = arena_alloc(&arena, 1000 * sizeof(int));
    numbers[0] = 55;

    // 3. Sprintf function that allocates to the arena 
    char *text   = arena_sprintf(&arena, "Hello %s", "World");

    // 4. Free everything at once
    arena_free(&arena);
    return 0;
}
```

### Temporary Scratch Memory

Use "Temp" arenas to perform complex operations and then rollback memory usage instantly.

```c
void parse_file(Arena *a) {
    // Save state
    ArenaTemp temp = arena_temp_begin(a);

    // Allocate tons of temporary structs
    for (int i = 0; i < 1000; i++) {
        do_heavy_work(a);
    }

    // Rollback! All 1000 allocations are effectively "freed"
    // The memory is poisoned so you can't accidentally use it again.
    arena_temp_end(temp); 
}
```

### Debugging & Safety

Memarena tells ASAN which bytes are valid and which are "poison." 

To enable this, compile with -fsanitize=address:

```bash
# GCC / Clang
cc -fsanitize=address -g tester/tester.c memarena.c -o tester/tester 
# Tester accepts flags --poison, --align, --all, --help
./tester/tester
```
Run the tester with poison flag to verify the crash protection:
```bash 
./tester/tester --poison 
```

If you try to access memory after `arena_reset` or `arena_temp_end`, your program will crash immediately with a helpful report:

```plaintext
==12345==ERROR: AddressSanitizer: use-after-poison on address 0x...
```

### Configuration

By default, dynamic resizing is enabled. To disable this and effectively allow only one mmap call per arena:
1. Compile with `-DMEMARENA_DISABLE_RESIZE` (or uncomment the define in `memarena.h`)
2. Now `arena_init` will allocate the full `MEMARENA_DEFAULT_SIZE` immediately on startup.
3. `arena_alloc` will return `NULL` if you exceed the fixed size.


```c 
// Usage in fixed Mode
Arena a = arena_init(PROT_READ | PROT_WRITE);
if (a.curr == NULL)
{
    perror("mmap failed");
    exit(1);
    // Or whatever error handling you prefer
}
```

When running in fixed mode, you might want to increase the block size. You can tweak the default block size in `memarena.h` or by compiling with `-DMEMARENA_DEFAULT_SIZE`.

```c 
// Default is 64MB
#define MEMARENA_DEFAULT_SIZE (64 * 1024 * 1024)
```

### Statistics

You can inspect the efficiency of your arena at any time. This is useful for tuning your MEMARENA_DEFAULT_SIZE.

*Note: If "Blocks" remains low even after allocating more than the default size, it means the arena successfully merged the new allocations into a contiguous block.*

```c 
arena_print_stats(&arena);
```

```plaintext
Arena Stats:
  OS Page size: 16KiB
  Blocks:   2
  Capacity: 134 MB (137248 KiB)
  Used:     70 MB (71680 KiB) [73400408 bytes]
```

