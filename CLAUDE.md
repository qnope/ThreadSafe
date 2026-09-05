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

Each trait is one consteval function — `detail::diagnose_is_sendable(info)`,
the structural walk — asked through a `_v` constexpr variable:
`is_sendable_v<T>`. The answer is a plain `bool`; the explanation lives in the
`static_assert` messages at the point of use (the launcher, the
`synchronized_value` constructor), never inside the trait.

`_v` is the memo: the compiler instantiates a variable template once per `T`,
so a walk over a type's members runs once per translation unit however many
times the answer is asked for.

The recursion reads the traits reflectively, through the `_v` variable
(`detail::trait_value` substitutes `^^is_sendable_v`), so a specialization
written in a user's translation unit still reaches it.

The walk is **conservative**: everything it cannot prove is a no.
Unreflectable state, borrowed ranges, non-default types (a user-written copy,
move or destructor, or a constructor template that could hijack them —
`detail::is_default_type`) all fail before the member walk even starts. A "no"
therefore never needs to be asserted; only trust does.

Two questions are not answered but **rejected**: `void` and incomplete types
(unbounded arrays excepted — their element carries the answer) hit a
`static_assert` in `detail::assert_queryable_type`, instantiated by every
`_v`. Recursion goes through `_v` too, so a `void*` member or an incomplete
pointee poisons the whole question instead of answering false: complete the
type — or vouch for it — before asking.

### `is_unsafe_<trait>` — the one customization point, opt-in only

The safe traits are **closed**: `is_sendable`, `is_synchronizable` and
`is_lifetime_aware` have no user-facing specialization. Every walk begins by
asking the unsafe layer, and a claim short-circuits to **yes**.

The unsafe primaries derive from `std::false_type`. A specialization can only
*grant* trust; it cannot force a no — `false` and "no claim" are the same
thing, and both fall through to the walk. There is no third state. A
conditional claim is therefore written as a `bool_constant`, where `false`
simply means "nothing vouched, let the walk decide":

```cpp
template <class T>
struct threadsafe::is_unsafe_synchronizable<threadsafe::synchronized_value<T>>
    : std::bool_constant<threadsafe::is_sendable_v<T>> {};
```

The library holds itself to that rule: `std::vector`, `std::unique_ptr`,
`std::atomic`, `synchronized_value`, `copy_on_write` are all vouched for this
way, and so are the shapes the walk cannot prove — `const T` (via the
const-read walk), arrays, function types. The word `unsafe` appears wherever
knowledge is asserted instead of proved.

Because the claim is read by instantiating `is_unsafe_<trait><T>`, the
specialization must be written before the first question about that `T`.

Asking a trait about `X<T>` **completes** `X<T>` (the walk needs its members),
so a class template must be completable for every `T` the traits may be asked
about: usage-time invariants belong in constructors — see the
`static_assert(sendable<T>)` in `synchronized_value`'s constructor — never in
the class body.

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