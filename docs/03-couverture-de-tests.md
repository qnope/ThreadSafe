# Couverture de tests — test par mutation

La suite actuelle compte 11 unités de traduction et environ 1 470 lignes de
`static_assert`. Elle est riche, mais « beaucoup d'assertions » ne veut pas dire
« les règles sont protégées ». Pour le mesurer, chaque règle de la bibliothèque a
été **cassée une par une** dans une copie des en-têtes, puis la suite entière a été
recompilée : si aucun test n'échoue, la règle n'est protégée par rien.

Méthode : 19 mutants, chacun désactivant exactement une règle. Un mutant *tué*
signifie qu'au moins un test l'a détecté ; un mutant *survivant* est un trou de
couverture.

## Résultat : 5 survivants sur 19

| mutant | détecté ? | par |
|---|---|---|
| `sendable-ref-always-true` | tué | containers, sendable, smart_pointers |
| `sendable-ptr-always-true` | tué | 7 fichiers |
| `drop-copy-move-guard` | tué | 8 fichiers |
| `drop-hijack-guard` | tué | sendable |
| `drop-unreflectable-state` | tué | 4 fichiers |
| `drop-mutable-check` | tué | containers, copy_on_write, synchronizable, synchronized_value |
| `drop-ref-member-check` | tué | synchronizable |
| `drop-const-pointer-check` | tué | 4 fichiers |
| `drop-polymorphic-guard` | tué | soundness_regressions |
| `lifetime-ptr-always-true` | tué | 6 fichiers |
| `lifetime-refwrapper-true` | tué | 3 fichiers |
| `syncval-always-shared-mutex` | tué | synchronized_value |
| `launcher-drop-lifetime-req` | tué | 3 fichiers |
| `launcher-drop-sendable-req` | tué | asynchronous_task_launcher |
| **`drop-borrowed-range-rule`** | **SURVIT** | — |
| **`cow-never-detach`** | **SURVIT** | — |
| **`cow-always-detach`** | **SURVIT** | — |
| **`guard-allow-rvalue-star`** | **SURVIT** | — |
| **`syncval-drop-sendable-assert`** | **SURVIT** | — |

Le noyau des traits est donc très bien protégé : 14 mutants sur 19 sont tués, et
souvent par plusieurs fichiers à la fois. Les cinq survivants se regroupent en
trois problèmes distincts.

---

## Survivant 1 — la règle `borrowed_range` n'est testée par aucun type qui en a besoin

`tests/test_lifetime_aware.cpp` teste bien la règle :

```cpp
static_assert(!is_lifetime_aware_v<std::span<int>>);
static_assert(!is_lifetime_aware_v<std::string_view>);
static_assert(!is_lifetime_aware_v<std::ranges::subrange<int*>>);
```

Mais supprimer entièrement la règle ne casse aucun de ces trois tests. La raison :
`span`, `string_view` et `subrange` **stockent tous un pointeur brut**, donc la
marche structurelle sur les membres les rejette déjà toute seule. Ces trois tests
passent par un chemin qui n'est pas celui qu'ils prétendent tester.

La règle est pourtant bien utile — il suffit d'un `borrowed_range` **vide**, qui
n'offre aucun membre à inspecter :

```cpp
static_assert(std::ranges::borrowed_range<std::ranges::empty_view<int>>
                  && std::is_empty_v<std::ranges::empty_view<int>>);
static_assert(!is_lifetime_aware_v<std::ranges::empty_view<int>>);
```

Sans la règle, `empty_view` devient « lifetime aware », car la marche ne trouve
aucun membre auquel objecter.

## Survivants 2 et 3 — la sémantique d'exécution de `copy_on_write` n'est vérifiée par rien

`if (ptr_.use_count() != 1)` peut être remplacé par `if (false)` (ne jamais
détacher — faille de sûreté) **ou** par `if (true)` (toujours copier — perte de
performance) sans qu'aucun test ne bronche. `tests/test_copy_on_write.cpp` ne
contient que des assertions sur les traits et sur les types de retour ; le
comportement du type n'y figure pas.

Ce n'est pas un oubli, c'est **structurel**. `CLAUDE.md` pose que les tests sont
uniquement à la compilation, or la sémantique de `copy_on_write` n'est pas
atteignable dans une expression constante :

```
error: call to non-'constexpr' function
       'std::shared_ptr<...> std::make_shared(_Args&& ...)'
```

`std::make_shared` n'est pas `constexpr` dans libstdc++ 16, et `std::mutex` non
plus. Les trois helpers (`copy_on_write`, `synchronized_value`,
`asynchronous_task_launcher`) ont donc un comportement d'exécution que
l'architecture de test actuelle **ne peut pas atteindre du tout**.

## Survivant 4 — le correctif d'évasion de référence n'est épinglé par aucun test

`value_guard` supprime `operator*` et `operator->` sur rvalue :

```cpp
T& operator*() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");
```

Remplacer ce `= delete(...)` par une définition qui rend la référence ne fait
échouer aucun test. C'est le correctif de la faille la plus sérieuse relevée lors
de l'audit précédent : il est en place, mais rien ne le retient.

## Survivant 5 — `static_assert(sendable<T>)` dans `synchronized_value` : non testable, et c'est le bon choix

Un `static_assert` qui échoue dans le corps d'un template est une erreur dure :
aucun test qui passe ne peut l'observer. On peut le rendre détectable en
contraignant le template :

```cpp
template <sendable T>
class synchronized_value Ellipsis;
```

Vérifié : `requires { typename threadsafe::synchronized_value<NonSendable>; }`
devient alors `false`, donc testable. **Mais** cela casse un test existant,
`tests/test_synchronized_value.cpp:48` :

```cpp
static_assert(!is_synchronizable_v<threadsafe::synchronized_value<NonSendable>>,
              "is_synchronizable — the T still crosses threads one at a time, "
              "so a non-sendable T is not rescued by the mutex");
```

car le type devient **innommable** pour un `T` non sendable : la requête de trait
passe d'une réponse `false` à une erreur de compilation.

**Conclusion : garder le `static_assert`.** Le message est meilleur, la requête de
trait reste possible, et le garde-fou est de toute façon doublé par
`is_synchronizable<synchronized_value<T>> : is_sendable<T>`, qui est testé. À
documenter comme limite connue, pas à corriger.

---

## Correctifs proposés — code complet

Deux fichiers. Vérifié : ils tuent 4 des 5 survivants (le cinquième étant le
compromis assumé ci-dessus).

| survivant | tué par |
|---|---|
| `drop-borrowed-range-rule` | compile : `test_coverage_gaps` |
| `guard-allow-rvalue-star` | compile : `test_coverage_gaps` |
| `cow-never-detach` | **exécution** : `test_runtime_behaviour` |
| `cow-always-detach` | **exécution** : `test_runtime_behaviour` |
| `syncval-drop-sendable-assert` | survit (compromis assumé) |

### `tests/test_coverage_gaps.cpp` — reste à la compilation

```cpp
#include <threadsafe/threadsafe.h>

#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>

namespace {
using sync_int = threadsafe::synchronized_value<int>;

// A guard bound to a temporary is destroyed at the semicolon; the rvalue
// overloads are deleted so it cannot hand out a reference that outlives it.
template <class V>
concept star_on_temporary = requires(V v) { *v.lock(); };
template <class V>
concept arrow_on_temporary = requires(V v) { v.lock().operator->(); };
template <class V>
concept star_on_temporary_shared = requires(const V v) { *v.lock_shared(); };
template <class V>
concept arrow_on_temporary_shared =
    requires(const V v) { v.lock_shared().operator->(); };
}

using threadsafe::is_lifetime_aware_v;

// --- is_lifetime_aware: the borrowed_range rule, on a type the structural
// walk cannot reject on its own. span/string_view/subrange all store a raw
// pointer, so the member walk already refuses them and they do not exercise
// the rule. An empty view has no member to walk.
static_assert(std::ranges::borrowed_range<std::ranges::empty_view<int>>
                  && std::is_empty_v<std::ranges::empty_view<int>>,
              "the premise: a borrowed range with no data member at all");
static_assert(!is_lifetime_aware_v<std::ranges::empty_view<int>>,
              "is_lifetime_aware — only the borrowed_range rule can reject an "
              "empty view; the member walk finds nothing to object to");
static_assert(!is_lifetime_aware_v<std::ranges::ref_view<std::string>>,
              "is_lifetime_aware — a ref_view borrows the range it wraps");

// --- value_guard: the deleted rvalue accessors. Nothing else pins this, and a
// regression would silently reopen the reference-escape hole.
static_assert(!star_on_temporary<sync_int>,
              "value_guard — operator* on a temporary guard must be deleted");
static_assert(!arrow_on_temporary<sync_int>,
              "value_guard — operator-> on a temporary guard must be deleted");
static_assert(!star_on_temporary_shared<sync_int>,
              "value_guard — likewise for the shared guard");
static_assert(!arrow_on_temporary_shared<sync_int>,
              "value_guard — likewise for the shared guard");
```

### `tests/test_runtime_behaviour.cpp` — la seule TU avec un `main()`

Ce fichier rompt délibérément avec la règle « les tests sont uniquement à la
compilation », parce que les trois helpers ont un comportement qu'aucun
`static_assert` ne peut atteindre. Il passe, et il est propre sous
ThreadSanitizer.

```cpp
// The rest of the suite is compile-time only. These three helpers have runtime
// behaviour that no static_assert can reach — std::make_shared and std::mutex
// are not usable in a constant expression — so this translation unit has a
// main() and is meant to be run.
#include <threadsafe/threadsafe.h>

#include <cassert>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::synchronized_value;

namespace {

void copy_on_write_detaches_only_when_shared() {
    copy_on_write<std::vector<int>> original(std::size_t{3}, 7);

    auto shared_handle = original;
    shared_handle.as_mutable().push_back(42);
    assert(original->size() == 3
           && "a write through a shared handle must not touch the other one");
    assert(shared_handle->size() == 4);

    // A unique handle must hand back the same object every time: detaching
    // here would be a silent copy on the hot path.
    copy_on_write<std::vector<int>> unique_handle(std::size_t{2}, 1);
    const int *first_address = unique_handle.as_mutable().data();
    const int *second_address = unique_handle.as_mutable().data();
    assert(first_address == second_address
           && "a unique handle must not copy");

    // And once the other handle is gone, the survivor is unique again.
    {
        auto temporary_handle = unique_handle;
        (void)temporary_handle.as_mutable();
    }
    const int *after_release = unique_handle.as_mutable().data();
    assert(after_release == first_address
           && "dropping the other handle restores uniqueness");
}

void synchronized_value_serializes_writers() {
    constexpr int writer_count = 8;
    constexpr int increments_per_writer = 10000;

    synchronized_value<int> counter{0};
    std::vector<std::jthread> writers;
    for (int writer = 0; writer < writer_count; ++writer)
        writers.emplace_back([&counter] {
            for (int i = 0; i < increments_per_writer; ++i) {
                auto guard = counter.lock();
                ++*guard;
            }
        });
    writers.clear();

    auto final_guard = counter.lock();
    assert(*final_guard == writer_count * increments_per_writer
           && "every increment must survive");
}

void shared_readers_observe_a_consistent_value() {
    // T is const-synchronizable, so the wrapper picks a shared_mutex and the
    // readers below really do run concurrently.
    static_assert(std::is_same_v<synchronized_value<std::string>::mutex,
                                 std::shared_mutex>);

    synchronized_value<std::string> text{std::string(1000, 'a')};
    std::vector<std::jthread> readers;
    for (int reader = 0; reader < 8; ++reader)
        readers.emplace_back([&text] {
            for (int i = 0; i < 1000; ++i) {
                auto guard = text.lock_shared();
                assert(guard->size() == 1000);
                assert(guard->front() == guard->back());
            }
        });
    readers.clear();
}

void launcher_joins_every_task_at_destruction() {
    // A raw pointer keeps nothing alive, so the launcher refuses it: the
    // shared_ptr is the checked way to hand the counter to the tasks.
    auto completed = std::make_shared<std::atomic<int>>(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int task = 0; task < 16; ++task)
            launcher.launch_task(
                [](std::shared_ptr<std::atomic<int>> done) { ++*done; },
                completed);
    }
    assert(completed->load() == 16
           && "the launcher's destructor must join every task");
}

}

int main() {
    copy_on_write_detaches_only_when_shared();
    synchronized_value_serializes_writers();
    shared_readers_observe_a_consistent_value();
    launcher_joins_every_task_at_destruction();
    std::puts("runtime behaviour: OK");
}
```

### `tests/CMakeLists.txt` — accueillir les deux fichiers

```cmake
# Compile-time test suite: building this target IS running the tests.
# OBJECT library so no main() is needed and the TUs are part of ALL.
add_library(threadsafe_tests OBJECT
    test_synchronizable.cpp
    test_sendable.cpp
    test_containers.cpp
    test_smart_pointers.cpp
    test_lifetime_aware.cpp
    test_asynchronous_task_launcher.cpp
    test_soundness_regressions.cpp
    test_deferred_specialization.cpp
    test_synchronized_value.cpp
    test_copy_on_write.cpp
    test_diagnostics.cpp
    test_coverage_gaps.cpp
)
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)

# The helpers' runtime behaviour is out of reach of a static_assert: neither
# std::make_shared nor std::mutex is usable in a constant expression. This one
# target has a main() and must actually run.
add_executable(threadsafe_runtime_tests test_runtime_behaviour.cpp)
target_link_libraries(threadsafe_runtime_tests PRIVATE ThreadSafe::threadsafe)

enable_testing()
add_test(NAME runtime_behaviour COMMAND threadsafe_runtime_tests)
```

---

## Note de méthode — ThreadSanitizer et la barrière de `copy_on_write`

GCC sur macOS/arm64 ne fournit pas de runtime TSan. Le harnais utilisé ici
compile avec `g++-16` (nécessaire pour `-freflection`) et lie explicitement le
runtime d'Apple clang ; la détection de courses réelles a été vérifiée sur un
programme témoin.

Une limite importante subsiste, signalée par le compilateur lui-même :

```
warning: 'atomic_thread_fence' is not supported with '-fsanitize=thread' [-Wtsan]
```

La barrière `std::atomic_thread_fence(std::memory_order_acquire)` de
`as_mutable()` est donc **invisible pour TSan**. Un run propre ne prouve rien sur
sa correction : toute affirmation à son sujet doit être argumentée sur le modèle
mémoire, pas sur un passage de sanitizer.
