# Probe harness

Scripts for compiling, running and race-checking scenarios against ThreadSafe.
They locate the repository themselves; override with `THREADSAFE_ROOT`.

    ./tsc   <file.cpp>   # compile-only (-fsyntax-only).
                         # exit 0 = accepted / every static_assert held.
                         # non-zero = rejected; stderr carries the library's diagnostic.
    ./tsrun <file.cpp>   # -O2 build + run. For behaviour and benchmarks.
    ./tstsan <file.cpp>  # ThreadSanitizer build + run.

Requirements: GCC 16 (`-freflection` is mandatory), C++26.

## Reading a rejection

The traits answer a bare `false`. To get the library's explanation, ask through
an `assert_*` inside a consteval function:

    consteval bool ask() { threadsafe::assert_sendable<T>(); return true; }
    static_assert(ask());

## ThreadSanitizer on macOS/arm64

GCC ships no TSan runtime there, and Apple clang cannot compile `-freflection`.
`tstsan` therefore compiles with `g++-16` and links Apple clang's runtime
explicitly, with the matching rpath. Override the runtime with `TSAN_RUNTIME`.

Verified against a control program with a deliberate race, so the instrumentation
is known to work on reflection-compiled code.

**Known blind spot**: GCC warns that `atomic_thread_fence` is not supported under
`-fsanitize=thread`. The acquire fence in `copy_on_write::as_mutable()` is
therefore invisible to TSan — a clean run proves nothing about it.

## Mutation testing

    python3 mutation_test.py

Breaks one library rule at a time in a copy of the headers and rebuilds the whole
suite. See `docs/03-couverture-de-tests.md`.
