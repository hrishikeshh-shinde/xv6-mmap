# OS Enhancements in xv6: Secure Memory Mapping, Copy-on-Write, and Virtual Memory Tools

This project extends the xv6 operating system with modern memory management features inspired by Unix/Linux. The enhancements improve system security, performance, and usability through virtual memory mapping, lazy allocation, copy-on-write fork, and ELF-based memory protection.

## Authors

- Apoorva Mittal
- Hrishikesh Shinde  

## Objectives

- Implement file-backed and anonymous memory mapping via custom `wmap()` and `wunmap()` system calls.
- Add page-level protection support by parsing ELF flags in user programs.
- Implement efficient `fork()` with copy-on-write semantics and reference counting.
- Provide user-accessible virtual memory diagnostics via `va2pa()` and `getwmapinfo()` system calls.
- Track and remove all memory mappings on process exit to prevent memory leaks.

## Features

- Memory protection based on ELF segment permissions
- Lazy allocation using page fault handler
- Shared memory mapping with support for file-backed regions
- Copy-on-write implementation using PTE flags and reference counting
- TLB flushing for correct memory behavior
- Additional system calls for memory translation and mmap inspection

## System Call Interfaces

```c
// Memory mapping
uint wmap(uint addr, int length, int flags, int fd);
int wunmap(uint addr);

// Virtual memory diagnostics
uint va2pa(uint va);
int getwmapinfo(struct wmapinfo *info);
```

## Usage Constraints

- Only addresses in the range `0x60000000` to `0x80000000` may be used with `wmap`
- Supported flags: `MAP_FIXED`, `MAP_SHARED`, `MAP_ANONYMOUS`
- Maximum number of mappings per process: 16 (`MAX_WMMAP_INFO`)
- Memory is always readable and writable; executable bit is ignored

## Sample Usage

```c
// Allocate two pages of anonymous shared memory at a fixed address
uint addr = wmap(0x60000000, 8192, MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS, -1);

// Write into the mapped region (triggers lazy allocation)
*(char *)addr = 'x';

// Unmap the region
wunmap(addr);
```

## Implementation Highlights

- Page fault handling added to `trap()` to support lazy allocation and copy-on-write
- `exec()` updated to respect read/write ELF flags using `loaduvm` and `mappages`
- Reference counts stored in a static array indexed by page frame number
- Custom PTE flag used to mark originally writable pages for copy-on-write safety
- TLB flushing via `lcr3` ensures correct page table updates
- All `wmap` mappings are freed on process exit to avoid memory leaks

## Additional System Calls

- `va2pa(uint va)`: Returns physical address mapped to a given virtual address or -1
- `getwmapinfo(struct wmapinfo *info)`: Returns number of active memory mappings, base addresses, lengths, and number of loaded physical pages per mapping

## Testing

All functionality is validated using user-level tests under the `tests/` directory. These tests verify:

- Basic allocation and unmapping
- Page faults triggering correct allocation
- File-backed memory mapping correctness
- Shared memory persistence across processes
- Copy-on-write correctness
- Proper read-only segmentation enforcement on ELF binaries
- Accuracy of `va2pa()` and `getwmapinfo()` output

To run tests, boot xv6 and use:

```sh
make qemu
```

Then execute relevant user programs found in the `tests/` folder.

## Directory Structure

- `wmap.h`: Flag definitions and constants for new syscalls
- `sysproc.c`, `syscall.c`: System call dispatch and logic
- `vm.c`: Memory allocation, page table updates, and page fault logic
- `exec.c`: Program loading with ELF-based permission control
- `trap.c`: Trap handler updated for `T_PGFLT` to support lazy allocation and COW
- `tests/`: Contains user-level tests for validation

## Future Improvements

- Add support for `MAP_PRIVATE` semantics
- Support partial unmapping of regions
- Use dynamic page tables or bitmap-based allocation to improve memory tracking
- Add per-page protection options like read-only or no-execute via `prot` flags

## License

Educational use only. Based on MIT xv6 and extended for university-level OS coursework.
