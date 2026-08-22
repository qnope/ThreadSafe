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

**Avoid** to write useless comments.

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
  2. Otherwise `T` is sendable if it has no user-provided copy/move constructor, assignment operator or destructor — defaulted or deleted members, implicit or explicit, are fine; detected via C++26 reflection (`has_only_default_copy_move_destroy`, `utils.h`) — and all of its base classes and non-static data members are sendable. The destructor is in the set for the same reason as the rest: whoever drops the object destroys it, and after a send that is the destination thread. The conservatism is real and accepted — `struct Owner { std::unique_ptr<int> p; ~Owner() {} };` is not sendable although it would be safe; write `= default` or specialize.
  3. A constructor template or an `operator=` template also blocks the default (`may_hijack_copy_move`, `utils.h`), even though neither is ever a copy or move member. Against a non-const lvalue `T&`, `template <class U> T(U&&)` deduces `U = T&` and matches exactly where the implicit `T(const T&)` needs a qualification conversion; the non-template tiebreaker never runs and `T b = a;` calls user code although every special member is implicit. (`const U&` and by-value forms tie and lose the tiebreaker, so only the deduce-to-`T&` shapes hijack.) Which shape it is cannot be told from here — `parameters_of` rejects a template — so an arity or a constraint that makes hijacking impossible, `T(It, It)` or `requires (!same_as<remove_cvref_t<U>, T>)`, is rejected with the greedy ones. `std::default_delete` needs an explicit rule for exactly this reason: its converting constructor is a template. `std::complex<T>` has the same shape — in C++26 libstdc++ declares `template <class U> explicit(…) complex(const complex<U>&)` — and is deliberately left without one, so `is_sendable<std::complex<T>>` is **false**. There was a `vocabulary.h` specialization forcing it true; it was removed because carrying `<complex>` into every TU cost ~17% of the whole suite (6.80 s → 5.62 s), and `std::complex` is not worth that; a caller who needs it specializes `is_sendable` itself, subject to the header-structure rule below.
  4. Arrays follow their element type: `is_sendable<T[N]>` = `is_sendable<T>`. Owned storage, not a borrow — and without this rule any type holding a C array is a hard error, not an answer.
  5. A non-empty class that reflects no bases and no members has state the recursion cannot inspect → false (`has_unreflectable_state`, `utils.h`). This is what a **closure type** looks like: GCC reports zero members for a lambda whatever it captures, so without this rule `[&local]{…}` recurses over nothing and answers true. Polymorphic classes are excluded — their unaccounted size is the vptr. `is_lifetime_aware` applies the same rule for the same reason.
- Std specializations (`smart_pointers.h`):
  - `std::unique_ptr<T, D>` = `is_sendable<T> && is_sendable<D>`, **and** the pointee's dynamic type must be knowable (`!is_polymorphic || is_final`). Through a non-final polymorphic base the owned object's real type is invisible, so a sendable base would otherwise smuggle a non-sendable derived object across. The deleter travels with the pointer; array forms follow the element type.
  - `std::shared_ptr<T>`, `std::weak_ptr<T>` = `is_synchronizable<T>` (sending shares the referent; Sync ⇒ Send makes that sufficient).
  - `std::reference_wrapper<T>` = `is_synchronizable<T>` (same rule as `T&`).

### Callables — there is no separate trait

Handing a callable to another thread *is* sending it, so `is_sendable` is the whole rule. Function types are synchronizable hence sendable; function pointers follow the sendable pointer rule; an empty functor or captureless lambda is sendable because the base/member recursion is vacuous; a capturing closure is caught by rule 5 above.

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

### `copy_on_write<T>` and the `mutable` guard

A shared `T` read through `const` only; `as_mutable()` copies first whenever the block is shared. The sendable rule is the reason the type exists:

```cpp
is_sendable<copy_on_write<T>> = is_sendable<T> && (is_synchronizable<T> || !detail::has_mutable_state(^^T));
```

`is_sendable<T>` is unconditional — the `T` is copied on the receiving thread and destroyed by whoever drops the last handle. The second factor is what `shared_ptr` cannot say: two handles sharing a `T` only ever *read* it, and reading concurrently is safe unless the `T` writes from a `const` method. `detail::has_mutable_state` (`utils.h`) is the proxy for that, walking bases, by-value members and arrays — a `mutable` behind a pointer is out of reach, and a template argument is not a member. So `std::optional<Cache>` is caught (it holds its `Cache` by value) while `std::vector<Cache>` is not. The walk descends into the standard library's own internals, and what it finds there is binding: libstdc++ makes `_Prime_rehash_policy::_M_next_resize` `mutable`, so `copy_on_write<std::unordered_map<...>>` is **not** sendable — false, since [res.on.data.races] covers that `const` path, and the price of not having a rule that turned on inline namespaces. There was a rule stopping the walk at any type whose parent is `^^std` and reading its template arguments instead, justified by [res.on.data.races]; it was removed because `parent_of` answers `std::__cxx11` for `std::string` and `std::list`, so which container got the element-reading behaviour was an accident of inline namespaces — `std::list<Cache, MyAlloc<Cache>>` and `std::list<Cache>` disagreed on an unchanged element type.

The type is not synchronizable — `as_mutable()` rebinds the handle, so one object belongs to one thread. Share it by copying the handle and sending the copy. Nothing constructs it from a `std::shared_ptr`, and no accessor hands one out: a caller keeping its own would hold a write path the detach cannot see, while pinning the count above one forever.

### Header structure

The traits are variable templates, so their value is fixed at the point of instantiation. A TU that saw only *some* specializations would compute a different answer for the same type than one that saw them all — silently, and IFNDR across TUs. Each trait header therefore pulls in the full specialization set at its bottom; `tests/test_include_isolation.cpp` pins this. A full specialization written in a header must be `inline`, or every TU emits its own definition and the link fails — and so must a non-template function defined in one, which the `default_is_*` and `is_*_type` functions now are.

### The info-level face of the traits

Each trait also exposes `is_sendable_type(std::meta::info)`, `is_synchronizable_type(...)`, `is_lifetime_aware_type(...)`, named after the predicates of `<meta>`. Same answers as the variable templates, for code written on the reflection side.

They exist because `default_is_sendable` and `default_is_lifetime_aware` take a `std::meta::info` **as a function parameter**, not as a template argument. That parameter is not a constant expression, so nothing in those functions can splice it — no `if constexpr`, no `template for`, no `static_assert`; the bodies are plain loops and `if`s, and the two diagnostics they used to `static_assert` are `throw std::meta::exception` (GCC prints `what()` verbatim, with the instantiation stack).

Reading a trait back for a reflected type therefore goes through `detail::trait_value` in `utils.h` — `std::meta::extract<bool>(std::meta::substitute(^^trait, {type}))`. Going through the variable template is not a detour: the specializations in `containers.h`, `smart_pointers.h` and `vocabulary.h` *are* the answer for their types, and only a template-id sees them. Substitution resolves at evaluation time, so a specialization declared below the `substitute` call — or in the user's own TU, via `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` — still counts; `tests/test_include_isolation.cpp` pins that too. The same mechanism reaches concepts: `std::ranges::borrowed_range` is queried this way, since `<meta>` has no `is_borrowed_range_type`.

See `docs/thread-safety-audit.md` for the soundness holes this design has closed and the ones that remain open.
