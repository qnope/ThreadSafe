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

Each trait is a class template paired with a `_v` constexpr variable — the
shape of `std::is_same` / `std::is_same_v`. Write `is_sendable_v<T>` to ask the
question. The answer is a `TraitAnswer`, not a `bool`: yes, or the reason it is
no. (A third state, *unanswered*, belongs to the unsafe traits below.)

The recursion reads the traits reflectively, through the `_v` variable
(`detail::trait_value` substitutes `^^is_sendable_v`), so a specialization
written in a user's translation unit still reaches it.

### `is_unsafe_<trait>` — the one customization point

The safe traits are **closed**: `is_sendable`, `is_synchronizable` and
`is_lifetime_aware` hold only their own definition — the structural walk and
the language shapes (`T&`, `T*`, `T[N]`, `const T`, function types).

Everything else is asserted, not proved, and is written as a specialization of
`is_unsafe_sendable` / `is_unsafe_synchronizable` / `is_unsafe_lifetime_aware`.
Their primary template is **empty**: specializing it is what claims the type,
and the claim is final — yes or no, the safe trait returns it verbatim. A type
nobody claimed has no `value` to read, which is the unanswered state.

The library holds itself to that rule: `std::vector`, `std::unique_ptr`,
`std::atomic`, `synchronized_value`, `copy_on_write` are all vouched for this
way. The word `unsafe` appears wherever knowledge is asserted instead of proved.

Because the claim is read by instantiating `is_unsafe_<trait><T>`, the
specialization must be written before the first question about that `T`.

### `is_synchronizable<T>` (≈ Rust `Sync`)

True if a `T` may be used from multiple threads at the same time.

#### `is_synchronizable<const T>` — thread-safe read

True if a `const T` may be read from multiple threads at the same time.
**Const behind an indirection is never trusted**

### `is_sendable<T>` (≈ Rust `Send`)

True if a `T` may be sent from one thread to another.

- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<T>`

### Callables — there is no separate trait

Handing a callable to another thread *is* sending it, so `is_sendable` is the whole rule.

### `is_lifetime_aware<T>`

True if a `T` owns its data or keeps its referent alive. Ownership is **transitive**

### `copy_on_write<T>`

A shared `T` read through `const` only; `as_mutable()` copies first whenever the block is shared.