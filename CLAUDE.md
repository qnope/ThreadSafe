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

Each trait is written twice over: a class template holding a
`static consteval TraitAnswer diagnose()` — the definition, which computes —
and a `_v` constexpr variable — the question, which remembers. Write
`is_sendable_v<T>` to ask; nothing calls `diagnose()` itself. The answer is a
`TraitAnswer`, not a `bool`: yes, or the reason it is no. (A third state,
*unanswered*, belongs to the unsafe traits below.)

`_v` is the memo: the compiler instantiates a variable template once per `T`,
so a walk over a type's members runs once per translation unit however many
times the answer is asked for. A specialization that delegates therefore reads
the memo — `return is_synchronizable_v<T>;` — and never inherits from another
trait, which would recompute the walk on every question.

`diagnose()` is total: it returns a reason, it never throws. A reason that must
be composed is built where it is thrown and never stored — see `detail::require`
in the launcher.

The recursion reads the traits reflectively, through the `_v` variable
(`detail::trait_value` substitutes `^^is_sendable_v`), so a specialization
written in a user's translation unit still reaches it.

### The path — where the reason was found

A `TraitAnswer` carries a reason *and* the path that leads to it. `diagnose()`
takes no arguments, and cannot: `_v` is nullary and the recursion reads it
reflectively, so no accumulated path could be handed down — and a path handed
down would make the answer depend on who asked, which is exactly what the memo
forbids. The path is therefore built the other way round. An answer's path is
relative to the type asked about, and every site that hands a deeper answer
back up prepends its own step:

```cpp
if (const auto answer = is_sendable_type(remove_cv(type_of(member))); !answer)
    return answer.prepend_path(path_step_of_member(member));
```

The steps are `detail::pointee_step` (`*`), `detail::referent_step` (`&`),
`detail::element_step` (`[]`), `path_step_of_base` (`base (Outer)`) and
`path_step_of_member`, which names the member *and* the type it is —
`borrowed (int*)`, so the path says what each hop is, not only where it went.
Steps join with `::`. `prepend_path` is a no-op on a yes, so a delegating
specialization stays one line:
`return is_synchronizable_v<T>.prepend_path(detail::pointee_step);`.

Prepend a step only when the answer came from a deeper question. When the
reason is about the type at hand — a pointer whose const stops at it, a
user-written copy constructor, a closure with captures — the reason replaces
the inner one and no step is added.

### The trait — which question the reason answers

A reason also remembers the trait that produced it. Each `_v` stamps its own
name on the way out — `is_sendable<T>::diagnose().with_trait("sendable")` —
and `with_trait` is a no-op on an already-stamped answer, so the deepest trait
keeps the credit: a `T*` that fails because its pointee is not synchronizable
travels up through `is_sendable` still saying *synchronizable*.

Reasons are therefore written as verb phrases with the failing entity as the
implicit subject — "borrows its referent instead of keeping it alive", "is a
pointer: the const stops at it" — because `detail::require` in the launcher
reads them into one sentence, root type first:

```
Outer::middle (Middle)::inner (Borrowing)::borrowed (int*)::*
    is not synchronizable because it carries no synchronization of its own: …
```

### `is_unsafe_<trait>` — the one customization point

The safe traits are **closed**: `is_sendable`, `is_synchronizable` and
`is_lifetime_aware` have no specialization at all. Each is one primary template
delegating to its structural walk, and every walk begins by asking the unsafe
layer.

Everything a walk cannot prove is written as a specialization of
`is_unsafe_sendable` / `is_unsafe_synchronizable` / `is_unsafe_lifetime_aware`.
Their primary template is **empty**: specializing it is what claims the type,
and the claim is final — yes or no, the safe trait returns it verbatim. A type
nobody claimed has no `diagnose()` to call, which is the unanswered state.

A specialization spells the claim out — `static consteval TraitAnswer
diagnose()`, with the return type written, never deduced:

```cpp
template <>
struct threadsafe::is_unsafe_synchronizable<MyType> {
    static consteval threadsafe::TraitAnswer diagnose() { return {}; }
};
```

The library holds itself to that rule: `std::vector`, `std::unique_ptr`,
`std::atomic`, `synchronized_value`, `copy_on_write` are all vouched for this
way, and so are the language shapes the walk cannot reach into — `T&`, `T*`,
`T[N]`, `const T`, function types. The word `unsafe` appears wherever knowledge
is asserted instead of proved.

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