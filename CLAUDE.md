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
  3. Arrays follow their element type: `is_sendable<T[N]>` = `is_sendable<T>`. Owned storage, not a borrow — and without this rule any type holding a C array is a hard error, not an answer.
- Std specializations (`smart_pointers.h`):
  - `std::unique_ptr<T, D>` = `is_sendable<T> && is_sendable<D>`, **and** the pointee's dynamic type must be knowable (`!is_polymorphic || is_final`). Through a non-final polymorphic base the owned object's real type is invisible, so a sendable base would otherwise smuggle a non-sendable derived object across. The deleter travels with the pointer; array forms follow the element type.
  - `std::shared_ptr<T>`, `std::weak_ptr<T>` = `is_synchronizable<T>` (sending shares the referent; Sync ⇒ Send makes that sufficient).
  - `std::reference_wrapper<T>` = `is_synchronizable<T>` (same rule as `T&`).

### `is_safe_callable<F>`

True if a `F` may be invoked from multiple threads at the same time while shared between them.

- `is_synchronizable<F>` implies `is_safe_callable<F>` (covers function types).
- Function pointers are safe callables (code is immutable).
- Empty class types are safe callables — no per-object state to race on, even through a `mutable` `operator()`. This states *safety*, not invocability: any empty class qualifies (compose with `std::invocable` at the use site). Note that emptiness is **not** statelessness: static data members do not count toward `std::is_empty_v`, so an empty functor may still race on state the trait cannot see.
- Member function pointers: false (out of scope for now).

### `is_lifetime_aware<T>`

True if a `T` owns its data or keeps its referent alive. Ownership is **transitive** — a struct holding a `T*` is no more an owner than the `T*` itself — so the by-value default recurses through bases and members, mirroring `is_sendable`.

| Type | Value |
|---|---|
| `T` (by value) | every base and non-static data member must be lifetime-aware; borrowed ranges → false |
| `T&`, `T*`, `std::reference_wrapper<T>` | false |
| `F*` where `F` is a function type | true (code has static storage duration) |
| `T[N]` | `is_lifetime_aware<T>` |
| `std::shared_ptr<T>`, `std::weak_ptr<T>` | true |
| owning containers, `pair`/`tuple`/`optional`/`variant`/`array` | conjunction over element and policy types |

Reflection cannot tell an owned pointer from a borrowed one, so **every owning std type needs an explicit rule** — the same obligation `is_sendable` already has in `containers.h`. Without one, the recursion descends into the implementation's internal raw pointers and wrongly answers false.

### Header structure

The traits are variable templates, so their value is fixed at the point of instantiation. A TU that saw only *some* specializations would compute a different answer for the same type than one that saw them all — silently, and IFNDR across TUs. Each trait header therefore pulls in the full specialization set at its bottom; `tests/test_include_isolation.cpp` pins this. A full specialization written in a header must be `inline`, or every TU emits its own definition and the link fails.

See `docs/thread-safety-audit.md` for the soundness holes this design has closed and the ones that remain open.
