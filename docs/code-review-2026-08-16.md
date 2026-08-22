# ThreadSafe — code review, 2026-08-16

Every claim below was compile-probed (and several linked and run) against
g++-16.1.0 with `-std=c++26 -freflection`. Where a claim could not be verified
by compiling, it is labelled. Items CLAUDE.md documents as deliberate choices
are excluded.

Method: a 15-agent audit — seven independent lenses (sendable soundness,
lifetime soundness, `copy_on_write`, `synchronized_value` + launcher, std
coverage and libstdc++ fragility, compile time, readability/docs/build), each
followed by an adversarial verifier whose default was to refute, then a
deduplicating synthesis. 61 findings survived verification.

Re-verified independently by hand afterwards: U1, U3, U4, U5, U6, U7, U9, U11,
C1, C2, C4, T1, B2. Where a hand probe disagreed with the agent, the text was
corrected — notably U3, whose namespace list was partly wrong: `^^std::string`
reflects the *alias*, whose parent is bare `std`, not the class
`std::__cxx11::basic_string`. The finding itself stands, and the answer flip it
predicts was reproduced.

No source file was modified to produce this report; every patch was applied to
a copy of `include/`.

---

## 1. UNSOUNDNESS — the traits accept genuinely unsafe code

This section is **not** empty. Eleven distinct holes survived verification; four of them run user code or race at runtime in a probe I executed.

### U1. A constructor or assignment *template* hijacks copy/move and is invisible to the guard
`include/threadsafe/utils.h:65` (consumed at `sendable.h:74`)

**Claim.** `is_copy_move_constructor_assignment` filters on `is_copy_constructor` / `is_move_constructor` / `is_copy_assignment` / `is_move_assignment`; per [class.copy.ctor]/2 a member *template* is never any of those, yet overload resolution picks it — so a type with a hijacking forwarding-reference constructor is `is_sendable`, and its user code runs on the destination thread.

**Evidence.** `is_sendable<TmplMove>`, `<TmplCopy>`, `<TmplAssign>`, `<VariadicCtor>` all true (controls `S(const S&, int=0)` and `S(S&)` correctly false); linked runtime probe printed `[TmplMove user ctor ran on WORKER thread]`.

**Fix.** Add a viability-checked template scan and OR it into `default_is_sendable`'s disjunction at `sendable.h:74`:

```cpp
inline consteval bool has_hijacking_copy_move_template(std::meta::info type) {
    const auto ctx  = std::meta::access_context::unchecked();
    const auto lref = std::meta::add_lvalue_reference(type);
    for (std::meta::info m : std::meta::members_of(type, ctx)) {
        if (!std::meta::is_function_template(m)) continue;
        if (!std::meta::is_constructor_template(m)
            && !std::meta::is_operator_function_template(m)) continue;
        if (std::meta::is_deleted(m)) continue;
        if (!std::meta::can_substitute(m, {lref})) continue;
        auto params = std::meta::parameters_of(std::meta::substitute(m, {lref}));
        if (params.empty() || std::meta::type_of(params[0]) != lref) continue;
        bool rest_defaulted = true;
        for (std::size_t i = 1; i < params.size(); ++i)
            if (!std::meta::has_default_argument(params[i])) rest_defaulted = false;
        if (rest_defaulted) return true;
    }
    return false;
}
```

Verified on a patched tree: the four shapes drop to false; genuine converting templates (`requires (!same_as<remove_cvref_t<U>,C>)`) and `std::default_delete<int>` stay true; all ten test TUs green. The trailing-defaulted-parameter check matters — `template<class U> S(U&&, int = 0)` escapes a `params.size() == 1` test.

### U2. `*sv.lock()` releases the lock while handing out a live `T&`
`include/threadsafe/synchronized_value.h:24`

**Claim.** `lock()` returns a prvalue guard and `operator*`/`operator->` are unqualified, so the guard dies at the end of the full-expression while the `T&`/`T*` it produced stays valid — a silent, permanent unlock.

**Evidence.** `auto& r = *sv.lock();` compiles with zero warnings under `-Wdangling-reference -Wdangling-pointer`; running it, a second thread's `sv.lock()` succeeded and joined while `r` was live, then `r = 9` was an unsynchronised write.

**Fix.** Ref-qualify the accessors and add the callback form that replaces the idiom they cost:

```cpp
T& operator*()  const&  noexcept { return *value_; }
T* operator->() const&  noexcept { return  value_; }
T& operator*()  const&& = delete;
T* operator->() const&& = delete;

template <class F> requires std::invocable<F&, T&>
decltype(auto) with_lock(F&& f) { std::unique_lock l{mutex_}; return std::invoke(f, value_); }
template <class F> requires std::invocable<F&, const T&>
decltype(auto) with_lock_shared(F&& f) const { std::shared_lock l{mutex_}; return std::invoke(f, value_); }
```

Honest cost, measured: the safe one-liner `sv.lock()->size()` stops compiling. `with_lock` is the replacement. All ten TUs green under the patch.

### U3. `has_mutable_state`'s `parent_of == ^^std` stop misses nested std namespaces
`include/threadsafe/utils.h:38`

**Claim.** The stop compares only the *immediate* enclosing namespace; libstdc++ puts most interesting types one level deeper, so the recursion walks implementation internals instead of template arguments — and `is_sendable<copy_on_write<std::list<Cache, MyAlloc<Cache>>>>` becomes true for a `Cache` that writes from `const`.

**Evidence.** Measured parents: `std::list<int>` → `std::__cxx11`, `std::string` → `std::__cxx11`, `std::filesystem::path` → `std::filesystem::__cxx11`, `std::chrono::milliseconds` → `std::chrono`, `std::pmr::polymorphic_allocator` → `std::pmr`; only `std::vector` is bare `std`. Result: `list<Cache,MyAlloc> mut=0 send<cow>=1` versus `list<Cache> (default alloc) mut=1 send<cow>=0` — the correct answer today is an accident of `std::allocator` living in bare `std`. Two `launch_task` calls sharing one such handle and both calling a writing `const` method compile clean. Second consequence: `has_mutable_state(^^std::filesystem::path)` *throws* `std::meta::exception` (masked today only by `&&` short-circuit).

**Fix.** Complete the [res.on.data.races] argument rather than widening it:

```cpp
inline consteval bool encloses_std(std::meta::info e) {
    while (e != ^^::) { if (e == ^^std) return true; e = std::meta::parent_of(e); }
    return false;
}
// utils.h:38
if (encloses_std(type)) { ... }
```

Verified: the one intended flip, `path` returns false instead of throwing, a 20-type before/after battery is otherwise byte-identical, all ten TUs green. Pin with `static_assert(!is_sendable<copy_on_write<std::list<Cache, MyAlloc<Cache>>>>)` beside `tests/test_copy_on_write.cpp:79`.

### U4. `synchronized_value` grants N-way concurrent `const` access without the guard `copy_on_write` applies
`include/threadsafe/synchronized_value.h:77`

**Claim.** `lock_shared()` hands `const T&` to N threads simultaneously, but `is_synchronizable<synchronized_value<T>> = is_sendable<T>` omits the `is_synchronizable<T> || !has_mutable_state(^^T)` factor that `copy_on_write.h:55` spells out for exactly this situation.

**Evidence.** The same `Memo` type is *refused* by `copy_on_write` and *accepted* by `synchronized_value`, then rides through `shared_ptr` into `launch_task`. Runtime: 8 threads × 200 000 `lock_shared()->get()`, `-O2`, expected 1 600 000, got 1 599 214 and 1 599 093 on two runs. Live data race, no sanitizer needed.

**Fix.** Either mirror the `copy_on_write` condition in the trait, or — better — select the lock instead of the answer, so the type stays usable and the race is closed at the source:

```cpp
static constexpr bool const_access_is_race_free =
    is_synchronizable<T> || !detail::has_mutable_state(^^T);
using const_guard = value_guard<const T,
    std::conditional_t<const_access_is_race_free,
                       std::shared_lock<std::shared_mutex>,
                       std::unique_lock<std::shared_mutex>>>;
```

Both verified green. Option 2 silently changes the concurrency of existing code — but only of code that was racing.

### U5. A user-provided destructor is never checked, yet it runs on the destination thread
`include/threadsafe/sendable.h:74`

**Claim.** The guard covers copy/move construction and assignment and stops there; `std::jthread` destroys the decay-copied callable and arguments on the worker.

**Evidence.** An *empty* thread-affine RAII guard (`std::is_empty_v == 1`, so `has_unreflectable_state` does not fire, no bases, no members) is `is_sendable` and `is_lifetime_aware` and passes `can_launch_task`. Running it printed `[lock on MAIN] / [unlock on MAIN] / worker body / [unlock on WORKER] / [unlock on WORKER]` — `std::mutex::unlock` from a thread that never locked it, UB under [thread.mutex.requirements.mutex].

**Fix.**

```cpp
inline consteval bool has_user_provided_destructor(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    for (std::meta::info m : std::meta::members_of(type, ctx))
        if (std::meta::is_destructor(m) && std::meta::is_user_provided(m)) return true;
    return false;
}
```

Verified: the empty guard drops to false, `= default` destructors stay true, `vector`/`string`/`unique_ptr` unaffected (they hit explicit specializations first), all ten TUs green. **Conservatism cost, measured and real:** `struct BenignOwner { std::unique_ptr<int> p; ~BenignOwner() {} };` becomes non-sendable. That is the same price the copy/move guard already charges. If you decide destructors are out of scope, the empty-guard shape needs a paragraph in CLAUDE.md and a pin in `test_soundness_regressions.cpp`, because today it is indistinguishable from the `EmptyUserCopy` case at lines 137-151, which the suite treats as a bug.

### U6. `as_mutable()` hands out an escaping `T&`; the `use_count()==1` fast path holds only for the instant it returns
`include/threadsafe/copy_on_write.h:32`

**Claim.** The sole-owner precondition is checked once and never revalidated; the returned reference outlives it, and the same thread that owns the reference can raise the count.

**Evidence.** `std::string& escaped = c.as_mutable();` at count 1 (no detach), then `launch_task(..., c)` raises the count, then `escaped.append(...)` writes what the worker reads — compiles clean. Runtime shows a second, single-threaded failure: after `auto& r = a.as_mutable(); auto b = a; a.as_mutable();`, the program printed `stale ref still points at old block: 1` and `b sees: hello!!! | a sees: hello` — the write vanished.

**Fix.** Add the scoped form and document it as the default; keep `as_mutable()` (pinned by `tests/test_copy_on_write.cpp:106,113-116`).

```cpp
template <class F> requires std::copy_constructible<T> && std::invocable<F, T&>
decltype(auto) with_mutable(F&& f) { return std::invoke(std::forward<F>(f), as_mutable()); }
```

Verified working. Be honest in the docs that `with_mutable` is a mitigation, not a closure: a callback can still stash the reference. Also document that the reference is invalidated by the next copy of the handle.

### U7. `__restrict__`-qualified pointers match neither pointer rule and answer true for both traits
`include/threadsafe/sendable.h:56`, `include/threadsafe/lifetime_aware.h:66`

**Claim.** `__restrict__` is part of the pointer type for partial ordering and `std::meta::remove_cv` does not strip it, so `is_sendable<T*>` (`sendable.h:25`) and `is_lifetime_aware<T*>` (`lifetime_aware.h:26`) are never selected; the primary templates then answer true via `is_scalar_type` and the non-class fallthrough.

**Evidence.** `struct Borrower { int* __restrict__ p; };` — `is_sendable`, `is_lifetime_aware` both true, and `L.launch_task([](Borrower x){ *x.p = 42; }, b)` on a struct pointing at a stack local compiles. `is_lifetime_aware<int& __restrict__>` is silently true; `is_sendable<int& __restrict__>` is a hard error.

**Fix.** Normalise at the info level rather than hoping every qualifier spelling hits a partial specialization. In `default_is_sendable`, before the scalar shortcut:

```cpp
if (std::meta::is_pointer_type(type))
    return is_synchronizable_type(std::meta::remove_cv(std::meta::remove_pointer(type)));
if (std::meta::is_reference_type(type))
    return is_synchronizable_type(std::meta::remove_cv(std::meta::remove_reference(type)));
```

and in `default_is_lifetime_aware`, before the borrowed-range check:

```cpp
if (std::meta::is_pointer_type(type)) return std::meta::is_function_type(std::meta::remove_pointer(type));
if (std::meta::is_reference_type(type)) return false;
```

Verified: all four assertions invert, function pointers and pointers-to-member stay true, all ten TUs green. **Land the `std::valarray` rule in the same commit** — `_Tp* __restrict__ _M_data` is the only reason `is_lifetime_aware<std::valarray<int>>` is true today, and that answer is correct.

Reachability is the reason this is high and not critical: `__restrict__` is a GCC extension no portable code types by accident. It is plausible in numeric/DSP code, which is where restrict is actually used.

### U8. Type erasure into a raw byte buffer launders a reference-capturing lambda past both traits
`include/threadsafe/utils.h:13` — **no fix exists; document it**

**Claim.** `has_unreflectable_state` fires only on a type that is non-empty, non-polymorphic, and reflects zero bases and zero members. Placement-new the closure into `alignas(max_align_t) unsigned char buf_[32]` inside an ordinary struct and every condition is gone.

**Evidence.** `!is_sendable<decltype(capturing)>` for the raw `[&local]` closure; `is_sendable<inplace_function>` and `is_lifetime_aware<inplace_function>` for the same closure erased into the buffer; `L.launch_task(erased)` compiles — a task borrowing a stack local, accepted. A bisect confirms the mechanism: a `std::_Any_data`-shaped union carrying a `void*` is rejected, the identical shape with a byte buffer is accepted.

**Why no fix.** The buffer fully accounts for the type's size, so no reflection-only accounting or layout heuristic distinguishes it from a legitimate POD buffer. This is the boundary of what reflection can see, and it covers every small-buffer callable in the wild (folly::Function, etl::delegate, in-house FixedFunction). Nothing here argues for reintroducing `is_safe_callable` — different mechanism entirely.

**Fix.** Document the boundary in CLAUDE.md's Callables section next to the existing static-data-member note, pin the std erasers explicitly (F5 below) so the library *states* the rule instead of depending on a `void*` happening to sit inside `std::_Any_data`, and add a `tests/test_soundness_regressions.cpp` case marking the limitation as known.

### U9. `const_cast` in a `const` method defeats the `mutable` proxy for `copy_on_write`
`include/threadsafe/copy_on_write.h:55`

**Claim.** `has_mutable_state` looks for the `mutable` keyword; a `const` method writing through `const_cast` produces the same racy shape with no keyword to find — and it is not even UB, because `make_shared` creates the object non-const.

**Evidence.** `is_sendable<copy_on_write<ConstCastCache>>` holds with `has_mutable_state(^^ConstCastCache) == 0`, and two `launch_task` calls sharing one handle and each calling the writing `get() const` compile clean. Pointer-ish variants are still caught by the `is_sendable<T>` factor; only inline-storage-plus-hidden-write survives.

**Assessment.** Unlike U3, the user has to defeat the language's const machinery first, and `utils.h:21` already says "only the `mutable` keyword". No reflection predicate can close this — function bodies are not reflectable in P2996. The honest options are documentation and an escape hatch.

**Fix.** Make the predicate user-specializable:

```cpp
template <class T> constexpr bool has_interior_mutability = detail::has_mutable_state(^^T);

template <class T>
constexpr bool is_sendable<copy_on_write<T>> =
    is_sendable<T> && (is_synchronizable<T> || !has_interior_mutability<T>);
```

Verified: a user specialization flips `cow<ConstCastCache>` to false, `cow<int>` unchanged, all ten TUs green. Amend the comment at `copy_on_write.h:49-51` and CLAUDE.md to say what the guard detects: the `mutable` keyword only.

### U10. `launch_scoped_task` cannot stop a borrow from escaping
`include/threadsafe/asynchronous_task_launcher.h:29`

**Claim.** Dropping the `lifetime_aware` requirement is justified by "the launcher joins", but the join bounds the *invocation*, not the *borrow*. `F` need only be `sendable`, which every free function satisfies.

**Evidence.** Compiles clean; plain run prints `after scope: g_leak still points at dead storage: 0x…`; under ASan, `stack-use-after-scope … WRITE of size 4 … thread T3 … Address is located in stack of thread T0`. The escaping helpers (`steal` parks the reference in a global; `spawn_detached` hands it to a detached thread) are plain free functions.

**Fix.** No type-level fix exists in C++ — this is Rust's `thread::scope` without the `'scope` lifetime. State it as a contract on the declaration:

```
// PRECONDITION: f must not outlive its own invocation — it must not store a
// reference to any argument beyond the call, nor hand one to a thread it does
// not itself join. The traits cannot check this; the join bounds the
// invocation, not the borrow.
```

and pin it as a comment beside `tests/test_asynchronous_task_launcher.cpp:52`.

### U11. `is_lifetime_aware` has no `T[]` rule, and a flexible array member of borrows passes `launch_task`
`include/threadsafe/lifetime_aware.h:33`

**Claim.** `is_sendable` specializes both `T[N]` (`sendable.h:28`) and `T[]` (`sendable.h:30`); `is_lifetime_aware` specializes only `T[N]`, so arrays of unknown bound fall through to the non-class `return true`.

**Evidence.** `!is_lifetime_aware<Borrower[4]>` and `is_lifetime_aware<Borrower[]>` both hold. Pushed further: with `struct Flex { int n; Borrow a[]; };` — `is_lifetime_aware<Flex>`, `is_sendable<Flex>` and `can_launch_task<…, Flex>` all hold, while the `Borrow a[4]` twin is correctly rejected. `-pedantic` only warns.

**Fix.** One line, mirroring `sendable.h:30` verbatim:

```cpp
template <class T>
constexpr bool is_lifetime_aware<T[]> = is_lifetime_aware<std::remove_cv_t<T>>;
```

Verified: both gap assertions invert, `int[]` and owning-element arrays stay true, all ten TUs green. Update the CLAUDE.md table row to `T[N]`, `T[]`.

Medium rather than high because the reachable exploit needs a GNU extension; the standard-C++ face is a direct query that generic code can produce.

---

## 2. CORRECTNESS BUGS — wrong answers, and inconsistencies between the three traits

### C1. `is_sendable<synchronized_value<Bad>>` is a hard error rather than `false`
`include/threadsafe/synchronized_value.h:77`

`is_synchronizable` and `is_lifetime_aware` both have specializations; `is_sendable` does not, so it falls to `default_is_sendable`, whose `is_complete_type` probe (`sendable.h:67`) instantiates the class body and trips its own `static_assert`. **Evidence:** `static_assert(!is_synchronizable<synchronized_value<Bad>>)` is clean; `static_assert(!is_sendable<synchronized_value<Bad>>)` errors at `synchronized_value.h:39`, and a `requires` probe errors identically — the trait is not SFINAE-friendly. **Fix:** add the missing specialization beside the other two — `template <class T> constexpr bool is_sendable<synchronized_value<T>> = is_sendable<T>;`. Verified green. Do *not* constrain the class template; that breaks `tests/test_synchronized_value.cpp:45`.

### C2. `is_lifetime_aware<std::filesystem::path>` is a hard compile error
`include/threadsafe/lifetime_aware.h:69`

`default_is_sendable` short-circuits at the copy/move guard before recursing; `default_is_lifetime_aware` has no such exit and descends into `path::_M_cmpts` → `unique_ptr<path::_Impl>` → incomplete `_Impl`. **Evidence:** `is_sendable<std::filesystem::path>` answers false cleanly; `is_lifetime_aware<std::vector<std::filesystem::path>>` and `is_lifetime_aware<std::filesystem::directory_entry>` both error with `'is_lifetime_aware<T> requires a complete type'`. A file list handed to a worker is a mainstream shape. **Fix:** add the explicit rules CLAUDE.md:69 already mandates, in `containers.h` (needs `<filesystem>`), for `path` and `directory_entry` on both traits. Verified green. Separately: give this throw the actionable wording `sendable.h:69` already carries — today it names a libstdc++ private type and offers no remedy. The escape hatch does work normally; the "no way to specialise around it" claim is refuted.

### C3. `std::thread` / `std::jthread` have no rule, so answers flip across platforms
`include/threadsafe/asynchronous_task_launcher.h:38`

CLAUDE.md's own obligation ("every owning std type needs an explicit rule") is unmet for the two types that most obviously own something. **Evidence:** on macOS, `std::thread` / `std::jthread` / `std::vector<std::jthread>` / `asynchronous_task_launcher` all report `send=0 life=0`, because the recursion bottoms out on `_opaque_pthread_t*`; a probe reproducing libstdc++'s exact layout with `pthread_t == unsigned long` reports `send=1 life=1`. **Fix:** in `vocabulary.h` (needs `<thread>`), `is_sendable` and `is_lifetime_aware` true for both, deliberately *no* `is_synchronizable` — leaving that false is what keeps `asynchronous_task_launcher` unshareable (`threads_.emplace_back` has no mutex). Verified green.

### C4. `is_synchronizable` has no cv-forwarding rule; the macro on `const T` is a silent no-op
`include/threadsafe/synchronizable_base.h:10`

`is_sendable` and `is_lifetime_aware` forward cv inside their default functions; `is_synchronizable` is the bare `constexpr bool = false` with no default function, so nothing forwards. **Evidence:** `is_synchronizable<std::atomic<int>>` true but `is_synchronizable<const std::atomic<int>>` false; after `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Widget)` the assertion compiles clean and *no consumer path sees it* (`is_sendable<const Widget&>`, `<const Widget*>`, `<shared_ptr<const Widget>>` all false). The failure direction is safe — this is a footgun, not a hole.

**Fix.** Add the three cv partial specializations after line 10 (verified: `is_synchronizable<const std::atomic<int>>` and the info-level face both become true). **They do not make the macro on `const Widget` ill-formed** — an explicit full specialization outranks a partial one; I compiled that case under the patch and it still succeeds silently. To get the loud failure, put the check in the macro:

```cpp
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)                       \
    static_assert(std::is_same_v<__VA_ARGS__, std::remove_cv_t<__VA_ARGS__>>, \
                  "assert synchronizability on the unqualified type: "      \
                  "every consumer strips cv before asking");                \
    template <> inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
```

Verified: `const Widget` errors, plain `Widget` and `std::map<int, std::string>` still work, all ten TUs green. Related one-liner: `std::atomic_flag` is sendable but not synchronizable, so it cannot be shared by reference the way `std::atomic<T>` can — add `template <> inline constexpr bool is_synchronizable<std::atomic_flag> = true;` beside `synchronizable.h:18`.

### C5. `synchronizable_base.h` is a public header that publishes the trait with zero specializations
`include/threadsafe/synchronizable_base.h:10`

This violates the invariant CLAUDE.md:85 states for every *other* trait header. **Evidence:** a TU including only `synchronizable_base.h` answers `!is_synchronizable<std::atomic<int>>` and `!is_synchronizable<void()>`; two TUs, one base-only and one full, linked and ran printing `TU_base_only=0 TU_full=1` with no compiler or linker diagnostic. (The single-TU form *is* caught: `partial specialization … after instantiation`.) Every missing specialization flips true→false, i.e. strictly conservative, and `is_sendable` is not even declared there — so nothing racy compiles. It is an IFNDR/hygiene defect.

**Fix.** Rename to `include/threadsafe/detail/synchronizable_fwd.h`, update the four includers (`sendable.h:7`, `synchronizable.h:7`, `copy_on_write.h:11`, `synchronized_value.h:11`), and move `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` to `synchronizable.h` with it. Verified: `test_include_isolation.cpp` still finds the macro, all ten TUs green; the alternative (bottom-include `synchronizable.h`) breaks 9 of 10.

**Related, user-side.** A `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` placed after a query in one TU and before it in another diverges silently — verified with a three-TU probe that compiled and linked with zero diagnostics while the two TUs held contradictory `static_assert`s. This is the generic customization-point IFNDR that `std::hash` and `enable_borrowed_range` share, and no change inside these headers prevents it. Document the placement rule on the macro (must be visible in *every* TU that queries the type or anything reaching it; in practice, in the header that defines the type) and add a two-TU pin exercising the intended placement.

### C6. A moved-from `copy_on_write` holds a null `shared_ptr` with no way to detect it
`include/threadsafe/copy_on_write.h:46`

No move operations are declared, so the implicit ones steal `ptr_`; `operator*`, `operator->` and `as_mutable()` all dereference unconditionally; there is no `operator bool`. **Evidence:** built with `-fsanitize=undefined -D_GLIBCXX_ASSERTIONS`, prints `moved-from operator->() == nullptr : 1` then aborts in `shared_ptr_base.h:1423` inside `as_mutable`'s `make_shared<T>(*ptr_)` (`use_count()` on a null handle is 0, so the detach branch runs). Reachability is ordinary use-after-move — **not** vector reallocation, which move-constructs then destroys and never exposes the sources. What is worse than the standard library is the *silence*: `shared_ptr`/`unique_ptr`/`optional` all define the moved-from state and expose `operator bool`.

**Fix.** Make the handle never-null by defining move as copy (one atomic increment):

```cpp
copy_on_write(copy_on_write&& o) noexcept : ptr_(o.ptr_) {}
copy_on_write& operator=(copy_on_write&& o) noexcept { ptr_ = o.ptr_; return *this; }
```

Verified: the probe exits 0 under UBSan, both trait answers unchanged (they are explicit specializations at `copy_on_write.h:53,58`, so the reflection copy/move rule is never consulted), all ten TUs green. Cost worth a doc line: the destination's next `as_mutable()` sees count 2 and detaches. The alternative is a genuine move plus `explicit operator bool()` and a documented valueless state.

### C7. The launcher's implicit destructor serialises every `request_stop` behind the preceding join
`include/threadsafe/asynchronous_task_launcher.h:39`

`~vector<jthread>` destroys front-to-back and `~jthread` is stop-then-join *per element*, so task N is not asked to stop until 0..N-1 have finished. **Evidence:** with element 0 sleeping 800 ms and ignoring its token, the three polite tasks spun 121/122/122 times *after* destruction began; with the fix, 0/0/0.

**Fix — and re-default the moves, or the class silently becomes immovable:**

```cpp
asynchronous_task_launcher() = default;
asynchronous_task_launcher(asynchronous_task_launcher&&) = default;
asynchronous_task_launcher& operator=(asynchronous_task_launcher&&) = default;
~asynchronous_task_launcher() { for (auto& t : threads_) t.request_stop(); }
```

Verified: without the re-defaulted moves, `asynchronous_task_launcher b{std::move(a)};` — which compiles today — hard-errors deep in `stl_construct.h` on the deleted `jthread` copy constructor.

### C8. `copy_on_write`'s variadic exclusion uses `same_as`, so it hijacks copy for derived classes
`include/threadsafe/copy_on_write.h:24`

For a `D&` argument (`D : copy_on_write<T>`), `remove_cvref_t` is `D`, the clause passes, and the variadic is an exact match while the copy constructor needs a derived-to-base conversion. **Evidence:** `copy from cow& shares block: 1` versus `copy from D& shares block: 0` — a silent deep copy. **Fix:** `&& (!std::derived_from<std::remove_cvref_t<Args>, copy_on_write> && ...)`. Verified green. Low, not medium: it needs public inheritance from a type with no virtual destructor and no inheritance story anywhere in the docs, *and* a greedy forwarding constructor in `T`. The clause already handles every shape the type is designed for.

### C9. Trait consistency nits
`include/threadsafe/lifetime_aware.h:66` — `is_lifetime_aware<void>` is true where `is_sendable<void>` is false; `is_lifetime_aware<int(&)(int)>` is false where `is_sendable` is true. Both unreachable as bugs (a member cannot have type `void`; `launch_task` decays function references to pointers). **Evidence:** measured directly; both fixes verified green. Fixes: `if (std::meta::is_void_type(type)) return false;` before the non-class fallthrough, and `template <class F> requires std::is_function_v<F> constexpr bool is_lifetime_aware<F&> = true;` beside line 28.

---

## 3. API / USABILITY

### A1. No synchronisation primitive is `is_synchronizable`, so none can be shared by reference
`include/threadsafe/vocabulary.h:50`

Everything the standard mandates as concurrently usable — `mutex`, `shared_mutex`, `condition_variable`, `latch`, `barrier<>`, `counting_semaphore`, `once_flag`, `atomic_flag` — is left at the default false. Because `is_sendable<T&> = is_sendable<T*> = is_sendable<reference_wrapper<T>> = is_sendable<shared_ptr<T>> = is_synchronizable<T>`, none of them can cross a thread boundary in the only form anyone uses. **Evidence:** all the `!is_synchronizable` and `!is_sendable<…&>` assertions hold; the fan-out/join idiom `launch_scoped_task(f, std::ref(latch))` fails to compile — in the one function whose whole point is that it joins, so stack borrows are legal.

**Fix.** Add the 13 opt-ins in a new `synchronization.h`, pulled in at the bottom of `sendable.h` and `lifetime_aware.h` per the header-structure rule. Verified green. **Adversarial check I ran:** this does *not* open a lock-transfer hole — `is_sendable<std::unique_lock<M>>` and `<std::shared_lock<M>>` stay false. But they stay false only because libstdc++ gives them user-provided move constructors, which is exactly the kind of accident the rest of this review complains about. Pin them:

```cpp
template <class M> constexpr bool is_sendable<std::unique_lock<M>> = false;
template <class M> constexpr bool is_sendable<std::shared_lock<M>> = false;
template <class M> constexpr bool is_sendable<std::lock_guard<M>>  = false;
```

`lock_guard<mutex>` does flip to sendable under the primitive opt-in; harmless twice over (immovable, and `is_lifetime_aware` is false so `launch_task` rejects it).

### A2. All of `<future>` and `<thread>` is unsendable, and `promise`/`future` disagree on ownership
`include/threadsafe/vocabulary.h:59`

**Evidence.** `!is_sendable<…>` holds for `future<int>`, `shared_future<int>`, `promise<int>`, `packaged_task<int()>`, `thread`, `jthread`, `thread::id`, `asynchronous_task_launcher`, `atomic<shared_ptr<int>>`. And the two halves of one promise/future channel give opposite ownership verdicts about the same shared state: `is_lifetime_aware<std::future<int>>` true (reaches the `shared_ptr` rule) against `!is_lifetime_aware<std::promise<int>>` (dragged false by its `unique_ptr<_Result<T>, _Deleter>` leg — the library's own `dynamic_type_is_known` rule firing on libstdc++'s polymorphic non-final `_Result<T>`). That incoherence is the sharpest part of this gap.

**Fix.** Add the explicit specializations. Two corrections to the obvious sketch, both verified: `is_sendable<std::future<T>>` must be `std::is_void_v<T> || is_sendable<T>`, or `future<void>`/`promise<void>` stay false; and `shared_future` should key on `is_synchronizable<T>`, not `is_sendable<T>`, since copies share one result. Add the idiom the generic rule asks the wrong question about:

```cpp
template <class T> constexpr bool is_synchronizable<std::atomic<std::shared_ptr<T>>> = is_synchronizable<T>;
template <class T> constexpr bool is_synchronizable<std::atomic<std::weak_ptr<T>>>   = is_synchronizable<T>;
```

Keep `packaged_task` explicitly false *with a comment* — it erases an arbitrary callable and belongs with the type-erasure family (U8, A5), not with `_Task_state_base`'s layout.

### A3. `std::exception_ptr` is neither sendable nor lifetime-aware, blocking worker→caller error propagation
`include/threadsafe/vocabulary.h:59`

**Evidence.** `!is_sendable<Result>` for the canonical `struct Result { int value; std::exception_ptr error; };`, plus `error_code`, `error_condition`, `type_index`, `source_location` all false on both traits. `exception_ptr`'s `is_lifetime_aware == false` is the answer that is wrong on the merits — it refcounts the exception object exactly as `shared_ptr` does, and `lifetime_aware.h:39` already grants `shared_ptr` true.

**Fix.** Take the safe subset unconditionally: `exception_ptr`, `type_index`, `source_location`, both traits true. **Do not blanket `is_lifetime_aware<std::error_code> = true`.** I compiled the counter-case: `error_category`'s default constructor is protected, so a user-derived category can be an automatic object, and the resulting `error_code` — now borrowing a stack local — sails through `launch_task` under that fix, where the unpatched tree correctly rejects it. `is_sendable<std::error_code> = true` is sound; the lifetime half needs either a stated assumption plus a pinned test, or nothing.

### A4. Missing `THREADSAFE_UNSAFE_ASSERT_SENDABLE` — the library's own error text steers users into a link failure
`include/threadsafe/sendable.h:69`

`sendable.h:69` tells the user to "specialize `is_sendable`". Executed the natural way — in a header, next to the type — that produces a duplicate-symbol link error, which the author has *already* worked around for `is_synchronizable` by shipping a macro. **Evidence:** built it — a user header doing `template <> constexpr bool threadsafe::is_sendable<Widget> = true;` included by two TUs: both compile, then `ld: 1 duplicate symbols`. **Fix:** add `THREADSAFE_UNSAFE_ASSERT_SENDABLE(...)` expanding to `template <> inline constexpr bool ::threadsafe::is_sendable<__VA_ARGS__> = true`, placed **in `sendable.h`** (placement matters for a user who includes only one header), and name it in the error text. Same for a `THREADSAFE_ASSERT_LIFETIME_AWARE`, which an owning range opting into `enable_borrowed_range` currently has no discoverable way to reach. Also add `template <class T> concept synchronizable = is_synchronizable<T>;` — the other two traits have concepts, this one does not. All verified green.

### A5. Type-erasure family answers false for an incidental reason
`tests/test_sendable.cpp:181`

The test's rationale, "`std::function` has a user-provided copy", is true of libstdc++ and is what drives the answer via `sendable.h:74`, but it is not *why* the answer ought to be false. **Evidence:** a bisect shows both libstdc++ choices are load-bearing — remove the user-provided copy *or* swap the `void*` in the erased storage for a byte buffer and the answer flips to true. **Fix:** pin `std::function`, `std::move_only_function`, `std::copyable_function` and `std::any` false on both traits, and reword the message to "the erased target is unknowable and may capture by reference". Verified green. `std::function_ref` needs no rule — it already answers false for the right reason. On the only supported toolchain nothing is wrong today; this is test robustness.

### A6. `threads_` only grows; no `join_all`, `size`, or result path
`include/threadsafe/asynchronous_task_launcher.h:38`

**Evidence.** After 5000 `launch_task([]{})` and a 300 ms wait, the private vector holds 5000 `jthread` objects, all still `joinable()` — both the C++ objects and the OS thread resources pinned for the launcher's lifetime. **Fix:** `request_stop()`, `join_all()`, `std::size_t size() const`. Verified green. Skip any `reap()` built on `!t.joinable()` — a finished-but-unjoined `jthread` is still joinable, so that predicate is dead code; automatic reaping needs per-task completion state. There is also no `std::future` path, so a task can only communicate through captured shared state — precisely what the traits exist to police.

### A7. An exception from a task calls `std::terminate`, including in the synchronous `launch_scoped_task`
`include/threadsafe/asynchronous_task_launcher.h:32`

**Evidence.** `terminate called after throwing … 'task failed'`, exit 134; the enclosing `catch` never runs. For `launch_task` this is the ordinary `std::thread` contract. For `launch_scoped_task` it is a trap — the call is synchronous in every observable way, so `try`/`catch` is the natural reflex. **Fix:** marshal via `exception_ptr` across the join, with the wrapper lambda's invocability **constrained to `F`'s**:

```cpp
std::jthread task{
    [&f, &ep]<class... A>(A&&... a) requires std::invocable<std::decay_t<F>&, A...> {
        try { std::invoke(f, std::forward<A>(a)...); }
        catch (...) { ep = std::current_exception(); }
    },
    std::forward<Args>(args)...};
```

An unconstrained `[&](auto&&... a)` **does not compile** — it is invocable with a `stop_token`, so `jthread::_S_create` injects one and the body then calls `std::invoke(f, stop_token)` on a `void()`. Verified: the constrained form catches the exception, still preserves stop_token injection, all ten TUs green.

### A8. `launch_scoped_task` is a synchronous call whose injected `stop_token` is structurally unreachable
`include/threadsafe/asynchronous_task_launcher.h:32`

The `stop_source` is never exposed and the `jthread` is joined on the next line, so `request_stop()` cannot be called before the join. **Evidence:** a task polling `stop_requested()` five times prints `ever saw stop_requested=NO`; ~17.9 µs per call at `-O2` versus a direct call. The function's one genuine product is the type-system concession (dropping `lifetime_aware`). **Fix:** rename to something honest (`run_on_worker_and_wait`) and mark it `static` — verified well-formed, it names no member — or take a `std::stop_source` from the caller so the path is real (note this breaks the existing call shape in two test files). At minimum, a comment.

### A9. No deadlock-free way to lock two `synchronized_value`s
`include/threadsafe/synchronized_value.h:66`

`lock()` returns an already-locked guard and `mutex_` is private with no accessor, so `std::scoped_lock`'s avoidance algorithm is unreachable. **Evidence:** the obvious two-value idiom ABBA-deadlocked on the first attempt (watchdog fired at 2 s, exit 42). A deadlock-free approach *does* exist today — a global lock order by convention, or one `synchronized_value<std::pair<A,B>>` — so the narrow claim is that the standard's mechanism is out of reach and correctness rests on caller discipline. **Fix:** a variadic free `with_lock` over `std::scoped_lock`, reached via one friend line. Verified: 100 000 iterations of two threads locking in opposite orders complete cleanly.

### A10. `value_guard` hard-codes `std::shared_mutex&` while `Lock` is a template parameter
`include/threadsafe/synchronized_value.h:31`

**Evidence.** `value_guard<int, std::unique_lock<std::mutex>> g{m, v};` — no matching constructor. Consequence: `sizeof(synchronized_value<int>) == 208` where a mutex-backed equivalent is 72, and every instance pays an rwlock's acquire path even if `lock_shared()` is never called. **Fix:** `value_guard(typename Lock::mutex_type& m, T& v)`, then `template <class T, class M = std::shared_mutex>` with a locally-written `shared_lockable` concept selecting `shared_lock<M>` or `unique_lock<M>` for `const_guard`, so `lock_shared()` degrades instead of failing to compile. Verified: compiles, runs both lock paths on a `std::mutex`-backed instance, traits unaffected, all ten TUs green. Note `shared_lockable` is not standard — it has to be written.

### A11. `template <class> friend class synchronized_value;` lets a user specialization forge a guard
`include/threadsafe/synchronized_value.h:28`

**Evidence.** An explicit `template <> class threadsafe::synchronized_value<Victim>` with a static `forge(std::shared_mutex&, int&)` compiles and produces a guard over unrelated data holding an unrelated lock. **Fix:** `friend class synchronized_value<std::remove_const_t<T>>;` — verified, the forge becomes a private-access error, all ten TUs green. Cheap and worth taking, but **not** a security fix: access control was never a boundary against hostile code, and the same user can read `mutex_` via `std::meta` with `access_context::unchecked()`, as this library itself does.

### A12. A false `is_sendable` names no member, type, or chain
`include/threadsafe/sendable.h:17`

**Evidence.** For `Top → std::map → Mid → std::vector → Leaf` (Leaf has a user-provided copy), the *complete* diagnostic is `error: static assertion failed: Top must be sendable` plus a caret. Through `launch_task` it ends at `'(sendable<typename std::decay<_Args>::type> && ...) [with Args = {Top&}]' evaluated to 'false'`. Neither names `Leaf`. **Fix:** an opt-in `THREADSAFE_EXPLAIN_SENDABLE(T)` / `detail::explain_is_sendable` pair (~20 lines, only instantiated by the macro, no change to `default_is_sendable`). Prototype verified, printing `is_sendable<T> is false; the innermost type responsible is Leaf`. Keep the `parent_of == ^^std` guard in the walk — without it the explainer reports `std::_Rb_tree_key_compare<…>`. Mirror as `THREADSAFE_EXPLAIN_LIFETIME_AWARE`.

### A13. `mutable std::atomic<T>` is a false negative in `copy_on_write` — conservative but fixable
`include/threadsafe/copy_on_write.h:55`

`struct SafeCounter { mutable std::atomic<int> hits{0}; int get() const { return hits.fetch_add(1); } };` gets `send<cow>=0` even though the member's own type is already known synchronizable. Two threads calling `fetch_add` through const handles is exactly what `copy_on_write` should permit. **The include-inversion objection is refutable:** `has_mutable_state` is called from exactly one place, `copy_on_write.h`, which already sees `synchronizable_base.h` — so define a `has_unsynchronized_mutable_state` variant there and leave `utils.h` untouched. Verified as a standalone function: `SafeCounter` relaxes to 0 while `Cache` and `vector<Cache>` stay 1. **Leaving it alone is entirely defensible** — a mutable atomic is rarely a type's only mutable member. If you do, point at the `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` workaround from the docs and note that for this shape the UNSAFE label overstates the risk.

---

## 4. COMPILE-TIME

All figures are min-of-3-to-6 runs, GCC 16.1.0, parse-only twins subtracted where noted. Suite = all ten test TUs.

### T1. `#include <complex>` in `vocabulary.h:4` — the single biggest lever (landed), but "redundant" was wrong
**Measured (original review, GCC 16.1.0 — superseded by the re-measurement at the end of this item).** Marginal cost over the library's 22-header std closure: **+0.282 s**; next largest line item is `<ranges>` at **0.033 s** — a 9× gap. It drags `<sstream>`/`<ostream>` at depth 2, 356 headers total. Per-header TU: `sendable.h` 0.774 → 0.494, `synchronizable.h` 0.743 → 0.463, `lifetime_aware.h` 0.718 → 0.459. **Suite 7.245 → 5.580 s (−23%).**

**Not redundant — this section's central claim was false, and the 25-assertion differential behind it is stale.** `is_sendable<std::complex<double>>` was true *only* because of the specialization. In C++26 mode libstdc++ gives even the `complex<double>` explicit specialization a converting **constructor template** — `template <class _Up> explicit(!requires(_Up __u) { value_type{__u}; }) constexpr complex(const complex<_Up>&)`, `complex:1616-1620`, guarded by `#if __cplusplus > 202002L` — which `detail::may_hijack_copy_move` (`utils.h:80`) rejects, so `has_only_default_copy_move_destroy` fails and the default recursion answers **false**. Confirmed by compiling `static_assert(detail::default_is_sendable(^^std::complex<double>) == false)` against this tree. The differential presumably predates `may_hijack_copy_move`. Only the `is_lifetime_aware` half was genuinely redundant: its default recursion already answers `true`.

**Landed anyway, with the trade-off stated.** `std::complex<T>` is now not sendable. That was accepted deliberately: the header cost is real and `std::complex` is not load-bearing for this library. A caller who needs it specializes `is_sendable` itself, subject to the header-structure rule. `CLAUDE.md` rule 3 records this next to `std::default_delete`, which has the same declaration shape.

**Done.** `vocabulary.h:4` and lines 44-47 deleted, along with `#include <complex>` and the `complex<double>` assertion in `tests/test_soundness_regressions.cpp`. That assertion was *not* kept as a pin: without the specialization it fails, and inverting it to `!is_sendable` would pin an incidental libstdc++ declaration shape as intended behaviour. Its old message (`"__complex__ T is neither scalar nor class; it needs its own rule"`) was independently false — GCC 16 reports `__complex__ double` as `scalar=1 arith=1 fundamental=1 class=0`, and the member is never reached anyway because the constructor template short-circuits first.

**Re-measured after landing** (GCC 16.2.0, this tree, min-of-2 clean builds): **suite 6.80 → 5.62 s, −17%**; `<complex>` no longer appears in a `threadsafe.h` `-H` dependency dump at all. The −23% above was against a different tree and does not reproduce.

### T2. `has_mutable_state` is unmemoized and costs ~6× `is_sendable` — but the obvious fix breaks self-referential types
`include/threadsafe/utils.h:23`

**Measured** (100 distinct types, each 40 members of a distinct 40-int leaf; parse-only twin 0.757 s): `is_sendable` 7.8 ms/type, `has_mutable_state` **44.4 ms/type = 5.7×**, `is_sendable<copy_on_write<T>>` 5.790 s. With memoization: `c_hms` −79%, `c_cow` −69%.

**The facade-via-`substitute` fix is broken as written.** A variable template cannot appear in its own initializer, and `struct Node { std::shared_ptr<Node> next; int v; };` closes exactly that loop:

```
error: the value of 'threadsafe::detail::has_mutable_state_v<Node>' is not usable in a constant expression
note: used in its own initializer
```

Stock compiles that cleanly — and still does at `-fconstexpr-depth=32`, proving `has_mutable_state` is never *evaluated* there, because `is_sendable<Node>` is false and `copy_on_write.h:53`'s `&&` short-circuits. The memoized version enters the cycle anyway, since the immediate invocation is folded while the initializer is instantiated. **All ten test TUs pass under the broken patch** — the suite contains no self-referential type.

**Fix.** Apply the memoization **only together with** a lazy second factor:

```cpp
namespace detail {
template <class T, bool = is_sendable<T>> struct cow_sendable : std::false_type {};
template <class T> struct cow_sendable<T, true>
    : std::bool_constant<is_synchronizable<T> || !has_mutable_state(^^T)> {};
}
template <class T> constexpr bool is_sendable<copy_on_write<T>> = detail::cow_sendable<T>::value;
```

Verified together: `Node` compiles again, the full win is retained (`c_hms` 1.031, `c_cow` 1.847), all ten TUs green. Add `static_assert(!is_sendable<copy_on_write<Node>>)` to `tests/test_copy_on_write.cpp` in the same commit.

### T3. `has_only_default_copy_move_constructor_assignment` pays a consteval call per `members_of` entry
`include/threadsafe/utils.h:68`

`members_of` returns member type aliases, static data members, member functions and member templates alongside the special members. **Measured:** 50 types × 200 member aliases, work 1.805 → 0.137 s (**−92%**); 100 types × [200 aliases + 50 statics + 20 templates], 10.693 → 1.187 s (**−89%**); 300 STL-shaped types (12 typedefs + 25 member functions each), work −40%.

**Equivalence proven, not sampled.** A compile-time assertion that `is_copy_move_constructor_assignment(m) ⇒ is_function(m)` over every member of 24 types — including inherited constructors and `operator=` via using-declarations, out-of-line-defaulted, deleted, converting constructor templates, ref- and volatile-qualified `operator=`, a union, polymorphic, private-copy, move-only, plus `string`/`vector`/`map`/`shared_ptr`/`unique_ptr`/`function`/`complex`/`optional`/`tuple`/`copy_on_write` — compiles with rc=0.

**Fix.** `if (!std::meta::is_function(m)) continue;` at the top of the loop body. Semantics-neutral; a 26-assertion differential including the `struct D1 : EmptyUserCopy {}` regression case is identical. Skip inlining `is_copy_move_constructor_assignment`'s disjunction — the guard recovers nearly all of the win and the named helper is worth the remainder.

### T4. Combined effect, and what is *not* available
| Workload | stock | fixed | |
|---|---|---|---|
| Full suite (10 TUs) | 7.245 s | 5.563 s | −23% (GCC 16.1.0; re-measured at −17% on 16.2.0 — see T1) |
| 100 types × noisy members | 8.182 s | 0.833 s | −90% |
| 100 × `is_sendable<cow<40×40>>` | 5.803 s | 1.500 s | −74% |
| 50 types × 200 aliases | 2.375 s | 0.594 s | −75% |
| 300 STL-shaped types | 1.299 s | 0.761 s | −41% |
| single-header TU (`sendable.h`) | 0.705 s | 0.448 s | −36% |

The three fixes do not add up because T1 owns the suite while T2/T3 only pay off on user code with many distinct types. Both audiences matter; the split is real, not an artifact.

**Facts worth recording rather than re-deriving.** The recursion is *already* memoized in both faces — 200 `is_sendable<H>` costs the same as 1 (0.756 vs 0.755 s), and 200 `is_sendable_type(^^H)` costs 0.767 s, so the `substitute`+`extract` path memoizes identically and reflection-side code need not hoist repeated queries. Cost is linear only in *distinct* types (200 distinct heavy types: 2.848 s). Depth is free (10× deeper inheritance chain: +0.043 s). Phase split on the slowest TU: parsing 68%, template instantiation 37%, **constant-expression evaluation 2%** — the suite is header-parse bound, not reflection bound.

**Two corrections to circulating numbers.** (a) The `^^std` shortcut in `has_mutable_state` does **not** cost ~18 ms — measured across five containers the deltas are ±0.012 s on a ~0.75 s TU, i.e. no effect in either direction. It is a correctness rule, and deleting it breaks `tests/test_copy_on_write.cpp:65` and `:79`. Say so in the comment so a future optimizer does not read it as a fast path. (b) Reordering `sendable.h:56` to `is_scalar_type || is_synchronizable_type` is value-preserving even for incomplete and abstract types declared synchronizable via the macro (GCC answers `is_scalar_type` on an incomplete class without complaint), but the claimed 2.6% does **not** reproduce — 0.6% at six repetitions, sign unstable across repetition counts. Optional, and arguably the current order reads better since Sync⇒Send is the rule the line exists to state.

**No further header pruning exists.** `<memory_resource>` is not in the library at all (only `tests/test_soundness_regressions.cpp`, where lines 102-103 genuinely need it). `<ranges>` looks like the second-most-expensive header standalone (0.302 s) but is 0.033 s marginal, because `containers.h` already drags `bits/ranges_base.h` — keep it; substituting a libstdc++ internal trades 33 ms for a dependency this library otherwise avoids. Post-`<complex>`, a `sendable.h` TU is ~0.57 s (GCC 16.2.0; ~0.60 s with `<complex>` — the original ~0.49 s figure was 16.1.0 and does not reproduce) of which roughly half is `<meta>` + `<type_traits>` and the rest is the specialization set's std headers: ~94% std-header parsing repeated per TU, and the only remaining lever is a module interface — a build change, not a code change. Worth stating in CLAUDE.md so nobody re-runs this.

---

## 5. READABILITY / DOCS / BUILD

**B1. `CLAUDE.md` documents none of `synchronized_value`, `value_guard`, or `asynchronous_task_launcher`** (`CLAUDE.md:20`). `grep -c` returns 0 for all five names including `launch_task` and `launch_scoped_task`. The omission that matters: `asynchronous_task_launcher.h:20-27` requires `sendable && lifetime_aware`, lines 29-35 require only `sendable` because the jthread is joined at line 34 — the one rule a user must understand, inferable only from the tests. Add two sections in the shape of the `copy_on_write` one, and state the asymmetry outright.

**B2. `CLAUDE.md:95` points at `docs/thread-safety-audit.md`, deleted in 595f87d** (372 lines). `grep -rn thread-safety-audit .` returns exactly that one hit; `git ls-files docs` returns nothing (the directory is an untracked leftover). Either delete the line and `rmdir docs`, or restore the file — if restored, U10 (the `launch_scoped_task` borrow escape) and C3 (platform-dependent thread answers) belong in the open list. That is precisely the kind of entry it existed to carry.

**B3. No LICENSE, no README, no example** (repo root: `.claude .git .gitignore build CLAUDE.md CMakeLists.txt docs include tests`). LICENSE is the blocking item — without one the code is all-rights-reserved. Then a short README: the three-trait table, the GCC 16 + `-freflection` requirement, the two build commands, and one compiling example.

**B4. The test target is an OBJECT library that is never linked, so the inline-on-full-specialization invariant is unenforced** (`tests/CMakeLists.txt:3`). Dropping `inline` from `vocabulary.h:50` compiles all ten objects — the suite reports green — and only fails at link: `duplicate symbol 'threadsafe::is_synchronizable<std::stop_token>'`. This works because GCC emits the definition even though the tests only read it in `static_assert`s (`nm` shows 6 such symbols per object). Baseline links clean, so this is a latent regression guard. Fix: add `link_check.cpp` containing `int main() {}` and one `add_executable` + `target_link_libraries` against the existing object library. Zero extra compile cost.

**B5. GCC 16 is required but nothing enforces or diagnoses it** (`CMakeLists.txt:13`). `cmake -DCMAKE_CXX_COMPILER=clang++` configures successfully and fails at build with `'meta' file not found` — no mention of GCC, 16, or `-freflection`. Add a `CMAKE_CXX_COMPILER_ID`/`VERSION_LESS 16` `FATAL_ERROR` after `project()`, plus an `#if !defined(__GNUC__) || __GNUC__ < 16 / #error` at the top of `utils.h` for non-CMake consumers. **If the CMake floor is revisited, use `3.30`, not `3.23`** — `cxx_std_26` was added in 3.30 (`PROJECT_IS_TOP_LEVEL` in 3.21), so a lower floor breaks `target_compile_features` on real 3.23-3.29.

**B6. `$<INSTALL_INTERFACE:include>` with no `install()`, export set, or package config** (`CMakeLists.txt:9`). Ran it: `cmake --install` produces an empty tree. Either delete the generator expression (declaring the library subdirectory-only, which is how the tests consume it) or add the four commands plus a `ThreadSafeConfig.cmake.in`.

**B7. Two test messages assert things that cannot happen.** `tests/test_soundness_regressions.cpp:134` says "a mutex may be moved to another thread", but `std::mutex` is not copy-, move-constructible or move-assignable — no program can perform the described operation. The *value* is right (Rust's `Mutex<T>` is `Send`), but it is reached entirely through libstdc++ layout, and the same accident covers `latch`, `condition_variable`, `once_flag`, `counting_semaphore`, `atomic_flag` while `std::barrier<>` answers **false** because libstdc++ routes it through `__tree_barrier`. Reword to the Rust-sense claim, and land A1 so the answer is *stated* rather than observed — `is_sendable<std::barrier<>>` then stops disagreeing with its siblings. (`test_soundness_regressions.cpp:179` is covered under T1.)

**B8. `tests/test_synchronized_value.cpp:85-87` justifies `value_guard`'s immovability with a claim the compiler contradicts.** `struct Holder { sync_int::guard g; }; Holder h{sv.lock()};` compiles and runs via guaranteed copy elision — no move involved — and `!is_sendable<Holder>` holds anyway. Making the guard movable leaves every travel-blocking assertion true; `is_sendable<value_guard<T,Lock>> = false` (`synchronized_value.h:83`) is what does the work. The behaviour is pinned and defensible on its own terms (a moved-from guard is a lock-holding object with no value); only the stated reason is wrong. Fix the comment. If `std::optional<guard>` is ever wanted, note that a move *constructor* alone is insufficient — `optional::operator=(optional&&)` stays deleted; both members are needed, and they should null the source's `value_`, not `= default`.

**B9. The pimpl hint fires only for an incomplete `T`, never for the raw-pointer pimpl it names** (`sendable.h:69`). `struct Impl; struct Widget { Impl* p; };` answers false silently via the `is_sendable<T*>` specialization, never reaching the branch; the `unique_ptr<Impl>` form does fire it, with a good stack — so the message is *accurate where it appears*, and what is missing is any diagnostic for the raw-pointer form. **More valuable than the wording:** reorder `smart_pointers.h:14-16` so the completeness check runs before `detail::dynamic_type_is_known`, otherwise the helpful exception is always trailed by an unfiltered `type_traits:3671: invalid use of incomplete type 'struct Impl'` from `std::is_polymorphic_v`.

**B10. Style, verified behaviour-neutral (all ten TUs green).** Extract `copy_on_write.h:23-25`'s exclusion clause into a named `detail::not_a_copy_of<Self, Args...>` — its intent is currently recoverable only from `test_copy_on_write.cpp:108-111`, and naming it carries the intent without violating the no-useless-comments rule. Rename the mutated parameter in `has_mutable_state` (`utils.h:25`) to a `const auto elem` local, matching `sendable.h:49` and `lifetime_aware.h:56`. Move `synchronized_value.h`'s friend declaration above `private:` (access specifiers do not affect friends). Do **not** constrain `value_guard`'s `Lock` on mismatch grounds — the constructor is private with only `synchronized_value` as friend, so `value_guard<int, unique_lock<mutex>>` is a well-formed but unconstructible instantiation; there is no reachable defect. Leave `threadsafe.h`'s include order alone — every public header compiles standalone with the full specialization set visible.

**B11. Unpinned behaviours found by mutation testing.** Removing `has_unreflectable_state` from `lifetime_aware.h:73` leaves the suite green; so does changing `is_lifetime_aware<unique_ptr<T,D>>` to `= true`. Pin `!is_lifetime_aware<CapturesReference>` beside `test_soundness_regressions.cpp:153` and `!is_lifetime_aware<std::unique_ptr<int*>>` beside line 113. (The `is_polymorphic_type` carve-out at `utils.h:16` *is* pinned, transitively — deleting it fails line 124 — so no assertion is needed there.)

**B12. Turn the 70-type std sweep into a checked-in test file.** The measured inventory (queue/stack sendable but `priority_queue` not, `valarray` S=false L=true, `bitset<64>` sendable, `atomic_ref<int>` not, `vector<bool>` sendable via the explicit vector rule, `unique_ptr<int[]>` T/T but `unique_ptr<int*[]>` false, `array<int,0>` sendable but `array<int*,0>` not) is only useful as a pinned regression baseline. That is what stops the next libstdc++ bump from silently flipping one.

---

## The three things I would fix first

**1. The constructor/assignment-template hijack (`utils.h:65` → `sendable.h:74`).** It is the only hole reachable by ordinary, idiomatic C++ — a perfect-forwarding constructor is a shape people write on purpose, with no extension, no `const_cast`, no unsafe macro. It is the exact hazard the project already documents as the reason `is_safe_callable` was retired, and `tests/test_soundness_regressions.cpp:137-151` already pins the sibling case; the guard just does not see through templates. A runtime probe printed user constructor code executing on the worker thread. The fix is ~15 consteval lines with a proven differential (four hijacking shapes closed, converting templates and `std::default_delete` untouched).

**2. `*sv.lock()` handing out an unguarded `T&` (`synchronized_value.h:24`).** Highest ratio of damage to typing effort in the whole review: `auto& r = *sv.lock();` is a one-liner someone writes on their first afternoon with the type, it compiles with zero warnings under every dangling-reference diagnostic GCC has, and it silently disables the mutex forever. Nothing in the trait system can flag it, because it isn't dangling — it's *unlocked*. Two ref-qualifiers close it, and `with_lock` restores the idiom they cost. Fix U4 (the `lock_shared` mutable-state gap) in the same commit; they are the same type's two ways of handing out unprotected access.

**3. `encloses_std` in `has_mutable_state` (`utils.h:38`).** The current answer for `std::list<Cache, MyAlloc<Cache>>` is *luck* — it depends on `std::allocator` living in bare `std` while `std::__cxx11::list` does not, which is why swapping the allocator flips the verdict on an unchanged element type. That is worse than being wrong: it is wrong non-obviously, and it will move under libstdc++. Three lines make the [res.on.data.races] argument complete rather than accidental, a 20-type battery is byte-identical before and after except the one intended flip, and it also removes a latent `std::meta::exception` throw masked today only by `&&` short-circuiting.

Runner-up, landed: deleting `#include <complex>` from `vocabulary.h` — a re-verified **−17%** on the whole suite (6.80 → 5.62 s). It does *not* cost nothing, contrary to what this review originally claimed: the 25-assertion differential was stale, the `is_sendable` specialization was load-bearing, and `std::complex<T>` is no longer sendable as a result. See T1 for why that was accepted. `test_soundness_regressions.cpp`'s `complex` assertion was deleted rather than corrected.