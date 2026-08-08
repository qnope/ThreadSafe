# ThreadSafe

A C++26 library for writing thread-safe code, with safety checked entirely at compile time. The trait model is inspired by Rust's `Send`/`Sync`, with deliberate differences noted below.

Each time user asks something, challenge the need.

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

- Default: **false**.
- Function types are synchronizable (code is immutable); function *pointers* need no special case anywhere — the sendable pointer rule below covers them.
- `is_synchronizable<std::atomic<T>>` = `is_sendable<T>`.

### `is_sendable<T>` (≈ Rust `Send`)

True if a `T` may be sent from one thread to another.

- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<T>` (sending a reference or pointer means sharing the referent).
- Default for a non-reference `T`, in order:
  1. If `is_synchronizable<T>` is true, `is_sendable<T>` is true. **Deliberate deviation from Rust** (where `Sync` does not imply `Send`) — do not "fix" this.
  2. Otherwise `T` is sendable if it has no user-provided copy/move constructor or assignment operator — defaulted or deleted members, implicit or explicit, are fine; detected via C++26 reflection — and all of its base classes and non-static data members are sendable.
- Std specializations (`smart_pointers.h`):
  - `std::unique_ptr<T, D>` = `is_sendable<T> && is_sendable<D>` (the deleter travels with the pointer; array forms follow the element type).
  - `std::shared_ptr<T>`, `std::weak_ptr<T>` = `is_synchronizable<T>` (sending shares the referent; Sync ⇒ Send makes that sufficient).
  - `std::reference_wrapper<T>` = `is_synchronizable<T>` (same rule as `T&`).

### `is_safe_callable<F>`

True if a `F` may be invoked from multiple threads at the same time while shared between them.

- `is_synchronizable<F>` implies `is_safe_callable<F>` (covers function types).
- Function pointers are safe callables (code is immutable).
- Empty class types are safe callables — no per-object state to race on, even through a `mutable` `operator()`. This states *safety*, not invocability: any empty class qualifies (compose with `std::invocable` at the use site).
- Member function pointers: false (out of scope for now).

### `is_lifetime_aware<T>`

True if a `T` owns its data or keeps its referent alive.

| Type | Value |
|---|---|
| `T` (by value) | true, **except** borrowed ranges → false |
| `T&`, `T*`, `std::reference_wrapper<T>` | false |
| `std::shared_ptr<T>`, `std::weak_ptr<T>` | true |
