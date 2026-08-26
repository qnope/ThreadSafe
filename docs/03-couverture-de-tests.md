# 03 — Couverture de tests

**Verdict.** La suite compte 344 `static_assert` répartis sur onze unités de traduction, elle
compile proprement en 7,9 s, et elle laisse passer 77 mutations réelles sur 246 : score de mutation
**167/246 = 68 %**. Le trou n'est pas dispersé, il est structurel et tient en une phrase : *la suite
n'exécute rien*. Compiler la cible **est** lancer les tests, chaque test est un `static_assert`,
donc aucun comportement qui a besoin de `std::make_shared`, d'un mutex ou d'un thread n'est
atteignable — et c'est pourquoi on peut remplacer `if (ptr_.use_count() != 1)` par `if (false)` dans
`copy_on_write::as_mutable`, faire de la classe un alias partagé qui rend une `T&` dans un bloc que
d'autres threads sont en train de lire, et voir les onze TU passer sans broncher. Le second trou est
d'une autre nature : `tests/test_diagnostics.cpp` affirme que la moitié « qui rejette » des traits
*« is a compile error by design »* et donc non testable. **C'est faux.** `std::meta::exception` est
un objet `consteval` que l'on peut attraper dans une fonction `consteval` — exactement ce que fait
`default_is_sendable` — donc chaque *message* de rejet est une valeur de compilation et se teste au
`static_assert`. J'ai assemblé les 79 tests tueurs en **quatre fichiers complets plus trois TU de
rejet**, je les ai compilés et exécutés contre les en-têtes d'origine, et j'ai réappliqué **chacune
des 76 mutations tuables statiquement, une par une**, pour vérifier que le fichier correspondant
échoue bien. Résultat mesuré : 68 % → **99 %**, pour +3,7 s de compilation et +3,3 s de `ctest`.

Rapports frères : [00](./00-synthese.md) · [01](./01-robustesse-des-traits.md) ·
[02](./02-robustesse-des-helpers.md) · [04](./04-diagnostics.md) · [05](./05-simplicite.md) ·
[06](./06-performance-compilation.md) · [07](./07-performance-execution.md) ·
[08](./08-api-et-flexibilite.md) · [09](./09-methodologie.md)

---

## 1. Les chiffres

### 1.1 La campagne

| | mutants |
|---|---:|
| appliqués | 260 |
| tués par la suite existante | 167 |
| survivants | 91 |
| prouvés équivalents (aucun test ne peut les distinguer) | 14 |
| **survivants réels** | **77** |
| **score de mutation** | **167 / 246 = 68 %** |

Un mutant « équivalent » est une mutation qui ne change *aucune* réponse observable : la retirer du
dénominateur est la convention standard, sans quoi le score serait mécaniquement plafonné.

### 1.2 Par groupe

| groupe | appliqués | tués | survivants | équivalents | survivants réels | documentés ici |
|---|---:|---:|---:|---:|---:|---:|
| `sendable.h` + `synchronizable_base.h` | 53 | 30 | 23 | 3 | 20 | 20 |
| `synchronizable.h` — la marche const | 39 | 26 | 11 | 2 | 9 | 11 |
| `lifetime_aware.h` + `utils.h` | 60 | 38 | 22 | 3 | 19 | 19 |
| helpers + surface `std` | 108 | 73 | 35 | 6 | 29 | 29 |
| **total** | **260** | **167** | **91** | **14** | **77** | **79** |

La dernière colonne est le nombre de survivants effectivement décrits dans ce rapport, avec un
scénario distinguant et un test tueur. Elle vaut 79 et non 77 : deux entrées du groupe « marche
const » figurent dans la liste détaillée alors que le décompte du groupe les avait classées
équivalentes. Je signale l'écart plutôt que de l'arrondir — il ne change rien aux conclusions, et
les deux entrées en question sont bien tuables (je les ai tuées, cf. §4.2).

### 1.3 Ce qui a résisté

167 mutations sur 260 sont mortes tout de suite. Mes données ne listent pas les mutants tués
individuellement, donc je m'en tiens à ce qui est traçable :

- **Les formes de tableau bornées sont couvertes.** `is_synchronizable<T[N]> → std::true_type` meurt
  instantanément sur la paire `is_sendable_v<std::atomic<int>(*)[4]>` / `!is_sendable_v<int(*)[4]>`
  de `test_sendable.cpp:161`. C'est la forme *non bornée*, `T[]`, qui n'a aucune paire équivalente.
- **La règle `T&` a sa réponse booléenne testée.** `test_sendable.cpp` affirme `!is_sendable_v<int&>`
  et `is_sendable_v<SyncType&>`. Ce qui manque n'est pas la règle, c'est le `remove_cv` qu'elle
  applique et la forme `T&&` jumelle.
- **Le noyau structurel tient.** Les gardes `has_only_default_copy_move_destroy`,
  `has_unreflectable_state`, `is_complete_type` et le membre `mutable` ont chacun des mutations
  tuées ; ce qui survit sur ces gardes, ce sont les *raisons* qu'elles produisent, pas les réponses.
- **Le rapport de qualité des survivants est révélateur** : sur 79, **9 sont critiques et 19
  élevées**. Ce ne sont pas des trous cosmétiques.

Le cœur est solide. Ce qui manque, c'est un banc d'essai qui *exécute*, et la moitié diagnostique.

---

## 2. Le défaut qui domine tous les autres : la suite n'exécute rien

`CLAUDE.md` le dit sans détour, et `tests/CMakeLists.txt` le répète en commentaire :

> **Tests**: compile-time only (`static_assert`). Building the test target *is* running the tests;
> there is no runtime test framework.

C'est un choix cohérent tant que la bibliothèque ne contient que des traits. Elle contient aussi
`copy_on_write<T>`, `synchronized_value<T>` et `asynchronous_task_launcher`, dont le comportement
passe par `std::make_shared`, `std::shared_mutex::lock` et `std::jthread` — trois choses qu'aucune
expression constante n'atteint. Ces trois classes ont donc, littéralement, **zéro test de
comportement**.

### 2.1 La démonstration : `copy_on_write` cesse d'être copy-on-write

Voici la mutation. Elle tient en une ligne :

```diff
--- a/include/threadsafe/details/copy_on_write.h
+++ b/include/threadsafe/details/copy_on_write.h
@@ -25,7 +25,7 @@
     T& as_mutable()
         requires std::copy_constructible<T>
     {
-        if (ptr_.use_count() != 1)
+        if (false)
             ptr_ = std::make_shared<T>(*ptr_);
         else
             std::atomic_thread_fence(std::memory_order_acquire);
```

`as_mutable()` ne se détache plus jamais. Un écrivain reçoit une `T&` **dans le bloc partagé**, et
tous les lecteurs qui tiennent une autre poignée voient l'écriture — sur un objet dont le contrat
public est « lecture seule à travers `const` ». C'est exactement la course que la classe existe pour
empêcher. Les onze TU compilent sans un avertissement. *Vérifié personnellement par le lead.*

Le programme qui distingue l'original du mutant, complet :

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>

int main() {
    threadsafe::copy_on_write<std::string> writer{"original"};
    threadsafe::copy_on_write<std::string> reader = writer;
    const std::string* reader_target = &*reader;

    writer.as_mutable() = "mutated";

    std::printf("reader                 = %s\n", reader->c_str());
    std::printf("reader handle rebound  = %s\n",
                &*reader == reader_target ? "no" : "yes");
    std::printf("writer aliases reader  = %s\n",
                &*writer == reader_target ? "YES (race)" : "no");
}
```

| | original | mutant `if (false)` |
|---|---|---|
| `reader` | `original` | `mutated` |
| `reader handle rebound` | `no` | `no` |
| `writer aliases reader` | `no` | **`YES (race)`** |

Trois autres mutants de la même famille survivent pour la même raison : `use_count() != 0` (copie
toujours), `if (true)` (copie toujours), et l'échange des deux branches (copie quand unique, aliase
quand partagé — le pire des deux mondes). Plus `ATL-11`, qui supprime le `task.join()` explicite de
`launch_scoped_task` et laisse `~jthread` faire l'attente : `~jthread` appelle `request_stop()`
*avant* de joindre, donc une tâche coopérative reçoit l'ordre d'arrêt à l'instant où on la lance.

### 2.2 La correction : une TU d'exécution

Fichier **`tests/test_runtime_helpers.cpp`**, complet, tel qu'il compile et s'exécute chez moi :

```cpp
// tests/test_runtime_helpers.cpp
//
// The first RUNTIME translation unit of the suite. std::make_shared, locking a
// mutex and starting a thread are not constexpr, so none of the behaviour below
// is reachable from a static_assert: this is the only test in the project that
// has to be executed, not merely compiled.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        ++failures;
        std::printf("FAILED: %s\n", what);
    }
}

// as_mutable must detach when the block is shared: the writer gets a private
// copy and every other handle keeps the value it was reading.
void detaching_when_shared_leaves_the_other_handles_alone() {
    threadsafe::copy_on_write<std::string> writer{"original"};
    threadsafe::copy_on_write<std::string> reader = writer;
    const std::string* reader_target = &*reader;

    writer.as_mutable() = "mutated";

    check(*reader == "original",
          "as_mutable wrote through a shared block: the reader saw the write");
    check(&*reader == reader_target, "as_mutable rebound the reader's handle");
    check(*writer == "mutated", "as_mutable did not write for the writer");
    check(&*writer != reader_target,
          "as_mutable handed out a reference into the shared block");
}

// Conversely, it must NOT detach when the block is not shared, or the type is a
// deep-copy wrapper wearing a copy-on-write name.
void detaching_when_unique_does_not_reallocate() {
    threadsafe::copy_on_write<std::string> sole_owner{"value"};
    const std::string* target_before = &*sole_owner;

    sole_owner.as_mutable() += "-appended";

    check(&*sole_owner == target_before,
          "as_mutable copied although this handle was the sole owner");
    check(*sole_owner == "value-appended", "as_mutable lost the write");
}

// A copy of a non-const lvalue must share the block, not wrap it: the variadic
// constructor must never outrank the copy constructor.
void copying_a_handle_shares_the_block() {
    threadsafe::copy_on_write<std::string> original{"shared"};
    threadsafe::copy_on_write<std::string> copy_from_lvalue(original);

    check(&*copy_from_lvalue == &*original,
          "copying a copy_on_write built a second block");
}

// A T that swallows anything: without the constructor's guard the variadic
// constructor is an exact match for a non-const lvalue and outranks the copy
// constructor, so the "copy" wraps the handle instead of sharing its block.
struct Sink {
    int tag = 0;
    Sink() = default;
    explicit Sink(int value) : tag(value) {}
    template <class U>
    Sink(U&&) {}
};

void copying_a_greedy_handle_still_shares_the_block() {
    threadsafe::copy_on_write<Sink> original{7};
    threadsafe::copy_on_write<Sink> copy_from_lvalue(original);

    check(&*copy_from_lvalue == &*original,
          "copying a copy_on_write<Sink> built a second block: the variadic "
          "constructor outranked the copy constructor");
    check(copy_from_lvalue->tag == 7, "the copy did not carry the original value");
}

// synchronized_value hands out a lock for the duration of the guard, and the
// guard really excludes: 8 threads incrementing 25000 times each must land on
// exactly 200000.
void the_mutex_really_excludes() {
    threadsafe::synchronized_value<long long> counter{0};
    constexpr int thread_count = 8;
    constexpr int increments = 25000;

    std::vector<std::jthread> workers;
    for (int worker = 0; worker < thread_count; ++worker)
        workers.emplace_back([&counter] {
            for (int step = 0; step < increments; ++step) {
                const auto exclusive_guard = counter.lock();
                *exclusive_guard += 1;
            }
        });
    workers.clear();

    const auto shared_guard = counter.lock_shared();
    check(*shared_guard == 1LL * thread_count * increments,
          "synchronized_value lost increments: the guard did not exclude");
}

struct StopObserver {
    std::atomic<bool>* saw_stop_request;

    void operator()(std::stop_token token) const {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        saw_stop_request->store(token.stop_requested(),
                                std::memory_order_relaxed);
    }
};

// launch_scoped_task joins the task; it must not cancel it. Letting ~jthread do
// the waiting would request_stop() first, so a task that honours its token is
// told to stop the instant it is launched.
void a_scoped_task_is_joined_not_cancelled() {
    std::atomic<bool> saw_stop_request{false};
    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_scoped_task(StopObserver{&saw_stop_request});

    check(!saw_stop_request.load(std::memory_order_relaxed),
          "launch_scoped_task cancelled the task instead of joining it");
}

}

int main() {
    detaching_when_shared_leaves_the_other_handles_alone();
    detaching_when_unique_does_not_reallocate();
    copying_a_handle_shares_the_block();
    copying_a_greedy_handle_still_shares_the_block();
    the_mutex_really_excludes();
    a_scoped_task_is_joined_not_cancelled();

    if (failures == 0)
        std::printf("all runtime checks passed\n");
    return failures == 0 ? 0 : 1;
}
```

### 2.3 Le changement CMake, exact et vérifié de bout en bout

`CMakeLists.txt` racine — une seule ligne ajoutée :

```cmake
option(THREADSAFE_BUILD_TESTS "Build compile-time tests" ${PROJECT_IS_TOP_LEVEL})
if(THREADSAFE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

`tests/CMakeLists.txt` — le bloc ajouté après le `target_link_libraries` existant :

```cmake
# The helpers' behaviour is not reachable from a static_assert: std::make_shared,
# locking a mutex and starting a thread are not constexpr. This is the only test
# in the suite that has to be RUN.
find_package(Threads REQUIRED)

add_executable(threadsafe_runtime_tests test_runtime_helpers.cpp)
target_link_libraries(threadsafe_runtime_tests
    PRIVATE ThreadSafe::threadsafe Threads::Threads)
add_test(NAME runtime_helpers COMMAND threadsafe_runtime_tests)
```

Vérification que j'ai faite moi-même, sur une copie du dépôt :

```
$ cmake -B build -DCMAKE_CXX_COMPILER=g++-16 && cmake --build build && (cd build && ctest)
    Start 1: runtime_helpers
1/4 Test #1: runtime_helpers ..........................   Passed    0.64 sec
```

et, mutation par mutation (`-Imut` désigne un arbre d'en-têtes muté) :

| mutant | résultat de `test_runtime_helpers` |
|---|---|
| `COW-03` `use_count() != 1` → `false` | **KILLED** — `as_mutable wrote through a shared block: the reader saw the write` + `as_mutable handed out a reference into the shared block` |
| `COW-09` branches échangées | **KILLED** — 3 lignes `FAILED` |
| `COW-02` `use_count() != 0` | **KILLED** — `as_mutable copied although this handle was the sole owner` |
| `COW-04` `if (true)` | **KILLED** — idem |
| `COW-14` garde du constructeur retirée | **KILLED** — `copying a copy_on_write<Sink> built a second block` |
| `ATL-11` `task.join()` retiré | **KILLED** — `launch_scoped_task cancelled the task instead of joining it` |
| en-têtes d'origine | `all runtime checks passed`, code de sortie 0 |

---

## 3. Les lacunes règle par règle, la pire d'abord

### 3.1 `is_sendable<T&&>` : 0 mutant tué sur 4 — la règle entière n'est pas testée

C'est la lacune la plus nette de la campagne. Quatre mutations frappent cette seule ligne, les
quatre survivent :

| mutant | mutation | ce que ça rend vrai |
|---|---|---|
| `M09` | `: std::true_type` | `int&&`, `std::string&&`, `int*&&` — **tout** |
| `M10` | `: is_sendable<std::remove_cv_t<T>>` | `std::string&&`, `std::vector<int>&&`, `Plain&&` |
| `M12` | `: is_synchronizable<const std::remove_cv_t<T>>` | tout type de données simples |
| `M11` | `: is_synchronizable<T>` (sans `remove_cv`) | `const int&&` |

*`M09` re-vérifié personnellement par le lead comme survivant aux onze TU.*

La règle est pourtant le cœur du modèle : `T&&` **est une référence**, donc l'envoyer partage le
référent, donc c'est `is_synchronizable<T>` qui décide — jamais `is_sendable<T>`, qui est la question
« un seul thread à la fois ». `M10` confond précisément les deux questions que la bibliothèque existe
pour distinguer, et sur une conférence internationale c'est *la* diapositive : Send ≠ Sync.

La conséquence pratique passe par le lanceur. `launchable_scoped_task` ne garde un argument que par
`sendable<Args>` ; avec `M09` la garde est ouverte pour tout type du programme.

Le programme distinguant, complet :

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {
struct SyncType {};
}

template <>
struct threadsafe::is_synchronizable<SyncType> : std::true_type {};

#define P(...) std::printf("%-24s = %s\n", #__VA_ARGS__, \
                           threadsafe::is_sendable_v<__VA_ARGS__> ? "true" : "false")

int main() {
    P(int&&);
    P(const int&&);
    P(std::string&&);
    P(int*&&);
    P(std::vector<int>&&);
    P(SyncType&&);
}
```

| type | original | `M09` | `M10` | `M12` | `M11` |
|---|---|---|---|---|---|
| `int&&` | false | **true** | **true** | **true** | false |
| `const int&&` | false | **true** | **true** | **true** | **true** |
| `std::string&&` | false | **true** | **true** | **true** | false |
| `int*&&` | false | **true** | false | false | false |
| `std::vector<int>&&` | false | **true** | **true** | **true** | false |
| `SyncType&&` | true | true | true | true | true |

Cinq `static_assert` suffisent à tuer les quatre (bloc « `is_sendable<T&&>` » de
`test_trait_rules.cpp`, §4.1) — je l'ai vérifié en réappliquant les quatre mutations une par une.

### 3.2 La boucle `bases_of` de la marche const peut être supprimée intégralement

```diff
--- a/include/threadsafe/details/synchronizable.h
+++ b/include/threadsafe/details/synchronizable.h
@@ -172,11 +172,5 @@
-    for (info base : bases_of(type, context))
-        if (!is_synchronizable_type(add_const(type_of(base))))
-            explain_const_synchronizable(
-                base, u8"is not readable from several threads at once",
-                type_of(base), path);
-
     for (info member : nonstatic_data_members_of(type, context)) {
```

*Re-vérifié personnellement par le lead : les onze TU passent.* Une variante plus sournoise survit
aussi (`M24`) : demander `is_sendable_type(type_of(base))` au lieu de
`is_synchronizable_type(add_const(type_of(base)))` — c'est-à-dire poser la question « un thread à la fois »
là où il faut poser « plusieurs threads en même temps ».

Le type qui les distingue déclare **zéro membre propre** : tout ce sur quoi les lecteurs peuvent
courir vit dans la base, donc la base est la seule chose que la marche puisse regarder.

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>

namespace {
struct MutableBase {
    mutable int cached;
};
struct DerivesFromMutableBase : MutableBase {};
}

int main() {
    std::printf("is_sendable_v<DerivesFromMutableBase>            = %s\n",
                threadsafe::is_sendable_v<DerivesFromMutableBase> ? "true" : "false");
    std::printf("is_synchronizable_v<const MutableBase>           = %s\n",
                threadsafe::is_synchronizable_v<const MutableBase> ? "true" : "false");
    std::printf("is_synchronizable_v<const DerivesFromMutableBase> = %s\n",
                threadsafe::is_synchronizable_v<const DerivesFromMutableBase> ? "true" : "false");
}
```

Original : `true / false / false`. Mutant `M25` (boucle supprimée) : `true / false / **true**`.
Mutant `M24` (`is_sendable` à la place) : `true / false / **true**`, puisque `MutableBase` *est*
envoyable.

Deux clients en dépendent directement, et ce sont eux qui donnent au trou sa portée :
`copy_on_write<DerivesFromMutableBase>` deviendrait envoyable (les lecteurs ne voient qu'un
`const T`, mais ce `const T` écrit), et `synchronized_value<DerivesFromMutableBase>::mutex`
basculerait de `std::mutex` à `std::shared_mutex` — deux threads en `shared_lock` écrivant le même
`cached`. Le test tueur affirme les deux, c'est ce qui en fait un test intéressant plutôt qu'une
assertion de plus.

### 3.3 `is_synchronizable<std::stop_token> = true` réintroduit exactement le bug corrigé par 79f1e4f

Le commit `79f1e4f` s'intitule *« Do not bless writing through a shared stop_token / stop_source »*.
Il a retiré le trait non qualifié et laissé seulement la forme `const`, avec le commentaire de
`vocabulary.h` qui explique pourquoi : `[stoptoken.general]` ne promet que `request_stop`,
`stop_requested` et `stop_possible` ; l'affectation et le `swap` touchent le compteur de références
de l'état partagé.

On peut remettre la ligne. Rien ne le remarque. *Re-vérifié personnellement par le lead.*

```diff
--- a/include/threadsafe/details/vocabulary.h
+++ b/include/threadsafe/details/vocabulary.h
@@ -30,6 +30,8 @@
 template <>
 struct is_sendable<std::stop_source> : std::true_type {};
 
+template <>
+struct is_synchronizable<std::stop_token> : std::true_type {};
 template <>
 struct is_synchronizable<const std::stop_token> : std::true_type {};
```

`is_sendable_v<std::stop_token&>` passe de `false` à `true` : on peut donner à un autre thread une
référence non-const vers un `stop_token` qu'on continue d'utiliser, et les deux threads courent sur
le compteur de références.

**Un bug corrigé sans test de non-régression n'est pas corrigé, il est en sursis.** C'est le
message le plus court et le plus transférable de tout ce rapport, et il tient sur une diapositive.
Le même trou existe pour `stop_source` (`VOC-02`). Deux `static_assert` referment les deux :

```cpp
static_assert(!threadsafe::is_synchronizable_v<std::stop_token>,
              "is_synchronizable — a stop_token is a refcounted handle; "
              "assigning to a shared one races on the reference count");
static_assert(!threadsafe::is_synchronizable_v<std::stop_source>,
              "is_synchronizable — same for a stop_source");
static_assert(!threadsafe::is_sendable_v<std::stop_token&>,
              "is_sendable — sharing a stop_token by reference gives the other "
              "thread a write path into the handle itself");
static_assert(!threadsafe::is_sendable_v<std::stop_source&>,
              "is_sendable — same for a stop_source");
```

Ils sont dans `test_trait_rules.cpp` (§4.1) ; ils iraient tout aussi bien dans
`test_soundness_regressions.cpp`, qui est le fichier dont c'est déjà le rôle. Le choix est cosmétique
— la seule chose qui compte est qu'ils existent.

### 3.4 « `*is* a compile error by design` » : la moitié diagnostique est parfaitement testable

`tests/test_diagnostics.cpp` s'ouvre sur ceci :

```cpp
// The assert_* functions are the diagnostic face of the traits: they agree with
// the trait on a conforming type (they compile and return), and turn a "false"
// into a std::meta::exception naming the culprit. Only the agreeing half is
// testable here — the throwing half *is* a compile error by design.
```

Le fichier ne contient donc que 9 `static_assert`, tous du côté « ça passe ». La campagne de mutation
établit que la seconde phrase est **fausse**, et le mécanisme est déjà dans la bibliothèque : à
`sendable.h:171`,

```cpp
inline consteval bool default_is_sendable(std::meta::info type) {
    try {
        diagnose_default_is_sendable(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}
```

`assert_sendable<T>()` ne *produit pas* de diagnostic : il **lance** une `std::meta::exception`.
Attraper cette exception dans une fonction `consteval` est exactement ce que fait le trait lui-même,
et `u8what()` est `consteval`. Donc le message de rejet est une valeur de compilation, et un
`static_assert` peut le contraindre :

```cpp
template <class Probe>
consteval bool rejection_mentions(Probe probe, std::u8string_view expected) {
    try {
        probe();
    } catch (const std::meta::exception& rejection) {
        return rejection.u8what().find(expected) != std::u8string_view::npos;
    }
    return false;
}

static_assert(rejection_mentions([] { threadsafe::assert_sendable<int*>(); },
                                 u8"is a pointer or a reference"));
```

C'est le résultat le plus rentable de la campagne, parce qu'il débloque **25 des 79 survivants d'un
coup** — c'est-à-dire près d'un tiers. La suite ne vérifie aujourd'hui *aucune* raison de rejet, et
la conséquence est que la moitié des gardes de `sendable.h`, `synchronizable.h`, `lifetime_aware.h`
et la totalité de `utils.h` (`describe`, `path_step`, `member_name`, `reject`, `reject_at`) sont, du
point de vue de la suite, du code mort.

Quelques exemples de ce qui passe aujourd'hui :

| mutation | ce que l'utilisateur lit à la place |
|---|---|
| `M19` supprimer le rejet pointeur/référence de la marche `sendable` | `int* is not sendable: is_sendable is specialized to false for it` — **un mensonge**, rien n'est spécialisé |
| `M20` ne tester que `is_reference_type` | idem, mais seulement pour les pointeurs : le motif de rejet **le plus fréquent** de la bibliothèque |
| `M30` supprimer le rejet « type incomplet » | `neither complete class type nor namespace` — l'erreur interne de la réflexion, au lieu du conseil sur le pimpl |
| `M49` inverser `path.empty()` dans `explain_sendable` | la marche profonde s'inverse : c'est l'appelant *sans* chemin (le trait) qui paie le coût quadratique, et celui *avec* chemin (`assert_*`) qui s'arrête au premier saut |
| `U31` `member_name` renvoie toujours `<unnamed>` | tous les chemins perdent le nom que l'utilisateur a écrit |
| `U25` `describe` perd sa branche « classe de base » | une base est nommée avec le display string de la dérivée |
| `L15` déplacer la garde `borrowed_range` **après** la marche des membres | une `std::span` est rejetée pour son pointeur membre, pas pour ce qu'elle est |

`M49` mérite un mot de plus : le commentaire de `sendable.h:77-80` justifie l'asymétrie par une mesure
(« *measured at 38x on a 60-level chain* »). Inverser le test conserve les réponses booléennes,
dégrade les messages, **et** rend chaque réponse `false` du trait quadratique. Une régression de
performance de cette taille passe la suite entière.

Le fichier complet qui teste les messages est en §4.2. Il tue les 25.

---

## 4. Les fichiers, prêts à ajouter

Plutôt que 79 fragments, sept fichiers. Chacun est donné **entier** ; chacun a été compilé contre
les en-têtes d'origine, et chaque mutation qu'il est censé tuer a été réappliquée une par une et
confirmée.

| fichier | survivants tués | statut vérifié |
|---|---:|---|
| `tests/test_trait_rules.cpp` | 36 | compile propre **et** tue les 36 (36/36 réappliquées et confirmées) |
| `tests/test_diagnostic_messages.cpp` | 25 | compile propre **et** tue les 25 (25/25 réappliquées et confirmées) |
| `tests/test_helper_contracts.cpp` | 7 | compile propre **et** tue les 7 (7/7 réappliquées et confirmées) |
| `tests/test_runtime_helpers.cpp` | 5 (+1 partagé avec `test_helper_contracts`) | s'exécute, code 0 **et** tue les 6 (6/6 réappliquées et confirmées) |
| `tests/rejections/non_sendable_synchronized_value.cpp` | 2 | refusé sur l'original, accepté sur les 2 mutants — confirmé via `ctest` |
| `tests/rejections/discarded_{exclusive,shared}_guard.cpp` | 1 | idem, et les deux TU sont nécessaires (confirmé) |
| `tests/test_runtime_cow_fence_race.cpp` | 3 | **plus faible** : cible ThreadSanitizer, non exécutable sur cette machine (voir §4.5) |

Le fichier d'exécution `tests/test_runtime_helpers.cpp` est donné en entier au **§2.2**, avec son
changement CMake ; il n'est pas répété ici.

Total : **76 des 79 survivants documentés**, tous vérifiés individuellement sur cette machine
(GCC 16.2.0). Les 3 restants sont les mutants de barrière mémoire, qui exigent un TSan que
`g++-16` ne sait pas lier ici.

Score de mutation après ajout : de **167/246 (68 %)** à **243/246 (99 %)** — à ±2 près, l'écart de
comptabilité signalé en §1.2.

### 4.1 `tests/test_trait_rules.cpp` — 36 survivants

Les règles que la suite énonce pour une orthographe d'un type et jamais pour les autres : la forme
`T&&`, le `remove_cv` que chaque règle d'indirection applique, les tableaux de borne inconnue, la
boucle `bases_of` de la marche const, et les spécialisations de `vocabulary.h` qui portent la garde
de non-régression de `79f1e4f`.

```cpp
// tests/test_trait_rules.cpp
//
// The rules the existing suite states for one spelling of a type and never for
// the others: the T&& form, the cv-stripping every indirection rule performs,
// the unbounded array forms, the const walk's base loop, and the vocabulary
// specializations that carry a fixed bug's regression guard.
#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <concepts>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <vector>

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// ---------------------------------------------------------------------------
// is_sendable<T&&> — the rule the suite never states at all.
// ---------------------------------------------------------------------------
namespace rvalue_reference_rule {
struct SelfSynchronizing {};
}

template <>
struct threadsafe::is_synchronizable<rvalue_reference_rule::SelfSynchronizing>
    : std::true_type {};

static_assert(is_sendable_v<rvalue_reference_rule::SelfSynchronizing&&>,
              "is_sendable — an rvalue reference to a synchronizable type is "
              "sendable, exactly like the lvalue form");
static_assert(!is_sendable_v<int&&>,
              "is_sendable — an rvalue reference is still a reference: it "
              "shares the referent, so is_synchronizable<int> decides");
static_assert(!is_sendable_v<const int&&>,
              "is_sendable — the referent's cv is stripped before the "
              "question: is_synchronizable<int> decides, never "
              "is_synchronizable<const int>");
static_assert(!is_sendable_v<std::string&&>,
              "is_sendable — an owning type is not synchronizable, so a "
              "reference to it may not cross");
static_assert(!is_sendable_v<std::vector<int>&&>,
              "is_sendable — the T&& rule wins over the std-wrapper rule, as "
              "the T& rule already does");

// ---------------------------------------------------------------------------
// The same cv-stripping, on the T& and T* forms.
// ---------------------------------------------------------------------------
static_assert(!is_sendable_v<const int&>,
              "is_sendable — a const lvalue reference still shares an object "
              "another alias may hold as int&");
static_assert(!is_sendable_v<const int*>,
              "is_sendable — a pointer to const still shares an object another "
              "alias may hold as int*");
static_assert(!is_sendable_v<const char*>,
              "is_sendable — likewise for the commonest borrow of all");

// ---------------------------------------------------------------------------
// Arrays of unknown bound — the shape unique_ptr<T[]> and new T[n] produce.
// ---------------------------------------------------------------------------
namespace array_rules {
struct UserCopyCtor {
    UserCopyCtor(const UserCopyCtor&);
};
struct OptedOut {};
}

template <>
struct threadsafe::is_sendable<std::vector<array_rules::OptedOut>>
    : std::false_type {};

static_assert(is_sendable_v<int[]>,
              "is_sendable — an array of unknown bound follows its element type");
static_assert(!is_sendable_v<int*[]>,
              "is_sendable — the missing extent is not a licence");
static_assert(!is_sendable_v<array_rules::UserCopyCtor[]>,
              "is_sendable — same rule for a structurally rejected element");
static_assert(!is_synchronizable_v<int[]>,
              "is_synchronizable — an array of unknown bound follows its "
              "element type, and int is not synchronizable");
static_assert(is_synchronizable_v<std::atomic<int>[]>,
              "is_synchronizable — the element is what decides, bounded or not");
static_assert(is_synchronizable_v<std::atomic<int>[4]>,
              "is_synchronizable — and the bounded form agrees");
static_assert(!is_sendable_v<int(*)[]>,
              "is_sendable — a pointer to an array of unknown bound shares the "
              "array, so the element's synchronizability decides");
static_assert(is_sendable_v<std::atomic<int>(*)[]>,
              "is_sendable — and it really is the element that decides");
static_assert(is_synchronizable_v<const int[]>,
              "is_synchronizable — an unbounded const array follows its "
              "element type, exactly as the bounded form does");
static_assert(is_synchronizable_v<const array_rules::OptedOut[]>,
              "is_synchronizable — and for a class element too");

static_assert(!is_sendable_v<std::vector<array_rules::OptedOut>>,
              "the premise: the element type is opted out of sendability");
static_assert(!is_sendable_v<const std::vector<array_rules::OptedOut>[4]>,
              "is_sendable — an array asks its element type with the cv "
              "stripped, so the opt-out survives the const");

// ---------------------------------------------------------------------------
// The structural member walk strips the member's cv (is_sendable side).
// ---------------------------------------------------------------------------
namespace member_cv_rules {
struct OptedOut {};
struct HoldsConstOptedVector {
    const std::vector<OptedOut> vector;
};
}

template <>
struct threadsafe::is_sendable<std::vector<member_cv_rules::OptedOut>>
    : std::false_type {};

static_assert(!is_sendable_v<std::vector<member_cv_rules::OptedOut>>,
              "the premise: the member's type is opted out of sendability");
static_assert(!is_sendable_v<member_cv_rules::HoldsConstOptedVector>,
              "is_sendable — the member walk strips the member's cv before "
              "asking, so the opt-out is not laundered by the const");

// ---------------------------------------------------------------------------
// A user-written move member blocks the structural default, as a copy does.
// ---------------------------------------------------------------------------
namespace move_member_rules {
struct UserMoveCtor {
    UserMoveCtor(UserMoveCtor&&);
};
struct UserMoveAssign {
    UserMoveAssign() = default;
    UserMoveAssign& operator=(UserMoveAssign&&);
};
}

static_assert(!is_sendable_v<move_member_rules::UserMoveCtor>,
              "is_sendable — a user-provided move constructor blocks the "
              "structural default, exactly as a copy constructor does");
static_assert(!is_synchronizable_v<const move_member_rules::UserMoveCtor>,
              "is_synchronizable — the const question uses the same guard");
static_assert(!is_sendable_v<move_member_rules::UserMoveAssign>,
              "is_sendable — a user-provided move assignment blocks it too");
static_assert(!is_synchronizable_v<const move_member_rules::UserMoveAssign>,
              "is_synchronizable — the const question uses the same guard");

// ---------------------------------------------------------------------------
// is_synchronizable<const T>: the base-class loop, and the cv it strips.
// ---------------------------------------------------------------------------
namespace const_walk_rules {
struct MutableBase {
    mutable int cached;
};
struct DerivesFromMutableBase : MutableBase {};

struct BorrowingBase {
    int* borrowed;
};
struct DerivesFromBorrowingBase : BorrowingBase {};

struct PlainBase {
    int value;
};
struct DerivesFromPlainBase : PlainBase {};

struct VouchedOpaque {
    int* borrowed;
};

struct HoldsVolatileShared {
    volatile std::shared_ptr<std::atomic<int>> handle;
};
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const_walk_rules::VouchedOpaque);

static_assert(is_sendable_v<const_walk_rules::MutableBase>
                  && is_sendable_v<const_walk_rules::DerivesFromMutableBase>,
              "the premise: one thread at a time is fine, which is why the "
              "base loop may not ask is_sendable");
static_assert(!is_synchronizable_v<const const_walk_rules::MutableBase>,
              "the premise: a mutable member defeats const");
static_assert(!is_synchronizable_v<const const_walk_rules::DerivesFromMutableBase>,
              "is_synchronizable — a base is reached through the derived "
              "object's const, so its const form must be read-safe too");
static_assert(!is_synchronizable_v<const const_walk_rules::DerivesFromBorrowingBase>,
              "is_synchronizable — a base that borrows gives every reader a "
              "write path");
static_assert(is_synchronizable_v<const const_walk_rules::DerivesFromPlainBase>,
              "is_synchronizable — a plain base costs nothing");

static_assert(!is_sendable_v<threadsafe::copy_on_write<
                  const_walk_rules::DerivesFromMutableBase>>,
              "copy_on_write — readers only ever see a const T, so a T whose "
              "base writes through const would race behind the shared handle");
static_assert(
    std::same_as<threadsafe::synchronized_value<
                     const_walk_rules::DerivesFromMutableBase>::mutex,
                 std::mutex>,
    "synchronized_value — no shared_mutex: the inherited mutable member means "
    "there is no read that may be shared");

static_assert(is_synchronizable_v<const const_walk_rules::VouchedOpaque>,
              "is_synchronizable — a vouched-for type keeps its answer");
static_assert(is_synchronizable_v<const volatile const_walk_rules::VouchedOpaque>,
              "is_synchronizable — volatile adds no writer: the walk strips the "
              "cv before asking the trait back, so the user's answer survives");

static_assert(is_synchronizable_v<const std::shared_ptr<std::atomic<int>>>,
              "the premise: without the volatile, the smart-pointer rule "
              "answers true");
static_assert(!is_synchronizable_v<const const_walk_rules::HoldsVolatileShared>,
              "is_synchronizable — a member is asked under its own "
              "cv-qualification, not under a laundered one");

// ---------------------------------------------------------------------------
// is_lifetime_aware: void, closures, cv-forwarding and the borrowed_range guard.
// ---------------------------------------------------------------------------
static_assert(!is_lifetime_aware_v<void>,
              "is_lifetime_aware — void holds no value to own");
static_assert(!is_lifetime_aware_v<const void>,
              "is_lifetime_aware — cv-qualified void holds no value either");

namespace lifetime_closure_rules {
[[maybe_unused]] auto capture_pointer(int* raw) { return [raw] { return *raw; }; }
[[maybe_unused]] auto capture_reference(std::string& text) {
    return [&text] { text += "x"; };
}

using CapturesPointer = decltype(capture_pointer(nullptr));
using CapturesReference =
    decltype(capture_reference(*std::declval<std::string*>()));
}

static_assert(std::is_class_v<lifetime_closure_rules::CapturesPointer>
                  && !std::is_empty_v<lifetime_closure_rules::CapturesPointer>,
              "premise: a capturing closure is a non-empty class whose "
              "captures reflection cannot enumerate");
static_assert(!is_lifetime_aware_v<lifetime_closure_rules::CapturesPointer>,
              "is_lifetime_aware — a closure with captures holds state the "
              "recursion cannot inspect");
static_assert(!is_lifetime_aware_v<lifetime_closure_rules::CapturesReference>,
              "is_lifetime_aware — least of all a capture that borrows");
static_assert(is_lifetime_aware_v<decltype([] {})>,
              "is_lifetime_aware — a captureless closure is empty: nothing to "
              "hide");

static_assert(is_lifetime_aware_v<std::shared_ptr<int>>,
              "premise: the unqualified form is answered by a specialization");
static_assert(is_lifetime_aware_v<const std::shared_ptr<int>>,
              "is_lifetime_aware — const on the handle forwards to the "
              "specialization for the handle");
static_assert(is_lifetime_aware_v<const std::unique_ptr<int>>,
              "is_lifetime_aware — same for the unique handle");
static_assert(is_lifetime_aware_v<const std::stop_token>,
              "is_lifetime_aware — and for a full specialization in "
              "vocabulary.h");

namespace lifetime_cv_rules {
// Borrows through `raw`, so it is not an owner. A user vouches for its *const*
// form only — the same const-keyed idiom the suite already uses for
// THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const ImmutableNode).
struct Handle {
    int* raw;
};
struct HoldsConstHandle {
    const Handle handle;
};
}

template <>
struct threadsafe::is_lifetime_aware<const lifetime_cv_rules::Handle>
    : std::true_type {};

static_assert(!is_lifetime_aware_v<lifetime_cv_rules::Handle>
                  && is_lifetime_aware_v<const lifetime_cv_rules::Handle>,
              "premise: only the const form is vouched for");
static_assert(!is_lifetime_aware_v<std::shared_ptr<const lifetime_cv_rules::Handle>>,
              "is_lifetime_aware — cv on the pointee is stripped: another "
              "handle to the same object is a shared_ptr<Handle>");
static_assert(!is_lifetime_aware_v<std::weak_ptr<const lifetime_cv_rules::Handle>>,
              "is_lifetime_aware — the weak form asks the same question");
static_assert(!is_lifetime_aware_v<lifetime_cv_rules::HoldsConstHandle>,
              "is_lifetime_aware — the member's cv is stripped before the "
              "recursion, so a const-only vouch cannot launder a borrow");
static_assert(!is_lifetime_aware_v<const lifetime_cv_rules::Handle[4]>,
              "is_lifetime_aware — the bounded array strips the element's cv");
static_assert(!is_lifetime_aware_v<const lifetime_cv_rules::Handle[]>,
              "is_lifetime_aware — and so does the unbounded array");

namespace borrowed_range_rules {
// Owns every byte it holds — and still hands out iterators that outlive it.
// Nothing in its members says so; only enable_borrowed_range does. This is the
// type the borrowed_range guard exists for: std::span and std::string_view are
// also rejected by the pointer member the structural walk finds in them, so
// they cannot tell the guard apart from the walk.
struct OwningButBorrowed {
    std::vector<int> data;
    int* begin() const;
    int* end() const;
};
}

template <>
inline constexpr bool std::ranges::enable_borrowed_range<
    borrowed_range_rules::OwningButBorrowed> = true;

static_assert(std::ranges::borrowed_range<borrowed_range_rules::OwningButBorrowed>,
              "premise: the type really is a borrowed range");
static_assert(is_lifetime_aware_v<std::vector<int>>,
              "premise: every member of it is lifetime aware on its own");
static_assert(!is_lifetime_aware_v<borrowed_range_rules::OwningButBorrowed>,
              "is_lifetime_aware — a borrowed range is rejected for being one, "
              "not by luck of holding a raw pointer the walk can see");
static_assert(!is_lifetime_aware_v<std::vector<borrowed_range_rules::OwningButBorrowed>>,
              "is_lifetime_aware — and that rejection travels through a "
              "container");

// ---------------------------------------------------------------------------
// std::unique_ptr's deleter travels inside the handle.
// ---------------------------------------------------------------------------
namespace deleter_rules {
struct PoolDeleter {
    std::atomic<int>* live_count;
    void operator()(int* raw) const noexcept {
        live_count->fetch_sub(1);
        delete raw;
    }
};

using pooled = std::unique_ptr<int, PoolDeleter>;
}

static_assert(is_sendable_v<deleter_rules::PoolDeleter>
                  && !is_lifetime_aware_v<deleter_rules::PoolDeleter>,
              "the premise: the deleter may cross, but it borrows the counter");
static_assert(is_sendable_v<deleter_rules::pooled>,
              "the premise: the handle itself may cross");
static_assert(!is_lifetime_aware_v<deleter_rules::pooled>,
              "is_lifetime_aware — the deleter travels inside the unique_ptr, "
              "so a deleter that borrows makes the whole handle a borrower");
static_assert(!threadsafe::launchable_task<decltype([](deleter_rules::pooled) {}),
                                           deleter_rules::pooled>,
              "launch_task — and that is what stops the handle being handed to "
              "a thread that may outlive the pool");

// ---------------------------------------------------------------------------
// The std-wrapper rule: the synchronizable short-circuit, and std::array.
// ---------------------------------------------------------------------------
namespace std_wrapper_rules {
struct SelfGuarding {
    SelfGuarding(const SelfGuarding&);
    int* borrowed;
};
struct NonSendable {
    NonSendable(const NonSendable&);
};
struct MutableCache {
    int raw;
    mutable int parsed;
};
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(
    std::vector<std_wrapper_rules::SelfGuarding>);

static_assert(!is_sendable_v<std_wrapper_rules::SelfGuarding>,
              "the premise: the element is not sendable on its own");
static_assert(is_synchronizable_v<std::vector<std_wrapper_rules::SelfGuarding>>,
              "the premise: the container is vouched for");
static_assert(is_sendable_v<std::vector<std_wrapper_rules::SelfGuarding>>,
              "is_sendable — a std wrapper that synchronizes itself is "
              "sendable whatever it holds; the wrapper rule must short-circuit "
              "on the synchronizable question exactly as the walk does");

// std::array<T, 0> holds no element object at all, so the structural walk sees
// an empty class and would answer yes for every T. The allow-list rule reads
// the template arguments instead.
static_assert(!is_sendable_v<std::array<std_wrapper_rules::NonSendable, 0>>,
              "is_sendable — the element type decides, even when the array "
              "stores none of them");
static_assert(!is_lifetime_aware_v<std::array<int*, 0>>,
              "is_lifetime_aware — likewise for ownership");
static_assert(!is_synchronizable_v<const std::array<std_wrapper_rules::MutableCache, 0>>,
              "is_synchronizable — likewise for the const read");

// ---------------------------------------------------------------------------
// vocabulary.h — the regression guard for the bug fixed in commit 79f1e4f,
// and the two stateless-allocator rules.
// ---------------------------------------------------------------------------
static_assert(!is_synchronizable_v<std::stop_token>,
              "is_synchronizable — a stop_token is a refcounted handle; "
              "assigning to a shared one races on the reference count");
static_assert(!is_synchronizable_v<std::stop_source>,
              "is_synchronizable — same for a stop_source");
static_assert(!is_sendable_v<std::stop_token&>,
              "is_sendable — sharing a stop_token by reference gives the other "
              "thread a write path into the handle itself");
static_assert(!is_sendable_v<std::stop_source&>,
              "is_sendable — same for a stop_source");
static_assert(is_sendable_v<std::stop_source>,
              "is_sendable — sending a stop_source is a copy, and copying the "
              "handle is refcount-atomic-safe");
static_assert(is_synchronizable_v<const std::stop_source>,
              "is_synchronizable — [stoptoken.general] makes request_stop, "
              "stop_requested and stop_possible race-free");
static_assert(is_lifetime_aware_v<std::stop_source>,
              "is_lifetime_aware — a stop_source owns a reference to the "
              "shared stop state and keeps it alive");

static_assert(is_sendable_v<std::allocator<std_wrapper_rules::NonSendable>>,
              "is_sendable — std::allocator holds no state, so its answer does "
              "not depend on T");
static_assert(!is_synchronizable_v<const std_wrapper_rules::MutableCache>,
              "the premise: a mutable member defeats const");
static_assert(
    is_synchronizable_v<const std::allocator<std_wrapper_rules::MutableCache>>,
    "is_synchronizable — a const std::allocator is read-safe whatever T is");
static_assert(!is_lifetime_aware_v<int*>,
              "the premise: a raw pointer owns nothing");
static_assert(is_lifetime_aware_v<std::allocator<int*>>,
              "is_lifetime_aware — std::allocator owns nothing and borrows "
              "nothing whatever T is");
```

Vérification, mutation par mutation (`g++-16 -std=c++26 -freflection -Imut -fsyntax-only`) :

| mutant | mutation appliquée | résultat |
|---|---|---|
| `M09` `M10` `M12` `M11` | les quatre formes de `is_sendable<T&&>` | KILLED × 4 |
| `M07` `M03` | `is_sendable<T&>` / `<T*>` sans `remove_cv` | KILLED × 2 |
| `M48` | `is_sendable<T[]> : std::true_type` | KILLED |
| `M13` | `is_sendable<T[N]> : is_sendable<T>` (garde le cv) | KILLED |
| `M35` | marche des membres sans `remove_cv` | KILLED |
| `M50` `M41` | `is_synchronizable<T[]>` vraie / supprimée | KILLED × 2 |
| `M24` `M25` | boucle `bases_of` de la marche const, altérée / supprimée | KILLED × 2 |
| `M08` | `type = remove_cv(type)` supprimé | KILLED |
| `M06` | règle `is_synchronizable<const T[]>` supprimée | KILLED |
| `M38` | la boucle des membres retire le cv du membre | KILLED |
| `U13` `U15` | `is_move_constructor` / `is_move_assignment` retirés | KILLED × 2 |
| `L13` | garde `borrowed_range` supprimée | KILLED |
| `L18` `L20` `L25` | rejet `void` / garde `has_unreflectable_state` / bloc de report du cv, supprimés | KILLED × 3 |
| `L24` `L27` `L10` | cv gardé par la marche des membres / le tableau borné / la règle `shared_ptr` | KILLED × 3 |
| `VOC-01` … `VOC-14` | les huit spécialisations de `vocabulary.h` | KILLED × 8 |
| `ASW-01` | court-circuit `is_synchronizable` du wrapper `std` supprimé | KILLED |
| `ASW-LIST-array` | `std::array` retiré de la liste blanche | KILLED |
| `SP-05` | `is_lifetime_aware<unique_ptr<T,D>>` cesse d'interroger `D` | KILLED |

Coût : 79 `static_assert`, **750 ms** en `-fsyntax-only` (meilleur de 3), à comparer aux 710 ms de
`test_soundness_regressions.cpp` mesurés dans les mêmes conditions sur la même machine.

### 4.2 `tests/test_diagnostic_messages.cpp` — 25 survivants

Le fichier qui invalide la phrase de `test_diagnostics.cpp`. Il est **le** livrable de ce rapport :
un tiers des survivants, et le seul qui prouve que les messages que la bibliothèque met tant de soin
à composer sont effectivement ceux qu'elle compose.

```cpp
// tests/test_diagnostic_messages.cpp
//
// The half of the traits the suite declares untestable. It is not: assert_*
// does not *emit* a diagnostic, it THROWS a std::meta::exception, and catching
// one inside a consteval function is exactly what default_is_sendable already
// does to turn a rejection into `false`. The message is therefore a
// compile-time value, and a static_assert can hold it to its contract.
#include <threadsafe/threadsafe.h>

#include <meta>
#include <ranges>
#include <span>
#include <string_view>

namespace diagnostic_fixtures {

template <class Probe>
consteval bool rejection_mentions(Probe probe, std::u8string_view expected) {
    try {
        probe();
    } catch (const std::meta::exception& rejection) {
        return rejection.u8what().find(expected) != std::u8string_view::npos;
    }
    return false;
}

struct Borrowing {
    int* borrowed;
};
struct BorrowingMiddle {
    Borrowing inner;
};
struct BorrowingOuter {
    BorrowingMiddle middle;
};
struct DerivesFromBorrowing : Borrowing {};
struct MutableCache {
    int raw;
    mutable int parsed;
};
struct MutableInt {
    mutable int value;
};
struct HoldsArrayOfMutableInt {
    MutableInt cells[4];
};
struct Plain {
    int value;
};
struct UserCopyCtor {
    UserCopyCtor(const UserCopyCtor&);
};
class NeverDefined;

// Sendable by decree, although the structural walk would refuse it, and never
// synchronizable: the only shape that tells the two questions apart inside the
// array branch of the sendable walk.
struct VouchedSendable {
    VouchedSendable(const VouchedSendable&);
};

// A borrowed range that also happens to hold a raw pointer. The guard that
// fires first is what the message reports, and the borrowed_range guard must be
// the one: the pointer member is an implementation detail of how it borrows.
struct BadBorrowed {
    int* p;
    int* begin() const;
    int* end() const;
};

// A GCC vector type is neither scalar, nor class, nor union, nor array, nor
// void: the one kind of type that reaches the unsupported-kind guard.
typedef int vector_of_four_ints __attribute__((vector_size(16)));

}

template <>
inline constexpr bool std::ranges::enable_borrowed_range<
    diagnostic_fixtures::BadBorrowed> = true;

template <>
struct threadsafe::is_sendable<diagnostic_fixtures::VouchedSendable>
    : std::true_type {};
template <>
struct threadsafe::is_sendable<diagnostic_fixtures::VouchedSendable[4]>
    : std::false_type {};

// A function type withdrawn from the function-type rule: the only way to make
// the sendable walk reach a type that is neither scalar, class, union, array,
// pointer, reference, void nor synchronizable.
template <>
struct threadsafe::is_synchronizable<void()> : std::false_type {};

// A function pointer points at code, which has static storage duration, so the
// lifetime walk must never lump it in with object pointers. The only way to
// reach the walk with one is to opt out explicitly.
template <>
struct threadsafe::is_lifetime_aware<void (*)()> : std::false_type {};

using diagnostic_fixtures::rejection_mentions;
namespace fixtures = diagnostic_fixtures;

// ---------------------------------------------------------------------------
// assert_sendable — every reject() of the sendable walk, in walk order.
// ---------------------------------------------------------------------------
static_assert(rejection_mentions([] { threadsafe::assert_sendable<int*>(); },
                                 u8"is a pointer or a reference"),
              "assert_sendable — a raw pointer is refused as a pointer, and "
              "the message says so");
static_assert(rejection_mentions([] { threadsafe::assert_sendable<int&>(); },
                                 u8"is a pointer or a reference"),
              "assert_sendable — and a reference by the same rule");
static_assert(rejection_mentions([] { threadsafe::assert_sendable<int*[4]>(); },
                                 u8"is a pointer or a reference"),
              "assert_sendable — the walk enters an array and names the "
              "element's own reason, not a generic one");
static_assert(
    rejection_mentions([] { threadsafe::assert_sendable<fixtures::UserCopyCtor[4]>(); },
                       u8"has a user-written copy, move or destructor"),
    "assert_sendable — and it does so for a structurally rejected element too");
static_assert(rejection_mentions([] { threadsafe::assert_sendable<void>(); },
                                 u8"holds no value to send"),
              "assert_sendable — void is refused for having no value, before "
              "the class-or-union guard gets to call it an unsupported kind");
static_assert(rejection_mentions([] { threadsafe::assert_sendable<void()>(); },
                                 u8"is not a scalar, class or union type"),
              "assert_sendable — a type kind the trait does not support is "
              "named as such, instead of leaking reflection's own error");
static_assert(
    rejection_mentions([] { threadsafe::assert_sendable<fixtures::NeverDefined>(); },
                       u8"is incomplete"),
    "assert_sendable — an incomplete type is named as incomplete, not left to "
    "reflection's own internal error");
static_assert(
    rejection_mentions([] { threadsafe::assert_sendable<fixtures::NeverDefined>(); },
                       u8"pimpl idiom"),
    "assert_sendable — with the advice that goes with it");
static_assert(
    rejection_mentions([] { threadsafe::assert_sendable<fixtures::VouchedSendable[4]>(); },
                       u8"is_sendable is specialized to false for it"),
    "assert_sendable — the array branch asks is_sendable of the element, not "
    "is_synchronizable: the element is sendable, so the array's own false "
    "answer is the reason");

// The deep walk: the message names the root cause and every hop taken to reach
// it, and a base is named as a base.
static_assert(
    rejection_mentions([] { threadsafe::assert_sendable<fixtures::BorrowingOuter>(); },
                       u8"::middle (diagnostic_fixtures::BorrowingMiddle)"
                       u8"::inner (diagnostic_fixtures::Borrowing)"
                       u8"::borrowed (int*) is a pointer or a reference"),
    "assert_sendable — the walk descends to the root cause and spells every "
    "step it took");
static_assert(
    rejection_mentions([] { threadsafe::assert_sendable<fixtures::DerivesFromBorrowing>(); },
                       u8"::(base diagnostic_fixtures::Borrowing)::borrowed (int*)"),
    "path_step — a base is named as a base in the path");

// describe() words the subject of a PATH-LESS rejection — the message the trait
// itself builds on every "false" answer, unreachable from the public API.
static_assert(
    rejection_mentions(
        [] {
            threadsafe::detail::diagnose_default_is_sendable(
                ^^fixtures::DerivesFromBorrowing);
        },
        u8"base class diagnostic_fixtures::Borrowing is not sendable"),
    "describe — a base subject opens the sentence as a base class");
static_assert(
    rejection_mentions(
        [] {
            threadsafe::detail::diagnose_default_is_sendable(^^fixtures::Borrowing);
        },
        u8"member `borrowed` of type int* is not sendable"),
    "describe / member_name — a member subject is spelled by its identifier, "
    "and the path-less form is what the trait itself builds on every false");

// ---------------------------------------------------------------------------
// assert_synchronizable — the const walk's own rejects.
// ---------------------------------------------------------------------------
static_assert(rejection_mentions([] { threadsafe::assert_synchronizable<int>(); },
                                 u8"is opt-in"),
              "assert_synchronizable — a non-const T gets the opt-in guidance");
static_assert(
    rejection_mentions([] { threadsafe::assert_synchronizable<fixtures::Plain>(); },
                       u8"is opt-in"),
    "assert_synchronizable — whatever the type holds");
static_assert(
    !rejection_mentions([] { threadsafe::assert_synchronizable<fixtures::Plain>(); },
                        u8"is specialized to false"),
    "assert_synchronizable — the walk never blames a specialization for the "
    "opt-in default");
static_assert(
    rejection_mentions([] { threadsafe::assert_synchronizable<const void>(); },
                       u8"holds no value to read"),
    "assert_synchronizable — void is rejected on its own terms");
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::vector_of_four_ints>(); },
        u8"is not a scalar, class or union type"),
    "assert_synchronizable — a type no rule covers is rejected before "
    "members_of is asked about it");
static_assert(
    !rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::vector_of_four_ints>(); },
        u8"neither complete class type nor namespace"),
    "assert_synchronizable — reflection's own exception is never the library's "
    "answer");
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::NeverDefined>(); },
        u8"is incomplete"),
    "assert_synchronizable — an incomplete type is named as such");
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::NeverDefined>(); },
        u8"pimpl idiom"),
    "assert_synchronizable — with the advice that goes with it");
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::MutableCache>(); },
        u8"::parsed (int) is mutable, so it is written through a const reference"),
    "reject_at — a terminal subobject is appended to the path");

// The array branch of the const walk is reached only while a path is being
// built, so only assert_synchronizable can observe it.
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::MutableInt[4]>(); },
        u8"is mutable"),
    "assert_synchronizable — the walk descends into an array element and names "
    "the member responsible");
static_assert(
    !rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::MutableInt[4]>(); },
        u8"is specialized to false"),
    "assert_synchronizable — an element the walk can explain is never blamed "
    "on a specialization it cannot read");
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_synchronizable<const fixtures::HoldsArrayOfMutableInt>(); },
        u8"is mutable"),
    "assert_synchronizable — and it descends through an array member of a "
    "class too");

// ---------------------------------------------------------------------------
// assert_lifetime_aware — the third walk, and the guards only it has.
// ---------------------------------------------------------------------------
static_assert(
    rejection_mentions([] { threadsafe::assert_lifetime_aware<fixtures::Borrowing>(); },
                       u8"diagnostic_fixtures::Borrowing::borrowed (int*) is a "
                       u8"reference or a raw pointer"),
    "reject — the path opens the sentence when there is one");
static_assert(
    rejection_mentions([] { threadsafe::assert_lifetime_aware<fixtures::BorrowingOuter>(); },
                       u8"::middle (diagnostic_fixtures::BorrowingMiddle)"
                       u8"::inner (diagnostic_fixtures::Borrowing)"
                       u8"::borrowed (int*) is a reference or a raw pointer"),
    "assert_lifetime_aware — the walk descends to the root cause and spells "
    "every step it took");
static_assert(
    rejection_mentions(
        [] { threadsafe::assert_lifetime_aware<fixtures::DerivesFromBorrowing>(); },
        u8"::(base diagnostic_fixtures::Borrowing)::borrowed (int*)"),
    "path_step — a base is named as a base here too");
static_assert(
    rejection_mentions(
        [] {
            threadsafe::detail::diagnose_default_is_lifetime_aware(
                ^^fixtures::DerivesFromBorrowing);
        },
        u8"base class diagnostic_fixtures::Borrowing is not lifetime aware"),
    "describe — a base subject opens the sentence as a base class");
static_assert(
    rejection_mentions(
        [] {
            threadsafe::detail::diagnose_default_is_lifetime_aware(^^fixtures::Borrowing);
        },
        u8"member `borrowed` of type int*"),
    "member_name — including in the path-less form describe() words");
static_assert(!threadsafe::is_lifetime_aware_v<fixtures::BadBorrowed>,
              "premise: the type is refused");
static_assert(
    rejection_mentions([] { threadsafe::assert_lifetime_aware<fixtures::BadBorrowed>(); },
                       u8"BadBorrowed is a borrowed range"),
    "assert_lifetime_aware — a borrowed range is reported as one, before the "
    "member walk gets to blame an incidental pointer member");
static_assert(
    rejection_mentions([] { threadsafe::assert_lifetime_aware<std::span<int>[4]>(); },
                       u8"is a borrowed range"),
    "assert_lifetime_aware — an array is explained through its element type");
static_assert(
    rejection_mentions([] { threadsafe::assert_lifetime_aware<void (*)()>(); },
                       u8"is_lifetime_aware is specialized to false for it"),
    "assert_lifetime_aware — a function pointer is refused only because the "
    "user said so, never for being a raw pointer");
```

Trois remarques honnêtes sur ce fichier.

1. **Une des assertions du test tueur d'origine était fausse et je l'ai corrigée.** La forme sans
   chemin de `describe()` sur un membre produit

   ```
   member `borrowed` of type int* is not sendable
   ```

   et non

   ```
   member `borrowed` of type int* is a pointer or a reference
   ```

   Le trait ne descend pas quand le chemin est vide : c'est toute la raison d'être de
   `explain_sendable`. J'ai instrumenté le message pour le lire (programme ci-dessous) avant de
   corriger l'attente.

   ```cpp
   #include <threadsafe/threadsafe.h>

   #include <array>
   #include <cstddef>
   #include <cstdio>
   #include <meta>
   #include <string>

   namespace fx {
   struct Borrowing { int* borrowed; };
   }

   template <class Probe>
   consteval std::u8string reason_of(Probe probe) {
       try {
           probe();
       } catch (const std::meta::exception& thrown) {
           return std::u8string(thrown.u8what());
       }
       return u8"<accepted>";
   }

   template <auto Probe>
   struct text {
       static constexpr std::size_t size = reason_of(Probe).size();
       static constexpr std::array<char, size + 1> value = [] {
           std::array<char, size + 1> out{};
           const std::u8string reason = reason_of(Probe);
           for (std::size_t index = 0; index < size; ++index)
               out[index] = static_cast<char>(reason[index]);
           return out;
       }();
   };

   constexpr auto sendable_probe = [] {
       threadsafe::detail::diagnose_default_is_sendable(^^fx::Borrowing);
   };
   constexpr auto lifetime_probe = [] {
       threadsafe::detail::diagnose_default_is_lifetime_aware(^^fx::Borrowing);
   };

   int main() {
       std::printf("sendable : %s\n", text<sendable_probe>::value.data());
       std::printf("lifetime : %s\n", text<lifetime_probe>::value.data());
   }
   ```

   Sortie :

   ```
   sendable : member `borrowed` of type int* is not sendable
   lifetime : member `borrowed` of type int* is not lifetime aware
   ```

   Ce petit programme mérite d'être gardé quelque part : c'est le seul moyen de *voir* un message de
   la bibliothèque sans provoquer une erreur de compilation, et il ferait une excellente démo.

2. **Le fichier est fragile aux noms.** Les attentes contiennent `diagnostic_fixtures::Borrowing` :
   renommer l'espace de noms casse le test. C'est le prix à payer pour vérifier qu'un chemin nomme
   bien ce que l'utilisateur a écrit — et c'est précisément ce que `U31` (`member_name` renvoie
   toujours `<unnamed>`) et `U25`/`U26` (les branches « classe de base ») demandent de vérifier. Un
   test de message qui ne regarderait que des sous-chaînes génériques ne tuerait ni `U25`, ni `U26`,
   ni `U31`.

3. **Il est le plus lent de la suite** : **1040 ms** en `-fsyntax-only`, soit +47 % sur la TU la plus
   coûteuse d'aujourd'hui. La raison est structurelle et non accidentelle : chaque assertion de
   message amorce un chemin, donc déclenche la marche *profonde* — celle que le trait, lui, ne paie
   jamais. C'est un coût qu'on choisit, pas un défaut.

Vérification, mutation par mutation — **les 25 ont été réappliquées individuellement** :

| header | mutants tués |
|---|---|
| `sendable.h` | `M19` (rejet pointeur/référence supprimé), `M20` (références seules), `M21` (branche tableau supprimée), `M23` (la branche tableau demande `is_synchronizable`), `M49` (`path.empty()` inversé), `M30` (rejet « incomplet » supprimé), `M28` (rejet `void` supprimé), `M51` (rejet `void` déplacé après le rejet classe/union), `M29` (rejet classe/union supprimé) |
| `synchronizable.h` | `M15` (branche tableau qui ne vérifie rien), `M14` (branche tableau qui demande `is_sendable`), `M19` (rejet classe/union supprimé), `M20` (rejet « incomplet » supprimé), `M39` (garde « opt-in » d'`assert_synchronizable` supprimée), `M18` (rejet `void` supprimé) |
| `utils.h` | `U25` (`describe` sans branche base), `U26` (`path_step` sans branche base), `U27` (`path_step` sans branche membre), `U28` (`reject` ignore le chemin), `U29` (`reject_at` n'ajoute pas son pas), `U31` (`member_name` toujours `<unnamed>`) |
| `lifetime_aware.h` | `L29` (`explain_lifetime_aware` ne descend plus), `L15` (garde `borrowed_range` déplacée après la marche), `L16` (branche tableau supprimée), `L26` (les pointeurs de fonction rejetés comme des pointeurs d'objet) |

Note de méthode : ma première reconstruction de `L15` plaçait la garde **avant** la boucle des
membres au lieu d'**après**, et le test survivait — à juste titre, puisque la garde tirait encore la
première. Avec la mutation exacte du rapport (garde déplacée *après* la boucle), le test tue. Je le
signale parce que c'est le genre d'écart qui transforme un « le test ne marche pas » en « la mutation
n'était pas celle que je croyais ».

### 4.3 `tests/test_helper_contracts.cpp` — 7 survivants

Des contrats de helpers qu'un `static_assert` *peut* atteindre mais que la suite n'énonce pas : les
accesseurs rvalue supprimés de `value_guard`, son constructeur privé, le `mutable` sur le mutex, la
garde du constructeur de `copy_on_write`, et le fait que `launch_task` prend son appelable et ses
arguments **par valeur**. Les trois derniers ont besoin d'un corps ou d'un site d'appel réel, pas
d'une `requires`-expression — d'où les fonctions que la TU instancie.

```cpp
// tests/test_helper_contracts.cpp
//
// Helper contracts that a static_assert CAN reach but the suite never states:
// the deleted rvalue accessors of value_guard, its private constructor, the
// `mutable` on the mutex, copy_on_write's constructor guard, and the fact that
// launch_task takes its callable and arguments BY VALUE. The last three need a
// real body or a real call site, not a requires-expression — so they are
// written as functions the TU instantiates.
#include <threadsafe/threadsafe.h>

#include <memory>
#include <shared_mutex>
#include <type_traits>
#include <utility>

namespace helper_contracts {

using sync_int = threadsafe::synchronized_value<int>;

// The requires-expression must go through a dependent template parameter:
// `requires(sync_int& v) { *v.lock(); }` written on the concrete type is a HARD
// error under GCC 16, not an unsatisfied requirement.
template <class Guard>
concept rvalue_derefable = requires(Guard guard) { *static_cast<Guard&&>(guard); };

template <class Guard>
concept rvalue_arrowable =
    requires(Guard guard) { static_cast<Guard&&>(guard).operator->(); };

// Not a static_assert: lock_shared()'s *body* is what needs the mutex to be
// mutable, and a requires-expression only checks the declaration. Defining a
// function that really calls it is what instantiates the body.
inline int read_through_const(const sync_int& value) {
    const auto shared_guard = value.lock_shared();
    return *shared_guard;
}

// A T that swallows anything: without copy_on_write's constructor guard the
// variadic constructor is an exact match for a non-const copy_on_write lvalue
// and outranks the copy constructor, so `cow<Sink> b(a)` wraps `a` in a new
// block instead of sharing its block — the type stops being a copy-on-write.
struct Sink {
    Sink() = default;
    template <class U>
    Sink(U&&) {}
};

using cow_sink = threadsafe::copy_on_write<Sink>;

// launch_task takes its callable and its arguments BY VALUE: owning them is
// what lets the launcher hand them to a thread. A by-reference parameter would
// both stop it owning them and reject every lvalue call site. The suite today
// never calls launch_task at all.
inline void launch_from_lvalues(threadsafe::asynchronous_task_launcher& launcher) {
    auto task = [](int, int) {};
    int first = 1;
    int second = 2;
    launcher.launch_task(task, first, second);
}

// Sendable and lifetime-aware, but move-only: the launcher owns its callable,
// so it must MOVE it onto the thread, never copy it.
struct MoveOnlyTask {
    std::unique_ptr<int> owned;
    void operator()() const {}
};

inline void launch_move_only(threadsafe::asynchronous_task_launcher& launcher) {
    launcher.launch_task(MoveOnlyTask{std::make_unique<int>(1)});
    launcher.launch_task([](std::unique_ptr<int>) {}, std::make_unique<int>(2));
}

}

static_assert(!helper_contracts::rvalue_derefable<helper_contracts::sync_int::guard>,
              "operator* — a temporary guard is destroyed at the semicolon, so "
              "it must not hand out a reference that outlives its lock");
static_assert(!helper_contracts::rvalue_derefable<helper_contracts::sync_int::const_guard>,
              "operator* — same for the shared guard");
static_assert(!helper_contracts::rvalue_arrowable<helper_contracts::sync_int::guard>,
              "operator-> — nor a pointer that outlives its lock");
static_assert(!helper_contracts::rvalue_arrowable<helper_contracts::sync_int::const_guard>,
              "operator-> — same for the shared guard");

static_assert(!std::is_constructible_v<helper_contracts::sync_int::guard,
                                       std::shared_mutex&, int&>,
              "value_guard — only synchronized_value may build a guard; a "
              "public constructor lets any caller pair an arbitrary mutex with "
              "an arbitrary object and claim it is protected");
static_assert(!std::is_constructible_v<helper_contracts::sync_int::const_guard,
                                       std::shared_mutex&, const int&>,
              "value_guard — same for the shared guard");

static_assert(std::is_nothrow_constructible_v<helper_contracts::cow_sink,
                                              helper_contracts::cow_sink&>,
              "copy_on_write — the variadic constructor must not outrank the "
              "copy constructor on a non-const lvalue: the copy constructor "
              "only copies a shared_ptr and is noexcept, the variadic one "
              "allocates");
static_assert(std::is_nothrow_constructible_v<helper_contracts::cow_sink,
                                              const helper_contracts::cow_sink&>,
              "copy_on_write — and the same on a const lvalue");

static_assert(threadsafe::launchable_task<helper_contracts::MoveOnlyTask>,
              "a move-only callable is launchable");
static_assert(&helper_contracts::read_through_const != nullptr,
              "lock_shared() const must be instantiable on a const wrapper");
static_assert(&helper_contracts::launch_from_lvalues != nullptr,
              "launch_task must accept lvalue callables and arguments");
static_assert(&helper_contracts::launch_move_only != nullptr,
              "launch_task must move its callable and arguments, not copy them");
```

Le fichier s'ajoute à la liste de sources de `threadsafe_tests` dans `tests/CMakeLists.txt`.

| mutant | mutation appliquée | résultat |
|---|---|---|
| `SV-07` | `operator*() &&` dé-supprimé | KILLED |
| `SV-08` | `operator->() &&` dé-supprimé | KILLED |
| `SV-19` | constructeur de `value_guard` rendu public | KILLED |
| `SV-12` | `mutable` retiré de `mutex_` | KILLED (erreur dure dans le corps de `lock_shared`) |
| `COW-14` | garde `!same_as<remove_cvref_t<Args>, copy_on_write>` retirée | KILLED |
| `ATL-12` | `launch_task(F&& f, Args&&... args)` | KILLED |
| `ATL-13` | `emplace_back(f, args...)` au lieu de `std::move` | KILLED (erreur dure dans `std_thread.h`) |

Deux remarques :

- La `requires`-expression sur `value_guard` **doit** passer par un paramètre de modèle dépendant.
  Écrite directement sur le type concret — `requires(sync_int& v) { *v.lock(); }` — c'est une erreur
  **dure** sous GCC 16, pas une exigence non satisfaite, et le fichier ne compile pas du tout. C'est
  un piège qu'il vaut la peine de documenter, parce que la version naïve *semble* marcher.
- `SV-12`, `ATL-12` et `ATL-13` ne sont pas des `static_assert` : leur défaut est dans une
  *signature* ou dans un *corps*, donc seul un site d'appel réel le voit. La suite d'aujourd'hui
  n'appelle jamais `launch_task`. Coût mesuré : **660 ms**, la TU la moins chère des trois.

### 4.4 `tests/rejections/` — 3 survivants qu'aucun `static_assert` ne peut atteindre

Un `static_assert` ne sait dire qu'une chose : « ceci compile ». Il ne peut pas tester un
`static_assert` situé *dans* la bibliothèque, ni un `[[nodiscard]]`. Pour ces trois-là il faut une
TU dont le contrat est de **ne pas compiler**.

**`tests/rejections/non_sendable_synchronized_value.cpp`** (tue `SV-05` et `SV-06`) :

```cpp
// tests/rejections/non_sendable_synchronized_value.cpp
// MUST NOT COMPILE. The static_assert inside synchronized_value is the only
// thing stopping a T that cannot cross a thread boundary from being wrapped in
// a mutex and crossing anyway — and no static_assert elsewhere can observe it,
// because naming synchronized_value<T> in a trait query never instantiates the
// class.
#include <threadsafe/threadsafe.h>

namespace {
struct NonSendable {
    NonSendable() = default;
    NonSendable(const NonSendable&) {}
};
}

threadsafe::synchronized_value<NonSendable> wrapped{};
```

**`tests/rejections/discarded_exclusive_guard.cpp`** :

```cpp
// tests/rejections/discarded_exclusive_guard.cpp
// MUST NOT COMPILE, built with -Werror=unused-result. A discarded guard is a
// temporary destroyed at the semicolon: a lock taken and released again with
// nothing done under it. [[nodiscard]] on lock() is what says so.
#include <threadsafe/threadsafe.h>

namespace {
using sync_int = threadsafe::synchronized_value<int>;
}

void discard_the_exclusive_lock(sync_int& value) { value.lock(); }
```

**`tests/rejections/discarded_shared_guard.cpp`** :

```cpp
// tests/rejections/discarded_shared_guard.cpp
// MUST NOT COMPILE, built with -Werror=unused-result. Same contract for the
// shared guard handed out by lock_shared(). Two separate TUs are required: with
// only one, dropping [[nodiscard]] from lock() alone would still be caught by
// the lock_shared() line.
#include <threadsafe/threadsafe.h>

namespace {
using sync_int = threadsafe::synchronized_value<int>;
}

void discard_the_shared_lock(const sync_int& value) { value.lock_shared(); }
```

Le bloc CMake, complet, à ajouter à `tests/CMakeLists.txt` :

```cmake
# Compile-time REJECTIONS. A static_assert can only say "this compiles"; these
# say "this must NOT compile", which is the only way to test a static_assert
# inside the library or a [[nodiscard]] on one of its members.
function(threadsafe_add_rejection_test name)
    add_library(reject_${name} OBJECT EXCLUDE_FROM_ALL rejections/${name}.cpp)
    target_link_libraries(reject_${name} PRIVATE ThreadSafe::threadsafe)
    target_compile_options(reject_${name} PRIVATE ${ARGN})
    add_test(NAME reject_${name}
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target reject_${name} --config $<CONFIG>)
    set_tests_properties(reject_${name} PROPERTIES WILL_FAIL TRUE)
endfunction()

threadsafe_add_rejection_test(non_sendable_synchronized_value)
threadsafe_add_rejection_test(discarded_exclusive_guard -Werror=unused-result)
threadsafe_add_rejection_test(discarded_shared_guard    -Werror=unused-result)
```

Vérifié via `ctest` sur une copie du dépôt :

```
2/4 Test #2: reject_non_sendable_synchronized_value ...   Passed    0.73 sec
3/4 Test #3: reject_discarded_exclusive_guard .........   Passed    0.74 sec
4/4 Test #4: reject_discarded_shared_guard ............   Passed    0.74 sec
```

et sur les mutants :

| mutant | `non_sendable_…` | `discarded_exclusive_…` | `discarded_shared_…` |
|---|---|---|---|
| original | refusé ✔ | refusé ✔ | refusé ✔ |
| `SV-05` `static_assert(sendable<T>)` supprimé | **compile ✘** | — | — |
| `SV-06` `static_assert(lifetime_aware<T>)` à la place | **compile ✘** | — | — |
| `SV-11` `[[nodiscard]]` retiré de `lock()` | — | **compile ✘** | refusé ✔ |
| `SV-11b` `[[nodiscard]]` retiré de `lock_shared()` | — | refusé ✔ | **compile ✘** |

Les deux dernières lignes justifient les deux TU séparées : avec une seule, retirer le
`[[nodiscard]]` d'une des deux fonctions resterait masqué par l'autre. C'est le genre de détail
qu'une campagne de mutation trouve et qu'une relecture ne trouve pas.

### 4.5 `tests/test_runtime_cow_fence_race.cpp` — 3 survivants, vérification **plus faible**

Les trois derniers survivants sont les mutations de la barrière mémoire de `as_mutable` :
suppression du `std::atomic_thread_fence(std::memory_order_acquire)` (`COW-05`), affaiblissement en
`relaxed` (`COW-06`), et passage en `release` — le mauvais côté de l'appariement (`COW-07`).

Ce que la barrière fait, en une phrase : libstdc++ lit `use_count()` avec un `load` **relaxed** ;
deux poignées sur un bloc, deux threads qui se détachent au même instant, le gagnant lit le bloc pour
en faire sa copie puis lâche sa poignée avec un décrément *release* ; le `load` relaxed du perdant
peut alors voir 1 et écrire **dans ce même bloc, sur place**. Seule une barrière *acquire* séquencée
après le `load` relaxed fait que le décrément release du gagnant se synchronise-avec
(`[atomics.fences]/3`), donc que sa lecture précède l'écriture du perdant.

Le fichier, complet :

```cpp
// tests/test_runtime_cow_fence_race.cpp
// A ThreadSanitizer target, not a self-checking test: it exercises the one
// window the acquire fence in copy_on_write::as_mutable exists to close.
// Build with -fsanitize=thread; a release or relaxed fence does not satisfy
// [atomics.fences]/3 -- only an acquire (or seq_cst) fence does.
#include <threadsafe/threadsafe.h>

#include <array>
#include <barrier>
#include <cstdio>
#include <functional>
#include <thread>

namespace {

struct Payload {
    std::array<unsigned long long, 512> words{};
};

void detach_and_overwrite(std::barrier<>& start,
                          threadsafe::copy_on_write<Payload>& handle,
                          unsigned long long tag) {
    start.arrive_and_wait();
    Payload& mine = handle.as_mutable();
    for (unsigned long long& word : mine.words)
        word = tag;
}

}

int main() {
    constexpr int rounds = 200000;
    for (int round = 0; round < rounds; ++round) {
        threadsafe::copy_on_write<Payload> first{};
        threadsafe::copy_on_write<Payload> second = first;

        std::barrier start{2};
        std::jthread contender{detach_and_overwrite, std::ref(start),
                               std::ref(second), 2ull};
        detach_and_overwrite(start, first, 1ull);
    }
    std::printf("no race reported\n");
    return 0;
}
```

Le bloc CMake, complet — la cible est optionnelle parce qu'elle échoue en *rapportant* une course,
pas en renvoyant un code de sortie :

```cmake
# ThreadSanitizer target for copy_on_write's acquire fence. Not self-checking:
# it fails by TSan reporting a race, so it is only registered where TSan works.
option(THREADSAFE_BUILD_TSAN_TESTS "Build the ThreadSanitizer race targets" OFF)
if(THREADSAFE_BUILD_TSAN_TESTS)
    add_executable(threadsafe_cow_fence_race test_runtime_cow_fence_race.cpp)
    target_compile_options(threadsafe_cow_fence_race PRIVATE -fsanitize=thread -O2)
    target_link_options(threadsafe_cow_fence_race PRIVATE -fsanitize=thread)
    target_link_libraries(threadsafe_cow_fence_race
        PRIVATE ThreadSafe::threadsafe Threads::Threads)
    add_test(NAME cow_fence_race COMMAND threadsafe_cow_fence_race)
    set_tests_properties(cow_fence_race PROPERTIES
        ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1"
        FAIL_REGULAR_EXPRESSION "data race")
endif()
```

**Statut honnête.** Je n'ai pas pu vérifier le kill. `g++-16` sur macOS/arm64 ne lie pas le runtime
TSan :

```
$ printf 'int main(){}\n' > tsan.cpp && g++-16 -fsanitize=thread tsan.cpp -o tsanbin
Undefined symbols for architecture arm64:
  "___tsan_init", referenced from:
      __sub_I_00099_0 in cc2ABYN8.o
ld: symbol(s) not found for architecture arm64
```

Ce que j'ai pu vérifier moi-même, et qui est une preuve de non-équivalence solide pour deux des
trois, c'est le code généré :

```
$ g++-16 -std=c++26 -freflection -O2 -S fence_probe.cpp -o - | grep dmb
```

| version | barrières émises (aarch64) |
|---|---|
| original (`acquire`) | `dmb ishld` × 1 |
| `COW-05` barrière supprimée | *aucune* |
| `COW-06` `relaxed` | *aucune* |
| `COW-07` `release` | `dmb ishld` + `dmb ishst` |

`COW-05` et `COW-06` sont donc **prouvés non équivalents** : ils suppriment purement et simplement
la barrière. `COW-07`, en revanche, est très probablement **équivalent sur aarch64** — GCC y émet un
`dmb ishld` pour une barrière release, ce qui couvre incidemment le côté acquire. Il ne serait
distinguable que sur x86-64, où une barrière release ne produit aucune instruction. Il faut donc
lancer cette cible sur Linux/x86-64 avec GCC 16 pour compléter la vérification.

Ma recommandation, en appliquant « challenge the need » : **ajoutez le fichier, laissez l'option à
`OFF` par défaut, et n'en faites pas une porte de CI tant qu'il n'a pas tourné une fois sur
x86-64.** Un test qui n'a jamais tourné vert quelque part est un passif, pas un actif.

---

## 5. Ce que ça coûte

Mesures faites par moi, sur cette machine (GCC 16.2.0, macOS/arm64) ; elle n'était pas parfaitement
au repos, donc les chiffres absolus sont ~8 % au-dessus de ceux du lead (j'obtiens 710 ms pour
`test_soundness_regressions.cpp` là où le lead mesure 655 ms). Les **rapports** restent valables.

| TU | `-fsyntax-only`, meilleur de 3 |
|---|---:|
| `test_diagnostics.cpp` (existant, le plus léger) | 690 ms |
| `test_soundness_regressions.cpp` (existant, le plus lourd) | 710 ms |
| **`test_helper_contracts.cpp`** (nouveau) | **660 ms** |
| **`test_runtime_helpers.cpp`** (nouveau) | **730 ms** |
| **`test_trait_rules.cpp`** (nouveau) | **750 ms** |
| **`test_diagnostic_messages.cpp`** (nouveau) | **1040 ms** |

Build complet, sur une copie du dépôt :

| | temps |
|---|---:|
| `cmake --build build`, suite actuelle (11 TU) | **7,9 s** |
| `cmake --build build`, suite augmentée (14 TU + exécutable d'exécution) | **11,7 s** |
| `ctest` (1 test d'exécution + 3 tests de rejet) | **3,3 s** |
| **total** | **15,0 s** |

Doubler quasiment le temps de la suite pour passer de 68 % à 99 % de score de mutation est un bon
échange, mais il faut le dire tel quel. Deux observations pour le relativiser :

- Le rapport [06](./06-performance-compilation.md) établit qu'une TU **vide** qui inclut seulement
  l'en-tête parapluie coûte déjà 586 ms des ~620 ms d'une TU de test typique. Autrement dit,
  **le coût est l'analyse syntaxique des en-têtes standard, pas les assertions**. Ajouter 128
  `static_assert` coûte des dizaines de millisecondes, pas des centaines ; ce qui coûte, c'est
  d'ajouter des *fichiers*.
- Le seul fichier qui sort du lot, `test_diagnostic_messages.cpp` à 1040 ms, le fait pour une raison
  qu'on peut expliquer : c'est le seul qui amorce des chemins et déclenche donc la marche profonde.
  Si un jour ce coût gêne, la bonne réponse est de le mettre derrière une option CMake, pas de le
  supprimer.

Si l'on voulait limiter le coût, l'ordre de valeur est clair : `test_runtime_helpers.cpp` d'abord
(il ferme la seule catégorie entière que la suite ne peut pas voir), puis
`test_diagnostic_messages.cpp` (25 survivants, un tiers du total, et il corrige une affirmation
fausse dans le dépôt), puis `test_trait_rules.cpp`, puis le reste.

---

## 6. Ce que je ne recommande pas

Le mandat est de contester le besoin plutôt que d'empiler. Voici où je m'arrête.

**Ne cherchez pas à tuer les 260 mutants.** 14 sont prouvés équivalents. Un score de 99 % est
excellent ; viser 100 % pousse à écrire des tests qui décrivent l'implémentation au lieu du contrat.

**Ne transformez pas `test_runtime_cow_fence_race.cpp` en garde de CI** avant qu'il ait tourné une
fois sur Linux/x86-64 (§4.5). Et `COW-07` est probablement équivalent sur aarch64 : le poursuivre
sur cette architecture serait du temps perdu.

**Il y a une limite que *aucun* test de cette famille ne peut fermer**, et elle mérite une phrase
dans `CLAUDE.md` plutôt qu'un test de plus. Le lead l'a établie, TSan à l'appui : une classe dont le
seul état partagé est un `static inline long` incrémenté dans une fonction membre **`const`** passe
`is_sendable`, `is_synchronizable<const T>` et `launchable_task` ; `synchronized_value` choisit donc
`std::shared_mutex` ; et ThreadSanitizer signale une vraie course de données avec les deux threads ne
tenant qu'un `shared_lock`. La réflexion raisonne sur des **déclarations**, jamais sur des **corps de
fonction**. Aucune version de ces traits ne peut fermer ce trou — ce n'est pas un bug de la
bibliothèque, c'est la frontière du modèle. Ni `CLAUDE.md` ni aucun en-tête ne l'énonce
aujourd'hui : **une phrase de documentation vaut mieux ici que n'importe quel test**, et sur une
conférence c'est une diapositive plus intéressante que n'importe laquelle de mes assertions.

**N'ajoutez pas de framework de test.** Les 12 vérifications d'exécution du §2.2 tiennent dans une
fonction `check(bool, const char*)` de six lignes et un compteur. Pour trois classes de helpers, une
dépendance à GoogleTest ou Catch2 serait un coût de compilation et de compréhension sans contrepartie
— et sur un projet dont la valeur pédagogique tient à ce qu'on puisse lire chaque fichier en entier,
ce serait un recul.

**Ne dupliquez pas les assertions existantes.** Les 79 tests tueurs des données brutes se recouvrent
énormément : 69 corps distincts, et six survivants partagent un seul fichier de messages. Après
déduplication, 128 `static_assert` + 12 vérifications d'exécution + 3 TU de rejet couvrent tout. Une
assertion par mutant aurait donné un ratio bruit/signal désastreux.

---

## 7. Récapitulatif : les 79 survivants et le fichier qui les tue

| # | id | sévérité | header | fichier de test | ce que le mutant casse |
|---|----|----------|--------|-----------------|------------------------|
| 1 | `L13-drop-borrowed-range` | high | `lifetime_aware.h` | `test_trait_rules.cpp` | The borrowed_range guard can be deleted outright and no test notices |
| 2 | `U13-cmd-drop-movector` | high | `utils.h` | `test_trait_rules.cpp` | A user-written move constructor no longer blocks is_sendable / is_synchronizable |
| 3 | `U15-cmd-drop-moveassign` | high | `utils.h` | `test_trait_rules.cpp` | A user-written move assignment no longer blocks is_sendable / is_synchronizable |
| 4 | `L20-drop-unreflectable` | high | `lifetime_aware.h` | `test_trait_rules.cpp` | A closure with captures becomes lifetime aware |
| 5 | `L25-drop-cv-forward` | high | `lifetime_aware.h` | `test_trait_rules.cpp` | The cv-forwarding block can be deleted: const shared_ptr / unique_ptr / stop_token stop being lifetime aware |
| 6 | `L18-drop-void-reject` | medium | `lifetime_aware.h` | `test_trait_rules.cpp` | is_lifetime_aware_v<void> answers true |
| 7 | `L29-explain-always-reject` | medium | `lifetime_aware.h` | `test_diagnostic_messages.cpp` | explain_lifetime_aware can stop descending: the whole root-cause walk disappears and no test sees it |
| 8 | `U28-reject-drop-path` | medium | `utils.h` | `test_diagnostic_messages.cpp` | reject() can ignore the path it was handed; every diagnostic collapses to a bare type name |
| 9 | `U27-pathstep-drop-member` | medium | `utils.h` | `test_diagnostic_messages.cpp` | path_step() can lose its data-member branch; paths stop naming any member |
| 10 | `U31-membername-always-unnamed` | medium | `utils.h` | `test_diagnostic_messages.cpp` | member_name() can always answer <unnamed> |
| 11 | `L10-shared-drop-remove-cv` | medium | `lifetime_aware.h` | `test_trait_rules.cpp` | The shared_ptr rule can stop stripping the pointee's cv |
| 12 | `L24-member-keep-cv` | medium | `lifetime_aware.h` | `test_trait_rules.cpp` | The member walk can stop stripping the member's cv |
| 13 | `U26-pathstep-drop-base` | low | `utils.h` | `test_diagnostic_messages.cpp` | path_step() can lose its base-class branch; a base hop reads like a member the user wrote |
| 14 | `U29-rejectat-drop-step` | low | `utils.h` | `test_diagnostic_messages.cpp` | reject_at() can forget to append its own step |
| 15 | `U25-describe-drop-base` | low | `utils.h` | `test_diagnostic_messages.cpp` | describe() can lose its base-class branch — and describe() is unreachable from the public API at all |
| 16 | `L15-borrowed-range-after-walk` | low | `lifetime_aware.h` | `test_diagnostic_messages.cpp` | Moving the borrowed_range reject after the class walk changes only the reason, and nothing checks reasons |
| 17 | `L16-drop-array-branch` | low | `lifetime_aware.h` | `test_diagnostic_messages.cpp` | The array branch of the lifetime walk is dead code as far as the suite is concerned |
| 18 | `L26-reject-fn-pointers` | low | `lifetime_aware.h` | `test_diagnostic_messages.cpp` | The function-pointer carve-out in the diagnose walk can be removed without any test noticing |
| 19 | `L27-array-spec-keep-cv` | low | `lifetime_aware.h` | `test_trait_rules.cpp` | The bounded-array specialization can stop stripping the element's cv |
| 20 | `M24` | critical | `synchronizable.h` | `test_trait_rules.cpp` | Bases loop asks is_sendable of the base instead of is_synchronizable<const Base> |
| 21 | `M25` | critical | `synchronizable.h` | `test_trait_rules.cpp` | The bases loop is deleted outright — no base is ever walked by the const question |
| 22 | `M08` | medium | `synchronizable.h` | `test_trait_rules.cpp` | `type = remove_cv(type);` deleted — a user's own specialization is lost under a cv spelling |
| 23 | `M06` | medium | `synchronizable.h` | `test_trait_rules.cpp` | The is_synchronizable<const T[]> rule is deleted — an unbounded const array becomes ambiguous, not false |
| 24 | `M15` | medium | `synchronizable.h` | `test_diagnostic_messages.cpp` | The array branch of the walk returns without checking the element at all |
| 25 | `M14` | medium | `synchronizable.h` | `test_diagnostic_messages.cpp` | The array branch asks is_sendable of the element instead of is_synchronizable<const Element> |
| 26 | `M19` | medium | `synchronizable.h` | `test_diagnostic_messages.cpp` | The non-class/non-union reject is deleted — reflection's internal exception escapes as the library's answer |
| 27 | `M20` | medium | `synchronizable.h` | `test_diagnostic_messages.cpp` | The is_complete_type reject is deleted — the pimpl guidance is replaced by a libstdc++ internal message |
| 28 | `M39` | low | `synchronizable.h` | `test_diagnostic_messages.cpp` | assert_synchronizable's opt-in guard is deleted — a non-const T is walked instead of explained |
| 29 | `M18` | low | `synchronizable.h` | `test_diagnostic_messages.cpp` | The is_void reject is deleted — void loses its own worded answer |
| 30 | `M38` | low | `synchronizable.h` | `test_trait_rules.cpp` | The member loop drops a member's own cv before asking the trait |
| 31 | `M09` | critical | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T&&> -> std::true_type: every rvalue reference becomes sendable |
| 32 | `M10` | critical | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T&&> -> is_sendable<T>: an rvalue reference answers like the value type |
| 33 | `M12` | critical | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T&&> -> is_synchronizable<const T>: an rvalue reference asks only for read safety |
| 34 | `M11` | high | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T&&> drops remove_cv: const T&& becomes sendable |
| 35 | `M03` | critical | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T*> drops remove_cv: every pointer-to-const becomes sendable |
| 36 | `M07` | critical | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T&> drops remove_cv: const T& becomes sendable |
| 37 | `M48` | high | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T[]> -> std::true_type: any array of unknown bound is sendable |
| 38 | `M50` | high | `synchronizable_base.h` | `test_trait_rules.cpp` | is_synchronizable<T[]> -> std::true_type: every array of unknown bound is synchronizable, so T(*)[] is sendable |
| 39 | `M41` | medium | `synchronizable_base.h` | `test_trait_rules.cpp` | delete the is_synchronizable<T[]> specialization: an array of unknown bound stops following its element |
| 40 | `M13` | medium | `sendable.h` | `test_trait_rules.cpp` | is_sendable<T[N]> does not strip cv from the element: a const array outranks the element's own answer |
| 41 | `M35` | medium | `sendable.h` | `test_trait_rules.cpp` | the member walk does not remove_cv the member type: a const member escapes its type's answer |
| 42 | `M19 (const walk)` | medium | `sendable.h` | `test_diagnostic_messages.cpp` | delete the pointer/reference reject in the walk: assert_sendable stops naming borrows |
| 43 | `M20 (const walk)` | medium | `sendable.h` | `test_diagnostic_messages.cpp` | the pointer/reference reject checks only references: pointers lose their reason |
| 44 | `M21` | medium | `sendable.h` | `test_diagnostic_messages.cpp` | delete the array branch of the walk: an array never gets a reason |
| 45 | `M49` | medium | `sendable.h` | `test_diagnostic_messages.cpp` | explain_sendable inverts path.empty(): the walk stops at the first hop instead of the root cause |
| 46 | `M30` | medium | `sendable.h` | `test_diagnostic_messages.cpp` | delete the incomplete-type reject: reflection's internal error escapes as the diagnostic |
| 47 | `M28` | low | `sendable.h` | `test_diagnostic_messages.cpp` | delete the void reject: void is refused as an unsupported kind instead of an empty one |
| 48 | `M51` | low | `sendable.h` | `test_diagnostic_messages.cpp` | the void reject moved below the class-or-union reject: dead by ordering |
| 49 | `M23` | low | `sendable.h` | `test_diagnostic_messages.cpp` | the array branch asks is_synchronizable of the element instead of is_sendable |
| 50 | `M29` | low | `sendable.h` | `test_diagnostic_messages.cpp` | delete the not-a-class-or-union reject: an unsupported type kind leaks reflection's internal error |
| 51 | `COW-03-never-copy` | critical | `copy_on_write.h` | `test_runtime_helpers.cpp` | as_mutable never detaches: a writer hands out a reference into a shared block |
| 52 | `COW-09-swap-branches` | critical | `copy_on_write.h` | `test_runtime_helpers.cpp` | as_mutable's two branches swapped: copies when unique, aliases when shared |
| 53 | `VOC-01-stop-token-unqualified-sync` | high | `vocabulary.h` | `test_trait_rules.cpp` | is_synchronizable<std::stop_token> made true -- the exact regression commit 79f1e4f fixed, with nothing guarding its return |
| 54 | `VOC-02-stop-source-unqualified-sync` | high | `vocabulary.h` | `test_trait_rules.cpp` | is_synchronizable<std::stop_source> made true -- same regression, other half |
| 55 | `SV-05-drop-sendable-assert` | high | `synchronized_value.h` | `rejections/*.cpp` | synchronized_value's static_assert(sendable<T>) deleted -- no static_assert anywhere can see it |
| 56 | `SV-06-sendable-to-lifetime-assert` | high | `synchronized_value.h` | `rejections/*.cpp` | synchronized_value asserts lifetime_aware<T> instead of sendable<T> |
| 57 | `SV-07-undelete-rvalue-deref` | high | `synchronized_value.h` | `test_helper_contracts.cpp` | value_guard's rvalue operator* un-deleted: `*v.lock()` yields a reference outliving its lock |
| 58 | `SV-08-undelete-rvalue-arrow` | high | `synchronized_value.h` | `test_helper_contracts.cpp` | value_guard's rvalue operator-> un-deleted: `v.lock()->member` touches the value after unlocking |
| 59 | `SP-05-uptr-lifetime-drop-deleter` | high | `smart_pointers.h` | `test_trait_rules.cpp` | is_lifetime_aware<unique_ptr<T,D>> stops asking about the deleter |
| 60 | `ATL-11-scoped-no-join` | high | `asynchronous_task_launcher.h` | `test_runtime_helpers.cpp` | launch_scoped_task's explicit join() removed: ~jthread cancels the task instead |
| 61 | `COW-14-drop-ctor-guard` | high | `copy_on_write.h` | `test_helper_contracts.cpp` | copy_on_write's constructor guard against itself removed: copying a handle wraps it instead of sharing |
| 62 | `COW-05-no-fence` | high | `copy_on_write.h` | `test_runtime_cow_fence_race.cpp` | as_mutable's acquire fence deleted: the "I am unique" conclusion carries no synchronization |
| 63 | `COW-06-fence-relaxed` | high | `copy_on_write.h` | `test_runtime_cow_fence_race.cpp` | as_mutable's fence weakened to memory_order_relaxed -- a no-op fence |
| 64 | `COW-07-fence-release` | medium | `copy_on_write.h` | `test_runtime_cow_fence_race.cpp` | as_mutable's fence changed to memory_order_release -- the wrong side of the pairing |
| 65 | `ASW-01-sendable-drop-sync-shortcut` | medium | `allowed_std_wrappers.h` | `test_trait_rules.cpp` | std_wrapper_is_sendable loses its is_synchronizable short-circuit |
| 66 | `ASW-LIST-array` | medium | `allowed_std_wrappers.h` | `test_trait_rules.cpp` | std::array removed from the allow-list: the only entry whose removal the suite does not detect |
| 67 | `COW-02-usecount-ne0` | medium | `copy_on_write.h` | `test_runtime_helpers.cpp` | as_mutable's uniqueness test becomes `use_count() != 0`: it always copies |
| 68 | `COW-04-always-copy` | medium | `copy_on_write.h` | `test_runtime_helpers.cpp` | as_mutable's uniqueness test replaced by `true`: it always copies |
| 69 | `SV-19-guard-ctor-public` | medium | `synchronized_value.h` | `test_helper_contracts.cpp` | value_guard's private constructor made public: anyone can forge a guard |
| 70 | `SV-11-drop-nodiscard-lock` | medium | `synchronized_value.h` | `rejections/*.cpp` | [[nodiscard]] removed from lock(): a discarded guard is a lock taken and released |
| 71 | `ATL-12-launch-task-by-rvalue-ref` | medium | `asynchronous_task_launcher.h` | `test_helper_contracts.cpp` | launch_task takes F&& / Args&&& instead of by value: the launcher stops owning what it moves from |
| 72 | `ATL-13-copy-not-move` | medium | `asynchronous_task_launcher.h` | `test_helper_contracts.cpp` | launch_task copies its callable and arguments onto the thread instead of moving them |
| 73 | `VOC-04-stop-source-not-sendable` | medium | `vocabulary.h` | `test_trait_rules.cpp` | is_sendable<std::stop_source> flipped to false |
| 74 | `VOC-06-const-stop-source-not-sync` | medium | `vocabulary.h` | `test_trait_rules.cpp` | is_synchronizable<const std::stop_source> flipped to false |
| 75 | `VOC-08-stop-source-not-lifetime` | medium | `vocabulary.h` | `test_trait_rules.cpp` | is_lifetime_aware<std::stop_source> flipped to false |
| 76 | `VOC-10-allocator-sendable-follows-T` | medium | `vocabulary.h` | `test_trait_rules.cpp` | is_sendable<std::allocator<T>> made to follow T instead of being unconditionally true |
| 77 | `SV-12-drop-mutable-mutex` | low | `synchronized_value.h` | `test_helper_contracts.cpp` | `mutable` removed from synchronized_value::mutex_ |
| 78 | `VOC-12-const-allocator-sync-follows-T` | low | `vocabulary.h` | `test_trait_rules.cpp` | is_synchronizable<const std::allocator<T>> made to follow const T |
| 79 | `VOC-14-allocator-lifetime-follows-T` | low | `vocabulary.h` | `test_trait_rules.cpp` | is_lifetime_aware<std::allocator<T>> made to follow T |


*(Les deux entrées `M19` et `M20` apparaissent chacune deux fois : les identifiants de mutant ont été
attribués par groupe, et `sendable.h` comme `synchronizable.h` en ont un. La colonne « header » lève
l'ambiguïté.)*

---

## 8. Résumé opérationnel

| action | effet | vérifié par moi |
|---|---|---|
| ajouter `tests/test_runtime_helpers.cpp` + `enable_testing()` + la cible `threadsafe_runtime_tests` | ferme la seule **catégorie entière** que la suite ne peut pas voir ; 6 survivants | oui — `ctest` vert, 6/6 mutants tués |
| ajouter `tests/test_diagnostic_messages.cpp` | invalide la phrase « *the throwing half is a compile error by design* » ; 25 survivants | oui — 25/25 mutants tués |
| ajouter `tests/test_trait_rules.cpp` | ferme `is_sendable<T&&>`, les formes `T[]`, la boucle `bases_of`, et la régression de `79f1e4f` ; 36 survivants | oui — 36/36 mutants tués |
| ajouter `tests/test_helper_contracts.cpp` | 7 survivants dans `synchronized_value`, `copy_on_write` et le lanceur | oui — 7/7 mutants tués |
| ajouter `tests/rejections/` + `threadsafe_add_rejection_test()` | les 3 contrats qu'un `static_assert` ne peut pas exprimer | oui — via `ctest`, mutants confirmés |
| ajouter `tests/test_runtime_cow_fence_race.cpp`, option **OFF** | 3 survivants de barrière mémoire | **non** — TSan ne se lie pas ici ; `COW-05`/`COW-06` prouvés non équivalents par le code généré, `COW-07` probablement équivalent sur aarch64 |
| écrire dans `CLAUDE.md` que la réflexion ne voit pas les corps de fonction | ferme une attente qu'aucun test ne pourra jamais satisfaire | limite établie par le lead, TSan à l'appui |

Score de mutation : **68 % → 99 %**. Coût : **+3,7 s** de compilation et **+3,3 s** de `ctest`, pour
+128 `static_assert`, +12 vérifications d'exécution et +3 TU de rejet.
