# Audit 12 — Simplicité du code, facilité d'usage de l'API, flexibilité

Compilateur : g++-16 (Homebrew GCC 16.2.0), `-std=c++2c -freflection`.
Tous les scénarios ci-dessous ont été compilés hors du dépôt ; les résultats
indiqués (accepté/rejeté) sont ceux réellement observés.

## Synthèse des verdicts

| # | Sujet | Verdict |
|---|-------|---------|
| 1 | `synchronized_value<T>` inutilisable quand `T` n'est pas const-synchronizable | **PROBLEME** |
| 2 | Réflexion inutile (`get_mutex_type`) dans `synchronized_value` — cause du problème 1 | SUGGESTION |
| 3 | Lambdas capturantes rejetées par `launch_task` — friction majeure non documentée hors tests | SUGGESTION |
| 4 | Aucun diagnostic « pourquoi ce type n'est pas sendable » | SUGGESTION |
| 5 | `containers.h` : ~40 spécialisations quasi identiques (205 lignes) | SUGGESTION |
| 6 | Asymétrie de l'opt-in : macro pour `is_synchronizable` seulement | SUGGESTION |
| 7 | Les headers `details/` inclus isolément changent la valeur des traits (risque ODR) | SUGGESTION |
| 8 | Lisibilité générale, commentaires, noms, localisation de la réflexion | OK |
| 9 | Extension à ses propres types et aux types tiers | OK |

---

## 1. PROBLEME — `synchronized_value<T>` ne compile pas dès que `T` n'est pas const-synchronizable

### Le constat

Les tests de `test_synchronized_value.cpp` vérifient les *types* (`static_assert`
sur `const_guard`, `can_lock`…) mais n'instancient jamais `lock()` ni
`lock_shared()` pour un `T` à état `mutable`. Or pour un tel `T` (le `Memo` du
test lui-même), **tout appel à `lock()` ou `lock_shared()` est une erreur de
compilation** : la classe est déclarable mais inutilisable.

La cause : `get_mutex_type()` choisit `std::mutex` quand
`!is_synchronizable<const T>`, donc `guard = value_guard<T, std::unique_lock<std::mutex>>` —
mais le membre stocké est codé en dur `mutable std::shared_mutex mutex_;` et le
constructeur de `value_guard` prend un `std::shared_mutex&`. On ne peut pas
construire un `std::unique_lock<std::mutex>` depuis un `std::shared_mutex`.

### Scénario de test (rejeté par le compilateur — NON attendu)

```cpp
#include <threadsafe/threadsafe.h>
#include <optional>

struct Memo {
    int key;
    mutable std::optional<int> cached;
};

int main() {
    threadsafe::synchronized_value<Memo> value(Memo{1, {}});
    {
        auto write_guard = value.lock();
        write_guard->key = 2;
    }
    {
        auto read_guard = std::as_const(value).lock_shared();
        (void)read_guard->key;
    }
    return 0;
}
```

Résultat observé :

```
synchronized_value.h:36:11: error: no matching function for call to
'std::unique_lock<std::mutex>::unique_lock(std::shared_mutex&)'
   36 |         : lock_(mutex), value_(&value) {}
```

`Memo` est bien `sendable` (le `static_assert` de la classe passe), c'est
l'instanciation de `lock()` qui explose — dans les entrailles de la
bibliothèque, avec un message qui ne dit rien à l'utilisateur sur sa faute
(il n'en a commis aucune).

### Le fichier concerné, en entier (`include/threadsafe/details/synchronized_value.h`)

```cpp
#pragma once

#include <concepts>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
class synchronized_value;

template <class T, class Lock>
class value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    // Don't capture by reference, because the lock is released when the guard is destroyed.
    // Another solution could have been to use a callable taking a reference to the value,
    //but that would have been more verbose and less convenient.
    T& operator*() const noexcept { return *value_; }
    T* operator->() const noexcept { return value_; }

private:
    template <class>
    friend class synchronized_value;

    value_guard(std::shared_mutex& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

public:
    static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];

    static consteval auto get_const_guard_type() {
        if constexpr (is_synchronizable<const T>) {
            return ^^value_guard<const T, std::shared_lock<mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = [:get_const_guard_type():];

    template <class... Args>
        requires std::constructible_from<T, Args...>
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static std::shared_ptr<synchronized_value>
    make(Args&&... args) {
        return std::make_shared<synchronized_value>(
            std::forward<Args>(args)...);
    }

    // nodiscard is load-bearing: a discarded guard is a temporary destroyed at
    // the semicolon, i.e. a lock taken and immediately released.
    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable std::shared_mutex mutex_;
    T value_;
};

template <class T>
constexpr bool is_synchronizable<synchronized_value<T>> = is_sendable<T>;

template <class T>
constexpr bool is_lifetime_aware<synchronized_value<T>> = is_lifetime_aware<T>;

template <class T, class Lock>
constexpr bool is_sendable<value_guard<T, Lock>> = false;
template <class T, class Lock>
constexpr bool is_lifetime_aware<value_guard<T, Lock>> = false;

}
```

### La solution complète (fichier corrigé intégral)

Le mutex stocké est toujours un `std::shared_mutex` ; la seule chose qui varie
est le *type de verrou* du `const_guard` : partagé si `const T` est réellement
sûr en lecture, exclusif sinon. Un `std::conditional_t` dit exactement cela —
et supprime au passage les deux fonctions `consteval` + splices (voir point 2).

```cpp
#pragma once

#include <concepts>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
class synchronized_value;

template <class T, class Lock>
class value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    // The guard hands out a reference, not a copy: the lock is held exactly as
    // long as the guard lives, so the reference is valid exactly as long as it
    // is reachable.
    T& operator*() const noexcept { return *value_; }
    T* operator->() const noexcept { return value_; }

private:
    template <class>
    friend class synchronized_value;

    value_guard(std::shared_mutex& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

    // Readers may share the lock only when a const T really is read-safe; a
    // mutable member (a cache, say) writes through const and forces exclusive
    // locking even for readers.
    using reader_lock =
        std::conditional_t<is_synchronizable<const T>,
                           std::shared_lock<std::shared_mutex>,
                           std::unique_lock<std::shared_mutex>>;

public:
    using guard = value_guard<T, std::unique_lock<std::shared_mutex>>;
    using const_guard = value_guard<const T, reader_lock>;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static std::shared_ptr<synchronized_value>
    make(Args&&... args) {
        return std::make_shared<synchronized_value>(
            std::forward<Args>(args)...);
    }

    // nodiscard is load-bearing: a discarded guard is a temporary destroyed at
    // the semicolon, i.e. a lock taken and immediately released.
    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable std::shared_mutex mutex_;
    T value_;
};

template <class T>
constexpr bool is_synchronizable<synchronized_value<T>> = is_sendable<T>;

template <class T>
constexpr bool is_lifetime_aware<synchronized_value<T>> = is_lifetime_aware<T>;

template <class T, class Lock>
constexpr bool is_sendable<value_guard<T, Lock>> = false;
template <class T, class Lock>
constexpr bool is_lifetime_aware<value_guard<T, Lock>> = false;

}
```

Mise à jour nécessaire du test (dernier `static_assert` de
`test_synchronized_value.cpp`) : le `const_guard` de `Memo` devient
`value_guard<const Memo, std::unique_lock<std::shared_mutex>>` (verrou exclusif
sur le mutex partagé, et non plus l'inconstructible `unique_lock<std::mutex>`) :

```cpp
static_assert(std::same_as<sync_int::const_guard,
                           threadsafe::value_guard<
                               const int, std::shared_lock<std::shared_mutex>>>,
              "lock_shared — readers of a const-synchronizable T really share");
static_assert(std::same_as<threadsafe::synchronized_value<Memo>::const_guard,
                           threadsafe::value_guard<
                               const Memo,
                               std::unique_lock<std::shared_mutex>>>,
              "Memo has mutable state, so readers lock exclusively");
```

### Vérification de la correction (accepté, comme attendu)

La version corrigée a été compilée telle quelle (namespace `threadsafe_fixed`
en scratchpad) avec ce scénario, qui compile **et** s'exécute :

```cpp
#include <threadsafe/threadsafe.h>
#include "fixed_synchronized_value.h"
#include <optional>

struct Memo {
    int key;
    mutable std::optional<int> cached;
};

static_assert(std::same_as<
    threadsafe_fixed::synchronized_value<int>::const_guard,
    threadsafe_fixed::value_guard<const int,
                                  std::shared_lock<std::shared_mutex>>>);
static_assert(std::same_as<
    threadsafe_fixed::synchronized_value<Memo>::const_guard,
    threadsafe_fixed::value_guard<const Memo,
                                  std::unique_lock<std::shared_mutex>>>);

int main() {
    threadsafe_fixed::synchronized_value<Memo> value(Memo{1, {}});
    {
        auto write_guard = value.lock();
        write_guard->key = 2;
    }
    {
        auto read_guard = std::as_const(value).lock_shared();
        read_guard->cached = 42;
        (void)read_guard->key;
    }
    return 0;
}
```

Résultat : compilation OK, exécution OK. Le comportement pour un `T`
const-synchronizable (verrou partagé pour les lecteurs) est inchangé — la suite
de tests complète du dépôt passe avec le header corrigé, moyennant le
`static_assert` mis à jour ci-dessus.

---

## 2. SUGGESTION — La réflexion de `synchronized_value` est de la complexité accidentelle

Point pédagogique important pour une conférence : dans ce fichier, la réflexion
(`^^`, `[: :]`, deux fonctions `consteval`, 17 lignes) n'apporte rien qu'un
`std::conditional_t` d'une ligne ne dise mieux — et c'est précisément dans ce
détour que le bug du point 1 s'est caché (`mutex` calculé ≠ `mutex_` stocké).
La correction du point 1 supprime `get_mutex_type`, `get_const_guard_type`, le
type membre public `mutex` (que rien n'utilisait, code mort) et les splices.

La réflexion de la bibliothèque reste alors exactement là où elle est
irremplaçable et bien commentée : `utils.h`, `sendable.h`, `synchronizable.h`,
`lifetime_aware.h` (marche structurelle sur les membres) et les faces
info-level (`is_sendable_type`…). C'est un meilleur message pour l'audience :
*la réflexion sert quand le système de types ne sait pas répondre, pas pour
choisir entre deux types.*

---

## 3. SUGGESTION — Les lambdas capturantes sont rejetées : la première chose qu'un utilisateur essaie

### Scénario « premier contact » (rejeté — voulu par la bibliothèque, mais surprise garantie)

Un utilisateur qui découvre la bibliothèque écrit ceci en premier :

```cpp
// Scénario 1 — producteur/consommateur naïf, comme un nouvel utilisateur.
#include <threadsafe/threadsafe.h>

#include <print>
#include <string>
#include <vector>

using namespace threadsafe;

int main() {
    auto queue = synchronized_value<std::vector<std::string>>::make();
    asynchronous_task_launcher launcher;

    launcher.launch_task([queue] {
        for (int index = 0; index < 100; ++index)
            queue->lock()->push_back("message " + std::to_string(index));
    });

    launcher.launch_task([queue] {
        auto guard = queue->lock();
        if (!guard->empty())
            std::println("last: {}", guard->back());
    });

    return 0;
}
```

Résultat : **rejeté**, alors que la capture (`shared_ptr` d'un
`synchronized_value`) est sémantiquement parfaitement sûre. Extrait du
diagnostic (une cinquantaine de lignes au total) :

```
error: no matching function for call to
'threadsafe::asynchronous_task_launcher::launch_task(main()::<lambda()>)'
  • candidate 1: 'template<class F, class ... Args> requires (sendable<F>) &&
    (lifetime_aware<F>) && ... void launch_task(F, Args ...)'
      • template argument deduction/substitution failed:
        • constraints not satisfied
```

Le message dit *que* la contrainte échoue, jamais *pourquoi* : la vraie raison
— l'état d'une closure n'est pas réflectable, donc toute lambda capturante est
conservativement non-sendable (`has_unreflectable_state` dans `utils.h`) —
n'apparaît nulle part. Elle n'est documentée que dans un `static_assert` de
`test_asynchronous_task_launcher.cpp` (« a capturing lambda is not a safe
callable »).

### La version idiomatique (acceptée, compile et s'exécute)

```cpp
// Scénario 1 corrigé — le style que la bibliothèque impose:
// lambda SANS capture, l'état passe par les arguments.
#include <threadsafe/threadsafe.h>

#include <memory>
#include <print>
#include <string>
#include <vector>

using namespace threadsafe;
using queue_type = synchronized_value<std::vector<std::string>>;

int main() {
    std::shared_ptr<queue_type> queue = queue_type::make();
    asynchronous_task_launcher launcher;

    launcher.launch_task(
        [](std::shared_ptr<queue_type> q) {
            for (int index = 0; index < 100; ++index)
                q->lock()->push_back("message " + std::to_string(index));
        },
        queue);

    launcher.launch_task(
        [](std::shared_ptr<queue_type> q) {
            auto guard = q->lock();
            if (!guard->empty())
                std::println("last: {}", guard->back());
        },
        queue);

    return 0;
}
```

Résultat : accepté, exécution correcte (`last: message 0`).

### Recommandations

1. **Documenter la règle en tête d'`asynchronous_task_launcher.h`** — c'est LE
   piège n° 1 de l'API. Proposition de commentaire à ajouter au-dessus de
   `launch_task` :

   ```cpp
   // A capturing lambda is rejected: its captures are state the reflection
   // cannot see, so the traits refuse it wholesale. Pass a captureless lambda
   // and hand the state over as arguments — the traits then check each one.
   //   launcher.launch_task([](std::shared_ptr<Q> q) { ... }, queue);
   ```

2. Coupler la contrainte à un diagnostic « pourquoi » (point 4) pour que le
   compilateur raconte l'histoire à la place du formateur.

---

## 4. SUGGESTION — Offrir un « pourquoi » : `assert_sendable(^^T)`

La bibliothèque possède déjà les faces info-level (`is_sendable_type`…) ; il
manque leur pendant *bavard*. Le prototype suivant a été validé : il remonte la
marche structurelle et nomme le sous-objet fautif dans le message d'erreur.
Solution complète (nouveau fichier proposé
`include/threadsafe/details/diagnostics.h`, à inclure depuis `threadsafe.h`) :

```cpp
#pragma once

#include <meta>
#include <string>

#include <threadsafe/details/sendable.h>

namespace threadsafe {

// Compile-time "why not": walks the same structure as the trait and throws a
// consteval exception naming the first subobject that fails, so the compiler
// error names the culprit instead of a failed constraint.
consteval void assert_sendable(std::meta::info type,
                               std::u8string path = u8"") {
    using namespace std::meta;

    if (is_sendable_type(type))
        return;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(remove_reference(type));

    if (is_class_type(unqualified) && is_complete_type(unqualified)) {
        for (info base : bases_of(unqualified, context))
            if (!is_sendable_type(type_of(base)))
                assert_sendable(type_of(base), path + u8"::<base>");
        for (info member : nonstatic_data_members_of(unqualified, context))
            if (!is_sendable_type(remove_cv(type_of(member))))
                assert_sendable(remove_cv(type_of(member)),
                                path + u8"." + u8identifier_of(member));
    }
    throw exception(u8"not sendable, because of the subobject `this" + path
                        + u8"` of type "
                        + u8display_string_of(unqualified),
                    type);
}

}
```

### Scénario de test (rejeté — comme attendu, avec le bon message)

```cpp
struct Inner {
    std::string name;
    int* borrowed;
};
struct Outer {
    Inner inner;
};

int main() {
    threadsafe::assert_sendable(^^Outer);
}
```

Diagnostic obtenu (GCC 16) :

```
error: uncaught exception of type 'std::meta::exception'; 'what()':
'not sendable, because of the subobject `this.inner.borrowed` of type int*'
```

Comparé au « constraints not satisfied » du point 3, c'est le jour et la nuit —
et c'est en soi une belle démo de conférence (la réflexion qui *explique* le
trait qu'elle calcule). Le même schéma se décline en
`assert_synchronizable` / `assert_lifetime_aware` si souhaité.

---

## 5. SUGGESTION — `containers.h` : 205 lignes dont ~160 de duplication mécanique

### Le constat

Pour chacun des 13 conteneurs standard, les trois traits sont *exactement* la
même conjonction sur les arguments template. Le fichier actuel épelle ~40
spécialisations à la main ; toute évolution (ajouter `std::flat_map`, ajouter
un trait) multiplie les points d'oubli. Extrait représentatif du fichier
concerné (le motif se répète à l'identique 13 fois par trait) :

```cpp
template <class T, class A>
constexpr bool is_sendable<std::vector<T, A>> = is_sendable<T> && is_sendable<A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_sendable<std::unordered_map<K, V, H, Eq, A>> =
    is_sendable<K> && is_sendable<V> && is_sendable<H> && is_sendable<Eq>
    && is_sendable<A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_synchronizable<const std::unordered_map<K, V, H, Eq, A>> =
    is_synchronizable<const K> && is_synchronizable<const V>
    && is_synchronizable<const H> && is_synchronizable<const Eq>
    && is_synchronizable<const A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_lifetime_aware<std::unordered_map<K, V, H, Eq, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<V> && is_lifetime_aware<H>
    && is_lifetime_aware<Eq> && is_lifetime_aware<A>;
// ... idem pour deque, list, forward_list, basic_string, map, multimap,
// set, multiset, unordered_multimap, unordered_set, unordered_multiset.
```

### Solution complète (remplacement intégral de `containers.h`, 64 lignes)

Une spécialisation partielle à pack (`C<Args...>`) capture tous les paramètres
d'un conteneur donné, quel que soit leur nombre ; une macro l'énonce une fois
par conteneur pour les trois traits :

```cpp
#pragma once

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
constexpr bool is_sendable<std::allocator<T>> = true;
template <class T>
constexpr bool is_synchronizable<const std::allocator<T>> = true;
template <class T>
constexpr bool is_lifetime_aware<std::allocator<T>> = true;

// [res.on.data.races]: the const member functions of a standard container may
// run concurrently, so a const container is read-safe exactly when everything
// a reader reaches through it — elements and stored policies — is. The
// explicit rules also keep the recursion out of libstdc++ internals, whose
// mutable members (unordered_*'s rehash policy) are covered by that guarantee.
//
// For every standard container the three rules are the same conjunction over
// its template arguments — element types and stored policies alike — so one
// macro states them once per container.
#define THREADSAFE_STANDARD_CONTAINER_TRAITS(container)                        \
    template <class... Args>                                                   \
    constexpr bool is_sendable<container<Args...>> =                           \
        (is_sendable<Args> && ...);                                            \
    template <class... Args>                                                   \
    constexpr bool is_synchronizable<const container<Args...>> =               \
        (is_synchronizable<const Args> && ...);                                \
    template <class... Args>                                                   \
    constexpr bool is_lifetime_aware<container<Args...>> =                     \
        (is_lifetime_aware<Args> && ...)

THREADSAFE_STANDARD_CONTAINER_TRAITS(std::vector);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::deque);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::list);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::forward_list);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::basic_string);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::map);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::multimap);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::set);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::multiset);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::unordered_map);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::unordered_multimap);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::unordered_set);
THREADSAFE_STANDARD_CONTAINER_TRAITS(std::unordered_multiset);

#undef THREADSAFE_STANDARD_CONTAINER_TRAITS

}
```

### Vérification

Les **10 fichiers de tests du dépôt compilent tous sans erreur** avec ce
remplacement (répertoire d'override placé avant `include/` dans le chemin
d'inclusion). Les sémantiques sont identiques : `basic_string` conjonctionne
aussi `char_traits` (type vide, trivialement conforme aux trois traits), et
`map` conjonctionne `Cmp` exactement comme aujourd'hui.

Différence assumée : `is_sendable<std::string>` vaut
`is_sendable<char> && is_sendable<char_traits<char>> && is_sendable<allocator<char>>`
au lieu d'omettre `char_traits`. Le résultat est le même pour tout
`char_traits` standard ; un `Traits` utilisateur pathologique serait désormais
inspecté aussi, ce qui est plutôt un gain de sûreté.

Note pour l'auditoire : la version dupliquée a une vertu pédagogique (chaque
règle se lit seule) ; si elle est conservée volontairement, un commentaire de
tête l'assumant (« spelled out on purpose ») éviterait qu'un lecteur y voie de
la négligence.

---

## 6. SUGGESTION — Asymétrie de l'opt-in

Il existe un seul macro d'opt-in, et pour un seul trait
(`synchronizable_base.h`) :

```cpp
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)         \
    template <>                                              \
    inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
```

Pour `is_sendable` et `is_lifetime_aware`, l'utilisateur doit écrire la
spécialisation brute. Elle fonctionne très bien (scénario du point 9), mais
l'asymétrie interroge : soit le macro dit quelque chose de spécial
(« UNSAFE », promesse la plus forte) et alors le README/talk doit le dire,
soit il faut les trois macros, soit aucun. Pour une bibliothèque éducative, ma
préférence : **aucun macro** — la spécialisation explicite
`template <> constexpr bool threadsafe::is_sendable<X> = true;` est
auto-documentée, grep-able, et montre le mécanisme réel (c'est d'ailleurs ce
que font déjà les tests du dépôt, qui n'utilisent le macro que deux fois).
Alternative si le macro reste : le renommer sur le modèle des deux autres et
fournir les trois (`THREADSAFE_UNSAFE_ASSERT_SENDABLE`,
`THREADSAFE_ASSERT_LIFETIME_AWARE`).

---

## 7. SUGGESTION — Les headers `details/` inclus isolément donnent d'autres réponses

Découvert par accident pendant l'audit : un TU qui inclut
`<threadsafe/details/synchronized_value.h>` (qui tire `sendable.h` mais pas
`vocabulary.h` ni `containers.h`) obtient `is_sendable<std::optional<int>> == false`
(le constructeur template d'`optional` bloque le défaut structurel), alors
qu'un TU incluant `<threadsafe/threadsafe.h>` obtient `true` via la
spécialisation de `vocabulary.h`.

Scénario observé : le même `static_assert(threadsafe::is_sendable<Memo>)`
(où `Memo` contient un `std::optional<int>`) **passe** avec `threadsafe.h` et
**échoue** avec le seul header de détail. Deux TU d'un même programme incluant
des sous-ensembles différents violent l'ODR sur la variable template (IFNDR).

Le nom `details/` signale déjà « ne pas inclure directement », mais rien ne
l'empêche. Garde-fou minimal, une ligne en tête de chaque header de
`details/` :

```cpp
#ifndef THREADSAFE_IN_AGGREGATE_HEADER
#error "include <threadsafe/threadsafe.h> instead of individual detail headers"
#endif
```

avec dans `threadsafe.h` :

```cpp
#pragma once
#define THREADSAFE_IN_AGGREGATE_HEADER
#include <threadsafe/details/synchronizable.h>
// ... (inclusions actuelles inchangées)
#undef THREADSAFE_IN_AGGREGATE_HEADER
```

(Ou, moins intrusif : un commentaire de tête dans chaque header de détail.)

---

## 8. OK — Lisibilité, noms, localisation de la réflexion

Points forts, à garder tels quels :

- **Les commentaires sont excellents** — le pavé de `utils.h` sur le
  détournement des constructeurs template par les formes forwarding
  (`may_hijack_copy_move`) est le meilleur commentaire technique du dépôt, et
  un vrai contenu de conférence. Idem pour la note `[res.on.data.races]` de
  `containers.h` et le « nodiscard is load-bearing » de `synchronized_value.h`.
- **La réflexion est localisée et motivée** : quatre fichiers seulement la
  touchent, chacun explique *pourquoi* (le commentaire de
  `test_deferred_specialization.cpp` sur `substitute` et la résolution différée
  des spécialisations est remarquable). Seule exception : le point 2 ci-dessus.
- **Les noms sont explicites** (`default_is_const_synchronizable`,
  `has_only_default_copy_move_destroy`, `asynchronous_task_launcher`), fidèles
  à la règle du projet.
- **Les tests servent de documentation** : presque chaque `static_assert`
  porte un message qui énonce la règle. C'est la meilleure « doc » du dépôt —
  raison de plus pour remonter les règles clés (lambdas capturantes,
  `make()` + `shared_ptr` comme idiome de partage) dans les headers ou un
  README, car un utilisateur ne lit pas les tests en premier.

Broutilles relevées (à corriger à l'occasion) : espace manquant dans
`//but that would have been` (`value_guard`) ; trailing whitespace dans
`synchronizable.h` (fin de `default_is_const_synchronizable`) ; le commentaire
de `value_guard` (« Don't capture by reference ») décrit une décision de
conception historique plus que le code présent — la version du point 1 le
reformule.

## 9. OK — Extension et composition (scénario accepté, comme attendu)

Le scénario suivant, écrit du point de vue d'un utilisateur qui adapte ses
types et un type tiers, compile tel quel — l'opt-in est simple et les traits
composent bien (`synchronizable` implique `sendable`, une référence vers un
type synchronizable peut traverser) :

```cpp
// Scénario 3 — opt-in pour ses propres types et des types tiers.
#include <threadsafe/threadsafe.h>

#include <mutex>
#include <string>
#include <vector>

// Un type utilisateur qui gère sa propre synchronisation.
class ThreadSafeLogger {
public:
    void log(const std::string& line) {
        std::scoped_lock guard(mutex_);
        lines_.push_back(line);
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;
};

// Opt-in via le macro (synchronizable seulement).
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(ThreadSafeLogger);

// Type "tiers" avec pimpl: incomplet -> le trait par défaut lance une
// exception consteval; l'utilisateur DOIT spécialiser.
struct ThirdPartyHandle {
    struct Impl;
    Impl* impl;
    ~ThirdPartyHandle();
};

template <>
constexpr bool threadsafe::is_sendable<ThirdPartyHandle> = true;
template <>
constexpr bool threadsafe::is_lifetime_aware<ThirdPartyHandle> = true;

static_assert(threadsafe::is_synchronizable<ThreadSafeLogger>);
static_assert(threadsafe::is_sendable<ThreadSafeLogger>);
static_assert(threadsafe::is_sendable<ThreadSafeLogger&>,
              "synchronizable => une référence peut traverser");
static_assert(threadsafe::is_sendable<ThirdPartyHandle>);

// Composition: un aggregate contenant le logger reste-t-il synchronizable ?
struct Server {
    ThreadSafeLogger logger;
    int port;
};
static_assert(threadsafe::is_sendable<Server>);
static_assert(!threadsafe::is_synchronizable<Server>,
              "synchronizable n'est PAS structurel: pas de propagation");

int main() { return 0; }
```

Résultat : accepté. Deux observations d'API en passant :

- Le message d'erreur consteval pour un type incomplet (pimpl) est **bon** : il
  nomme le remède (« specialize is_sendable … the pimpl idiom »). C'est la
  preuve que la bibliothèque *sait* produire de bons diagnostics — le point 4
  généralise cette pratique.
- `is_synchronizable` volontairement non structurel (dernier `static_assert`)
  est cohérent mais mérite une ligne de doc : un utilisateur pourrait
  s'attendre à ce qu'un aggregate de types synchronizables le soit.

## Scénario 2 — partage de configuration par `copy_on_write` (accepté, comme attendu)

```cpp
// Scénario 2 — partage de configuration en lecture, copy_on_write.
#include <threadsafe/threadsafe.h>

#include <map>
#include <string>
#include <thread>

using namespace threadsafe;

struct Config {
    std::map<std::string, std::string> entries;
    int verbosity = 0;
};

int main() {
    copy_on_write<Config> config(Config{{{"host", "localhost"}}, 2});

    static_assert(is_sendable<copy_on_write<Config>>);

    copy_on_write<Config> snapshot_for_thread = config; // copie superficielle
    std::jthread reader([snapshot = snapshot_for_thread] {
        (void)snapshot->verbosity;
    });

    config.as_mutable().verbosity = 3; // clone si partagé
    return 0;
}
```

Résultat : compile et s'exécute. `copy_on_write` est l'API la plus agréable de
la bibliothèque : zéro friction sur ce scénario, la construction `explicit`
avec un `Config` agrégé est naturelle, et « partager = copier le handle » est
bien couvert par le `static_assert` des tests (« share by copying it »).
(Nota : le `jthread` ci-dessus contourne le launcher précisément à cause du
point 3 — avec `launch_task` il faudrait passer le snapshot en argument.)
