# 13 — Thread safety réelle et performance

Audit runtime de la bibliothèque ThreadSafe : data races réelles (ThreadSanitizer + tests empiriques), races logiques (TOCTOU), performance de compilation et performance runtime des helpers (`synchronized_value`, `copy_on_write`, `asynchronous_task_launcher`).

**Environnement** : macOS arm64 (Darwin 25.6.0), g++-16 (Homebrew GCC 16.2.0), `-std=c++2c -freflection` ; Apple clang 21 pour ThreadSanitizer (voir méthodologie).

## Résumé des verdicts

| # | Sujet | Verdict |
|---|-------|---------|
| 1 | `synchronized_value` : `lock()`/`lock_shared()` ne compilent pas quand `const T` n'est pas synchronizable (membre `std::shared_mutex` codé en dur) | **PROBLÈME (majeur)** |
| 2 | Data race atteignable par l'API publique : évasion de référence hors d'un guard temporaire | **PROBLÈME (majeur, limitation connue)** |
| 3 | `copy_on_write::as_mutable()` : `use_count()` relaxed → race selon le modèle mémoire (raison du retrait de `shared_ptr::unique()` en C++20) | **PROBLÈME (subtil)** |
| 4 | TOCTOU : deux `lock()` séparés violent un invariant ; auto-deadlock facile ; pas de verrouillage multiple | **SUGGESTION** |
| 5 | Usage nominal des trois helpers sous TSan et en stress réel : zéro race, résultats exacts | **OK** |
| 6 | Performance de compilation : ~0,62 s/TU, dominée par les includes standard ; la réflexion est négligeable | **OK (suggestions mineures)** |
| 7 | Performance runtime : `shared_mutex` systématique (+87 % vs `std::mutex`) ; `copy_on_write` sans surcoût vs `shared_ptr` ; un thread OS par tâche dans le launcher | **SUGGESTION** |
| 8 | Infrastructure de test : aucun corps de fonction runtime n'est jamais instancié par la suite compile-time | **PROBLÈME (cause racine du #1)** |

---

## Méthodologie — et une contrainte d'infrastructure

ThreadSanitizer exige que **tout** le code instancié soit instrumenté ; or **Homebrew GCC sur Apple Silicon ne fournit pas `libtsan`** (seul `libasan` est présent dans `/opt/homebrew/Cellar/gcc/16.2.0/lib/gcc/current/`) — l'édition de liens échoue sur `___tsan_atomic32_*`. Aucun clang installé ne supporte la réflexion P2996. Deux voies complémentaires ont donc été utilisées :

1. **TSan via un shim** : les corps runtime des trois helpers ont été recopiés **verbatim** dans un header sans réflexion (les traits `consteval` n'ont aucun effet runtime ; `get_mutex_type` remplacé par son comportement effectif — voir finding #1), compilés avec Apple clang `-fsanitize=thread -O1 -g`.
2. **Empirique sur les vrais headers** : les mêmes scénarios compilés avec g++-16 sur la vraie bibliothèque, avec assertions sur les invariants (compteurs exacts, originaux non modifiés), exécutés en boucle.

**Recommandation** : ajouter un job CI Linux (GCC + `-fsanitize=thread`) exécutant des scénarios runtime — sur cette plateforme, la vraie bibliothèque est instrumentable directement.

---

## 1. PROBLÈME MAJEUR — `synchronized_value` ne compile pas hors du chemin `shared_mutex`

### Le code concerné (fichier complet, `include/threadsafe/details/synchronized_value.h`)

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

### Le défaut

`get_mutex_type()` calcule élégamment `std::mutex` ou `std::shared_mutex` selon `is_synchronizable<const T>`, et les alias `guard`/`const_guard` utilisent ce type… mais **le membre est codé en dur** :

```cpp
mutable std::shared_mutex mutex_;                       // toujours shared_mutex
value_guard(std::shared_mutex& mutex, T& value);        // signature figée aussi
```

Dès que `is_synchronizable<const T>` est faux, `guard` = `value_guard<T, std::unique_lock<std::mutex>>` et son membre `std::unique_lock<std::mutex>` n'est **pas constructible** depuis un `std::shared_mutex&` : tout appel à `lock()` ou `lock_shared()` est une erreur de compilation.

Conséquence double :
- **Fonctionnel** : `synchronized_value<T>` est inutilisable pour tout `T` non const-synchronizable (le cas *memo/cache mutable* pourtant testé au niveau des types dans `test_synchronized_value.cpp`, ligne 116 : `value_guard<const Memo, std::unique_lock<std::mutex>>`).
- **Performance** : même quand ça compile (chemin `shared_mutex`), un `std::shared_mutex` est payé alors qu'un `std::mutex` aurait suffi — mesuré section 7 : **+28 %** sur un incrément verrouillé.

### Pourquoi la suite de tests ne l'a jamais vu

Les tests sont uniquement des `static_assert` sur des types et des `requires`-expressions (`can_lock<T>` vérifie la bonne formation de **l'expression d'appel**, c'est-à-dire la signature — pas le **corps** de `lock()`, qui n'est instancié qu'à l'utilisation réelle). Aucun TU de la suite n'instancie jamais un corps de fonction runtime. C'est le finding #8 : *compiler = tester* ne couvre que ce que la compilation instancie.

### Scénario (rejeté par le compilateur alors qu'il devrait être accepté)

`scenario_mutex_type.cpp` :

```cpp
#include <threadsafe/threadsafe.h>

// A type that is sendable but NOT const-synchronizable:
// a mutable int is writable through const&, and is_synchronizable<int> is false.
struct cache_holder {
    mutable int cached_result = 0;
    int value = 0;
};

static_assert(threadsafe::is_sendable<cache_holder>);
static_assert(!threadsafe::is_synchronizable<const cache_holder>);

// So synchronized_value must pick std::mutex...
static_assert(std::same_as<
    typename threadsafe::synchronized_value<cache_holder>::mutex,
    std::mutex>);

int main() {
    threadsafe::synchronized_value<cache_holder> synchronized_cache{};
    {
        auto guard = synchronized_cache.lock();
        guard->value = 42;
    }
    auto const_guard = synchronized_cache.lock_shared();
    return const_guard->value;
}
```

**Résultat** : les trois `static_assert` passent (les traits et l'alias `mutex` sont corrects), mais l'instanciation de `lock()` échoue :

```
synchronized_value.h:36:11: error: no matching function for call to
    'std::unique_lock<std::mutex>::unique_lock(std::shared_mutex&)'
   36 |         : lock_(mutex), value_(&value) {}
```

### Solution complète (fichier corrigé intégral)

Deux changements : `value_guard` déduit le type de mutex de son `Lock` (`typename Lock::mutex_type&`), et le membre utilise l'alias calculé (`mutable mutex mutex_;`).

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

    value_guard(typename Lock::mutex_type& mutex, T& value)
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
    mutable mutex mutex_;
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

**Validation** : avec ce fichier, le scénario compile et retourne 42 ; `test_synchronized_value.cpp` et `test_soundness_regressions.cpp` compilent inchangés ; le scénario de stress nominal (section 5) donne des résultats exacts. Bonus : `synchronized_value<cache_holder>` porte maintenant un `std::mutex`, moins cher (section 7).

---

## 2. PROBLÈME MAJEUR — data race atteignable par l'API publique (évasion de référence)

La bibliothèque annonce une sûreté « entièrement vérifiée à la compilation ». Sans borrow checker, une référence obtenue à travers un guard **temporaire** survit au verrou — API publique seule, sans `const_cast` ni astuce.

### Scénario complet (`tsan_guard_escape.cpp`)

```cpp
// Tentative de PROVOQUER une race via l'API publique, sans const_cast ni astuce :
// on lie une référence au travers d'un guard TEMPORAIRE. Le guard est détruit à la
// fin de l'expression complète -> le verrou est relâché, la référence survit.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>
#include <vector>

int main() {
    auto shared_counter = threadsafe::synchronized_value<int>::make(0);
    std::vector<std::jthread> workers;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        workers.emplace_back([shared_counter] {
            // API publique uniquement : lock() -> operator* -> int&.
            // Le temporaire value_guard meurt au point-virgule.
            int& unprotected = *shared_counter->lock();
            for (int i = 0; i < 100000; ++i)
                ++unprotected;   // course de données : plus aucun verrou tenu
        });
    workers.clear();
    std::printf("count=%d (expected 400000)\n", *shared_counter->lock_shared());
    return 0;
}
```

### Résultats

- **TSan (shim, Apple clang)** : `WARNING: ThreadSanitizer: data race` ; total observé 300 000/400 000.
- **Vraie bibliothèque (g++-16 -O1, 3 exécutions)** : `count=100000 (expected 400000)` à chaque fois — 75 % des incréments perdus. Race confirmée sur le vrai code.
- `-Wdangling-reference` de GCC 16 **ne signale rien** sur ce motif (0 avertissement).

### Analyse

Le commentaire dans `value_guard` (« *Don't capture by reference, because the lock is released when the guard is destroyed* ») montre que le point est connu, mais il documente un piège au lieu de le fermer. C'est la limite fondamentale de tout guard C++ (`std::lock_guard`, `boost::synchronized_value` ont le même trou) — impossible à fermer complètement sans borrow checking. On peut cependant **réduire fortement la surface** :

1. **Offrir une API à portée fermée `apply()`** : le verrou englobe structurellement l'usage ; l'évasion demande alors une capture explicite de référence dans la lambda, visuellement suspecte, au lieu d'un simple `auto&` idiomatique. Ajout complet à `synchronized_value` (compatible avec le fichier corrigé de la section 1) :

```cpp
    // Runs f under the lock; the lock scope structurally encloses the use.
    template <class F>
        requires std::invocable<F&, T&>
    decltype(auto) apply(F&& f) {
        std::unique_lock<mutex> lock(mutex_);
        return std::forward<F>(f)(value_);
    }

    template <class F>
        requires std::invocable<F&, const T&>
    decltype(auto) apply(F&& f) const {
        const_guard_lock lock(mutex_);
        return std::forward<F>(f)(std::as_const(value_));
    }

    // avec, dans la partie types :
    static consteval auto get_const_lock_type() {
        if constexpr (is_synchronizable<const T>) {
            return ^^std::shared_lock<mutex>;
        } else {
            return ^^std::unique_lock<mutex>;
        }
    }
    using const_guard_lock = [:get_const_lock_type():];
```

2. **Documenter la règle unique** : *toujours* nommer le guard (`auto guard = sv.lock();`), jamais déréférencer un guard temporaire pour initialiser une référence.
3. Pour un build clang aval : `[[clang::lifetimebound]]` sur `operator*`/`operator->` diagnostiquerait précisément ce motif ; GCC ne l'offre pas encore.

Pour une bibliothèque pédagogique, ce trou mérite une diapositive : c'est exactement la différence entre `MutexGuard<'a, T>` de Rust (le borrow checker refuse l'évasion) et le meilleur C++ possible.

---

## 3. PROBLÈME — `copy_on_write::as_mutable()` : le test `use_count()` est un chargement *relaxed*

### Le code concerné (fichier complet, `include/threadsafe/details/copy_on_write.h`)

```cpp
#pragma once

#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
class copy_on_write {
public:
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_.get(); }

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

namespace detail {
template <class T>
consteval bool cow_is_sendable() {
    if constexpr (is_sendable<T>)
        return is_synchronizable<const T>;
    else
        return false;
}
}

template <class T>
constexpr bool is_sendable<copy_on_write<T>> = detail::cow_is_sendable<T>();

template <class T>
constexpr bool is_lifetime_aware<copy_on_write<T>> = is_lifetime_aware<T>;

}
```

### Le défaut

Le modèle de traits est correct (voir plus bas) et la logique de `as_mutable()` est celle de `Arc::make_mut` de Rust — **sauf l'ordonnancement mémoire**. `shared_ptr::use_count()` est un chargement **relaxed** ; libstdc++ le dit lui-même (`bits/shared_ptr_base.h`, `_M_get_use_count`) :

```cpp
// No memory barrier is used here so there is no synchronization
// with other threads.
```

Scénario : le thread B possède une copie du `copy_on_write` (envoyée par valeur, ce que les traits autorisent quand `T` est const-synchronizable), lit `*copy`, puis détruit sa copie — décrément **release** du compteur. Le thread A appelle `as_mutable()`, lit `use_count() == 1` (relaxed) et **écrit en place**. Aucune arête *happens-before* entre les lectures de B et l'écriture de A : data race selon le modèle mémoire. C'est précisément la raison pour laquelle `shared_ptr::unique()` a été déprécié en C++17 puis retiré en C++20.

### Scénario TSan (`tsan_cow_use_count.cpp`)

```cpp
// as_mutable() se fonde sur ptr_.use_count() == 1 (chargement *relaxed*) pour
// muter en place. Le thread lecteur qui relâche sa copie fait un décrément
// *release* ; sans chargement/fence *acquire* côté écrivain, les lectures du
// lecteur ne sont pas ordonnées avant l'écriture en place -> data race
// théorique (raison pour laquelle shared_ptr::unique() a été retiré en C++20).
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>

int main() {
    for (int round = 0; round < 20000; ++round) {
        threadsafe::copy_on_write<int> value(round);
        std::jthread reader([copy = value] {
            volatile int sink = *copy;  // lecture du bloc partagé
            (void)sink;
        });                             // release du bloc à la destruction
        while (true) {
            int& mutable_ref = value.as_mutable();
            if (&mutable_ref == &*value && value.operator->() != nullptr) {
                mutable_ref = -round;   // écriture, potentiellement en place
                break;
            }
        }
    }
    std::puts("tsan_cow_use_count: done");
    return 0;
}
```

**Résultat (TSan, shim)** : `WARNING: ThreadSanitizer: data race … tsan_cow_use_count.cpp:21` — l'écriture en place est signalée en course avec la lecture du thread relâchant sa copie. Le défaut est dans la logique recopiée verbatim, indépendante de la réflexion.

### Solution complète (fichier corrigé intégral)

Le correctif est celui de `Arc::make_mut` : une **fence acquire** appariée au décrément release, exécutée avant de muter en place. (`<atomic>` est déjà inclus.)

```cpp
#pragma once

#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
class copy_on_write {
public:
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_.get(); }

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1) {
            ptr_ = std::make_shared<T>(*ptr_);
        } else {
            // use_count() is a relaxed load; the last other owner released the
            // block with a release decrement. Pair it before writing in place
            // (same pattern as Rust's Arc::make_mut).
            std::atomic_thread_fence(std::memory_order_acquire);
        }
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

namespace detail {
template <class T>
consteval bool cow_is_sendable() {
    if constexpr (is_sendable<T>)
        return is_synchronizable<const T>;
    else
        return false;
}
}

template <class T>
constexpr bool is_sendable<copy_on_write<T>> = detail::cow_is_sendable<T>();

template <class T>
constexpr bool is_lifetime_aware<copy_on_write<T>> = is_lifetime_aware<T>;

}
```

**Validation** : compile avec les vrais headers ; `test_copy_on_write.cpp` passe inchangé ; le stress nominal reste exact. **Caveat honnête** : TSan ne modélise pas `atomic_thread_fence` (limitation documentée du runtime TSan), donc l'avertissement persiste sous TSan même corrigé — la justification est normative (§atomics.fences : opération release + fence acquire précédée d'un chargement observant la valeur écrite ⇒ *synchronizes-with*), pas expérimentale. C'est le même correctif, pour la même raison, que `fence(Acquire)` dans `Arc::make_mut`. Alternative sans fence si l'on veut la propreté TSan : toujours copier (perte de l'optimisation propriétaire-unique).

### Ce qui est correct dans `copy_on_write` (vérifié)

- `is_sendable<copy_on_write<T>>` exige `is_synchronizable<const T>` : indispensable, car deux copies dans deux threads lisent le même bloc via `const T&`.
- Le `copy_on_write` lui-même n'est **pas** synchronizable (template primaire à `false`, et sa constness structurelle est bloquée par le constructeur template via `may_hijack_copy_move`) : impossible de partager le même objet `copy_on_write` entre threads — c'est ce qui rend le protocole `use_count` sain (le compteur ne peut qu'être décrémenté par un tiers, jamais incrémenté à l'insu du propriétaire). L'analogie `Arc<T>`/`make_mut` est exacte, à la fence près.

---

## 4. SUGGESTION — races logiques (TOCTOU), auto-deadlock, verrouillage multiple

### TOCTOU : démonstration (`toctou_demo.cpp`)

```cpp
// Race condition LOGIQUE (TOCTOU) : chaque accès est verrouillé, aucune data
// race, mais l'invariant "ne jamais dépasser la limite" est violé parce que le
// test et la modification prennent deux verrous séparés.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int tickets_limit = 1000;
    auto tickets_sold = threadsafe::synchronized_value<int>::make(0);
    std::vector<std::jthread> sellers;
    for (int seller = 0; seller < 8; ++seller)
        sellers.emplace_back([tickets_sold] {
            for (int attempt = 0; attempt < 100000; ++attempt) {
                if (*tickets_sold->lock_shared() < tickets_limit) {  // check
                    // ... un autre thread peut vendre ici ...
                    ++*tickets_sold->lock();                         // act
                }
            }
        });
    sellers.clear();
    const int total = *tickets_sold->lock_shared();
    std::printf("tickets vendus = %d (limite %d)%s\n", total, tickets_limit,
                total > tickets_limit ? "  <-- invariant viole" : "");
    return 0;
}
```

**Résultat (vraie bibliothèque, 2 exécutions)** : `tickets vendus = 1003` puis `1002` — invariant violé, sans aucune data race. Le motif `*sv->lock_shared()` puis `*sv->lock()` est le plus court à écrire, donc c'est celui que l'utilisateur écrira. L'API guard permet la version correcte (un seul `lock()` englobant), mais ne la rend pas plus facile que la mauvaise ; `apply()` (section 2) rend la bonne version la plus courte : `tickets_sold->apply([](int& sold) { if (sold < limit) ++sold; });`.

### Auto-deadlock trivial

Rencontré pendant l'audit lui-même : garder un `guard` vivant et rappeler `lock()`/`lock_shared()` sur le même objet bloque définitivement (ni `std::mutex` ni `std::shared_mutex` ne sont récursifs, et l'upgrade lecteur→écrivain n'existe pas). Aucune détection possible à la compilation ; à documenter explicitement — idéal pédagogiquement, d'ailleurs.

### Verrouillage multiple

Rien n'équivaut à `std::scoped_lock(a, b)` : verrouiller deux `synchronized_value` s'écrit `auto ga = a.lock(); auto gb = b.lock();` — ordre non canonicalisé, deadlock si un autre thread verrouille dans l'ordre inverse. Suggestion : un ami `template <class... SVs> auto lock_all(SVs&... svs)` bâti sur `std::scoped_lock`/`std::lock` à exposer, ou au minimum documenter l'ordre.

---

## 5. OK — usage nominal : zéro data race, résultats exacts

Scénario complet (`tsan_helpers_ok.cpp`), exécuté (a) sous TSan via le shim, (b) trois fois sur la vraie bibliothèque avec g++-16 :

```cpp
// Usage nominal des helpers sous ThreadSanitizer : attendu = zéro race.
#include <threadsafe/threadsafe.h>

#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // 1) synchronized_value : incréments concurrents
    {
        auto shared_counter = threadsafe::synchronized_value<int>::make(0);
        std::vector<std::jthread> workers;
        for (int worker_index = 0; worker_index < 8; ++worker_index)
            workers.emplace_back([shared_counter] {
                for (int i = 0; i < 10000; ++i)
                    ++*shared_counter->lock();
            });
        workers.clear();
        assert(*shared_counter->lock_shared() == 80000);
    }

    // 2) synchronized_value : lecteurs partagés + écrivain
    {
        auto shared_text =
            threadsafe::synchronized_value<std::vector<int>>::make(100, 7);
        std::vector<std::jthread> workers;
        for (int reader = 0; reader < 4; ++reader)
            workers.emplace_back([shared_text] {
                for (int i = 0; i < 5000; ++i) {
                    auto guard = shared_text->lock_shared();
                    volatile int sink = guard->front() + guard->back();
                    (void)sink;
                }
            });
        workers.emplace_back([shared_text] {
            for (int i = 0; i < 5000; ++i)
                shared_text->lock()->push_back(i);
        });
        workers.clear();
    }

    // 3) copy_on_write : copies envoyées à d'autres threads, as_mutable concurrent
    {
        threadsafe::copy_on_write<std::vector<int>> original(1000, 1);
        std::vector<std::jthread> workers;
        for (int worker_index = 0; worker_index < 8; ++worker_index)
            workers.emplace_back([cow_copy = original] mutable {
                for (int i = 0; i < 200; ++i) {
                    volatile int sink = (*cow_copy)[i];  // lecture du bloc partagé
                    (void)sink;
                    cow_copy.as_mutable()[i] = i;        // doit copier avant d'écrire
                }
                assert((*cow_copy)[199] == 199);
            });
        workers.clear();
        assert((*original)[0] == 1);  // l'original n'a jamais été modifié
    }

    // 4) asynchronous_task_launcher : tâches détachées + tâche scoped
    {
        threadsafe::asynchronous_task_launcher launcher;
        auto shared_counter = threadsafe::synchronized_value<long>::make(0);
        for (int task_index = 0; task_index < 8; ++task_index)
            launcher.launch_task([](std::shared_ptr<threadsafe::synchronized_value<long>> counter) {
                for (int i = 0; i < 10000; ++i)
                    ++*counter->lock();
            }, shared_counter);
        launcher.launch_scoped_task([](std::shared_ptr<threadsafe::synchronized_value<long>> counter) {
            *counter->lock() += 5;
        }, shared_counter);
    }

    std::puts("tsan_helpers_ok: done");
    return 0;
}
```

**Résultats** : TSan (shim) : **aucun avertissement**, sortie `tsan_helpers_ok: done`. Vraie bibliothèque : trois exécutions, toutes assertions passées, compteurs exacts. Les tentatives d'évasion *bloquées par les traits* fonctionnent aussi comme attendu (déjà couvertes par la suite : guard non sendable vers `launch_task`, pointeur brut rejeté par `lifetime_aware`, etc.).

---

## 6. Performance de compilation — OK, dominée par la bibliothèque standard

Mesures (g++-16, `-j1`, machine arm64) :

| Mesure | Temps |
|---|---|
| Build complet des 10 TU de test (séquentiel) | **6,2 s** (~0,62 s/TU) |
| TU vide incluant `<threadsafe/threadsafe.h>` | **0,63 s** |
| TU vide sans include | 0,07 s |
| `<meta>` seul | 0,22 s |
| `<memory>` seul | 0,27 s |
| `<thread>` seul | 0,45 s |
| `<ranges>` seul | 0,28 s |
| `<functional>` seul | 0,23 s |
| `details/sendable.h` seul | 0,23 s |
| `details/containers.h` seul | 0,39 s |

Lecture : **un TU de test coûte le même prix qu'un TU vide incluant le header** — le contenu des tests (centaines d'instanciations de traits consteval, réflexion comprise) est perdu dans le bruit. La récursion structurelle des traits est peu profonde et memoïsée par l'instanciation des variable templates. Aucun problème de passage à l'échelle visible.

Améliorations possibles (mineures) :
- `lifetime_aware.h` paie `<ranges>` (~0,28 s) pour un seul concept (`std::ranges::borrowed_range`). Si un TU n'inclut que ce header, c'est le poste dominant. Difficile à éviter proprement — à assumer.
- `asynchronous_task_launcher.h` importe `<thread>` (0,45 s, le plus cher). Inévitable pour `jthread`, mais c'est un argument pour garder l'inclusion granulaire (`threadsafe.h` agrégateur ≠ obligatoire) et documenter que les utilisateurs « traits seulement » incluent `details/sendable.h`… qui reste léger (0,23 s).
- Rien à gagner côté réflexion : elle est gratuite à cette échelle.

---

## 7. Performance runtime — coûts cachés mesurés

Micro-benchmark (`bench.cpp`, g++-16 `-O2`, 5 M itérations, 1 thread) :

```cpp
// Micro-benchmark : synchronized_value (shared_mutex systematique) vs
// mutex + T brut vs shared_mutex + T brut ; copy_on_write vs shared_ptr.
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

template <class F>
double time_ms(F&& body) {
    const auto start = std::chrono::steady_clock::now();
    body();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

volatile long sink;

int main() {
    constexpr long iterations = 5'000'000;

    threadsafe::synchronized_value<long> synchronized_counter{0};
    std::printf("increment verrouille, 1 thread, %ld iterations:\n", iterations);
    std::printf("  synchronized_value<long>::lock()      : %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) ++*synchronized_counter.lock(); }));

    long raw_counter = 0;
    std::mutex plain_mutex;
    std::printf("  std::mutex + long brut                : %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) { std::lock_guard g(plain_mutex); ++raw_counter; } }));

    long raw_counter2 = 0;
    std::shared_mutex shared_mutex_only;
    std::printf("  std::shared_mutex + long brut         : %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) { std::unique_lock g(shared_mutex_only); ++raw_counter2; } }));

    std::printf("lecture, 1 thread, %ld iterations:\n", iterations);
    std::printf("  synchronized_value lock_shared()      : %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) sink = *synchronized_counter.lock_shared(); }));

    threadsafe::copy_on_write<long> cow_value{7};
    std::printf("  copy_on_write operator* (lecture)     : %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) sink = *cow_value; }));

    auto shared = std::make_shared<long>(7);
    std::printf("  shared_ptr operator* (lecture)        : %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) sink = *shared; }));

    std::printf("  copy_on_write as_mutable (non partage): %8.1f ms\n",
        time_ms([&] { for (long i = 0; i < iterations; ++i) ++cow_value.as_mutable(); }));

    threadsafe::asynchronous_task_launcher launcher;
    constexpr int scoped_tasks = 2000;
    std::printf("launch_scoped_task x%d (spawn+join/appel): %8.1f ms\n", scoped_tasks,
        time_ms([&] { for (int i = 0; i < scoped_tasks; ++i) launcher.launch_scoped_task([] {}); }));

    return 0;
}
```

**Résultats** :

```
increment verrouille, 1 thread, 5000000 iterations:
  synchronized_value<long>::lock()      :     46.1 ms
  std::mutex + long brut                :     24.7 ms
  std::shared_mutex + long brut         :     31.7 ms
lecture, 1 thread, 5000000 iterations:
  synchronized_value lock_shared()      :     31.5 ms
  copy_on_write operator* (lecture)     :      1.3 ms
  shared_ptr operator* (lecture)        :      1.2 ms
  copy_on_write as_mutable (non partage):      1.8 ms
launch_scoped_task x2000 (spawn+join/appel):     31.9 ms
```

Analyse :

- **`synchronized_value` : +87 % vs `std::mutex` brut** (46,1 vs 24,7 ms ≈ 9,2 vs 4,9 ns/op). Deux composantes : le `shared_mutex` systématique (bug #1 ; `shared_mutex` brut = 31,7 ms, soit +28 % structurels) et le guard (allocation du `unique_lock` + pointeur, ~14 ms résiduels — en partie incompressible, en partie inlining). Le correctif de la section 1 récupère mécaniquement la part `shared_mutex` pour tous les `T` non const-synchronizables. À contention nulle c'est 4–9 ns/op : parfaitement acceptable ; à signaler seulement en boucle chaude.
- **`copy_on_write` : zéro surcoût mesurable** vs `shared_ptr` en lecture (1,3 vs 1,2 ms), et `as_mutable` non partagé coûte un chargement atomique relaxed (1,8 ms, ~0,4 ns/op). Excellent — c'est le helper le plus efficace de la bibliothèque.
- **`launch_scoped_task` : ~16 µs par appel** (spawn + join d'un thread OS par tâche, zéro parallélisme par construction puisque l'appelant joint immédiatement). C'est un outil de *démonstration du borrow scoped*, pas d'exécution : à documenter. De même, `launch_task` crée un thread OS par tâche et le `std::vector<std::jthread>` croît sans borne : pas de moissonnage des tâches finies, pas de `join()` explicite (tout attend le destructeur). Suggestions : une note « éducatif, pas un pool », une méthode `join_all()`, et/ou une borne.
- Pas d'atomics superflus, pas de copies cachées ni d'allocations inattendues dans `synchronized_value`/`copy_on_write` (le `make()` via `make_shared` est une seule allocation, bloc de contrôle compris).

---

## 8. Recommandations récapitulatives

1. **Corriger** `synchronized_value` (section 1 — fichier complet fourni) : membre `mutable mutex mutex_;` et `value_guard` prenant `typename Lock::mutex_type&`. Corrige à la fois la compilation du chemin `std::mutex` et le surcoût `shared_mutex`.
2. **Corriger** `copy_on_write::as_mutable` (section 3 — fichier complet fourni) : fence acquire avant la mutation en place.
3. **Ajouter un TU de tests qui instancie les corps runtime** (appelle réellement `lock()`, `lock_shared()`, `as_mutable()`, `launch_task` pour plusieurs `T`, dont un non const-synchronizable) — le bug #1 était invisible pour une suite 100 % `static_assert`, y compris pour les `requires`-expressions qui ne voient que les signatures.
4. **Ajouter `apply()`** (section 2) : réduit à la fois la surface d'évasion de référence et le motif TOCTOU ; documenter l'auto-deadlock et le verrouillage multiple.
5. **CI Linux avec `-fsanitize=thread`** : sur macOS/arm64, Homebrew GCC n'a pas de `libtsan` et aucun clang ne compile la réflexion — la vraie bibliothèque n'y est pas TSan-able.

Tous les scénarios, shims et binaires de cet audit sont dans le scratchpad de session (`.../scratchpad/tsan/`, `.../scratchpad/fixed/`), hors du dépôt.
