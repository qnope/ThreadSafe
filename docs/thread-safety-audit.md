# Thread-safety audit — ThreadSafe

**Date:** 2026-08-09 · **Toolchain:** GCC 16.1.0, `-std=c++26 -freflection` · **Scope:** all of `include/threadsafe`

The question this audit answers: *can you write code that this library accepts, which is
nevertheless a data race or a use-after-free?*

Yes — five distinct ways, four of which needed no user opt-in at all. All five are now closed.
Every claim below was reproduced with a compile, a link, or a sanitizer run; nothing here is
reasoning-only. Findings were produced by six independent audit passes and each was handed to a
separate reviewer whose job was to refute it by compiling the proof-of-concept themselves.

Status legend: **FIXED** — closed, with a regression test. **MITIGATED** — narrowed as far as
the type system allows. **OPEN** — cannot be expressed as a type-level trait.

---

## 1. `is_lifetime_aware` was not transitive — FIXED

**Severity: critical.** This was the library's only defence against a task outliving its data,
and one layer of composition defeated it.

`is_sendable` recurses through bases and members. `is_lifetime_aware` did not: it was
`!std::ranges::borrowed_range<T>`, which is true for essentially every class type, and it never
looked inside. So every "does not keep its referent alive" rule — `T*`, `T&`,
`std::reference_wrapper<T>`, the borrowed-range rule — applied only at the outermost type.

```cpp
struct Borrow { std::atomic<int>* counter; };   // an ordinary struct

static_assert(!is_lifetime_aware<std::atomic<int>*>);              // caught bare
static_assert(is_sendable<Borrow> && is_lifetime_aware<Borrow>);   // NOT caught wrapped
```

`launch_task` rejected the bare pointer and accepted the identical pointer inside a struct.

```cpp
threadsafe::asynchronous_task_launcher launcher;   // joins in its destructor
auto* counter = new std::atomic<int>{0};
launcher.launch_task([](Borrow b) { std::this_thread::sleep_for(300ms);
                                    b.counter->fetch_add(1); },
                     Borrow{counter});
delete counter;                                     // freed while the task sleeps
```

```
==40066==ERROR: AddressSanitizer: heap-use-after-free on address 0x6020000000d0
WRITE of size 4 at 0x6020000000d0 thread T1
    #0 main::'lambda'(Borrow)::operator()(Borrow) const uaf.cpp:16
freed by thread T0 here:
    #1 main uaf.cpp:21
```

Confirmed simultaneously `sendable && lifetime_aware` before the fix, with no user
specializations: `struct{T*}`, `struct{T&}`, `struct{reference_wrapper<T>}`, `struct{span<T>}`,
`struct X : span<T>`, `vector<T*>`, `map<K,T*>`, `tuple<T*>`, `T*[4]` — for any `T` the library
considers synchronizable. `asynchronous_task_launcher.h` sold this bound as "≈ Rust's `'static`";
Rust's `'static` is transitive over fields, this was not transitive at all.

**Fix.** `lifetime_aware.h` now computes the by-value case structurally by reflection, mirroring
`default_is_sendable`, with the pointer/reference/view rules as terminators.

Reflection alone cannot distinguish a pointer a type *owns* from one it *borrows*, so owning
std types need explicit rules exactly as they already did for `is_sendable` — otherwise the
recursion descends into `std::vector`'s own internal `T*` and answers `false` for every
container. Those rules were added to `containers.h`, `smart_pointers.h` and the new
`vocabulary.h`, propagating through the element type:

```cpp
template <class T, class A>
constexpr bool is_lifetime_aware<std::vector<T, A>> =
    is_lifetime_aware<T> && is_lifetime_aware<A>;
```

Side effects worth noting: `std::vector<std::string_view>` now correctly reports *borrowing*,
and so does `std::pmr::vector<int>` — a `polymorphic_allocator` holds a `memory_resource*`.

---

## 2. Type erasure through a polymorphic base — FIXED (narrowed)

**Severity: critical.** `is_sendable<std::unique_ptr<T, D>>` tested the *static* pointee type,
but `unique_ptr<Base>` erases the dynamic type:

```cpp
struct Base { virtual ~Base() = default; };
struct Derived : Base { Derived(const Derived&) {} };   // user-provided copy ⇒ not sendable

static_assert(!is_sendable<Derived>);                // correctly rejected
static_assert(is_sendable<std::unique_ptr<Base>>);   // ...and smuggled across anyway
```

C++ has no `Box<dyn Trait + Send>` to carry the promise along with the erased type.

**Fix.** `is_sendable<unique_ptr<T, D>>` now also requires `detail::dynamic_type_is_known<T>`
(`!is_polymorphic_v<T> || is_final_v<T>`). A polymorphic, non-final pointee is refused; a `final`
type or a non-polymorphic one is unaffected. A hierarchy that genuinely is sendable must say so
with an explicit `is_sendable` specialization on the base — which is then an auditable,
deliberate assertion rather than an accident.

The same erasure exists for `std::shared_ptr` in two forms that are **OPEN** (see §9): the
type-erased deleter, and `is_synchronizable<Base>` necessarily covering every derived type.

---

## 3. Translation units disagreed silently — FIXED

**Severity: high.** All four traits are variable templates, so a trait value was a property of a
*(type, translation unit)* pair rather than of a type. The library's own header decomposition
produced the divergence with no user opt-in:

```
TU A (includes only sendable.h)  says vector<int> sendable = false
TU B (includes threadsafe.h)     says vector<int> sendable = true
```

Both compile, link cleanly, and produce no diagnostic — IFNDR. `is_sendable<std::atomic<int>&>`
diverged the same way (`false` vs `true`) because the `std::atomic` rule lives in
`synchronizable.h`. A `static_assert` passing in one TU proved nothing about another.
`tests/test_lifetime_aware.cpp` was itself on the wrong side of this.

**Fix.** Each trait header now pulls in the complete specialization set at the bottom, after the
trait is declared. The nested includes are no-ops while a header is still in flight
(`#pragma once`) and nothing is instantiated during header processing, so the cycle is benign.
Any entry point into the library now gives the same answer.

`tests/test_include_isolation.cpp` pins this: it includes exactly one granular header and asserts
the answers match the umbrella's.

> A cleaner long-term structure would move the granular headers under `threadsafe/detail/` and
> make `threadsafe.h` the only public entry point, removing the hazard by construction rather
> than by discipline. The bottom-include approach was chosen because it preserves the current
> public header layout.

**Related, found while fixing this:** a full (explicit) specialization of a variable template is
a *variable definition*, not a template, so one defined in a header needs `inline` or every TU
emits its own and the link fails with duplicate symbols. This bit the `std::stop_token` rules
during development. `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` emits `inline` for this reason.

---

## 4. The callable was never checked for sendability — FIXED

**Severity: high.** `launch_task` required `safe_callable<F> && lifetime_aware<F>`, never
`sendable<F>` — although `std::jthread` decay-copies the callable into the new thread and
destroys that copy **on that thread**.

```cpp
struct EmptyUserCopy { EmptyUserCopy(const EmptyUserCopy&) {} void operator()() const {} };
static_assert(is_safe_callable<EmptyUserCopy>);   // empty ⇒ safe to share
static_assert(!is_sendable<EmptyUserCopy>);       // user-provided copy ⇒ not sendable
// ...and launch_task accepted it anyway.
```

The library rejected this type as an *argument* and accepted it as the *callable*.

**Fix.** Initially `sendable<std::decay_t<F>>` on both entry points. Then `is_safe_callable` was
rebased on `is_sendable`, keeping an empty-class escape hatch guarded by the same
user-provided-copy check — which fixed `EmptyUserCopy` but not the general case, see §8.
**Final: the trait is gone** and both entry points require `sendable<std::decay_t<F>>` again.

---

## 5. `std::jthread` injected an unchecked argument — FIXED

**Severity: medium.** `std::jthread` prepends a `std::stop_token` whenever the callable accepts
one. That argument never passed through the `Args` constraints — a zero-argument call satisfies
the fold vacuously — and `is_sendable<std::stop_token>` was `false`, so the launcher was
injecting an argument its own trait system rejected.

**Fix.** The trait was wrong, not the launcher: [stoptoken.general] guarantees concurrent
observer calls and `request_stop` are race-free, so `std::stop_token` and `std::stop_source` are
synchronizable *by specification*, and both share ownership of a refcounted stop state.
`vocabulary.h` states this, and `asynchronous_task_launcher` now carries a `static_assert` that
the injected argument satisfies the traits on its own rather than assuming it.

---

## 6. Usability defects that pushed users toward unchecked assertions — FIXED

Not soundness bugs on their own, but each one ended the same way: the user writes
`is_synchronizable<T> = true` to get moving, which is an unchecked promise with no marker.

| Symptom | Cause | Fix |
|---|---|---|
| `std::array`, `std::mutex`, `std::function`, `std::any`, `std::promise`, `struct { char buf[64]; }` **hard-errored** | `default_is_sendable` had no array branch, so any C-array member hit the class/union assertion — an uncatchable `static_assert` plus an uncaught `std::meta::exception`, not SFINAE-friendly | `is_sendable<T[N]>` / `<T[]>` follow the element type; same for `is_lifetime_aware` |
| `std::pair`, `std::tuple`, `std::optional`, `std::deque`, `std::list` reported **not sendable** | libstdc++ declares constrained-but-semantically-defaulted special members (`pair& operator=(const pair&) requires ...`), which `std::meta::is_defaulted` reports as user-provided; node-based containers expose internal raw pointers | explicit rules in the new `vocabulary.h` and in `containers.h` |
| `std::complex<double>` **hard-errored** | its value lives in `__complex__ T`, a compiler extension type that is neither scalar, class nor union | explicit rule in `vocabulary.h` |
| a plain function could not be launched | `is_lifetime_aware<T*> = false` also matched function pointers | `is_lifetime_aware<F*> = true` for function types — code has static storage duration |
| `is_safe_callable<void (*const)()>` was `false` | the `F*` partial specialization does not match a top-level-`const` pointer | the trait was retired (§8); `is_sendable` already forwards cv-qualified queries |

**Deliberately still a hard error:** `is_sendable` on an incomplete type, which breaks the pimpl
idiom (`struct Widget { std::unique_ptr<Impl> p; };`). Answering `false` from incompleteness
would make the answer depend on the point of instantiation — reintroducing §3, a soundness bug,
to fix a usability one. The diagnostic now names the remedy: specialize `is_sendable` for the
pimpl'd type.

---

## 7. Reflection sees no members on a closure type — FIXED

**Severity: high.** GCC 16 reports zero bases and zero non-static data members for a lambda's
closure type, whatever it captures. `all_bases_and_members_sendable` therefore iterated over
nothing and returned true, and `is_lifetime_aware` did the same:

```cpp
std::string local;
auto f = [&local] { local += "x"; };
static_assert(is_sendable<decltype(f)>);        // held, over a dangling reference
static_assert(is_lifetime_aware<decltype(f)>);  // held too
```

Only the launcher was shielded, and only by accident: the old `is_safe_callable` asked for
`is_synchronizable || empty`, and a capturing closure is neither. Rebasing the trait on
`is_sendable` (§4) would have handed that blindness straight to `launch_task`.

**Fix.** `detail::has_unreflectable_state<T>()` in `utils.h`: a class that occupies storage
(`!is_empty_v`) while reflecting no bases and no members is hiding state, so `is_sendable` and
`is_lifetime_aware` both answer false. Polymorphic classes are excluded — their unaccounted size
is the vptr, and `PolyBase { virtual ~PolyBase() = default; }` would otherwise stop being
sendable. The rule is conservative in the right direction: it costs captureful lambdas, which
were already rejected, and buys back nothing that reflection can actually inspect.

---

## 8. The empty-callable escape hatch was inherited — FIXED by deleting the trait

**Severity: high.** §4 left `is_safe_callable<F>` = `is_sendable<F> || (class && is_empty_v<F> &&
no user-provided copy/move)`. The guard on that second disjunct inspected `F` itself. Emptiness,
however, is inherited — and so is a base's user-provided copy constructor:

```cpp
struct EmptyUserCopy { EmptyUserCopy(const EmptyUserCopy&) {} void operator()() const {} };
struct Derived : EmptyUserCopy {};              // still empty; its own members are defaulted

static_assert(!is_sendable<Derived>);           // correct: the recursion sees the base
static_assert(is_safe_callable<Derived>);       // laundered
launcher.launch_scoped_task(Derived{});         // accepted — §4 reopened, one level down
```

`is_sendable` recurses through bases and gets this right; the escape hatch existed only to skip
that recursion.

**Fix.** The disjunct bought nothing: an empty class is sendable exactly when no class in its base
chain has a user-provided copy/move, which is the answer the trait should have given. So
`is_safe_callable` is **removed**, along with its redundant `F*` specialization for function types
(`is_sendable<T*>` = `is_synchronizable<T>` already covers function pointers). The launcher
requires `sendable<std::decay_t<F>>`. Every assertion in the old `test_safe_callable.cpp` holds
verbatim under `is_sendable` and now lives in `test_sendable.cpp`; the `Derived` case above is a
regression test.

The lesson generalizes: a second trait definitionally equal to `is_sendable` on everything that
matters reads as a fourth axis next to sync/send/lifetime, so it attracts "improvements" that only
one of the two traits then gets. Keeping the axes minimal is a soundness property, not a style
preference.

---

## 9. Remaining holes — OPEN

These cannot be expressed as type-level traits. They are the honest limits of the design.

**Global, static and `thread_local` state.** Documented already, but worth stating sharply:
`is_empty_v` is used as the proxy for "stateless", and static data members do not count toward
emptiness. So this is accepted and races:

```cpp
struct Counter { static int hits; void operator()() const { for (int i=0;i<100000;++i) ++hits; } };
// launch two of them → hits = 100000 (expected 200000), reproducibly
```

Emptiness means *no per-object state*. It is not a safety proof, and `CLAUDE.md` says so where the
callable rules are described. The same applies to an empty allocator backed by a `thread_local`
arena, which `containers.h` blesses.

**`shared_ptr` ownership is assumed, not proven.** `is_lifetime_aware<shared_ptr<T>> = true`,
but the aliasing constructor (`shared_ptr(other, ptr)`) and a no-op deleter both produce
`shared_ptr`s that keep something *other* than the pointee alive. Neither is visible in the type.

**`shared_ptr`'s deleter is type-erased** into the control block and runs on whichever thread
drops the last reference. A deleter capturing a borrow into the sender's scope is invisible.

**`is_synchronizable` is an unsafe escape hatch.** One line blesses `T&`, `T*`, `shared_ptr<T>`,
`weak_ptr<T>` and `reference_wrapper<T>` at once, and for a polymorphic base
it covers every derived type reached through it. Nothing distinguishes a correct assertion from a
false one. Mitigation: `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(T)` makes every such promise
greppable — the Rust `unsafe impl` marker this design otherwise lacks.

**`launch_scoped_task` cannot exclude publication.** `join()` proves the task finished, not that
it published nothing. A task handed both a caller-scope reference and a longer-lived object can
store the former into the latter. Documented in the header.

**Exceptions.** An exception escaping a task calls `std::terminate`; `launch_scoped_task` looks
synchronous but cannot propagate one to the caller.

**`synchronized_value`'s guard leaks its reference.** `auto g = v->lock(); T* p = &*g;` carries the
reference out of the critical section, and nothing in the language sees it. The alternative shape —
`with_lock(f)` constraining `invoke_result_t<F, T&>` to `lifetime_aware` — would close the *return*
path only, never a by-reference capture into an enclosing scope. The guard was chosen because it is
simpler to read for the same real safety.

**`lock_shared()` trusts `const`.** Several readers run concurrently under the shared lock, so a
`const` member function of `T` that mutates a `mutable` member without synchronization races. No
trait here distinguishes a `mutable` cache from an atomic one.

**`synchronized_value` has no reentrancy or lock ordering.** `std::shared_mutex` is not recursive:
two nested `lock()` calls on the same object deadlock, and so does taking two `synchronized_value`s
in opposite orders on two threads. Neither is a type-level property.

**`copy_on_write` sees the `mutable` keyword, not the writes.** Its sendable rule waives
`is_synchronizable<T>` only when `detail::has_mutable_state(^^T)` is false, and that helper walks
bases, by-value members, arrays, and — inside namespace `std` only — template arguments. So a `T`
whose `const` member function writes through a pointer member, or touches a static or
`thread_local`, passes the guard and races between two readers. The keyword is a proxy for
logically-const mutation, the same way `is_empty_v` is a proxy for statelessness above.

**And it over-rejects the types that handle it correctly.** A `T` carrying a `mutable std::mutex`
or a `mutable std::atomic<int>` is precisely one that *is* safe to read concurrently, yet the
keyword is all the guard sees, so it demands `is_synchronizable<T>`. `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE`
is the way out, which puts the promise where it belongs — greppable, at the type.

**Namespace `std` is trusted wholesale.** `has_mutable_state` stops at any type whose parent is
`^^std` and reads its template arguments instead of its members. The justification is
[res.on.data.races]: a standard library `const` member function does not modify what other threads
can reach. It is a real guarantee, but it is taken on faith about the *implementation* — a
libstdc++ internal is never inspected. It also means the guard is blind to a user type reached
through a std type by a route other than a template argument.

**`copy_on_write::as_mutable` decides on an approximate count.** `use_count()` is a relaxed load
and the standard calls it approximate in a multithreaded context. Reading 1 is nonetheless
conclusive here — no other thread can create a reference, since every handle sharing the block is
itself non-`synchronizable` and so reachable from one thread only — but the *ordering* is not free:
the acquire fence in the unique branch is what pairs with the releasing decrements of the handles
that are gone, so their reads of the value precede the writes the returned reference opens.
Correctness rests on that pairing, not on a `shared_ptr` guarantee.

**Detaching is untested.** The suite is `static_assert`-only and `std::shared_ptr` is not
`constexpr` in C++26, so `test_copy_on_write.cpp` pins the traits and the shapes — that a
non-`const` handle still yields `const T&`, that the variadic constructor does not outrank the copy
constructor — and nothing about the runtime behaviour of the copy. That the type detaches when
shared and mutates in place when unique is reviewed code, not tested code.

---

## Files changed

| File | Change |
|---|---|
| `lifetime_aware.h` | rewritten: structural recursion, array and function-pointer rules |
| `sendable.h` | array specializations; `dynamic_type_is_known`; actionable incomplete-type diagnostic; bottom includes |
| `containers.h` | lifetime rules for every container; `deque`/`list`/`forward_list` sendability |
| `vocabulary.h` | **new** — `pair`, `tuple`, `optional`, `variant`, `array`, `complex`, `stop_token`, `stop_source` |
| `smart_pointers.h` | polymorphic-erasure guard on `unique_ptr`; `unique_ptr` lifetime rule |
| `safe_callable.h` | **deleted** (§8) — `is_sendable` is the whole rule for callables |
| `utils.h` | **new** — special-member reflection helpers; `has_unreflectable_state` (§7) |
| `synchronizable_base.h` | `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` |
| `asynchronous_task_launcher.h` | `F` must be `sendable` (§4, §8); stop_token assertion |
| `sendable.h`, `lifetime_aware.h` | `has_unreflectable_state` guard (§7) |
| `tests/test_soundness_regressions.cpp` | **new** — one block per hole above |
| `synchronized_value.h` | **new** — a `shared_mutex`-guarded `T`; the first checked way to share a user type |
| `tests/test_include_isolation.cpp` | **new** — pins the §3 fix |
| `tests/test_synchronized_value.cpp` | **new** — traits, guard shape, composition with the launcher |
| `copy_on_write.h` | **new** — a shared `T` read without a lock, copied only on write |
| `utils.h` | `has_mutable_state` (§9) — the guard behind `copy_on_write`'s sendable rule |
| `tests/test_copy_on_write.cpp` | **new** — traits, the `mutable` recursion, const-only access, copy-constructor hijack |
| `tests/test_safe_callable.cpp` | **deleted** (§8) — its assertions moved verbatim into `test_sendable.cpp` |
| `threadsafe.h`, `tests/CMakeLists.txt` | drop the deleted header and TU |

All 8 test translation units compile clean, and the multi-TU link case now agrees. Two
pre-existing assertions changed meaning: a plain function pointer is now launchable (§6), and
an empty class deriving from one with a user-provided copy is no longer accepted as a callable
(§8).
