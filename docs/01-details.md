# Audit ThreadSafe — détail des constats

Document de détail : un constat par section, avec le code fautif, la correction proposée quand il y en a une, la reproduction compilée et le compte rendu du vérificateur indépendant chargé de le réfuter.

La synthèse et le verdict d'ensemble sont dans [`00-synthese.md`](./00-synthese.md).

Le corps de chaque constat est reproduit **tel qu'il a été rédigé et vérifié**, en anglais : c'est la langue du code, des messages du compilateur et des sorties collées en preuve. Traduire aurait mis une paraphrase entre le lecteur et l'évidence.

## Comment lire un constat

- **Gravité** — `critique` : la bibliothèque répond « sûr » sur du code qui ne l'est pas, ou casse la mémoire. `majeur` : la bibliothèque refuse du code correct, ment sur la raison, ou coûte un ordre de grandeur. `moyen` / `mineur` / `détail` : par ordre décroissant d'impact.
- **Confiance** — le degré de certitude du vérificateur après sa tentative de réfutation.
- **Vérification** — ce que le vérificateur a tenté pour faire tomber le constat, et ce qui a survécu. Quand il corrige ou nuance le constat initial, c'est écrit là.

## Doublons assumés

61 constats confirmés, mais pas 61 défauts distincts : plusieurs agents ont atteint le même défaut par des axes différents. Les rapports sont tous conservés — chacun apporte une reproduction ou une conséquence que les autres n'ont pas — et reliés entre eux dans l'en-tête de chaque constat. Après regroupement, **49 défauts distincts**.

- `F05`, `F06`, `F07`, `F09` : les diagnostics re-dérivés par la marche structurelle
- `F02`, `F13` : les types récursifs propriétaires
- `F12`, `F21` : has_unreflectable_state et les champs de bits
- `F25`, `F27`, `F30` : copy_on_write sans règle is_synchronizable<const ...>
- `F40`, `F42`, `F61` : les splices et helpers consteval de synchronized_value
- `F44`, `F47` : le helper cow_is_sendable
- `F34`, `F46` : la barrière mémoire non commentée de as_mutable
- `F52`, `F53` : les threads terminés jamais récupérés

## Sommaire

- **Robustesse — les traits**
  - `F01` [critique] is_sendable<T*>/<T&>/<T&&>, is_sendable and const-is_synchronizable for shared_ptr/weak_ptr/reference_wrapper, and the pointer and reference-member branches of 
  - `F02` [critique] A self-referential owning type (vector/list/map/unique_ptr of itself) makes is_lifetime_aware_v and is_sendable_v ill-formed instead of answering true — includi
  - `F03` [critique] The derived-behind-base guard fires only on polymorphic pointees, so a shared_ptr (or custom-deleter unique_ptr) to a non-final, non-polymorphic base is judged 
  - `F04` [majeur] Every completeness test in the library freezes a `false` into a memoized specialization — `is_sendable`/`is_synchronizable`/`is_lifetime_aware` defaults and `de
  - `F05` [majeur] assert_lifetime_aware discards the specialization's reason and re-derives one from the structural walk, printing libstdc++ layout members and circular advice — 
  - `F06` [majeur] `descend_sendable` explains every rejection with the structural walk, so any type answering false through a specialization keyed on a class template — the whole
  - `F07` [majeur] descend_sendable runs the structural walk even when the answer came from a specialization, so shared_ptr<int> is rejected with a non-operative reason whose advi
  - `F08` [majeur] The unreflectable-state message advises a specialization that cannot be written for a block-scope closure, and never names the one-line workaround (message word
  - `F09` [majeur] assert_* prints a fabricated reason and actively wrong advice whenever the false answer came from a partial specialization: descend_* always re-runs the primary
  - `F10` [majeur] THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE also grants Send by value (sendable.h:127, allowed_std_wrappers.h:80), so a Sync-but-not-Send type is unsafe by default;
  - `F11` [majeur] The mandated rebind constructor template makes every conforming custom allocator non-sendable, so allow-listed containers parameterized on one answer false for 
  - `F12` [moyen] has_unreflectable_state also flags two non-closure shapes — an empty union and a struct whose only members are unnamed bit-fields — and the rejection message as
  - `F13` [moyen] A value-held self-reference (`std::vector<Self>`, `unique_ptr<const Self>`) makes `is_synchronizable<const T>` / `is_sendable<T>` a non-recoverable template cyc
  - `F14` [moyen] No standard synchronization primitive is blessed synchronizable, so a `mutable std::mutex` / `once_flag` / `atomic_flag` member sinks the const answer — `synchr
  - `F15` [moyen] is_lifetime_aware's borrowed_range branch is a false negative for value-generating views (iota_view, empty_view) and changes no verdict the structural member wa
  - `F16` [moyen] type_index, error_code, error_condition and source_location answer false for all three traits, so launch_task cannot take a source_location
  - `F17` [moyen] has_only_default_copy_move_destroy tests std::meta::is_defaulted instead of is_user_provided, so is_sendable/is_synchronizable answer differently depending on w
  - `F18` [moyen] std::thread::id answers false for all three traits: the structural walk descends into libstdc++'s _M_thread, which is a pointer on Darwin and an integer on glib
  - `F19` [moyen] is_synchronizable is never specialized for the standard synchronization primitives, so latch, barrier, counting_semaphore and atomic_flag cannot be shared by re
  - `F20` [moyen] `std_wrapper_is_const_synchronizable` calls `add_const` on each template argument, which is a no-op for a reference argument, so `std::pair<T&,U>` / `std::tuple
  - `F21` [mineur] `has_unreflectable_state` reports "a closure type with captures" for any type whose only members are unnamed bit-fields, rejecting a sendable/synchronizable typ
  - `F22` [détail] THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE expands to a qualified specialization and so only works at global scope, with no comment saying so
- **Robustesse — les helpers**
  - `F23` [critique] as_mutable() returns a bare T& that stays live after a later handle copy re-shares the block, so a program following the library's own "share by copying it" rul
  - `F24` [critique] synchronized_value selects its mutex *type* from the user-extensible is_synchronizable trait, so layout is not carried in the type's identity: a TU that has not
  - `F25` [majeur] `is_synchronizable<const copy_on_write<T>>` is false because the generic const walk rejects copy_on_write's own variadic constructor template, so a COW handle c
  - `F26` [majeur] The `&&`-delete message on `operator*`/`operator->` promises a guarantee C++ cannot give: a reference escapes the lock via a named guard, or via any `const valu
  - `F27` [moyen] `copy_on_write<T>` is the only library wrapper given `is_sendable` without a matching `is_synchronizable<const …>` rule, so a shared-by-const `copy_on_write` — 
  - `F28` [moyen] assert_ownable_by_launcher's move_constructible branch is dead for callables that are neither copyable nor movable (by-value parameter fails first), and the "us
  - `F29` [moyen] synchronized_value's sendable failure names only T, not the offending member, though the library's own assert_sendable already prints the member path (copy_on_w
  - `F30` [mineur] A copy_on_write member poisons its owner's const-synchronizability, so nested COW is not sendable and synchronized_value downgrades to a plain mutex — not "cons
  - `F31` [mineur] Neither launchable_task nor launchable_scoped_task checks invocability, so an arity mistake escapes into 49 (resp. 38) lines of libstdc++ <thread> internals
- **Thread safety**
  - `F32` [majeur] asynchronous_task_launcher has no destructor, so ~vector<jthread> stops and joins tasks one at a time: shutdown latency is the sum of the tasks' stop latencies,
  - `F33` [majeur] launch_scoped_task joins its jthread before anything can request the stop, so a stop_token-taking callable — one both launchable_scoped_task and the class's own
  - `F34` [mineur] The `use_count()==1` fast path in `as_mutable()` is memory-ordering-critical but uncommented, and its acquire fence pairs with a refcount release the standard e
- **Couverture de tests**
  - `F35` [moyen] tests/test_synchronized_value.cpp:83 — the file's only launch_scoped_task assertion is inert: its requires-expression is satisfied by the launcher's unconstrain
  - `F36` [moyen] tests/test_diagnostics.cpp asserts the rejection half is untestable; it is testable — a consteval caller can catch std::meta::exception and pin the message, lea
  - `F37` [moyen] The deleted `operator*() &&` / `operator->() &&` on value_guard — the only `= delete("...")` in the library and the sole thing stopping `*sv.lock()` from return
  - `F38` [mineur] The guard-immobility assertion tests !std::movable, which a move-constructible-but-not-move-assignable guard satisfies; it does not pin move-constructibility, a
- **Simplicité du code**
  - `F39` [majeur] The diagnostic walk machinery (descend/explain/default_is) is written three times; two copies even cite the third's comment instead of carrying their own
  - `F40` [majeur] synchronized_value contains the library's only two `[: :]` type-computing splices, used for a conditional typedef that std::conditional_t does in one line — and
  - `F41` [majeur] asynchronous_task_launcher.h is the only details/ header that is not standalone-includable: its class-scope static_assert depends on std::stop_token specializat
  - `F42` [moyen] Two public `consteval` functions and two splices where a single named `conditional_t` condition is type-identical — but `mutex` must stay public or tests/test_s
  - `F43` [moyen] The weak_ptr rules are verbatim copies of the shared_ptr rules in all three traits, and the shared_ptr answer is split across lifetime_aware.h and smart_pointer
  - `F44` [moyen] detail::cow_is_sendable is a single-use consteval helper whose if constexpr is behaviorally inert; inlining the two-term && states the copy_on_write rule where 
  - `F45` [mineur] `threadsafe::function_type` is a single-use public concept wrapping `std::is_function_v`, which the library also spells two other ways
  - `F46` [mineur] copy_on_write::as_mutable's acquire fence is the library's only hand-written memory ordering and the only unexplained line in a codebase that comments every oth
  - `F47` [détail] detail::cow_is_sendable buys no short-circuiting that std::conditional_t does not already give for free — the helper is pure indirection in the one rule a confe
  - `F48` [détail] Lambda-in-fold for per-argument checking, where the same file already uses a plain fold over a named consteval helper twice
- **Performance à la compilation**
  - `F49` [majeur] Every `false` trait answer renders a diagnostic message that the trait catches and discards — the rendering, not the throw, makes a negative answer cost 3.4x a 
  - `F50` [moyen] No precompiled header on the test target: 11 TUs re-parse the same 140,249-line umbrella, costing ~7.0s of the 7.5s serial suite — but a PCH only helps serial a
- **Performance à l'exécution**
  - `F51` [majeur] synchronized_value<T> selects std::shared_mutex for every sendable value-like T (int, string, vector, map) with no opt-out, so the common short-critical-section
  - `F52` [majeur] launch_task retains every finished jthread for the launcher's whole life: 200k tasks cost 100 s / 3.3 GB where the same thread-per-task loop with the record rea
  - `F53` [mineur] launch_task never reaps finished tasks: each completed jthread's stack stays resident until the launcher is destroyed (~16 KB/task measured, 2 MB -> 163 MB over
- **API et flexibilité**
  - `F54` [majeur] `is_sendable` is the only one of the three traits not specialized for `synchronized_value`, so `is_sendable_v<synchronized_value<T>>` completes the class throug
  - `F55` [majeur] allowed_std_wrappers is a closed 18-template list, so common std vocabulary (chrono::duration, bitset, complex, expected, queue, stack, valarray, flat_map) is n
  - `F56` [moyen] The unconstrained fallback makes launch_task acceptance undetectable: `requires { launcher.launch_task(bad) }` is true for any trait rejection (and a hard error
  - `F57` [moyen] synchronized_value exposes no multi-value lock and, because its mutex is private and the type is not Lockable, the caller cannot reach std::scoped_lock either —
  - `F58` [moyen] `is_synchronizable` is the only trait with no concept face — `sendable` and `lifetime_aware` both ship one, so users cannot constrain a template on the library'
  - `F59` [moyen] assert_sendable/assert_synchronizable/assert_lifetime_aware return void, so neither `static_assert(assert_sendable<T>())` nor a bare namespace-scope call compil
  - `F60` [mineur] Function references are the sole outlier in is_lifetime_aware: `void()` and `void(*)()` answer true, `void(&)()` answers false
  - `F61` [détail] synchronized_value's two alias-computing consteval helpers are public API (callable, used nowhere but the two splices beside them), and threadsafe::function_typ

---

# Robustesse — les traits

Les quatre traits pris un par un : `is_synchronizable<T>`, `is_synchronizable<const T>`, `is_sendable<T>`, `is_lifetime_aware<T>`, ainsi que la couche réflexive qui les porte.

## F01 — is_sendable<T*>/<T&>/<T&&>, is_sendable and const-is_synchronizable for shared_ptr/weak_ptr/reference_wrapper, and the pointer and reference-member branches of the const walk consume an is_synchronizable<T> answer without detail::dynamic_type_is_known, so an opt-in on a polymorphic non-final base blesses every derived class — inconsistent with unique_ptr and with is_lifetime_aware<shared_ptr>, which do apply the guard on top of opt-in answers

| | |
|---|---|
| **Gravité** | critique |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/sendable.h:32` |

### Le problème

`detail::dynamic_type_is_known` exists precisely because a structural/opt-in answer about a static type proves nothing about the object actually behind an indirection, and `smart_pointers.h` applies it to every `std::unique_ptr` rule. Every other indirection skips it: `is_sendable<T*>`/`<T&>`/`<T&&>` (sendable.h:27,29,32), `is_sendable<shared_ptr<T>>`/`<weak_ptr<T>>`/`<reference_wrapper<T>>` (smart_pointers.h:29,33,37) and their `const` synchronizable counterparts (smart_pointers.h:55,59,63), plus the pointer branch (synchronizable.h:116-125) and the reference-member branch (synchronizable.h:188-196) of the const walk. Specializing `is_synchronizable<Interface>` to true is the library's documented extension point for an interface whose implementations lock themselves — but the moment a derived class does not, the whole hierarchy is blessed. This is not gated behind THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE; a plain user specialization is enough, and `asynchronous_task_launcher::launch_scoped_task` then compiles a program with a real race on a plain `int`.

### Le code concerné

```cpp
template <class T>
struct is_sendable<T*> : is_synchronizable<std::remove_cv_t<T>> {};
```

### La correction

The proposed fix is correct as written and I validated it end to end against a patched copy of the tree; the whole test suite (all 11 TUs) still compiles, the repro and the e2e race are rejected, and atomic/function/synchronized_value cases are unaffected. Concretely:

template <class T>
struct is_sendable<T*>
    : std::bool_constant<is_synchronizable_v<std::remove_cv_t<T>>
                         && detail::dynamic_type_is_known<std::remove_cv_t<T>>> {};

with the same conjunct on is_sendable<T&> and <T&&> (sendable.h:27,29), on is_sendable<shared_ptr<T>>/<weak_ptr<T>>/<reference_wrapper<T>> (smart_pointers.h:29,33,37) and on is_synchronizable<const shared_ptr<T>>/<const weak_ptr<T>>/<const reference_wrapper<T>> (smart_pointers.h:55,59,63), each over the same remove_cv_t/remove_all_extents_t spelling that rule already uses; and inside diagnose_default_is_const_synchronizable, the pointer branch (synchronizable.h:116) and the reference-member branch (synchronizable.h:188) gain

    || !trait_value(^^detail::dynamic_type_is_known, pointee)          // resp. remove_cvref(member_type)

Two amendments I would insist on. (a) Factor the conjunction into one named helper — e.g. `inline consteval bool is_synchronizable_through_indirection(std::meta::info type)` returning `is_synchronizable_type(type) && trait_value(^^detail::dynamic_type_is_known, type)` — plus a `_v` template for the class-template rules, and use it at all eleven sites. Eleven hand-repeated `&& detail::dynamic_type_is_known<std::remove_cv_t<...>>` conjuncts is exactly the kind of duplication that reads badly on a conference slide, and the single name states the rule the guard encodes: "a static-type answer only survives an indirection when the dynamic type is known."
(b) Document the cost in that helper's comment: an opt-in on a polymorphic non-final type becomes unusable through every indirection, so an abstract interface whose implementations lock themselves can no longer be blessed at the interface. The supported spellings become a `final` implementation type or a specialization per concrete class. I verified both halves of this (`is_sendable_v<Iface*>` false, `is_sendable_v<Impl*>` true for `struct Impl final : Iface`). The library already accepts this cost for unique_ptr; making it explicit is what keeps the fix teachable.

### Reproduction

```text
$ cat probe_sync_polybase.cpp
#include <threadsafe/threadsafe.h>
#include <memory>
struct Counter { virtual ~Counter() = default; virtual void bump() = 0; };
template <> struct threadsafe::is_synchronizable<Counter> : std::true_type {};
struct RacyCounter : Counter { int value_ = 0; void bump() override { ++value_; } };
using namespace threadsafe;
static_assert(!is_sendable_v<std::unique_ptr<Counter>>, "unique_ptr<Counter> IS sendable");
static_assert(!is_sendable_v<Counter*>,                "Counter* IS sendable");
static_assert(!is_sendable_v<Counter&>,                "Counter& IS sendable");
static_assert(!is_sendable_v<std::shared_ptr<Counter>>, "shared_ptr<Counter> IS sendable");
static_assert(!is_sendable_v<std::weak_ptr<Counter>>,   "weak_ptr<Counter> IS sendable");
static_assert(!is_sendable_v<std::reference_wrapper<Counter>>, "reference_wrapper<Counter> IS sendable");
static_assert(!is_synchronizable_v<const std::shared_ptr<Counter>>, "const shared_ptr<Counter> IS synchronizable");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_sync_polybase.cpp
probe_sync_polybase.cpp:26:15: error: static assertion failed: Counter* IS sendable
probe_sync_polybase.cpp:27:15: error: static assertion failed: Counter& IS sendable
probe_sync_polybase.cpp:28:15: error: static assertion failed: shared_ptr<Counter> IS sendable
probe_sync_polybase.cpp:29:15: error: static assertion failed: weak_ptr<Counter> IS sendable
probe_sync_polybase.cpp:30:15: error: static assertion failed: reference_wrapper<Counter> IS sendable
probe_sync_polybase.cpp:31:15: error: static assertion failed: const shared_ptr<Counter> IS synchronizable
(note: the unique_ptr<Counter> assertion is the ONLY one that passes -- it is guarded.)

The same hole inside the const walk (probe_sync_polybase_member.cpp), same prelude:
struct HoldsPointer   { Counter *c_; };
struct HoldsReference { Counter &c_; };
static_assert(!is_synchronizable_v<const HoldsPointer>,   "const HoldsPointer TRUE");
static_assert(!is_synchronizable_v<const HoldsReference>, "const HoldsReference TRUE");
static_assert(!is_sendable_v<HoldsPointer>,               "HoldsPointer sendable TRUE");
->
probe_sync_polybase_member.cpp:9:15: error: static assertion failed: const HoldsPointer TRUE
probe_sync_polybase_member.cpp:10:15: error: static assertion failed: const HoldsReference TRUE
probe_sync_polybase_member.cpp:11:15: error: static assertion failed: HoldsPointer sendable TRUE

End to end -- this compiles clean (probe_sync_polybase_e2e2.cpp), same prelude:
void race() {
    RacyCounter racy;
    Counter *as_interface = &racy;
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task([](Counter *c) { c->bump(); }, as_interface);
}
$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_sync_polybase_e2e2.cpp && echo COMPILES
COMPILES: library accepted handing a RacyCounter to another thread

The proposed fix was validated in the same TU (probe_sync_polybase_member.cpp):
template <class T> constexpr bool fixed_sendable_ptr =
    is_synchronizable_v<std::remove_cv_t<T>> && detail::dynamic_type_is_known<std::remove_cv_t<T>>;
static_assert(!fixed_sendable_ptr<Counter>,           "fix fails to reject Counter*");
static_assert(fixed_sendable_ptr<std::atomic<int>>,   "fix breaks atomic<int>*");
static_assert(fixed_sendable_ptr<void()>,             "fix breaks function pointers");
-> none of these three fire (dynamic_type_is_known<void()> is true, verified separately
   in probe_sync_dtk.cpp).
```

### Vérification

I tried three refutation angles and all three failed.

(1) "The repro is wrong / stale." It is not. I re-ran it verbatim in a uniquely-named probe against the real header tree. Exactly the reported set fires: `Counter*`, `Counter&`, `shared_ptr<Counter>`, `weak_ptr<Counter>`, `reference_wrapper<Counter>`, `const shared_ptr<Counter>`, plus `const HoldsPointer` / `const HoldsReference` / `HoldsPointer` in the const walk. The `unique_ptr<Counter>` assertion is the only one that does NOT fire — it is guarded at smart_pointers.h:19. The end-to-end `launch_scoped_task([](Counter*){c->bump();}, as_interface)` compiles clean against the unmodified library, so the library really does hand a `RacyCounter` (plain `int value_`) to another thread.

(2) "Garbage in, garbage out — the user vouched via a specialization, which is what `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` expands to, so the library is entitled to believe it." This does not survive contact with the library's own doctrine. `detail::dynamic_type_is_known` is applied *unconditionally*, including on top of opt-in answers: `is_sendable_v<Counter>` is already true purely because of the user's `is_synchronizable<Counter>` specialization, and `is_sendable<unique_ptr<Counter>>` still rejects it. So the library has already decided that a user's static-type promise does not transfer to an unknown dynamic type. The same decision is baked into `is_lifetime_aware<shared_ptr<T>>` and `<weak_ptr<T>>` (lifetime_aware.h:52-62) and is pinned by tests/test_soundness_regressions.cpp:141-148. `is_sendable`/`is_synchronizable` are the only traits that let `shared_ptr`/`weak_ptr`/`reference_wrapper`/`T*`/`T&` through unguarded. That is an internal inconsistency, not a deliberate policy.

Note also that the promise need not be a lie when written: `Counter` as declared is abstract and stateless, so specializing it is honest; the unsoundness is introduced later by a derived class the author of the specialization never saw. That is precisely the scenario the guard's own comment in utils.h:76-81 describes.

(3) "The fix is unworkable / breaks the suite." It is not and does not. I copied the include tree into the scratchpad, applied the proposed conjunct to `is_sendable<T&>`, `<T&&>`, `<T*>`, `is_sendable<shared_ptr/weak_ptr/reference_wrapper>`, `is_synchronizable<const shared_ptr/weak_ptr/reference_wrapper>`, and both branches of `diagnose_default_is_const_synchronizable` (the pointer branch and the reference-member branch, via `trait_value(^^detail::dynamic_type_is_known, ...)`). All eleven test TUs still compile clean, the repro is fully rejected, the e2e race is rejected, and `atomic<int>*`, `atomic<int>&`, `void(*)()`, `void(&)()`, `synchronized_value<int>&`, `shared_ptr<atomic<int>>`, a `void(&)()` member and a `atomic<int>&` member all still answer true.

Two corrections to the report, neither of which retracts it. First, the "documented extension point" framing is inaccurate — nothing documents it, and a plain specialization is literally what the `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` macro emits, so the report's "not gated behind the macro" phrasing is a distinction without a difference. What actually makes it a bug is the inconsistency with the three guards already in the codebase. Second, the fix has a real, unmentioned cost: it makes an opt-in on any polymorphic non-final type inert, since an abstract interface can only be touched through a pointer or reference. I verified this (`is_sendable_v<Iface*>` becomes false). The library has already paid that price for `unique_ptr`, and the escape is a `final` implementation type or a per-concrete-class specialization (verified: `Impl final` still answers true) — but for an educational library this trade should be stated in the comment, not left implicit.

## F02 — A self-referential owning type (vector/list/map/unique_ptr of itself) makes is_lifetime_aware_v and is_sendable_v ill-formed instead of answering true — including the suite's own Tree/SharedNode

| | |
|---|---|
| **Gravité** | critique |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/lifetime_aware.h:22` |
| **Même défaut que** | `F13` — les types récursifs propriétaires |

### Le problème

The primary template computes its value eagerly at instantiation, and the walk recurses back through `is_lifetime_aware_v` (utils.h:99 `trait_value`). For `struct Node { std::vector<Node> kids; };` the chain is is_lifetime_aware<Node> -> std_wrapper_is_lifetime_aware(vector<Node>) -> is_lifetime_aware_v<Node>, which re-enters a specialization that is still being instantiated. GCC reports `'value' is not a member of ...` rather than a trait answer, so the query is a hard compile error that no `if constexpr` or SFINAE can recover from. Every owning recursive shape I tried fails: `std::vector<Node>`, `std::list<Node>`, `std::map<int,Node>`, `std::optional<std::vector<Node>>`, `std::unique_ptr<Node>`, `std::shared_ptr<Node>`, and mutual recursion through a second struct. This is the single most common shape of an *owning* data structure, i.e. exactly the shape the trait exists to bless, and on a conference slide it is the first thing an attendee will type. The same cycle breaks `is_sendable_v<Node>` and `is_synchronizable_v<Node>` identically, so the fix belongs in the shared recursion strategy.

### Le code concerné

```cpp
template <class T>
struct is_lifetime_aware
    : std::bool_constant<detail::default_is_lifetime_aware(^^T)> {};
```

### La correction

```cpp
Short term (verified, no library change): document and use forward-declare + specialize to cut the cycle —

    struct Node;
    namespace threadsafe {
    template <> struct is_lifetime_aware<Node> : std::true_type {};
    template <> struct is_sendable<Node> : std::true_type {};
    }
    struct Node { std::vector<Node> kids; };

(the is_lifetime_aware half is compile-verified; add the is_sendable specialization for the same reason.)

Proper fix: make the structural walk cycle-tolerant. Have diagnose_default_is_lifetime_aware / the sendable equivalent carry a `std::vector<std::meta::info> visited`, recurse into the consteval walk directly for types that would land on the structural default, and treat an already-visited type as true (coinduction, as Rust's auto traits do). Keep the reflective `trait_value(^^is_lifetime_aware_v, ...)` hop for leaf types so deferred user specializations (tests/test_deferred_specialization.cpp) stay visible. I did not compile this direction, so treat it as a design sketch, not a validated patch.

Add regression tests to tests/test_lifetime_aware.cpp and tests/test_sendable.cpp using the Tree/SharedNode types that already exist in tests/test_smart_pointers.cpp:15-19 — those two types are already in the suite and are only ever asked the one question that terminates.
```

### Reproduction

```text
$ cat probe_la_recursive.cpp
#include <threadsafe/threadsafe.h>
#include <vector>
struct Node { std::vector<Node> kids; };
static_assert(threadsafe::is_lifetime_aware_v<Node>);

$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_la_recursive.cpp
.../lifetime_aware.h:27:60: error: 'value' is not a member of 'threadsafe::is_lifetime_aware<std::vector<Node> >'
   27 | constexpr bool is_lifetime_aware_v = is_lifetime_aware<T>::value;
.../lifetime_aware.h:27:60: error: 'value' is not a member of 'threadsafe::is_lifetime_aware<Node>'
probe_la_recursive.cpp:4:27: error: non-constant condition for static assertion

Sweep of member shapes (COMPILE ERROR = hard error, not a false answer):
  std::vector<Node> kids                   => COMPILE ERROR
  std::unique_ptr<Node> next               => COMPILE ERROR
  std::shared_ptr<Node> next               => COMPILE ERROR
  std::optional<std::vector<Node>> kids    => COMPILE ERROR
  std::list<Node> kids                     => COMPILE ERROR
  std::map<int,Node> kids                  => COMPILE ERROR
  mutual A/B through unique_ptr            => COMPILE ERROR

Same type, all three traits:
  is_sendable_v<Node>        => COMPILE ERROR
  is_synchronizable_v<Node>  => COMPILE ERROR
  is_lifetime_aware_v<Node>  => COMPILE ERROR

Workaround (a) verified to compile and answer TRUE (probe_la_recfix.cpp).
```

### Vérification

I tried to refute it three ways and failed on the core claim.

1) Reproduced verbatim. `struct Node { std::vector<Node> kids; }; static_assert(threadsafe::is_lifetime_aware_v<Node>);` is a hard compile error under g++-16 -std=c++26 -freflection. The cycle is exactly as described and visible in the GCC instantiation stack: `is_lifetime_aware<Node>` (lifetime_aware.h:24, primary, eager `default_is_lifetime_aware(^^T)`) -> `diagnose_default_is_lifetime_aware` member loop (lifetime_aware.h:184) -> `is_lifetime_aware_type` (lifetime_aware.h:71) -> `trait_value` (utils.h:99, `substitute(^^is_lifetime_aware_v, ...)`) -> `is_lifetime_aware<std::vector<Node>>` (allowed_std_wrappers.h:130) -> `std_wrapper_is_lifetime_aware` (allowed_std_wrappers.h:112) -> back to `is_lifetime_aware_v<Node>`, which is still being instantiated. GCC's exact wording on my run is "the value of 'threadsafe::is_lifetime_aware_v<Node>' is not usable in a constant expression" (the auditor quoted "'value' is not a member of ..."; both forms appear depending on the probe — my `requires`-guarded probe produced the auditor's wording).

2) Tried to make it recoverable. `if constexpr (requires { requires threadsafe::is_lifetime_aware_v<T>; })` still hard-errors — the failure is inside the instantiation of the class template, not in the immediate context, so no SFINAE/`requires` escape exists. The claim "no `if constexpr` or SFINAE can recover" holds.

3) Tried to argue the type is unusual or the expected answer isn't "true". It isn't unusual — `vector`/`list`/`map` of an incomplete self type is legal since C++17, and a node that owns its children is precisely what `is_lifetime_aware` exists to bless. And the library's own test suite already contains this shape: tests/test_smart_pointers.cpp:15-19 define `struct Tree { std::unique_ptr<Tree> left; };` and `struct SharedNode { std::shared_ptr<SharedNode> next; };`. `is_lifetime_aware_v<Tree>` hard-errors (4 diagnostics) — the suite only ever asks `!is_synchronizable_v<const Tree>`, which is the one query that happens to terminate.

Two details of the report are wrong and I corrected them:
- "Same type, all three traits => COMPILE ERROR" is false for `is_synchronizable`. My sweep shows `is_synchronizable_v<Node>` answers a clean `false` for every member shape (correct answer: a mutable non-final struct isn't Sync). Only `is_lifetime_aware` and `is_sendable` hard-error. Likewise `is_sendable_v<Node>` with a `std::shared_ptr<Node>` member answers a clean `false`, because `is_sendable<shared_ptr<T>>` routes to `is_synchronizable<T>` and leaves the cycle.
- "nothing in the suite currently exercises a recursive type" is wrong (see Tree/SharedNode above). What is untested is `is_lifetime_aware_v` / `is_sendable_v` on those same types, and both are broken today.

Verified sweep (HARD ERROR = ill-formed, FALSE = clean static_assert failure), member of `Node`:
  vector<Node>            : lifetime_aware HARD ERROR, sendable HARD ERROR, synchronizable FALSE
  unique_ptr<Node>        : HARD ERROR, HARD ERROR, FALSE
  shared_ptr<Node>        : HARD ERROR, FALSE, FALSE
  optional<vector<Node>>  : HARD ERROR, HARD ERROR, FALSE
  list<Node>              : HARD ERROR, HARD ERROR, FALSE
  map<int,Node>           : HARD ERROR, HARD ERROR, FALSE

Fix sanity check: workaround (a) compiles and answers true for both `Node` and `vector<Node>` (probe_refute_la_fix.cpp, exit 0). It touches no library file, so it cannot break the suite; I confirmed all 11 test TUs currently pass with `-fsyntax-only` as a baseline. Direction (b)'s premise is also correct: the cycle crosses a class-template instantiation boundary via `substitute`, which carries no state, so a visited-set threaded only through `diagnose_default_is_lifetime_aware` genuinely cannot observe it — the structural walk would have to recurse into the consteval function directly to break it. I did not implement (b), so I do not vouch for it compiling as written; (a) is the fix I would apply today, plus a regression test.

Severity: for a conference-facing educational library, a recursive owning node is the first thing an attendee types, and the failure is a wall of instantiation-stack diagnostics rather than a false answer. Critical is defensible; at minimum it is high.

## F03 — The derived-behind-base guard fires only on polymorphic pointees, so a shared_ptr (or custom-deleter unique_ptr) to a non-final, non-polymorphic base is judged by the base's members — reaching shared_ptr<synchronized_value<T>> from the library's own make()

| | |
|---|---|
| **Gravité** | critique |
| **Confiance** | probable |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/utils.h:92` |

### Le problème

The guard asks `!is_polymorphic_v<T> || is_final_v<T>`, i.e. it only worries about virtual-function types. But polymorphism is not what makes derived storage legal — a correct deletion path is. `std::shared_ptr<Base>` always type-erases its deleter, so `std::shared_ptr<Base> p = std::make_shared<Derived>()` is well-defined for a completely non-polymorphic `Base`; likewise `std::unique_ptr<Base, void(*)(Base*)>` whose deleter downcasts. For every such handle the guard returns true and the trait answers from `Base`'s members, although the object really there is a `Derived` carrying members the walk never saw. This defeats all four rules that were written to depend on the guard: `is_sendable<unique_ptr<T,D>>`, `is_lifetime_aware<unique_ptr<T,D>>`, `is_synchronizable<const unique_ptr<T,D>>`, and `is_lifetime_aware<shared_ptr/weak_ptr>`. The suite only ever tests the polymorphic case (`PolyBase`/`PolyFinal` in tests/test_soundness_regressions.cpp:52-55), so the non-polymorphic base — the far more common shape behind a shared_ptr — is untested. It also reaches the library's own recommended API: `synchronized_value<T>` is neither final nor polymorphic, so a `std::shared_ptr<synchronized_value<int>>` handed out by `synchronized_value::make()` is sendable even when it owns a derived class that adds a raw pointer.

### Le code concerné

```cpp
template <class T>
consteval bool compute_dynamic_type_is_known() {
    // void erases the type outright: the object behind it is of some other
    // type entirely, and nothing here names it. That is the question this
    // guard asks, so the answer is no.
    if constexpr (std::is_void_v<T>)
        return false;
    else if constexpr (!std::meta::is_complete_type(^^T))
        return false;
    else
        return !std::is_polymorphic_v<T> || std::is_final_v<T>;
}
```

### La correction

Do NOT apply the proposed `no_derived_type_possible` to the shared_ptr/weak_ptr rules: `!is_class_v<T> || is_final_v<T>` rejects every non-final class pointee, which breaks test_lifetime_aware.cpp:71,73 (`shared_ptr<std::string[]>`, `shared_ptr<const std::string>`) and test_synchronized_value.cpp:69 (`shared_ptr<sync_int>`), and makes `synchronized_value::make()` unusable with `launch_task`. C++ cannot enumerate derived classes, so no sound *and* usable guard exists there.

Split the problem instead:

(a) unique_ptr with a non-default deleter — tighten. Derived storage is well-defined precisely because the deleter downcasts, and this shape is rare enough that a stricter guard costs nothing. In the three unique_ptr rules, require `dynamic_type_is_known<T> && (std::is_same_v<D, std::default_delete<T>> || !std::is_class_v<T> || std::is_final_v<T>)`. With `default_delete` and a non-virtual destructor, storing a derived object is itself UB, so the existing guard remains the right answer there.

(b) shared_ptr / weak_ptr — document, do not check. Extend the comment at lifetime_aware.h:47-49 to say plainly that the trait answers about the *static* pointee: a `shared_ptr<Base>` may own a `Derived` whose extra members the walk never saw, and whose destructor runs on the receiving thread. Tell the reader the two ways out — make the base `final`, or specialize `is_lifetime_aware`/`is_sendable` for the base to state the intent. For an educational library this is the honest move: naming the limit is worth more than a guard that would reject `shared_ptr<std::string>`.

(c) Add the missing test either way. tests/test_soundness_regressions.cpp:52-55 covers only `PolyBase`/`PolyFinal`; add a non-polymorphic base with a custom deleter so the tightened unique_ptr rule is pinned, and a comment recording the shared_ptr case as a known, documented limitation.

### Reproduction

```text
// probe_soundness_dynguard.cpp
#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>
using namespace threadsafe;

struct Owner { std::vector<int> data; };                  // non-polymorphic base
struct Leaky : Owner { int* borrowed; };                  // neither sendable nor lifetime aware

void destroy_as_leaky(Owner* p) { delete static_cast<Leaky*>(p); }
using Handle = std::unique_ptr<Owner, void (*)(Owner*)>;  // custom deleter: derived storage is legal

static_assert(!is_sendable_v<Leaky>,        "Leaky itself is rejected");
static_assert(!is_lifetime_aware_v<Leaky>,  "Leaky itself is rejected");

static_assert(detail::dynamic_type_is_known<Owner>, "guard passes: Owner is not polymorphic");
static_assert(is_sendable_v<Handle>,        "HOLE: sendable");
static_assert(is_lifetime_aware_v<Handle>,  "HOLE: lifetime aware");
static_assert(is_lifetime_aware_v<std::shared_ptr<Owner>>, "HOLE: shared_ptr lifetime aware");

int main() {
    asynchronous_task_launcher launcher;
    Handle handle{new Leaky{{}, nullptr}, &destroy_as_leaky};   // dynamic type is Leaky
    launcher.launch_task([](Handle) {}, std::move(handle));     // ACCEPTED
}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_soundness_dynguard.cpp
$ echo $?
0

Every assertion holds and launch_task accepts the call: a raw borrowed pointer is
handed to a worker thread through a handle both traits declare clean.
```

### Vérification

I tried to refute it three ways and it survived all three.

1. Is the claimed code path real? Yes. `compute_dynamic_type_is_known` (utils.h:83-93) returns `!is_polymorphic_v<T> || is_final_v<T>`, so every non-polymorphic class passes. That guard is the only thing standing between a *structural* answer (walk of the static pointee's members) and an object of a derived type. The three rules that genuinely carry a structural answer through an indirection are `is_sendable<unique_ptr<T,D>>`, `is_lifetime_aware<unique_ptr<T,D>>` and `is_lifetime_aware<shared_ptr/weak_ptr>`. (The fourth cited rule, `is_synchronizable<const unique_ptr<T,D>>`, asks the *full* opt-in `is_synchronizable_v<T>`, and `is_sendable<shared_ptr<T>>` likewise — those are user vouches, not structural walks, so they are much weaker instances. Same for raw `T*`/`T&`, which is why they carry no guard at all. That part of the finding is overstated but does not change the verdict.)

2. Is derived storage behind a non-polymorphic base actually well-defined? Yes, in exactly the two shapes named: `shared_ptr` always type-erases its deleter, and a `unique_ptr<Base, void(*)(Base*)>` whose deleter downcasts destroys the right type. No UB to hide behind.

3. Does it compile? The submitted repro compiles clean (exit 0) once I dropped my own extra assert. I then built a stronger one that shows actual harm rather than just a `true`: a `Derived : Base` whose destructor writes through a borrowed pointer, held as `shared_ptr<Base>` — the last reference is dropped on the worker thread, so `~Derived` runs there, on a dead stack frame. And it reaches the library's flagship API: `std::shared_ptr<synchronized_value<int>>`, the exact type `synchronized_value::make()` hands out, is both sendable and lifetime-aware, and `launch_task` accepts it while the object behind it is a `Sneaky : synchronized_value<int>` carrying a raw borrow. The library rejects `Sneaky` itself and accepts the handle to it — an inconsistency by its own standard.

What does NOT survive is the proposed fix. `!is_class_v<T> || is_final_v<T>` applied to the shared_ptr/weak_ptr rules rejects every non-final class pointee, i.e. essentially every shared_ptr in practice. I checked the pointees of currently-passing tests: `std::string` (test_lifetime_aware.cpp:71,73) and `synchronized_value<int>` (test_synchronized_value.cpp:69) both fail the proposed predicate, so the fix breaks the suite and, worse, makes the library's own recommended `synchronized_value::make()` pattern unusable with `launch_task`. C++ has no closed world of derived classes; a sound-and-usable guard for shared_ptr does not exist, so this half has to be documented rather than checked.

One honest counterargument, which is why I am at "likely" rather than "certain": the guard's comment scopes itself to polymorphic pointees on purpose, and for a non-polymorphic base no base-typed *operation* can reach a derived member without an explicit downcast. The counter to that counter is the destructor — for shared_ptr and for a downcasting deleter, the derived destructor really does run on the receiving thread, which is precisely what Send governs. The gap is real; only its priority is arguable.

## F04 — Every completeness test in the library freezes a `false` into a memoized specialization — `is_sendable`/`is_synchronizable`/`is_lifetime_aware` defaults and `detail::dynamic_type_is_known` — so the plain pimpl idiom gets opposite answers in the .cpp that completes the impl and in every other TU (IFNDR, no diagnostic)

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/synchronizable.h:150` |

### Le problème

`is_synchronizable<const T>` and `is_sendable<T>` are class template specializations: the first instantiation freezes the answer for the whole TU. When `T` is still incomplete at that point the walk `reject`s and the frozen answer is `false`, even after `T` is later completed. Because the rejection is a `false` and not a hard error, nothing warns. A header included in a different order in another TU instantiates the same specialization with the opposite value; the two definitions of `threadsafe::is_synchronizable<std::atomic<Later>>` differ and the program is ill-formed, no diagnostic required. I built and ran a three-TU program that prints two different answers for the same trait. For an educational library this is the worst kind of trap: everything compiles, both `static_assert`s pass, and the safety answer depends on include order.

### Le code concerné

```cpp
if (!is_complete_type(type))
        reject(type,
               u8"is incomplete — is_synchronizable<const T> needs a complete "
               u8"type; specialize is_synchronizable for types holding a "
               u8"pointer to an incomplete type (the pimpl idiom)",
               path);
```

### La correction

Do NOT apply the fix as proposed: it breaks test_soundness_regressions.cpp:150 and it does not close the hole (divergence persists through `dynamic_type_is_known` even with a user specialization present — verified).

Four sites must change together, or none:
  - sendable.h:141          (incomplete class/union reject)
  - synchronizable.h:150    (same)
  - lifetime_aware.h:167    (same)
  - utils.h:88-93           `compute_dynamic_type_is_known`: the `!is_complete_type(^^T)` branch returns false and is memoized into the `dynamic_type_is_known` variable template

At each, replace the silent `false` with an escaping `std::meta::exception` (for the three traits, hoist the class/union completeness test above the try/catch so the catch-all in `default_is_*` does not swallow it). I verified this compiles and turns the pimpl divergence into a hard error in BOTH TUs instead of 1-vs-0.

Then the pimpl idiom must be re-expressed as a specialization on the HANDLE, not on the incomplete type — `template<> struct threadsafe::is_sendable<Widget> : std::true_type {};` in the shared header — which is what the existing diagnostic text already tells users to do and which is TU-order-independent. Consequently test_soundness_regressions.cpp:150 (`!is_sendable_v<Pimpl>`) and :153 (`!is_lifetime_aware_v<std::shared_ptr<Implementation>>`) must be rewritten; under the full patch those are the only two failures across all 11 test files.

If the maintainer prefers not to take a breaking change for a conference-facing library, the honest minimum is to stop pretending the query is answerable: document at the four sites that asking any of these traits about an incomplete type (directly or through a smart pointer) yields a TU-local answer and is IFNDR, and make the pimpl test in test_soundness_regressions.cpp say so.

### Reproduction

```text
Single TU (probe_sync_incomplete_memo.cpp):
#include <threadsafe/threadsafe.h>
struct Later;
static_assert(!threadsafe::is_synchronizable_v<const Later>, "incomplete answered TRUE");
static_assert(!threadsafe::is_sendable_v<Later>, "incomplete sendable answered TRUE");
struct Later { int value; };   // now complete and obviously safe
static_assert(threadsafe::is_synchronizable_v<const Later>, "const Later still FALSE after completion");
static_assert(threadsafe::is_sendable_v<Later>, "Later still NOT sendable after completion");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_sync_incomplete_memo.cpp
probe_sync_incomplete_memo.cpp:8:15: error: static assertion failed: const Later still FALSE after completion
probe_sync_incomplete_memo.cpp:9:15: error: static assertion failed: Later still NOT sendable after completion

Cross-TU, reaching the NON-const is_synchronizable through the std::atomic rule:
// probe_sync_odr_common.h
#pragma once
#include <threadsafe/threadsafe.h>
struct Later;
bool answer_from_other_tu();
// probe_sync_odr_a.cpp
#include "probe_sync_odr_common.h"
static_assert(!threadsafe::is_synchronizable_v<std::atomic<Later>>);
struct Later { int value; };
bool answer_here() { return threadsafe::is_synchronizable<std::atomic<Later>>::value; }
// probe_sync_odr_b.cpp
#include "probe_sync_odr_common.h"
struct Later { int value; };
static_assert(threadsafe::is_synchronizable_v<std::atomic<Later>>);
bool answer_from_other_tu() { return threadsafe::is_synchronizable<std::atomic<Later>>::value; }
// probe_sync_odr_main.cpp prints both

$ g++-16 -std=c++26 -freflection -I include probe_sync_odr_a.cpp probe_sync_odr_b.cpp probe_sync_odr_main.cpp -o probe_sync_odr && ./probe_sync_odr
TU-A says 0, TU-B says 1
exit=0

Both static_asserts -- one asserting false, one asserting true, for the same trait on
the same type -- compiled without a diagnostic, and the linked program reports both.
```

### Vérification

I tried to refute this on the reproduction lens and could not; both repros reproduce verbatim, and the underlying defect is broader than reported while the proposed fix is demonstrably wrong.

WHAT SURVIVED
1. Single-TU repro: reproduced exactly. `is_synchronizable_v<const Later>` / `is_sendable_v<Later>` freeze at false while `Later` is incomplete and stay false after `struct Later { int value; };` completes it. Both post-completion static_asserts fail.
2. Cross-TU repro: reproduced exactly, byte-for-byte the claimed output — `TU-A says 0, TU-B says 1`, exit 0, no diagnostic. Two implicit instantiations of the same specialization with different meanings is IFNDR under [temp.point]/8, so the ODR claim is formally right.
3. I built a STRONGER, non-contrived repro that the original finding does not have: the plain pimpl idiom the library's own error message recommends, with no static_asserts and no deliberate ordering trick (scratchpad/refute_pimpl/). `widget.h` forward-declares `WidgetImpl` and defines `struct Widget { std::unique_ptr<WidgetImpl> impl; };`; `widget.cpp` completes `WidgetImpl` as the idiom requires, `user.cpp` does not. `threadsafe::is_sendable<Widget>::value` is 1 in one TU and 0 in the other, silently. This is ordinary, correct client code — not an auditor's trap.

CORRECTIONS TO THE FINDING
a) The axis label is wrong. The frozen answer is always `false`, the conservative side — no unsafe type is ever blessed true because of incompleteness. This is an IFNDR / include-order-determinism defect, not a permissiveness hole. It still deserves attention (silent, order-dependent answers in a teaching library), but calling it "soundness, answers TRUE for an unsafe type" misstates it.
b) The location is too narrow. synchronizable.h:150 is not the branch a realistic program hits. The identical shape is at sendable.h:141 and lifetime_aware.h:167 — and, missed entirely by the finding, at utils.h:88-93 (`compute_dynamic_type_is_known`, memoized into the `dynamic_type_is_known` variable template), which is what the smart-pointer rules consult.
c) The suggested escape hatch does not work. I declared `template<> struct threadsafe::is_sendable<WidgetImpl> : std::true_type;` in the shared header, before any use, on the still-incomplete type. The two TUs STILL print 1 and 0, because `is_sendable<unique_ptr<T,D>>` also multiplies in `dynamic_type_is_known<T>`, which is independently completeness-dependent and independently memoized. A trait-only fix leaves the bug intact for every smart-pointer handle.

THE PROPOSED FIX IS WRONG
I applied it literally to a copy of the tree (hoisting the completeness test above the try/catch in `default_is_const_synchronizable` and `default_is_sendable`, throwing `std::meta::exception`) and compiled all 11 test files. It breaks test_soundness_regressions.cpp:150 — `static_assert(!is_sendable_v<Pimpl>)` becomes a hard compile error, "uncaught exception of type 'std::meta::exception'", cascading into `'value' is not a member of threadsafe::is_sendable<Pimpl>`. That test exists precisely to pin the pimpl behaviour the current message advertises, so the fix destroys the feature the finding's own diagnostic text points users at. The finding's parenthetical ("the pointer branch never reaches here") is true for the const-synchronizable branch but irrelevant: `is_sendable<unique_ptr<T,D>>` asks `is_sendable_v<T>` directly on the incomplete pointee, which does land on the reject.

VERDICT: real, reproducible, but mis-titled, under-scoped, and shipped with a fix that breaks the suite.

## F05 — assert_lifetime_aware discards the specialization's reason and re-derives one from the structural walk, printing libstdc++ layout members and circular advice — the same message it would print for types the library accepts

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/lifetime_aware.h:92` |
| **Même défaut que** | `F06`, `F07`, `F09` — les diagnostics re-dérivés par la marche structurelle |

### Le problème

`descend_lifetime_aware` unconditionally re-derives a reason with the *structural* walk, even when the trait's `false` came from a specialization in smart_pointers.h or allowed_std_wrappers.h. The structural walk then dives into libstdc++ layout and reports a member the specialization never consulted. For `std::unique_ptr<Base>` (rejected because `dynamic_type_is_known<Base>` is false) the message names `std::__uniq_ptr_data<...>::_Head_base<0, Base*, false>::_M_head_impl (Base*)` and advises "hold the object, or a std::shared_ptr to it" — but `std::shared_ptr<Base>` is rejected for exactly the same reason, so the advice sends the user in a circle. For `std::vector<int*>` (rejected because its element type is not lifetime aware) the message names `std::_Vector_base<...>::_Vector_impl_data::_M_start (int**)`. The comment above the function asserts "Coming back from the walk means `inner` answers false through a specialization the walk cannot read" — that is precisely what does *not* happen here: the walk never comes back, it invents its own reason. In a library whose pitch is that the diagnostics name the culprit, this is the failure mode the audience will see.

### Le code concerné

```cpp
[[noreturn]] inline consteval void
descend_lifetime_aware(std::meta::info inner, const std::u8string &path) {
    diagnose_default_is_lifetime_aware(inner, path);

    reject(inner,
           u8"is not lifetime aware: is_lifetime_aware is specialized to false "
           u8"for it",
           path);
}
```

### La correction

Reject the proposed fix as written; it is directionally right but replaces a precise message with a wrong one for `reference_wrapper` and `unique_ptr`. Instead, teach `descend_lifetime_aware` the reasons behind the library's own specializations, before it ever falls into the structural walk:

1. `std_wrapper` (allowed_std_wrappers.h): find the first entry of `detail::wrapped_types_of(inner)` that is not lifetime aware and recurse into it, so `std::vector<std::atomic<int>*>` reports the element type `std::atomic<int>*` as the culprit rather than `_M_start`.
2. `unique_ptr`/`shared_ptr`/`weak_ptr` (smart_pointers.h): split the two conjuncts. If the pointee is not lifetime aware, recurse into the pointee. If the pointee is fine but `detail::dynamic_type_is_known` is false, say that — e.g. "its pointee is polymorphic and not final, so the answer read off the static type does not hold of the object actually pointed to; mark it final or specialize is_lifetime_aware". This is what removes the circular "use a std::shared_ptr" advice.
3. `reference_wrapper`: reject directly with "borrows its referent instead of keeping it alive" without descending into `_M_data`.

Keep the auditor's `is_declared_in_std` guard, but place it *after* those cases, as a backstop for std types the library has no rule for — never before them.

Apply the same treatment to sendable.h and synchronizable.h, whose `descend_*` functions have the identical shape (verified: the launcher probe with `std::vector<int*>` shows assert_sendable emitting its own unrelated "has a user-written copy, move or destructor" reason for a type answered by a specialization).

### Reproduction

```text
$ cat probe_la_diag2.cpp
#include <threadsafe/threadsafe.h>
#include <memory>
struct Base { virtual ~Base(); int x; };
consteval void go() { threadsafe::assert_lifetime_aware<std::unique_ptr<Base>>(); }
constexpr int x = (go(), 0);

CURRENT (g++-16 -std=c++26 -freflection -fsyntax-only):
  std::unique_ptr<Base>
    what(): 'std::unique_ptr<Base>::_M_t (std::__uniq_ptr_data<Base, std::default_delete<Base>, true, true>)::(base std::__uniq_ptr_impl<...>)::_M_t (std::tuple<Base*, std::default_delete<Base> >)::(base std::_Tuple_impl<0, Base*, std::default_delete<Base> >)::(base std::_Head_base<0, Base*, false>)::_M_head_impl (Base*) is a reference or a raw pointer: it borrows its referent instead of keeping it alive — hold the object, or a std::shared_ptr to it'
  std::vector<int*>
    what(): 'std::vector<int*>::(base std::_Vector_base<int*, std::allocator<int*> >)::_M_impl (..._Vector_impl)::(base ..._Vector_impl_data)::_M_start (int**) is a reference or a raw pointer: ...'
  std::reference_wrapper<int>
    what(): 'std::reference_wrapper<int>::_M_data (int*) is a reference or a raw pointer: ...'
  std::function<void()>
    what(): 'std::function<void()>::(base std::_Function_base)::_M_functor (std::_Any_data)::_M_unused (std::_Nocopy_types)::_M_object (void*) is a reference or a raw pointer: ...'

WITH THE FIX APPLIED (copy of include/ patched in scratchpad, dir inc_d):
  std::vector<int*>            what(): 'std::vector<int*> is not lifetime aware — a standard type answers through a specialization of is_lifetime_aware, so look at what you asked it to hold'
  std::unique_ptr<Base>        (same wording, names std::unique_ptr<Base>)
  std::reference_wrapper<int>  (same wording)
  std::function<void()>        (same wording)

Regression check, all 11 files in tests/ compiled against inc_d: errors=0 for every file (test_diagnostics.cpp included).
```

### Vérification

I tried to refute this on the reproduction lens and failed on every attempt.

1. The repro is exact. `assert_lifetime_aware<std::unique_ptr<Base>>()` produces character-for-character the quoted `..._Head_base<0, Base*, false>::_M_head_impl (Base*) is a reference or a raw pointer ... hold the object, or a std::shared_ptr to it`. The other three quoted messages (`std::vector<int*>`, `std::reference_wrapper<int>`, `std::function<void()>`) also reproduce verbatim.

2. The mechanism is as described, not incidental. `assert_lifetime_aware` (lifetime_aware.h:83) calls `descend_lifetime_aware(^^T, ...)`, which calls `diagnose_default_is_lifetime_aware` — the *structural* function — bypassing whatever specialization actually produced the `false`. The `reject` at line 95 that the comment at 88-90 describes is genuinely unreachable for these types.

3. I tried to argue the printed reason is at least true in substance. It is provably not. I called `detail::descend_lifetime_aware(^^std::vector<int>, u8"std::vector<int>")` on a type the library answers TRUE for (`static_assert(is_lifetime_aware_v<std::vector<int>>)` passes) and got the *same message shape* naming `_Vector_impl_data::_M_start (int*)`. So the structural walk emits this reason for accepted and rejected vectors alike — it carries zero information about the verdict.

4. I tried to argue the circular-advice claim was overstated. It is not: `static_assert(!is_lifetime_aware_v<std::shared_ptr<Base>>)` passes, so "hold the object, or a std::shared_ptr to it" sends the user to a type rejected for the identical reason. And the real reason is not borrowing at all — `is_lifetime_aware_v<Base>` is TRUE; the rejection comes from `dynamic_type_is_known<Base>` being false (`is_lifetime_aware_v<std::unique_ptr<Leaf>>` with `Leaf final` is true). The message never mentions polymorphism.

5. I tried to argue it is hypothetical (only reachable by hand-calling assert_*). It is not. `asynchronous_task_launcher.h:61,63` calls `assert_lifetime_aware<F>()` / `assert_lifetime_aware<Args>()`, so the headline API hits it. `launcher.launch_task([](V){}, V{})` with `V = std::vector<std::atomic<int>*>` (sendable, not lifetime aware, so the sendable assert passes first) prints the `_Vector_impl_data::_M_start (std::atomic<int>**)` message. This is exactly what a conference audience would see.

The one place the finding is soft is its proposed fix. I applied it to a copy of include/ and confirmed both of its claims: it compiles, and all 11 test files give errors=0. But it degrades two cases. For `std::reference_wrapper<int>` the current message ("_M_data (int*) ... it borrows its referent") is accurate and its advice is correct, and the fix replaces it with "look at what you asked it to hold" — which is wrong, since reference_wrapper is never lifetime aware regardless of its argument. Same wrongness for `std::unique_ptr<Base>`, where Base *is* lifetime aware. So the fix trades a precise-but-irrelevant message for a vague-and-still-misleading one. I corrected it below: give the library's own specialization families real reasons first, and keep the std boundary only as a backstop.

## F06 — `descend_sendable` explains every rejection with the structural walk, so any type answering false through a specialization keyed on a class template — the whole std wrapper allow-list, unique_ptr, shared_ptr, weak_ptr, reference_wrapper, copy_on_write — is blamed on libstdc++'s constructor templates instead of its real cause; `std::vector<int*>` is told to "specialize is_sendable" when the answer is `int*`

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/sendable.h:67` |
| **Même défaut que** | `F05`, `F07`, `F09` — les diagnostics re-dérivés par la marche structurelle |

### Le problème

`descend_sendable` explains a rejection by re-running `diagnose_default_is_sendable`, i.e. the structural default. But for every type whose answer came from a *specialization* — the whole `allowed_std_wrappers` family, `unique_ptr`, `shared_ptr`, `copy_on_write` — the structural walk is not the code that produced the answer, and it stops at the first thing it dislikes: `has_only_default_copy_move_destroy` is false for all of them because they have constructor templates. So the user is told the container has "a user-written copy, move or destructor" and is advised to "specialize is_sendable to state the intent", which is exactly the wrong advice: the real and correct reason is `int*`. `asynchronous_task_launcher::launch_task` routes all its diagnostics through `assert_sendable`, so this is the message a conference audience sees for the single most idiomatic failure — a container of borrows. tests/test_soundness_regressions.cpp:120 already exercises that exact case, but only asserts that it is rejected, never that the reason is right.

### Le code concerné

```cpp
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);

    reject(inner,
           u8"is not sendable: is_sendable is specialized to false for it",
           path);
}
```

### La correction

The proposed fix is correct as written and I verified it builds and keeps all 11 test files green. Apply it as given: forward-declare `explain_sendable` next to `descend_sendable` in the detail block at sendable.h:12-18, then read the template arguments at the top of `descend_sendable`, before falling back to `diagnose_default_is_sendable`.

Two amendments.

First, the report's residual list is right and should be closed rather than left as a note, because `std::shared_ptr<int>` is a headline conference example and still emits the wrong message after the fix. `shared_ptr`, `weak_ptr` and `reference_wrapper` are keyed on `is_synchronizable`, not `is_sendable`, so the added loop passes them. They need one explicit line each in `descend_sendable`, e.g. for a `shared_ptr<T>` whose `T` is not synchronizable: "shares its pointee with every other copy, so the pointee must be synchronizable". `std::unique_ptr<PolyBase>` is a fourth case the report missed — misexplained before and after the fix, since its true reason is `detail::dynamic_type_is_known`, which no message mentions; that deserves its own line too.

Second, add a regression test. tests/test_diagnostics.cpp currently tests only the half of the contract that returns, and states so in a comment; nothing anywhere pins a rejection message, which is why this went unnoticed. The messages are thrown as `std::meta::exception`, so they are catchable in a consteval context — a test can call `detail::descend_sendable` inside a try/catch and `static_assert` that `what()` contains the expected substring, which would make the reason itself testable rather than only the boolean.

Do not move the template-argument loop into `diagnose_default_is_sendable`; the report is right that this would change answers for a class template that does not actually hold its argument.

### Reproduction

```text
// probe_sendable_diag_vec.cpp
#include <threadsafe/threadsafe.h>
#include <vector>
int main() { threadsafe::assert_sendable<std::vector<int*>>(); }

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_sendable_diag_vec.cpp
error: uncaught exception of type 'std::meta::exception'; 'what()':
  'std::vector<int*> has a user-written copy, move or destructor — or a template
   that may be selected as one — which can share state the members do not show;
   specialize is_sendable to state the intent'

// Identical wrong reason for, verified one probe each:
//   std::pair<int*,int>, std::optional<int*>, std::unique_ptr<int*>,
//   std::shared_ptr<int>, threadsafe::copy_on_write<int*>
// and through the launcher:
//   launcher.launch_task([](std::vector<int*>) {}, pointers);
//   -> same 'std::vector<int*> has a user-written copy, move or destructor ...'

// With the proposed fix applied to a copy of the tree, the same five probes give:
//   'std::vector<int*> is a pointer or a reference: sending it shares its
//    referent with the other thread, so the referent must be synchronizable ...'
// (root cause now correct; the path still does not record the hop into the
//  argument, because path_step returns {} for a type subject). All 11 files in
// tests/ still compile with zero errors.
```

### Vérification

I re-ran every probe the finding claims, on the real tree, and each one reproduced verbatim.

1. The core repro is exact. `assert_sendable<std::vector<int*>>()` on the unmodified tree at include/threadsafe/details/sendable.h:65-72 yields, character for character, the message quoted in the report: "std::vector<int*> has a user-written copy, move or destructor — or a template that may be selected as one — which can share state the members do not show; specialize is_sendable to state the intent". The real reason is `int*`, and the advice given ("specialize is_sendable to state the intent") is exactly the wrong advice for a container of borrows.

2. The mechanism is as described. `descend_sendable` calls only `diagnose_default_is_sendable`, the structural default. For `std::vector<int*>` the answer actually came from the `detail::std_wrapper` partial specialization in allowed_std_wrappers.h, which never consults members — it reads `wrapped_types_of`. The structural walk that produces the message is therefore not the code that produced the answer, and it stops at the `has_only_default_copy_move_destroy` gate (sendable.h:146), which is false for every libstdc++ container because of their constructor templates. allowed_std_wrappers.h even documents this ("its constructor templates block the structural default anyway").

3. The four secondary probes reproduce identically: std::pair<int*,int>, std::optional<int*>, std::unique_ptr<int*>, std::shared_ptr<int>, threadsafe::copy_on_write<int*> all emit the same "user-written copy, move or destructor" text. I found three more the report did not list — std::weak_ptr<int>, std::reference_wrapper<int>, std::function<void()> — same wrong message, so the blast radius is if anything larger than reported.

4. The launcher claim holds. `launcher.launch_task([](std::vector<int*>){}, pointers)` routes through `explain_launch_task` -> `assert_sendable<Args>` and surfaces the same wrong message, reported at asynchronous_task_launcher.h:99. So this is the message a conference audience sees for the most idiomatic failure.

5. tests/test_soundness_regressions.cpp:120-123 does assert `!can_launch_task<decltype([](std::vector<SyncType*>){}), std::vector<SyncType*>>` and never checks the reason — confirmed by reading it. tests/test_diagnostics.cpp only tests the *agreeing* half (assert_* compiles on conforming types) and explicitly notes "the throwing half *is* a compile error by design", so no test anywhere pins a rejection message.

6. I applied the proposed fix verbatim to a full copy of the tree (forward-declared `explain_sendable` alongside `descend_sendable`, added the template-argument loop at the top of `descend_sendable`). It compiles. All 11 files in tests/ compile with zero errors. The five probes now give "std::vector<int*> is a pointer or a reference: sending it shares its referent ..." — root cause correct. The fix is explanation-only: `descend_sendable` is reachable only with a non-empty path, and only `assert_sendable` seeds one, so no trait answer can change — which the 11 passing test files confirm.

Attempts to refute that failed: (a) the messages are not merely imprecise, they name a different type and give actively misleading advice; (b) it is not confined to one exotic type — it covers the entire std wrapper allow-list, both smart pointers, and copy_on_write; (c) no existing test would catch a regression here; (d) the proposed fix is not speculative, it builds and keeps the suite green.

The only thing I would push back on is the title's universal quantifier. Not *every* specialization-sourced false is misexplained: the `is_sendable<T*>`, `is_sendable<T&>` and `is_sendable<T[N]>` specializations are mirrored by explicit branches in `diagnose_default_is_sendable` (sendable.h:112-125), and I verified `int*`, `int&` and `int*[3]` all produce the correct root-cause message. The defect is specific to specializations keyed on a class template. That narrows the title, not the severity.

The report's own residual list is accurate and I verified it: with the fix, std::shared_ptr<int> still says "user-written copy, move or destructor" because that specialization is keyed on is_synchronizable, not is_sendable, so the argument `int` passes the added loop. Same for weak_ptr and reference_wrapper. I also found one the report missed: std::unique_ptr<PolyBase> is misexplained both before and after the fix — its real reason is `dynamic_type_is_known`, which no message mentions. Note a small internal inconsistency in the report: the repro block says "the same five probes give" the corrected message, but shared_ptr<int> does not; the Residual note immediately below states this correctly, so it reads as sloppy wording rather than a false claim.

One caveat on the fix worth carrying forward: because the loop runs before the completeness and structural gates, a class template whose true rejection reason is something else (incompleteness, unknown dynamic type) but which happens to carry a non-sendable type argument will now be explained by that argument. Since the code only runs on types already answering false, this trades one imperfect explanation for a usually-better one, but it is a heuristic, not a proof.

## F07 — descend_sendable runs the structural walk even when the answer came from a specialization, so shared_ptr<int> is rejected with a non-operative reason whose advice ("specialize is_sendable") compiles a data race

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | probable |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/sendable.h:65` |
| **Même défaut que** | `F05`, `F06`, `F09` — les diagnostics re-dérivés par la marche structurelle |

### Le problème

descend_sendable / descend_lifetime_aware run the *structural* diagnosis on `inner` unconditionally, even when `inner`'s answer comes from a partial specialization the walk never used. The structural walk then throws for its own unrelated reason and that reason is printed as the truth. `launcher.launch_task([](std::shared_ptr<int> p){}, std::make_shared<int>(1))` — the single likeliest slip in a live demo — is rejected because is_sendable<shared_ptr<T>> = is_synchronizable<T> and int is not synchronizable, yet it says "std::shared_ptr<int> has a user-written copy, move or destructor ... specialize is_sendable to state the intent". Following that advice would bless every shared_ptr<int> and turn the library's central safety rule off. The lifetime_aware side is worse: is_lifetime_aware<std::reference_wrapper<T>> is an explicit std::false_type at lifetime_aware.h:47, and the diagnostic still walks into libstdc++ and blames the private member `_M_data`.

### Le code concerné

```cpp
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);

    reject(inner,
           u8"is not sendable: is_sendable is specialized to false for it",
           path);
}
```

### La correction

Do not adopt the marker gate alone — it silences the bad advice but names no cause, and it adds a registry that must be kept in sync with every specialization (including users'). Two changes, in order of value:

1. Teach the walk the indirections the library already specializes, so the message names the real culprit. At the top of diagnose_default_is_sendable, before the structural guards, recognise std::shared_ptr<T> / std::weak_ptr<T> / std::reference_wrapper<T> and chain into the synchronizable explanation of the referent, e.g. reject with "shares its referent with the other thread, so int must be synchronizable — is_synchronizable<T> is opt-in; use shared_ptr<const int>, or a type that synchronizes itself". That reproduces the actual rule (smart_pointers.h:29) instead of a coincidental structural fact, and it is the message a conference audience needs.

2. Independently, stop the copy/move guard from reading as "vouch for it" on a type the trait did not answer structurally. If the marker gate is kept, it should sit in descend_sendable / descend_lifetime_aware / descend_const_synchronizable as proposed (it compiles and breaks no test), but only as a backstop behind (1).

Drop the reference_wrapper part of the finding entirely: that diagnostic is correct and its advice is safe.

### Reproduction

```text
// probe_launcher_misdiag2.cpp
#include <threadsafe/threadsafe.h>
#include <memory>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](std::shared_ptr<int> p){ (void)p; }, std::make_shared<int>(1));
}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_launcher_misdiag2.cpp
error: uncaught exception of type 'std::meta::exception'; 'what()':
  'std::shared_ptr<int> has a user-written copy, move or destructor — or a template
   that may be selected as one — which can share state the members do not show;
   specialize is_sendable to state the intent'

// and, with an explicit false_type specialization in the library:
// probe_launcher_advice_loop.cpp -> launcher.launch_task(std::ref(counter));
'what()': 'std::reference_wrapper<SyncCounter>::_M_data (SyncCounter*) is a reference or a
           raw pointer: it borrows its referent instead of keeping it alive — hold the
           object, or a std::shared_ptr to it'
```

### Vérification

I tried to refute this on four fronts and only two of them landed.

WHAT SURVIVES. The shared_ptr repro reproduces byte-for-byte. `descend_sendable` (sendable.h:65-72) runs `diagnose_default_is_sendable` unconditionally, so for a type whose answer comes from a partial specialization the structural walk gets to throw first and its reason is what the user reads. `is_sendable<std::shared_ptr<T>>` is `is_synchronizable<T>` (smart_pointers.h:29) and `int` is not synchronizable — that is the operative reason, deliberately so (test_smart_pointers.cpp:48 asserts exactly this answer). But the printed reason is the copy/move guard, and its remediation is "specialize is_sendable to state the intent". I followed that advice literally in probe_refute_descend_advice.cpp: it compiles clean and launches two threads incrementing the same `int` through the same shared_ptr. So the message steers a user off the library's central rule. The `[[noreturn]]` fallback at sendable.h:69 ("is_sendable is specialized to false for it") exists precisely for this case and is unreachable for any std type, because every such type trips `has_only_default_copy_move_destroy` first. That is a defect against the code's own stated intent (the comment at sendable.h:62-64 asserts "coming back from the walk means inner answers false through a specialization"), not a documented trade-off — CLAUDE.md says nothing about diagnostics at all, and test_diagnostics.cpp:7-10 explicitly states only the agreeing half is tested, so no message content is pinned anywhere.

WHAT DOES NOT SURVIVE. (1) The title's word "false". `std::shared_ptr<int>` really does have user-written copy/move/destructor members, so the sentence is a true statement about the type — both reasons independently hold. The defect is that it is not the operative reason and its advice is unsound, not that it is a false claim. (2) The reference_wrapper half is wrong, and I confirmed it by running it. The message is "std::reference_wrapper<SyncCounter>::_M_data (SyncCounter*) is a reference or a raw pointer: it borrows its referent instead of keeping it alive — hold the object, or a std::shared_ptr to it". That reason is correct, and the advice is correct and safe — reference_wrapper does borrow. Only the path spells a private libstdc++ member, which is cosmetic. Calling it "worse" inverts the facts; it should be dropped from the finding.

THE PROPOSED FIX. Mechanically sound — I compiled the marker + `trait_value(^^trait_is_specialized, inner)` gate and it works (probe_refute_descend_fix.cpp), and it changes no trait value so no static_assert in tests/ moves. I also confirmed there is no free reflective alternative: `template_of(substitute(^^is_sendable, {^^shared_ptr<int>}))` equals `template_of(substitute(^^is_sendable, {^^int}))`, so reflection cannot tell a partial specialization from the primary. But the fix is only half a fix and reads badly for a conference: it replaces a misleading cause with no cause, printing "read that specialization, not its members" instead of naming `int`. It also introduces a hand-maintained registry parallel to every specialization, which cuts against CLAUDE.md's simplicity requirement and against the whole reason the traits are read reflectively (a user's own specialize-to-false type stays ungated and hits the same bug).

SEVERITY. I would rate this medium, not high: no trait answer is wrong and nothing in the suite is affected; the harm is confined to one misleading compile error that a user can act on unsoundly. It is real, and it lands on a plausible demo line, but it is a diagnostics defect, not a soundness or usability hole in the traits.

## F08 — The unreflectable-state message advises a specialization that cannot be written for a block-scope closure, and never names the one-line workaround (message wording; the rejection itself is intended and sound)

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | probable |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/utils.h:103` |

### Le problème

has_unreflectable_state makes every closure with any capture opaque, so the most idiomatic safe task in the language — `launcher.launch_task([counter = std::make_shared<std::atomic<int>>()] { counter->fetch_add(1); })` — is refused. The refusal is sound (the walk genuinely cannot see the captures), but the message says "specialize is_sendable to state the intent", and a closure type declared inside a function body can never be named by a namespace-scope explicit specialization: `template <> struct threadsafe::is_sendable<decltype(body)>` at block scope is "a template declaration cannot appear at block scope". The advice is unfollowable exactly where it is given. A workaround does exist (move the captures into arguments — verified compiling), so the message should name it.

### Le code concerné

```cpp
if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); specialize is_sendable to state the intent",
               path);
```

### La correction

```cpp
Keep each trait's message generic and layered — do not put launch_task advice in sendable.h. In all three sites (sendable.h:154, synchronizable.h:165, lifetime_aware.h:170), replace only the tail clause, keeping it short:

    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); give the state a named type whose members the "
               u8"traits can read, or specialize is_sendable — a lambda "
               u8"written inside a function body cannot be named by a "
               u8"specialization",
               path);

(and the same tail with is_synchronizable / is_lifetime_aware in the other two files).

Then put the launcher-specific remedy where the launcher is actually known, in detail::explain_launch_task / explain_launch_scoped_task in asynchronous_task_launcher.h — e.g. catch the meta::exception from assert_sendable<F>() and rethrow with ", or pass what the callable captures as task arguments instead" appended, or simply document it in the launcher's comment block. That keeps the trait diagnostics reusable and the advice accurate at each layer.

Given severity, doing nothing is also defensible: the behavior is correct, tested, and the workaround costs one line.
```

### Reproduction

```text
// probe_launcher_escape_hatch.cpp
#include <threadsafe/threadsafe.h>
#include <memory>
#include <atomic>
int main() {
    auto counter = std::make_shared<std::atomic<int>>(0);
    auto body = [counter] { counter->fetch_add(1); };
    template <> struct threadsafe::is_sendable<decltype(body)> : std::true_type {};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(body);
}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_launcher_escape_hatch.cpp
error: a template declaration cannot appear at block scope
error: uncaught exception of type 'std::meta::exception'; 'what()': 'main()::<lambda()>
  holds state reflection cannot see (a closure type with captures); specialize
  is_sendable to state the intent'

// The workaround that does compile (probe_launcher_workaround.cpp): WORKAROUND_OK
//   launcher.launch_task([](std::shared_ptr<std::atomic<int>> c){ c->fetch_add(1); }, counter);
```

### Vérification

I tried to refute on three fronts and failed on the substance.

(1) "Every capturing lambda is rejected" — true without exception. GCC 16 exposes no nonstatic data members for closure types, so has_unreflectable_state is true for any non-empty closure. I hunted for a capturing closure that escapes the guard (capture of an empty class by value, hoping is_empty_type would be true) and it is still rejected (probe_refute_lambda_e.cpp).

(2) "The escape hatch cannot be written for a block-scope lambda" — true. A template declaration cannot appear at block scope, and a closure type declared in a function body has no namespace-scope spelling, so `template <> struct threadsafe::is_sendable<decltype(body)>` is unwritable. The only ways out are to hoist the lambda to namespace scope (probe_refute_lambda_c.cpp, compiles, but then it is no longer a block-scope lambda and you must specialize is_lifetime_aware too) or to pass the captures as arguments (probe_refute_lambda_d.cpp, compiles).

(3) The message is the one quoted. So the finding's factual core survives.

What does NOT survive, and must be corrected:

- LOCATION is wrong. utils.h:103 is has_unreflectable_state, a predicate with no message. The quoted CURRENT CODE is sendable.h:154-158. The same sentence is duplicated verbatim at synchronizable.h:165 and lifetime_aware.h:170. Patching only one site leaves three divergent messages for one guard.

- The PROPOSED FIX is defective. It injects launcher-specific advice into a general trait diagnostic. explain_sendable also fires from assert_sendable<T> for any struct holding a closure member, with no launcher in sight; and the identical text in synchronizable.h / lifetime_aware.h would tell a user asking "is this synchronizable?" to restructure a launch_task call. It is also several lines of prose in a codebase whose stated first-class requirement is conference-grade brevity.

- The fix's blanket phrasing "a lambda declared inside a function cannot be named by a specialization" is over-broad for the site it sits at: probe_refute_lambda_g.cpp shows a non-closure type (struct OnlyUnnamedBitfield { int : 8; }) also trips has_unreflectable_state, and for that named type the specialization advice works exactly as written. The message's own parenthetical already hedges with "(a closure type with captures)".

- SEVERITY high overstates it. The rejection is intended, sound, and pinned by tests/test_asynchronous_task_launcher.cpp: static_assert(!can_launch_task<decltype([x = 42] {})>, "launch_task — a capturing lambda is not a safe callable"). Nothing unsafe is accepted and nothing safe is unreachable — the workaround is one line. This is an error-message wording defect, low-to-medium, not a high usability hole.

Neither the current message nor the proposed one is matched by any test (test_diagnostics.cpp only exercises the agreeing half), so a reworded message breaks no test.

## F09 — assert_* prints a fabricated reason and actively wrong advice whenever the false answer came from a partial specialization: descend_* always re-runs the primary structural walk, so std::vector/optional/pair/tuple/unique_ptr/shared_ptr/copy_on_write all report "has a user-written copy, move or destructor ... specialize is_sendable" instead of naming the offending element — and descend_lifetime_aware instead exposes libstdc++'s _Vector_impl_data::_M_start

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/sendable.h:65` |
| **Même défaut que** | `F05`, `F06`, `F07` — les diagnostics re-dérivés par la marche structurelle |

### Le problème

descend_sendable always re-runs diagnose_default_is_sendable — the *primary structural walk* — even when the trait's false answer actually came from a partial specialization (the std_wrapper rules, the smart-pointer rules, copy_on_write). For std::vector<T> the structural walk trips on vector's constructor templates and reports "has a user-written copy, move or destructor ... specialize is_sendable", which is not the reason and is terrible advice. The true reason (an element member holds a raw pointer) is never printed. std::vector is the most common type a user will hold, so this is the message the audience will actually see. The same defect is duplicated in descend_lifetime_aware (lifetime_aware.h:91) and descend_const_synchronizable (synchronizable.h:75).

### Le code concerné

```cpp
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);

    reject(inner,
           u8"is not sendable: is_sendable is specialized to false for it",
           path);
}
```

### La correction

```cpp
Two changes, in this order.

(a) Fix the ordering with zero new machinery. descend_sendable / descend_lifetime_aware / descend_const_synchronizable are already forward-declared at the top of their headers (sendable.h:16, lifetime_aware.h:18, synchronizable.h:28) and are only ever *called* from explain_*. So move only their three definitions into a new details/diagnose_specializations.h, included last from threadsafe.h after allowed_std_wrappers.h, smart_pointers.h and copy_on_write.h. No helper moves, no weak hook. I built exactly this and it compiles.

(b) In each moved definition, dispatch to the specialization family before falling back to the structural walk, asking the trait that family's rule actually asks:

  [[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                      const std::u8string &path) {
      if (is_allowed_std_wrapper(inner))
          for (std::meta::info wrapped : wrapped_types_of(inner))
              if (!is_sendable_type(wrapped))
                  descend_sendable(wrapped, path + u8"::<element "
                                                + type_name(wrapped) + u8">");
      diagnose_default_is_sendable(inner, path);
      reject(inner, u8"is not sendable: is_sendable is specialized to false for it", path);
  }

The report stops here, which leaves the smart pointers and copy_on_write still fabricating — I confirmed both still print the bogus text under this partial patch. Add the remaining families, minding that each asks a *different* trait of its argument:
  - unique_ptr<T,D>: descend_sendable into T and into D, and report the dynamic_type_is_known failure separately ("T is polymorphic; the dynamic type may add state the walk never saw").
  - shared_ptr<T> / weak_ptr<T> / reference_wrapper<T>: the rule is is_synchronizable<T>, not is_sendable<T>, so descend with descend_const_synchronizable and word it as "shares its pointee, which must be synchronizable". A generic "walk the template arguments with the same trait" shortcut is wrong here and would still fabricate for shared_ptr<int>.
  - copy_on_write<T>: cow_is_sendable asks is_sendable<T> then is_synchronizable<const T>; descend into whichever failed.

Do the same in descend_lifetime_aware (std_wrapper_is_lifetime_aware, unique_ptr, shared_ptr) and descend_const_synchronizable (std_wrapper_is_const_synchronizable, the const smart-pointer rules).
```

### Reproduction

```text
// probe_api_assert_stdwrap.cpp
#include <threadsafe/threadsafe.h>
#include <vector>
namespace app { struct Leaf { int* borrowed; }; }
static_assert((threadsafe::assert_sendable<std::vector<app::Leaf>>(), true));

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_assert_stdwrap.cpp
'what()': 'std::vector<app::Leaf> has a user-written copy, move or destructor — or a
 template that may be selected as one — which can share state the members do not show;
 specialize is_sendable to state the intent'

// identical bogus text observed for std::optional<app::Leaf>, std::unique_ptr<app::Leaf>,
// std::pair<int, app::Leaf>, std::shared_ptr<app::Leaf>, threadsafe::copy_on_write<Config>.
// For contrast, the flat user specialization path is correct:
template <> struct threadsafe::is_sendable<app::Handle> : std::false_type {};
struct Wrapper { app::Handle handle; };
assert_sendable<Wrapper>()  ->  'app::Wrapper::handle (app::Handle) is not sendable:
                                 is_sendable is specialized to false for it'  (good)
```

### Vérification

I tried to refute on the reproduction axis and failed — every claimed message reproduced verbatim.

1. Location is accurate. `descend_sendable` is defined at include/threadsafe/details/sendable.h:65 and its body is exactly the CURRENT CODE quoted. The two siblings are at lifetime_aware.h:92 and synchronizable.h:76 (the report said 91 and 75 — off by one, cosmetic).

2. Mechanism is real. `assert_sendable<T>` seeds a path and calls `descend_sendable`, which unconditionally runs `diagnose_default_is_sendable` — the primary structural walk. But the false answer for std::vector<Leaf> came from the constrained partial specialization `is_sendable<detail::std_wrapper T>` in allowed_std_wrappers.h, whose rule is `std_wrapper_is_sendable` ("some wrapped type is not sendable"). The structural walk never sees that rule; it trips on vector's constructor templates at the `has_only_default_copy_move_destroy` check and prints an unrelated reason plus actively wrong advice.

3. I reproduced the exact quoted text for std::vector<app::Leaf>, std::optional<app::Leaf>, std::pair<int, app::Leaf>, std::unique_ptr<app::Leaf>, std::shared_ptr<app::Leaf>, threadsafe::copy_on_write<app::Leaf>, and additionally std::tuple<app::Leaf>. The realistic conference case — `struct App { std::vector<Leaf> items; }` — prints `app::App::items (std::vector<app::Leaf>) has a user-written copy, move or destructor ... specialize is_sendable to state the intent`: it correctly names the member, then fabricates the reason and tells the user to specialize is_sendable for std::vector<app::Leaf>. The actual defect (Leaf::borrowed is a raw pointer) is never printed.

4. The contrast case is also as claimed: a flat user specialization gives the correct `app::Wrapper::handle (app::Handle) is not sendable: is_sendable is specialized to false for it`.

5. std::array is the one exception the report did not mention: it is a std_wrapper but has no constructor templates, so the structural walk succeeds and reaches `_M_elems ... ::borrowed (int*)` with the right reason. That narrows nothing — every other wrapper fails.

6. The report actually understates one sibling. `descend_lifetime_aware` does not merely fabricate: for `std::vector<std::string_view>` the structural walk succeeds and drags the audience through libstdc++ internals — `...::(base std::_Vector_base<...>::_Vector_impl_data)::_M_start (std::basic_string_view<char>*) is a reference or a raw pointer` — the exact leak that `std_wrapper_is_lifetime_aware` exists to avoid. That is a worse slide than a wrong sentence.

7. Severity: the walk is only reached from assert_* (the trait itself leaves `path` empty and short-circuits in `explain_*`), so this is diagnostics-only — no soundness impact, and the boolean answers are all correct. But assert_* is the diagnostic face of the library and std::vector is the type an audience will type first, so "high" on the usability axis is defensible for an educational library.

On the proposed fix: shape is right but it does not compile as written and does not cover what its own title claims. `descend_sendable` is a non-template inline function, so `detail::is_allowed_std_wrapper` must be declared before its body is parsed, and allowed_std_wrappers.h includes sendable.h. The report concedes this and proposes moving the helpers or adding a weak hook — both heavier than needed, since only the *definition* of descend_sendable needs to move (it is already forward-declared at sendable.h:16). Second gap: the proposed body only handles std wrappers, so unique_ptr, shared_ptr and copy_on_write keep fabricating; I confirmed that under the patched tree.

## F10 — THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE also grants Send by value (sendable.h:127, allowed_std_wrappers.h:80), so a Sync-but-not-Send type is unsafe by default; the opt-out (`is_sendable<T> : std::false_type`) works but is undocumented

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | probable |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/sendable.h:127` |

### Le problème

`diagnose_default_is_sendable` returns immediately when `is_synchronizable_type(type)` holds, and `std_wrapper_is_sendable` (allowed_std_wrappers.h:80) does the same. That bakes Sync ⇒ Send into the model, but the Rust model the library cites keeps the two independent precisely because Sync-but-not-Send is a real and common category: a handle that internally locks so it is safe to *use* from several threads, yet is thread-affine and must be released (or used) on its creating thread — Rust's `MutexGuard`, a pthread mutex owner, a GPU/driver context, a COM apartment-threaded object. THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE is the only vouching mechanism the library offers, and using it to state the true, narrow claim ("shareable by reference") silently also states the false one ("movable between threads"). There is no `is_sendable<T> : std::false_type` escape either: writing it makes the two answers contradict, since the trait never consults it once the Sync short-circuit fires — and even if it did, the user would have to know to write it.

### Le code concerné

```cpp
if (is_synchronizable_type(type) || is_scalar_type(type))
        return;
```

### La correction

```cpp
Do NOT delete the short-circuit as proposed — it breaks four test files (function types, atomic<shared_ptr<T>>, synchronized_value, and the deferred-specialization demo) and the fix names no replacement for function types or for user-vouched types.

Two viable options, cheapest first:

A. Documentation + a companion vouch, keeping the implication (minimal, fits the educational goal):
   - In CLAUDE.md and next to the macro, state plainly that vouching Sync in this library also vouches Send by value, because is_synchronizable is read as "usable from any thread, concurrently — including destruction".
   - Document the opt-out that already works: `template <> struct threadsafe::is_sendable<T> : std::false_type {};` for a thread-affine type. Verified: it yields !is_sendable_v<T>, keeps is_sendable_v<T&> true, and propagates through std::vector<T>, std::optional<T> and struct members.
   - Optionally add the symmetric macro so the pair reads as a pair:
       #define THREADSAFE_UNSAFE_ASSERT_THREAD_AFFINE(...) \
           template <> struct threadsafe::is_sendable<__VA_ARGS__> : std::false_type {}

B. Separating the two opt-ins (the finding's intent), done completely:
   - Remove the short-circuit from both sites, AND add:
       template <function_type F> struct is_sendable<F> : std::true_type {};
       template <class T> struct is_sendable<std::atomic<T>> : is_sendable<T> {};
       template <class T> struct is_sendable<synchronized_value<T>> : is_sendable<T> {};
       #define THREADSAFE_UNSAFE_ASSERT_SENDABLE(...) \
           template <> struct threadsafe::is_sendable<__VA_ARGS__> : std::true_type {}
   - Update tests/test_sendable.cpp:144, tests/test_containers.cpp:159, tests/test_synchronized_value.cpp:46,145, tests/test_smart_pointers.cpp:94 and tests/test_deferred_specialization.cpp:43,47 (the last needs the new SENDABLE vouch on Opaque alongside the SYNCHRONIZABLE one).
   Note the proposed `is_sendable<std::atomic<T>>` is redundant for atomic<int> (the structural walk already accepts it, verified) but required for atomic<shared_ptr<T>>.
```

### Reproduction

```text
// probe_soundness_syncsend.cpp
#include <threadsafe/threadsafe.h>
#include <mutex>
#include <vector>
using namespace threadsafe;

// A handle that is safe to USE from several threads (it locks internally) but
// is thread-affine: the driver requires release on the creating thread.
class gpu_context {
public:
    gpu_context();
    ~gpu_context();                       // must run on the creating thread
    gpu_context(gpu_context&&) noexcept;
    void submit(int command) const;
private:
    mutable std::mutex queue_mutex_;
    std::vector<int> queue_;
};

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(gpu_context);

static_assert(is_synchronizable_v<gpu_context>);
static_assert(is_sendable_v<gpu_context>,      "HOLE: Sync silently implies Send");
static_assert(is_lifetime_aware_v<gpu_context>);

int main() {
    asynchronous_task_launcher launcher;
    gpu_context context;
    launcher.launch_task([](gpu_context owned) { owned.submit(1); },
                         std::move(context));   // ACCEPTED: destroyed on the worker
}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_soundness_syncsend.cpp
$ echo $?
0

And the companion probe showing the fix is viable:

static_assert(threadsafe::detail::default_is_sendable(^^std::atomic<int>));   // passes
static_assert(threadsafe::detail::default_is_sendable(^^std::atomic<long*>)); // fails, as it should
```

### Vérification

The mechanism reproduces exactly as described: sendable.h:127 returns early on is_synchronizable_type(type), ahead of the has_only_default_copy_move_destroy and has_unreflectable_state guards, so THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE on a thread-affine type also makes it sendable by value and launch_task accepts std::move of it. allowed_std_wrappers.h:80 repeats it for containers. The concern is legitimate for a library that explicitly cites Rust's model and implements Sync correctly at the reference level (is_sendable<T&> = is_synchronizable<T>) while adding a value-level implication Rust deliberately omits; the library's own value_guard needed a hand-written is_sendable : false_type because of exactly this category.

However two load-bearing claims in the finding are false, so it must be restated and its fix replaced.

(1) "There is no is_sendable<T> : std::false_type escape — the trait never consults it once the Sync short-circuit fires." Refuted. is_sendable_type goes through detail::trait_value(^^is_sendable_v, type), which resolves user specializations; the short-circuit only exists inside default_is_sendable, which is reached from the primary template. A user specialization to false_type works and propagates correctly through vector, optional and structural members, while is_sendable_v<T&> stays true (the correct answer for sharing). So the thread-affine type IS expressible; it needs a second, undocumented specialization. That downgrades the finding from "soundness hole with no workaround" to "unsafe default plus a documentation gap".

(2) The proposed fix is incomplete and regressive. I applied it verbatim to a copy of the include tree and compiled all 11 test files: four break. test_sendable.cpp:232 loses function types entirely (default_is_sendable rejects them as "not a scalar, class or union" — the fix names no replacement rule). test_smart_pointers.cpp:94 (atomic<shared_ptr<SyncType>>), test_synchronized_value.cpp:46 and :145, and test_deferred_specialization.cpp:43/47 — the library's flagship deferred-specialization demo, where Opaque{int* borrowed} is vouched Sync so Holder is sendable, a *sound* use of the implication that the fix kills with no replacement. The finding's own companion probe also cuts against it: default_is_sendable(^^std::atomic<int>) is already true structurally with and without the fix, so the proposed atomic specialization is redundant there while being required for atomic<shared_ptr<T>>, which the finding missed.

Reported real=true because the over-grant is genuine and demonstrable, but the corrected title and fix below are what should actually be acted on.

## F11 — The mandated rebind constructor template makes every conforming custom allocator non-sendable, so allow-listed containers parameterized on one answer false for is_sendable and is_synchronizable<const> while is_lifetime_aware answers true — and the diagnostic blames the container's own constructor templates instead of the allocator

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/allowed_std_wrappers.h:79` |

### Le problème

std_wrapper_is_sendable / std_wrapper_is_const_synchronizable ask the trait about every *type* argument, and for a container that includes the Allocator. [allocator.requirements] requires that `A a(b)` compile for a rebound allocator, i.e. every conforming allocator carries `template <class U> Alloc(const Alloc<U>&)`. may_hijack_copy_move flags any constructor template, so has_only_default_copy_move_destroy rejects the allocator, and the container inherits that `false`. std::allocator escapes only because vocabulary.h hard-codes it; std::pmr::polymorphic_allocator does not, so std::pmr::vector, std::pmr::string and every other pmr alias answer false for is_sendable and is_synchronizable<const T>. This is not the closed-list problem — std::vector *is* on the list. Worse, is_lifetime_aware does not run the copy/move guard, so the three traits disagree about one type: is_sendable_v<V> is false while is_lifetime_aware_v<V> is true, and launch_task then rejects a container the ownership trait vouched for. The rejection message names the container, never the allocator.

### Le code concerné

```cpp
inline consteval bool std_wrapper_is_sendable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return true;

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_sendable_type(wrapped))
            return false;

    return true;
}
```

### La correction

```cpp
In include/threadsafe/details/utils.h, suppress ONLY the may_hijack_copy_move rejection for an empty type — a type with no state has nothing for a hijacked copy to share — while keeping the defaulted/deleted requirement on real special members, so EmptyUserCopy still answers false:

inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    // An empty class holds no state, so a constructor template that may be
    // selected as a copy has nothing to share through it. A user-*written*
    // copy, move or destructor is still rejected: it runs on the receiving
    // thread whether or not the object carries members.
    const bool empty = std::meta::is_empty_type(type);
    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (!empty && may_hijack_copy_move(member))
            return false;

        if (!is_copy_move_destroy_member(member))
            continue;

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return false;
    }
    return true;
}

DROP the proposed vocabulary.h blessing of std::pmr::polymorphic_allocator. polymorphic_allocator is not empty and stays false, which is the correct answer: monotonic_buffer_resource and unsynchronized_pool_resource are not thread-safe, so sending a pmr container would share an unsynchronized resource. This also keeps the existing test_soundness_regressions.cpp:105 assertion (!is_lifetime_aware_v<std::pmr::vector<int>>, "a pmr container borrows its memory_resource") coherent with is_sendable.

Independently of which line is chosen, two things still need doing: (a) add a conforming custom allocator to tests/test_containers.cpp — the existing BadAlloc deliberately omits the rebind constructor, so nothing in the suite covers this; (b) fix assert_sendable's message for allow-listed wrappers, which currently re-enters the structural walk on std::vector and reports libstdc++'s own constructor templates rather than the template argument that actually failed.
```

### Reproduction

```text
// probe_reflayer_alloc.cpp
#include <threadsafe/threadsafe.h>
#include <vector>
#include <string>
#include <memory_resource>
#include <cstddef>

// The canonical minimal conforming allocator ([allocator.requirements]):
// the rebinding converting constructor template is REQUIRED.
template <class T>
struct minimal_allocator {
    using value_type = T;
    minimal_allocator() = default;
    template <class U> minimal_allocator(const minimal_allocator<U>&) noexcept {}
    T* allocate(std::size_t n) { return static_cast<T*>(::operator new(n * sizeof(T))); }
    void deallocate(T* p, std::size_t) noexcept { ::operator delete(p); }
    bool operator==(const minimal_allocator&) const = default;
};

using V = std::vector<int, minimal_allocator<int>>;
using S = std::basic_string<char, std::char_traits<char>, minimal_allocator<char>>;

static_assert(!threadsafe::is_sendable_v<V>, "SENDABLE(vector) IS TRUE");
static_assert(!threadsafe::is_sendable_v<S>, "SENDABLE(string) IS TRUE");
static_assert(!threadsafe::is_lifetime_aware_v<V>, "LIFETIME(vector) IS TRUE");
static_assert(!threadsafe::is_synchronizable_v<const V>, "CONSTSYNC(vector) IS TRUE");
static_assert(!threadsafe::is_sendable_v<std::pmr::vector<int>>, "SENDABLE(pmr::vector) IS TRUE");
static_assert(!threadsafe::is_sendable_v<std::pmr::string>, "SENDABLE(pmr::string) IS TRUE");
static_assert(threadsafe::is_sendable_v<std::vector<int>>, "baseline broken");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_reflayer_alloc.cpp
probe_reflayer_alloc.cpp:24:15: error: static assertion failed: LIFETIME(vector<int,minimal_allocator>) IS TRUE
   24 | static_assert(!threadsafe::is_lifetime_aware_v<V>, ...

// Only the lifetime assertion fires: is_sendable and is_synchronizable<const V>
// are both FALSE (so is their pmr equivalents), while is_lifetime_aware is TRUE.

// And the diagnostic never mentions the allocator:
// probe_reflayer_alloc_msg.cpp
#include <threadsafe/threadsafe.h>
constexpr int forced = (threadsafe::assert_sendable<V>(), 0);

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_reflayer_alloc_msg.cpp
probe_reflayer_alloc_msg.cpp:16:60: error: uncaught exception of type 'std::meta::exception'; 'what()':
 'std::vector<int, minimal_allocator<int> > has a user-written copy, move or destructor
  — or a template that may be selected as one — which can share state the members do not
  show; specialize is_sendable to state the intent'

// Fix validated (probe_reflayer_fixes.cpp, compiles clean):
static_assert(!threadsafe::detail::has_only_default_copy_move_destroy(^^minimal_allocator<int>));
static_assert(proposed_has_only_default_copy_move_destroy(^^minimal_allocator<int>));
struct StatefulHijacker { int *shared; template <class U> StatefulHijacker(U&&) {} };
static_assert(!proposed_has_only_default_copy_move_destroy(^^StatefulHijacker));
```

### Vérification

The core claim reproduces exactly, and I could not refute it.

WHAT I CONFIRMED (probe_refute_alloc_rebind.cpp, compiled clean with every assertion in the *negated* direction, i.e. all of the bad answers hold):
- `is_sendable_v<minimal_allocator<int>>` is FALSE. The allocator is empty and stateless; the only thing that sinks it is the `[allocator.requirements]`-mandated rebinding constructor template `template <class U> minimal_allocator(const minimal_allocator<U>&)`, which `may_hijack_copy_move` (utils.h:135) flags unconditionally, so `has_only_default_copy_move_destroy` returns false and `diagnose_default_is_sendable` rejects.
- That false propagates through `std_wrapper_is_sendable` / `std_wrapper_is_const_synchronizable` (allowed_std_wrappers.h:79 / :95), because `wrapped_types_of` returns every *type* argument including the Allocator. So `is_sendable_v<std::vector<int, minimal_allocator<int>>>`, `is_sendable_v<basic_string<char, char_traits<char>, minimal_allocator<char>>>` and `is_synchronizable_v<const V>` are all FALSE while `is_sendable_v<std::vector<int>>` is TRUE (std::allocator escapes only because vocabulary.h:15 hard-codes it).
- The trait disagreement is real: `is_lifetime_aware_v<V>` is TRUE (the lifetime rule at allowed_std_wrappers.h:110 never runs the copy/move guard) while `is_sendable_v<V>` is FALSE. One type, two traits, opposite answers.
- The diagnostic is confirmed and is worse than the finding says. `assert_sendable<V>()` prints "std::vector<int, minimal_allocator<int> > has a user-written copy, move or destructor — or a template that may be selected as one". It never names the allocator, AND the reason is wrong: assert_sendable re-enters the *structural* walk on std::vector, so the message reports libstdc++'s own vector constructor templates, not the actual culprit the wrapper rule tripped on.
- This is genuinely not the closed-list problem: std::vector is on the list. Nor is it a contrived allocator — the test suite's own `BadAlloc` (test_containers.cpp:12) deliberately omits the rebind constructor and is rejected for the right reason (a `UserCopyCtor` member), so no test in the repo ever exercises a *conforming* custom allocator. The gap is untested.

WHERE THE FINDING IS WRONG, AND WHY I CORRECTED IT:
1. The proposed fix is not viable as written. I copied the header tree, applied the exact `if (std::meta::is_empty_type(type)) return true;` early-out at the top of `has_only_default_copy_move_destroy`, and compiled all 12 test files: 8 of them FAIL. It directly contradicts an explicit soundness-regression test, test_soundness_regressions.cpp:164 — `static_assert(!is_sendable_v<EmptyUserCopy>, "empty is not enough: the copy launch_task makes onto the thread runs a user-provided constructor there")` — plus `UserCopyCtor`, `UserCopy`, `BadDeleter`, `NonSendable` in six other files. Blanket-trusting empty types retires a guard the library retired `is_safe_callable` to install.
2. The pmr half of the claim is factually true but is not the hole it is presented as. `std::pmr::vector<int>` and `std::pmr::string` do answer false — but that is arguably the *right* answer, and the library already commits to it elsewhere: test_soundness_regressions.cpp:105 asserts `!is_lifetime_aware_v<std::pmr::vector<int>>` with the comment "a pmr container borrows its memory_resource". Adding the proposed `is_sendable<std::pmr::polymorphic_allocator<T>> : std::true_type` to vocabulary.h would be a soundness REGRESSION, not a fix: `monotonic_buffer_resource` and `unsynchronized_pool_resource` are explicitly not thread-safe, so sending a pmr container hands another thread a resource the sending thread may still be allocating from. That part of the fix must be dropped.

CORRECTED FIX, TESTED: narrow the relaxation so it suppresses only the `may_hijack_copy_move` rejection on an empty type, leaving the defaulted/deleted check on real special members intact. With that patch all 12 test files compile clean (EmptyUserCopy still correctly answers false, since its copy constructor is user-provided and non-template) and `is_sendable_v<V>` / `is_synchronizable_v<const V>` both become TRUE. pmr stays false, as it should.

Caveat I want on the record, since this is educational code: the narrowed line is a judgement call, not a theorem. The stated rationale for `may_hijack_copy_move` is "can share state the members do not show", and an empty type has no state shown or hidden — that is what justifies the exemption. But EmptyUserCopy is rejected on a *different* rationale ("runs a user-provided constructor there"), and by that second rationale a hijacking template on an empty type should be rejected too. If the author prefers to keep the two rationales aligned, the alternative resolution is to leave the guard alone and instead document — in allowed_std_wrappers.h, next to the "constructor templates block the structural default anyway" comment, which is precisely the sentence that hides this — that any conforming custom allocator, comparator or hasher requires an explicit `is_sendable` / `is_synchronizable<const>` specialization, and to fix `assert_sendable` so the message names the offending template argument. Either way the current state is a defect: the comment asserts the constructor-template block is harmless for wrapper arguments, and it is not.

## F12 — has_unreflectable_state also flags two non-closure shapes — an empty union and a struct whose only members are unnamed bit-fields — and the rejection message asserts they are "a closure type with captures", which is false

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/utils.h:103` |
| **Même défaut que** | `F21` — has_unreflectable_state et les champs de bits |

### Le problème

The heuristic infers "unreflectable state" from `!is_empty_type && no bases && no reflectable members`, which is meant to catch closure types (GCC 16 really does report zero `nonstatic_data_members_of` for a capturing lambda -- I verified that). But two ordinary language constructs land in the same hole: a class whose only member is an *unnamed* bit-field (`unsigned : 3;`) is not an empty type yet reports no data members, and a union with no members is never an empty type either. Both are trivially safe to read from several threads and both are rejected. The emitted diagnostic asserts something factually false about the user's type, which is the worst possible outcome for a library whose selling point is that its `false` answers explain themselves.

### Le code concerné

```cpp
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}
```

### La correction

The finding's primary fix does not compile: std::meta::is_closure_type does not exist in GCC 16. Its fallback is also incomplete — adding !is_union_type cures the empty union but leaves the unnamed-bit-field struct rejected, and no predicate on this toolchain can cure that one (an unnamed bit-field appears in neither nonstatic_data_members_of nor members_of, and has_identifier is true for closure types so it cannot discriminate either).

The fix I would actually apply, in include/threadsafe/details/utils.h:

// A type that occupies storage no reflectable member accounts for. The case
// this exists for is the capturing closure: GCC reports zero data members for
// it whatever it captures. A union has no such blind spot, so it is excluded.
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return !std::meta::is_empty_type(type)
        && !std::meta::is_union_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}

and reword the three rejection sites (synchronizable.h:165, sendable.h:154, lifetime_aware.h:170) from "(a closure type with captures)" to something the library can actually prove, e.g. "occupies storage no data member accounts for — typically a closure type with captures". No test pins the message text (only comments mention closures), and I verified all 11 test files still compile with the union exclusion applied.

The residual unnamed-bit-field-only struct stays rejected, but with a message that is true rather than one that misinforms the audience.

### Reproduction

```text
$ cat probe_sync_statics.cpp   (excerpt)
struct UnnamedBitfield { unsigned : 3; };
static_assert(is_synchronizable_v<const UnnamedBitfield>, "unnamed bit-field rejected");
union EmptyUnion {};
static_assert(is_synchronizable_v<const EmptyUnion>, "empty union rejected");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_sync_statics.cpp
probe_sync_statics.cpp:20:15: error: static assertion failed: unnamed bit-field rejected
probe_sync_statics.cpp:22:15: error: static assertion failed: empty union rejected

The message (probe_sync_diag_bitfield.cpp / probe_sync_diag_union.cpp):
  what()': 'const OnlyPad holds state reflection cannot see (a closure type with captures);
            specialize is_synchronizable to state the intent'
  what()': 'const EmptyUnion holds state reflection cannot see (a closure type with captures);
            specialize is_synchronizable to state the intent'

For contrast, the heuristic's real target is confirmed necessary on this toolchain
(probe_sync_closure_reflect.cpp):
    auto closure = [n = local, &local]{ return n + local; };
    static_assert(nonstatic_data_members_of(^^decltype(closure), unchecked()).size() == 0, ...);  // passes
    static_assert(std::meta::is_empty_type(^^decltype(closure)), "closure is NOT empty");
-> error: static assertion failed: closure is NOT empty
```

### Vérification

I tried to refute on three axes and failed on the core claim, but the proposed fix does not survive.

(1) Technical truth — confirmed by compilation. `struct UnnamedBitfield { unsigned : 3; };` yields is_empty_type == false, bases_of == {}, nonstatic_data_members_of == {} on GCC 16, so has_unreflectable_state(utils.h:103) returns true and `is_synchronizable_v<const UnnamedBitfield>` is false. `union EmptyUnion {};` takes the identical path (a union is never an empty type). Both are trivially safe to read concurrently, so both are genuine false negatives. The escaping std::meta::exception literally reads "holds state reflection cannot see (a closure type with captures)", which is a false statement about both types.

(2) Is the heuristic's target real? Yes — a capturing lambda reports zero members from BOTH nonstatic_data_members_of and members_of on this toolchain, and is not empty. So the function cannot simply be deleted; the finding correctly frames this as a misfire, not as dead code.

(3) Scope — narrower than the severity suggests. I enumerated adjacent constructs and all of them pass: named bit-fields, zero-width unnamed bit-fields (still empty), mixed named+unnamed bit-field register structs, structs holding an anonymous union, unions with members, captureless lambdas. Exactly three shapes reach has_unreflectable_state == true: capturing closures (intended), empty unions, and structs whose only members are unnamed non-zero-width bit-fields. Both misfires are exotic. That argues for low/medium rather than medium, but it does not refute the claim, and the misleading wording is a fair hit against a library whose stated selling point is that its `false` answers explain themselves.

(4) The proposed fix does NOT survive. `std::meta::is_closure_type` does not exist in GCC 16 (hard compile error, suggests is_class_type), so the primary fix will not build. The fallback branch claims to exclude "the two constructs it misreads" but its code adds only `!is_union_type`; I applied exactly that patch to a copied tree and confirmed all 11 test files still compile and EmptyUnion becomes accepted by is_synchronizable/is_sendable/is_lifetime_aware — while UnnamedBitfield stays rejected. An unnamed bit-field is invisible to reflection on GCC 16 (absent from members_of as well), and has_identifier cannot separate a closure either (GCC gives closure types an identifier), so no available predicate distinguishes the bit-field struct from a capturing closure. That half of the fix is not implementable here; only the diagnostic wording can be corrected.

Verdict: real=true, with a corrected (union-exclusion + honest wording) fix and severity trimmed toward low/medium.

## F13 — A value-held self-reference (`std::vector<Self>`, `unique_ptr<const Self>`) makes `is_synchronizable<const T>` / `is_sendable<T>` a non-recoverable template cycle error instead of an answer

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/synchronizable.h:38` |
| **Même défaut que** | `F02` — les types récursifs propriétaires |

### Le problème

The structural walk asks `is_synchronizable_v<const Member>` from inside the instantiation of `is_synchronizable<const T>`. When a member reaches back to T — a tree, an AST, a JSON value, the most common shape in a conference demo — the walk re-enters the specialization currently being instantiated and the compiler emits `'value' is not a member of threadsafe::is_synchronizable<const Tree>`, with no mention of the trait's vocabulary or of any escape. `assert_synchronizable` cannot help: the failure happens before any `std::meta::exception` is thrown, so the whole diagnostic machinery is bypassed. An escape does exist — an explicit `is_synchronizable<const Tree>` specialization short-circuits the cycle and compiles — but nothing in the message points to it. `is_sendable` has the identical cycle.

### Le code concerné

```cpp
template <class T>
struct is_synchronizable<const T>
    : std::bool_constant<detail::default_is_const_synchronizable(^^T)> {};
```

### La correction

No cheap code fix exists, and the honest fix is documentation plus a pinned test.

A true coinductive fix (Rust's approach: assume `true` for a type already on the walk stack) would require threading an in-progress set through every reflective hop — `diagnose_default_is_const_synchronizable`, `trait_value`/`is_synchronizable_type`, and `std_wrapper_is_const_synchronizable` — because each hop currently re-enters the trait variable and loses all context. That is a large refactor and directly hostile to CLAUDE.md's "simplicity and readability are first-class" mandate for a conference library.

What I would actually apply:
1. CLAUDE.md, under `is_synchronizable<const T>`: state that the structural walk is inductive and cannot close a value-held cycle — a member that reaches back to `T` by value re-enters the specialization being instantiated and the compiler reports "used in its own initializer". Name the escape explicitly: `template <> struct threadsafe::is_synchronizable<const Tree> : std::true_type {};` (or `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Tree)`), and note that self-reference *through a pointer* terminates on its own.
2. tests/test_synchronizable.cpp: add the recipe next to the existing `Node`/`PList` cases so the demo shows both the terminating and the specialized shapes, e.g.
   struct ValueTree { int value; std::vector<ValueTree> children; };
   template <> struct threadsafe::is_synchronizable<const ValueTree> : std::true_type {};
   static_assert(is_synchronizable_v<const ValueTree>, "a value-held cycle is closed by hand: the inductive walk cannot");
   I compiled exactly this shape (probe_refute_cycle_escape.cpp) — clean, exit 0 — so it does not disturb the existing suite.
3. Optionally, `smart_pointers.h:50` could apply `remove_cv` to the pointee the way the `shared_ptr` rule already does, which removes the `unique_ptr<const Self>` instance of the cycle. But that changes what the trait *means* for `unique_ptr<const T>` (it would stop asking the const question of the pointee), so it needs the author's intent and is not a drop-in.

### Reproduction

```text
// probe_constsync_recursive.cpp
#include <threadsafe/threadsafe.h>
#include <vector>
struct Tree { int value; std::vector<Tree> children; };
static_assert(threadsafe::is_synchronizable_v<const Tree>);
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_constsync_recursive.cpp
include/threadsafe/details/utils.h:99:36: error: the value of 'threadsafe::is_synchronizable_v<const Tree>' is not usable in a constant expression
include/threadsafe/details/synchronizable_base.h:15:60: error: 'value' is not a member of 'threadsafe::is_synchronizable<const std::vector<Tree> >'
include/threadsafe/details/synchronizable_base.h:15:60: error: 'value' is not a member of 'threadsafe::is_synchronizable<const Tree>'
probe_constsync_recursive.cpp:4:27: error: non-constant condition for static assertion

Same for a self-owning node (probe_constsync_recursive3.cpp):
  struct Node { int value; std::unique_ptr<const Node> next; };
  include/threadsafe/details/smart_pointers.h:50:11: error: the value of 'threadsafe::is_synchronizable_v<const Node>' is not usable in a constant expression

Same for is_sendable (probe_constsync_recursive2.cpp):
  include/threadsafe/details/sendable.h:24:48: error: 'value' is not a member of 'threadsafe::is_sendable<Tree>'

The escape that does work, and that no message names (probe_constsync_recursive4.cpp, compiles clean):
  template <> struct threadsafe::is_synchronizable<const Tree> : std::true_type {};
```

### Vérification

I tried four ways to refute this and all four failed.

1. "It is a documented trade-off" — refuted. CLAUDE.md says nothing about recursive/self-referential types. The test suite *does* show the author thought about recursion, but only about the shapes that terminate: tests/test_synchronizable.cpp:139 (`Node{Node*}`, `PList{const PList*}`), tests/test_smart_pointers.cpp:133 (`Tree{unique_ptr<Tree>}`, `SharedNode{shared_ptr<SharedNode>}`), tests/test_copy_on_write.cpp:105. Every one of those terminates because the pointer rule drops to the *full* opt-in trait (`is_synchronizable<Node>` = primary `false_type`), never re-entering `is_synchronizable<const Node>`. The value-held cycle is a different shape and is nowhere covered or mentioned. Silence is a gap, not a trade-off.

2. "The type is legitimately false, so the error is just noise" — refuted by control probe. `struct Inner{int;}; struct Outer{std::vector<Inner> children;};` and `struct Leaf{int; std::vector<int>;}` both answer TRUE (exit 0). Only the self-reference turns that into a hard error, so the library genuinely loses an answer it would otherwise give, for a type that is in fact safe to read concurrently.

3. "The error is recoverable / SFINAE-friendly, so callers can guard" — refuted. probe_refute_cycle_sfinae.cpp gives the constrained overload a viable unconstrained fallback; the TU still dies. The failure is a constant-expression error inside the variable template's own initializer, outside the immediate context, so nothing can trap it.

4. "assert_synchronizable rescues the diagnostic" — refuted. probe_refute_cycle_assert.cpp emits the identical raw cycle error plus "call to consteval function ... is not a constant expression". The `std::meta::exception` machinery is never reached.

Honest narrowing the finding should carry: the blast radius is *value-held* self-reference only — `std::vector<Self>` / any walked-into container of Self, and `std::unique_ptr<const Self>` (smart_pointers.h:50 asks `is_synchronizable_v<remove_all_extents_t<T>>` without `remove_cv`, so a `const` pointee re-enters the const specialization, unlike the `shared_ptr` rule which does strip cv). `Self*`, `unique_ptr<Self>`, `shared_ptr<Self>` all terminate correctly. That still leaves the single most demo-friendly shape — a tree/AST/JSON node holding `std::vector<Self>` — as a compiler cycle error at a conference.

GCC does say "'is_synchronizable_v<const Tree>' used in its own initializer", which is a real cycle hint, so the finding slightly overstates "no mention of any escape". That is a wording correction, not a refutation.

## F14 — No standard synchronization primitive is blessed synchronizable, so a `mutable std::mutex` / `once_flag` / `atomic_flag` member sinks the const answer — `synchronized_value` covers the guarded-cache case, but not a lock guarding an external sink or a one-time flag

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/synchronizable.h:183` |

### Le problème

The rule itself is right — a mutable member is written through a `const&`, so it must be write-safe. But the library ships exactly one type that ever satisfies `is_synchronizable<T>` out of the box: `std::atomic<T>`. `std::mutex`, `std::shared_mutex`, `std::atomic_flag`, `std::once_flag`, `std::latch`, `std::counting_semaphore` and `std::condition_variable` all answer false, so a `mutable` one of them sinks the const answer for its whole enclosing class. That rules out the canonical C++ const-thread-safe idiom (a mutable lock guarding an external sink, a mutable atomic_flag or once_flag for one-time work) and it is the one place where a Rust comparison is most visible on a slide: `Mutex<T>: Sync` is the first Sync impl anybody quotes.

### Le code concerné

```cpp
if (is_mutable_member(member)) {
            // mutable defeats const: this member is writable through a const&, so it
            // needs the full (write-safe) trait, not the const one.
            if (!is_synchronizable_type(remove_cv(member_type)))
                reject_at(member,
                          u8"is mutable, so it is written through a const "
                          u8"reference: its type must be fully synchronizable",
                          path);
```

### La correction

```cpp
Keep the mutable branch exactly as written — the rule is right. Add the specializations in vocabulary.h, but state the reason and the limit, since this is slide material:

// Every member of these types is race-free by [thread.mutex.requirements.mutex],
// [thread.once.callonce], [thread.latch], [thread.sema], [thread.condition]:
// they exist to be used from several threads at once, which is what the
// unqualified trait asks. What they do NOT do is vouch for the state they
// guard -- a `mutable T cache` next to a `mutable std::mutex` is still
// rejected, because reflection cannot see that the lock covers the cache.
// Use synchronized_value<T> for that.
template <> struct is_synchronizable<std::mutex> : std::true_type {};
template <> struct is_synchronizable<std::recursive_mutex> : std::true_type {};
template <> struct is_synchronizable<std::timed_mutex> : std::true_type {};
template <> struct is_synchronizable<std::recursive_timed_mutex> : std::true_type {};
template <> struct is_synchronizable<std::shared_mutex> : std::true_type {};
template <> struct is_synchronizable<std::shared_timed_mutex> : std::true_type {};
template <> struct is_synchronizable<std::atomic_flag> : std::true_type {};
template <> struct is_synchronizable<std::once_flag> : std::true_type {};
template <> struct is_synchronizable<std::latch> : std::true_type {};
template <> struct is_synchronizable<std::condition_variable> : std::true_type {};
template <> struct is_synchronizable<std::condition_variable_any> : std::true_type {};
template <std::ptrdiff_t LeastMaxValue>
struct is_synchronizable<std::counting_semaphore<LeastMaxValue>> : std::true_type {};
template <class CompletionFunction>
struct is_synchronizable<std::barrier<CompletionFunction>> : std::true_type {};

(The finding's list omits the timed mutexes, condition_variable_any and barrier, which are race-free on the same wording; std::barrier's completion function makes it the one entry worth a second look before shipping.)

Drop the memoization example from the explanation: it does not become legal with this fix. The honest slide is "the primitive is synchronizable; the state it guards is not, and that is why synchronized_value exists."
```

### Reproduction

```text
// probe_constsync_mutable_primitives.cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <latch>
#include <semaphore>
#include <condition_variable>
using namespace threadsafe;
struct OnceFlagged { mutable std::atomic_flag done; int value; };
struct AtomicCached { mutable std::atomic<int> cache; int value; };
struct MutexCached  { mutable std::mutex guard; mutable int cache; int value; };
struct OnceCached   { mutable std::once_flag once; int value; };
static_assert( is_synchronizable_v<const AtomicCached>);   // the one blessed primitive
static_assert(!is_synchronizable_v<const OnceFlagged>);
static_assert(!is_synchronizable_v<const MutexCached>);
static_assert(!is_synchronizable_v<const OnceCached>);
static_assert(!is_synchronizable_v<std::atomic_flag>);
static_assert(!is_synchronizable_v<std::mutex>);
static_assert(!is_synchronizable_v<std::shared_mutex>);
static_assert(!is_synchronizable_v<std::once_flag>);
static_assert(!is_synchronizable_v<std::latch>);
static_assert(!is_synchronizable_v<std::counting_semaphore<4>>);
static_assert(!is_synchronizable_v<std::condition_variable>);
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_constsync_mutable_primitives.cpp
(no output -- every one of them holds)

The message (probe_constsync_diag_mutex.cpp):
  error: uncaught exception of type 'std::meta::exception'; 'what()': 'const MutexCached::guard (std::mutex) is mutable, so it is written through a const reference: its type must be fully synchronizable'

With the two specializations added in a user TU (probe_constsync_fixes.cpp), both
  struct Logger { mutable std::mutex sink_guard; std::string prefix; };
  struct OnceFlagged { mutable std::atomic_flag done; int value; };
become const-synchronizable and compile clean.
```

### Vérification

I re-ran the claimed repro verbatim (probe_refute_mutable_primitives.cpp) and it compiled clean with g++-16 -std=c++26 -freflection: every one of the eleven static_asserts holds, including `!is_synchronizable_v<std::mutex>`, `!is_synchronizable_v<std::once_flag>`, `!is_synchronizable_v<std::atomic_flag>`, `!is_synchronizable_v<std::latch>`, `!is_synchronizable_v<std::counting_semaphore<4>>`, `!is_synchronizable_v<std::condition_variable>`, and the three rejected const wrappers. I also reproduced the exact diagnostic text quoted in the finding. So the reproduction is honest.

I then tried to refute it three ways.

(1) Is the code at the stated location what the finding says? Yes — synchronizable.h:181-189 is the mutable branch, and it calls `is_synchronizable_type(remove_cv(member_type))`, the full trait. Nothing in the walk gives a mutable member a weaker question.

(2) Is the blessed-type inventory as bare as claimed? Not quite. Besides `std::atomic<T>` the library ships `is_synchronizable<F>` for function types, `is_synchronizable<synchronized_value<T>> : is_sendable<T>`, and the `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` escape hatch. That matters, because the library's intended answer to "mutable state guarded by a lock" is `synchronized_value`, and it works today — I verified that `struct Logger { mutable synchronized_value<std::string> sink; int id; };` is const-synchronizable out of the box, no user specialization needed. So the finding's "exactly one type" is an overstatement and the severity framing ("rules out the canonical idiom") is too strong for the guarded-cache case.

(3) Does the proposed fix actually buy what it claims? Partly. Injected into a user TU, the eight specializations do make `struct Logger { mutable std::mutex sink_guard; std::string prefix; }` and `struct OnceFlagged { mutable std::atomic_flag done; int value; }` const-synchronizable. But they do NOT unlock the memoization idiom the explanation leads with: `struct MutexCached { mutable std::mutex guard; mutable int cache; int value; }` is still rejected, because the guarded `mutable int cache` is itself a mutable member demanding the full trait. The reflective walk cannot see that the mutex covers the cache — no per-type specialization list can fix that; only `synchronized_value` (or the UNSAFE macro) can.

I also checked the fix does not regress anything: with the eight specializations force-included ahead of every test, all eleven files in tests/ still compile clean, same as baseline.

Net: the reproduction survives untouched, and there is a real residual gap — a bare `mutable std::mutex` guarding an external sink, and `mutable std::once_flag` / `mutable std::atomic_flag` for one-time work, have no library-supported answer except the UNSAFE macro. Real, but the useful finding is narrower than the write-up.

## F15 — is_lifetime_aware's borrowed_range branch is a false negative for value-generating views (iota_view, empty_view) and changes no verdict the structural member walk does not already reach (usability + redundancy, not soundness)

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/lifetime_aware.h:156` |

### Le problème

`std::ranges::borrowed_range` means "iterators stay valid after the range object dies". That is true both for views over foreign storage (span, string_view, subrange, ref_view) and for ranges that generate values out of thin air and reference nothing at all (iota_view, empty_view). The branch treats the second group as borrowers. `std::views::iota(0, 10)` is an `iota_view<int,int>` holding two ints; it is trivially safe to move to another thread, is_sendable_v says TRUE, and yet launch_task rejects it. Worse, the branch buys nothing: every borrowed range in the standard library stores a raw pointer or a pointer-like iterator, so the structural member walk already answers false for all of them. I removed the branch entirely from a copy of the header and every one of the library's 11 test files still compiled with zero errors, while span / string_view / subrange / ref_view all stayed false. For an educational library, a decision rule that is both unsound in one direction and provably redundant in the other is worth deleting.

### Le code concerné

```cpp
if (trait_value(^^std::ranges::borrowed_range, type))
        reject(type,
               u8"is a borrowed range: a view over someone else's storage, it "
               u8"does not keep its elements alive",
               path);
```

### La correction

```cpp
Endorse the proposed fix: delete the standalone branch at lifetime_aware.h:156-160 and demote the borrowed-range wording to a message refinement at the structural failure site, so span/string_view/subrange/ref_view keep their good diagnostic while iota_view/empty_view become TRUE. Verified to compile with all 11 test files and to preserve the message.

Two refinements I would add:
1. Apply the same wording at the base-class loop, not only the member loop, so a borrowed range that fails through a base still gets the nice message. Cleanest form is a small helper called from both failure sites rather than the inline `if` duplicated:

    [[noreturn]] inline consteval void
    explain_borrowing_subobject(std::meta::info type, std::meta::info subject,
                                std::meta::info inner, const std::u8string &path) {
        if (trait_value(^^std::ranges::borrowed_range, type))
            reject(type,
                   u8"is a borrowed range: a view over someone else's storage, "
                   u8"it does not keep its elements alive",
                   path);
        explain_lifetime_aware(subject, u8"is not lifetime aware", inner, path);
    }

   called as `explain_borrowing_subobject(type, base, type_of(base), path)` and
   `explain_borrowing_subobject(type, member, member_type, path)`.
2. Keep the `<ranges>` include — it is still needed for the concept used in the message test.

If the maintainer prefers maximum simplicity over diagnostic polish (a defensible call for a conference talk), plain deletion of the branch is also correct: all 11 test files still pass, and the fallback message for string_view is still accurate, just less memorable.
```

### Reproduction

```text
$ cat probe_la_launch3.cpp
#include <threadsafe/threadsafe.h>
#include <ranges>
void f() {
  threadsafe::asynchronous_task_launcher launcher;
  launcher.launch_task([](auto range) { for (int v : range) (void)v; },
                       std::views::iota(0, 10));
}
$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_la_launch3.cpp
  what(): 'std::ranges::iota_view<int, int> is a borrowed range: a view over someone else's storage, it does not keep its elements alive'

Current answers:  is_sendable_v = TRUE, is_lifetime_aware_v = false, for both
  decltype(std::views::iota(0,10))  and  std::ranges::empty_view<int>.

Branch removed entirely (scratchpad copy inc_nb):
  std::span<int>                                    false
  std::span<int, 3>                                 false
  std::string_view                                  false
  std::ranges::subrange<int*>                       false
  std::ranges::subrange<vector<int>::iterator>      false
  std::ranges::ref_view<std::vector<int>>           false
  decltype(std::views::iota(0,10))                  TRUE
  std::ranges::empty_view<int>                      TRUE
  -> all 11 files in tests/ compile with errors=0.

Proposed fix applied (message kept, verdict structural):
  assert_lifetime_aware<std::string_view>() still reports
    'std::basic_string_view<char> is a borrowed range: a view over someone else's storage, ...'
  same eight values as above, and all 11 test files compile with errors=0.
```

### Vérification

I tried to refute this three ways and it survived all three.

(1) Repro is real. `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/lifetime_aware.h:156-160` is exactly as quoted, and it sits in `diagnose_default_is_lifetime_aware` ahead of the structural walk. `launcher.launch_task(lambda, std::views::iota(0, 10))` fails with `'std::ranges::iota_view<int, int> is a borrowed range: ...'`. `is_sendable_v` is TRUE for it, and `is_lifetime_aware_v` is false for both `decltype(std::views::iota(0,10))` and `std::ranges::empty_view<int>`. `launch_task` requires `lifetime_aware` on every argument, so the rejection is not hypothetical.

(2) Design intent does not bless it. CLAUDE.md defines the trait as "True if a `T` owns its data or keeps its referent alive. Ownership is **transitive**". `iota_view<int,int>` holds two ints and references nothing; by the documented definition it owns its data. There is no documented borrowed-range carve-out anywhere in CLAUDE.md, and no test asserts anything about iota/empty_view. The only in-repo acknowledgement is a comment at `allowed_std_wrappers.h:107` ("No borrowed_range test here, unlike the structural walk") — that makes the branch deliberate, but deliberate is not the same as a documented trade-off, and that comment is about the *wrapper* path, not about iota. So this is a defect, not an intentional trade-off.

(3) The redundancy claim holds and I could not construct a counterexample. I patched a scratchpad copy of the header with the branch deleted: all 11 files in `tests/` compile with zero errors, and `span<int>`, `span<int,3>`, `string_view`, `subrange<int*>`, `subrange<vector<int>::iterator>`, `ref_view<vector<int>>` all stay false — each is caught by its stored pointer in the member walk. I additionally swept view adaptors that inherit borrowed-ness (`reverse(span)`, `take(span)`, `views::all(vector)` -> `ref_view`, `drop(string_view)`) and none leaked TRUE without the branch; only `take(iota)` and `drop(iota)` flipped to TRUE, and those are safe. The logic behind that is structural: a view over foreign storage must store a handle into it, and any such handle is either a raw pointer / pointer-wrapper (already rejected) or is itself lifetime-aware (in which case the view genuinely does keep the storage alive and should not be rejected). So the branch cannot be load-bearing for soundness.

The one thing I do correct is the AXIS label. Answering FALSE for a safe type is a usability hole (false negative) by the stated definitions, not a soundness hole — the library never says TRUE for something unsafe here. The real cost is a false negative plus a decision rule in an educational codebase that changes no verdict it is supposed to change.

I also checked the proposed fix compiles and does not regress: applied verbatim to a scratchpad copy, all 11 test files compile, the eight values come out as claimed, and `assert_lifetime_aware<std::string_view>()` still produces the nice `'std::basic_string_view<char> is a borrowed range: ...'` message (without the fix, deletion alone degrades it to `'std::basic_string_view<char>::_M_str (const char*) is a reference or a raw pointer: ...'`).

Severity: I would keep it at medium at most — it is a clean deletion in a codebase whose stated first-class requirement is simplicity, but `std::views::iota` reaching `launch_task` is a narrow trigger.

## F16 — type_index, error_code, error_condition and source_location answer false for all three traits, so launch_task cannot take a source_location

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/vocabulary.h:38` |

### Le problème

Each of these stores a raw pointer to an object with static storage duration — the `type_info`, the `error_category` singleton, the compiler-emitted `source_location::__impl` — so none of them can ever dangle, and all four are among the handful of types a real task actually carries across a thread boundary (a diagnostic, an error code, the call site). The structural walk sees only the raw pointer and answers false. vocabulary.h exists precisely to state by hand what reflection cannot deduce (it already does so for `std::allocator`, `std::stop_token`, `std::stop_source`), so the omission looks like an oversight rather than a decision. `std::source_location` is the sharpest case: it is the natural argument to a logging task.

### Le code concerné

```cpp
template <>
struct is_lifetime_aware<std::stop_token> : std::true_type {};
template <>
struct is_lifetime_aware<std::stop_source> : std::true_type {};
```

### La correction

Add to vocabulary.h (with <typeindex>, <system_error>, <source_location>). All three traits are needed — the lifetime_aware four alone leave launch_task failing on is_sendable, which I verified.

Airtight, normatively:

// The type_info a type_index names lives until the program ends ([expr.typeid]),
// and the __impl behind a source_location is emitted static by the compiler.
// The walk sees the pointer and stops; the pointee outlives every thread.
template <> struct is_sendable<std::type_index> : std::true_type {};
template <> struct is_synchronizable<const std::type_index> : std::true_type {};
template <> struct is_lifetime_aware<std::type_index> : std::true_type {};

template <> struct is_sendable<std::source_location> : std::true_type {};
template <> struct is_synchronizable<const std::source_location> : std::true_type {};
template <> struct is_lifetime_aware<std::source_location> : std::true_type {};

For error_code / error_condition the same three specializations apply, but the
comment must not claim a guarantee the standard does not give — [syserr.errcat.overview]
only *recommends*, in a Note, that a custom error_category be a single object of
static storage duration, and an error_code bound to a stack category compiles today:

// The category behind an error_code is a singleton by convention, not by rule:
// [syserr.errcat.overview] states it in a Note. Every category in the standard
// library, and every one written the way that Note asks, outlives the program.
// This is a vouch for that convention, not a deduction.
template <> struct is_sendable<std::error_code> : std::true_type {};
template <> struct is_synchronizable<const std::error_code> : std::true_type {};
template <> struct is_lifetime_aware<std::error_code> : std::true_type {};
   (and the same three for std::error_condition)

If the library prefers not to vouch for a convention, ship the type_index and
source_location entries alone — those two carry the usability weight anyway
(source_location is the argument a logging task takes), and they cost nothing
in soundness.

### Reproduction

```text
$ ./probe_la_survey2   (g++-16 -std=c++26 -freflection -I.../include)
  std::type_index          false
  std::error_code          false
  std::error_condition     false
  std::source_location     false

$ assert_lifetime_aware diagnostics:
  'std::type_index::_M_target (const std::type_info*) is a reference or a raw pointer: it borrows its referent instead of keeping it alive — hold the object, or a std::shared_ptr to it'
  'std::error_code::_M_cat (const std::_V2::error_category*) is a reference or a raw pointer: ...'
  'std::source_location::_M_impl (const std::source_location::__impl*) is a reference or a raw pointer: ...'

$ ./probe_la_sendsurvey  (is_sendable is false too)
  std::type_index        send=false  life=false
  std::error_code        send=false  life=false
  std::source_location   send=false  life=false

$ ./probe_la_fixes2  (the four specializations above added in the probe)
  std::type_index          TRUE
  std::error_code          TRUE
  std::source_location     TRUE
```

### Vérification

I tried to break this finding on the reproduction lens and it held on every point.

1. The survey reproduces verbatim. All four types answer false for is_lifetime_aware_v, and also false for is_sendable_v and is_synchronizable_v<const ...>. I ran all twelve queries, not just the four the finding quoted.

2. The diagnostics reproduce verbatim, for all four (the finding only showed three; std::error_condition produces the same message via `_M_cat`). The messages are character-for-character what was claimed.

3. The end-to-end consequence is real: `launcher.launch_task(log_it, std::source_location::current())` is a hard compile error today, on a plain `void log_it(std::source_location)`. This is the sharpest case and it is exactly as described.

4. The proposed fix compiles, works, and cannot break the suite. With the specializations added the four answer true, `std::vector<std::error_code>` also becomes true through the std_wrapper rule (good — that composes), and the launch_task call above succeeds. `grep -rn "type_index|error_code|error_condition|source_location"` over both tests/ and include/ returns nothing, so no existing static_assert depends on the current false answers; adding true specializations is monotone.

5. LOCATION is accurate — vocabulary.h:38 is `is_lifetime_aware<std::stop_token>`, and the CURRENT CODE quote matches lines 37-40. The framing is accurate too: vocabulary.h's own comments show it exists to hand-write what reflection cannot deduce, and git log on that file shows no commit that considered and rejected these types.

The one thing I did find is a qualification, not a refutation, and it touches two of the four types. The finding justifies all four with "stores a raw pointer to an object with static storage duration". For std::type_index that is normative ([expr.typeid]: the lifetime of the type_info object extends to the end of the program) and for std::source_location the __impl is compiler-emitted static — both airtight. For std::error_code / std::error_condition it is only a convention: [syserr.errcat.overview] states in a *Note* that applications "should" create a single object of each custom category type. I compiled a program that returns an `std::error_code` bound to a stack-allocated custom category — it builds fine. So blessing those two is a hand-written vouch over user code the library cannot see, in a library that is otherwise conservative about exactly that. That changes the comment those two entries deserve, and is a legitimate reason to scope them or drop them, but it does not make the false negatives unreal — and it leaves type_index and source_location untouched.

Also worth noting for the fix: the PROPOSED FIX code block lists only the four is_lifetime_aware specializations, while the three traits are all needed. I verified that all twelve specializations are required before launch_task accepts a source_location; with only the lifetime_aware four, the call still fails on is_sendable.

## F17 — has_only_default_copy_move_destroy tests std::meta::is_defaulted instead of is_user_provided, so is_sendable/is_synchronizable answer differently depending on whether the TU has seen the out-of-line `= default` — an IFNDR inconsistency (and a false negative in every header-only TU) for the ordinary pimpl/ABI idiom

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/utils.h:149` |

### Le problème

`std::meta::is_defaulted` is true for a copy constructor whose out-of-line definition is `= default`, and false in a translation unit that only sees the in-class declaration. `is_sendable<Widget>` is a class template specialization, so the two TUs define the same specialization with different base classes — a silent ODR violation, no diagnostic required. The trigger is the most ordinary C++ there is: declare the special members in the class for ABI stability or pimpl, define them `= default` in the .cpp. This also undercuts the design claim in CLAUDE.md that the reflective `_v` lookup exists so a specialization written in one TU still reaches every other. `std::meta::is_user_provided` exists in GCC 16 and is exactly the standard's notion ("user-declared and not explicitly defaulted on its first declaration"), so it is stable across TUs; swapping it in keeps the whole existing suite green (verified) and makes both TUs answer false. The same two lines are shared by is_synchronizable<const T> and is_lifetime_aware, so all three traits are affected.

### Le code concerné

```cpp
if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return false;
```

### La correction

In include/threadsafe/details/utils.h:149, replace

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return false;

with

        if (std::meta::is_user_provided(member))
            return false;

`is_user_provided` is "user-declared and not explicitly defaulted or deleted on its *first* declaration", so it cannot change when a later TU sees the out-of-line definition. It also subsumes the `is_deleted` arm (a deleted definition must be the first declaration, hence never user-provided), which is why two conditions collapse to one — a readability win for a conference talk. It aligns the code with its own diagnostic text ("has a user-written copy, move or destructor") and with how the standard library's own triviality traits stay TU-stable.

Caveat worth stating in the talk: the fix makes the answer uniformly `false` for out-of-line-defaulted members, which is conservative rather than ideal (such a copy really is memberwise). That is the same trade-off `std::is_trivially_copyable` makes, and the escape hatch is the one the library already advertises — specialize the trait.

### Reproduction

```text
// probe_sendable_odr.h
#pragma once
struct Widget {
    Widget();
    Widget(const Widget&);
    ~Widget();
    int handle;
};

// probe_sendable_odr_a.cpp
#include <threadsafe/threadsafe.h>
#include "probe_sendable_odr.h"
static_assert(!threadsafe::is_sendable_v<Widget>, "TU_A: expected false");

// probe_sendable_odr_b.cpp
#include <threadsafe/threadsafe.h>
#include "probe_sendable_odr.h"
Widget::Widget() = default;
Widget::Widget(const Widget&) = default;
Widget::~Widget() = default;
static_assert(threadsafe::is_sendable_v<Widget>, "TU_B: expected true");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> -I. probe_sendable_odr_a.cpp
<clean>   // is_sendable_v<Widget> == false
$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> -I. probe_sendable_odr_b.cpp
<clean>   // is_sendable_v<Widget> == true

// With the proposed fix, TU_B fails its (now wrong) assertion and both TUs
// agree on false; all 11 files in tests/ still compile with zero errors:
$ g++-16 ... -Isendaudit/include probe_sendable_odr_b.cpp
error: static assertion failed: TU_B: expected true
```

### Vérification

I tried four refutation angles and none held.

1) Design intent (my assigned lens). CLAUDE.md says nothing about translation-unit sensitivity of the *default* rule, and the library's own diagnostic contradicts the code: `include/threadsafe/details/sendable.h:147` rejects with "has a user-written copy, move or destructor", which is literally the standard's `is_user_provided` notion, while `utils.h:149` tests `is_defaulted`. Code and message disagree, so this is an implementation slip, not a documented trade-off. The nearby comment block on `may_hijack_copy_move` shows the author documents deliberate conservatism when he intends it; there is no such note here.

2) "It's inherent to the design." The library's user-specialization model is already point-of-instantiation sensitive (specialize in one .cpp, not another). But that hazard is user-authored and governed by the familiar "declare the specialization before first use" rule. This one fires on code that never mentions the library: declare special members in the header, define them `= default` in the .cpp — the pimpl/ABI idiom. Not exculpatory.

3) "It's not really soundness." This is the one partly-valid correction. An out-of-line `= default` copy is memberwise, so `true` is the semantically *correct* answer and `false` is the conservative one. There is no type for which the library blesses genuinely unsafe sharing here. The defect is TU-inconsistency (an IFNDR program: `is_sendable<Widget>` gets different base classes / `is_sendable_v<Widget>` different values in different TUs) plus a usability false negative in every header-only TU. I kept real=true but retitled the axis honestly.

4) Report accuracy. One claim in the explanation is wrong: `grep` shows `has_only_default_copy_move_destroy` has exactly two call sites — `sendable.h:146` and `synchronizable.h:157`. `lifetime_aware.h` does not use it. Two traits affected, not three.

Extra manifestation I confirmed beyond the report: because a variable template specialization is memoized at its first instantiation, the divergence also occurs *within a single TU*. In probe_odrdefault_pimpl.cpp, an assert before the out-of-line defaults yields false, and an identical assert after them still yields false (the first instantiation wins) — so the answer depends on where in the TU the trait is first touched, not just on which TU.

Fix verification: `std::meta::is_user_provided` exists in GCC 16 and returns true for the out-of-line-defaulted copy ctor (proved directly). Substituting it makes both TUs answer false; `= delete` on first declaration is not user-provided so deleted members still pass; in-class `= default` still passes; a user-written body is still rejected. All 11 files in tests/ compile clean against the patched header.

## F18 — std::thread::id answers false for all three traits: the structural walk descends into libstdc++'s _M_thread, which is a pointer on Darwin and an integer on glibc, so a curated std vocabulary type gets a platform-dependent false negative

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/sendable.h:112` |

### Le problème

`std::thread::id` is a value type whose entire purpose is to be copied, compared and passed around; it is trivially sendable. On this toolchain the walk reaches `_M_thread`, whose type is `pthread_t` = `_opaque_pthread_t*`, and rejects it as a shared referent. On a platform where `pthread_t` is an integer the same query answers true. The library already recognises that walking libstdc++ internals is not a reliable way to answer these questions — that is why `allowed_std_wrappers` exists and why the comment there says the recursion is deliberately kept out of libstdc++ internals — but `std::thread::id` has no entry anywhere. Practical effect: a task that reports which thread it ran on cannot be launched, and the demo behaves differently on Linux and macOS.

### Le code concerné

```cpp
// no specialization anywhere in include/threadsafe/ for std::thread::id;
// it falls through to the structural walk in diagnose_default_is_sendable:
    if (is_pointer_type(type) || is_reference_type(type))
        reject(type,
               u8"is a pointer or a reference: sending it shares its referent "
               u8"with the other thread, so the referent must be "
               u8"synchronizable — and synchronizability is opt-in",
               path);
```

### La correction

```cpp
The proposed fix is correct as written — I applied it to a copy of the tree and all eleven test TUs still compile clean. In include/threadsafe/details/vocabulary.h, add `#include <thread>` and, next to the stop_token rules:

    // std::thread::id is an opaque, copyable value: it never dereferences the
    // handle it stores, so a copy shares nothing. The structural walk cannot
    // see that — libstdc++ spells the handle as pthread_t, which is a pointer
    // on Darwin and an integer on glibc.
    template <>
    struct is_sendable<std::thread::id> : std::true_type {};
    template <>
    struct is_synchronizable<const std::thread::id> : std::true_type {};
    template <>
    struct is_lifetime_aware<std::thread::id> : std::true_type {};

All three are needed, not just is_sendable: `launchable_task` requires `sendable && lifetime_aware`, so omitting the lifetime_aware specialization leaves `launch_task` still rejecting a thread::id argument. Worth pairing with a regression test asserting the three traits plus `is_sendable_v<std::vector<std::thread::id>>`, since the current suite never mentions thread::id and would not notice a regression.
```

### Reproduction

```text
// probe_sendable_threadid.cpp
#include <threadsafe/threadsafe.h>
#include <thread>
int main() { threadsafe::assert_sendable<std::thread::id>(); }

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_sendable_threadid.cpp
error: uncaught exception of type 'std::meta::exception'; 'what()':
  'std::thread::id::_M_thread (_opaque_pthread_t*) is a pointer or a reference:
   sending it shares its referent with the other thread, so the referent must be
   synchronizable — and synchronizability is opt-in'

// and a struct holding one inherits it:
//   struct HoldsThreadId { std::thread::id id; };
//   static_assert(threadsafe::is_sendable_v<HoldsThreadId>);  // fails
```

### Vérification

I tried to refute this on four fronts and it survived all of them.

1. Does the repro actually reproduce? Yes, verbatim. `threadsafe::assert_sendable<std::thread::id>()` fails with exactly the quoted message, naming `std::thread::id::_M_thread (_opaque_pthread_t*)` and the pointer-rejection reason from sendable.h:112-117. A `static_assert` probe confirms all three traits answer false: `is_sendable_v<std::thread::id>`, `is_synchronizable_v<const std::thread::id>`, `is_lifetime_aware_v<std::thread::id>`. A struct holding one inherits the false, as claimed.

2. Is there an existing specialization the auditor missed? No. `grep -rn "thread" include/` shows `<thread>` is included only by asynchronous_task_launcher.h, and nothing anywhere specializes any trait for `std::thread::id`. `allowed_std_wrappers` is keyed on template-ids (`has_template_arguments`), so a non-template class like `thread::id` can never match it. vocabulary.h covers `std::allocator`, `std::stop_token`, `std::stop_source` and nothing else. So `thread::id` genuinely falls through to the structural walk.

3. Is `std::thread::id` actually safe, i.e. is this a true false-negative rather than a correct rejection? Yes, it is safe. It is a copyable, comparable, hashable opaque value; it never dereferences the handle it stores, so sending a copy shares no mutable state, and its const operations (==, <, hash, operator<<) are race-free. Rust's `ThreadId` is `Send + Sync` for the same reason. So the library is answering FALSE for a type that IS safe — a usability hole by the stated definition.

4. Is the platform-dependence sub-claim technically true? Yes, and the mechanism is verifiable locally. glibc spells `pthread_t` as `unsigned long int`; Darwin spells it `_opaque_pthread_t*`. A probe modelling both layouts (`struct { unsigned long _M_thread; }` vs `struct { _opaque *_M_thread; }`) shows the first is sendable and the second is not, so the same query really does flip across platforms. The finding's characterization of the library's own stance is also accurate: allowed_std_wrappers.h says in so many words that reading template arguments "keeps the recursion out of libstdc++ internals", which is exactly the principle `thread::id` is falling outside of.

5. Does the practical effect hold? Yes. `launcher.launch_task([](std::thread::id origin){}, std::this_thread::get_id())` is rejected by `launchable_task` and the fallback overload emits the same `_M_thread` diagnostic. For a conference demo, "which thread did this run on" is a very natural first example, and it does not compile on macOS.

6. Does the proposed fix compile and keep the suite green? Yes on both counts. I copied include/ and tests/ into the scratchpad, inserted the three specializations plus `#include <thread>` into vocabulary.h next to the stop_token rules, and compiled all eleven test TUs with `g++-16 -std=c++26 -freflection -fsyntax-only`. All eleven passed with zero diagnostics. No test asserts the negative for `thread::id`, so nothing regresses. All three specializations are load-bearing for the reported symptom, since `launchable_task` requires both `sendable` and `lifetime_aware`.

The only thing I would soften is severity. The library documents user specialization as its extension point and test_deferred_specialization.cpp proves a specialization written in a user TU is seen, so a user can work around this in three lines. That argues low-to-medium rather than medium. But the correctness lens I was asked to apply is satisfied without qualification: every factual claim in the finding is true of this exact code.

## F19 — is_synchronizable is never specialized for the standard synchronization primitives, so latch, barrier, counting_semaphore and atomic_flag cannot be shared by reference or via std::ref, and the canonical fan-out demo fails with a diagnostic blaming reference_wrapper's copy constructor

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/sendable.h:27` |

### Le problème

The reference/pointer rule is the mechanism for sharing something with a worker thread, and it is gated entirely on `is_synchronizable`. The only types the library ever makes synchronizable are `std::atomic<T>` (synchronizable.h:22), function types, `synchronized_value<T>`, and whatever a user vouches for. Every standard synchronization primitive is therefore non-sendable by reference — including `std::latch&` and `std::barrier<>&`, which are the canonical "pass this to N threads" demo objects and are specified to be race-free on every member but the destructor. `std::atomic_flag` is the sharpest case: `is_sendable_v<std::atomic_flag>` is true (verified) but `is_sendable_v<std::atomic_flag&>` is false, i.e. the library will let you send the flag by value and lose the point of it, but not share it. A conference audience will try `std::latch&` in the first ten minutes.

### Le code concerné

```cpp
template <class T>
struct is_synchronizable<std::atomic<T>> : is_sendable<T> {};
// ... and nothing else in the library for the standard primitives.
```

### La correction

In include/threadsafe/details/vocabulary.h, alongside the existing stop_token block and in the same citation style. Verified to compile and to leave all 11 test TUs passing.

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <latch>
#include <semaphore>
#include <shared_mutex>

// [thread.sema.cnt]/2, [thread.latch.class]/2, [thread.barrier.class]/3:
// concurrent calls to the members, other than the destructor, do not race.
// The barrier runs its completion function on one of the arriving threads.
template <std::ptrdiff_t LeastMaxValue>
struct is_synchronizable<std::counting_semaphore<LeastMaxValue>> : std::true_type {};
template <> struct is_synchronizable<std::latch> : std::true_type {};
template <class CompletionFunction>
struct is_synchronizable<std::barrier<CompletionFunction>> : is_sendable<CompletionFunction> {};

// [atomics.flag]: the guarantee std::atomic<T> already gets in synchronizable.h.
template <> struct is_synchronizable<std::atomic_flag> : std::true_type {};

// [thread.condition.condvar]: notify_one/notify_all are atomic and wait is
// specified in three atomic parts, so concurrent use is race-free.
template <> struct is_synchronizable<std::condition_variable> : std::true_type {};
template <> struct is_synchronizable<std::condition_variable_any> : std::true_type {};

Corrections to the proposed fix as submitted:
- It named std::condition_variable in the finding text but shipped no specialization for it, and omitted condition_variable_any, recursive_timed_mutex and shared_timed_mutex. Added above (all four verified to compile).
- I would drop its four mutex specializations (std::mutex, recursive_mutex, shared_mutex, timed_mutex). They are sound -- [thread.mutex.requirements.mutex]/6 and [thread.sharedmutex.requirements]/4 back them, and I verified `struct { std::mutex& m; int& guarded; }` still answers false -- but blessing a bare `std::mutex&` teaches the audience to pass a naked mutex around, which is the exact habit synchronized_value<T> exists to replace. The library has a wrapper for the mutex case and none for latch/barrier/semaphore/atomic_flag; that asymmetry is the line to draw. If the maintainer disagrees, the four mutex specializations also compile and break nothing.
- The fix should be paired with a test-suite addition in tests/test_sendable.cpp asserting is_sendable_v<std::latch&>, is_sendable_v<std::barrier<>&>, is_sendable_v<std::reference_wrapper<std::latch>> and can_launch_scoped_task<std::reference_wrapper<std::latch>>, so the fan-out demo is pinned.

### Reproduction

```text
// probe_sendable_lambdas.cpp (excerpt)
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <barrier>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
using threadsafe::is_sendable_v;
#define SHOW(N, ...) static_assert(is_sendable_v<__VA_ARGS__>, "L" #N " " #__VA_ARGS__);
SHOW(13, std::mutex&)
SHOW(14, std::atomic<int>&)
SHOW(15, std::shared_mutex&)
SHOW(16, std::latch&)
SHOW(17, std::barrier<>&)
SHOW(18, std::counting_semaphore<>&)
SHOW(19, std::atomic_flag&)
SHOW(20, std::condition_variable&)

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_sendable_lambdas.cpp
error: static assertion failed: L13 std::mutex&
error: static assertion failed: L15 std::shared_mutex&
error: static assertion failed: L16 std::latch&
error: static assertion failed: L17 std::barrier<>&
error: static assertion failed: L18 std::counting_semaphore<>&
error: static assertion failed: L19 std::atomic_flag&
error: static assertion failed: L20 std::condition_variable&
// L14 (std::atomic<int>&) is the only one that passes.
// Separately verified: is_sendable_v<std::atomic_flag> == true.
```

### Vérification

I reproduced the claim, then attacked it on three fronts; only one attack landed, and only against the title's wording, not the substance.

1. Design intent (the assigned lens). CLAUDE.md does say `is_synchronizable<T>` is opt-in, and synchronizable.h:58-63 repeats it in the assert message. If the library's policy were "we ship no std answers, users vouch", this would be a documented trade-off. But the library's actual policy is the opposite: `vocabulary.h` exists solely to ship std answers, each with a standard citation ([stoptoken.general] for stop_token/stop_source, plus std::allocator), and `allowed_std_wrappers.h` does the same for containers citing [res.on.data.races]. So the maintainers demonstrably do this work for std vocabulary types. Nothing in CLAUDE.md, Task.md, or 40 commits of git log states or implies a decision to exclude the synchronization primitives. It is an omission, not a stated policy.

2. "Does it actually block the demo?" I thought I had a refutation here: `launch_task` also requires `lifetime_aware<Args>`, and `is_lifetime_aware<T&>` is hard-coded false (lifetime_aware.h:30), so a `std::latch&` would be rejected on the lifetime axis anyway and the fix would buy nothing. That refutation fails. `launch_scoped_task` deliberately drops the lifetime_aware requirement (asynchronous_task_launcher.h:30-33) precisely to allow borrowing, and the sharing channel is `std::ref` -> `is_sendable<std::reference_wrapper<T>> : is_synchronizable<T>` (smart_pointers.h:36-38). I compiled the canonical fan-out demo: it is rejected today and compiles with the fix. Worse, the rejection message is actively misleading -- it blames `std::reference_wrapper`'s copy constructor and never names `std::latch`.

3. "Is the fix sound / does it break tests?" I copied the include tree, applied the fix, and compiled all 11 test TUs: zero failures (baseline is also clean, so the comparison is meaningful). I also probed composites: `struct { std::mutex& m; int& guarded; }` stays non-sendable, and `is_synchronizable<std::barrier<F>> : is_sendable<F>` correctly stays false for a non-sendable completion.

What I did knock down: the title's claim that the `T&` rule "pays off nowhere" is false. I verified `is_sendable_v<std::atomic<int>&>`, `is_sendable_v<synchronized_value<std::string>&>`, and `is_sendable_v<void(&)(int)>` are all true today. The rule pays off; what is missing is the vocabulary. I also found the proposed fix incomplete: it names `std::condition_variable` in the title but omits it from the code, along with `condition_variable_any`, `recursive_timed_mutex` and `shared_timed_mutex`.

The one genuine judgment call left is `std::mutex` specifically: blessing a bare `std::mutex&` arguably undercuts the talk's own lesson that you should reach for `synchronized_value<T>` instead of a naked mutex. That is a fair reason to drop the four mutex specializations. It is not a reason to drop `std::latch`, `std::barrier`, `std::counting_semaphore` or `std::atomic_flag`, which have no wrapper in this library, are unambiguously race-free per [thread.latch.class]/2, [thread.barrier.class]/3, [thread.sema.cnt]/2 and [atomics.flag], and are the objects an audience reaches for first. `std::atomic_flag` remains the sharpest single data point: sendable by value, not shareable by reference.

Severity medium is right -- it is a usability/false-negative gap with an escape hatch (THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE), not a soundness hole. But the escape hatch is named "UNSAFE", so telling an audience to vouch for `std::latch` with it teaches the wrong reflex about a type the standard already guarantees.

## F20 — `std_wrapper_is_const_synchronizable` calls `add_const` on each template argument, which is a no-op for a reference argument, so `std::pair<T&,U>` / `std::tuple<T&>` / `std::optional<T&>` answer false where the structurally identical hand-written struct answers true — the allow-list rule is missing the reference branch that the structural walk in `synchronizable.h` already has

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/allowed_std_wrappers.h:100` |

### Le problème

The structural const walk has a dedicated branch for a reference member: it strips the reference and asks the full is_synchronizable of the referent, because "a reference member's constness is unrelated to the referent's". The allow-list rule has no such branch — it calls add_const on each type argument, and add_const on `T&` is the identity, so the query becomes is_synchronizable<T&>, which no specialization matches and the primary template answers false. The result is that the two paths disagree about the same shape: `struct PairLike { std::atomic<int> &first; int second; }` is const-synchronizable, `std::pair<std::atomic<int>&, int>` is not. is_sendable does not diverge (is_sendable<T&> is specialized), so the inconsistency is confined to the const question, which is the one that decides whether an aggregate holding the pair can be shared and whether synchronized_value picks shared_mutex.

### Le code concerné

```cpp
for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_synchronizable_type(std::meta::add_const(wrapped)))
            return false;
```

### La correction

```cpp
In `include/threadsafe/details/allowed_std_wrappers.h`, replace the loop body of `std_wrapper_is_const_synchronizable` so a reference argument takes the same branch a reference *member* takes in the structural walk:

    for (std::meta::info wrapped : wrapped_types_of(type)) {
        // A reference argument's const is unrelated to the referent's, exactly
        // as for a reference member in the structural walk: add_const would be
        // a no-op on it, so the referent is asked the full trait instead.
        const bool readable_from_several_threads =
            std::meta::is_reference_type(wrapped)
                ? is_synchronizable_type(std::meta::remove_cvref(wrapped))
                : is_synchronizable_type(std::meta::add_const(wrapped));

        if (!readable_from_several_threads)
            return false;
    }

This is the proposed fix unchanged — I could not improve on it. `remove_cvref` is the right choice over `remove_reference`: it is what the member branch in `synchronizable.h` uses, so `std::tuple<const T&>` and a `const T &member` keep giving the same answer.

Two optional refinements, both cosmetic rather than corrective:
- The `wrapped_types_of` helper already does `remove_cv` on each argument; that is a no-op for references, so it neither helps nor hurts here and needs no change.
- If the divergence is meant to stay impossible rather than merely fixed, the cleanest long-term shape is to factor the three-way "mutable / reference / value" decision out of `diagnose_default_is_const_synchronizable` into one named helper that both the structural walk and this loop call, so a future branch cannot be added to one path only. That is a larger refactor than the finding warrants; the six-line fix above is sufficient and matches the file's existing style.
```

### Reproduction

```text
// probe_reflayer_ref.cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <tuple>
#include <utility>
using namespace threadsafe;
using AtomicInt = std::atomic<int>;

// hand-written equivalent of std::pair<AtomicInt&, int>
struct PairLike { AtomicInt &first; int second; };

static_assert(is_synchronizable_v<AtomicInt>);
static_assert(is_synchronizable_v<const PairLike>);            // holds
static_assert(is_sendable_v<PairLike>);                        // holds
static_assert(is_sendable_v<std::pair<AtomicInt&, int>>);      // holds
static_assert(is_synchronizable_v<const std::pair<AtomicInt&, int>>, "PAIR CONST-SYNC IS FALSE");
static_assert(is_synchronizable_v<const std::tuple<AtomicInt&>>, "TUPLE CONST-SYNC IS FALSE");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_reflayer_ref.cpp
probe_reflayer_ref.cpp:21:15: error: static assertion failed: PAIR CONST-SYNC IS FALSE
probe_reflayer_ref.cpp:22:15: error: static assertion failed: TUPLE CONST-SYNC IS FALSE

// Fix validated (probe_reflayer_reffix.cpp, compiles clean):
static_assert(!threadsafe::is_synchronizable_v<const PairRef>);   // current answer
static_assert(proposed_const_sync(^^PairRef));
static_assert(proposed_const_sync(^^TupleRef));
static_assert(!proposed_const_sync(^^std::pair<std::string&, int>));  // still conservative
static_assert(!proposed_const_sync(^^std::pair<int*, int>));
```

### Vérification

I read the real file and reproduced every claim.

Mechanism confirmed:
- `allowed_std_wrappers.h:100-102` asks `is_synchronizable_type(add_const(wrapped))` for each type argument. `std::meta::add_const` on a reference type is the identity (references are never cv-qualified), so for `std::pair<std::atomic<int>&, int>` the query is literally `is_synchronizable<std::atomic<int>&>`.
- `synchronizable_base.h` has no `is_synchronizable<T&>` partial specialization (only `T[N]`, `T[]`, plus `const T`, `atomic<T>`, function types elsewhere), so the primary `std::false_type` answers, and the whole wrapper answers false.
- The structural walk in `synchronizable.h` has the missing branch explicitly (`else if (is_reference_type(member_type))` → `is_synchronizable_type(remove_cvref(member_type))`, with the comment "a reference member's constness is unrelated to the referent's"). The two paths genuinely disagree about the same shape.

Refutation attempts, all failed:
1. "Maybe it is deliberate conservatism." The header's own rationale at lines 90-94 states the rule: a const wrapper is read-safe "exactly when everything a reader reaches through it ... is". I compiled the proof that a reader of a *const* pair/tuple of references reaches a **mutable** reference: `decltype(std::get<0>(declval<const std::tuple<AtomicInt&>&>()))` is `AtomicInt&`, and `declval<const std::pair<AtomicInt&,int>&>().first` is `AtomicInt&`. So `add_const` is the wrong question by the file's own stated criterion — this is an oversight, not intent.
2. "Maybe the hand-written struct is also false." No: `struct PairLike { AtomicInt &first; int second; };` gives `is_synchronizable_v<const PairLike> == true` (compiled clean), while the pair/tuple assertions fail.
3. "Maybe it does not propagate / has no consequence." It does: `struct HoldsPair { std::pair<AtomicInt&,int> p; }` is const-non-synchronizable while the structurally identical `HoldsLike` is const-synchronizable (compiled). Via `synchronized_value::get_mutex_type()` (`synchronized_value.h:54`) that flips the mutex from `std::shared_mutex` to `std::mutex` and the const guard from `shared_lock` to `unique_lock`.
4. "Maybe `is_sendable` diverges too, making the finding mis-scoped." It does not — `is_sendable_v<std::pair<AtomicInt&,int>>` is true, because `is_sendable<T&>` is specialized. The finding correctly confines the divergence to the const question.
5. "Maybe the proposed fix is unsound or over-permissive." I replicated both rules over `std::meta::info` and compiled the full matrix: the proposed rule still says false for `std::pair<std::string&,int>`, `std::pair<int*,int>`, and `std::tuple<const std::string&>`, and true for `std::pair<int,int>` and `std::tuple<const AtomicInt&>`. `remove_cvref` (rather than `remove_reference`) matches what the structural walk already does for a `const T&` member, so the two paths stay in lockstep. It also fixes the degenerate `std::pair<void(&)(), int>` case for free.

Scope note: the affected set is exactly `pair`, `tuple`, `optional` — the only allow-listed templates that accept a reference type argument (`vector<T&>`, `variant<T&>`, `array<T&,N>` are ill-formed). This is a false negative / internal inconsistency (usability), not a soundness hole; nothing unsafe is blessed. For an educational library where the structural walk and the allow-list are shown as two views of one rule, having them disagree on the same shape is the real cost.

## F21 — `has_unreflectable_state` reports "a closure type with captures" for any type whose only members are unnamed bit-fields, rejecting a sendable/synchronizable type with a message that is untrue

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/utils.h:103` |
| **Même défaut que** | `F12` — has_unreflectable_state et les champs de bits |

### Le problème

`has_unreflectable_state` infers "closure with captures" from the negative evidence "not empty, not polymorphic, no bases, no non-static data members". `nonstatic_data_members_of` does not report unnamed bit-fields (verified: it returns 0 for `struct { unsigned : 3; unsigned : 5; }`), and such a type is not `is_empty_type`, so it lands in the same bucket. The answer is a false negative and the message is simply untrue, which is costly in a library whose selling point is that every "no" carries its reason. The inconsistency is visible from outside: adding one named member, or one empty base, flips the same type to sendable. GCC 16's <meta> has no `is_closure_type`, so the heuristic cannot be made exact today — but it can be made accurate about what it actually observed.

### Le code concerné

```cpp
// Mostly for closure type.
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}
```

### La correction

```cpp
The proposed reword is correct and compiles, but note it does not remove the false negative — the type stays rejected; it only makes the message accurate about what was observed. Apply it in both places, and refresh the stale comment at utils.h:102 (`// Mostly for closure type.`) so the heuristic's actual scope is stated where it is defined:

utils.h:102
    // Not-empty, no bases, no polymorphism and no non-static data member the
    // access-unchecked walk can see: a closure type with captures, or a type
    // whose only members are unnamed bit-fields. GCC 16's <meta> has no
    // is_closure_type and gives closure types an identifier, so the two cannot
    // be told apart from here.

sendable.h:150
    if (has_unreflectable_state(type))
        reject(type,
               u8"occupies storage that reflection reports no member for — a "
               u8"closure type with captures, or a type whose only members are "
               u8"unnamed bit-fields; specialize is_sendable to state the "
               u8"intent",
               path);

synchronizable.h:166 — same wording with `is_synchronizable` in the closing clause.

Verified: patched copies of both headers compile every file in tests/ (11/11 OK); no test greps for the old message.
```

### Reproduction

```text
// probe_sendable_bitfield.cpp
#include <threadsafe/threadsafe.h>
struct Flags { unsigned : 3; unsigned : 5; };
int main() { threadsafe::assert_sendable<Flags>(); }

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_sendable_bitfield.cpp
error: uncaught exception of type 'std::meta::exception'; 'what()':
  'Flags holds state reflection cannot see (a closure type with captures);
   specialize is_sendable to state the intent'

// The neighbours, verified in one file:
//   struct OnlyUnnamedBits { unsigned : 3; unsigned : 5; };  -> not sendable
//   struct NamedPlusBits   { unsigned a; unsigned : 5; };    -> sendable
//   struct BitsWithBase : Empty { unsigned : 3; };           -> sendable
// and nonstatic_data_members_of(^^OnlyUnnamedBits).size() == 0.
```

### Vérification

Every factual claim reproduces. `nonstatic_data_members_of` reports zero members for `struct { unsigned : 3; unsigned : 5; }` (even `members_of` yields no non-static data member among its 8 entries), and `is_empty_type` is false for it, so `has_unreflectable_state` at utils.h:103 fires and the type is rejected with a message asserting it is "a closure type with captures" — which is untrue. The false negative is confirmed on both twins: `assert_sendable<Flags>()` and `assert_synchronizable<const Flags>()` both throw that message; the sync case is the more glaring one since a `const` object with no reflectable state is trivially shareable. The neighbour inconsistency is confirmed: adding one named member or one empty base flips the same type to sendable. I tried to refute via a tighter closure test and failed: GCC 16's `<meta>` has no `is_closure_type`, and `has_identifier(^^ClosureType)` returns true, so the heuristic cannot be narrowed today — which is exactly what the finding says. The proposed reword compiles and all 11 test files still pass against the patched headers; no test asserts on the message text. Severity low is right: a struct whose only members are unnamed bit-fields is a degenerate shape, and the damage is a misleading diagnostic rather than unsoundness. The finding's framing needs one correction — the proposed fix does not restore sendability, it only makes the message state the evidence instead of the guess.

## F22 — THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE expands to a qualified specialization and so only works at global scope, with no comment saying so

| | |
|---|---|
| **Gravité** | détail |
| **Confiance** | probable |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/synchronizable_base.h:30` |

### Le problème

There is no README (docs were removed in 3df8ad7) and CLAUDE.md is an agent-instruction file, so the only place a user learns how to opt a type in is the text of a diagnostic — and only for is_sendable. Two conventions are never stated anywhere: that read-only safety is claimed by specializing is_synchronizable<const X> rather than is_synchronizable<X>, and that THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE expands to a qualified definition and therefore only works at global scope. Since users declare their types inside namespaces, the second bites on the first attempt.

### Le code concerné

```cpp
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)  \
    template <>                                       \
    struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}
```

### La correction

Keep only the scope note; drop the rest of the proposed block, since the const convention and the macro are already stated in the is_synchronizable diagnostic (synchronizable.h:59-63) and in the comment above `is_synchronizable<const T>`.

Above the macro in synchronizable_base.h:

// Defines a qualified specialization, so it must appear at global scope --
// outside any namespace of your own:
//     namespace app { struct Widget { ... }; }
//     THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(app::Widget);
// Accepts the const form too: THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const app::Node).

Comment-only, so the test suite is unaffected.

### Reproduction

```text
// probe_api_macro_ns.cpp — the first thing a user writes
#include <threadsafe/threadsafe.h>
namespace app {
struct Widget { int* borrowed; };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Widget);
}

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_macro_ns.cpp
synchronizable_base.h:32:24: error: declaration of
  'struct threadsafe::is_synchronizable<app::Widget>' in namespace 'app'
  which does not enclose 'threadsafe'

// For the record, the extension mechanism itself is sound -- probe_api_flex_user_types.cpp
// (own mutex wrapper, own owning smart pointer, third-party incomplete type; partial and
// explicit specializations, including ones overlapping the std_wrapper rule) compiles with
// zero diagnostics. The gap is documentation, not capability.
```

### Vérification

Reproduction lens: I re-ran the repro under a unique name and it fails exactly as claimed, at exactly the cited line (synchronizable_base.h:32, the macro body). So the load-bearing half of the finding — THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE expands to a *qualified* specialization `struct threadsafe::is_synchronizable<...>` and is therefore usable only at global scope (or another namespace enclosing `threadsafe`, of which there is none) — is confirmed, and nothing in the tree documents that: `grep -rn THREADSAFE_UNSAFE_ASSERT include tests CLAUDE.md Task.md` shows the definition, one mention in a diagnostic string, and five call sites, all at global scope. There is no README (confirmed: repo root holds only CLAUDE.md, CMakeLists.txt, Task.md, include/, tests/).

Two supporting claims are false, though, and they are the ones I could refute:
(1) "documented only inside error-message strings — and only for is_sendable". The is_synchronizable path has its own diagnostic naming the macro (synchronizable.h:56-63). I triggered it: assert_synchronizable<app::Widget>() prints "is_synchronizable<T> is opt-in: it holds only for types that synchronize themselves (std::atomic, a mutex-protected wrapper). Ask is_synchronizable<const T> for a read-only share, or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE to vouch for it".
(2) "that read-only safety is claimed via the const form is never stated anywhere" — that same one diagnostic states it, and the header comment above `is_synchronizable<const T>` (synchronizable.h:70-76) explains the const model; tests/test_synchronizable.cpp:57 also uses the macro in its const form. So the const convention is not undocumented.

Also worth weighing against severity: the compiler's message here is unusually self-explanatory ("in namespace 'app' which does not enclose 'threadsafe'"), the fix is to move one line outside the braces, and the constraint is the same one every trait-customization point in C++ carries (std::hash, formatter). It is a genuine nit, not a trap.

Fix sanity-check: the proposal is comment-only, so it cannot affect the compile-time test suite. I compiled the recipe it proposes verbatim (own template `app::box<T>` with is_sendable / is_lifetime_aware / is_synchronizable<const ...> partial specializations inside namespace threadsafe, plus the macro at global scope for app::Widget) — exit 0, zero diagnostics, all four static_asserts pass, and the user's `is_synchronizable<const app::box<T>>` does not tie with the library's own `is_synchronizable<const T>`.

Net: the finding survives, narrowed to the macro-scope half; the documentation-gap framing around the const convention should be dropped.

---

# Robustesse — les helpers

`copy_on_write<T>`, `synchronized_value<T>` et `asynchronous_task_launcher`.

## F23 — as_mutable() returns a bare T& that stays live after a later handle copy re-shares the block, so a program following the library's own "share by copying it" rule races — but the proposed mutation_guard does not fix this and breaks test_copy_on_write.cpp:124

| | |
|---|---|
| **Gravité** | critique |
| **Confiance** | probable |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:29` |

### Le problème

`as_mutable()` checks exclusivity once and then returns a raw `T&` with no lifetime bound. The check is a statement about the instant it ran, but the reference outlives it: copying the handle afterwards (which `is_sendable_v<copy_on_write<T>>` explicitly blesses, so `launch_task` accepts it) re-shares the very block the caller still holds a mutable reference into. The result is a real data race — verified as an AddressSanitizer heap-use-after-free with the reader thread reading a buffer the writer thread reallocated. Note the library already knows this hazard and guards it elsewhere: `value_guard` in synchronized_value.h deletes `operator*() &&` with the message "a temporary guard is destroyed at the semicolon, so it cannot hand out a reference", yet `copy_on_write`, whose whole safety story is the detach, hands out an unguarded one. (The same root cause silently rebinds references obtained from `operator*`: `const T& r = *cow;` followed by a detaching `as_mutable()` leaves `r` pointing at the old block, which dies with the last other holder.)

### Le code concerné

```cpp
T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }
```

### La correction

```cpp
Reject the proposed mutation_guard: it is inert (verified — the identical ASAN heap-use-after-free reproduces with it in place, because a named guard is legal and the handle stays copyable alongside it) and it breaks tests/test_copy_on_write.cpp:124.

Only two options actually address the mechanism.

(a) Bound the mutation window to a call, so no reference can outlive the exclusivity check:

    template <std::invocable<T&> Mutator>
    decltype(auto) mutate(Mutator mutator)
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
        return std::invoke(std::move(mutator), *ptr_);
    }

This is not airtight — a mutator capturing the handle by reference can still copy it mid-callback — but it converts an accidental hazard into a deliberate one, and it is the shape the library already reaches for elsewhere. It does require updating test_copy_on_write.cpp:124.

(b) If the T& return is kept for teaching reasons (it is the more readable spelling, and this is conference code), state the precondition in the same voice the launcher already uses at asynchronous_task_launcher.h:101-104:

    // PRECONDITION: the handle must not be copied while the returned reference
    // is live. The detach proves exclusivity at the moment it runs; the
    // reference outlives that moment and a later copy re-shares the block.

Given the educational mandate, (b) is the proportionate change and (a) is the sound one. Do not ship the mutation_guard.
```

### Reproduction

```text
// (1) the library accepts the racy program -- /private/tmp/.../probe_cow_escaping_ref.cpp
#include <threadsafe/threadsafe.h>
#include <string>
using namespace threadsafe;
void reader(copy_on_write<std::string> snapshot) { volatile auto n = snapshot->size(); (void)n; }
void demonstrate() {
    asynchronous_task_launcher launcher;
    copy_on_write<std::string> document{"hello"};
    std::string& mutable_view = document.as_mutable();   // use_count()==1 -> in place
    launcher.launch_task(reader, document);              // blessed: is_sendable_v<cow<string>>
    mutable_view.append(" world");                       // writes the block the task reads
}
$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_cow_escaping_ref.cpp
(no diagnostics -- compiles cleanly)

// (2) the race is real -- probe_cow_asan.cpp
int main() {
    copy_on_write<std::string> document{std::string(64, 'h')};
    std::string& mutable_view = document.as_mutable();
    copy_on_write<std::string> snapshot = document;      // shared now, count==2
    std::thread reader([snapshot] {
        unsigned long long checksum = 0;
        for (int i = 0; i < 2000000; ++i) for (char c : *snapshot) checksum += c;
        std::printf("checksum %llu\n", checksum);
    });
    for (int i = 0; i < 2000000; ++i) { mutable_view.append("x"); mutable_view.resize(64); mutable_view.shrink_to_fit(); }
    reader.join();
}
$ g++-16 -std=c++26 -freflection -fsanitize=address -g -O1 -I<include> probe_cow_asan.cpp -o probe_cow_asan && ./probe_cow_asan
==73813==ERROR: AddressSanitizer: heap-use-after-free on address 0x6070000000cb
READ of size 1 at 0x6070000000cb thread T1
    #0 ... in std::thread::_State_impl<...>::_M_run() probe_cow_asan.cpp:255
freed by thread T0 here:
    #1 ... in std::__cxx11::basic_string<...>::_M_mutate(...) atomicity.h:405
    #2 ... in main probe_cow_asan.cpp:22
SUMMARY: AddressSanitizer: heap-use-after-free

// (3) the proposed guard rejects both leaking spellings -- probe_cow_guard_fix.cpp
static_assert(hands_out_bare_ref<copy_on_write<std::string>>);   // today
static_assert(!hands_out_bare_ref<cow2<std::string>>);           // with the guard
static_assert(!deref_of_temporary_ok<cow2<std::string>>);        // `T& r = *cow.as_mutable();` is a hard error
$ g++-16 ... probe_cow_guard_fix.cpp  ->  GUARD FIX WORKS
```

### Vérification

I tried to refute this and the factual core held, but the finding's supporting analysis and its fix both collapsed.

WHAT SURVIVES. as_mutable() at copy_on_write.h:29 returns a bare T& with no lifetime bound after a use_count() check that is only true at the instant it runs. I reproduced the reported heap-use-after-free verbatim against the unmodified header (probe_refute_cow_asan.cpp, ASAN: "READ of size 1 thread T1", freed in basic_string::_M_mutate from T0), and confirmed the launch_task variant compiles clean because is_sendable_v<copy_on_write<std::string>> is true.

The strongest reason this is a library defect rather than generic C++ reference invalidation: the racy program follows the library's OWN documented discipline. test_copy_on_write.cpp:109 states "as_mutable rebinds the handle, so one copy_on_write object belongs to one thread; share by copying it" — and the repro does exactly that: one thread owns `document`, the other receives a copy. Nothing in the model is violated, and it still races. Unlike std::vector reallocation, the invalidating act here (copying the handle) is the library's own sanctioned sharing mechanism, performed at a point where the user has done nothing wrong. cow is also the one type that relaxes the sendability requirement from is_synchronizable<T> down to is_synchronizable<const T>, and it buys that relaxation entirely with the detach — so a write that outlives the detach spends a soundness credit the type never earned.

WHAT I REFUTED (three material errors, each proven):

(1) The proposed fix does not fix the reported bug. I built the proposal's mutation_guard as fix::cow2 and re-ran the identical race (probe_refute_cow_fixfails.cpp): same ASAN heap-use-after-free. Deleting operator*() && only kills the one-liner `T& r = *cow.as_mutable();`. A NAMED guard is legal, and the handle remains copyable while it is alive — which is the exact mechanism the finding's own EXPLANATION identifies as the root cause. The remedy is inert against the defect it describes.

(2) The fix breaks the existing test suite: tests/test_copy_on_write.cpp:124 asserts std::same_as<decltype(declval<cow<int>&>().as_mutable()), int&>. Retargeted at the proposal it fails with "static assertion failed / constraints not satisfied".

(3) The claimed precedent is a misreading of the code. value_guard::operator*() const& hands out a bare T& just as freely; probe_refute_guard_leaks.cpp compiles clean and writes through an escaped pointer with no lock held. value_guard's deleted rvalue overloads guard against a lock RELEASED AT THE SEMICOLON, not against a reference escaping the guard's scope. So "the library already knows this hazard and guards it elsewhere" is false, and the inconsistency argument dissolves.

Severity is also overstated at critical: triggering this requires deliberately parking a T& across a handle copy, and the library is explicitly not a borrow checker (asynchronous_task_launcher.h:101-104 already carries a PRECONDITION comment conceding "The traits cannot check this; the join bounds the invocation, not the borrow"). I mark confidence "likely" rather than "certain" because whether reference-lifetime tracking is in scope for a type-level Send/Sync model is a design judgment, not a fact I can compile.

## F24 — synchronized_value selects its mutex *type* from the user-extensible is_synchronizable trait, so layout is not carried in the type's identity: a TU that has not seen the opt-in builds a 72-byte object that a TU that has builds as 208 bytes, under one mangled name — escalating a bool-level trait IFNDR into silent memory corruption

| | |
|---|---|
| **Gravité** | critique |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:61` |

### Le problème

The mutex type — and therefore the size and the offset of value_ — is chosen from is_synchronizable_v<const T> at the point synchronized_value<T> is first instantiated. That trait is designed to be answered late, from a user's own TU (THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE, or a plain specialization; tests/test_deferred_specialization.cpp and tests/test_soundness_regressions.cpp:79 both do exactly this). A TU that has not yet seen the opt-in builds synchronized_value<Config> as 72 bytes around a std::mutex; a TU that has seen it builds the same type name as 208 bytes around a std::shared_mutex. Both mangle identically, so the program links with no diagnostic and then locks a shared_mutex that was never constructed. The choice varies but is not carried in the type's identity — that is the whole defect. This is distinct from the known memoization/IFNDR issue about frozen trait answers: here the divergence is in the class layout of a library type, and it corrupts memory rather than disagreeing about a bool.

### Le code concerné

```cpp
static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];
```

### La correction

```cpp
Do NOT use the proposed `template <class T, class Mutex = std::conditional_t<...>>`. I compiled and ran it: a default template argument is resolved per point-of-use, so it is exactly as late-bound as the trait it wraps, and because a non-template function's return type is not mangled, `std::shared_ptr<synchronized_value<Config>> make_in_tu_a()` still mangles to `_Z13make_in_tu_av` in both TUs. The two TUs name different types, link silently anyway, and abort identically (72 vs 208).

Instead, remove the trait from the layout entirely — store one mutex type unconditionally and let the trait gate only *availability*:

    template <class T>
    class synchronized_value {
        static_assert(sendable<T>, ...);
    public:
        using guard = value_guard<T, std::unique_lock<std::shared_mutex>>;
        using const_guard = value_guard<const T, std::shared_lock<std::shared_mutex>>;

        [[nodiscard]] guard lock() { return guard{mutex_, value_}; }

        [[nodiscard]] const_guard lock_shared() const
            requires is_synchronizable_v<const T>
        { return const_guard{mutex_, value_}; }

    private:
        mutable std::shared_mutex mutex_;
        T value_;
    };

Every TU now agrees on sizeof and on the offset of value_, whatever it believes about the trait. A TU that has not seen the opt-in simply cannot call lock_shared() — a diagnosed compile error rather than corruption — and a TU that has seen it takes a shared_lock on the very same shared_mutex the other TU locks exclusively, which is correct mutual exclusion. This also deletes both [: :] splices and both public consteval helpers, which is the simpler thing to put on a conference slide.

Cost: sizeof(std::shared_mutex) is 200 vs 64 on this libstdc++, paid even by types that only ever take the exclusive lock. If that is unacceptable, the other sound option is an explicit, NON-defaulted second parameter (`synchronized_value<Config, std::shared_mutex>`) so the choice is spelled in the source of every TU and cannot be resolved late — but that changes the API for every user, and a defaulted version of it is the broken fix above.

Worth stating in the docs regardless: a program where one TU declares a threadsafe:: trait specialization that another TU's instantiation never saw is already IFNDR under [temp.expl.spec]/7. The library cannot repeal that rule; what it can do is stop amplifying it from a disagreement about a bool into a disagreement about member offsets.
```

### Reproduction

```text
// probe_interact_odr_shared.h
#pragma once
#include <threadsafe/threadsafe.h>
#include <memory>
#include <cstddef>
struct Config { int value; mutable int cached_hits; };
using SV = threadsafe::synchronized_value<Config>;
std::shared_ptr<SV> make_in_tu_a();
std::size_t size_in_tu_a();
const char* mutex_in_tu_a();

// probe_interact_odr_a.cpp
#include "probe_interact_odr_shared.h"
#include <concepts>
#include <shared_mutex>
std::shared_ptr<SV> make_in_tu_a() { return SV::make(Config{7, 0}); }
std::size_t size_in_tu_a() { return sizeof(SV); }
const char* mutex_in_tu_a() {
    return std::same_as<SV::mutex, std::shared_mutex> ? "shared_mutex" : "mutex"; }

// probe_interact_odr_b.cpp
#include "probe_interact_odr_shared.h"
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Config);   // the documented opt-in, in the user's TU
#include <concepts>
#include <shared_mutex>
#include <cstdio>
int main() {
    std::printf("TU A: sizeof(SV)=%zu mutex=%s\n", size_in_tu_a(), mutex_in_tu_a());
    std::printf("TU B: sizeof(SV)=%zu mutex=%s\n", sizeof(SV),
                std::same_as<SV::mutex, std::shared_mutex> ? "shared_mutex" : "mutex");
    auto shared = make_in_tu_a();   // constructed with TU A's 72-byte layout
    auto guard = shared->lock();    // locked/indexed with TU B's 208-byte layout
    guard->value = 1234;
    std::printf("survived\n");
}

$ g++-16 -std=c++26 -freflection -I<inc> probe_interact_odr_a.cpp probe_interact_odr_b.cpp -o probe_interact_odr
   (compiles and links with zero diagnostics)
$ ./probe_interact_odr
TU A: sizeof(SV)=72 mutex=mutex
TU B: sizeof(SV)=208 mutex=shared_mutex
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/shared_mutex:205: void std::__shared_mutex_pthread::lock(): Assertion '__ret == 0' failed.
exit=134
```

### Vérification

I tried to refute this three ways and it survived two of them; the third refutation landed on the proposed fix, not the finding.

1) Does the repro actually reproduce? Yes, byte-for-byte. I rebuilt it with uniquely-named probes against the real headers. Two TUs, both spelling `threadsafe::synchronized_value<Config>`, compile and link with zero diagnostics and produce `sizeof` 72 vs 208; the process then aborts inside `std::__shared_mutex_pthread::lock()` because TU B locks a `shared_mutex` at an offset where TU A constructed a `mutex`. Exit 134.

2) Is it really the library's fault, or just plain IFNDR the library cannot repeal? Partly the latter — TU A implicitly instantiates `is_synchronizable<Config>` without ever seeing the specialization, which is already ill-formed-no-diagnostic-required under [temp.expl.spec]/7. So the finding overstates it slightly when it frames this as the library manufacturing a fresh ODR violation. But the substantive claim survives: what the library controls is the blast radius. `get_mutex_type()` at synchronized_value.h:53-61 is the *only* place in the library where a late-bindable, user-extensible trait selects a type that lands in a class's layout — I grepped; `copy_on_write.h:46-47` only computes a bool, and no other header splices a type out of a trait. Everywhere else a stale trait answer means two TUs disagree about a `bool`; here it means two TUs disagree about member offsets. Escalating a bool-level IFNDR into memory corruption is a design choice this file makes and can unmake, and the library's own documented feature ("a specialization written in a user's TU still reaches the recursion" — tests/test_deferred_specialization.cpp, tests/test_soundness_regressions.cpp) is what sets the hazard up. So real=true.

3) Does the PROPOSED fix close it? No — and this is where I had to correct the finding. I transcribed the proposed `template <class T, class Mutex = std::conditional_t<...>>` verbatim and ran two scenarios:
  - Opt-in written *after* the alias that forces the default argument: it errors, but only accidentally — the default arg instantiates `is_synchronizable<Config>` early, so GCC emits "specialization of 'threadsafe::is_synchronizable<Config>' after instantiation". That diagnostic comes from the eager instantiation, not from the type-identity encoding.
  - Opt-in written *correctly*, before first use in that TU (which is what a careful user does): the two TUs now genuinely name different types, `synchronized_value<Config, std::mutex>` and `synchronized_value<Config, std::shared_mutex>` — and it still links silently and still aborts with 72 vs 208. Reason: a non-template free function's return type is not mangled, so `std::shared_ptr<synchronized_value<Config>> make_in_tu_a()` mangles to `_Z13make_in_tu_av` in both TUs regardless of which type the default argument resolved to. A default template argument is resolved per point-of-use, so it is exactly as late-bound as the trait it wraps; moving the divergence into the template-id does not move it into any mangled name that the linker compares. The fix only works when the type appears in a *parameter* type of a cross-TU symbol, which is not the case in its own claimed repro.

So the correct fix is to stop letting the trait touch the layout at all. I built and ran that: one fixed `std::shared_mutex` member, with the trait constraining only which member function exists (`lock_shared() requires is_synchronizable_v<const T>`). Both TUs report sizeof=208, the program reads and writes correctly, exit 0. Worst case under a divergent trait is now a diagnosed compile error in the TU lacking the opt-in, or a conservatively-exclusive lock — never corruption.

Note this also costs the library nothing in readability; it deletes both `[: :]` splices and both public `consteval` helpers, which is a win for an educational, conference-facing codebase.

## F25 — `is_synchronizable<const copy_on_write<T>>` is false because the generic const walk rejects copy_on_write's own variadic constructor template, so a COW handle can never be nested inside another COW nor be a by-value member of a const-shared struct — and `synchronized_value<copy_on_write<T>>` picks std::mutex over std::shared_mutex

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:57` |
| **Même défaut que** | `F27`, `F30` — copy_on_write sans règle is_synchronizable<const ...> |

### Le problème

A `const copy_on_write<T>` exposes only `operator*`/`operator->` (both const, both yielding `const T`) and the copy constructor, which touches nothing but the control block's atomic refcount. Reading one from several threads at once is therefore exactly as safe as reading a `const T` — yet the trait answers false, because no specialization exists and the generic const walk rejects the type outright on its variadic constructor template. The false negative is contagious: `is_sendable<copy_on_write<U>>` requires `is_synchronizable_v<const U>`, so *no* type containing a copy_on_write can itself be COW'd or shared. `copy_on_write<copy_on_write<std::string>>`, `copy_on_write<std::vector<copy_on_write<std::string>>>`, and `copy_on_write<struct{ copy_on_write<std::string> body; int rev; }>` are all rejected — a COW tree, the canonical use of the pattern, is unreachable. And `synchronized_value<copy_on_write<T>>::mutex` picks `std::mutex` instead of `std::shared_mutex`, serializing the readers that copy-on-write exists to let run in parallel.

### Le code concerné

```cpp
template <class T>
struct is_sendable<copy_on_write<T>>
    : std::bool_constant<detail::cow_is_sendable<T>()> {};

template <class T>
struct is_lifetime_aware<copy_on_write<T>> : is_lifetime_aware<T> {};
```

### La correction

Add the missing const specialization next to the existing send rule in copy_on_write.h, but reuse the predicate already there rather than weakening it to `is_synchronizable<const T>`:

// Through const the handle offers only `const T&` — plus the copy constructor,
// which hands the other thread co-ownership of the T. That is the same pair of
// obligations as sending the handle, so it asks the same question. The full
// trait stays false: as_mutable rebinds ptr_, so a non-const handle is one
// thread's (see the existing !is_synchronizable_v<cow<int>> test).
template <class T>
struct is_synchronizable<const copy_on_write<T>>
    : std::bool_constant<detail::cow_is_sendable<T>()> {};

This unblocks every case in the finding (nested COW, cow<Document>, cow<vector<Body>>, shared_mutex in synchronized_value), still refuses cow<Cache> and cow<Outer>, and additionally refuses `const cow<T>` for a T that is const-readable but not sendable — which the finding's `: is_synchronizable<const T>` version wrongly accepts. Verified: probe_cowrefute_fix2.cpp passes, and all 11 files in tests/ compile clean with it injected.

### Reproduction

```text
// probe_cow_nested.cpp -- what the library answers today (all of these HOLD)
#include <threadsafe/threadsafe.h>
using namespace threadsafe;
using Body = copy_on_write<std::string>;
static_assert(is_sendable_v<Body>);                        // the leaf works
static_assert(!is_sendable_v<copy_on_write<Body>>,         "nested COW rejected");
struct Document { Body body; int revision; };
static_assert(is_sendable_v<Document>);
static_assert(!is_synchronizable_v<const Document>,        "not const-shareable");
static_assert(!is_sendable_v<copy_on_write<Document>>,     "so it cannot be COW'd");
static_assert(!is_sendable_v<copy_on_write<std::vector<Body>>>);
static_assert(std::is_same_v<synchronized_value<Body>::mutex, std::mutex>);
static_assert(std::is_same_v<synchronized_value<std::string>::mutex, std::shared_mutex>);
$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_cow_nested.cpp  ->  ALL HOLD

// probe_cow_fix_constsync.cpp -- with the one-line specialization added
static_assert(is_synchronizable_v<const Body>);
static_assert(!is_synchronizable_v<Body>,                  "still not full-Sync");
static_assert(is_sendable_v<copy_on_write<Body>>,          "nested COW now works");
static_assert(is_sendable_v<copy_on_write<Document>>);
static_assert(is_sendable_v<copy_on_write<std::vector<Body>>>);
static_assert(std::is_same_v<synchronized_value<Body>::mutex, std::shared_mutex>);
struct Cache { int raw; mutable int parsed; };
static_assert(!is_synchronizable_v<const copy_on_write<Cache>>);  // still refused
static_assert(!is_sendable_v<copy_on_write<Cache>>);
$ g++-16 ... probe_cow_fix_constsync.cpp  ->  FIX WORKS

// and the whole existing suite stays green with the specialization injected:
$ for f in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -I<include> -include inject.h $f; done
test_asynchronous_task_launcher.cpp  PASS
test_containers.cpp                  PASS
test_copy_on_write.cpp               PASS
test_deferred_specialization.cpp     PASS
test_diagnostics.cpp                 PASS
test_lifetime_aware.cpp              PASS
test_sendable.cpp                    PASS
test_smart_pointers.cpp              PASS
test_soundness_regressions.cpp       PASS
test_synchronizable.cpp              PASS
test_synchronized_value.cpp          PASS
```

### Vérification

I tried to refute this on the reproduction axis and could not. Every assertion in the claimed repro compiles and holds verbatim against the unmodified headers, and the mechanism the finding names is the real one.

WHAT I CONFIRMED
1. Baseline (probe_cowrefute_today.cpp, unmodified headers, all 10 static_asserts pass): `is_sendable_v<cow<std::string>>` true; `!is_sendable_v<cow<cow<std::string>>>`; `is_sendable_v<Document>` true but `!is_synchronizable_v<const Document>` and `!is_sendable_v<cow<Document>>`; `!is_sendable_v<cow<std::vector<Body>>>`; `synchronized_value<Body>::mutex == std::mutex` while `synchronized_value<std::string>::mutex == std::shared_mutex`. So the false negative and its contagion are exactly as described.
2. The stated cause is correct, not guessed. Running `assert_synchronizable<const copy_on_write<std::string>>()` yields: "has a user-written copy, move or destructor — or a template that may be selected as one". Tracing it, `has_only_default_copy_move_destroy` (utils.h:139) calls `may_hijack_copy_move` (utils.h:133), which returns true for ANY `is_constructor_template` — the comment at utils.h:128-132 states outright that a constraint like `requires !same_as<remove_cvref_t<U>, T>` is indistinguishable from a greedy forwarding ctor, so copy_on_write's own variadic ctor (copy_on_write.h:18-24) blocks the generic const walk. copy_on_write is rejected by a guard whose escape hatch ("specialize the trait") the header never uses for the const question.
3. The proposed one-line specialization compiles, produces every claimed result (including still refusing `cow<Cache>` with its mutable member, and still leaving the full `is_synchronizable<copy_on_write<T>>` false), and all 11 test files stay green under `-include`. I verified the `-include` was actually reaching the TU with an `#error` marker first, since a silently-unapplied injection would have faked the PASS row.

MY ONE REAL OBJECTION — to the fix, not the finding
The proposed `is_synchronizable<const copy_on_write<T>> : is_synchronizable<const T>` drops a requirement the library itself insists on elsewhere. A `const copy_on_write<T>&` still exposes the copy constructor, so a second thread can take co-ownership of the T and may be the one that runs its destructor. test_copy_on_write.cpp says this in so many words for the send rule: `!is_sendable_v<cow<NonSendable>>`, "the T is copied on the receiving thread and destroyed by whoever drops the last handle". The proposed fix would answer true for a T that is const-readable but not sendable.

I tried to build an exploit and failed: `is_sendable<T&>` (sendable.h:27) and `is_sendable<reference_wrapper<T>>` both strip cv and ask the FULL `is_synchronizable`, which stays false, and every container that would carry the handle to another thread demands `is_sendable<copy_on_write<T>>` first. probe_cowrefute_sound.cpp confirms `is_sendable_v<reference_wrapper<const cow<ThreadBound>>>` is false. So the hole is unreachable today — but it is latent, and the conservative form is also shorter and needs no new reasoning, so I'd apply that instead.

Verdict: the finding survives. Usability, and the severity is fair for an educational library — a COW tree is a natural thing to show on stage and the library cannot express one.

## F26 — The `&&`-delete message on `operator*`/`operator->` promises a guarantee C++ cannot give: a reference escapes the lock via a named guard, or via any `const value_guard&` binding of the prvalue

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:29` |

### Le problème

The deletion only fires when the *implicit object argument is an rvalue*. Binding the prvalue guard to a `const value_guard&` — via a `static_cast`, or simply by passing it to any function whose parameter is `const Guard&` — reclassifies it as an lvalue, so the `const&` overloads on lines 32-33 are selected and hand out a raw `T&`/`T*`. The guard temporary is still destroyed at the end of the full-expression, so the reference outlives the lock. I compiled and *ran* this: the escaped `std::vector<int>&` mutated the protected value with no lock held, and then again while a shared_lock was outstanding (the exact race a second thread in `lock_shared()` would lose). GCC 16 emits nothing at `-Wall -Wextra -Wdangling-reference -Wdangling-pointer`, and it ignores `[[gnu::lifetimebound]]` entirely (verified: `warning: 'gnu::lifetimebound' scoped attribute directive ignored`), so there is no compiler assist available. The delete-message states a guarantee — "so it cannot hand out a reference" — that the type does not deliver, which is the worst failure mode for a library whose thesis is compile-time safety.

### Le code concerné

```cpp
T& operator*() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");
    T* operator->() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");

    T& operator*() const& noexcept { return *value_; }
    T* operator->() const& noexcept { return value_; }
```

### La correction

Keep the guard API and the `&&` deletions — they are correct and they do block the accidental `*sv.lock()` one-liner. Fix only the message, so it states what the deletion actually buys instead of a guarantee the type cannot deliver:

    T& operator*() && noexcept = delete("`*sv.lock()` locks and unlocks in the same expression; name the guard so the lock outlives the access");
    T* operator->() && noexcept = delete("`sv.lock()->f()` locks and unlocks in the same expression; name the guard so the lock outlives the access");

and extend the existing comment on lines 25-27, which already names the hazard, to say plainly that the deletion is a guard against the one-liner and not a proof — a reference or pointer taken out of a guard outlives the lock and the compiler will not say so (GCC 16 ignores `[[gnu::lifetimebound]]`, verified). For an educational library, naming the boundary of what the type enforces is worth more than the deletion itself.

Do NOT replace the API with `with(Operation)`: it breaks tests/test_synchronized_value.cpp:103-144 (which assert `can_lock`/`can_lock_shared` and the exact `value_guard` instantiations, including the shared_lock-vs-unique_lock selection that is the type's whole teaching point), it has no const/shared-read overload, and it still leaks by capture — verified compiling.

### Reproduction

```text
// probe_syncval_dangle.cpp
#include <threadsafe/threadsafe.h>
#include <vector>
#include <cstdio>
using namespace threadsafe;
using SV = synchronized_value<std::vector<int>>;

template <class Guard> auto& deref(const Guard& g) { return *g; }

int main() {
    SV sv;
    std::vector<int>& unguarded = deref(sv.lock());  // guard dies at the semicolon
    unguarded.push_back(1);                          // mutating with NO lock held
    auto reader = sv.lock_shared();                  // another thread may be in here too
    std::printf("size=%zu locked-view-size=%zu\n", unguarded.size(), reader->size());
    unguarded.push_back(2);                          // writing while a SHARED lock is held
    std::printf("after=%zu\n", reader->size());
}

$ g++-16 -std=c++26 -freflection -Wall -Wextra -Wdangling-reference -Wdangling-pointer \
    -Wreturn-local-addr -I<include> probe_syncval_dangle.cpp -o probe_syncval_dangle
(no diagnostics at all)
$ ./probe_syncval_dangle
size=1 locked-view-size=1
after=2

// A cast reaches the same place with no helper, in one statement:
std::vector<int>& escape(SV& sv) { return *static_cast<const SV::guard&>(sv.lock()); }
// and structured bindings ride through the same crack:
auto& [a, b] = *static_cast<const SP::guard&>(sp.lock());   // lock already released
// both compile clean under -Wall -Wextra.

// GCC 16 offers no assist:
$ g++-16 -std=c++26 -Wall -Wextra -Wdangling-reference -fsyntax-only probe_syncval_lb.cpp
probe_syncval_lb.cpp:4:63: warning: 'gnu::lifetimebound' scoped attribute directive ignored [-Wattributes]
```

### Vérification

Every mechanical claim reproduces on this exact code (synchronized_value.h:29-33, GCC 16.2.0).

1. The `&&` deletion only removes the overload when the implicit object argument is an rvalue. Binding the prvalue guard to a `const value_guard&` (helper parameter or `static_cast`) makes it an lvalue, so lines 32-33 are viable and non-deleted. `deref(sv.lock())` and `*static_cast<const SV::guard&>(sv.lock())` both compile with zero diagnostics at `-Wall -Wextra -Wdangling-reference -Wdangling-pointer -Wreturn-local-addr`, and the built binary mutates the protected vector with no lock held and again while a `shared_lock` is outstanding. Verified by compiling AND running.
2. `[[gnu::lifetimebound]]` is genuinely ignored by GCC 16.2.0 ("scoped attribute directive ignored [-Wattributes]"), so no compiler assist exists.
3. So the delete message — "so it cannot hand out a reference" — states a guarantee the type does not deliver. That part is simply true, and for a library whose thesis is compile-time safety, a message that promises more than it enforces is the defect worth fixing.

Two corrections that change the finding's shape (and its severity), which is why I rewrote the title and the fix:

A. The const&-binding is NOT the crack. The escape needs no cast and no helper at all — the ordinary, intended, named-guard form leaks just as freely:
   `{ auto held = sv.lock(); escaped = &*held; }  escaped->push_back(42);`
   compiles clean and races (verified, ran). Reference escape from an RAII guard is not preventable in C++ by any overload-set trick; it is a property of every guard API (std::lock_guard, folly::Synchronized, boost::synchronized_value). The `&&`-delete was only ever able to block the accidental one-liner `*sv.lock()`. The library's own comment on lines 25-27 already documents the hazard ("Don't capture by reference, because the lock is released when the guard is destroyed"). So this is a wrong-promise-in-a-message defect, not a newly discovered hole in the guard design.

B. The trait engine is untouched. `is_sendable<value_guard>` and `is_lifetime_aware<value_guard>` are both false (lines 108-111), and the escaped `std::vector<int>&` is non-sendable since `is_sendable<T&> = is_synchronizable<T>`, so the escaped reference cannot be pushed to another thread through `asynchronous_task_launcher`. The race is still reachable (thread A holds the escaped ref, thread B takes the real lock), so this is not harmless — but it is a helper-API wording defect, not a case of a trait answering TRUE for an unsafe type. High severity is overstated; medium is right.

The proposed fix is not applicable as written and I would not ship it: (i) it replaces `lock()`/`lock_shared()`, which breaks the existing suite — tests/test_synchronized_value.cpp:103, 106, 131, 134, 144 assert `can_lock`/`can_lock_shared`, and 109-128 assert the exact `value_guard<...>` instantiations including the shared_lock-vs-unique_lock selection, the actual teaching point of the type; (ii) it has no const overload, so a `const synchronized_value` becomes unreadable and the whole `shared_mutex` story disappears; (iii) it does not close the hole — I compiled it, and `p.with([&](auto& v){ leaked = &v; }); leaked->push_back(2);` escapes with no diagnostic, exactly as the finding itself concedes. Trading a documented limitation for an undocumented one plus a broken test suite is not an improvement for an educational library.

## F27 — `copy_on_write<T>` is the only library wrapper given `is_sendable` without a matching `is_synchronizable<const …>` rule, so a shared-by-const `copy_on_write` — and any aggregate holding one — answers false, and `synchronized_value<copy_on_write<T>>` downgrades to `std::mutex`

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:53` |
| **Même défaut que** | `F25`, `F30` — copy_on_write sans règle is_synchronizable<const ...> |

### Le problème

`copy_on_write<T>` hands out `const T&` only and copies before any mutation, and `is_sendable<copy_on_write<T>>` already requires `is_synchronizable_v<const T>`. But no `is_synchronizable<const copy_on_write<T>>` specialization exists, so the const question falls to the structural walk, which rejects it on its own forwarding constructor template. The consequences are concrete: no aggregate containing a `copy_on_write` member can ever be const-synchronizable, and `synchronized_value<copy_on_write<T>>` selects a plain `std::mutex` where `synchronized_value<T>` selects a `shared_mutex` — the copy-on-write type gets *fewer* concurrent readers than the type it wraps. The message a user gets blames a constructor template rather than anything about sharing, so the answer is right-looking for the wrong reason.

### Le code concerné

```cpp
template <class T>
struct is_sendable<copy_on_write<T>>
    : std::bool_constant<detail::cow_is_sendable<T>()> {};

template <class T>
struct is_lifetime_aware<copy_on_write<T>> : is_lifetime_aware<T> {};
```

### La correction

```cpp
The proposed fix is correct as written; I compiled it and ran the full suite against it. Keep it as-is in `include/threadsafe/details/copy_on_write.h`, placed between the two existing specializations:

    template <class T>
    struct is_sendable<copy_on_write<T>>
        : std::bool_constant<detail::cow_is_sendable<T>()> {};

    // Reading a copy_on_write is reading the T it shares: the same question the
    // sendable rule already answers, since as_mutable() copies before it writes.
    template <class T>
    struct is_synchronizable<const copy_on_write<T>>
        : is_sendable<copy_on_write<T>> {};

    template <class T>
    struct is_lifetime_aware<copy_on_write<T>> : is_lifetime_aware<T> {};

Two notes worth keeping in the review record, since a reviewer will ask both:
- Deriving from `is_sendable<copy_on_write<T>>` (rather than from `is_synchronizable<const T>` alone) is not merely conservative — it is load-bearing. A thread that copies the shared const object may become the last owner and run `~T`, so `is_sendable_v<T>` is genuinely required, and `cow_is_sendable<T>()` already conjoins exactly `is_sendable_v<T> && is_synchronizable_v<const T>`.
- The fix leaves the non-const answer untouched: `!is_synchronizable_v<copy_on_write<int>>` still holds, so `test_copy_on_write.cpp:108` and its "belongs to one thread" intent are preserved.
```

### Reproduction

```text
// probe_constsync_cow_consequence.cpp
#include <threadsafe/threadsafe.h>
#include <shared_mutex>
#include <mutex>
#include <string>
using namespace threadsafe;
struct Document { copy_on_write<std::string> text; int version; };
static_assert(is_sendable_v<copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<const copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<const Document>);
static_assert(std::is_same_v<synchronized_value<copy_on_write<std::string>>::mutex, std::mutex>);
static_assert(std::is_same_v<synchronized_value<std::string>::mutex, std::shared_mutex>);
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include probe_constsync_cow_consequence.cpp
(no output -- all five hold)

The reason the user is shown (probe_constsync_diag_cow.cpp):
  int main() { threadsafe::assert_synchronizable<const threadsafe::copy_on_write<int>>(); }
  error: uncaught exception of type 'std::meta::exception'; 'what()': 'const threadsafe::copy_on_write<int> has a user-written copy, move or destructor — or a template that may be selected as one — which can share state the members do not show; specialize is_synchronizable to state the intent'

With the proposed specialization added in a user TU (probe_constsync_fixes.cpp), all of
  is_synchronizable_v<const copy_on_write<std::string>>, is_synchronizable_v<const Document>,
  !is_synchronizable_v<const copy_on_write<int*>>
compile clean.
```

### Vérification

I tried four refutation angles and all failed.

(1) **Does the repro hold on the real code?** Yes, verbatim. `copy_on_write.h` (60 lines) declares exactly two trait specializations — `is_sendable<copy_on_write<T>>` at line 54 and `is_lifetime_aware<copy_on_write<T>>` at line 58. There is no `is_synchronizable<const copy_on_write<T>>`, so `const copy_on_write<T>` falls to the generic `is_synchronizable<const T>` at `synchronizable.h:38` → `detail::default_is_const_synchronizable`, which hits `has_only_default_copy_move_destroy` → `may_hijack_copy_move` → `is_constructor_template` on the line-18 forwarding constructor and rejects. All five static_asserts in the repro compile clean, including both consequences (`const Document` false; `synchronized_value<copy_on_write<std::string>>::mutex == std::mutex` while `synchronized_value<std::string>::mutex == std::shared_mutex`). The quoted diagnostic reproduces word-for-word.

(2) **Is the "should be true" half actually true — is a shared `const copy_on_write<T>` really safe?** Yes. The only operations reachable through `const` are `operator*`, `operator->` (both `const T&`/`const T*`) and the implicit copy constructor; `as_mutable()` is non-const. Concurrent copies of the same `const std::shared_ptr` lvalue are thread-safe, and no one can write `ptr_`. A thread that copies and then calls `as_mutable()` sees `use_count() != 1` and copies first. `T` can be destroyed on a thread other than its creator (a copy may become the last owner), which is exactly why the rule must also demand `is_sendable_v<T>` — and reusing `is_sendable<copy_on_write<T>>` supplies both halves.

(3) **Is the gap intentional?** `test_copy_on_write.cpp:108` asserts `!is_synchronizable_v<cow<int>>` with "copy_on_write object belongs to one thread; share by copying it" — but that is the *non-const* question, which the fix leaves false (verified). And the library's own pattern refutes intent: `smart_pointers.h` pairs `is_sendable<X>` with `is_synchronizable<const X>` for `unique_ptr`, `shared_ptr`, `weak_ptr`, `reference_wrapper`, `default_delete`; `allowed_std_wrappers.h:125` does the same for every blessed std wrapper; `vocabulary.h` for `allocator`/`stop_token`/`stop_source`. `copy_on_write` — the library's own share-by-const type — is the single wrapper that has the sendable half and not the const half.

(4) **Would the fix break anything?** No. I copied the include tree, applied the proposed specialization verbatim, and compiled all 11 test files: all OK. No partial-ordering ambiguity with `is_synchronizable<const T>` (more specialized) or with the `std_wrapper`-constrained one (`copy_on_write` is not a `std_wrapper`).

The finding survives.

## F28 — assert_ownable_by_launcher's move_constructible branch is dead for callables that are neither copyable nor movable (by-value parameter fails first), and the "use std::ref" advice it prints in the cases it does reach is valid only for launch_scoped_task, not launch_task

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:41` |

### Le problème

assert_ownable_by_launcher exists to catch a non-movable callable, and the test file documents SyncCounter (an atomic member, so neither copyable nor movable) as that case. It never runs: launch_task's parameter is `F f` by value, so copy-initialising it fails during overload resolution and the user gets bare "use of deleted function 'SyncCounter::SyncCounter(const SyncCounter&)'". The message only fires for the rarer copy-yes/move-deleted shape — and there the advice is itself wrong for launch_task, because std::reference_wrapper is never lifetime_aware, so following it produces a second rejection.

### Le code concerné

```cpp
if (!std::move_constructible<F>)
        throw std::meta::exception(
            u8"the launcher owns its callable, so a non-movable one cannot "
            u8"cross; share it with std::ref instead",
            ^^F);
```

### La correction

```cpp
Two independent defects; fix them separately.

(a) The advice must stop being shared between the two entry points. `assert_ownable_by_launcher` is called from both `explain_launch_task` and `explain_launch_scoped_task`, so it cannot name a remedy that is only valid for one of them. Pass the remedy in, or inline the check into each explainer:

    template <class F, class... Args>
    consteval void assert_ownable_by_launcher(std::u8string_view remedy) {
        if (!std::move_constructible<F>)
            throw std::meta::exception(
                u8"the launcher owns its callable, so a non-movable one cannot "
                u8"cross; " + remedy, ^^F);
        ...
    }

with, from `explain_launch_scoped_task`, `u8"share it with std::ref instead"` (verified: `launch_scoped_task(std::ref(c))` compiles), and from `explain_launch_task`, something that is actually reachable — NOT std::ref and NOT a bare shared_ptr:

    u8"give the task a movable callable that owns the object — a struct with a "
    u8"std::shared_ptr member and an operator(), since a capturing lambda is "
    u8"not sendable either"

I compiled that shape and it is accepted:

    struct OwningTask {
        std::shared_ptr<SyncCounter> owned;
        void operator()() const { (*owned)(); }
    };
    launcher.launch_task(OwningTask{std::make_shared<SyncCounter>()});   // OK

Do NOT use the originally proposed "hold it in a std::shared_ptr and send that" — passing the shared_ptr itself as the callable passes `launchable_task` and then fails inside libstdc++ ("std::jthread arguments must be invocable after conversion to rvalues"), and wrapping it in a lambda is rejected as a capturing closure.

(b) Reachability. Keeping the by-value parameters (they are the point of the lesson), the `!std::move_constructible<F>` branch can never fire for a type that is also non-copyable, so for that family the library has no message at all. Either accept that and drop the dead branch — leaving `ownable_by_launcher` as a pure concept-level gate and letting the compiler's own "use of deleted function" note (which already points at `launch_task(F, Args...)`) be the diagnostic — or, if the message is wanted, have the fallback overload take the pack by forwarding reference so its body is reachable while the *constrained* overload keeps by-value ownership:

    template <typename F, typename... Args>
    void launch_task(F&&, Args&&...) { detail::explain_launch_task<std::decay_t<F>, std::decay_t<Args>...>(); }

The constrained `launch_task(F f, Args... args)` is unchanged and still wins by subsumption whenever it applies, so the by-value teaching signature survives and the fallback becomes reachable for the SyncCounter case.

Either way, also fix the misleading assert string in tests/test_asynchronous_task_launcher.cpp on `!can_launch_task<SyncCounter>` — it says "share it with std::ref instead" four lines below a static_assert proving std::ref does not work for launch_task. Note the test suite only evaluates the concepts, never calls the launcher, so none of these changes can break it.
```

### Reproduction

```text
// probe_launcher_nonmovable.cpp -- the case tests/test_asynchronous_task_launcher.cpp documents
struct SyncCounter { std::atomic<int> counter{0}; void operator()() const {} };
template <> struct threadsafe::is_synchronizable<SyncCounter> : std::true_type {};
int main() { threadsafe::asynchronous_task_launcher launcher; SyncCounter c; launcher.launch_task(c); }

$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_launcher_nonmovable.cpp
error: use of deleted function 'SyncCounter::SyncCounter(const SyncCounter&)'
// no threadsafe message at all

// probe_launcher_ownable_reach.cpp -- copy-yes / move-deleted DOES reach it:
'what()': 'the launcher owns its callable, so a non-movable one cannot cross; share it with std::ref instead'

// probe_launcher_advice_loop.cpp -- and following that advice fails again:
launcher.launch_task(std::ref(counter));
'what()': 'std::reference_wrapper<SyncCounter>::_M_data (SyncCounter*) is a reference or a raw
           pointer: it borrows its referent instead of keeping it alive — hold the object,
           or a std::shared_ptr to it'
```

### Vérification

I tried to break the finding on four fronts and every one of them confirmed it instead.

(1) Reachability. `launch_task(F f, Args... args)` — both the constrained overload and the explaining fallback — takes the callable **by value**. For the case the test file itself documents (`SyncCounter`, an `std::atomic<int>` member, so copy and move both deleted), the constrained overload drops out on `move_constructible<F>`, the fallback is selected, and then argument copy-initialisation is ill-formed *before* the body is instantiated. GCC emits only `error: use of deleted function 'SyncCounter::SyncCounter(const SyncCounter&)'` plus a `note: initializing argument 1 of ... launch_task(F, Args...)`. `explain_launch_task` is never instantiated, so `assert_ownable_by_launcher`'s message never appears. Confirmed identically for `launch_scoped_task`. So the `!std::move_constructible<F>` branch is dead for the whole copy-deleted-and-move-deleted family, which is exactly the family the reader is shown.

(2) It fires only for copy-yes/move-deleted. A `CopyOnly` struct (`CopyOnly(CopyOnly&&) = delete`, copy defaulted) does reach it, because the by-value parameter copy-initialises fine and only the concept rejects. Message printed verbatim.

(3) The advice is wrong for `launch_task`. `launcher.launch_task(std::ref(c))` is rejected a second time with `std::reference_wrapper<SyncCounter>::_M_data ... borrows its referent instead of keeping it alive`. That is not incidental: `tests/test_asynchronous_task_launcher.cpp` contains, four lines apart, `!can_launch_task<std::reference_wrapper<SyncCounter>>` ("the callable must keep itself alive too, and a reference_wrapper does not") and then `!can_launch_task<SyncCounter>` whose own assert string repeats "share it with std::ref instead". The educational material contradicts itself on adjacent lines. The message is correct only for `launch_scoped_task` — I verified `launcher.launch_scoped_task(std::ref(c))` compiles clean — but `assert_ownable_by_launcher` is shared by both entry points and cannot tell which one the user called.

The one thing I did refute is the **proposed fix**. Its replacement text says "hold it in a std::shared_ptr and send that", which does not work for a *callable*: `launch_task(std::make_shared<SyncCounter>())` passes `launchable_task` (the concept never checks invocability) and then dies inside libstdc++ with `static assertion failed: std::jthread arguments must be invocable after conversion to rvalues`. Wrapping it in a capturing lambda fails too — `launch_task([owned]{ (*owned)(); })` is rejected with "holds state reflection cannot see (a closure type with captures)". The only shape that actually works is a hand-written struct with a `shared_ptr` member and an `operator()`, which I compiled clean. So the finding stands but its fix must be rewritten.

Also worth flagging for the fix's structural note: switching to `F&&` makes the diagnostic reachable, but it trades away the by-value ownership that the header's own comment ("owning them is what lets it hand them to a thread") is there to teach, and it does not make the operation possible — `jthread` decay-copies regardless, so a non-movable callable is still unusable. The cheaper repair is to keep the by-value signature and delete the dead branch, moving the ownership requirement into the message that actually fires.

## F29 — synchronized_value's sendable failure names only T, not the offending member, though the library's own assert_sendable already prints the member path (copy_on_write half of the original finding is not a defect)

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | probable |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:48` |

### Le problème

synchronized_value is the first thing a user reaches for, so its failure message is the one most people will read. It uses a plain static_assert on the concept, which yields "T must be sendable" plus GCC's "evaluated to false" — no indication of which subobject is at fault. The library already owns assert_sendable, which prints the full member path down to the culprit. The two messages for the same type are dramatically different in usefulness. copy_on_write has no check at all, so an unsafe T there is not diagnosed anywhere.

### Le code concerné

```cpp
class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");
```

### La correction

```cpp
Add the diagnostic assert alongside the existing one rather than replacing it, so both the "why" sentence and the member path are printed:

class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");
    static_assert((assert_sendable<T>(), true));

Do NOT apply the originally proposed single-assert replacement: it discards the explanatory message string. Do NOT add any check to copy_on_write — a non-sendable T there is legal single-threaded use and the existing tests (tests/test_copy_on_write.cpp) require it to compile.
```

### Reproduction

```text
// probe_api_sv_diag.cpp
#include <threadsafe/threadsafe.h>
namespace app {
struct Leaf { int* borrowed; };
struct Middle { Leaf leaf; };
struct Session { Middle middle; int id; };
}
threadsafe::synchronized_value<app::Session> session{};

WHAT THE USER GETS TODAY:
  synchronized_value.h:48:19: error: static assertion failed: the mutex serializes
  access, but the T still crosses thread boundaries ... so T must be sendable
    • the expression 'is_sendable_v<T> [with T = app::Session]' evaluated to 'false'

WHAT assert_sendable<app::Session>() ALREADY PRINTS:
  'app::Session::middle (app::Middle)::leaf (app::Leaf)::borrowed (int*) is a pointer
   or a reference: sending it shares its referent with the other thread, so the
   referent must be synchronizable — and synchronizability is opt-in'
```

### Vérification

I tried to refute this three ways and only the copy_on_write half of it fell.

1. Repro is exact. `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/synchronized_value.h:48` is a plain `static_assert(sendable<T>, "...")`. Compiling the claimed repro reproduces the reported output verbatim: the type name `app::Session` and "evaluated to 'false'", nothing about `middle::leaf::borrowed`.

2. Design-intent defence fails. CLAUDE.md says nothing about diagnostics, and no comment in the tree documents a deliberate choice here. The only cost comment that exists (sendable.h:77) argues the opposite way: the deep walk is skipped in the *trait* because "walking every subobject a second time to word a message nobody reads would make each 'false' answer quadratic" — `assert_sendable<T>()` returns immediately when `is_sendable_v<T>` is true, so calling it from a helper costs nothing on the passing path. I confirmed this: `both_sv<int>` compiles clean. The library itself already routes user-supplied types through `assert_sendable` in asynchronous_task_launcher.h:60-73, so the inconsistency is internal, not a stated trade-off.

3. No test breakage either way. tests/test_synchronized_value.cpp:48 writes `!is_synchronizable_v<synchronized_value<NonSendable>>`, which goes through the `is_synchronizable<synchronized_value<T>>` specialization and never instantiates the class body — the existing static_assert already proves that (it would fire today otherwise).

What I *did* refute: (a) the copy_on_write sub-claim. `copy_on_write<T>` with a non-sendable T is legal single-threaded code by design, and the suite depends on it — tests/test_copy_on_write.cpp asserts `!is_sendable_v<cow<NonSendable>>`, `!is_sendable_v<cow<Cache>>`, etc. Adding a check there would reject correct programs; the trait answering false at the thread boundary is the right place. (b) The proposed fix as written. Replacing the condition with `(assert_sendable<T>(), true)` makes GCC print "non-constant condition for static assertion" plus `what()` and *drop the assert's own message string* entirely — the "the mutex serializes access, but the T still crosses thread boundaries" sentence, which is the pedagogical half, disappears. For a library built to be shown at a conference that is a regression. Keeping both asserts prints both diagnostics (verified), so that is the fix that survives.

Net: the usability gap is real and verified, but it is narrower and lower-severity than reported — a diagnostic-quality inconsistency on one helper, not a two-helper problem, and the proposed patch needs correcting before it is applied.

## F30 — A copy_on_write member poisons its owner's const-synchronizability, so nested COW is not sendable and synchronized_value downgrades to a plain mutex — not "const cow& cannot cross a thread", which the reference rule blocks regardless

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | probable |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:18` |
| **Même défaut que** | `F25`, `F27` — copy_on_write sans règle is_synchronizable<const ...> |

### Le problème

copy_on_write declares a variadic constructor template, which detail::may_hijack_copy_move (utils.h:133-137) flags, so has_only_default_copy_move_destroy rejects the type and the structural const walk answers false. The consequence is that `const copy_on_write<T>&` cannot cross a thread boundary and a copy_on_write member makes its enclosing type non-const-synchronizable, even though reading through operator*/operator-> is exactly the safe, race-free path the class was designed for. Users are pushed to the one-copy-per-thread route via is_sendable, which is also the route that makes the escaped-reference race of finding 1 reachable.

### Le code concerné

```cpp
template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}
```

### La correction

// Same condition as the is_sendable rule right above it: reading a shared block
// through const is what the class guarantees, but a thread that holds the const
// handle can copy it and become the owner that destroys (or copy-constructs) the T,
// so T must be sendable too. Placed in copy_on_write.h next to the is_sendable rule.
template <class T>
struct is_synchronizable<const copy_on_write<T>>
    : std::bool_constant<detail::cow_is_sendable<T>()> {};

// NOT `: is_synchronizable<const T>` as proposed — that answers true for
// const copy_on_write<ThreadBound> where is_sendable<ThreadBound> is false.
// Note this leaves is_synchronizable<copy_on_write<T>> (unqualified) false, which
// is correct and is what test_copy_on_write.cpp already asserts: as_mutable rebinds
// the handle. And it does not make `const copy_on_write<T>&` sendable — sendable.h:27
// strips cv from a reference and asks the unqualified trait, by design.

### Reproduction

```text
// probe_rt_traits.cpp
#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Payload { int v = 0; };
static_assert(!is_synchronizable_v<copy_on_write<Payload>>);
static_assert(!is_synchronizable_v<const copy_on_write<Payload>>);  // <-- holds
static_assert(is_synchronizable_v<synchronized_value<Payload>>);
static_assert(is_synchronizable_v<const synchronized_value<Payload>>);
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_rt_traits.cpp
ALL TRAIT ASSERTIONS HOLD

synchronized_value escapes the same trap only because its unqualified
is_synchronizable specialization (synchronized_value.h:103) short-circuits the
const walk at synchronizable.h:110.
```

### Vérification

The bare fact survives, but the diagnosis, the stated consequence, and the fix do not.

WHAT SURVIVES. `is_synchronizable_v<const copy_on_write<T>>` is false, and the cause named is the first one hit: `assert_synchronizable<const copy_on_write<Payload>>()` throws "has a user-written copy, move or destructor — or a template that may be selected as one", i.e. `may_hijack_copy_move` firing on the variadic constructor template at copy_on_write.h:18. And a `const cow<T>` really is safe to read from several threads when T is sendable and const-readable (operator*/operator-> hand out only `const T&`, `ptr_` is never written through const, copying the handle from a const lvalue is refcount-atomic), so this is a genuine false negative.

WHAT I REFUTED.
(1) The cause is not the only blocker. `is_synchronizable<const std::shared_ptr<T>> : is_synchronizable<T>` (smart_pointers.h) asks the *unqualified*, opt-in trait of the pointee, so even with the constructor template written out as explicit special members the walk would still answer false for the `shared_ptr<T>` member. Only an explicit specialization can fix this; loosening the hijack guard cannot.
(2) The headline consequence is wrong, and the proposed fix does not deliver it. `is_sendable<T&> : is_synchronizable<std::remove_cv_t<T>>` (sendable.h:27) strips the const and asks the unqualified trait, so `const copy_on_write<T>&` stays non-sendable *with the fix applied* — I compiled `static_assert(!is_sendable_v<const copy_on_write<Payload>&>)` on top of the proposed specialization and it holds. That is the library's documented reference rule (CLAUDE.md: `is_sendable<T&> = is_synchronizable<T>`) and `const std::string&` behaves identically; it is not a copy_on_write gap.
(3) "The one use COW exists for" is contradicted by the library's own tests: test_copy_on_write.cpp asserts `!is_synchronizable_v<cow<int>>` with the rationale "one copy_on_write object belongs to one thread; share by copying it", and the launch_task test passes `cow<std::string>` BY VALUE as "the point of the type". Copy-the-handle is the designed route and it works.
(4) The proposed fix is unsound. `is_synchronizable<const copy_on_write<T>> : is_synchronizable<const T>` drops the `is_sendable<T>` half that `detail::cow_is_sendable` keeps. A thread holding a `const cow<T>&` can copy the handle, then owns the block: it may destroy T (last handle) or copy-construct T in `as_mutable()`. With the fix, `is_synchronizable_v<const copy_on_write<ThreadBound>>` is true while `is_sendable_v<copy_on_write<ThreadBound>>` is false — sharing would grant strictly more than sending, for a type the library was told is thread-pinned.

WHAT THE REAL BITE IS. Not references: containment. A `copy_on_write` member makes its owner non-const-synchronizable, so `copy_on_write<Config>` with `struct Config { copy_on_write<Payload> settings; }` is NOT sendable (cow_is_sendable needs `is_synchronizable_v<const Config>`) — nested COW does not compose — and `synchronized_value<Config>` silently downgrades from `std::shared_mutex` to `std::mutex`. Both verified by compilation, both fixed by the corrected specialization, which I compiled against all 11 test files with no failures.

Low severity and mis-stated, but the underlying false negative is real.

## F31 — Neither launchable_task nor launchable_scoped_task checks invocability, so an arity mistake escapes into 49 (resp. 38) lines of libstdc++ <thread> internals

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:23` |

### Le problème

launchable_task checks movability, sendability and lifetime, but never that F is callable with Args. The most common first-timer mistake — wrong number or type of arguments — therefore falls straight through the constrained overload into std::jthread's own static_assert, producing 49 lines of <thread> internals. The library invested heavily in explain_launch_task specifically so users would never see that; one missing conjunct undoes it for the most likely error.

### Le code concerné

```cpp
template <class F, class... Args>
concept launchable_task = ownable_by_launcher<F, Args...>
                       && sendable<F>
                       && lifetime_aware<F>
                       && (sendable<Args> && ...)
                       && (lifetime_aware<Args> && ...);
```

### La correction

```cpp
The proposed fix is directionally right but incomplete: it patches only launchable_task, leaving launch_scoped_task (which builds a std::jthread directly at asynchronous_task_launcher.h:108) with the same hole — 38 lines of <thread> internals for the same mistake. Both concepts need the conjunct. Putting invocability first also keeps assert_* in the "reading order" the detail:: comment promises, rather than smuggling it into assert_ownable_by_launcher whose name no longer covers it.

In include/threadsafe/details/asynchronous_task_launcher.h, after ownable_by_launcher:

    // std::jthread may prepend a stop_token, so either shape is a real call.
    template <class F, class... Args>
    concept invocable_by_launcher =
        std::invocable<F, Args...> || std::invocable<F, std::stop_token, Args...>;

    template <class F, class... Args>
    concept launchable_task = invocable_by_launcher<F, Args...>
                           && ownable_by_launcher<F, Args...>
                           && sendable<F>
                           && lifetime_aware<F>
                           && (sendable<Args> && ...)
                           && (lifetime_aware<Args> && ...);

    template <class F, class... Args>
    concept launchable_scoped_task = invocable_by_launcher<F, Args...>
                                  && ownable_by_launcher<F, Args...>
                                  && sendable<F>
                                  && (sendable<Args> && ...);

and in namespace detail, a matching assertion called first by both explain_* functions:

    template <class F, class... Args>
    consteval void assert_invocable_by_launcher() {
        if (!invocable_by_launcher<F, Args...>)
            throw std::meta::exception(
                u8"the callable cannot be invoked with these arguments; note that "
                u8"std::jthread may also prepend a std::stop_token",
                ^^F);
    }

    // explain_launch_task():        assert_invocable_by_launcher<F, Args...>(); first
    // explain_launch_scoped_task(): assert_invocable_by_launcher<F, Args...>(); first

Verified: all 11 files in tests/ still compile; the launch_task probe drops 49 -> 12 lines and the launch_scoped_task probe 38 -> 12, both naming the library's own message. Cost is ~13 lines in a 121-line educational header, which is the real trade-off to weigh against Task.md's simplicity requirement — the finding is correctly rated low severity.
```

### Reproduction

```text
// probe_api_arity.cpp
#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](int a, int b) { (void)a; (void)b; }, 1);
}

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_arity.cpp 2>&1 | wc -l
49
$ ... | grep error:
/opt/homebrew/.../c++/16/thread:274:27: error: static assertion failed:
  std::jthread arguments must be invocable after conversion to rvalues
  • error: no match for call to '(main()::<lambda(int, int)>) (int)'
```

### Vérification

The repro reproduces byte-for-byte: 49 lines, `thread:274:27: error: static assertion failed: std::jthread arguments must be invocable after conversion to rvalues` plus `no match for call to '(main()::<lambda(int, int)>) (int)'`. 26 of the 49 lines are libstdc++ instantiation frames (_S_create / construct_at / alloc_traits / vector.tcc) that precede the user's own source line — exactly the output explain_launch_task was built to prevent.

I tried four refutations and none held.

(1) "launchable_task is deliberately a safety-only gate; invocability belongs to jthread." Refuted by the code itself: launchable_task already conjoins ownable_by_launcher (std::move_constructible), a plain-C++ precondition with zero thread-safety content, and assert_ownable_by_launcher gives it a bespoke u8 message. The separation the concept would need to justify the omission is already not observed. Nothing in CLAUDE.md, Task.md or the headers documents invocability as delegated; the only "states safety, not invocability" remark (tests/test_sendable.cpp:239) is about the is_sendable trait, not the launcher's API concept.

(2) "The fix breaks the suite." It does not. I copied the include tree, applied the conjunct plus the diagnostic throw, and compiled all 11 files in tests/ with g++-16 -std=c++26 -freflection -fsyntax-only: all 11 OK. Every negative assertion (!can_launch_task<...>) stays false under an added conjunct, and each positive one is genuinely invocable.

(3) "The fix false-rejects valid calls." It does not. Because launch_task(F f, Args... args) takes by value, F and Args are already decayed, so std::invocable<F, Args...> agrees exactly with jthread's is_invocable_v<decay_t<_Callable>, decay_t<_Args>...>, and the `|| std::invocable<F, std::stop_token, Args...>` arm covers jthread's prepended-token form. I compiled a regression probe with [](std::stop_token){}, [](std::stop_token,int){}, [](auto&&...){}, [](int&&){} and launch_scoped_task([](std::stop_token){}): 0 errors both before and after the patch.

(4) "GCC's message is already adequate." Partially true — it does end with `candidate expects 2 arguments, 1 provided` — but only after the 26-frame backtrace. This justifies the low severity, not a refutation.

One genuine defect in the proposed fix, which I corrected: it patches only launchable_task. launch_scoped_task constructs std::jthread directly at asynchronous_task_launcher.h:108 and has the identical hole — I measured 38 lines of <thread> internals for the same arity mistake. With the conjunct added to both concepts, both cases drop to 12 lines carrying the library's own std::meta::exception, and all 11 test files still pass.

---

# Thread safety

Data races et race conditions dans le code d'exécution de la bibliothèque elle-même.

## F32 — asynchronous_task_launcher has no destructor, so ~vector<jthread> stops and joins tasks one at a time: shutdown latency is the sum of the tasks' stop latencies, and it hangs whenever an earlier-launched task's exit depends on a later-launched task being stopped

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | thread-safety |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:118` |

### Le problème

Destroying std::vector<std::jthread> destroys the elements one after another, and each ~jthread performs request_stop() followed by join(). Thread k is therefore fully joined before thread k+1 is even asked to stop. Two consequences, both measured. First, latency: shutting down N tasks costs N x (time to notice a stop) instead of one such interval. Second, and worse, it deadlocks: any task whose completion depends on a *later* task having observed its own stop request will never complete, because that later stop request is issued only after the earlier join returns. This is the standard producer/coordinator shape, and the user wrote nothing wrong — every reasonable stop-source-based pool broadcasts stop before joining anything. Since the destructor is implicit there is nothing in the source that even hints at the ordering.

### Le code concerné

```cpp
private:
    std::vector<std::jthread> threads_;
```

### La correction

```cpp
Broadcast the stop request to every task before any join, and keep the class movable — a user-declared destructor otherwise suppresses the implicit move operations, and `std::is_move_constructible_v` keeps reporting true while an actual move becomes a deep std::vector-of-noncopyable template error:

    asynchronous_task_launcher() = default;
    asynchronous_task_launcher(asynchronous_task_launcher&&) = default;
    asynchronous_task_launcher& operator=(asynchronous_task_launcher&&) = default;

    // Broadcast the stop request before any join, so no task waits on a peer
    // that has not been asked to stop yet. ~vector then joins threads that are
    // already on their way out.
    ~asynchronous_task_launcher() {
        for (std::jthread& task : threads_)
            task.request_stop();
    }

private:
    std::vector<std::jthread> threads_;

Verified: all 11 files in tests/ compile clean with -std=c++26 -freflection -fsyntax-only, `launcher b = std::move(a);` still compiles (it does NOT under the originally proposed destructor-only patch), the latency repro drops from 2016 ms to 510 ms, and the deadlock repro exits normally.
```

### Reproduction

```text
// (a) latency -- probe_rt_serial_shutdown.cpp
struct SlowToNoticeStop {   // polls its stop token every 500 ms
    int index;
    void operator()(std::stop_token stop) const {
        while (!stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::printf("task %d stopped\n", index);
    }
};
int main() {
    const auto start = std::chrono::steady_clock::now();
    { asynchronous_task_launcher launcher;
      for (int i = 0; i < 4; ++i) launcher.launch_task(SlowToNoticeStop{i});
      std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    /* print elapsed */
}
$ ./probe_rt_serial_shutdown
task 0 stopped
task 1 stopped
task 2 stopped
task 3 stopped
shutdown of 4 tasks took 2039 ms      <-- 4 x 500 ms, not 500 ms

// (b) deadlock -- probe_rt_shutdown_deadlock.cpp
using shared_flag = std::shared_ptr<std::atomic<bool>>;
struct Coordinator { shared_flag worker_finished;
    void operator()(std::stop_token) const {
        while (!worker_finished->load())
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
struct Worker { shared_flag worker_finished;
    void operator()(std::stop_token stop) const {
        while (!stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        worker_finished->store(true); } };
static_assert(is_sendable_v<Coordinator> && is_lifetime_aware_v<Coordinator>);
static_assert(is_sendable_v<Worker> && is_lifetime_aware_v<Worker>);
int main() {
    auto flag = std::make_shared<std::atomic<bool>>(false);
    { asynchronous_task_launcher launcher;
      launcher.launch_task(Coordinator{flag});
      launcher.launch_task(Worker{flag}); }
    std::printf("left scope\n");
}
$ ./probe_rt_shutdown_deadlock
entering scope
>>> STILL RUNNING AFTER 4s -> DESTRUCTOR DEADLOCK CONFIRMED

Same program with the proposed destructor body (probe_rt_shutdown_fix.cpp):
worker finished
coordinator saw the worker finish
left scope
>>> exited normally: broadcasting stop first fixes it
```

### Vérification

I read the real file: asynchronous_task_launcher.h:118 is `std::vector<std::jthread> threads_;` inside a class with no user-declared destructor, so shutdown runs ~vector, which destroys elements in order, and each ~jthread does request_stop() then join(). Thread k is fully joined before thread k+1 is asked to stop. Both claimed consequences reproduce on this machine with g++-16: 4 tasks polling their stop token every 500 ms take 2016 ms to shut down (not ~500 ms), and a coordinator/worker pair where the first-launched task completes only once the second-launched task observes its stop request hangs indefinitely (still running after 4 s). Both callables satisfy is_sendable_v and is_lifetime_aware_v, so the library accepts the program without complaint. Broadcasting stop before any join fixes both: the deadlock repro exits normally and the latency drops to 505 ms.

The refutation lens I was asked to apply is design intent, and it fails. CLAUDE.md does not mention asynchronous_task_launcher at all, and certainly documents no shutdown-ordering trade-off; the only precondition spelled out in the header is the borrow-vs-join note on launch_scoped_task (lines 101-104). The destructor is implicit, so there is no expression of a deliberate choice anywhere in the source. There is also no plausible reading in which serial stop-then-join is the intended teaching point: nothing names it.

Two honest qualifications, neither of which refutes the finding. First, this is not a soundness hole in the audit's sense (the traits do not answer TRUE for an unsafe type; the trait model is untouched) — it is a liveness defect in a shipped helper, which still lands on the thread-safety axis. Second, the proposed fix as written is a silent regression: adding a user-declared destructor suppresses the implicit move operations, and since std::vector's copy constructor is declared-but-ill-formed-on-instantiation for jthread, std::is_move_constructible_v<asynchronous_task_launcher> keeps reporting true while an actual move turns into a deep stl_construct.h error. I verified that `launcher b = std::move(a);` compiles on baseline and fails under the proposed patch. Defaulting the move operations alongside the destructor restores movability; with that version all 11 test files compile clean, the move probe compiles, and both runtime repros are fixed.

## F33 — launch_scoped_task joins its jthread before anything can request the stop, so a stop_token-taking callable — one both launchable_scoped_task and the class's own static_assert accept, and that launch_task cancels correctly — deadlocks the caller forever

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | thread-safety |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:108` |

### Le problème

launch_scoped_task builds a std::jthread, which injects a stop_token whenever F is invocable with one, and then calls join() unconditionally. request_stop() only ever runs in ~jthread, which is reached after join() has already returned — so the stop request is never delivered and the join never completes. launch_task, on the identical callable, works: its jthreads live in threads_ and ~jthread stops before joining. Both entry points accept the callable (launchable_task and launchable_scoped_task both hold), and the class-level static_assert on line 82 explicitly certifies stop_token as an injected argument, so nothing warns the user that only one of the two launchers honours it. The two members disagree about the contract of the very argument the class asserts about.

### Le code concerné

```cpp
template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }
```

### La correction

```cpp
A launcher that joins unconditionally has no stop protocol to offer, so it must not inject a token it can never trigger. Use std::thread, which performs no stop_token injection — but add the invocability requirement to `launchable_scoped_task` ONLY, so the rejection names the callable instead of unwinding inside <thread>:

    template <class F, class... Args>
    concept launchable_scoped_task = ownable_by_launcher<F, Args...>
                                  && std::invocable<F&, Args&...>
                                  && sendable<F>
                                  && (sendable<Args> && ...);

    // launch_scoped_task joins, so it injects no stop_token; the callable must
    // be invocable with exactly the arguments handed to it.
    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::thread task{std::move(f), std::move(args)...};
        task.join();
    }

and mirror it in detail::explain_launch_scoped_task, in reading order before assert_sendable<F>(), so the house diagnostic style is preserved:

    if (!std::invocable<F&, Args&...>)
        throw std::meta::exception(
            u8"launch_scoped_task joins, so it injects no stop_token; the "
            u8"callable must be invocable with exactly these arguments", ^^F);

Do NOT add std::invocable to `launchable_task`: its jthread genuinely injects a token, and the constraint would flip the deliberate `can_launch_task<decltype([](std::stop_token) {})>` assertion at tests/test_soundness_regressions.cpp:188 to false. Existing scoped-task assertions all use matching arity (SyncCounter has `operator()() const`, so `std::reference_wrapper<SyncCounter>` stays invocable), so no scoped test changes meaning.

Rejected alternative: deleting `task.join()` and letting ~jthread do request_stop()+join() also removes the hang, but it cancels the task the instant it starts — a silent semantic change, worse for an educational library than an explicit rejection.
```

### Reproduction

```text
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stop_token>
#include <thread>

std::atomic<bool> entered{false};
struct Worker {
    void operator()(std::stop_token stop) const {
        entered = true;
        while (!stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::printf("worker saw the stop request\n");
    }
};
static_assert(threadsafe::launchable_task<Worker>);
static_assert(threadsafe::launchable_scoped_task<Worker>);   // both accept it

int main() {
    { threadsafe::asynchronous_task_launcher launcher;
      launcher.launch_task(Worker{});
      while (!entered) std::this_thread::yield(); }
    std::printf("launch_task returned\n");

    entered = false;
    threadsafe::asynchronous_task_launcher launcher;
    std::printf("calling launch_scoped_task ...\n"); std::fflush(stdout);
    launcher.launch_scoped_task(Worker{});
    std::printf("launch_scoped_task returned (never reached)\n");
}

$ g++-16 -std=c++26 -freflection -I<inc> probe_interact_scoped_stop.cpp -o probe_interact_scoped_stop
$ ./probe_interact_scoped_stop      # SIGKILLed after a 4 s watchdog
worker saw the stop request
launch_task returned
calling launch_scoped_task ...
exit=137        <- hung; the third printf is never reached
```

### Vérification

I tried to refute this three ways and it survived all of them.

1. Does the callable really get through? Yes. `launchable_scoped_task<Worker>` and `launchable_task<Worker>` both hold for `struct Worker { void operator()(std::stop_token) const; }` — I compiled both static_asserts against the real headers and they passed. Neither concept carries an invocability requirement (asynchronous_task_launcher.h:22-32), so arity mismatch is never a filter.

2. Is the deadlock real or just theoretical? Real, and unbounded. `std::jthread task{std::move(f)}` (line 108) injects a token from the jthread's *internal* stop_source; the only handle to it is `task`, a local. `task.join()` on line 109 blocks until the callable returns, and the callable returns only when stop is requested. `~jthread` does request_stop() then join(), but it runs after join() has already returned, so the request can never arrive first. No third party can reach that stop_source. I ran the binary under a 5 s watchdog: `worker saw the stop request / launch_task returned / calling launch_scoped_task ...` then exit=137 — killed, the fourth printf never printed.

3. Is it the user's fault, or out of contract? No. The class-level static_assert at line 82 exists *specifically* to certify that the jthread-injected `stop_token` satisfies the traits on its own, and tests/test_soundness_regressions.cpp:188 blesses `can_launch_task<decltype([](std::stop_token) {})>`. The library therefore advertises stop_token injection as a supported shape. The only comment on launch_scoped_task (lines 101-104) is about borrows outliving the invocation, not about cancellation. Nothing anywhere says the scoped launcher does not honour a stop_token — and the two members genuinely disagree about it: the same callable is cancelled correctly by launch_task (via threads_' ~jthread) and hangs forever in launch_scoped_task.

The one correction I'd make is to the fix, not the finding. Switching to std::thread is right — a launcher that joins unconditionally has no stop protocol and must not inject a token — but on its own it turns the rejection into a raw error inside <thread>. The invocability constraint must be added to `launchable_scoped_task` only: adding it to `launchable_task` would flip test_soundness_regressions.cpp:188 to false, since a `void(std::stop_token)` callable is deliberately *not* invocable with zero args there. I verified the guarded version diagnoses cleanly. A one-line alternative (delete `task.join()` and let ~jthread do request_stop()+join()) also removes the hang, but it silently cancels the task the instant it starts, which is a worse story to tell on stage than an outright rejection.

This is a liveness/API-consistency defect rather than a data-race soundness hole, but the trait does answer TRUE for a callable the launcher cannot actually run.

## F34 — The `use_count()==1` fast path in `as_mutable()` is memory-ordering-critical but uncommented, and its acquire fence pairs with a refcount release the standard explicitly refuses to promise

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | certaine |
| **Axe** | thread-safety |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:35` |
| **Même défaut que** | `F46` — la barrière mémoire non commentée de as_mutable |

### Le problème

I verified the fence is right and load-bearing: a handle that detaches releases the old block with `__atomic_fetch_add(&_M_use_count, -1, __ATOMIC_ACQ_REL)` (ext/atomicity.h:71) *after* it has finished copy-reading the block, while `use_count()` is a `__ATOMIC_RELAXED` load (bits/shared_ptr_base.h:235); the relaxed load reading that value, sequenced before the acquire fence, gives fence-atomic synchronization per [atomics.fences]/4, so the in-place write is ordered after the other thread's copy-read. Without it, the ==1 branch would race with a concurrent detach. Two problems remain. First, in a file written to be read aloud at a conference this is the single most subtle line in the library and it carries no rationale at all — a reader sees a bare fence in an `else` and cannot reconstruct which release it pairs with. Second, the standard guarantees release semantics only for the *final* decrement (the one that runs the deleter); nothing in [util.smartptr.shared] promises it for a 2->1 decrement, so the pairing rests on libstdc++/libc++/MSVC behaviour rather than on the standard.

### Le code concerné

```cpp
if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
```

### La correction

```cpp
if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            // Reading 1 means every other holder has already dropped its handle,
            // and each of them finished reading the block before dropping it.
            // We are about to write that block in place, so we need those reads
            // ordered before our write. libstdc++/libc++/MSVC decrement the use
            // count with a release RMW and implement use_count() as a relaxed
            // load of the same counter, so this fence turns that decrement into
            // a synchronizes-with edge ([atomics.fences]/4). The standard does
            // not model the refcount at all -- [util.smartptr.shared.obs] warns
            // that use_count() is approximate and that ==1 "does not imply that
            // accesses through a previously destroyed shared_ptr have in any
            // sense completed" -- so this is an implementation guarantee, not a
            // portable one. There is no portable alternative: unique() was
            // removed in C++20.
            std::atomic_thread_fence(std::memory_order_acquire);
```

### Reproduction

```text
$ grep -n '_M_get_use_count' -A 6 /opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/bits/shared_ptr_base.h
230:      long
231:      _M_get_use_count() const noexcept
232:      {
233:	// No memory barrier is used here so there is no synchronization
234:	// with other threads.
235:	auto __count = __atomic_load_n(&_M_use_count, __ATOMIC_RELAXED);

$ grep -n '__exchange_and_add(' -A 2 /opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/ext/atomicity.h
70:  __exchange_and_add(volatile _Atomic_word* __mem, int __val)
71:  { return __atomic_fetch_add(__mem, __val, __ATOMIC_ACQ_REL); }

// -> relaxed load + acquire fence pairs with an ACQ_REL decrement: correct on
//    libstdc++. Reported as documentation/portability, not as a bug.
```

### Vérification

I tried four ways to kill this finding and all four failed.

(1) "The fence is not load-bearing — the library never lets two threads share one block." Refuted. `include/threadsafe/details/copy_on_write.h:54` makes `is_sendable<copy_on_write<T>>` true whenever `is_sendable<T> && is_synchronizable<const T>`, and `tests/test_copy_on_write.cpp` asserts both `is_sendable_v<cow<int>>` and `can_launch_task<decltype([](cow<std::string>){}), cow<std::string>>`, with the comment "one copy_on_write object belongs to one thread; share by copying it". So handle-copy + send to another thread is the *designed* usage, and the 2->1 shape (thread B const-reads `*ptr_`, then drops its handle; thread A then sees `use_count()==1` and writes in place at line 36) is exactly the race the fence exists to close. My probe compiles that shape cleanly. Without the fence there is no happens-before between B's read and A's write.

(2) "The libstdc++ mechanism is misdescribed." Refuted — the repro reproduces verbatim on this machine's GCC 16.2.0, same line numbers. I also checked the part the finding did not: `_Sp_counted_base<_S_atomic>::_M_release()` (shared_ptr_base.h:392) has a fast path at :414-420 that does *plain non-atomic* stores `_M_weak_count = _M_use_count = 0;`. That path fires only when both counts are already 1, i.e. the last handle, where by construction no other handle exists — so the 2->1 decrement in our scenario falls through to `__exchange_and_add_dispatch` at :427/:436 and is the ACQ_REL RMW the finding names. The pairing argument survives that detail. It also survives the multi-dropper case: consecutive decrements are RMWs in the release sequence headed by the first, so [atomics.fences]/4 gives A synchronization with every dropper, not just the last.

(3) "The pairing IS standard-guaranteed, so the portability half is wrong." Refuted, and the truth runs the other way. The standard does not model the refcount as an atomic object at all, so there is no `M` for [atomics.fences]/4 to name; worse, [util.smartptr.shared.obs] carries an explicit note on `use_count()` saying the result should be treated as approximate and that "use_count() == 1 does not imply that accesses through a previously destroyed shared_ptr have in any sense completed". The standard specifically disclaims the inference this line makes. So the finding's conclusion is right and understated.

(4) "The proposed fix breaks something." Refuted. I copied the include tree, applied the comment, and syntax-checked all 11 test TUs against the patched header (confirming via `-E` that the patched copy is the one resolved). All pass. It is a comment, so this was never in doubt.

One sub-claim in the finding IS inaccurate and I corrected it in the fix: "the standard guarantees release semantics only for the *final* decrement (the one that runs the deleter)". The standard specifies release semantics for no decrement at all — it never talks about the refcount. That error is generous to the standard, not to the code, so it does not rescue the line.

Verdict on the lens I was given (is the claim technically true about this exact code): yes. The fence at line 35 is correct, necessary, uncommented, and rests on implementation behaviour that the standard explicitly declines to promise. It stays a low-severity documentation/portability note, not a bug — and for a file meant to be read aloud at a conference, an unexplained bare `std::atomic_thread_fence` in an `else` is a real readability defect against CLAUDE.md's "simplicity and readability are first-class" requirement. Note there is no portable escape either: `shared_ptr::unique()` was removed in C++20, so `use_count() != 1` is the only API available; the honest remedy is to document the reliance, not to restructure.

---

# Couverture de tests

Ce que la suite `static_assert` ne vérifie pas, et les assertions qui passent sans rien prouver.

## F35 — tests/test_synchronized_value.cpp:83 — the file's only launch_scoped_task assertion is inert: its requires-expression is satisfied by the launcher's unconstrained explaining fallback, so it degenerates to a move-constructibility check and asserts none of the sendable/synchronizable rule it names

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `tests/test_synchronized_value.cpp:83` |

### Le problème

This file defines can_launch_scoped_task as a requires-expression over launcher.launch_scoped_task(...) instead of using the launchable_scoped_task concept the sibling test file uses. The launcher carries an unconstrained explaining overload, so the call expression is always well-formed and the predicate is true for every F and Args that are copy-constructible — including an arity mismatch. The single assertion built on it (line 83, the one that is supposed to prove a reference to a synchronizable object may cross a joined boundary) therefore proves nothing, and the file has no negative assertion that would have exposed the vacuity. The same predicate name in tests/test_asynchronous_task_launcher.cpp:22 is defined correctly via the concept, so the two files disagree about how to ask the question.

### Le code concerné

```cpp
template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_scoped_task(f, args...);
    };
...
static_assert(can_launch_scoped_task<decltype([](sync_int&) {}),
                                     std::reference_wrapper<sync_int>>,
              "launch_scoped_task — the launcher joins, so a reference to a "
              "synchronizable object may cross");
```

### La correction

```cpp
Define the predicate through the concept, as the sibling file already does (tests/test_asynchronous_task_launcher.cpp:22-24), and add the negative case that makes the positive one load-bearing. Only tests/test_synchronized_value.cpp changes; the library is correct as-is.

    template <class F, class... Args>
    constexpr bool can_launch_scoped_task =
        threadsafe::launchable_scoped_task<F, Args...>;

    static_assert(can_launch_scoped_task<decltype([](sync_int&) {}),
                                         std::reference_wrapper<sync_int>>,
                  "launch_scoped_task — the launcher joins, so a reference to a "
                  "synchronizable object may cross");
    static_assert(!can_launch_scoped_task<decltype([](Memo&) {}),
                                          std::reference_wrapper<Memo>>,
                  "and refused for a T no const& could share");

Both of these compile with the intended results (verified). Worth a glance across the other test files for any further requires-expression phrased against `launch_task`/`launch_scoped_task` rather than against the concept — the fallback makes every such predicate inert in the same way. (`can_lock` / `can_lock_shared` in the same file are fine: `synchronized_value` has no explaining fallback for `lock()`, so those requires-expressions do bite — confirmed by the passing `!can_lock<const sync_int&>` assertion.)
```

### Reproduction

```text
#include <threadsafe/threadsafe.h>
#include <functional>
#include <string>

// verbatim from tests/test_synchronized_value.cpp:27-31
template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_scoped_task(f, args...);
    };

struct Racy { std::string* borrowed; };

static_assert(!threadsafe::launchable_scoped_task<
                  decltype([](std::string&) {}), std::reference_wrapper<std::string>>);
static_assert(can_launch_scoped_task<decltype([](std::string&) {}),
                                     std::reference_wrapper<std::string>>,
              "VACUOUS: the unconstrained fallback satisfies the requires-expression");
static_assert(!threadsafe::launchable_scoped_task<decltype([](Racy) {}), Racy>);
static_assert(can_launch_scoped_task<decltype([](Racy) {}), Racy>,
              "VACUOUS for a struct-wrapped borrow too");
static_assert(can_launch_scoped_task<decltype([](int, int, int) {}), int>,
              "VACUOUS even for an arity mismatch");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<inc> probe_interact_vacuous_scoped.cpp
ALL VACUOUS ASSERTS PASS
```

### Vérification

I tried to refute this three ways and it survived all three.

1. Is the requires-expression really satisfied regardless of the traits? Yes. `asynchronous_task_launcher` (include/threadsafe/details/asynchronous_task_launcher.h:112-115) carries an unconstrained `launch_scoped_task(F, Args...)` fallback whose rejection lives in the *body* (`detail::explain_launch_scoped_task<F, Args...>()`, a throwing consteval). A requires-expression only checks that the call expression is well-formed; it never instantiates the body. So overload resolution succeeds and the predicate is true. Compiled: `can_launch_scoped_task<decltype([](std::string&){}), std::reference_wrapper<std::string>>` is true while `threadsafe::launchable_scoped_task<...>` for the same arguments is false. Same for a struct-wrapped raw borrow, and even for an arity mismatch (`[](int,int,int){}` with a single `int`).

2. Is the predicate vacuous for *literally* every input (which would overstate the claim)? No — and the finding's own wording already scopes it correctly. The by-value parameters `F f, Args... args` in the requires-expression's parameter list still require move-constructibility, so it rejects a non-movable type. I verified that too. The predicate therefore degenerates to exactly `ownable_by_launcher`, i.e. move-constructibility — none of the sendable/synchronizable rules the assertion is written to demonstrate.

3. Does the assertion at line 83 nonetheless happen to be a real test? No. The file's *only* `can_launch_scoped_task` use is that single positive assertion (grep shows uses at lines 28 and 83 only), and there is no negative counterpart, so nothing in the file would have exposed the degeneracy. The sibling file tests/test_asynchronous_task_launcher.cpp:20-24 explicitly comments on this exact trap ("the launcher carries an explaining fallback overload, so a rejected call is still a well-formed one — the concepts, not a requires-expression, are what state the rule") and defines both predicates via the concepts. Within test_synchronized_value.cpp itself, `can_launch_task` (line 25) is defined via `threadsafe::launchable_task`, so the two predicates in the *same* file disagree — this is an inconsistency, not a deliberate choice.

I also compiled the proposed fix: switching to the concept keeps the positive assertion passing and makes the suggested negative (`[](Memo&)` with `std::reference_wrapper<Memo>`) genuinely fail, so the fix is correct and does not require touching the library.

Scope note: this is a test-quality defect, not a soundness or usability hole in the library. `launchable_scoped_task` itself is correct; only the assertion that is supposed to demonstrate it is inert.

## F36 — tests/test_diagnostics.cpp asserts the rejection half is untestable; it is testable — a consteval caller can catch std::meta::exception and pin the message, leaving the whole diagnostic surface (walk paths, reasons, advice) with zero coverage

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | usability |
| **Emplacement** | `tests/test_diagnostics.cpp:10` |

### Le problème

The file's header comment states that only the agreeing half of assert_* is testable because the throwing half is a compile error by design. That is not so: assert_sendable<T>() is consteval and reports through a std::meta::exception, which a consteval caller can catch and whose u8what() is an ordinary constant-evaluable string. A four-line helper turns every diagnostic into a static_assert. Because the suite accepted the premise, the entire diagnostic surface — the walk paths, the reasons, the advice — is untested, which is why the wrong-blame defects in descend_sendable / descend_lifetime_aware / assert_lifetime_aware all shipped: they are exactly the kind of regression a message assertion catches.

### Le code concerné

```cpp
// The assert_* functions are the diagnostic face of the traits: they agree with
// the trait on a conforming type (they compile and return), and turn a "false"
// into a std::meta::exception naming the culprit. Only the agreeing half is
// testable here — the throwing half *is* a compile error by design.
```

### La correction

Delete the "Only the agreeing half is testable here" clause (keep the true observation that an uncaught assert_* is a compile error by design), and add the catching helper plus message assertions.

template <class T>
consteval bool sendable_rejection_mentions(std::u8string_view needle) {
    try { threadsafe::assert_sendable<T>(); }
    catch (const std::meta::exception &error) {
        return std::u8string_view(error.u8what()).find(needle)
            != std::u8string_view::npos;
    }
    return false;   // accepted when it should have been rejected
}

static_assert(sendable_rejection_mentions<BorrowingOuter>(u8"::middle"));
static_assert(sendable_rejection_mentions<BorrowingOuter>(u8"::inner"));
static_assert(sendable_rejection_mentions<BorrowingOuter>(u8"::borrowed"));
static_assert(sendable_rejection_mentions<BorrowingOuter>(u8"is a pointer or a reference"));

Return bool from a fully-consteval comparison rather than returning the text: a
std::u8string cannot escape constant evaluation into a constexpr variable
(GCC rejects it as "refers to a result of operator new"), so a text-returning
helper only works when consumed inside another consteval call.

Assert on substrings, never whole sentences, and prefer member-name hops
(::middle, ::inner, ::borrowed) over type spellings — the type text comes from
std::meta::u8display_string_of, whose spacing and qualification are
implementation-defined and would make the suite brittle.

Note the false-on-acceptance branch: returning false when assert_* fails to
throw means the same static_assert catches both a wrong message and a
wrongly-accepted type.

Mirror the helper for assert_synchronizable and assert_lifetime_aware — I
confirmed both throw catchable exceptions under the identical shape.

### Reproduction

```text
#include <threadsafe/threadsafe.h>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

struct Borrowing { int* borrowed; };
struct Middle { Borrowing inner; };
struct Outer { Middle middle; };

template <class T>
consteval std::u8string sendable_rejection() {
    try { threadsafe::assert_sendable<T>(); }
    catch (const std::meta::exception& e) { return std::u8string(e.u8what()); }
    return u8"<accepted>";
}
consteval bool has(std::u8string_view h, std::u8string_view n) {
    return h.find(n) != std::u8string_view::npos;
}

static_assert(has(sendable_rejection<Outer>(),
                  u8"Outer::middle (Middle)::inner (Borrowing)::borrowed (int*)"),
              "the throwing half IS testable: the walk names every hop");
static_assert(has(sendable_rejection<std::vector<int*>>(),
                  u8"has a user-written copy, move or destructor"));

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<inc> probe_interact_diagtest.cpp
PASS: diagnostics ARE compile-time testable

// and the same technique prints them, confirming the text:
Outer        : Outer::middle (Middle)::inner (Borrowing)::borrowed (int*) is a pointer or a reference: ...
vector<int*> : std::vector<int*> has a user-written copy, move or destructor — or a template that may be selected as one; ...
```

### Vérification

I tried to refute this three ways and failed on all three.

1. **Is the mechanism real?** I re-ran the claimed repro verbatim under `g++-16 -std=c++26 -freflection`. It compiled clean (`EXIT=0`). A `consteval` caller wrapping `threadsafe::assert_sendable<T>()` in `try/catch (const std::meta::exception&)` does catch the rejection during constant evaluation, and `e.u8what()` is usable as an ordinary constant-evaluable `std::u8string`. This is not a GCC quirk being exploited — constexpr exceptions (P3068) and the catchability of `std::meta::exception` (P2996) are exactly the C++26 design.

2. **Are the assertions vacuous?** My first refutation attempt was that the `static_assert`s might be passing for the wrong reason (e.g. the exception being swallowed and every needle matching, or the helper never being instantiated). I added negative controls: `!has(sendable_rejection<Outer>(), u8"ZZZ-not-present")` passes, `sendable_rejection<int>() == u8"<accepted>"` passes, and a deliberately wrong needle `u8"Outer::wrong (Nope)"` **does** fail the `static_assert`. So the assertions discriminate: they pin real text, and a message regression would break the build. That is a working regression test, not a tautology.

3. **Is the comment defensible on a charitable reading?** The comment says "Only the agreeing half is testable here — the throwing half *is* a compile error by design." The second clause is true and unobjectionable (an uncaught `assert_sendable` on a bad type is a hard error, and that is the intended user-facing behavior). But the first clause is a claim about testability, and it is false. The word "here" could be stretched to mean "in this file as currently written", but that reading makes the sentence content-free — it is plainly offered as the reason the file stops where it does, and it is the wrong reason.

I also confirmed the technique generalizes: `assert_synchronizable<const Outer>()` and `assert_lifetime_aware<Outer>()` both throw catchable exceptions and both yield non-`<accepted>` text under the same helper shape, so all three diagnostic families are testable, not just `assert_sendable`.

Consequence check: `tests/test_diagnostics.cpp` currently asserts only that (a) `assert_*` returns on conforming types and (b) the traits still answer plain `false` on non-conforming ones — i.e. that the exception does *not* escape. Nothing anywhere in `tests/` inspects a single character of rejection text. Since the walk paths, the blame attribution, and the advice strings are the entire point of the `path_step` / `reject_at` machinery in `details/utils.h`, that machinery ships with zero coverage. The finding's framing of this as the reason wrong-blame defects survived is a plausible causal story I did not independently verify (it depends on other findings in this audit), so I trimmed it out of the title and left the verifiable part: the premise is false and the surface is untested.

Two caveats worth carrying into the fix rather than treating as refutations. First, pinning the *full* message string would make the suite brittle against harmless rewording; the proposed `mentions`-on-substring shape is the right call and should be kept, asserting the path spine and one distinctive reason phrase rather than whole sentences. Second, the exact spelling of a type in the path comes from `std::meta::u8display_string_of`, which is implementation-defined in its details (`int*` vs `int *` spacing, class-name qualification), so needles should be chosen to avoid depending on that — the member-name hops (`::middle`, `::inner`, `::borrowed`) are stable, the type spellings less so. On a related note, I tried to dump the full message via a user-generated `static_assert` message and hit `'rej<Outer>()' is not a constant expression because it refers to a result of 'operator new'` — a `std::u8string` returned from consteval cannot escape into a `constexpr` variable. This does not affect the finding (the substring assertions never materialize the string outside constant evaluation), but it does mean the helper must return `bool` from a fully-consteval comparison rather than exposing the text, which the proposed fix already does correctly.

This is a test-coverage and comment-accuracy defect, not a soundness hole in the traits — no type is answered TRUE that is unsafe. Severity is real but bounded, and it is unusually cheap to fix.

## F37 — The deleted `operator*() &&` / `operator->() &&` on value_guard — the only `= delete("...")` in the library and the sole thing stopping `*sv.lock()` from returning a reference into an already-released lock — have zero test coverage; deleting synchronized_value.h:29-30 breaks no test

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `tests/test_synchronized_value.cpp:93` |

### Le problème

value_guard deletes operator*() && and operator->() && with a custom message; that deletion is the single mechanism stopping `*sv.lock()` from returning a reference to data whose lock died at the semicolon. tests/test_synchronized_value.cpp asserts the return types of the surviving const& overloads (lines 93-98) but never asserts that the rvalue forms are rejected, and no other test file touches lock() at all except through can_lock/can_lock_shared, which only ask that the call expression exists. Deleting those two lines from the header would break no test. The assertions are trivially writable as requires-expressions.

### Le code concerné

```cpp
static_assert(
    std::same_as<decltype(*std::declval<const sync_int::guard&>()), int&>,
    "the guard hands out a mutable reference for the duration of the lock");
static_assert(std::same_as<decltype(*std::declval<const sync_int::const_guard&>()),
                           const int&>,
              "the shared guard hands out a const reference");
```

### La correction

```cpp
Add to tests/test_synchronized_value.cpp, near the existing const& deref assertions at lines 93-98. Cover both mutex paths: sync_int (const_guard holds a shared_lock) and sync_memo (const_guard holds a unique_lock).

    template <class T>
    constexpr bool derefs_temporary_guard = requires(T& v) { *v.lock(); };
    template <class T>
    constexpr bool derefs_temporary_shared_guard =
        requires(const T& v) { *v.lock_shared(); };
    template <class T>
    constexpr bool arrows_temporary_guard = requires(T& v) { v.lock()->key; };
    template <class T>
    constexpr bool arrows_temporary_shared_guard =
        requires(const T& v) { v.lock_shared()->key; };

    static_assert(!derefs_temporary_guard<sync_int>,
                  "a temporary guard is destroyed at the semicolon, so it hands "
                  "out no reference");
    static_assert(!derefs_temporary_shared_guard<sync_int>);
    static_assert(!derefs_temporary_guard<sync_memo>);
    static_assert(!derefs_temporary_shared_guard<sync_memo>,
                  "the unique_lock const_guard is covered too, not only the "
                  "shared_lock one");
    static_assert(!arrows_temporary_guard<sync_memo>);
    static_assert(!arrows_temporary_shared_guard<sync_memo>);

Do NOT use the proposed `v.lock()->operator=(1)` on sync_int: with T = int the arrow probe is vacuously false (`int*->operator=` is invalid whether or not the overload is deleted), so it would keep passing after the deletion was removed. The arrow probe needs a class-type T and a real member — `->key` on Memo — which is why the arrow assertions above are written against sync_memo only.

Optionally pair each with its positive control so a future refactor that breaks *all* dereferencing is caught as well:

    static_assert(requires(const sync_memo::guard& g) { g->key; });
    static_assert(requires(const sync_memo::const_guard& g) { g->key; });
```

### Reproduction

```text
#include <threadsafe/threadsafe.h>
#include <concepts>
using sync_int = threadsafe::synchronized_value<int>;

template <class SV>
constexpr bool derefs_temporary_guard = requires(SV& v) { *v.lock(); };
template <class SV>
constexpr bool arrows_temporary_guard = requires(SV& v) { v.lock()->operator=(1); };
template <class SV>
constexpr bool derefs_temporary_shared_guard =
    requires(const SV& v) { *v.lock_shared(); };

static_assert(!derefs_temporary_guard<sync_int>);
static_assert(!arrows_temporary_guard<sync_int>);
static_assert(!derefs_temporary_shared_guard<sync_int>);

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<inc> probe_interact_guard_rvalue.cpp
PASS: writable, and absent from tests/
$ grep -rn "lock()" /Users/amorrier/Programmation/ThreadSafe/tests/
tests/test_synchronized_value.cpp:34:constexpr bool can_lock = requires(T v) { v.lock(); };
tests/test_synchronized_value.cpp:144:  "const only removes lock(), and lock_shared() is already safe");
   (no other occurrence: nothing ever dereferences a guard, temporary or not)
```

### Vérification

The factual core survives every refutation attempt.

1. The mechanism exists and is the marquee safety feature. include/threadsafe/details/synchronized_value.h:29-30 are the only two `= delete("...")` in the entire library (grep over details/ returns exactly those two lines). They are what stops `*sv.lock()` from binding a reference to data whose lock is released at the semicolon.

2. Nothing tests them. `grep -rln guard tests/` returns four files; in three of them "guard" is only the word "structural guard" in a comment string (test_containers.cpp:155, test_soundness_regressions.cpp:172, test_synchronizable.cpp:102). test_synchronized_value.cpp exercises guards only via trait queries (is_sendable/is_lifetime_aware), type identity (std::same_as on guard/const_guard), copyability, and the two const& deref return types at lines 93-98. `can_lock`/`can_lock_shared` only ask that the call expression exists. test_diagnostics.cpp never mentions synchronized_value. So no test would fail if lines 29-30 were deleted.

3. The proposed assertions are writable. probe_testgap_rvalue_final.cpp compiles and passes with the real header: a requires-expression does detect selection of the deleted rvalue overload and yields false, for both sync_int (shared_mutex path, const_guard = shared_lock) and sync_memo (mutex path, const_guard = unique_lock).

4. The assertions are not vacuous — this was the strongest refutation route and it failed. I built a mutation (probe_testgap_rvalue_mutation.cpp): the same guard shape with the two `&&` deletions removed. `*v.lock()` then becomes well-formed (a prvalue guard binds happily to the const&-qualified operator*), so the deref probes flip to true and the static_asserts would fire. The probes therefore actually pin the deletion.

One correction to the proposed fix, found by the same mutation probe. `arrows_temporary_guard<sync_int> = requires(SV& v) { v.lock()->operator=(1); }` is vacuously false: with T = int, `operator->()` yields `int*`, and `int*->operator=` is invalid regardless of the deletion. My mutation probe confirms `!arrows_temporary_guard<weak_sv<int>>` holds even with no deletion present, while it flips to true for a class type. The arrow probe must therefore use a class-type T and a real member — `v.lock()->key` on sync_memo — which I verified is both false with the deletion and non-vacuous.

Scope caveat, not a refutation: this is a test-coverage gap, not a soundness or usability hole in the library. The deletions are present, correct, and effective; nothing unsafe compiles today. What is missing is the regression net protecting a feature the library will be shown off for at a conference.

## F38 — The guard-immobility assertion tests !std::movable, which a move-constructible-but-not-move-assignable guard satisfies; it does not pin move-constructibility, and its message names a threat the trait walk already blocks

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | certaine |
| **Axe** | soundness |
| **Emplacement** | `tests/test_synchronized_value.cpp:88` |

### Le problème

The comment says a movable guard could be lodged in an aggregate and travel, but std::movable additionally requires assignable_from and swappable, so !std::movable is satisfied by any type that moves but does not move-assign. A value_guard that grew a move constructor and kept move assignment deleted — the natural way someone would make a guard returnable — would still satisfy this assertion while doing exactly what the comment forbids, and would still be embeddable in a movable aggregate. The assertion passes today only because the guard currently has no move constructor at all; it does not test for one.

### Le code concerné

```cpp
static_assert(!std::copy_constructible<sync_int::guard>
                  && !std::movable<sync_int::guard>,
              "a movable guard could be lodged in an aggregate and travel");
```

### La correction

```cpp
Replace the two-clause conjunction with the single stronger clause (it subsumes both `!copy_constructible` and `!movable`), cover `const_guard`, and fix the message to name the property actually being pinned — a guard never outlives its lock scope — rather than the aggregate-travel scenario, which `is_sendable<value_guard<T, Lock>> : std::false_type` already blocks via the member walk:

static_assert(!std::move_constructible<sync_int::guard>
                  && !std::move_constructible<sync_int::const_guard>,
              "a guard that moves outlives the scope that took the lock");
static_assert(!std::copy_constructible<sync_int>);
```

### Reproduction

```text
#include <threadsafe/threadsafe.h>
#include <concepts>
#include <mutex>
using sync_int = threadsafe::synchronized_value<int>;

// what the suite asserts today
static_assert(!std::copy_constructible<sync_int::guard> && !std::movable<sync_int::guard>);

// a guard that satisfies that assertion and still travels
struct MovableGuard {
    MovableGuard(const MovableGuard&) = delete;
    MovableGuard& operator=(const MovableGuard&) = delete;
    MovableGuard(MovableGuard&&) = default;
    MovableGuard& operator=(MovableGuard&&) = delete;
    std::unique_lock<std::mutex> lock_;
    int* value_;
};
static_assert(!std::copy_constructible<MovableGuard> && !std::movable<MovableGuard>,
              "the suite's assertion still holds ...");
static_assert(std::move_constructible<MovableGuard>,
              "... while the guard moves, which the comment forbids");
struct Aggregate { MovableGuard g; };
static_assert(std::move_constructible<Aggregate>, "and travels inside an aggregate");

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<inc> probe_interact_movable.cpp
OK

// and, confirming the current guard is immobile for the right reason:
static_assert(!std::move_constructible<sync_int::guard>);
static_assert(!std::move_constructible<sync_int::const_guard>);   // both pass
```

### Vérification

The technical claim is exactly right and I could not refute it. `std::movable<T>` = `is_object_v<T> && move_constructible<T> && assignable_from<T&,T> && swappable<T>`, so `!std::movable<T>` is satisfied by any type that move-constructs but does not move-assign — which is precisely the shape someone would write when making a guard returnable (`guard(guard&&) = default; guard& operator=(guard&&) = delete;`). I compiled that hypothetical guard: it satisfies the suite's exact assertion (`!copy_constructible && !movable`) while being `move_constructible` and embeddable in a move-constructible aggregate. So the assertion at tests/test_synchronized_value.cpp:88 does not test the property its message names; it passes today only because `value_guard` declares a deleted copy constructor, which suppresses the implicit move constructor entirely.

Refutation attempt 1 — maybe `!movable` is enough in practice: no. Verified `!std::movable<MovableGuard> && std::move_constructible<MovableGuard>` compiles clean.

Refutation attempt 2 — maybe the assertion is redundant because the reflective member walk already blocks the aggregate: I compiled `struct HoldsGuard { sync_int::guard g; };` and both `is_sendable_v` and `is_synchronizable_v` are false for it, independent of movability (the walk keys off `is_sendable<value_guard<...>> : std::false_type`, not off movability). This does NOT refute the finding — it refutes the *comment's* stated rationale. The aggregate-travel scenario is already covered by the traits; what the assertion actually pins is the narrower design decision that a guard never outlives its lock scope (unlike `std::unique_lock`). So the message should be corrected along with the concept.

Severity is low: this is test precision, not a library soundness or usability hole. The current guard is genuinely immobile; the assertion is simply a weak regression pin. Worth fixing in an educational codebase where an assertion's message is teaching material.

One correction to the proposed fix: `std::copy_constructible<T>` subsumes `std::move_constructible<T>`, so `!std::move_constructible<T>` already implies `!std::copy_constructible<T>` and `!std::movable<T>`. I verified this subsumption compiles. The two-clause conjunction is redundant; a single clause states the property more clearly, which matters here.

---

# Simplicité du code

Le code est destiné à être projeté sur un écran : chaque construction doit s'expliquer en une phrase.

## F39 — The diagnostic walk machinery (descend/explain/default_is) is written three times; two copies even cite the third's comment instead of carrying their own

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/sendable.h:60` |

### Le problème

`descend_X`, `explain_X` and `default_is_X` are byte-identical across sendable.h, synchronizable.h and lifetime_aware.h — only the trait name and one reason string change. Four comment blocks are duplicated word-for-word in all three files (`Continue the walk inside \`inner\`…`, `Seeding the path with the type…`, `phrased so that "no" carries its reason…`, plus the try/catch). A reader who understands one trait must re-read the identical 40 lines twice to convince themselves the other two are the same; on a slide the audience sees the same paragraph three times and learns nothing new. Factoring the invariant part into utils.h leaves each trait header holding only what is genuinely its own: the reason string and the `diagnose` body.

### Le code concerné

```cpp
// sendable.h:12-18 and 60-89 and 171-178 (the same shape appears in
// synchronizable.h:22-29/70-96/206-213 and lifetime_aware.h:14-20/86-112/190-197)
namespace detail {
consteval void diagnose_default_is_sendable(std::meta::info type,
                                            std::u8string path = {});
consteval bool default_is_sendable(std::meta::info type);
[[noreturn]] consteval void descend_sendable(std::meta::info inner,
                                             const std::u8string &path);
}

template <class T>
struct is_sendable : std::bool_constant<detail::default_is_sendable(^^T)> {};

// ...

[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);

    reject(inner,
           u8"is not sendable: is_sendable is specialized to false for it",
           path);
}

[[noreturn]] inline consteval void explain_sendable(std::meta::info subject,
                                                    std::u8string_view reason,
                                                    std::meta::info inner,
                                                    const std::u8string &path) {
    if (path.empty())
        reject(subject, reason);

    descend_sendable(inner, path + path_step(subject));
}

inline consteval bool default_is_sendable(std::meta::info type) {
    try {
        diagnose_default_is_sendable(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}
```

### La correction

```cpp
The finding's fix is correct as proposed and I verified it compiles with byte-identical diagnostics. Two refinements:

1. Name `holds` something a reader can decode on a slide — `default_holds` or `walk_answers_yes`. `holds<sendable_walk>(^^T)` inside a `std::bool_constant` is the first thing the audience reads about the trait, and `holds` names nothing.

2. Move the long rationale comment into utils.h intact, including the "measured at 38x on a 60-level chain" figure. Today that paragraph exists in long form only in sendable.h, which is precisely why synchronizable.h:87 and lifetime_aware.h:103 have to say "see explain_sendable". Writing it once where the code lives removes the cross-reference, which is the clearest single win of the whole refactor.

Concretely, in utils.h after `reject`/`path_step`:

  template <class Walk>
  [[noreturn]] consteval void descend(std::meta::info inner,
                                      const std::u8string &path) {
      Walk::diagnose(inner, path);
      reject(inner, Walk::specialized_to_false, path);
  }

  template <class Walk>
  [[noreturn]] consteval void explain(std::meta::info subject,
                                      std::u8string_view reason,
                                      std::meta::info inner,
                                      const std::u8string &path) {
      if (path.empty())
          reject(subject, reason);
      descend<Walk>(inner, path + path_step(subject));
  }

  template <class Walk>
  consteval bool default_holds(std::meta::info type) {
      try { Walk::diagnose(type, {}); return true; }
      catch (const std::meta::exception &) { return false; }
  }

and in each trait header only:

  namespace detail {
  struct sendable_walk {
      static consteval void diagnose(std::meta::info type,
                                     std::u8string path = {});
      static constexpr std::u8string_view specialized_to_false =
          u8"is not sendable: is_sendable is specialized to false for it";
  };
  }

  template <class T>
  struct is_sendable
      : std::bool_constant<detail::default_holds<detail::sendable_walk>(^^T)> {};

with the walk body becoming `inline consteval void sendable_walk::diagnose(...)`, every `explain_sendable(` becoming `explain<sendable_walk>(`, and assert_sendable calling `detail::descend<detail::sendable_walk>(^^T, detail::type_name(^^T))`. Same shape for `const_synchronizable_walk` and `lifetime_aware_walk`.
```

### Reproduction

```text
Applied to a copy of the tree at
/private/tmp/claude-501/.../scratchpad/simp2/include. All 11 test TUs compile:

  for t in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude $t; done
  tests/test_asynchronous_task_launcher.cpp    OK
  tests/test_containers.cpp                    OK
  tests/test_copy_on_write.cpp                 OK
  tests/test_deferred_specialization.cpp       OK
  tests/test_diagnostics.cpp                   OK
  tests/test_lifetime_aware.cpp                OK
  tests/test_sendable.cpp                      OK
  tests/test_smart_pointers.cpp                OK
  tests/test_soundness_regressions.cpp         OK
  tests/test_synchronizable.cpp                OK
  tests/test_synchronized_value.cpp            OK

Diagnostic text is byte-identical. probe_simplicity_msgs.cpp:

  struct Leaf { int *borrowed; };  struct Mid { Leaf leaf; };  struct Root { Mid mid; };
  struct Opaque {};  template <> struct threadsafe::is_sendable<Opaque> : std::false_type {};
  struct HoldsOpaque { Opaque o; };
  struct Ref { int &r; };  struct HoldsRef { Ref inner; };
  struct NotOwn { int *p; };  struct HoldsNotOwn { NotOwn n; };
  void f() { threadsafe::assert_sendable<Root>();
             threadsafe::assert_sendable<HoldsOpaque>();
             threadsafe::assert_synchronizable<const HoldsRef>();
             threadsafe::assert_lifetime_aware<HoldsNotOwn>(); }

  $ diff <(run original) <(run refactored) && echo IDENTICAL DIAGNOSTICS
  IDENTICAL DIAGNOSTICS

  what()': 'Root::mid (Mid)::leaf (Leaf)::borrowed (int*) is a pointer or a reference: ...'
  what()': 'HoldsOpaque::o (Opaque) is not sendable: is_sendable is specialized to false for it'
  what()': 'const HoldsRef::inner (Ref)::r (int&) is a reference: the const stops there — ...'
  what()': 'HoldsNotOwn::n (NotOwn)::p (int*) is a reference or a raw pointer: ...'

Header sizes: sendable.h 182->146, synchronizable.h 217->181,
lifetime_aware.h 201->166, utils.h 155->204 (net -99 lines of duplication
for +49 lines written once).
```

### Vérification

I tried to refute this on three fronts and failed on all three.

(1) Is the duplication real? Yes. I extracted the three blocks (sendable.h:60-89 + 171-178, synchronizable.h:72-96 + 206-213, lifetime_aware.h:88-112 + 190-197) and compared them. `descend_X`, `explain_X` and `default_is_X` are identical in structure, statement for statement, differing only in the embedded trait name and the "specialized to false" reason string. The three comment blocks the finding enumerates are word-for-word identical across all three headers, and the try/catch body is identical three times.

The code even admits it: synchronizable.h:87 and lifetime_aware.h:103 both say "see explain_sendable for why the trait itself must leave it empty" — a cross-reference into a *different header* to explain a function that is textually present right where the comment sits. A copy that has to point at its original for its own rationale is the duplication complaining about itself, not a mitigation.

(2) Does the proposed fix compile? Yes. I applied it to a copy of the tree (scratchpad/refute_dup/tree): three `*_walk` policy structs each carrying `static consteval void diagnose(info, u8string path = {})` and `static constexpr u8string_view specialized_to_false`, with `descend<Walk>` / `explain<Walk>` / `holds<Walk>` written once in utils.h. All 11 test TUs compile under g++-16 -std=c++26 -freflection.

(3) Does it break anything subtle? No. The forward-declaration dance that the current code needs (declare `diagnose_default_is_X` before `is_X`, define it after) survives as a declared-then-defined-out-of-line static member; test_deferred_specialization.cpp — the test that exercises exactly that late-instantiation property — passes. Diagnostics are byte-identical on 8 probes covering all three traits: deep member chain, opaque specialization for each trait, mutable member, reference member, cv-qualified root. `diff` of the sorted what() strings is clean.

The only thing I could correct is arithmetic: the finding claims net -99 lines; I measured 755 -> 696 across the four headers, so net -59. Direction right, magnitude overstated. That does not touch the substance.

On the educational axis the finding is if anything strengthened rather than weakened: the argument against a policy-struct indirection would be that it adds a layer an audience must decode, but here the layer replaces a cross-header "go read the other file" comment, and it leaves each trait header holding exactly its reason string and its walk body — which is the part that is genuinely different and genuinely worth a slide.

## F40 — synchronized_value contains the library's only two `[: :]` type-computing splices, used for a conditional typedef that std::conditional_t does in one line — and both consteval helpers are public

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:53` |
| **Même défaut que** | `F42`, `F61` — les splices et helpers consteval de synchronized_value |

### Le problème

Two `static consteval auto` functions returning `std::meta::info`, plus two `[: :]` splices, exist only to pick between `shared_mutex`/`mutex` and between `shared_lock`/`unique_lock` — a job `std::conditional_t` does in one expression. Both helpers are `public`, so they leak into the class's API and will show up in documentation and completion. This is the strongest example in the library of reflection used where a plain template reads better; on a slide it teaches the audience that C++26 reflection is needed for a conditional typedef, which is the opposite of the intended message. The same `is_synchronizable_v<const T>` question is also spelled out twice; naming it once makes the design decision ("can reads be shared?") visible.

### Le code concerné

```cpp
static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];

    static consteval auto get_const_guard_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^value_guard<const T, std::shared_lock<mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = [:get_const_guard_type():];
```

### La correction

```cpp
Replace lines 53-72 of include/threadsafe/details/synchronized_value.h. Put the named condition in the class's existing implicit private section (beside the sendable<T> static_assert) rather than under `public:`, so the rewrite removes two public members instead of trading them for a new one:

template <class T>
class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

    static constexpr bool reads_share = is_synchronizable_v<const T>;

public:
    using mutex = std::conditional_t<reads_share, std::shared_mutex, std::mutex>;
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard =
        value_guard<const T, std::conditional_t<reads_share,
                                                std::shared_lock<mutex>,
                                                std::unique_lock<mutex>>>;

<type_traits> is already included at line 7, so no new include is needed. 20 lines -> 7; two public consteval helpers and the library's only two splices go away, and the design question ("can reads be shared?") is asked once by name.
```

### Reproduction

```text
Applied to a copy of the tree; all 11 test TUs still compile, including
tests/test_synchronized_value.cpp which pins the exact types:

  static_assert(std::same_as<sync_int::const_guard,
                             threadsafe::value_guard<
                                 const int, std::shared_lock<std::shared_mutex>>>, ...);
  static_assert(std::same_as<sync_memo::mutex, std::mutex>,
                "no shared_mutex: there is no read that may be shared");
  static_assert(std::same_as<sync_memo::guard,
                             threadsafe::value_guard<Memo, std::unique_lock<std::mutex>>>);

  $ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude tests/test_synchronized_value.cpp
  (no output)

22 lines -> 7, two public helpers and two splices removed.
```

### Vérification

I could not refute it. (1) The repro reproduces exactly: applying the fix to a copy of the tree, all 11 test TUs compile clean with g++-16 -std=c++26 -freflection -fsyntax-only. (2) The assertions that pin the types are non-vacuous — a negative control forcing reads_share=false makes test_synchronized_value.cpp fail its static_assert, so the passing suite really does prove the rewrite is type-identical for both the const-synchronizable T (int -> shared_mutex/shared_lock) and the non-const-synchronizable T (Memo -> mutex/unique_lock). (3) The discarded conditional_t branch (std::shared_lock<std::mutex> when mutex is std::mutex) is only named, never instantiated, so it is not a hard error — confirmed by the sync_memo tests compiling. (4) The strongest possible defense — "this is the library's reflection house style" — is false and in fact inverts against the code: grep -rn "\[:" include/ returns exactly two splice sites in the whole library, both here. Every other reflection use (utils.h, sendable.h, synchronizable.h, lifetime_aware.h, allowed_std_wrappers.h) is a consteval predicate returning bool over std::meta::info. This is the only place reflection computes a type, and it is exactly the place a one-line std::conditional_t does the job — which is the finding's pedagogical point. (5) The helpers have no other callers: grep for get_mutex_type/get_const_guard_type hits only the definitions and their two splices, and no test names them, so removal is safe. The only thing I would change is the fix itself: the proposal declares reads_share under public:, which partially reintroduces the API leak it removes. Moving it into the class's existing implicit private section compiles all 11 TUs identically and leaves only mutex/guard/const_guard public. I also dropped the proposed two-line comment, since CLAUDE.md asks to avoid useless comments and the name reads_share carries the idea.

## F41 — asynchronous_task_launcher.h is the only details/ header that is not standalone-includable: its class-scope static_assert depends on std::stop_token specializations declared in vocabulary.h, which it neither includes nor names

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:12` |

### Le problème

The class-scope `static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>)` is only satisfied by the specializations in vocabulary.h, which this header never includes. It compiles today purely because threadsafe.h lists vocabulary.h before it. Every other header in the library is standalone-includable; this one is not, and the failure mode is a static_assert whose message says nothing about include order. A reader tracing where `std::stop_token` becomes sendable has to go find a file called `vocabulary.h` that the header gives no pointer to — the definition of holding two files in your head.

### Le code concerné

```cpp
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {
// ...
class asynchronous_task_launcher {
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");
```

### La correction

```cpp
Add the missing include to include/threadsafe/details/asynchronous_task_launcher.h, after line 12:

  #include <threadsafe/details/lifetime_aware.h>
  #include <threadsafe/details/sendable.h>
  #include <threadsafe/details/vocabulary.h>  // answers the std::stop_token assertion below

This is the fix as proposed and it is correct as written; no cycle (vocabulary.h pulls only lifetime_aware.h and sendable.h), and it also removes the latent use-before-explicit-specialization of is_sendable<std::stop_token>. Verified: standalone include, launcher-before-umbrella, and all 11 tests/*.cpp compile clean against a patched copy. Optionally shorten the assert message to point at vocabulary.h, so a reader at that line knows where the answer is stated.
```

### Reproduction

```text
$ echo '#include <threadsafe/details/asynchronous_task_launcher.h>' > probe.cpp
$ g++-16 -std=c++26 -freflection -fsyntax-only -I/Users/amorrier/Programmation/ThreadSafe/include probe.cpp
.../asynchronous_task_launcher.h:82:19: error: static assertion failed: std::jthread
  injects a stop_token that the Args constraints never see; it must satisfy them on its own

Even with the umbrella present, order decides:
$ cat probe_order.cpp
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/threadsafe.h>
$ g++-16 -std=c++26 -freflection -fsyntax-only -I... probe_order.cpp
.../asynchronous_task_launcher.h:82:19: error: static assertion failed: ...

With the added include (scratchpad/simp3/include), the standalone probe compiles clean.
Every other details/*.h compiles standalone today (verified for all 11).
```

### Vérification

I set out to refute this via the design-intent lens and failed on every avenue.

Refutation attempt 1 — "CLAUDE.md documents this as a trade-off." It does not. CLAUDE.md covers the toolchain, the trait shapes, and the reflective `_v` recursion. It says nothing about header self-containment, include order, entry points, or a public/private split. There is no documented intent to appeal to, so the finding cannot be dismissed as an accepted design choice.

Refutation attempt 2 — "details/ headers are private by convention, so non-standalone is the norm." Disproved empirically. I compiled each of the 11 details/*.h in isolation: allowed_std_wrappers, copy_on_write, lifetime_aware, sendable, smart_pointers, synchronizable_base, synchronizable, synchronized_value, utils, vocabulary all pass; asynchronous_task_launcher is the only failure. A 10-of-11 rate makes this an outlier, not a convention.

Refutation attempt 3 — "the umbrella makes it a non-issue." Partially true and it is the finding's real weakness, but it does not kill it. Including the launcher header before threadsafe.h still fails, because the class-scope static_assert is evaluated at the first class definition; only umbrella-first works. More importantly the axis here is simplicity, not correctness: a reader at line 82 sees `sendable<std::stop_token>` asserted with no pointer to where that answer is established. Those specializations are in vocabulary.h:28 and :38, a file the header never names and never includes. That is two files held in the head to understand one assert, and it is a real cost for code whose stated purpose is being read aloud at a conference.

Refutation attempt 4 — "the fix is unsafe or redundant." No. vocabulary.h includes only lifetime_aware.h and sendable.h, so adding it creates no cycle. On a patched copy of the tree the standalone probe compiles, the launcher-first-then-umbrella probe compiles, and all 11 test translation units compile clean.

Note the mechanism is slightly worse than plain include-order fragility: the static_assert implicitly instantiates is_sendable<std::stop_token> at that point, so any TU reaching the launcher header before vocabulary.h uses the primary template before the explicit specialization is declared. In the umbrella's order this never happens, but the ordering hazard is latent in the header itself.

The one honest correction is severity. Because details/ is a private directory and threadsafe.h is the practical entry point, no supported usage hits this today, so "high" overstates it; medium is the right level. The finding itself survives.

## F42 — Two public `consteval` functions and two splices where a single named `conditional_t` condition is type-identical — but `mutex` must stay public or tests/test_synchronized_value.cpp:120 breaks

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:53` |
| **Même défaut que** | `F40`, `F61` — les splices et helpers consteval de synchronized_value |

### Le problème

`get_mutex_type()` and `get_const_guard_type()` are reflection machinery used to pick between two known types — the case `std::conditional_t` has covered since C++11. They also sit in the `public:` section, so `sv.get_mutex_type()` and `get_const_guard_type()` are part of the type's advertised interface, alongside the genuinely public `mutex`, `guard` and `const_guard` aliases. `is_synchronizable_v<const T>` is spelled out twice and the two decisions (which mutex, which lock) are stated in two places that must be kept in agreement by hand. On a header whose stated purpose is to be read from a conference stage, this spends the audience's attention on reflection that carries no reflective idea — every other use of reflection in this library is doing something `conditional_t` cannot.

### Le code concerné

```cpp
static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];

    static consteval auto get_const_guard_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^value_guard<const T, std::shared_lock<mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = [:get_const_guard_type():];
```

### La correction

```cpp
Replace lines 53-72 with the following. Note the difference from the originally proposed fix: `mutex` stays PUBLIC (tests/test_synchronized_value.cpp:120 asserts on `sync_memo::mutex`), and only `shared_reads` lives in the class's already-implicit private section, so no new access specifier is introduced:

    // (still in the implicit private section that holds the sendable<T> static_assert)
    // Readers share the value exactly when a const T is safe to read from
    // several threads at once; otherwise every reader is a writer's peer.
    static constexpr bool shared_reads = is_synchronizable_v<const T>;

public:
    using mutex = std::conditional_t<shared_reads, std::shared_mutex, std::mutex>;
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<
        const T,
        std::conditional_t<shared_reads, std::shared_lock<mutex>,
                                         std::unique_lock<mutex>>>;

`<type_traits>` is already included at line 7. Verified: all 11 files in tests/ compile clean against this header.
```

### Reproduction

```text
// Confirms the replacement is type-identical for both branches of the condition.
#include <threadsafe/threadsafe.h>
#include <vector>
#include <shared_mutex>
#include <mutex>
using namespace threadsafe;

template <class T> struct simpler {
    static constexpr bool shared_reads = is_synchronizable_v<const T>;
    using mutex = std::conditional_t<shared_reads, std::shared_mutex, std::mutex>;
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<const T, std::conditional_t<
        shared_reads, std::shared_lock<mutex>, std::unique_lock<mutex>>>;
};
struct MutableCache { mutable int cached; };   // sendable, NOT const-synchronizable
using A = synchronized_value<std::vector<int>>; using SA = simpler<std::vector<int>>;
using B = synchronized_value<MutableCache>;     using SB = simpler<MutableCache>;
static_assert(std::is_same_v<A::mutex, SA::mutex>);
static_assert(std::is_same_v<A::guard, SA::guard>);
static_assert(std::is_same_v<A::const_guard, SA::const_guard>);
static_assert(std::is_same_v<B::mutex, SB::mutex>);
static_assert(std::is_same_v<B::guard, SB::guard>);
static_assert(std::is_same_v<B::const_guard, SB::const_guard>);
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_syncval_simpler.cpp
SIMPLER FORM IS TYPE-IDENTICAL
```

### Vérification

The finding survives refutation on the reproduction lens. (1) The quoted code at synchronized_value.h:53-72 is verbatim correct, including that both consteval functions sit in the `public:` section. (2) I re-ran the repro with a unique name and it compiles clean; I strengthened it with `static_assert(!std::is_same_v<A::mutex, B::mutex>)` plus the trait premises (`is_sendable_v<MutableCache>`, `!is_synchronizable_v<const MutableCache>`) to prove both branches of the condition are genuinely exercised and genuinely differ — so "type-identical" is not vacuously true from both sides landing on the same type. (3) The simplification claim holds: `conditional_t` covers this exactly, 20 lines collapse to 10, `is_synchronizable_v<const T>` goes from twice to once, and two consteval functions leave the advertised interface. Nothing in the reflective machinery is load-bearing here — I confirmed by substituting the whole block in a copied header tree and compiling all 11 test files clean. The ONE defect is the proposed fix, not the finding: it puts `mutex` in `private:`, and tests/test_synchronized_value.cpp:120 reads `sync_memo::mutex` publicly, so the patched header fails with "'using threadsafe::synchronized_value<Memo>::mutex' is private within this context". `mutex` must stay public. I verified a corrected variant against the full suite. Also worth noting the corrected form needs no extra access specifier at all: the class body's implicit private section (which already holds the `sendable<T>` static_assert) is the natural home for the named condition, so the diff is a clean deletion rather than a re-shuffle of access blocks. `<type_traits>` is already included at line 7, so no new include is required.

## F43 — The weak_ptr rules are verbatim copies of the shared_ptr rules in all three traits, and the shared_ptr answer is split across lifetime_aware.h and smart_pointers.h

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/lifetime_aware.h:52` |

### Le problème

To answer "what does the library say about std::shared_ptr<T>?" you must read lifetime_aware.h (the ownership rule) and smart_pointers.h (the sendable and synchronizable rules) — same for weak_ptr and reference_wrapper. Inside those rules, `std::remove_all_extents_t<T>` appears 14 times in two spellings, half with `remove_cv_t` wrapped around it and half without, three lines apart, with nothing saying which is which. On top of that the weak_ptr rules are verbatim copies of the shared_ptr rules in all three traits — the 5-line lifetime rule is pasted twice in a row. Naming the pointee once and letting weak_ptr inherit shared_ptr's answer makes the intent ("a weak_ptr is the same handle without the strong count") readable instead of inferable.

### Le code concerné

```cpp
// lifetime_aware.h:52-63
template <class T>
struct is_lifetime_aware<std::shared_ptr<T>>
    : std::bool_constant<
          is_lifetime_aware_v<std::remove_cv_t<std::remove_all_extents_t<T>>>
          && detail::dynamic_type_is_known<
                 std::remove_cv_t<std::remove_all_extents_t<T>>>> {};
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>>
    : std::bool_constant<
          is_lifetime_aware_v<std::remove_cv_t<std::remove_all_extents_t<T>>>
          && detail::dynamic_type_is_known<
                 std::remove_cv_t<std::remove_all_extents_t<T>>>> {};

// smart_pointers.h:32-34 and 58-60
template <class T>
struct is_sendable<std::weak_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::weak_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};
```

### La correction

Drop the `pointee` alias entirely — it nets +2 lines across the tree, replaces a self-describing standard name with a project-local one, and leaves both spellings in place anyway. Keep only the weak_ptr dedup (267 -> 263 lines, all 11 TUs pass):

// lifetime_aware.h, replacing the pasted second copy
// A weak_ptr is the same handle without the strong count: same question.
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>> : is_lifetime_aware<std::shared_ptr<T>> {};

// smart_pointers.h
template <class T>
struct is_sendable<std::weak_ptr<T>> : is_sendable<std::shared_ptr<T>> {};

template <class T>
struct is_synchronizable<const std::weak_ptr<T>>
    : is_synchronizable<const std::shared_ptr<T>> {};

For the "two spellings" half, the fix is a comment, not an alias. Move the existing explanation up to the first bare occurrence (is_sendable<unique_ptr>, smart_pointers.h:16) so the reader meets the reason before the spelling: "unique_ptr is the one indirection that keeps the pointee's cv-qualification, because unique ownership means no other alias can write through it; every shared handle below strips it and asks the full trait." While there, note that the `std::remove_cv_t` in the two is_lifetime_aware rules is dead weight — the primary template already forwards cv-qualified types through remove_cv — so it can simply be deleted.

State in the commit that the inheritance changes one unpinned answer: a user specialization of is_sendable<std::shared_ptr<X>> now propagates to weak_ptr<X>, where it previously did not.

### Reproduction

```text
Applied to a copy of the tree; all 11 test TUs compile, including the weak_ptr
assertions that pin every branch:

  test_smart_pointers.cpp:57  !is_sendable_v<std::weak_ptr<void>> && !is_synchronizable_v<...>
  test_smart_pointers.cpp:79   is_sendable_v<std::weak_ptr<SyncType>>
  test_smart_pointers.cpp:123 !is_synchronizable_v<const std::weak_ptr<const std::string>>
                           &&  is_synchronizable_v<const std::weak_ptr<std::atomic<int>>>
  test_lifetime_aware.cpp:56   is_lifetime_aware_v<std::weak_ptr<int>>
  test_lifetime_aware.cpp:62  !is_lifetime_aware_v<std::weak_ptr<std::span<int>>>
  test_soundness_regressions.cpp:144 !is_lifetime_aware_v<std::weak_ptr<PolyBase>>

  $ for t in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude $t; done
  (all 11 OK)
```

### Vérification

I tried to refute this through the reproduction lens and could not. Every factual claim checks out at the real lines: the weak_ptr rules are byte-identical copies of the shared_ptr rules in all three traits (lifetime_aware.h:52-63, smart_pointers.h:28-34 and 56-62); `grep -c remove_all_extents_t` gives exactly 14 (4 + 10) split 8 cv-wrapped / 6 bare; all six cited static_asserts exist and pin the branches; the shared_ptr/weak_ptr/reference_wrapper answers genuinely live in two headers. I copied the tree, applied the proposed fix verbatim, and all 11 test TUs compile — the claimed repro reproduces exactly.

Two things I would correct, both measured rather than argued. First, the `pointee` alias half of the fix does not simplify: applied fully it takes smart_pointers.h 66->65 and lifetime_aware.h 201->199 while adding 5 lines to utils.h, a net +2 lines across the tree. It replaces `std::remove_all_extents_t` — a name every C++ reader knows — with a project-local `detail::pointee` the conference audience must look up, and the name is inaccurate for the case it exists for (`pointee<T[]>` yields the element, not the pointee). It also leaves both spellings in place (`remove_cv_t<pointee<T>>` vs `pointee<T>`), so it does not fix the complaint it was written for; the explanatory comment does. Second, I verified the weak_ptr dedup alone is the entire win: 267->263 lines, all 11 TUs pass.

I also found one overstatement in the finding: the cv distinction is not entirely unexplained. smart_pointers.h:43-47 says "The one indirection that trusts the pointee's const: unique ownership means no other alias can write through it", and CLAUDE.md states the rule. The weakness is placement — that comment sits on `is_synchronizable<const unique_ptr>`, twenty lines after the first bare spelling appears on `is_sendable<unique_ptr>`. Separately, the `remove_cv_t` in the two lifetime rules is dead weight: the primary template already forwards cv-qualified types through `remove_cv`, so it changes no answer.

Finally, a consequence the finding does not mention and no test pins: the inheritance is not purely mechanical. I compiled a probe where a user writes `template <> struct is_sendable<std::shared_ptr<Widget>> : std::true_type {}`. HEAD answers `is_sendable_v<std::weak_ptr<Widget>> == false` (it re-asks is_synchronizable<Widget>); the patched tree answers true. The change is in the right direction — lock() hands back exactly the shared_ptr the user blessed, so HEAD's answer is a false negative — but it should be stated in the commit rather than discovered later.

Net: the duplication is real, the repro is honest, and the fix works; only the alias half should be dropped.

## F44 — detail::cow_is_sendable is a single-use consteval helper whose if constexpr is behaviorally inert; inlining the two-term && states the copy_on_write rule where the reader looks for it

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | probable |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:43` |
| **Même défaut que** | `F47` — le helper cow_is_sendable |

### Le problème

`cow_is_sendable<T>()` opens a `detail` namespace, defines a consteval function template and an `if constexpr` chain, all to compute `is_sendable_v<T> && is_synchronizable_v<const T>`. The `if constexpr` buys no protection here: `is_synchronizable_v<const T>` is well-formed for every T that reaches this rule. It is an abstraction used once, and it hides the interesting part — the two-condition rule that makes copy_on_write safe — behind a name that does not state it. Every other helper type in this library states its rule inline in the specialization.

### Le code concerné

```cpp
namespace detail {
template <class T>
consteval bool cow_is_sendable() {
    if constexpr (is_sendable_v<T>)
        return is_synchronizable_v<const T>;
    else
        return false;
}
}

template <class T>
struct is_sendable<copy_on_write<T>>
    : std::bool_constant<detail::cow_is_sendable<T>()> {};
```

### La correction

// Readers only ever see a const T and a writer copies before touching a shared
// block: sendable to hand the value over, const-synchronizable for the shared reads.
template <class T>
struct is_sendable<copy_on_write<T>>
    : std::bool_constant<is_sendable_v<T> && is_synchronizable_v<const T>> {};

(delete the detail namespace and cow_is_sendable entirely; 13 lines -> 5.
Note for the author: the only thing lost is the short-circuit — a non-sendable T
now also pays the const walk, measured at ~20% on a synthetic 60-deep chain and
unobservable on the test suite. If that cost is ever deemed to matter, keep the
guard but say why in a comment; today it reads as ceremony.)

### Reproduction

```text
Applied to a copy of the tree; all 11 test TUs compile, including
tests/test_copy_on_write.cpp (140 lines of assertions over copy_on_write):

  $ for t in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude $t; done
  tests/test_copy_on_write.cpp                 OK
  (all 11 OK)

13 lines -> 6, one namespace and one single-use function removed.
```

### Vérification

I tried to refute the finding on three fronts and failed on all three.

(1) Is the `if constexpr` load-bearing (i.e. does `is_synchronizable_v<const T>` ever hard-error when `is_sendable_v<T>` is false)? I built a patched copy of the tree with the proposed one-liner and compared answers against the real header over 33 types chosen to stress every reject path in `default_is_const_synchronizable`: void, incomplete, abstract, polymorphic non-final, user-written copy ctor, mutable member, reference member, union, empty, enum, array, pointer, cv-qualified, capture-less closure, nested `cow<cow<T>>`, non-copyable T, reference T, plus std::vector<Inc*> (incomplete element), mutex, atomic, variant, function, string_view, span, thread, future, shared_ptr, optional<unique_ptr>, reference_wrapper, synchronized_value, and two self-referential/cyclic types (Rec, Cyc). Both variants produced bit-identical bitmask codes (116608, 4102, 0) and neither produced a hard error or a runaway instantiation. So the guard is behaviorally inert: `is_synchronizable_v<const T>` is well-formed for every T that reaches this rule, exactly as the finding claims.

(2) Does the proposed fix break anything? All 11 test TUs compile clean against the patched include, including tests/test_copy_on_write.cpp with its self-referential SelfRef/PList assertions.

(3) Is it a deliberate documented trade-off? CLAUDE.md says nothing about it; git history shows the helper predates the current trait shape (it was a `constexpr bool` variable specialization first, mechanically carried over to bool_constant) rather than being introduced to solve a stated problem. There is no comment at the site claiming a reason, and the sibling single-use consteval helper in the codebase (detail::compute_dynamic_type_is_known) carries an explicit comment for why its guard is required — this one does not, because there is nothing to say.

The one thing the guard does buy, which the finding overstates as "nothing": short-circuiting skips a second reflective walk when T is not sendable. Measured on a synthetic 60-level non-sendable chain, guarded 0.70s vs unguarded 0.85s (~20%); on the real test suite the difference is unobservable. That is not a compile-time cliff of the kind sendable.h documents (the 38x quadratic-path note), and it only affects false answers. It does not justify a namespace plus a single-use consteval function in a library whose stated first-class requirement is being readable on a conference slide, and it hides the two-condition rule that is the whole point of copy_on_write behind a name that does not state it.

So the finding survives. Confidence "likely" rather than "certain" only because of that small measured instantiation cost — a reviewer who weighs compile-time budget heavily could defend keeping the short-circuit, though they would need to add the comment explaining it.

## F45 — `threadsafe::function_type` is a single-use public concept wrapping `std::is_function_v`, which the library also spells two other ways

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | probable |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/synchronizable.h:13` |

### Le problème

`function_type` is declared in the public `threadsafe` namespace, used by exactly one specialization in the same file, and shadows the meaning of the existing `std::meta::is_function_type`. Three files away, lifetime_aware.h expresses the identical idea with an inline `requires std::is_function_v<F>` — so the library teaches two spellings of one rule, and neither header tells the reader why functions are special. Dropping the concept removes a public name, removes the indirection, and makes the two traits read the same way.

### Le code concerné

```cpp
template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
struct is_synchronizable<F> : std::true_type {};
```

### La correction

```cpp
Drop the concept and constrain the specialization inline, matching the form lifetime_aware.h:36-38 already uses:

    // Code is not data: a function has no state to race on.
    template <class F>
        requires std::is_function_v<F>
    struct is_synchronizable<F> : std::true_type {};

`<type_traits>` is already included and `std::is_function_v` is still used, so no include changes. Keep the comment — the rule is not self-evident, and neither header currently states why functions answer true. (Optionally, for full consistency, lifetime_aware.h:137's `is_function_type(remove_pointer(type))` is the same question on the reflection side; that one is correctly spelled since it operates on a `std::meta::info`, so leave it.)
```

### Reproduction

```text
`function_type` has exactly one use:
$ grep -rn "function_type" include/ tests/ | grep -v is_function_type
include/threadsafe/details/synchronizable.h:14:concept function_type = std::is_function_v<F>;
include/threadsafe/details/synchronizable.h:16:template <function_type F>

The form proposed is the one lifetime_aware.h:36-38 already uses.
Applied to a copy of the tree; all 11 test TUs compile:
  $ for t in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude $t; done
  (all 11 OK)
```

### Vérification

I tried to refute this three ways and it survived all three.

(1) Does the repro show what it claims? Yes. `grep -rn "function_type" include/ tests/` returns exactly three lines: the concept's definition (synchronizable.h:14), its single use (synchronizable.h:16), and an unrelated `std::meta::is_function_type` call in lifetime_aware.h:137. So `threadsafe::function_type` really is a public-namespace concept with one use site, in the same file that defines it. It is also the only concept in the library that is not a trait face: the other public concepts are `sendable`, `lifetime_aware`, `std_wrapper`, and the two launcher constraints — each names a library question. `function_type` just names `std::is_function_v`.

(2) Is the proposed fix even legal? I doubted it — a partial specialization whose argument list is `<F>` is identical to the primary template's, and is only well-formed because a constraint makes it more specialized. That is true of the current concept form too, so both spellings rely on the same rule. I applied the patch to a copy of the tree and it compiles.

(3) Does it break anything? No. All 11 test TUs compile with the patch. I also wrote a probe exercising plain, noexcept, variadic, and abominable (`int(double) const&`) function types plus `is_sendable<F*>` / `is_sendable<F&>` / `is_lifetime_aware<F*>`, and the original and patched trees give byte-identical answers (including identically failing on `is_synchronizable_v<Plain*>` and on a pointer-to-member-function, which is a different matter and not this finding's concern). So the specialization is load-bearing — `is_sendable<T*>` forwards to `is_synchronizable<T>`, which is what makes function pointers sendable — but the *concept wrapper* around it is not.

Two parts of the prior auditor's explanation are overstated and I am correcting them rather than letting them carry the finding: `threadsafe::function_type` does not "shadow" `std::meta::is_function_type` — different identifier, different namespace, different domain (a type vs a `std::meta::info`), and no lookup ever sees both. What is actually true, and is a stronger version of the same point, is that the library spells the single question "is this a function" three different ways across three files: the concept here, `requires std::is_function_v<F>` at lifetime_aware.h:37, and `std::meta::is_function_type` at lifetime_aware.h:137. For a codebase whose stated purpose is to be read on a conference stage, one predicate wearing three faces is the defect; the unused public name is a secondary cost.

The severity is right at low — nothing is wrong, nothing is unsound, and a reader is not misled. It is a consistency cleanup that removes a name from the public namespace and makes two traits read alike.

## F46 — copy_on_write::as_mutable's acquire fence is the library's only hand-written memory ordering and the only unexplained line in a codebase that comments every other subtlety — and its justification is the departing owner's reads, not its writes

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | probable |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:35` |
| **Même défaut que** | `F34` — la barrière mémoire non commentée de as_mutable |

### Le problème

An `else` branch whose entire body is `std::atomic_thread_fence(std::memory_order_acquire)` will stop an audience cold. It is the only raw memory-ordering primitive in a library whose whole pitch is that you do not write these by hand, and it is unexplained while the surrounding file explains far more obvious things at length. Whatever the reasoning is — pairing the relaxed `use_count()` read with the release performed by the thread that dropped the other reference, so the sole owner sees that thread's writes — it needs to be on the slide, or the line reads as cargo cult.

### Le code concerné

```cpp
T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }
```

### La correction

```cpp
Keep the fence (it is load-bearing) and state the correct reason. Do not add the braces — the file's own style is brace-less single-statement branches, and that change is unrelated:

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            // Sole owner: write in place. No other owner can have *written* —
            // every handle but this one hands out `const T&`. But another
            // owner may have been *reading* right up to the moment it dropped
            // its handle, and use_count() is a relaxed load that orders
            // nothing. The acquire pairs with the release that reader
            // performed on the reference count, putting its last read before
            // this write. Without it the two race.
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }

Reject the originally proposed comment: "the acquire ... makes that owner's writes to the value visible here" describes a hazard this class structurally cannot have.
```

### Reproduction

```text
Comment only; no behaviour change. (I did not attempt to verify that the stated
reasoning is the correct justification for the fence — that is a soundness
question. The readability point stands either way: the line currently carries
no justification at all.)
```

### Vérification

I tried to refute this and could only partially. Verified facts:

1. `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/copy_on_write.h:35` is exactly as quoted, and `grep -rn "atomic_thread_fence" include/` returns that one line only. So "the only raw memory-ordering primitive in the library" is TRUE.
2. `grep -c "^\s*//\|/\*"` per header: copy_on_write.h = 0. utils.h = 40, synchronizable.h = 33, allowed_std_wrappers.h = 28, sendable.h = 24, lifetime_aware.h = 24. So the line has no comment (TRUE) but the finding's stated contrast — "the surrounding file explains far more obvious things at length" — is FABRICATED: that file has zero comments. The real contrast is with the *rest of the library*, which does comment subtle reasoning heavily. The finding is right for a reason it stated wrongly.
3. There is no repro to re-run; the auditor says so. Nothing to falsify there.
4. I sanity-checked the proposed fix: copied the include tree to scratchpad, applied it verbatim, and compiled all 11 test TUs with `g++-16 -std=c++26 -freflection -fsyntax-only`. All clean. So the fix compiles and breaks nothing.

Where the finding is materially wrong is the *content* of the comment it proposes. It says the acquire "makes that owner's writes to the value visible here." Under this class's own discipline no other owner can ever have written: `operator*`/`operator->` hand out `const T&`/`const T*` only (asserted in probe_cowfence_race.cpp), and `as_mutable` detaches the moment `use_count() != 1`, so a non-sole owner's writes land in a *different* block. The write-visibility path is also already covered by the send that moved the handle to the other thread.

What the fence is actually load-bearing for is the opposite direction — the departing owner's *reads*. Owner B reads `*ptr_`, then destroys its handle (release RMW on the use count); A's `use_count()` is a relaxed load, so per [atomics.fences]/3 the acquire fence sequenced after that load synchronizes-with B's release decrement, placing B's read happens-before A's in-place write. Delete the fence and that read/write pair is unordered — a data race. This is the same reason Rust's `Arc::get_mut` runs `acquire!` before handing out `&mut T`.

So: the defect (an unexplained fence, in an educational library whose pitch is that you never hand-write these, in the one file that comments nothing) survives. The proposed patch does not — pasting that wrong justification onto a conference slide is worse than the current silence. Confidence is "likely" rather than "certain" only because "needs a comment" is a judgement call, and because CLAUDE.md says "Avoid useless comments" — though the codebase's own 40-line comment blocks in utils.h show that bar is about trivia, not about load-bearing memory ordering.

## F47 — detail::cow_is_sendable buys no short-circuiting that std::conditional_t does not already give for free — the helper is pure indirection in the one rule a conference audience must read in place

| | |
|---|---|
| **Gravité** | détail |
| **Confiance** | certaine |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/copy_on_write.h:43` |
| **Même défaut que** | `F44` — le helper cow_is_sendable |

### Le problème

The helper exists to avoid instantiating `is_synchronizable_v<const T>` when `T` is not sendable — a real compile-time saving, since the const walk is the expensive one. But `std::conditional_t` gets the same short-circuit for free: the unselected branch is only a template-id and is never instantiated. Collapsing it removes a `detail` namespace, a consteval function template instantiated once per `T`, and the indirection a conference audience has to follow to learn what the rule actually is — the rule becomes readable in place as "sendable T, read-safe const T". I verified the answers are identical across all eleven test files and that both forms short-circuit.

### Le code concerné

```cpp
namespace detail {
template <class T>
consteval bool cow_is_sendable() {
    if constexpr (is_sendable_v<T>)
        return is_synchronizable_v<const T>;
    else
        return false;
}
}

template <class T>
struct is_sendable<copy_on_write<T>>
    : std::bool_constant<detail::cow_is_sendable<T>()> {};
```

### La correction

Delete the `detail` namespace and the `cow_is_sendable` helper (copy_on_write.h:43-55) and write the rule in place, matching the inheritance style already used by the neighbouring `is_lifetime_aware<copy_on_write<T>>` rule:

// Readers only ever see a const T, and a writer detaches before touching a
// shared block -- so a sendable T that is read-safe through const is enough.
template <class T>
struct is_sendable<copy_on_write<T>>
    : std::conditional_t<is_sendable_v<T>, is_synchronizable<const T>,
                         std::false_type> {};

The unselected branch is only a template-id, so `is_synchronizable<const T>` is still not instantiated when T is not sendable — the expensive const walk is skipped exactly as before. `<utility>` and the other includes stay as they are; `<type_traits>` is already included.

### Reproduction

```text
// Whole include tree copied, only this specialization rewritten:
$ for f in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -I$SP/inc_cond $f; done
test_asynchronous_task_launcher.cpp  PASS
test_containers.cpp                  PASS
test_copy_on_write.cpp               PASS
test_deferred_specialization.cpp     PASS
test_diagnostics.cpp                 PASS
test_lifetime_aware.cpp              PASS
test_sendable.cpp                    PASS
test_smart_pointers.cpp              PASS
test_soundness_regressions.cpp       PASS
test_synchronizable.cpp              PASS
test_synchronized_value.cpp          PASS

// short-circuiting preserved -- probe_cow_shortcircuit2.cpp
template <class Tag> struct NotSendable { NotSendable(const NotSendable&); };
template <class Tag> struct threadsafe::is_synchronizable<const NotSendable<Tag>> {
    static_assert(sizeof(Tag) == 0, "INSTANTIATED - no short circuit");
    static constexpr bool value = false;
};
struct Tag {};
static_assert(!threadsafe::is_sendable_v<threadsafe::copy_on_write<NotSendable<Tag>>>);
$ conditional_t  ->  short-circuits
$ current        ->  short-circuits
```

### Vérification

I set out to refute and could not. (1) Reproduction: I copied the include tree, applied the proposed conditional_t specialization verbatim, and compiled all 11 test files with -fsyntax-only — 11/11 PASS, and the unpatched tree also 11/11, so the result is not vacuous. I confirmed the patched header was actually on the include path. (2) The report's short-circuit claim reproduces, and I strengthened it with the control the original lacked: the trap specialization `is_synchronizable<const NotSendable<Tag>>` containing `static_assert(sizeof(Tag)==0)` DOES fire when asked directly (both trees error with "TRAP FIRED"), proving the trap is live; asking `is_sendable_v<copy_on_write<NotSendable<Tag>>>` compiles cleanly under both the consteval helper and the conditional_t form, so both genuinely avoid instantiating `is_synchronizable<const T>` when T is not sendable. The reason is standard: a template-id in an unselected `conditional_t` branch is only named, never required to be complete. (3) Answer equivalence: I compiled and RAN a 32-type differential (scalars, `void`, `int&`, `int[4]`, pointers, mutable/atomic/reference members, user-written copy ctor, lambda-typed member, std::string/vector/mutex/atomic/shared_ptr/unique_ptr/array, nested copy_on_write, abstract class) against both trees — outputs byte-identical, and both programs built, so no type makes `const T` ill-formed under the always-named form. The only behavioral difference I could construct is that `conditional_t` inherits the user's `is_synchronizable<const T>` specialization directly, so a contract-violating specialization that does not derive from `std::bool_constant` leaves `is_sendable<copy_on_write<T>>` without `::value_type`/`operator()`. That is not a valid objection: the same inheritance-from-a-trait pattern is already used in five places in this library, including `is_lifetime_aware<copy_on_write<T>> : is_lifetime_aware<T>` on the very next line of the same file, and `is_synchronizable<std::atomic<T>> : is_sendable<T>`. The fix makes the file more internally consistent. The finding stands as a legitimate simplicity nit: the helper buys nothing the language does not already give for free, and costs a `detail` namespace plus an indirection an audience must chase.

## F48 — Lambda-in-fold for per-argument checking, where the same file already uses a plain fold over a named consteval helper twice

| | |
|---|---|
| **Gravité** | détail |
| **Confiance** | probable |
| **Axe** | simplicity |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:48` |

### Le problème

`(..., [] { ... }())` is the standard workaround for putting statements in a fold, but on a slide it is four pieces of syntax stacked — comma fold, unary left fold, lambda, immediate invocation — where the reader wants to see "do this for every argument". Extracting the body into a named consteval function makes the fold read as one line, removes the duplicated message text, and yields slightly better grammar in the diagnostic ("argument" rather than "arguments" for the single argument that failed).

### Le code concerné

```cpp
template <class F, class... Args>
consteval void assert_ownable_by_launcher() {
    if (!std::move_constructible<F>)
        throw std::meta::exception(
            u8"the launcher owns its callable, so a non-movable one cannot "
            u8"cross; share it with std::ref instead",
            ^^F);

    (..., [] {
        if (!std::move_constructible<Args>)
            throw std::meta::exception(
                u8"the launcher owns its arguments, so a non-movable one "
                u8"cannot cross; share it with std::ref instead",
                ^^Args);
    }());
}
```

### La correction

```cpp
Prefer a string-free extraction that mirrors the file's own existing idiom (no <string>/<string_view> needed, message unchanged), and insert it BELOW the comment block at lines 36-39 so that comment still sits on assert_ownable_by_launcher:

template <class Arg>
consteval void assert_argument_ownable_by_launcher() {
    if (!std::move_constructible<Arg>)
        throw std::meta::exception(
            u8"the launcher owns its arguments, so a non-movable one cannot "
            u8"cross; share it with std::ref instead",
            ^^Arg);
}

template <class F, class... Args>
consteval void assert_ownable_by_launcher() {
    if (!std::move_constructible<F>)
        throw std::meta::exception(
            u8"the launcher owns its callable, so a non-movable one cannot "
            u8"cross; share it with std::ref instead",
            ^^F);

    (assert_argument_ownable_by_launcher<Args>(), ...);
}

This makes the fold read exactly like the neighbouring `(assert_sendable<Args>(), ...)` at line 62. The originally proposed role-string version also works, but it adds a stringly-typed parameter and two includes for a one-word difference; take it only if the singular "argument" wording is wanted.
```

### Reproduction

```text
Applied to a copy of the tree; all 11 test TUs compile, and the diagnostic is
the same sentence:

$ cat probe_simplicity_launcher_msg.cpp
#include <threadsafe/threadsafe.h>
struct Immovable { Immovable() = default; Immovable(Immovable&&) = delete; };
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Immovable) {}, Immovable{});
}

original:  'the launcher owns its arguments, so a non-movable one cannot cross; share it with std::ref instead'
rewritten: 'the launcher owns its argument, so a non-movable one cannot cross; share it with std::ref instead'

$ for t in tests/*.cpp; do g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude $t; done
(all 11 OK)
```

### Vérification

I read the real line (asynchronous_task_launcher.h:48) and it matches the quoted code. The natural refutation — "lambda-in-fold is this library's deliberate idiom for per-pack work" — is contradicted by the same file: explain_launch_task/explain_launch_scoped_task at lines 62-63 and 73 already use the plain `(assert_sendable<Args>(), ...)` form over named consteval helpers. So the lambda-in-fold is inconsistent with its own immediate neighbours, and CLAUDE.md documents no trade-off covering diagnostic style (it only asks for explicit names and few comments, both of which the plain fold satisfies at least as well). The second refutation angle — that the proposed fix would not compile, because building a u8string inside a consteval function and handing it to std::meta::exception is a transient-allocation hazard — also fails: details/utils.h:55-60 already throws a concatenated std::u8string, so the pattern is established and it compiled. I applied the proposed fix to a copy of the tree: all 11 test TUs compile and the diagnostic becomes the same sentence with "argument" instead of "arguments". I also built a simpler variant that needs no string concatenation and no new includes; it likewise passes all 11 TUs and reproduces today's message byte-for-byte, so I prefer it as the corrected fix. Confidence is "likely" rather than "certain" only because simplicity is a judgement axis and this is a nit — the mechanics are proven, but whether a conference audience is better served is ultimately the author's call. One caveat the original finding missed: the comment block at lines 36-39 describes assert_ownable_by_launcher, so the extracted helper must go below that comment or the comment ends up stranded over the wrong function.

---

# Performance à la compilation

Mesures réelles, pas d'impressions : chaque chiffre vient d'une compilation chronométrée.

## F49 — Every `false` trait answer renders a diagnostic message that the trait catches and discards — the rendering, not the throw, makes a negative answer cost 3.4x a positive one, and skipping it also makes `describe()` dead code

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | compile-perf |
| **Emplacement** | `include/threadsafe/details/utils.h:54` |

### Le problème

`default_is_sendable` / `default_is_const_synchronizable` / `default_is_lifetime_aware` all answer `false` by catching an exception that `reject` throws. When the trait is asking (rather than `assert_*`), `path` is empty and nobody ever reads the message — yet `reject` still calls `describe()`, which runs `u8display_string_of` and three `std::u8string` concatenations, and then pays the full consteval throw/unwind. Measured on 400 distinct types (min-of-5, `-fsyntax-only`): 400 types answering true = 0.92s vs 0.66s control (0.65 ms/type); 400 types answering false = 1.58s vs 0.65s control (2.33 ms/type). Isolated outside the library at N=800, a negative answer by `return false` costs 0.0125 ms, by throwing a literal-message exception 0.56 ms (45x), and by throwing a rendered-message exception 1.04 ms (83x). `path.empty()` already means exactly "the trait is asking and will discard this", so the message can be skipped with no loss — and this also makes `describe()` (utils.h:20-31) dead code, removing 12 lines from an educational codebase.

### Le code concerné

```cpp
[[noreturn]] inline consteval void reject(std::meta::info subject,
                                          std::u8string_view reason,
                                          std::u8string_view path = {}) {
    throw std::meta::exception(
        (path.empty() ? describe(subject) : std::u8string(path)) + u8" "
            + std::u8string(reason),
        subject);
}
```

### La correction

```cpp
The proposed fix is correct as written; apply it and delete `describe()` (utils.h:20-31).

    // An empty path means the trait is asking: it catches this to answer
    // `false` and never reads the message. Rendering one costs more than the
    // walk that found the reason.
    [[noreturn]] inline consteval void reject(std::meta::info subject,
                                              std::u8string_view reason,
                                              std::u8string_view path = {}) {
        if (path.empty())
            throw std::meta::exception(u8"", subject);

        throw std::meta::exception(
            std::u8string(path) + u8" " + std::u8string(reason), subject);
    }

Two notes that refine the finding rather than change the patch:

- Keep the message empty. The tempting variant `throw std::meta::exception(reason, subject)` — literal string_view, no concat, no `u8display_string_of` — looks strictly nicer but measures 1.192s on the false-400 probe versus 0.966s for the empty message, recovering only about half the win. The `u8""` is load-bearing.

- Do not go further and try to remove the throw itself. After this patch a negative answer costs about the same as a positive one (0.324s vs 0.277s net over control), so the exception-based control flow that keeps `diagnose_*` as the single source of truth for both the trait and `assert_*` is not a measurable cost and should stay — that shared structure is the point of the design.

`describe()` is safe to delete: `reject` is its only caller repo-wide, and `type_name`/`member_name` stay live through `path_step`.
```

### Reproduction

```text
$ python3 -c '
print("#include <threadsafe/threadsafe.h>\nusing namespace threadsafe;")
for i in range(400):
    print(f"struct T{i} {{ int a; double b; char c; int *bad; }};")
    print(f"static_assert(!is_sendable_v<T{i}>);")' > false400.cpp

# same file with the static_asserts removed = control
$ time g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude false400.cpp

Measured (min of 5 runs each):
  control (declarations only)      0.65 s
  400 x is_sendable_v  -> true     0.92 s   (net 0.26 s = 0.65 ms/type)
  400 x is_sendable_v  -> false    1.58 s   (net 0.93 s = 2.33 ms/type)
  400 x false, WITH the fix above  1.03 s   (net 0.38 s = 0.95 ms/type)

-> the fix removes 59% of the cost of a negative answer.
All 11 test TUs (including tests/test_diagnostics.cpp, which asserts on
message content) still compile clean with the patch applied.

Library-independent decomposition, N=800 consteval evaluations that answer
"no", control 0.21 s:
  return false                                      0.21 s  (net 0.01 s)
  render u8display_string_of + concat, no throw     0.40 s  (net 0.19 s)
  throw/catch meta::exception, literal message      0.66 s  (net 0.45 s)
  throw/catch meta::exception, rendered message     1.04 s  (net 0.83 s)
```

### Vérification

I tried to refute this four ways and it survived all of them.

1) Is the code as described? Yes. `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/utils.h:54-61` is verbatim the quoted `reject`. `describe()` (lines 20-31) has exactly one caller in the whole repo — `reject` itself (grep over `include/` and `tests/` returns only the definition site and utils.h:58). And `path.empty()` really does mean "the trait is asking": the only three call sites that enter a walk with a default/empty path are `default_is_sendable` (sendable.h:171-178), `default_is_const_synchronizable` (synchronizable.h:206) and `default_is_lifetime_aware` (lifetime_aware.h:190), and all three wrap the call in `try { ... } catch (const std::meta::exception &) { return false; }` — the message is constructed and immediately discarded. Every `assert_*` entry point seeds a non-empty path (`detail::type_name(^^T)`), so no user-visible diagnostic ever comes from the `path.empty()` branch.

2) Do the numbers reproduce? Yes, closely. Min-of-5, `-fsyntax-only`, my own probe files: control 0.589s / 400x true 0.866s / 400x false 1.538s. Net-of-control that is 0.277s vs 0.949s, i.e. a negative answer costs 3.4x a positive one (finding said 3.6x — same effect, my machine).

3) Does the fix work? Yes, and slightly better than claimed. Patched tree: false-400 drops 1.538s -> 0.966s against a 0.665s control, so the net cost per negative answer falls from 0.949s to 0.324s — a 66% reduction (finding claimed 59%). After the fix a negative answer (0.324s net) costs about the same as a positive one (0.277s net), which the original 3.4x gap no longer shows.

4) Does it break anything? No. All 11 test TUs compile clean against the patched tree. More importantly I checked the thing the fix could plausibly break — diagnostic text. I built a probe with 8 failing `assert_sendable` / `assert_synchronizable` / `assert_lifetime_aware` calls covering nested members, bases, references, polymorphic pointees, the opt-in `is_synchronizable<T>` message and the user-written-copy-member message, and diffed the compiler's `what()` output between the original and patched trees: byte-identical. I then deleted `describe()` outright from the patched tree and recompiled all 11 test TUs — clean, confirming the 12 dead lines.

One inaccuracy in the finding, immaterial to the claim: it says `tests/test_diagnostics.cpp` "asserts on message content". It does not — it only asserts that the agreeing half compiles and that the trait still answers a plain `false`. The message-content check I ran myself is what actually establishes the fix is behavior-preserving.

The one thing worth correcting is the finding's attribution of cost. Its library-independent decomposition implies the consteval throw/unwind is itself a 45x term, which reads as "there is a second, larger problem here". In this library that is not what dominates: removing only the rendering brings a negative answer down to roughly the cost of a positive one, so the rendering is the whole gap and the exception-based control flow is not worth restructuring. I confirmed this from the other side by testing an alternative fix that keeps a non-empty message (`throw std::meta::exception(reason, subject)` — no concat, no `u8display_string_of`, just the literal): 1.192s, recovering only about half the win. So the proposed fix's empty message is the right choice, not an over-reach.

On severity: "high" is generous in absolute terms. The library's own test suite has 152 negative asserts, and interleaved min-of-5 whole-suite runs give 7.494s original vs 7.166s patched — a real, consistent 4.4% win (~2.2 ms per negative answer, matching the per-type figure), but not dramatic. The stronger argument for the fix in an educational codebase is the 12 lines of `describe()` that go away for free. The claim itself is technically true about this exact code, so I report real=true.

## F50 — No precompiled header on the test target: 11 TUs re-parse the same 140,249-line umbrella, costing ~7.0s of the 7.5s serial suite — but a PCH only helps serial and incremental builds, and makes a clean `-j` build ~30% slower

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | probable |
| **Axe** | compile-perf |
| **Emplacement** | `tests/CMakeLists.txt:3` |

### Le problème

All 11 test TUs include the same umbrella header, which costs 0.58s of the 0.58-0.65s each TU takes. The suite therefore re-parses the identical 140,249 preprocessed lines eleven times. Enabling a PCH on the OBJECT library cuts the compile phase from 7.62s to 3.68s (2.07x), with a one-time 1.39s PCH build — so even a fully clean build drops from 7.62s to 5.07s (1.50x), and every incremental rebuild is 2.07x. This is orthogonal to the header split above and composes with it.

### Le code concerné

```cpp
add_library(threadsafe_tests OBJECT
    test_synchronizable.cpp
    test_sendable.cpp
    ...
)
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)
```

### La correction

```cpp
The mechanical change is correct as proposed and does compile:

    target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)

    # All 11 TUs include the same 140k-line umbrella (0.64s each to parse).
    # Parsing it once cuts the serial suite 7.5s -> 3.5s and a parallel
    # incremental rebuild 1.6s -> 0.8s. Note the 1.4s PCH compile is a serial
    # prologue: a clean `-j` build gets ~30% SLOWER (1.9s -> 2.5s), and the
    # .gch adds 135 MB to the build tree.
    target_precompile_headers(threadsafe_tests PRIVATE
        <threadsafe/threadsafe.h>)

But do not land it unconditionally. Pick based on how the suite is actually built:

1. Cheapest real fix, no downside, no disk cost: stop building serially. The project ships the Unix Makefiles generator and CLAUDE.md documents `cmake --build build` with no -j, which is where the entire 7.6s comes from. Documenting `cmake --build build -j` (or setting the Ninja generator, or CMAKE_BUILD_PARALLEL_LEVEL) takes the suite to ~1.9s with zero configuration change and zero 135 MB artifact. That is a bigger win than the PCH and it composes with nothing needed.

2. If the PCH is still wanted on top of that, gate it so it does not penalize clean parallel builds:

    option(THREADSAFE_TESTS_PCH "Precompile the umbrella header for the test suite" OFF)
    if(THREADSAFE_TESTS_PCH)
        target_precompile_headers(threadsafe_tests PRIVATE <threadsafe/threadsafe.h>)
    endif()

   ON is right for an iterating developer (parallel incremental 1.6s -> 0.84s, serial 7.5s -> 4.3s); OFF is right for CI and for a conference attendee doing one clean build.

For an educational repo shown at a conference, I would take (1) and skip the PCH entirely — a 135 MB .gch and an option knob are more CMake surface than the talk needs.
```

### Reproduction

```text
$ time cmake --build build_compileperf --clean-first
  ... 6.77s user 0.69s system 97% cpu 7.628 total

$ time (for f in tests/*.cpp; do \
    g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude $f; done)
  7.622 total          # no PCH, matches the cmake build

$ echo '#include <threadsafe/threadsafe.h>' > pchdir/ts_pch.h
$ time g++-16 -std=c++26 -freflection -x c++-header -Iinclude \
      pchdir/ts_pch.h -o pchdir/ts_pch.h.gch
  1.39 total           # 135 MB .gch

$ time (for f in tests/*.cpp; do \
    g++-16 -std=c++26 -freflection -fsyntax-only \
      -Ipchdir -include ts_pch.h -Iinclude $f; done)
  3.681 total          # 2.07x faster, all TUs still clean

Per-TU times without PCH (min of 1 run each, control 0.58s empty):
  0.65 test_soundness_regressions   0.61 test_smart_pointers
  0.64 test_synchronizable          0.61 test_diagnostics
  0.64 test_containers              0.61 test_asynchronous_task_launcher
  0.63 test_sendable                0.60 test_synchronized_value
  0.62 test_copy_on_write           0.60 test_lifetime_aware
                                    0.58 test_deferred_specialization
```

### Vérification

I tried to refute this and could not knock out its core. Every factual claim reproduced within noise on my own runs: tests/CMakeLists.txt genuinely has no target_precompile_headers, all 11 TUs include the umbrella, the umbrella is exactly 140,249 preprocessed lines and costs 0.64s per TU, and a PCH is genuinely usable with -freflection under GCC 16.2 (confirmed with -H, which prints "! ts_pch.h.gch" — the PCH is consumed, not silently ignored and re-parsed). Serial suite drops 7.51s -> 3.48s. The proposed CMake fix configures and builds cleanly on an OBJECT library, produces all 11 objects, and a static_assert(false) canary still fires, so the suite is not being silently skipped.

Where the finding overreaches, and the reason I am "likely" rather than "certain": the 7.6s baseline exists only because the repro built serially. The generator is Unix Makefiles and `cmake --build build` (the exact command CLAUDE.md documents) defaults to one TU at a time. Add -j and the picture inverts for the case the finding leads with. Measured on 12 cores:
  clean, no PCH, -j : 1.75 / 1.96 / 1.96 s
  clean, PCH,    -j : 2.52 / 2.55 / 2.34 s   <-- 25-45% SLOWER
The 1.38s PCH compile is a serial prologue that cannot overlap anything, in front of 11 TUs that otherwise finish concurrently in ~0.65s of wall time. So the finding's "even a fully clean build drops from 7.62s to 5.07s (1.50x), and every incremental rebuild is 2.07x" is stated unconditionally but is serial-only. Parallel incremental is still a genuine win (1.60s -> 0.84s); parallel clean is a regression.

Two smaller inaccuracies. The per-TU table lists "control 0.58s empty" — an actually empty TU compiles in 0.03s here; 0.58-0.64s is the umbrella cost, so the control is mislabeled (this does not change the conclusion, it is the same number wearing the wrong name). And the finding omits the footprint: the .gch is 135 MB, taking the build directory from 796 KB to 145 MB — a real consideration for a repo whose stated purpose is being cloned and built by conference attendees.

Net: the defect (no PCH; 11x redundant parse of an identical 140k-line header) is real and the fix is safe and effective on the workflow the project itself documents. The claimed magnitude is conditional on serial builds in a way the write-up does not disclose, and the fix actively regresses the parallel clean build, so severity is lower than "medium" as framed and the fix deserves a caveat rather than a bare recommendation.

---

# Performance à l'exécution

Les traits doivent ne rien coûter au runtime ; les wrappers, eux, coûtent.

## F51 — synchronized_value<T> selects std::shared_mutex for every sendable value-like T (int, string, vector, map) with no opt-out, so the common short-critical-section case pays a reader-writer lock: ~90-115x slower on contended writes, ~19x on contended short reads, 208 vs 72 bytes

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | runtime-perf |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:53` |

### Le problème

get_mutex_type() returns ^^std::shared_mutex whenever is_synchronizable_v<const T>. But is_synchronizable<const T> is the *structural* trait, so it is true for int, long, Point{double,double}, std::string and std::vector<int> — i.e. for every type one would plausibly put in a synchronized_value. There is no template parameter and no other opt-out, so the user cannot ask for a plain mutex. A reader-writer lock only pays off when read critical sections are long and read-dominated; synchronized_value's critical sections are whatever the user does under one guard, and the shipped default is the pessimal one. On this box (macOS, libstdc++ shared_mutex = pthread_rwlock_t) it is 35% slower uncontended, ~18x slower for 8 concurrent short reads, and ~105x slower for 4-8 concurrent writes, plus 208 bytes vs 72 for synchronized_value<long>. I confirmed the gap is entirely the mutex type by re-running the same loop with a hand-rolled std::shared_mutex + unique_lock, which reproduces the 1734 ns/op figure exactly.

### Le code concerné

```cpp
static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];
```

### La correction

```cpp
Give the caller the choice, but keep the trait as the gate — and carry the new parameter through every place that names the class. In include/threadsafe/details/synchronized_value.h:

    enum class read_sharing { exclusive, shared };

    // line 16-17 — forward declaration must match
    template <class T, read_sharing Sharing = read_sharing::exclusive>
    class synchronized_value;

    // line 36-37 — inside value_guard
    template <class, read_sharing>
    friend class synchronized_value;

    template <class T, read_sharing Sharing>
    class synchronized_value {
        static_assert(sendable<T>, ...);
        static_assert(Sharing == read_sharing::exclusive
                          || is_synchronizable_v<const T>,
                      "shared reads need a const T that is safe to read from "
                      "several threads at once");

        static constexpr bool share_reads = Sharing == read_sharing::shared;

    public:
        using mutex = std::conditional_t<share_reads,
                                         std::shared_mutex, std::mutex>;
        using guard = value_guard<T, std::unique_lock<mutex>>;
        using const_guard = value_guard<
            const T, std::conditional_t<share_reads,
                                        std::shared_lock<mutex>,
                                        std::unique_lock<mutex>>>;
        // ...
    };

    // lines 102-106 — MUST carry Sharing, or the shared form silently
    // loses both traits and stops being cross-thread shareable
    template <class T, read_sharing Sharing>
    struct is_synchronizable<synchronized_value<T, Sharing>> : is_sendable<T> {};

    template <class T, read_sharing Sharing>
    struct is_lifetime_aware<synchronized_value<T, Sharing>>
        : is_lifetime_aware<T> {};

Then update tests/test_synchronized_value.cpp:108-111 to spell the shared flavour explicitly:

    using sync_int_shared =
        threadsafe::synchronized_value<int, threadsafe::read_sharing::shared>;
    static_assert(std::same_as<sync_int_shared::const_guard,
                               threadsafe::value_guard<
                                   const int,
                                   std::shared_lock<std::shared_mutex>>>,
                  "lock_shared — readers of a const-synchronizable T really share");
    static_assert(std::same_as<sync_int::const_guard,
                               threadsafe::value_guard<
                                   const int, std::unique_lock<std::mutex>>>,
                  "the default is the cheap lock; sharing reads is asked for");

Note that `std::conditional_t` replaces both `consteval` splices, which is a readability win for a conference audience. The lesson is preserved and arguably sharpened: the trait still makes `read_sharing::shared` unreachable for a T whose const form is not read-safe (the Memo case at tests/test_synchronized_value.cpp:120-128 becomes a hard error rather than a silent downgrade), but the caller — who alone knows whether reads dominate and how long the critical sections are — picks. If the author prefers to keep the current default for pedagogical reasons, the minimum acceptable change is to document the cost at the point of the choice, since as shipped nothing in the header warns that `synchronized_value<int>` embeds a 200-byte pthread_rwlock_t.
```

### Reproduction

```text
// probe_rtperf_which.cpp — which mutex each T actually gets
#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
#include <cstdio>
template <class T> void report(const char* n){
  std::printf("%-28s -> %-13s sizeof=%zu\n", n,
    std::is_same_v<typename threadsafe::synchronized_value<T>::mutex, std::shared_mutex> ? "shared_mutex":"mutex",
    sizeof(threadsafe::synchronized_value<T>)); }
struct Point { double x, y; };
int main(){ report<int>("int"); report<long>("long"); report<Point>("Point{double,double}");
  report<std::string>("std::string"); report<std::vector<int>>("std::vector<int>"); }

$ g++-16 -std=c++26 -freflection -O2 -I include probe_rtperf_which.cpp -o which && ./which
int                          -> shared_mutex  sizeof=208
long                         -> shared_mutex  sizeof=208
Point{double,double}         -> shared_mutex  sizeof=216
std::string                  -> shared_mutex  sizeof=232
std::vector<int>             -> shared_mutex  sizeof=224

// probe_rtperf_sv_vs_hand.cpp — 300k ++ per thread through the guard
$ ./bench_sv
synchronized_value<long>.lock()  1 thr:      7.1 ns/op
std::mutex + long                1 thr:      5.1 ns/op
synchronized_value<long>.lock()  4 thr:   1588.7 ns/op   <-- 100x
std::mutex + long                4 thr:     15.9 ns/op
synchronized_value<long>.lock()  8 thr:   1375.0 ns/op   <-- 94x
std::mutex + long                8 thr:     14.7 ns/op
sizeof(synchronized_value<long>)=208  vs hand-rolled=72

// probe_rtperf_confirm.cpp — same loop, no library, isolates the mutex type
$ ./bench_confirm
shared_mutex + unique_lock (write)       4 thr:   1734.8 ns/op
mutex + unique_lock (write)              4 thr:     16.5 ns/op
shared_mutex + unique_lock (write)       8 thr:   1684.1 ns/op
mutex + unique_lock (write)              8 thr:     13.4 ns/op

// probe_rtperf_bench_readers.cpp — short *read* critical sections, the case
// shared_mutex is supposed to win
$ ./bench_readers
mutex, exclusive read            8 threads:     15.9 ns/op
shared_mutex, shared read        8 threads:    288.4 ns/op   <-- 18x slower

// probe_rtperf_sv.cpp — the generated code for synchronized_value<int>::lock()
__Z4bumpRN10threadsafe18synchronized_valueIiEE:
	bl	_pthread_rwlock_wrlock      ; not pthread_mutex_lock
	ldr	w0, [x19, 200]              ; the int lives 200 bytes in
```

### Vérification

I tried to refute this on four fronts and it survived all of them.

1. Source match. `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/synchronized_value.h:53-61` is verbatim what the finding quotes. `mutex` is a public typedef spliced from `get_mutex_type()`, gated only on `is_synchronizable_v<const T>`. Lines 16-17, 36-37, 71-72, 92-95, 102-106 confirm there is exactly one template parameter and no policy hook anywhere.

2. "Essentially every T". Reproduced. int, double, long, Point{double,double}, std::string, std::vector<int>, std::map<int,std::string>, std::vector<std::string> all select std::shared_mutex. I looked for a large escape class and could not find one that matters: the only T I found landing on std::mutex are `struct{mutable int}` and `struct{unique_ptr<int>}`. And the space of admissible T is already narrowed by the `static_assert(sendable<T>)` at line 48 — `struct{int*}` and `struct{shared_ptr<int>}` are rejected outright, so the wrapper's whole domain is sendable value-like types, which is precisely the set for which `is_synchronizable<const T>` is true. If anything this strengthens the claim rather than refuting it.

3. Opt-out. There genuinely is none. The only lever is specializing `is_synchronizable<const MyType>` to false, which would poison every other trait query for that type across the library — not an opt-out, a lie. `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` pushes the other direction.

4. The numbers, and whether the mutex type is really the cause. Re-measured independently with my own harness (300k ops/thread, std::barrier start, per-thread timing). The gap is real and, on this box, larger than reported: 4 threads 5226 vs 58 ns/op (~90x), 8 threads 16775 vs 144 ns/op (~115x). The prior auditor's causal control also holds up — my hand-rolled `std::shared_mutex + std::unique_lock` on a bare `long` tracks `synchronized_value<long>` almost exactly (4 thr: 5309 vs 5236; 8 thr: 15193 vs 10306), so the cost is the mutex type, not the wrapper. Read side reproduces too: 8 concurrent short reads, `shared_lock<shared_mutex>` 3035 ns/op vs `unique_lock<mutex>` 160 ns/op (~19x — shared_mutex loses at the very case it exists for, at these critical-section lengths). sizeof confirmed: `sizeof(std::shared_mutex)==200`, `sizeof(std::mutex)==64`, so 208 vs 72 for `synchronized_value<long>` is right.

Two honest caveats that shade the framing but do not touch the truth of the claim:

- "Silently" is unfair. The behavior is deliberate and pinned by the suite: `tests/test_synchronized_value.cpp:108-111` asserts `sync_int::const_guard == value_guard<const int, std::shared_lock<std::shared_mutex>>` with the comment "readers of a const-synchronizable T really share", and lines 120-128 assert the std::mutex downgrade for a T with a mutable member. This is the pedagogical punchline of the header — the trait decides which lock you are entitled to. So this is a design tradeoff with a known cost, not an oversight.
- The 100x magnitude is a Darwin `pthread_rwlock_t` property. The direction (rwlock slower than mutex for short critical sections) holds on glibc too, but the multiplier there is single-digit to low-double-digit, not 100x. The finding does scope itself to "on this box", so it is not overclaiming.

The proposed fix, however, does not survive as written — three concrete breakages, which is why I supply a corrected one:
(a) `template <class T> struct is_synchronizable<synchronized_value<T>>` (line 102) and `is_lifetime_aware<...>` (line 105) stop covering the shared form. With a defaulted second parameter, the pattern `synchronized_value<T>` deduces only `Sharing == exclusive`; `synchronized_value<int, shared>` falls through to the primary template and answers false, i.e. the shared-reads flavour would no longer be shareable across threads — the exact opposite of the point. I compiled a minimal model of this and the static_assert fails.
(b) The forward declaration at lines 16-17 and the `template <class> friend class synchronized_value;` at lines 36-37 both need the extra parameter or `value_guard`'s private constructor becomes unreachable.
(c) Flipping the default to `exclusive` fails the pinned `static_assert` at tests/test_synchronized_value.cpp:108-111. That test has to be updated to spell `synchronized_value<int, read_sharing::shared>` — which is arguably good, since it makes the lesson explicit, but the fix as proposed does not mention it and the suite currently builds clean (I verified with cmake + g++-16).

## F52 — launch_task retains every finished jthread for the launcher's whole life: 200k tasks cost 100 s / 3.3 GB where the same thread-per-task loop with the record reaped costs 1.1 s / 8.9 MB (retention, not thread-per-task, is 90x of the cost)

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | runtime-perf |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:118` |
| **Même défaut que** | `F53` — les threads terminés jamais récupérés |

### Le problème

launch_task pushes a fresh std::jthread into threads_ and nothing ever pops it. There is no join_all(), no reap of finished threads, and no pooling — the vector only drains when the launcher is destroyed. So the launcher spawns one OS thread per task and retains the joinable thread record for the whole life of the launcher. Measured with a task body of one relaxed fetch_add: 4000 tasks cost 15.1 us each; 200000 tasks cost 91 s wall and 2.33 GB peak RSS (~11.6 KB retained per finished task). The identical 200000 tasks through a minimal condition_variable thread pool cost 0.26 s and 8.9 MB — 352x faster and 260x smaller. For a conference demo this is the shape people copy, so the cost is worth naming even if pooling is out of scope.

### Le code concerné

```cpp
void launch_task(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }

private:
    std::vector<std::jthread> threads_;
```

### La correction

```cpp
The auditor's `join_all()` bounds the growth but deadlocks on the stop_token tasks the class explicitly blesses. Mirror std::jthread's own destructor contract — stop, then join:

    // Drain: request stop before joining, exactly as ~jthread does, so a task
    // written against the injected stop_token ends instead of hanging the drain.
    void request_stop_and_join_all() {
        for (std::jthread &task : threads_)
            task.request_stop();
        for (std::jthread &task : threads_)
            if (task.joinable())
                task.join();
        threads_.clear();
    }

Two caveats to state alongside it. (1) This is opt-in, so it bounds `threads_` only by caller convention — it is a drain, not a bound. (2) If a plain "wait for natural completion" is also wanted, it must be a separate, separately-named member; do not overload one `join_all()` to mean both.

The real bound, if a pool is in scope: fixed jthread workers built in the constructor plus a `synchronized_value<std::queue<std::move_only_function<void()>>>` of pending work, with `launch_task` enqueueing rather than spawning. The traits are untouched — `launchable_task` still gates what may be enqueued — and the existing static_assert-only test suite is unaffected either way (I confirmed all 11 test TUs still compile against a patched header).
```

### Reproduction

```text
// probe_rtperf_bench_accum.cpp — thread-per-task cost
$ ./bench_accum
4000 launch_task: 60.5 ms wall; ~15.1 us/task

// probe_rtperf_bench_limit.cpp — 200000 launch_task calls, body is one fetch_add
$ /usr/bin/time -l ./bench_limit
       91,05 real         0,67 user       102,14 sys
          2333392896  maximum resident set size       <-- 2.33 GB
          3348136432  peak memory footprint

// probe_rtperf_pool.cpp — identical 200000 tasks through a minimal pool
$ /usr/bin/time -l ./pool
thread pool, 200000 tasks: 258.5 ms total, 1.29 us/task
        0,62 real         0,09 user         0,94 sys
             8880128  maximum resident set size       <-- 8.9 MB
```

### Vérification

The code claim is exact: `asynchronous_task_launcher::launch_task` (asynchronous_task_launcher.h:89-91) emplaces a fresh `std::jthread` into `threads_` (line 118) and nothing ever pops, joins, or reaps it — the vector drains only in the implicit destructor. There is no `join_all`, no in-flight bound, no pooling.

I rebuilt the repro from scratch with a `launchable_task`-legal payload (captureless lambda + `std::shared_ptr<std::atomic<int>>`, since raw pointers and `reference_wrapper` are rejected by the traits) and reproduced the numbers: 4000 tasks -> 15.01 us/task (auditor: 15.1); 200000 tasks -> 100.8 s wall, 109 s sys, peak memory footprint 3,348,070,920 B (auditor: 3,348,136,432 B — an essentially exact match, so it is the same phenomenon). Max RSS came out 1.24 GB on my run vs the reported 2.33 GB; that single number is run-dependent under memory pressure, and the linear scan (20k -> 331 MB, 50k -> 825 MB, i.e. ~16.5 KB retained per finished task) predicts the 3.3 GB footprint the auditor and I both measured. So the title's "2.3 GB" is a run-specific RSS figure, not a fabricated one, and the order of magnitude (GB vs MB) is solid.

I then tried the strongest available refutation: that the cost is inherent to one-OS-thread-per-task, not to the failure to reap — which would make the pool comparison apples-to-oranges and the title's causal claim wrong. It is not. A control with the identical thread-per-task shape and identical payload, differing only in that the thread record is reaped (`std::thread` + `detach()`), ran the same 200000 tasks in 1.12 s at 8.9 MB peak. Retention alone therefore accounts for ~90x of the time and ~140x of the memory; thread-per-task alone costs 5.6 us/task. The degradation is also superlinear in retained threads (15 -> 57 -> 133 -> 502 us/task at 4k/20k/50k/200k), consistent with the 109 s of kernel time spent in vm_map with 200k live stacks.

I also verified the proposed fix: I patched a private copy of the header with the auditor's `join_all()`, and it compiles, all 11 test translation units still pass `-fsyntax-only` (the suite is static_assert-only and never touches the member), and calling it every 256 tasks brings 200000 tasks to 1.83 s / 8.9 MB. But the proposed body is defective as written: it calls `join()` without `request_stop()`, so it does NOT honor the contract that `std::jthread`'s own destructor honors. A task written against the injected `stop_token` — precisely the case the class's static_assert at lines 82-84 exists to bless — hangs the drain forever. I confirmed this: a launcher with one `[](std::stop_token stop){ while(!stop.stop_requested()) yield(); }` task was still running 3 s after `join_all()` and had to be SIGKILLed, whereas the current destructor terminates it fine. So the fix needs correcting, and it is worth noting that an opt-in drain only bounds growth by caller convention.

Severity: the defect is real and the measurements hold, but "high" is calibrated to a 200k-task workload that is not the shape of a conference demo; for the artifact's stated purpose I would call it medium — the reason to name it is exactly the one the auditor gives, that this is the code people copy.

## F53 — launch_task never reaps finished tasks: each completed jthread's stack stays resident until the launcher is destroyed (~16 KB/task measured, 2 MB -> 163 MB over 10k tasks), so a launcher used as a long-lived dispatcher grows without bound. (The proposed erase_if fix is a no-op — a finished jthread is still joinable.)

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | certaine |
| **Axe** | runtime-perf |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:90` |
| **Même défaut que** | `F52` — les threads terminés jamais récupérés |

### Le problème

Every launch_task appends a std::jthread and nothing ever removes one. A completed task's jthread stays joinable-but-finished in the vector, holding an OS thread handle (and, on most platforms, an unreclaimed thread stack) until the launcher is destroyed. A long-lived launcher used as a general dispatcher therefore leaks one thread handle per task, and its destructor then has to walk every entry ever created. Combined with the sequential-shutdown behaviour above, shutdown cost grows with the total number of tasks ever launched rather than with the number still running.

### Le code concerné

```cpp
void launch_task(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }
```

### La correction

```cpp
Do NOT apply the proposed `std::erase_if(threads_, !joinable)` — measured to erase 0 of 4000 finished entries while adding an O(n) scan per launch.

Preferred fix for an educational library (simplicity-first, per CLAUDE.md): keep the one-line `launch_task` and make the scope-bounded contract explicit in the type's documentation and name, since the vector-of-jthread design already gives correct join-all-at-destruction semantics for its intended use:

    // An asynchronous_task_launcher is a scope-bounded task group: it owns every
    // task it launches and joins them all in its destructor. It does not reap
    // finished tasks, so each completed task's thread stack stays resident until
    // the launcher itself dies. Give a launcher the lifetime of the work it
    // launches; do not keep one alive as a process-wide dispatcher.
    class asynchronous_task_launcher { ... };

Only if actual reaping is wanted does it need completion tracking, which costs real complexity (a shared flag per task, set by a wrapper around `f`, plus care around the `stop_token` that `std::jthread` injects ahead of `Args...`):

    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    void launch_task(F f, Args... args) {
        std::erase_if(tasks_, [](const running_task& task) {
            return task.finished->test();     // finished threads join instantly
        });
        auto finished = std::make_shared<std::atomic_flag>();
        tasks_.emplace_back(
            std::jthread{[finished, f = std::move(f)](auto&&... forwarded) mutable {
                f(std::forward<decltype(forwarded)>(forwarded)...);
                finished->test_and_set();
            }, std::move(args)...},
            finished);
    }

That is a lot of machinery for a conference talk; I would take the documentation fix.
```

### Reproduction

```text
Read from the source: launch_task (line 89-91) only ever calls
threads_.emplace_back, and `threads_` (line 118) is touched nowhere else in
the class -- no erase, no clear, no shrink. grep over the whole header:

$ grep -n 'threads_' include/threadsafe/details/asynchronous_task_launcher.h
90:        threads_.emplace_back(std::move(f), std::move(args)...);
118:    std::vector<std::jthread> threads_;
```

### Vérification

I tried three refutation angles and all failed on the core claim, but the last one demolished the proposed fix.

1. Is the code claim literally true? Yes. `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/asynchronous_task_launcher.h` line 90 is the only write to `threads_`, and line 118 is its only other mention. There is no erase, clear, shrink, or reap anywhere in the class. `launch_scoped_task` (lines 107-110) does not touch the vector at all — it constructs a local jthread and joins it — so the growth is specific to `launch_task`.

2. Is the growth actually observable, or is it just a vector of 8-byte handles (i.e. cosmetic)? I measured it. Resident set grows ~16 KB per completed-but-unjoined task and never comes back until the launcher dies: 2 MB baseline -> 34 MB (2k tasks) -> 66 MB (4k) -> 98 MB (6k) -> 131 MB (8k) -> 163 MB (10k), then back to 3 MB immediately after the launcher is destroyed. 10 000 jthread objects are only ~80 KB of vector, so the 160 MB is retained pthread stack/TCB pages, not container overhead. The finding's mechanism is right.

3. Is the shutdown-cost half true? Yes but it is small: destroying a launcher holding 10 000 already-finished jthreads took 12.6 ms. It does scale with total tasks ever launched, not with tasks still running, exactly as claimed.

One wording correction to the finding: on macOS the *kernel* thread is reclaimed at task exit — `task_threads()` reported `live_kernel_threads=1` the whole time, even with 10 000 unjoined finished tasks. So "holding an OS thread handle" overstates it; what is actually retained is the userspace pthread record and its stack, plus the process-wide pthread count that eventually caps thread creation. The resource leak is real, the attribution needs a small edit.

Where the finding is wrong is the PROPOSED FIX, and the auditor's own trailing comment already concedes it. `std::jthread::joinable()` stays true for a thread that has run to completion — it only goes false for default-constructed or moved-from objects. I transcribed the proposed body verbatim into a struct and ran it: after 4000 launches with an 800 ms quiesce in the middle, `std::erase_if` removed 0 entries, the vector still held 4000, and RSS was unchanged. A standalone check confirms the predicate directly: 100 of 100 finished jthreads still report `joinable() == true`, and `erase_if` returns 0. So the fix compiles cleanly and would not break the test suite (which is `static_assert`-only and never instantiates a launcher at runtime), but it is a semantic no-op that adds an O(n) scan to every launch — strictly worse than the status quo.

Verdict: finding survives, proposed fix does not. Severity "low" is right for an educational library, and given CLAUDE.md's simplicity-first mandate the honest remedy is the documentation/naming one the auditor mentions parenthetically, not the erase_if.

---

# API et flexibilité

Ce qu'un utilisateur doit taper, ce qu'il peut apprendre à la bibliothèque, et ce qu'il ne peut pas.

## F54 — `is_sendable` is the only one of the three traits not specialized for `synchronized_value`, so `is_sendable_v<synchronized_value<T>>` completes the class through `is_complete_type` and detonates its `static_assert` instead of answering `false` (also reachable via `std::vector<synchronized_value<T>>`); the other two traits answer cleanly

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:103` |

### Le problème

`is_synchronizable` and `is_lifetime_aware` are both specialized for `synchronized_value`, so asking them short-circuits before the structural walk. `is_sendable` is not, so `is_sendable_v<synchronized_value<T>>` falls into `detail::default_is_sendable`, which calls `is_complete_type(type)` (sendable.h:139) — that *completes* the class, which detonates the `static_assert(sendable<T>)` on line 48. The trait stops being a question you can ask: it answers `true`, or it kills the translation unit. This is exactly backwards from the library's own error-reporting design, where a "no" is supposed to travel as a `std::meta::exception` that `default_is_sendable` catches. It also defeats the nice diagnostics: `launch_task`'s fallback calls `assert_sendable<F>()`, which for any `F` reaching a bad `synchronized_value` dies on the raw static_assert instead of producing the walked path message.

### Le code concerné

```cpp
template <class T>
struct is_synchronizable<synchronized_value<T>> : is_sendable<T> {};

template <class T>
struct is_lifetime_aware<synchronized_value<T>> : is_lifetime_aware<T> {};
```

### La correction

```cpp
Add the missing sibling specialization next to the two that already exist, in include/threadsafe/details/synchronized_value.h:

    template <class T>
    struct is_synchronizable<synchronized_value<T>> : is_sendable<T> {};

    template <class T>
    struct is_sendable<synchronized_value<T>> : is_sendable<T> {};

    template <class T>
    struct is_lifetime_aware<synchronized_value<T>> : is_lifetime_aware<T> {};

A matching partial specialization keeps the primary template — and therefore `is_complete_type` — out of the picture, so the class is never implicitly instantiated by a trait query. For a sendable T the answer is unchanged (`default_is_sendable` already returned true early via `is_synchronizable_type`), so the specialization only converts the hard error into `false`, and it states the intent explicitly instead of leaving it to be re-derived through the synchronizable short-circuit.

Do NOT sell this as a diagnostics fix — verified that it changes no `assert_sendable` / `launch_task` message.
```

### Reproduction

```text
#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct NotSendable { int* borrowed; };
constexpr bool answer = is_sendable_v<synchronized_value<NotSendable>>;
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I<include> probe_syncval_query.cpp
.../synchronized_value.h: In instantiation of 'class threadsafe::synchronized_value<NotSendable>':
.../sendable.h:139:26:   required from 'struct threadsafe::is_sendable<threadsafe::synchronized_value<NotSendable> >'
  139 |     if (!is_complete_type(type))
.../synchronized_value.h:48:19: error: static assertion failed: the mutex serializes
 access, but the T still crosses thread boundaries ...

// The other two traits, which ARE specialized, answer cleanly:
static_assert(!is_synchronizable_v<synchronized_value<NotSendable>>);   // OK
static_assert(!is_lifetime_aware_v<synchronized_value<int*>>);          // OK
```

### Vérification

I tried to refute this on the reproduction lens and failed — the repro is exact.

VERIFIED (all re-run by me):
1. The claimed repro compiles and produces literally the claimed diagnostic chain: `is_sendable_v<synchronized_value<NotSendable>>` -> primary `is_sendable` -> `default_is_sendable` -> `diagnose_default_is_sendable` -> `is_complete_type(type)` at sendable.h:139 -> implicit instantiation of `synchronized_value<NotSendable>` -> `static_assert(sendable<T>)` at synchronized_value.h:48. Hard error, not `false`.
2. The contrast is real: `!is_synchronizable_v<synchronized_value<NotSendable>>` and `!is_lifetime_aware_v<synchronized_value<int*>>` both static_assert cleanly. The two traits that ARE specialized answer; the one that is not detonates.
3. The location is right: lines 102-106 of synchronized_value.h hold exactly the two quoted specializations, with no `is_sendable` sibling.
4. The proposed fix works and is not a regression: I copied the include tree, inserted `template <class T> struct is_sendable<synchronized_value<T>> : is_sendable<T> {};` between the two existing specializations, and compiled ALL ELEVEN tests/*.cpp with g++-16 -std=c++26 -freflection -fsyntax-only. Every one passed. With the fix, `!is_sendable_v<synchronized_value<NotSendable>>` and `is_sendable_v<synchronized_value<int>>` both hold.
5. There is a second, less contrived trigger the finding did not mention, which strengthens it: `is_sendable_v<std::vector<synchronized_value<NotSendable>>>` also hard-errors. `std::vector` tolerates an incomplete element type, so merely *naming* that vector is legal; the allowed-std-wrapper rule then asks `is_sendable` about the element and the trait query itself is what completes the class and fires the static_assert.

WHAT I DID REFUTE (two overstatements, reflected in the corrected title/severity):
- The diagnostics claim is not substantiated. I could not build an `F` where `assert_sendable<F>()` loses its walked-path message to this. Every indirection short-circuits to the specialized `is_synchronizable`: `is_sendable_v<synchronized_value<Bad>&>`, `...*`, and `std::shared_ptr<synchronized_value<Bad>>` all answer a clean `false` today, unpatched. Capturing lambdas stop earlier on `has_unreflectable_state`. And in the one case that does reach it (a named struct with a `std::vector<synchronized_value<Bad>>` member handed to `launch_task`), the static_assert still fires WITH the fix applied — from concept checking of `std::movable`, not from the trait — while the walked-path message ("Task::items ... has a user-written copy, move or destructor") is emitted identically in both builds. So the fix does not buy better diagnostics; it buys an askable trait.
- Severity "high" is too strong. `synchronized_value<Bad>` is a type nobody can instantiate, and every by-value containment other than the incomplete-tolerant std containers already completes the class at its own definition, so the static_assert fires there first regardless. The blast radius is: a direct trait query on `synchronized_value<Bad>`, or on a `std::vector`-family wrapper of it. That is an API-consistency wart in a library whose stated design is that a "no" travels as a `std::meta::exception` — not a soundness or usability hole for well-formed code. Low/medium.

The finding survives on its own terms: the trait is not a question you can ask, the fix is two lines next to the two that already exist, and the suite stays green.

## F55 — allowed_std_wrappers is a closed 18-template list, so common std vocabulary (chrono::duration, bitset, complex, expected, queue, stack, valarray, flat_map) is not sendable — launch_task(f, 5ms) does not compile

| | |
|---|---|
| **Gravité** | majeur |
| **Confiance** | certaine |
| **Axe** | flexibility |
| **Emplacement** | `include/threadsafe/details/allowed_std_wrappers.h:39` |

### Le problème

allowed_std_wrappers is a closed list of 18 templates. Anything else falls to the structural walk, where may_hijack_copy_move (utils.h:133) rejects any type carrying a constructor template — which is most std vocabulary types. The result is that std::chrono::milliseconds is not sendable, so launch_task(f, 5ms) does not compile; nor are std::bitset, std::complex, std::expected, std::queue, std::stack, std::valarray, std::flat_map, or std::chrono::system_clock::time_point. A duration is a single arithmetic member and is the single most likely argument to hand a worker thread. The user's only recourse is to hand-write three specializations per type, and nothing documents that the synchronizable one must be written on const X rather than X.

### Le code concerné

```cpp
inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,        ^^std::deque,             ^^std::list,
    ^^std::forward_list,  ^^std::basic_string,      ^^std::map,
    ^^std::multimap,      ^^std::set,               ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset,
    ^^std::pair,          ^^std::tuple,             ^^std::optional,
    ^^std::variant,       ^^std::array,
};
```

### La correction

```cpp
Same shape as proposed, with the chrono rules tightened to derive from the wrapped type rather than hardcoding true.

In allowed_std_wrappers.h, extend the list (and add the matching includes <complex>, <expected>, <flat_map>, <flat_set>, <queue>, <stack>, <valarray>):

    ^^std::expected,      ^^std::queue,             ^^std::stack,
    ^^std::priority_queue, ^^std::flat_map,         ^^std::flat_set,
    ^^std::valarray,      ^^std::complex,

In vocabulary.h (add <bitset>, <chrono>, <cstddef>), state the non-template-argument families directly — but derive, do not assert:

    template <class Rep, class Period>
    struct is_sendable<std::chrono::duration<Rep, Period>> : is_sendable<Rep> {};
    template <class Rep, class Period>
    struct is_lifetime_aware<std::chrono::duration<Rep, Period>> : is_lifetime_aware<Rep> {};
    template <class Rep, class Period>
    struct is_synchronizable<const std::chrono::duration<Rep, Period>> : is_synchronizable<const Rep> {};
    template <class Clock, class Duration>
    struct is_sendable<std::chrono::time_point<Clock, Duration>> : is_sendable<Duration> {};
    template <class Clock, class Duration>
    struct is_lifetime_aware<std::chrono::time_point<Clock, Duration>> : is_lifetime_aware<Duration> {};
    template <class Clock, class Duration>
    struct is_synchronizable<const std::chrono::time_point<Clock, Duration>> : is_synchronizable<const Duration> {};
    template <std::size_t N> struct is_sendable<std::bitset<N>> : std::true_type {};
    template <std::size_t N> struct is_lifetime_aware<std::bitset<N>> : std::true_type {};
    template <std::size_t N> struct is_synchronizable<const std::bitset<N>> : std::true_type {};

The proposed unconditional std::true_type for duration/time_point lifetime-aware and const-synchronizable asserts more than reflection knows about a user-supplied Rep; the derived form is strictly tighter and was verified to compile identically.

Separately, the is_lifetime_aware specializations for duration/time_point are not strictly required — is_lifetime_aware already answers true for durations today — but stating them keeps the three answers written together and guards against a future change to the lifetime walk.
```

### Reproduction

```text
// probe_api_stdlist.cpp
#include <threadsafe/threadsafe.h>
#include <bitset>
#include <chrono>
#include <complex>
#include <expected>
#include <valarray>
#include <queue>
#include <stack>
#include <flat_map>
#define ROW(...) static_assert(threadsafe::is_sendable_v<__VA_ARGS__>, #__VA_ARGS__)
ROW(std::bitset<8>);
ROW(std::chrono::milliseconds);
ROW(std::complex<double>);
ROW(std::expected<int, int>);
ROW(std::valarray<int>);
ROW(std::queue<int>);
ROW(std::stack<int>);
ROW(std::flat_map<int, int>);
ROW(std::chrono::system_clock::time_point);

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_stdlist.cpp
error: static assertion failed: std::bitset<8>
error: static assertion failed: std::chrono::milliseconds
error: static assertion failed: std::complex<double>
error: static assertion failed: std::expected<int, int>
error: static assertion failed: std::valarray<int>
error: static assertion failed: std::queue<int>
error: static assertion failed: std::stack<int>
error: static assertion failed: std::flat_map<int, int>
error: static assertion failed: std::chrono::system_clock::time_point
// (all nine fail; std::vector<bool> passes)

// and the reason, via the diagnostic face:
assert_sendable<std::chrono::milliseconds>() ->
 'std::chrono::duration<long long int, std::ratio<1, 1000> > has a user-written copy,
  move or destructor — or a template that may be selected as one — ...'
```

### Vérification

I re-ran the reproduction verbatim against the unmodified tree and all nine static_asserts fail exactly as reported. The quoted diagnostic matches character for character. The location is correct: allowed_std_wrappers.h:39 is the array, and it contains exactly 18 entries; is_allowed_std_wrapper is a closed membership test with no user-extension hook. The practical consequence is real, not speculative: launcher.launch_task(f, 5ms) fails to compile with the 'has a user-written copy, move or destructor — or a template that may be selected as one' exception, because may_hijack_copy_move (utils.h:133) rejects any type carrying a constructor template, which std::chrono::duration has.

I checked the proposed fix end to end. Baseline: all 11 test files compile clean. After applying the fix to a scratchpad copy of the include tree, all nine probe rows pass, launch_task(f, 5ms) compiles, and all 11 test files — including test_soundness_regressions.cpp and test_containers.cpp — still compile clean. So the fix is both effective and non-regressing.

Two corrections, neither of which rescues the code. (1) 'three specializations per type' is overstated: is_lifetime_aware_v<std::chrono::milliseconds> is already true on the unmodified tree, since the lifetime walk does not consult may_hijack_copy_move. Only is_sendable and is_synchronizable<const T> answer false, and a single is_sendable specialization is enough to make launch_task compile (verified, 0 errors). (2) The proposed chrono rules hardcode std::true_type for lifetime-aware and const-synchronizable, asserting more than they know for a user-supplied Rep; deriving from is_lifetime_aware<Rep> / is_synchronizable<const Rep> is strictly tighter and compiles identically.

The one angle that could have refuted this — an existing cheap escape hatch — turns out to cut the other way. THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(std::chrono::milliseconds) is a working one-liner (unqualified synchronizable short-circuits both sendable and lifetime-aware, verified 0 errors), but it asserts that two threads may write a shared milliseconds& concurrently, which is false. The library's only cheap workaround requires the user to state something untrue, which strengthens the finding.

Severity 'high' is defensible for an educational library shown at a conference: std::chrono::milliseconds is close to the most likely argument anyone would hand a worker thread in a live demo, and it does not compile.

## F56 — The unconstrained fallback makes launch_task acceptance undetectable: `requires { launcher.launch_task(bad) }` is true for any trait rejection (and a hard error, never false, for a non-movable one) — same for launch_scoped_task

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/asynchronous_task_launcher.h:93` |

### Le problème

The explaining fallback is an unconstrained overload, so it is always a viable candidate and the call expression is always well-formed at overload-resolution time; the error only appears when the fallback body is instantiated. A caller who guards with `if constexpr (requires { launcher.launch_task(f); })` therefore always takes the "it works" branch and then fails to compile inside it — the exact opposite of what the guard was written for. The test file works around this by asserting on the concepts directly, which shows the problem was noticed but not surfaced: the public escape is `threadsafe::launchable_task<F, Args...>`, and nothing in the header says so.

### Le code concerné

```cpp
// Fallback: the constrained overload is more constrained, so it always wins
    // when it applies. This one is instantiated only on a rejection, and exists
    // only to name it — instantiating it is always a compile error.
    template <typename F, typename... Args>
    void launch_task(F, Args...) {
        detail::explain_launch_task<F, Args...>();
    }
```

### La correction

```cpp
Comment-only, applied to BOTH fallbacks (line 93 and line 112):

    // Fallback: the constrained overload is more constrained, so it always wins
    // when it applies. This one is instantiated only on a rejection, and exists
    // only to name it — instantiating it is always a compile error.
    //
    // Being unconstrained is what makes the diagnostic readable, and it is also
    // why the call is never SFINAE-detectable: `requires { launcher.launch_task(f) }`
    // is true for a rejected call, and hard-errors when f is not movable. Ask
    // `launchable_task<F, Args...>` when you want to detect acceptance rather
    // than trigger it — that is what the tests do.
    template <typename F, typename... Args>
    void launch_task(F, Args...) {
        detail::explain_launch_task<F, Args...>();
    }

and the mirror wording naming `launchable_scoped_task` on the launch_scoped_task fallback, which currently carries no comment at all.
```

### Reproduction

```text
// probe_launcher_detect.cpp
#include <threadsafe/threadsafe.h>
threadsafe::asynchronous_task_launcher launcher;
int captured = 0;
using Bad = decltype([c = captured] { return c; });
static_assert(requires (Bad b) { launcher.launch_task(b); });
static_assert(!threadsafe::launchable_task<Bad>);
static_assert(requires (int* p) { launcher.launch_task([](int*){}, p); });
int main() {}

$ g++-16 -std=c++26 -freflection -fsyntax-only -I.../include probe_launcher_detect.cpp
DETECTION_HOLE_CONFIRMED (both requires-expressions are TRUE)   # compiles clean
```

### Vérification

The code at /Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/asynchronous_task_launcher.h:96-99 (and the identical launch_scoped_task fallback at 112-115) is a fully unconstrained member template. It is therefore a viable candidate for any argument list whose parameters can be initialized, so the call expression is well-formed at overload-resolution time and only explodes when the consteval body is instantiated. Every leg of the repro reproduced exactly.

I tried three refutation angles, all failed:
1. "Maybe the error is in the immediate context and SFINAEs out." No — I compiled an `if constexpr (requires { launcher.launch_task(f, a...); })` guard around a rejected call; the guard takes the true branch and the build then fails inside it with "call to consteval function explain_launch_task<...> is not a constant expression". That is precisely the inverted-guard failure mode described.
2. "Maybe the header or CLAUDE.md already names launchable_task as the detection escape." No — grep over include/ and the repo's .md files finds `launchable_task` mentioned exactly once outside its own definition, as the requires-clause on line 88. Nothing documents it as the way to ask.
3. "Maybe the tests exercise the call, so it isn't really the escape." No — every test (tests/test_asynchronous_task_launcher.cpp:20, test_copy_on_write.cpp:54, test_synchronized_value.cpp:25) defines `can_launch_task = threadsafe::launchable_task<F, Args...>` and asserts on the concept, never on the call. That corroborates the auditor's point rather than refuting it.

One correction to the claim's wording: "always true" is overbroad. When F or an Arg is not move-constructible, the fallback's by-value parameter cannot be initialized and the requires-expression does not become false — GCC 16 emits a hard error ("use of deleted function") from inside the requires-expression. So detection is broken in both directions: silently true for trait rejections, hard error for ownability rejections. Never usefully false. That strengthens rather than weakens the finding.

The proposed fix is comment-only, so it compiles verbatim and cannot affect the test suite. It is the right minimal fix for an educational library: the fallback exists to produce a readable diagnostic, and that goal is fundamentally at odds with detectability, so documenting the escape hatch is the correct resolution rather than restructuring. I would extend it to the launch_scoped_task fallback too, which has the same shape and no comment at all.

Severity medium is fair, arguably low-medium: this is an API-ergonomics/documentation gap, not a soundness hole. No unsafe type is ever accepted; the launch still fails to compile, just at the wrong phase and in a way a SFINAE guard cannot see.

## F57 — synchronized_value exposes no multi-value lock and, because its mutex is private and the type is not Lockable, the caller cannot reach std::scoped_lock either — cross-value invariants deadlock with no ordering rule stated anywhere

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:92` |

### Le problème

lock() and lock_shared() are the only entry points, and each takes exactly one mutex. Any invariant spanning two synchronized_values — a transfer, a move between two containers, a consistent snapshot of a pair — forces the caller to take two guards in sequence, and nothing in the API establishes or even mentions an ordering. Two callers that name the pair in opposite orders deadlock. The same single-mutex shape also makes check-then-act the path of least resistance: `{ auto r = v.lock_shared(); if (r->empty()) ... }` followed by `{ auto w = v.lock(); w->push_back(x); }` compiles cleanly and the predicate is stale by the time the second lock is taken. Task.md's bar is that race conditions be hard to write; here the correct composition is the one the API makes harder.

### Le code concerné

```cpp
[[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }
```

### La correction

The shape is right but does not compile as given: the friend declaration must repeat the requires-clause, otherwise it declares a different template and grants no access to value_/mutex_.

In synchronized_value's public section:
    using value_type = T;

In its private section:
    template <class Operation, class... Values>
        requires std::invocable<Operation&&, typename Values::value_type&...>
    friend decltype(auto) apply_all(Operation&&, Values&...);

At namespace scope, after the class:
// Locks every value at once. std::scoped_lock uses std::lock's deadlock-avoidance
// algorithm, so no ordering rule is needed -- and it works across both mutex types
// synchronized_value can pick.
template <class Operation, class... Values>
    requires std::invocable<Operation&&, typename Values::value_type&...>
decltype(auto) apply_all(Operation&& operation, Values&... values) {
    std::scoped_lock everything{values.mutex_...};
    return std::forward<Operation>(operation)(values.value_...);
}

Two notes worth carrying: <mutex> is already included, so no new dependency; and this covers exclusive access only -- a shared/read-only multi-lock is not expressible through std::scoped_lock's uniform ownership and is a reasonable thing to leave out of an educational API.

### Reproduction

```text
// probe_rt_two_locks.cpp
#include <threadsafe/threadsafe.h>
#include <chrono>
#include <memory>
#include <thread>
using namespace threadsafe;
using Account = synchronized_value<long>;
struct Transfer {
    std::shared_ptr<Account> from, to;
    void operator()() const {
        for (int i = 0; i < 100000; ++i) {
            auto source = from->lock();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            auto target = to->lock();       // no ordering rule anywhere in the API
            *source -= 1; *target += 1;
        }
    }
};
static_assert(is_sendable_v<Transfer> && is_lifetime_aware_v<Transfer>);
int main() {
    auto a = Account::make(0L), b = Account::make(0L);
    asynchronous_task_launcher launcher;
    launcher.launch_task(Transfer{a, b});
    launcher.launch_task(Transfer{b, a});
}

$ ./probe_rt_two_locks
launched
>>> DEADLOCK CONFIRMED (no lock-ordering / multi-lock facility)

Verified separately that std::scoped_lock over the two possible mutex types
compiles: `std::mutex a; std::shared_mutex b; std::scoped_lock lock{a, b};`
```

### Vérification

I attempted to refute on three fronts and failed on all three.

(1) Repro validity. The original repro compiles unchanged and hangs, but its stated evidence was weak: 100000 iterations with a 1us sleep is genuinely slow on macOS, so "still running" alone does not prove deadlock. I rebuilt it with atomic per-thread progress counters (probe_refute_multilock_deadlock2.cpp). Both threads report progress 0 for 8 consecutive seconds, then the launcher destructor hangs on join. Classic ABBA deadlock, confirmed.

(2) "The user can just use std::scoped_lock themselves." Refuted: mutex_ is private and synchronized_value is not Lockable (lock() returns a guard, no try_lock/unlock), so std::scoped_lock everything{a, b}; fails to compile. The standard deadlock-avoidance remedy is unreachable from outside the class; only a library-side addition can supply it. This makes the finding stronger than reported.

(3) "Ordering is documented somewhere." Refuted: grep over include/, tests/ finds no std::lock, no scoped_lock, no mention of lock ordering or deadlock anywhere.

Against Task.md's bar ("Difficile d'avoir des race conditions", "API facile a utiliser", "Flexibilite") the gap is real. Severity medium is right: this is a liveness/ergonomics gap, not a soundness hole in the traits, and Rust's own Mutex has the same single-lock shape - but C++ ships the fix and this wrapper hides the mutex that would let you use it.

The proposed fix needed one correction: the friend declaration omits the requires-clause, so it declares a different template and grants no access (compiler: "value_ is private within this context"). With the constraint repeated on the friend, the fix compiles, eliminates the deadlock, and leaves all 11 test files compiling identically to baseline.

## F58 — `is_synchronizable` is the only trait with no concept face — `sendable` and `lifetime_aware` both ship one, so users cannot constrain a template on the library's central trait

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/synchronizable_base.h:15` |

### Le problème

The public namespace ships concept sendable (sendable.h:40) and concept lifetime_aware (lifetime_aware.h:66), but no concept synchronizable. is_synchronizable is the central trait of the library — the one the CLAUDE.md architecture section opens with — and it is the only one a user cannot constrain a template on. The names are otherwise perfectly regular (is_X / is_X_v / is_X_type / assert_X / concept X), so the gap reads as an oversight and the compiler's suggestion ("did you mean is_synchronizable?") sends the user to the trait rather than the concept.

### Le code concerné

```cpp
template <class T>
constexpr bool is_synchronizable_v = is_synchronizable<T>::value;
```

### La correction

In include/threadsafe/details/synchronizable.h, immediately after the const array specializations (the `is_synchronizable<const T[N]>` / `<const T[]>` pair, ~line 46) and before `assert_synchronizable`, add:

    template <class T>
    concept synchronizable = is_synchronizable_v<T>;

    template <class T>
    concept const_synchronizable = is_synchronizable_v<const T>;

Placement matters for readability rather than correctness (concepts are evaluated at each use, so synchronizable_base.h would also work), but putting them after the const rules keeps the file readable top-to-bottom. Verified: all 11 tests/*.cpp still compile clean against a patched tree.

The `const_synchronizable` half is worth keeping even though the reported gap is only the first: the full trait is opt-in and mostly false, so `is_synchronizable_v<const T>` is the question users actually ask, and spelling it `synchronizable<const T>` is easy to get wrong. Both names are free — the similar detail:: identifiers are in a nested namespace.

### Reproduction

```text
// probe_api_concept_sym.cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
template <threadsafe::sendable T>       void takes_sendable(T) {}
template <threadsafe::lifetime_aware T> void takes_owner(T) {}
template <threadsafe::synchronizable T> void takes_shared(T&) {}
int main() { std::atomic<int> a{}; takes_shared(a); }

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_concept_sym.cpp
probe_api_concept_sym.cpp:6:11: error: 'threadsafe::synchronizable' has not been
  declared; did you mean 'threadsafe::is_synchronizable'?
```

### Vérification

I tried to refute this on the reproduction lens and failed on every attempt.

1. The repro reproduces verbatim. Compiling the auditor's exact source with g++-16 -std=c++26 -freflection -fsyntax-only yields precisely the quoted diagnostic, including the "did you mean 'threadsafe::is_synchronizable'?" suggestion. So the repro does show what it says it shows.

2. Its premises hold. `grep -rn "concept" include/` finds exactly seven concepts: `sendable` (sendable.h:40), `lifetime_aware` (lifetime_aware.h:66), `std_wrapper`, `function_type`, `ownable_by_launcher`, `launchable_task`, `launchable_scoped_task`. There is no `synchronizable`. I isolated the first two constraints into their own probe and it compiles exit 0, confirming those two concepts genuinely work as template constraints and that the third really is the only missing one.

3. The naming-regularity claim holds. is_synchronizable (synchronizable_base.h:12), is_synchronizable_v (:15), is_synchronizable_type (:24), assert_synchronizable (synchronizable.h:51), THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE (:30). Every face of the quintet exists except the concept.

4. The proposed fix compiles and is non-breaking. I copied the include tree to the scratchpad, inserted both concepts into synchronizable.h just after the const array specializations, and: all 11 tests/*.cpp compile clean (including test_deferred_specialization.cpp, the one most likely to be perturbed by concept caching); the original repro now compiles exit 0; and semantics are correct (synchronizable<std::atomic<int>> true, synchronizable<plain> false, const_synchronizable<plain> true, const_synchronizable<with_pointer> false). No collision with threadsafe::detail::default_is_const_synchronizable / descend_const_synchronizable, which are in a nested namespace.

The only thing I would adjust is severity framing, not existence: this is pure API symmetry, zero soundness impact, and the workaround (`requires threadsafe::is_synchronizable_v<T>`) is one line. A separate caveat — concept satisfaction is cached per translation unit, so a `synchronizable<T>` use before a deferred THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE specialization could be stale — turns out to argue *for* the fix rather than against it, since sendable and lifetime_aware already ship with that exact hazard; adding the third concept makes the API consistent rather than introducing a new class of problem.

Finding survives.

## F59 — assert_sendable/assert_synchronizable/assert_lifetime_aware return void, so neither `static_assert(assert_sendable<T>())` nor a bare namespace-scope call compiles — the latter fails as a parse error that never mentions thread safety

| | |
|---|---|
| **Gravité** | moyen |
| **Confiance** | certaine |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/sendable.h:50` |

### Le problème

These are the good diagnostic tools the library ships, but their signature makes them awkward to reach. static_assert(threadsafe::assert_sendable<T>()) — the first thing a user will try — fails with "could not convert from 'void' to 'bool'", and a consteval call is an expression so it cannot appear at namespace scope at all. The only working spellings are static_assert((f<T>(), true)) or burying the call in a dummy function body. Returning bool costs nothing and makes all three spellings work, including use inside synchronized_value's own static_assert.

### Le code concerné

```cpp
template <class T>
consteval void assert_sendable() {
    if (is_sendable_v<T>)
        return;

    detail::descend_sendable(^^T, detail::type_name(^^T));
}
```

### La correction

```cpp
The proposed fix is correct as written and I verified it. In all three headers change the return type to `bool` and the early `return;` to `return true;`:

  // sendable.h:50
  template <class T>
  consteval bool assert_sendable() {
      if (is_sendable_v<T>) return true;
      detail::descend_sendable(^^T, detail::type_name(^^T));
  }

  // synchronizable.h:51 — same, keeping both the non-const `throw` and the
  // trailing detail::descend_const_synchronizable(...) call
  // lifetime_aware.h:77 — same, trailing detail::descend_lifetime_aware(...)

No `return` is needed on the failing paths: `descend_sendable`, `descend_const_synchronizable` and `descend_lifetime_aware` are all `[[noreturn]] inline consteval`, and GCC 16 emits no `-Wreturn-type` under `-Wall -Wextra`.

Two caveats worth folding in, neither of which blocks the change:
- Do NOT add `[[nodiscard]]`. `detail::explain_launch_task` and `detail::explain_launch_scoped_task` (asynchronous_task_launcher.h:58-76) call all three as discarded expressions, as do the ten call sites in tests/test_diagnostics.cpp; `[[nodiscard]]` would turn the whole suite into a warning storm.
- Optionally follow through at the one internal call site that benefits: synchronized_value.h:49's `static_assert(sendable<T>, "...")` can become `static_assert(assert_sendable<T>(), "...")`, which upgrades the failure from the static message to the subobject-naming one. That is a separate judgement call, not required by the fix.
```

### Reproduction

```text
// probe_api_assert_ergo.cpp
#include <threadsafe/threadsafe.h>
namespace app { struct Leaf { int* borrowed; }; }
static_assert(threadsafe::assert_sendable<app::Leaf>());

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_assert_ergo.cpp
probe_api_assert_ergo.cpp:4:53: error: could not convert
  'threadsafe::assert_sendable<app::Leaf>()' from 'void' to 'bool'

// the working, non-obvious spelling:
static_assert((threadsafe::assert_sendable<app::Leaf>(), true));
// -> 'app::Leaf::borrowed (int*) is a pointer or a reference: ...' (the useful message)
```

### Vérification

I tried to refute this on three fronts and failed on all three.

1. The repro is exact. `include/threadsafe/details/sendable.h:50-57` really declares `consteval void assert_sendable()`, and `static_assert(threadsafe::assert_sendable<app::Leaf>())` fails with GCC 16's `could not convert ... from 'void' to 'bool'`. The same shape holds at `synchronizable.h:51` and `lifetime_aware.h:77`.

2. The secondary claim ("cannot appear at namespace scope at all") is also true, and for a nastier reason than a plain error: a bare `threadsafe::assert_sendable<app::Leaf>();` at namespace scope is parsed as a *declaration*, so the diagnostic is `'template<class T> consteval void threadsafe::assert_sendable()' is not a type` followed by `expected unqualified-id` — a parse error that says nothing about thread safety. Only `static_assert((f<T>(), true))` or burying the call in a `consteval` function body (what `tests/test_diagnostics.cpp` does) works today; both produce the genuinely good message.

3. The fix is sound. I copied the include tree, changed all three `void`→`bool` / `return;`→`return true;`, and compiled all eleven files in `tests/` with `-Wall -Wextra`: zero errors, zero warnings. No `-Wreturn-type` fires because each failing path ends in a `[[noreturn]] consteval` `descend_*` call, exactly as the finding states. `explain_launch_task` / `explain_launch_scoped_task` discard the value and are unaffected (they compiled). The `synchronized_value` sub-claim also holds: a class-scope `static_assert(assert_sendable<T>(), "...")` in a class template instantiated with a bad `T` yields `uncaught exception ... 'Leaf::borrowed (int*) is a pointer or a reference: ...'` — strictly more informative than the current bare `static_assert(sendable<T>, "...")`.

The only counter-argument I can construct is a design one, not a correctness one: a `bool` that can only ever be `true` (the false path always throws) is arguably a misleading signature, and the library already ships `is_sendable_v<T>` / `sendable<T>` as the honest predicate for `static_assert`. But that is a taste argument about the fix, not a refutation of the claim — the claim as written is that the natural spelling does not compile, and it does not. Severity "medium" on the api axis is right: this is ergonomics on a diagnostic tool, no soundness or usability (false-negative) consequence for the traits themselves.

## F60 — Function references are the sole outlier in is_lifetime_aware: `void()` and `void(*)()` answer true, `void(&)()` answers false

| | |
|---|---|
| **Gravité** | mineur |
| **Confiance** | probable |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/lifetime_aware.h:30` |

### Le problème

lifetime_aware.h:38 already carves function pointers out of the raw-pointer rule, because a function has static storage duration and a pointer to it can never dangle. The identical argument applies to `void(&)()`, but it is swallowed by the blanket `is_lifetime_aware<T&> : std::false_type`. A struct holding a `void (&callback)()` — a perfectly ordinary non-owning-but-never-dangling callback slot — is therefore rejected, while the same struct holding `void (*callback)()` is accepted. On a slide about "references borrow, pointers borrow, functions are special", the asymmetry is the kind of thing that gets noticed.

### Le code concerné

```cpp
template <class T>
struct is_lifetime_aware<T&> : std::false_type {};
template <class T>
struct is_lifetime_aware<T&&> : std::false_type {};
template <class T>
struct is_lifetime_aware<T*> : std::false_type {};

template <class F>
    requires std::is_function_v<F>
struct is_lifetime_aware<F*> : std::true_type {};
```

### La correction

The proposed fix is correct as written. Add, immediately after the existing function-pointer carve-out at include/threadsafe/details/lifetime_aware.h:38:

template <class F>
    requires std::is_function_v<F>
struct is_lifetime_aware<F&> : std::true_type {};

Verified: compiles, orders unambiguously against `is_lifetime_aware<T&>`, and all 11 files in tests/ still pass.

Two amendments:
- Do NOT also add an `F&&` form. `void(&&)()` stays false after the fix, but deduction never produces it from a function lvalue, so the extra specialization buys an unreachable case at the cost of a third near-duplicate block in a header whose readability is a stated first-class requirement.
- No change is needed at lifetime_aware.h:136. That reject ("is a reference or a raw pointer") already excludes function pointers via `!is_function_type(remove_pointer(type))`, and with the fix a function reference makes `assert_lifetime_aware` return early at line 78, so the stale wording becomes unreachable rather than wrong.

Optionally in scope, same root cause: `is_lifetime_aware<std::reference_wrapper<T>>` at line 47 is unconditionally false, so `std::reference_wrapper<void()>` is false too. Probably leave it — a `reference_wrapper` over a function is rarer still, and the blanket rule reads better on a slide.

### Reproduction

```text
Before (./probe_la_survey, ./probe_la_survey2):
  void(*)()                  TRUE
  void(&)()                  false
  struct FnRefMember { void (&fn)(); };   false

After adding the four-line specialization in probe_la_fixes2.cpp
(g++-16 -std=c++26 -freflection -I.../include, no other change):
  void(&)()                  TRUE
  FnRefMember                TRUE
  int&                       false     <- unchanged, no over-reach
  void(*)()                  TRUE      <- unchanged
```

### Vérification

I tried three refutation angles and all failed.

(1) "Deliberate documented trade-off?" — No. CLAUDE.md (read in full) says only "True if a `T` owns its data or keeps its referent alive. Ownership is transitive". It never mentions function pointers, function references, or any reference carve-out. The only in-repo statement of intent is the test comment at tests/test_soundness_regressions.cpp:190, `"functions have static storage duration"`, which is precisely the argument that applies verbatim to `void(&)()`. Nothing documents the reference case as an intentional exclusion.

(2) "Maybe the reference is semantically weaker than the pointer?" — The opposite. `void(&)()` cannot be null and cannot be rebound; it is strictly safer than `void(*)()`. There is no safety story that admits the pointer and rejects the reference.

(3) "Maybe it's unreachable in practice?" — Partly, and this is the one real dent in the finding, which is why severity low is correct. `asynchronous_task_launcher::launch_task(F f, Args... args)` takes by value, so a function lvalue decays to a function pointer before the trait is ever asked; `launcher.launch_task(free_function)` compiles fine today. The bite is limited to a direct trait query or a `void (&)()` data member. But the trait is public API and the member form is legal C++, so "unreachable" is too strong.

What actually strengthens the finding: the outlier is narrower than reported. I measured all three forms and the *bare function type* also answers true — `is_lifetime_aware_v<void()>` is TRUE (a function type is neither class nor union, so `diagnose_default_is_lifetime_aware` falls through line 162 and returns). So the library says `void()` TRUE, `void(*)()` TRUE, `void(&)()` FALSE. The reference is the single dissenter among its own family, which makes an accidental-omission reading much more likely than a deliberate-simplicity reading. (`std::reference_wrapper<void()>` is FALSE too, via the blanket specialization at lifetime_aware.h:47 — same root cause, one more instance.)

Fix verification: I copied the include tree, added the four lines, and compiled all 11 test files in tests/ with `-fsyntax-only`. All 11 PASS. The constrained partial specialization orders correctly against `is_lifetime_aware<T&>`, exactly as `is_lifetime_aware<F*>` already does against `is_lifetime_aware<T*>` — no ambiguity error. `int&` and `int*` stay false; `void(*)()` stays true. The diagnostic branch at lifetime_aware.h:136 needs no change: `assert_lifetime_aware` returns early once the trait is true, so a function reference can no longer reach that reject message.

One correction to the proposed fix: it is incomplete. `void(&&)()` remains false because it matches `is_lifetime_aware<T&&>` at line 32 (verified). That form is essentially unreachable — deducing `F&&` from a function lvalue yields `F&` — so I would leave it alone rather than add a fourth specialization to an educational header, but the auditor should know the asymmetry is not fully closed.

## F61 — synchronized_value's two alias-computing consteval helpers are public API (callable, used nowhere but the two splices beside them), and threadsafe::function_type is a single-use restatement of std::is_function_v living in the public namespace instead of detail

| | |
|---|---|
| **Gravité** | détail |
| **Confiance** | certaine |
| **Axe** | api |
| **Emplacement** | `include/threadsafe/details/synchronized_value.h:53` |
| **Même défaut que** | `F40`, `F42` — les splices et helpers consteval de synchronized_value |

### Le problème

get_mutex_type() and get_const_guard_type() exist only to compute the mutex and guard aliases, but they sit under public: and are callable from user code, so they appear in the interface of the library's flagship type alongside lock() and make(). Separately, threadsafe::function_type (synchronizable.h:14) is a general-purpose spelling of std::is_function_v exported into the public namespace, where it collides conceptually with anything a user might name the same; it is used exactly once, to constrain one partial specialization.

### Le code concerné

```cpp
public:
    static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }
```

### La correction

```cpp
In synchronized_value.h, move `get_mutex_type()` and `get_const_guard_type()` into the implicit private section at the top of the class (before the `public:` label), but keep the three aliases public — the fix is only viable in this shape:

    template <class T>
    class synchronized_value {
        static_assert(sendable<T>, ...);

        static consteval auto get_mutex_type() { ... }
        static consteval auto get_const_guard_type() { ... }

    public:
        using mutex = [:get_mutex_type():];
        using guard = value_guard<T, std::unique_lock<mutex>>;
        using const_guard = [:get_const_guard_type():];
        ...

`get_const_guard_type()`'s body may name `mutex` even though the alias now follows it — a member function body is a complete-class context. Verified to compile.

In synchronizable.h, wrap the concept:

    namespace detail {
    template <class F>
    concept function_type = std::is_function_v<F>;
    }

    template <detail::function_type F>
    struct is_synchronizable<F> : std::true_type {};

Both edits together leave all 12 files in tests/ compiling unchanged.
```

### Reproduction

```text
// probe_api_sv_public.cpp — compiles, i.e. the helpers are public API today
#include <threadsafe/threadsafe.h>
#include <vector>
using SV = threadsafe::synchronized_value<std::vector<int>>;
static_assert(SV::get_mutex_type() == ^^std::shared_mutex);
static_assert(std::meta::info(SV::get_const_guard_type()) != std::meta::info{});

$ g++-16 -std=c++26 -freflection -fsyntax-only -Iinclude probe_api_sv_public.cpp
$ echo $?
0
```

### Vérification

I tried three refutation angles and all failed.

(1) "The helpers aren't really callable." False. The original repro compiles verbatim against the unmodified headers (exit 0): `SV::get_mutex_type()` and `SV::get_const_guard_type()` are reachable from user code on `threadsafe::synchronized_value<std::vector<int>>`. They sit under `public:` at synchronized_value.h:52-69, above the `mutex`/`guard`/`const_guard` aliases.

(2) "They serve a purpose beyond the aliases, or must be public for access reasons." False on both counts. `grep -rn get_mutex_type\|get_const_guard_type include tests` returns exactly four hits: the two definitions and the two `[: :]` splices that consume them. Nothing else in the library or the test suite touches them, and the public `SV::mutex` alias already answers the only question `get_mutex_type()` could answer for a user (`static_assert(std::is_same_v<SV::mutex, std::shared_mutex>)` compiles). Access is not a barrier either: I applied the fix to a copy of the header tree and all 12 test files still compile with `-fsyntax-only`, including the case where `get_const_guard_type()`'s body names the `mutex` alias that is now declared *after* it — member function bodies are complete-class contexts, so the reordering is legal.

(3) "`function_type` is used more than once / is needed publicly." False. `grep` shows `threadsafe::function_type` is declared at synchronizable.h:14 and used at synchronizable.h:16 and nowhere else — the `is_function_type` hit in lifetime_aware.h:137 is `std::meta::is_function_type`, unrelated. Moving it into `namespace detail` and spelling the specialization `template <detail::function_type F>` compiles and leaves the whole test suite green, matching the `detail::std_wrapper` precedent the finding cites.

The one thing I would soften: "collides conceptually with anything a user might name the same" overstates it — the concept is namespace-qualified, so there is no actual collision absent a `using namespace threadsafe;`. The real point is surface-area, not collision, and I folded that into the corrected title. Also, the proposed fix as literally worded ("move both helpers above the `public:` label") must not drag `using mutex` down with them — that alias is genuine public API and has to stay public; my corrected fix states this.

Severity "nit" is right: this is pure interface hygiene on an educational codebase where the class shown on a conference slide should read as `mutex / guard / const_guard / ctor / make / lock / lock_shared` and nothing else.

---

# Annexe — les constats écartés

51 constats ont été levés puis réfutés par le vérificateur. Ils sont listés ici parce qu'un constat écarté est une propriété vérifiée : chacune de ces lignes est un endroit où la bibliothèque a été attaquée et a tenu.

- **std::mutex, std::shared_mutex, std::recursive_mutex and std::atomic_flag have no is_synchronizable answer, so the two canonical C++ thread-safety idioms are rejected out of the box** — `include/threadsafe/details/synchronizable.h`  
  The baseline facts are true (std::mutex, std::atomic_flag et al. answer false today), but the finding's consequence and its fix do not survive. 1. The proposed fix does not fix its own repro. I injected the patch verbatim into a TU and recompiled the finding's three static_asserts: `is_sendable_v<Spinlock*>` is STILL false. Reason: the unqualified `is_synchronizable<T>` is opt-in by construction — `synchronizable_base.h:12` is `std::false_type` and there is no structural walk for the non-const question (only `is_synchronizable<const T>` has one). Blessing `std::atomic_flag` therefore does nothing for a user type containing one. Proof that this is about the opt-in design and not about a […]
- **The walk checks `mutable` members but not static data members, so the same memoizing cache is rejected when spelled `mutable` and accepted when spelled `static inline`** — `include/threadsafe/details/synchronizable.h`  
  The repro is accurate and reproduces verbatim, but it demonstrates the model's boundary, not a soundness hole. (1) A static data member is not a subobject. I proved sizeof(CacheAsStatic) == sizeof(int): memo_ is not in the object. is_synchronizable<const T> / is_sendable<T> answer "may this OBJECT be shared / sent". The race in CacheAsStatic exists with zero sharing and zero sending -- two threads each holding a private, never-shared CacheAsStatic race identically on memo_. A trait about object shareability cannot be "wrong" about a hazard that is not a function of object shareability. The mutable spelling is different in kind: there the datum IS a subobject, so the race exists if and only […]
- **is_synchronizable has no cv-forwarding rule, so `volatile std::atomic<int>` is false while `const volatile std::atomic<int>` is true and `is_sendable<volatile std::atomic<int>>` is true** — `include/threadsafe/details/synchronizable.h`  
  The observed behaviour is real, but it is not the defect claimed, and the proposed fix is a hard compile error. 1. The axis is wrong: this is not a soundness hole. Every TRUE the finding lists is a *correct* TRUE — `const volatile std::atomic<int>` is genuinely readable from several threads, and `volatile std::atomic<int>` is genuinely sendable. The only anomalous answer is `is_synchronizable_v<volatile std::atomic<int>> == false`, i.e. a FALSE for a safe type. By the audit's own definition that is a usability (false-negative) hole, and a conservative one: it can only make the library reject code, never bless a race. 2. Impact is nil, not merely "low". The finding concedes indirections […]
- **Every capturing lambda is rejected, and the diagnostic's advice -- "specialize is_sendable to state the intent" -- is impossible to follow for a block-scope closure type** — `include/threadsafe/details/synchronizable.h`  
  The finding has two parts. The first — capturing closures are all rejected because GCC 16 reflects zero data members for them — I reproduced and it is true, but the auditor explicitly concedes it is toolchain-forced and "not a defect". So the finding rests entirely on its second claim: that the diagnostic's advice ("specialize is_sendable / is_synchronizable to state the intent") is *impossible to follow* for a closure declared inside a function, and that "only a namespace-scope lambda can be blessed (verified)". That load-bearing claim is false, and a compiling counterexample refutes it. A closure type declared at block scope is unnameable *by name* outside its function, but it is […]
- **`is_synchronizable<const std::unique_ptr<const T>>` trusts a const it does not own — the exact leak of "const behind an indirection is never trusted"** — `include/threadsafe/details/smart_pointers.h`  
  Every mechanical claim in the report reproduces exactly — I compiled it. `is_synchronizable<const std::unique_ptr<T,D>>` (smart_pointers.h:47-52) does keep the pointee's cv, `is_synchronizable_v<const Aliased>` is true while `is_synchronizable_v<const Honest>` is false, `copy_on_write<Aliased>` is sendable and `synchronized_value<Aliased>::mutex` is `std::shared_mutex`. But the interpretation — "soundness hole", high severity — does not survive. 1. The counterexample requires violating unique_ptr's contract, and the identical violation defeats rules the finding accepts. The repro's race is only reachable because the programmer hands `new std::string` to a `unique_ptr` and keeps writing […]
- **The const walk's pointer and reference branches skip the `dynamic_type_is_known` guard the unique_ptr rule enforces** — `include/threadsafe/details/synchronizable.h`  
  The repro compiles exactly as claimed — I re-ran it — but what it demonstrates is not a defect of the const walk, and the proposed fix is both wrong-headed and incomplete. 1. The guard's own stated rationale does not apply here. utils.h:74-77 scopes it precisely: "A **structural** trait walks the members of the *static* type … a **structural answer** about a polymorphic non-final pointee proves nothing." The guard protects structurally-derived answers. Where unique_ptr applies it, a structural answer is genuinely at stake: `is_sendable<unique_ptr<T,D>>` asks `is_sendable_v<T>` (structural walk of T's members), and `is_synchronizable<const unique_ptr<T,D>>` asks […]
- **The walk trusts every scalar member, so indirection encoded as an index escapes the pointer rule** — `include/threadsafe/details/synchronizable.h`  
  The observation compiles, but as a finding against this library it does not survive on three counts. 1. Wrong mechanism, wrong location. The report anchors at synchronizable.h:138 (`if (is_scalar_type(type)) return;`) and claims a scalar member is what lets the indirection through. I removed the scalar member entirely: `struct EmptyButWritesGlobal { void write(int v) const { shared_storage[0] = v; } };` has no non-static data members at all, never reaches line 138 for any member, and is still `is_synchronizable_v<const ...> == true` — because it is empty and there is nothing unsafe in it. The `int index` is a red herring; the hole is "a const member function may name a global", not "a […]
- **std::shared_ptr is blessed as owning unconditionally, but the aliasing constructor and a no-op deleter make it point at a stack object** — `include/threadsafe/details/lifetime_aware.h`  
  The repro compiles exactly as claimed, but the finding does not survive on three counts. (a) The title's "unconditionally" is factually wrong. lifetime_aware.h:53 imposes two conditions — the pointee must itself be lifetime aware and its dynamic type must be knowable — and both are exercised by the existing suite (test_soundness_regressions.cpp:142 rejects shared_ptr<PolyBase>, :153 rejects shared_ptr<Implementation>; test_lifetime_aware.cpp:59/64/67 reject shared_ptr<span<int>>, shared_ptr<int*>, shared_ptr<void>). (b) The load-bearing contrast is false. The finding's entire argument for this being a shared_ptr-specific gap is "unlike std::unique_ptr<T,D>, whose deleter smart_pointers.h:25 […]
- **is_lifetime_aware<std::weak_ptr<T>> is TRUE although a weak_ptr keeps neither its data nor its referent alive** — `include/threadsafe/details/lifetime_aware.h`  
  The behavioural half of the report reproduces exactly (I re-ran it: weak_ptr<int> TRUE, weak_ptr<Base> false, weak_ptr<span<int>> false, weak_ptr<Final> TRUE), and the reporter concedes there is no soundness consequence. What does not survive is the claim itself, on the correctness lens. 1. The premise is factually wrong on its own terms. CLAUDE.md's contract is a disjunction: "owns its data **or** keeps its referent alive". The finding collapses the two clauses and asserts weak_ptr does neither. But weak_ptr does own its data: its data is the weak reference to the control block, which it keeps alive (the weak count) and manages on copy/destroy. Only the *second* disjunct — keeps its […]
- **is_sendable<T*>/<T&>/<T&&> skip the dynamic-type guard the library applies to unique_ptr, so a vouched polymorphic base launders any derived class across threads** — `include/threadsafe/details/sendable.h`  
  The mechanical observation is true but the diagnosis is wrong: the guard's absence at sendable.h:27-33 is a documented, deliberate design line, and the "hole" is not reachable without the user's own UNSAFE vouch. 1. The guard answers a question that only structural traits raise. `detail::compute_dynamic_type_is_known` (utils.h:76-96) is introduced under the comment "A structural trait walks the members of the *static* type... a structural answer about a polymorphic non-final pointee proves nothing about the object actually there." Every guarded site reads a *structural* answer the library derived itself: `is_sendable_v<T>` (smart_pointers.h:19), `is_lifetime_aware_v<T>` […]
- **The move-only RAII owner — the most send-worthy shape there is — is never sendable, while a leaky POD handle is sendable for free** — `include/threadsafe/details/sendable.h`  
  The repro reproduces exactly (MO1 fails, MO2 passes), but the finding does not survive as a defect. Three independent reasons: 1. Its central technical premise is false. The explanation rests on "a move constructor cannot introduce sharing — it transfers it". I compiled a counterexample where it does: a move constructor whose only member is an `int` but which pushes `this` into a global `std::vector<void*>`. All members are sendable and reflection sees nothing wrong; the object becomes aliased from shared global state the moment it is moved. The library correctly answers false today. Under proposed fix (a) it would answer true — turning a claimed medium usability nit into an actual […]
- **The exclusivity invariant behind `use_count() != 1` is nowhere stated and is upheld only by an accidental answer from the generic const walk** — `include/threadsafe/details/copy_on_write.h`  
  The finding's diagnostic repro is real, but its causal chain is not. Four independent facts, each compiled, break it. 1. `is_synchronizable<const copy_on_write<T>>` is not the guard on `use_count() != 1`. `as_mutable()` (copy_on_write.h:29) is a **non-const** member function. Const sharing hands a reader only `operator*`/`operator->`, which read `*ptr_` and never touch `ptr_` or the control block. `mutable_through_const<cow<int>>` is false. So blessing the const form cannot "silently invalidate the exclusivity test" — there is no path from a const share to `as_mutable`. 2. The const trait never gates cross-thread reference sharing anyway. `is_sendable<T&> : […]
- **Synchronizability stops at the wrapper: a struct holding a `synchronized_value<T>` is not synchronizable, so the shared handle to it is not sendable** — `include/threadsafe/details/synchronized_value.h`  
  The mechanical repro reproduces exactly as claimed, but the finding does not survive the design-intent lens, and its two load-bearing claims are both false. 1. The behavior is the trait model's stated axiom, not an oversight. `synchronizable_base.h:12` is `template <class T> struct is_synchronizable : std::false_type {};` — opt-in by construction. `synchronizable.h:55-56` says so in a comment ("the full trait is opt-in, so a non-const T has nothing to explain beyond that"), the diagnostic at :59-63 enumerates the recourses, and `tests/test_synchronizable.cpp:64` pins it as a test: `static_assert(!is_synchronizable_v<Plain>, "is_synchronizable — default is false for class types")`. A trait […]
- **`value_guard` is silently non-movable: declaring the copy constructor deleted suppresses the implicit move constructor** — `include/threadsafe/details/synchronized_value.h`  
  The C++ mechanism the finding describes is technically true and I reproduced both error messages verbatim. `value_guard` (synchronized_value.h:22-23) user-declares the copy constructor and copy assignment as deleted, which suppresses the implicit move constructor; `std::is_move_constructible_v<SV::guard>` is false, `return g;` from a named local fails, and `std::optional<guard>::emplace(a.lock())` fails. But the finding's actual claim is not the mechanism — it is (a) that this is a usability hole and (b) that "nothing says whether this is deliberate ... the reader cannot tell the decision from the accident." Both are refuted by the test suite, which in this library *is* the specification […]
- **`lock_shared()` is not always shared: the mutex is chosen invisibly from `is_synchronizable_v<const T>`, so the same call is UB for one T and a deadlock for another** — `include/threadsafe/details/synchronized_value.h`  
  The finding does not survive. Four independent refutations, three of them mechanically verified. **1. The central factual claim is false: the mutex choice is not invisible.** The finding says the mutex is "decided by a trait the caller never sees" and picked "behind the user's back." But `synchronized_value<T>::mutex` and `::const_guard` are *public member typedefs*, and the deciding trait `is_synchronizable_v<const T>` is public library API documented in CLAUDE.md under "`is_synchronizable<const T>` — thread-safe read". The finding's own repro proves this: a `static_assert` can only observe public API, and its repro static_asserts on `synchronized_value<MutableCache>::mutex` directly. A […]
- **launch_scoped_task join()s without request_stop(), so a cooperative stop_token task deadlocks the caller forever** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The factual core of the claim is true — `launch_scoped_task` with a `while (!token.stop_requested())` task never returns; I reproduced it (exit=137 under a 4s watchdog). But the finding does not survive as a defect, for three reasons, each compiled and measured. (1) There is nothing stop_token-specific about the hang. `launch_scoped_task`'s entire contract is "run f to completion, then return" — that is exactly what bounds the borrow described in the PRECONDITION comment at lines 101-104 (it is the overload that drops `lifetime_aware` and lets a `reference_wrapper<SyncCounter>` cross, per tests/test_asynchronous_task_launcher.cpp:46). Any task that never returns on its own hangs it: `[]{ […]
- **launch_scoped_task is fully serialized: it starts a thread and immediately joins it, so N scoped tasks never overlap** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The repro is mechanically accurate — I re-ran it and got 440ms for four 100ms scoped tasks — but the finding does not survive as a defect, on three counts. 1. The serialization is not an oversight; it is the load-bearing invariant that licenses the relaxed concept. `launchable_scoped_task` (asynchronous_task_launcher.h:30-32) is exactly `launchable_task` minus every `lifetime_aware` requirement. The only thing paying for that relaxation is the in-call join. The header says so at lines 101-104 ("the join bounds the invocation"), and tests/test_asynchronous_task_launcher.cpp asserts it in prose twice: "the launcher waits for the task, so a reference to a synchronizable object may cross". […]
- **launchable_task / launchable_scoped_task accept reference Args that the launcher can never take, turning a satisfied concept into a libstdc++ hard error** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The repro does compile and does show exactly what it claims — I reran it verbatim (probe_refute_launcher_refargs.cpp) and got the same libstdc++ static_assert at thread:274 after the concept was satisfied. But the finding does not survive on three counts: its axis, its causal attribution, and its fix. 1) Wrong axis. Nothing unsafe is blessed. `is_sendable<Blessed&> = is_synchronizable<Blessed>` is answered on a type the user explicitly vouched for with THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE, and the program is then *rejected at compile time*. No race crosses; no reference is ever bound across the thread boundary (std::jthread always decay-copies). At worst this is diagnostic quality — a […]
- **launch_scoped_task's safety argument is a comment: a scoped task that detaches produces a verified stack-use-after-scope** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The ASan crash reproduces exactly as reported, but the finding does not survive as a defect — it reports the API's defining feature, at the line that documents it, using the documentation's own words. 1. Dropping `lifetime_aware` IS `launch_scoped_task`. Compare the two concepts (asynchronous_task_launcher.h:22-32): `launchable_task` and `launchable_scoped_task` are identical except that the scoped one omits `lifetime_aware<F>` and `(lifetime_aware<Args> && ...)`. That omission is the entire difference between the two APIs. "Requiring lifetime_aware here" is not a fix, it is deleting the feature and leaving two spellings of `launch_task`. This mirrors `std::thread` scope facilities and […]
- **Ordinary safe value types are refused as task arguments: string literals, std::string_view, std::chrono::seconds, std::cref** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The reproduction is accurate — all four cases compile to the exact quoted diagnostics — but the finding built on top of it does not survive. (a) Wrong location/attribution. asynchronous_task_launcher.h:30 is `concept launchable_scoped_task`; none of the four behaviors originate there. Every one reproduces from a bare `static_assert(is_sendable_v<T>)` with no launcher present. The launcher only reports trait answers. The quoted CURRENT CODE actually lives at smart_pointers.h:37. (b) Two of the four rejections are load-bearing soundness, not usability. `const char*` is refused by the rule CLAUDE.md states explicitly (is_sendable<T*> = is_synchronizable<T>) — refusing raw pointers is the […]
- **launchable_task does not require invocability, so an arity or parameter mismatch escapes into 49 lines of libstdc++ error** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The mechanics of the repro are accurate — I reproduced 49 lines with no threadsafe-authored message — but the finding does not survive as a defect in *this* library, for four reasons. (1) The stated mechanism is wrong. The auditor writes that explain_launch_task's fallback throw, "launch_task rejects this call but every trait holds", "is exactly the case that occurs, but it never fires". It is not that case. launch_task does not reject the call at all: it accepts it, and it is right to — `[](int,int){}` with an `int` is perfectly sendable and lifetime-aware, so the safety question the library exists to answer is genuinely "yes". The comment above that throw says it guards against the […]
- **No result path and no failure path: launch_task returns void, an escaping exception calls std::terminate, and threads_ is never reaped** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The finding bundles three claims, and none survives as an actionable defect. (1) "An escaping exception calls std::terminate." True and reproduced (exit=134), but this is verbatim std::thread/std::jthread semantics per [thread.thread.class] — an exception escaping the thread's initial function calls std::terminate. launch_task introduces nothing; any fire-and-forget wrapper over jthread behaves identically. No fix is proposed for it. Note the claimed repro as written omits <cstdio> for std::printf and would not compile; I had to add it. (2) "launch_task returns void, no future/handle." This is an absent feature, not a bug, and the finding itself concedes it is "the good news for the […]
- **The comment on is_synchronizable<const std::unique_ptr> states the opposite of what the code does** — `include/threadsafe/details/smart_pointers.h`  
  The comment says "the one indirection that **trusts the pointee's const**". The prior auditor read that as "the const on the `unique_ptr` object is pushed onto the pointee", then showed `const unique_ptr<Plain>` is false and declared the comment inverted. That is a misreading of the possessive: "the pointee's const" is the const the *pointee type already carries*, i.e. the `const` the user wrote inside the angle brackets. Read that way the comment is exactly, and uniquely, true of this code. Every other indirection rule in `smart_pointers.h` — lines 30, 34, 38, 56, 60, 64 — wraps the pointee in `std::remove_cv_t<...>`, deliberately throwing the pointee's const away and asking the full […]
- **"assert" means two opposite things in the same library, and the macro hides in synchronizable_base.h** — `include/threadsafe/details/synchronizable_base.h`  
  The finding's raw greps reproduce, but each of its three load-bearing claims collapses under examination, and its proposed fix is a net regression on the very axis it argues from. 1. "Same verb, opposite direction, both in the reader's field of view." The co-occurrence claim is false in practice. I checked every file that mentions the macro: in all five test files the macro appears and `assert_synchronizable<...>` appears zero times; `assert_synchronizable` is called only in tests/test_diagnostics.cpp, which never mentions the macro. The single file where both tokens appear is synchronizable.h itself — and there the macro name occurs *only* inside the failure diagnostic at line 62, which […]
- **value_guard carries a three-line comment attached to nothing, describing a design that was not chosen** — `include/threadsafe/details/synchronized_value.h`  
  The finding rests on three factual claims. All three are false, and the proposed fix would delete information that exists nowhere else. (1) "Attached to nothing / refers to no declaration." The block sits in `value_guard`'s public section immediately above the only two members that hand out a reference to the guarded value. Its subject is those operators: "the lock is released when the guard is destroyed" is precisely the property that makes handing out `T&`/`T*` dangerous. A blank line after a comment is a spacing choice, not evidence of an orphan. "Names no capture" reads the word as C++ lambda capture; it plainly means "do not keep hold of the reference you got out of the guard" — […]
- **`// Mostly for closure type.` is the one place the reflection genuinely needs an explanation** — `include/threadsafe/details/utils.h`  
  The finding's load-bearing claim is "the reasoning — a lambda's captures are not reflectable data members, so a non-empty class with no bases and no members must be hiding something — is nowhere written down." That is factually false, and the compiler prints the counter-evidence. 1. The reasoning is written down at every call site, in the user-facing diagnostic. `has_unreflectable_state` has exactly three callers (sendable.h:154, synchronizable.h:165, lifetime_aware.h:170) and each one immediately follows it with `reject(type, u8"holds state reflection cannot see (a closure type with captures); specialize is_<trait> to state the intent", path)`. That string is the proposed fix's content — […]
- **copy_on_write::as_mutable() hands out an unbounded-lifetime T& into the shared block; a later copy re-shares that exact object, producing a data race with every trait satisfied** — `include/threadsafe/details/copy_on_write.h`  
  The mechanical core of the claim is true and I reproduced it: `as_mutable()` (copy_on_write.h:29) returns a `T&` with no lifetime bound, a later copy of the handle re-shares that exact object, and the repro compiles clean with `is_sendable_v<copy_on_write<Payload>>`, `is_sendable_v<Reader>` and `is_lifetime_aware_v<Reader>` all satisfied. But the finding *as characterized* — a critical soundness hole localized to line 29, remedied by the proposed `with_mutable` — does not survive on three counts. (1) It is not a soundness hole under this audit's own definition ("the library answers TRUE for a type that is NOT actually safe"). `copy_on_write<Payload>` genuinely IS safe to send: readers only […]
- **launch_scoped_task calls task.join() instead of letting ~jthread request stop, so any stop_token-aware callable — the exact shape the class's own static_assert blesses — hangs forever** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The repro is honest about its mechanics but wrong about what it proves, and the proposed fix is worse than the thing it fixes. 1. The hang reproduces — but it is not stop_token-specific, so the diagnosis is misattributed. `launch_scoped_task` is a *synchronous run-to-completion* API: it constructs a jthread and joins it in the same statement. Any callable that never returns on its own blocks the caller. I compiled `struct SpinForever { void operator()() const { for(;;){} } };` — no stop_token anywhere — and got exactly the same "before" / never-"after" hang. The finding therefore reduces to "a blocking join blocks on a non-terminating callable," which is the documented behaviour, not a […]
- **value_guard::operator*() const& returns a raw T& that silently outlives the lock; the deleted && overloads catch only the one-liner form** — `include/threadsafe/details/synchronized_value.h`  
  The literal mechanical claim is true — I reproduced it (`escaped = &*guard;` compiles clean, and the pointer outlives the lock). But the finding as a defect-with-a-fix does not survive, on four independent grounds, each of which I verified with the compiler. 1. The proposed fix does not compile. It contains `typename const_guard_lock_type exclusive_or_shared{mutex_};`. `const_guard_lock_type` does not exist anywhere in the repository (`grep -rn "const_guard_lock_type" .` returns nothing; the class exposes only `mutex`, `guard`, `const_guard`), and `typename X x{...}` is not valid syntax regardless. GCC 16: "expected nested-name-specifier before 'const_guard_lock_type'". 2. The fix, even […]
- **launch_scoped_task joins inline, so the scoped facility delivers zero parallelism — N scoped tasks always run strictly one after another** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The observation is factually correct but is a deliberate, documented trade-off, and the proposed fix converts it into the exact class of bug the library exists to prevent. 1. The inline join is the load-bearing safety mechanism, not an oversight. Compare the two concepts in the same file: `launchable_task` (lines 22-27) requires `lifetime_aware<F>` and `(lifetime_aware<Args> && ...)`; `launchable_scoped_task` (lines 29-32) deliberately drops both. Those two clauses are precisely what would otherwise reject `std::ref(stack_local)` and a raw `T&`. Nothing else in the library replaces them — the only thing standing between that relaxation and a dangling borrow is `task.join()` on line 109. The […]
- **An exception escaping a task aborts the whole process, and the launcher offers no way to observe it** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The repro is factually accurate — I rebuilt and re-ran it and got the identical `terminate called ... boom`, exit=134, with neither "caught:" nor "survived" printed. But the observation is not a defect of ThreadSafe, and the proposed fix is a regression. 1. The library introduces nothing here. I ran the same throwing callable through a bare `std::jthread` with ThreadSafe not included at all (probe_baseline_jthread_exc.cpp): identical output, exit=134. This is standard-mandated behaviour ([thread.jthread.cons]: an uncaught exception escaping the thread function calls `std::terminate`). `asynchronous_task_launcher` is a ~5-line wrapper over `std::vector<std::jthread>`; it inherits the […]
- **Taking a second guard on the same synchronized_value from one thread crashes or deadlocks, with nothing in the API or the diagnostics warning about it** — `include/threadsafe/details/synchronized_value.h`  
  The runtime behaviour is real — I rebuilt and ran the repro and it aborts with "Resource deadlock avoided" — but the *finding* does not survive as a defect in this library. 1. It is not a property of ThreadSafe; it is the defined semantics of the standard types the class is built from. `synchronized_value` is a transparent two-member wrapper (`mutable mutex mutex_; T value_;`) and the mutex type is *public* API: `using mutex = [:get_mutex_type():];` resolving to `std::mutex` or `std::shared_mutex`. Non-reentrancy is deducible from what the header already exposes. The identical program deadlocks with `std::lock_guard`, `std::scoped_lock`, P0290's proposed `std::synchronized_value`, […]
- **The traits stop compiling at 126 levels of nesting and emit 10.6 MB / 102,332 lines of diagnostics — ~4.1 constexpr frames are burned per nesting level** — `include/threadsafe/details/sendable.h`  
  The reproduction is flawless — I re-ran every measurement and all of them match exactly (cap 125/126 on all three traits, 102,332 lines, 10,575,760 bytes, 127 "is not a member" errors, -fconstexpr-depth=1024 fixes it, inlining trait_value yields exactly 167 with all 11 tests passing). The location is correct: is_sendable_type at sendable.h:44 really is on the recursion path via the nonstatic_data_members_of loop. So the finding is factually accurate in every particular. It nevertheless does not survive as a defect, for three reasons I established by measurement rather than argument: (1) The threshold is unreachable by real code. I built a deliberately monstrous but realistic type — […]
- **The umbrella header forces <thread>, <mutex> and <shared_mutex> on every translation unit — the three helper class templates cost 25% of every include, even for TUs that only want the traits** — `include/threadsafe/threadsafe.h`  
  The raw numbers reproduce (140,249 lines / 0.60 s umbrella vs 127,581 / 0.45 s traits-only), but three independent checks kill the finding. 1) The attribution in the title is wrong. I compiled a TU containing ONLY the libstdc++ headers the library pulls, with zero lines of ThreadSafe: 126,605 lines / 0.46 s — versus the six trait headers' 127,581 / 0.45 s. Add `<thread> <mutex> <shared_mutex>` to that same std-only TU and it becomes 138,975 / 0.63 s, versus the umbrella's 140,249 / 0.60 s. So ThreadSafe's own ~1330 lines contribute under measurement noise, and the entire claimed 0.16 s is `<thread>` + `<mutex>` + `<shared_mutex>` themselves. "The three helper class templates cost 25% of […]
- **`is_sendable`'s scalar short-circuit is second in the `||`, so every scalar type instantiates `is_synchronizable_v` first — swapping the operands makes scalar answers free** — `include/threadsafe/details/sendable.h`  
  Three independent checks, all against the real files and re-run measurements: (1) The headline repro number does not reproduce. Interleaved, 9 reps, min-of-runs, GCC 16.2.0, -fsyntax-only: control (600 enum decls only) orig 0.690 s swap 0.688 s 600 x is_sendable_v<E_i> orig 0.762 s swap 0.722 s net orig 0.073 s swap 0.035 s The "as written" figure matches the report (0.07 s), but the swapped column is 0.035 s, not 0.00 s. The swap removes roughly half the per-type cost, not all of it — the remaining half is instantiating is_sendable<E>/is_sendable_v<E> and running the consteval walk itself, which the swap cannot avoid. So the title's claim ("makes scalar answers free") and the repro table's […]
- **lifetime_aware.h includes <ranges> for the single concept `std::ranges::borrowed_range`; <iterator> supplies it for 10,555 fewer preprocessed lines** — `include/threadsafe/details/lifetime_aware.h`  
  The measurements reproduce exactly, but the "defect" characterization does not survive. WHAT I CONFIRMED (the numbers are honest): - lifetime_aware.h:7 really is `#include <ranges>`, and line 156 (`trait_value(^^std::ranges::borrowed_range, type)`) really is its only use. Nothing else in include/ uses `<ranges>`; `allowed_std_wrappers.h:52` uses `std::ranges::contains` but includes `<algorithm>` itself, so there is no hidden second consumer. - Swapping to `<iterator>`: umbrella 140,249 -> 129,694 preprocessed lines, byte-identical to the claim. All 11 test TUs compile clean. - Wall clock is actually *slightly better* than the auditor claimed, not worse: min-of-5 per heavy TU 0.75 -> 0.72 […]
- **launch_scoped_task spawns a thread and immediately joins it, so it buys zero parallelism at 12.7 us per task** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The mechanical observation is accurate — `launch_scoped_task` spawns and joins in two lines, so one call never overlaps another, and I reproduced 12915.8 ns/task against 2.0 ns for a direct call. But it is the deliberate, in-code-documented contract, not a defect, and the proposed remedy is strictly worse on both axes it claims to serve. 1. The join is load-bearing and documented. `asynchronous_task_launcher.h:101-104` says "the join bounds the invocation, not the borrow", and the split between the two concepts exists only because of it: `launchable_task` requires `sendable && lifetime_aware`; `launchable_scoped_task` requires only `sendable`. Two test files assert the join as the reason […]
- **value_guard is built on std::unique_lock, whose owns-lock flag can never be false — 24 bytes and a dead destructor branch instead of 16** — `include/threadsafe/details/synchronized_value.h`  
  The finding's *static* facts check out, but the claim it is filed under — runtime-perf — is disproven by measurement, and the "drops straight in" claim is false. What survives (all verified by compilation): - `sizeof(synchronized_value<int>::guard) == 24`; a `lock_guard`-based equivalent is 16. - `value_guard` is neither copyable nor movable and exposes no `unlock()`/`release()`, so `unique_lock`'s owns-lock flag is invariantly true. - `std::lock_guard` does supply `mutex_type` and the `mutex&` ctor, so it substitutes syntactically. What refutes it: 1. **The stated runtime cost is exactly zero, not "paid always."** The finding concedes the branch folds when `lock()` inlines but insists "the […]
- **copy_on_write::as_mutable emits an acquire barrier on the unique fast path guarding writes no thread can have made** — `include/threadsafe/details/copy_on_write.h`  
  Two separate questions: does the repro show what it says (mostly yes), and does the finding survive (no). REPRO — reproduces, magnitude overstated. `g++-16 -std=c++26 -freflection -O2 -S` on aarch64-apple-darwin25 does emit exactly one `dmb ishld` in `as_mutable`, on the unique fast path, right after `cmp w0,1 / bne`. That half is accurate. The timing half is not: I measured 0.55 ns/call with the fence vs 0.42-0.43 without, stable across runs and across a warm second pass — ~28%, not the claimed ~60% (0.79 vs 0.49). In absolute terms the fence costs ~0.12 ns/call, roughly half a cycle, on an operation whose entire body is a load, a compare and a branch. FINDING — the barrier is NOT guarding […]
- **copy_on_write's accessors are not [[nodiscard]]; a discarded as_mutable() silently pays for a deep copy** — `include/threadsafe/details/copy_on_write.h`  
  The mechanical observation is accurate — `copy_on_write` has no `[[nodiscard]]` anywhere and the repro compiles clean under `-Wall -Wextra`. But the load-bearing rationale of the finding is factually wrong, and once it is removed nothing of severity remains. 1. "the whole cost of the operation with none of its effect" is false. `as_mutable()` is not an accessor; it is a mutator that returns a reference. Discarding the reference does not discard the work: `ptr_ = std::make_shared<T>(*ptr_)` permanently rebinds the handle. I measured it (probe_perf_nodiscard_cow_effect.cpp): before the discarded call `use_count == 2` and the block is shared with the copy `b`; after it, `use_count == 1` and […]
- **Every capturing lambda is rejected, and the message names a remedy that cannot be written** — `include/threadsafe/details/utils.h`  
  The mechanical half of the finding reproduces exactly, but the finding as framed — a *critical usability* defect whose message "names a remedy that cannot be written" — does not survive. 1. The rejection is confirmed and is deliberate, tested design, not an oversight. `tests/test_asynchronous_task_launcher.cpp:56`, `tests/test_sendable.cpp:249`, `tests/test_synchronizable.cpp:135` and `tests/test_soundness_regressions.cpp:179` all assert that a capturing closure is rejected, with comments stating the reason ("a closure reflects no members whatever it captures"). The finding itself concedes the rejection is sound and proposes keeping it — so nothing about the library's *answers* is claimed […]
- **details/ headers are includable on their own and silently give different trait answers — an ODR trap** — `include/threadsafe/details/sendable.h`  
  The repro is factually accurate — I reproduced it — but it does not identify a defect. Three independent reasons it fails. 1. The hazard is the architecture, not the directory layout. I compiled `probe_api_detailsguard_documented_path.cpp`, which includes only the umbrella and asserts `!is_sendable_v<Holder>` (exit 0). The shipped test `tests/test_deferred_specialization.cpp` includes only the umbrella and asserts the opposite, `is_sendable_v<Holder>` (exit 0), differing solely by one `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Opaque);` line. Same types, same program, two values for `is_sendable<Holder>::value`, zero diagnostics — the identical silent IFNDR the finding describes, reached […]
- **The one escape hatch is the wrong one: vouching for "sendable" forces the strictly stronger "synchronizable" claim** — `include/threadsafe/details/synchronizable_base.h`  
  The repro compiles, but every load-bearing premise of the finding is false when checked against the real code. 1. "The traits fail far more often on is_sendable and is_lifetime_aware than on is_synchronizable" — inverted. `is_synchronizable` is the *opt-in* trait: its primary template is `std::false_type`, so it answers false for literally every type, including `int` and a POD struct (probe E compiles `static_assert(!threadsafe::is_synchronizable_v<int>)`). `is_sendable` and `is_lifetime_aware` have structural primary templates (`std::bool_constant<detail::default_is_sendable(^^T)>`) and answer *true* by default. The library ships exactly one macro for exactly the one trait that has no […]
- **The unconstrained launch_task fallback makes the API report "yes, launchable" for unsafe callables in any detection idiom** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The narrow observation reproduces: the unconstrained fallback at asynchronous_task_launcher.h:96-99 makes `requires { l.launch_task(f); }` true for a non-sendable callable. But the finding does not survive as a defect, for three independently sufficient reasons. (1) The proposed fix is broken and fails its own stated goal. `(detail::explain_launch_task<F, Args...>(), false)` in a requires-clause makes the atomic constraint evaluate a throwing consteval call. Constraint satisfaction is not the immediate context, so this is a hard error rather than an unsatisfied constraint — and it fires *inside* the very requires-expression the fix was written to make false. I compiled it: […]
- **launch_scoped_task runs nothing concurrently, and the borrowing it exists to permit cannot be expressed with a lambda** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The load-bearing half of the finding is factually wrong, and the other half is the deliberate mechanism that makes the relaxed constraint sound. 1. "The relaxed constraint is unreachable through the natural spelling: the user must hand-write a functor struct holding a reference." — FALSE. In this library the borrow channel is the *argument list*, not the capture list. `launch_scoped_task(F f, Args... args)` forwards `Args...` to the jthread, and `is_sendable<T&> = is_synchronizable<T>` (CLAUDE.md) is exactly the rule that lets a `std::reference_wrapper<T>` cross when `T` is synchronizable. The idiomatic spelling is a captureless lambda taking `T&` plus `std::ref(object)`. I compiled and ran […]
- **asynchronous_task_launcher offers no way to wait: the thread vector only grows and is joined by the destructor** — `include/threadsafe/details/asynchronous_task_launcher.h`  
  The structural observation is accurate — `launch_task` only `emplace_back`s (line 90) and `threads_` is never pruned, so the destructor is the sole join point. But every consequence the finding draws from that is either wrong or is `std::jthread`'s own documented RAII contract, and the proposed fix is demonstrably broken. 1. "A user who wants 'run these five, then continue' has no join_all() and must construct a launcher in a nested scope purely to get the destructor to fire." That nested scope IS the answer, it is three lines, and it works — I compiled and ran it: five parallel `launch_task` calls in a braced block, all five observed finished immediately after the block. Calling the […]
- **Static data members are invisible to all three walks, so a type whose entire mutable state is `inline static` is sendable, const-synchronizable and lifetime aware** — `include/threadsafe/details/utils.h`  
  The repro compiles (all three traits true for a type whose mutable state is `inline static`), but it does not isolate a defect in the walks. A static data member is not part of the object's value — it is a global with a scoped name. I compiled the byte-identical variants where the same `pool`/`used` state lives at namespace scope, and where it lives as a function-local `static` inside a `const` member function: both are accepted by all three traits, and no walk over members can ever see them. So the acceptance is not caused by `nonstatic_data_members_of`; it is caused by the traits being structural over the value, which is the library's (and Rust's) whole model. Rust's auto traits ignore […]
- **Opaque storage — a `std::byte`/`char` buffer or an integer handle — reads as sendable and lifetime aware, defeating the structural walk with no diagnostic** — `include/threadsafe/details/sendable.h`  
  The mechanical repro reproduces, but it does not describe a defect. Four things refute it. 1. It is not a deviation from the stated model — it *is* the model. CLAUDE.md says the traits are "inspired by Rust's Send/Sync." Rust gives the identical answer: `[u8; 24]`, `usize`, and `fn(*const u8)` are all auto-`Send` (and auto-`Sync`), so a Rust `struct S { f: fn(*const u8), storage: [u8; 24] }` is `Send` with no diagnostic either. Real Rust SBO/type-erasure types opt *out* by adding `PhantomData<*const ()>` — exactly the burden this library places on the author via `specialize is_sendable`. Matching the reference model's answer is not a hole in it. 2. The finding's severity claim is factually […]
- **is_synchronizable has no structural rule, so a struct whose every member is a synchronized_value or an atomic is not shareable at all — only the UNSAFE macro recovers it** — `include/threadsafe/details/synchronizable_base.h`  
  The repro's static_asserts all hold, so the *observation* is factually accurate: `is_synchronizable` has no structural default and stops at any un-specialized class type. But every step from that observation to "defect" fails. 1. It is an explicit, documented stance, not an oversight. `assert_synchronizable<T>()` emits, verbatim: "is_synchronizable<T> is opt-in: it holds only for types that synchronize themselves (std::atomic, a mutex-protected wrapper). Ask is_synchronizable<const T> for a read-only share, or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE to vouch for it." A user who hits this is routed, not stranded. tests/test_synchronizable.cpp:61-64 pins the default-false with the same […]
- **The structural walk never looks at static data members, so a class whose only shared mutable state is a `static inline` member — including a raw pointer the walk would reject as a nonstatic member — answers true for all three traits** — `include/threadsafe/details/utils.h`  
  The behavioral claims are all factually true and I reproduced them exactly: all three walks use `nonstatic_data_members_of`, `has_only_default_copy_move_destroy` iterates `members_of` but only tests copy/move/destroy members and templates, and `IdGenerator`/`PoolHandle` (static `std::string *`) answer true for `is_sendable`, `is_synchronizable<const T>`, `is_lifetime_aware`, and through `std::vector<IdGenerator>`. What does not survive is the conclusion that this is a soundness hole in the traits' contract, or that the proposed check is the right response. (1) A static data member is a global, not object state. Sending an `IdGenerator` moves exactly the bytes of `identifier`; the receiving […]
- **Every borrow rule strips cv before asking, so no read-only borrow is ever sendable — const T&, std::cref(x), std::string_view and std::span<const T> all answer false, which leaves launch_scoped_task, the one API built for borrows, with no standard borrow it can carry** — `include/threadsafe/details/sendable.h`  
  The mechanical facts in the finding all reproduce, but the conclusion drawn from them does not survive. WHAT IS TRUE (I compiled it): - `is_sendable<T&>`, `is_sendable<T*>` (sendable.h:27-32) and `is_sendable<std::reference_wrapper<T>>` (smart_pointers.h:37) all substitute `std::remove_cv_t<T>`, so a read-only borrow is asked the full write-safe question. - `is_synchronizable_v<const std::string>` is true while `is_sendable_v<const std::string&>`, `is_sendable_v<std::reference_wrapper<const std::string>>`, `is_sendable_v<std::string_view>`, `is_sendable_v<std::span<const int>>` and `is_sendable_v<const char*>` are all false. - `launcher.launch_scoped_task(f, std::cref(owned))` is rejected, […]
