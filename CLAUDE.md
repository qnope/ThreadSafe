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
  4. A non-empty class that reflects no bases and no members has state the recursion cannot inspect → false (`has_unreflectable_state`, `utils.h`). This is what a **closure type** looks like: GCC reports zero members for a lambda whatever it captures, so without this rule `[&local]{…}` recurses over nothing and answers true. Polymorphic classes are excluded — their unaccounted size is the vptr. `is_lifetime_aware` applies the same rule for the same reason.
- Std specializations (`smart_pointers.h`):
  - `std::unique_ptr<T, D>` = `is_sendable<T> && is_sendable<D>`, **and** the pointee's dynamic type must be knowable (`!is_polymorphic || is_final`). Through a non-final polymorphic base the owned object's real type is invisible, so a sendable base would otherwise smuggle a non-sendable derived object across. The deleter travels with the pointer; array forms follow the element type.
  - `std::shared_ptr<T>`, `std::weak_ptr<T>` = `is_synchronizable<T>` (sending shares the referent; Sync ⇒ Send makes that sufficient).
  - `std::reference_wrapper<T>` = `is_synchronizable<T>` (same rule as `T&`).

### Callables — there is no separate trait

Handing a callable to another thread *is* sending it, so `is_sendable` is the whole rule. Function types are synchronizable hence sendable; function pointers follow the sendable pointer rule; an empty functor or captureless lambda is sendable because the base/member recursion is vacuous; a capturing closure is caught by rule 4 above.

There was an `is_safe_callable<F>` = `is_sendable<F> || (empty class with no user-provided copy/move)`. **Do not reintroduce it.** The disjunct admitted nothing that `is_sendable` rejects for a good reason, and it was unsound: emptiness is inherited but that copy/move guard checked only `F` itself, so `struct D : EmptyUserCopy {}` was empty, had defaulted members of its own, and rode past the launcher — running a user-provided copy constructor on the destination thread. `tests/test_soundness_regressions.cpp` pins the case.

Two properties survive the removal and still matter at the use site:

- The trait states *safety*, not invocability: any sendable type qualifies, member function pointers included (they are scalars). Compose with `std::invocable`.
- Emptiness is **not** statelessness: static data members do not count toward `std::is_empty_v`, so an empty functor may still race on state no trait here can see.

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

The traits are variable templates, so their value is fixed at the point of instantiation. A TU that saw only *some* specializations would compute a different answer for the same type than one that saw them all — silently, and IFNDR across TUs. Each trait header therefore pulls in the full specialization set at its bottom; `tests/test_include_isolation.cpp` pins this. A full specialization written in a header must be `inline`, or every TU emits its own definition and the link fails — and so must a non-template function defined in one, which the `default_is_*` and `is_*_type` functions now are.

### The info-level face of the traits

Each trait also exposes `is_sendable_type(std::meta::info)`, `is_synchronizable_type(...)`, `is_lifetime_aware_type(...)`, named after the predicates of `<meta>`. Same answers as the variable templates, for code written on the reflection side.

They exist because `default_is_sendable` and `default_is_lifetime_aware` take a `std::meta::info` **as a function parameter**, not as a template argument. That parameter is not a constant expression, so nothing in those functions can splice it — no `if constexpr`, no `template for`, no `static_assert`; the bodies are plain loops and `if`s, and the two diagnostics they used to `static_assert` are `throw std::meta::exception` (GCC prints `what()` verbatim, with the instantiation stack).

Reading a trait back for a reflected type therefore goes through `detail::trait_value` in `utils.h` — `std::meta::extract<bool>(std::meta::substitute(^^trait, {type}))`. Going through the variable template is not a detour: the specializations in `containers.h`, `smart_pointers.h` and `vocabulary.h` *are* the answer for their types, and only a template-id sees them. Substitution resolves at evaluation time, so a specialization declared below the `substitute` call — or in the user's own TU, via `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` — still counts; `tests/test_include_isolation.cpp` pins that too. The same mechanism reaches concepts: `std::ranges::borrowed_range` is queried this way, since `<meta>` has no `is_borrowed_range_type`.

See `docs/thread-safety-audit.md` for the soundness holes this design has closed and the ones that remain open.
