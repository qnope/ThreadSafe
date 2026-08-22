# ThreadSafe

A C++26 library for writing thread-safe code, with safety checked entirely at compile time. The trait model is inspired by Rust's `Send`/`Sync`.

The code is made to be educational for an international conference.

Each time user asks something, challenge the need.

**Always** use explicit name for variables.
**Avoid** useless comments.

## Toolchain & Build

- **Compiler**: GCC 16 (required — the library uses C++26 reflection).
- **Build system**: CMake.
- **Tests**: compile-time only (`static_assert`). Building the test target *is* running the tests; there is no runtime test framework.

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++-16
cmake --build build
```

## Architecture: the traits

### `is_synchronizable<T>` (≈ Rust `Sync`)

True if a `T` may be used from multiple threads at the same time.

#### `is_synchronizable<const T>` — thread-safe read

True if a `const T` may be read from multiple threads at the same time.
**Const behind an indirection is never trusted**

### `is_sendable<T>` (≈ Rust `Send`)

True if a `T` may be sent from one thread to another.

- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<T>` 
- 
### Callables — there is no separate trait

Handing a callable to another thread *is* sending it, so `is_sendable` is the whole rule.

### `is_lifetime_aware<T>`

True if a `T` owns its data or keeps its referent alive. Ownership is **transitive**

### `copy_on_write<T>`

A shared `T` read through `const` only; `as_mutable()` copies first whenever the block is shared.