# NBOS Software Development Kit

The SDK contains the freestanding headers, startup object, compiler wrapper,
and example programs intended for NBOS applications.

## Status and scope

The public headers define an application-facing ABI based on `int 0x80`
syscalls and a BGI-style graphics API. The current repository does not yet
provide a complete user-mode loader or matching kernel implementation for
every declared syscall, so the SDK is an interface and example foundation,
not a self-contained application runtime.

## Layout

| Path | Purpose |
| --- | --- |
| `include/nbos.h` | Syscall numbers, wrappers, memory, strings, console, and control APIs |
| `include/graphics.h` | Header-only BGI-compatible drawing helpers |
| `include/stdint.h` | Fixed-width integer types and limits |
| `lib/crt0.asm` | Application entry/startup object |
| `tools/nbos-gcc` | Compiler and linker wrapper |
| `tools/nbos.mk` | Shared Make definitions |
| `examples/hello` | Minimal console example |
| `examples/graphics` | Drawing API example |
| `examples/game` | Pong-style input/graphics example |

## Setup and build

```bash
source sdk/env.sh
cd sdk/examples/hello
make
```

`env.sh` exports `NBOS_SDK`, `NBOS_INCLUDE`, and `NBOS_LIB`, and puts
`nbos-gcc` on `PATH`. The wrapper prefers `x86_64-elf-gcc`/`ld` and falls
back to the host `gcc`/`ld`.

The wrapper compiles with freestanding 64-bit flags, disables the red zone
and SIMD instructions, links without the host C library, and uses `crt0.o`
when it exists. `make clean` removes generated binaries and object files in
each example directory.

## Syscall interface

The syscall number constants are declared in `include/nbos.h`:

| Number | Name | Purpose |
| ---: | --- | --- |
| 0 | `SYS_EXIT` | Terminate the application |
| 1 | `SYS_PRINT` | Console output |
| 2 | `SYS_GETKEY` | Blocking key input |
| 3 | `SYS_KBHIT` | Test for key input |
| 4-5 | `SYS_MALLOC`, `SYS_FREE` | Heap allocation |
| 6 | `SYS_SLEEP` | Millisecond delay |
| 10-14 | `SYS_PUTPIXEL` through `SYS_GETHEIGHT` | Graphics and screen queries |

The wrappers place the syscall number in `RAX` and arguments in the registers
required by the wrapper arity, then issue `int $0x80`. Kernel-side syscall
dispatch must preserve this contract.

## Graphics API

`graphics.h` keeps drawing state in header-local `static` variables and
implements primitives inline. `initgraph` obtains the screen dimensions from
the kernel, `setcolor`/`setbkcolor` update state, and drawing functions issue
pixel syscalls. BGI palette values are converted to packed RGB values.

Some compatibility functions are intentionally simplified: `arc` currently
draws a complete circle, and viewport clipping is state-only. Treat these
as documented compatibility shims rather than full BGI implementations.

## Examples

Each example has its own Makefile and includes `tools/nbos.mk`:

```bash
cd sdk/examples/graphics
make
```

The generated `.bin` files are application artifacts; they are not copied
into the boot image by the default top-level build.
