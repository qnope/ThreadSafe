# Audit 11 — Robustesse des helpers (`copy_on_write`, `synchronized_value`, `asynchronous_task_launcher`)

Audit réalisé avec g++-16 (Homebrew GCC 16.2.0), `-std=c++2c -freflection`, sur la base du build de référence CMake (suite de tests du dépôt : 100 % verte avant audit).

## Synthèse des verdicts

| Sujet | Verdict | Description |
|---|---|---|
| `copy_on_write` — sémantique COW (détachement, partage, copie/move) | OK | Vérifiée à l'exécution : pas de copie quand unique, copie quand partagé, l'alias garde l'ancienne valeur. |
| `copy_on_write` — T non copiable | OK | `as_mutable()` disparaît par contrainte (SFINAE), le handle reste lisible. |
| `copy_on_write` — traits | OK | `is_sendable` exige `is_sendable<T> && is_synchronizable<const T>` ; jamais synchronizable ; lifetime transitive. Cohérent et sûr. |
| `copy_on_write` — état après move | SUGGESTION | Un handle move-from a un `ptr_` nul ; `operator*`/`as_mutable()` sur ce handle est un déréférencement nul (UB), silencieux car `noexcept`. |
| `synchronized_value` — mutex incohérent | **PROBLEME** | `mutex_` et le constructeur de `value_guard` codent en dur `std::shared_mutex` ; pour tout `T` non const-synchronizable (la branche `std::mutex` !), `lock()` et `lock_shared()` **ne compilent pas**. Le type est inutilisable exactement dans le cas que la branche devait servir. |
| `synchronized_value` — traits, guards, API const | OK | Guards ni copiables, ni movables, ni sendables ; `lock()` refusé sur objet const ; `shared_ptr<synchronized_value>` est la forme sendable/lifetime-aware. |
| `synchronized_value` — fuite de référence | SUGGESTION (limitation connue) | `int& r = *sv.lock();` compile : la référence survit au guard, accès non protégé. Documenté dans le code ; une API à callable fermerait le trou. |
| `synchronized_value` — deadlocks | SUGGESTION (limitation) | Self-lock (`lock()` imbriqué) et lecture-puis-écriture dans le même thread compilent et bloquent à l'exécution. Indétectable à la compilation dans ce modèle. |
| `asynchronous_task_launcher` — rejets de sûreté | OK | Tous les scénarios unsafe demandés sont rejetés à la compilation : capture par référence (locale ou globale), capture d'un `T*` vers non-sync, `shared_ptr<T>` avec T non-sync, lambda mutable capturante, arguments non sendables, `reference_wrapper` vers non-sync, `std::function`. |
| `asynchronous_task_launcher` — résultat non-sendable | OK (par construction) | `std::jthread` jette la valeur de retour sur le thread qui l'a produite : aucun résultat ne traverse une frontière de thread, aucune contrainte n'est nécessaire. |
| `asynchronous_task_launcher` — invocabilité non contrainte | SUGGESTION | La clause `requires` ne vérifie pas que `f(args...)` est appelable : un appel invalide passe la contrainte puis explose au fond de `std::jthread` avec un diagnostic illisible. |

---

## 1. `copy_on_write<T>`

### 1.1 Sémantique COW — vérifiée à l'exécution (scénario s1)

Fichier : `scratchpad/s1_cow_semantics.cpp` — **compile et s'exécute sans échec d'assertion**.

```cpp
// Sémantique COW à l'exécution : as_mutable copie quand partagé, pas quand unique.
#include <threadsafe/threadsafe.h>
#include <cassert>
#include <string>
#include <utility>

using threadsafe::copy_on_write;

int main() {
    // 1. Unique : as_mutable ne copie pas (adresse stable).
    copy_on_write<std::string> unique_handle{"hello"};
    const std::string* address_before = &*unique_handle;
    unique_handle.as_mutable() += " world";
    assert(&*unique_handle == address_before);
    assert(*unique_handle == "hello world");

    // 2. Partagé par copie : as_mutable détache, l'autre copie garde l'ancienne valeur.
    copy_on_write<std::string> original{"shared"};
    copy_on_write<std::string> alias = original;
    assert(&*original == &*alias);
    original.as_mutable() = "changed";
    assert(&*original != &*alias);
    assert(*alias == "shared");
    assert(*original == "changed");

    // 3. Détachement en chaîne : après détachement, alias devient unique -> plus de copie.
    const std::string* alias_address = &*alias;
    alias.as_mutable() += "!";
    assert(&*alias == alias_address);

    // 4. Copie-assignation repartage le bloc.
    alias = original;
    assert(&*alias == &*original);

    // 5. Move : le bloc suit le nouveau propriétaire sans copie du T.
    const std::string* moved_from_address = &*original;
    copy_on_write<std::string> moved_to = std::move(original);
    assert(&*moved_to == moved_from_address);

    // 6. Lecture const partagée : operator*/-> renvoient bien du const.
    const copy_on_write<std::string>& const_view = moved_to;
    assert(const_view->size() == (*const_view).size());

    return 0;
}
```

Résultat : **accepté, toutes les assertions passent** (comportement attendu).

### 1.2 Traits et API — vérifiés à la compilation (scénario s2)

Fichier : `scratchpad/s2_cow_traits.cpp` — **compile** (tous les `static_assert` passent, comme attendu).

```cpp
// Interactions traits / API de copy_on_write, tout en compile-time.
#include <threadsafe/threadsafe.h>
#include <concepts>
#include <memory>
#include <string>

using threadsafe::copy_on_write;
using threadsafe::is_lifetime_aware;
using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

namespace {
struct MutCache {
    int raw;
    mutable int cached; // int n'est pas synchronizable => casse la lecture const partagée
};

template <class C>
constexpr bool can_detach = requires(C c) { c.as_mutable(); };
}

// T non copiable : construction possible, as_mutable indisponible (SFINAE, pas d'erreur dure).
static_assert(std::constructible_from<copy_on_write<std::unique_ptr<int>>,
                                      std::unique_ptr<int>>);
static_assert(!can_detach<copy_on_write<std::unique_ptr<int>>>);
static_assert(can_detach<copy_on_write<std::string>>);

// Copie/move : le type reste copiable et movable (sémantique de handle).
static_assert(std::copy_constructible<copy_on_write<std::string>>);
static_assert(std::movable<copy_on_write<std::string>>);

// La copie n'est pas détournée par le constructeur variadique.
static_assert(std::constructible_from<copy_on_write<int>, copy_on_write<int>&>);

// Traits : sendable ssi T sendable ET const T lisible en parallèle.
static_assert(is_sendable<copy_on_write<std::string>>);
static_assert(!is_sendable<copy_on_write<MutCache>>);
// cv-uniformité : la forme const doit répondre pareil.
static_assert(is_sendable<const copy_on_write<std::string>>);
static_assert(!is_sendable<const copy_on_write<MutCache>>);

// Jamais synchronizable : as_mutable rebinde le handle, un objet = un thread.
static_assert(!is_synchronizable<copy_on_write<std::string>>);
// Et pas non plus en const partagé : as_mutable d'une copie soeur ne protege pas
// un lecteur du meme objet ; la bibliothèque reste conservatrice.
static_assert(!is_synchronizable<const copy_on_write<std::string>>);

// Lifetime : le bloc partagé possède le T, mais la possession est transitive.
static_assert(is_lifetime_aware<copy_on_write<std::string>>);
static_assert(!is_lifetime_aware<copy_on_write<int*>>);

int main() {}
```

Résultat : **accepté** (comportement attendu). Le modèle est sain : le détachement par `use_count() != 1` est fiable parce que `copy_on_write` n'est pas synchronizable — un autre thread ne peut détenir le bloc qu'à travers sa *propre* copie, comptée dans `use_count`.

### 1.3 SUGGESTION — handle move-from

Après `auto b = std::move(a);`, `a.ptr_` est nul ; `*a`, `a->` (tous deux `noexcept`) et `a.as_mutable()` déréférencent un pointeur nul. C'est le contrat habituel « moved-from = seulement assignable/destructible », acceptable pour une bibliothèque pédagogique, mais mérite d'être dit dans la documentation du type. Aucun changement de code exigé ; si on veut durcir, il suffit de supprimer le move (`copy_on_write(copy_on_write&&) = delete;`) — la copie d'un `shared_ptr` est bon marché — au prix de la perte de la sémantique move.

---

## 2. `synchronized_value<T>` — **PROBLEME confirmé** : mutex codé en dur

### 2.1 Le défaut

`get_mutex_type()` choisit `std::mutex` quand `is_synchronizable<const T>` est faux (T à membre `mutable` non synchronisé, par exemple). Mais deux endroits ignorent ce choix :

- le membre `mutex_` est déclaré `mutable std::shared_mutex mutex_;` au lieu du type calculé `mutex` ;
- le constructeur de `value_guard` prend `std::shared_mutex&` en dur, alors que `Lock` peut être `std::unique_lock<std::mutex>`.

Conséquence : pour tout `T` sendable mais non const-synchronizable, `lock()` et `lock_shared()` **ne compilent pas** (`std::unique_lock<std::mutex>` inconstructible depuis un `std::shared_mutex&`). La branche `std::mutex` est donc morte : le test du dépôt `test_synchronized_value.cpp` vérifie bien que `synchronized_value<Memo>::const_guard` est `value_guard<const Memo, std::unique_lock<std::mutex>>`, mais aucun test n'appelle `lock()` sur ce type — le trou est passé sous le radar.

### 2.2 Code problématique complet (`include/threadsafe/details/synchronized_value.h`, version actuelle)

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

### 2.3 Scénario de reproduction (s3) — échec de compilation constaté

Fichier : `scratchpad/s3_sync_memo_lock.cpp`. Ce fichier est un usage **légitime** et devrait compiler.

```cpp
// Usage NORMAL de synchronized_value avec un T sendable mais non
// const-synchronizable (membre mutable non atomique) : DOIT compiler.
#include <threadsafe/threadsafe.h>

struct Memo {
    int key;
    mutable int cached;
};

int main() {
    threadsafe::synchronized_value<Memo> guarded_memo{Memo{1, 0}};
    {
        auto exclusive_guard = guarded_memo.lock();
        exclusive_guard->cached = 42;
    }
    {
        auto read_guard = guarded_memo.lock_shared();
        (void)read_guard->key;
    }
    return 0;
}
```

Résultat sur la version actuelle : **rejeté** (inattendu — c'est le bug) :

```text
synchronized_value.h:36:11: error: no matching function for call to
    'std::unique_lock<std::mutex>::unique_lock(std::shared_mutex&)'
   36 |         : lock_(mutex), value_(&value) {}
```

### 2.4 Solution complète (fichier corrigé intégral)

Deux changements : le constructeur de `value_guard` prend `typename Lock::mutex_type&` (le type de mutex que `Lock` sait verrouiller — `std::unique_lock` et `std::shared_lock` exposent tous deux `mutex_type`), et `mutex_` utilise l'alias `mutex` calculé. Le reste du fichier est inchangé.

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

Validation effectuée : avec ce fichier corrigé (overlay d'include), le scénario s3 **compile et s'exécute**, et les 11 fichiers de tests du dépôt **compilent tous à l'identique** (aucune régression de `static_assert`).

### 2.5 Traits, guards et API const — OK (couverts par la suite du dépôt, re-vérifiés)

Les tests existants de `tests/test_synchronized_value.cpp` couvrent déjà : guards non sendables / non lifetime-aware / non copiables / non movables, `lock()` refusé sur `const synchronized_value&`, `shared_ptr<synchronized_value>` accepté par `launch_task`, guard refusé par `launch_task`, choix `shared_lock` vs `unique_lock`. Tout est confirmé par le build de référence.

### 2.6 Limitations non détectables à la compilation (scénario s4)

Fichier : `scratchpad/s4_sync_limits.cpp` — **compile** (c'est le constat : ces dangers passent).

```cpp
// Limites de synchronized_value que le compilateur N'ATTRAPE PAS.
// Ce fichier COMPILE : chaque bloc est un danger runtime non détecté.
#include <threadsafe/threadsafe.h>

using sync_int = threadsafe::synchronized_value<int>;

// 1. Fuite de référence : le guard temporaire meurt au point-virgule,
//    la référence pointe vers l'intérieur, déverrouillé.
int& leak_reference(sync_int& value) {
    return *value.lock(); // compile — accès non protégé ensuite
}

// 2. Fuite de pointeur, même mécanique.
int* leak_pointer(sync_int& value) {
    return value.lock().operator->();
}

// 3. Self-deadlock : deux lock() imbriqués sur le même objet dans le même
//    thread — std::shared_mutex n'est pas récursif.
void self_deadlock(sync_int& value) {
    auto first_guard = value.lock();
    auto second_guard = value.lock(); // compile — bloque à l'exécution
    (void)*first_guard;
    (void)*second_guard;
}

// 4. lock_shared puis lock dans le même thread : deadlock aussi.
void upgrade_deadlock(sync_int& value) {
    auto read_guard = value.lock_shared();
    auto write_guard = value.lock(); // compile — bloque à l'exécution
    (void)*read_guard;
    (void)*write_guard;
}

int main() {}
```

Résultat : **accepté** (attendu : ce sont des limites assumées). Le commentaire du code source assume le compromis « guard » contre « callable ». Suggestion si l'on veut fermer la fuite de référence sans perdre le guard : ajouter en complément une API à callable, qui ne peut rien laisser fuir de plus que ce que l'utilisateur écrit explicitement :

```cpp
    template <class F>
        requires std::invocable<F, T&>
    decltype(auto) with_lock(F&& f) {
        auto exclusive_guard = lock();
        return std::forward<F>(f)(*exclusive_guard);
    }

    template <class F>
        requires std::invocable<F, const T&>
    decltype(auto) with_lock_shared(F&& f) const {
        auto read_guard = lock_shared();
        return std::forward<F>(f)(*read_guard);
    }
```

Les deadlocks (3, 4), eux, ne sont pas exprimables dans le système de traits : ils dépendent du flot d'exécution, pas des types. À signaler dans la documentation du type (comme le fait Rust pour `Mutex`).

---

## 3. `asynchronous_task_launcher`

### 3.1 Rejets de sûreté — tous conformes (scénario s5)

Fichier : `scratchpad/s5_launcher_rejections.cpp` — **compile** : chaque `static_assert(!can_launch...)` prouve que la contrainte rejette le cas.

```cpp
// Rejets attendus de asynchronous_task_launcher, prouvés via requires
// (la contrainte échoue => can_launch_task est false => le fichier compile).
#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <string>

namespace {
struct NonSync {
    int value;
    NonSync(const NonSync&); // copie utilisateur => non structurel, non sendable
};

struct PlainCounter {
    int hits; // sendable mais PAS synchronizable
};

template <class F, class... Args>
constexpr bool can_launch_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_task(f, args...);
    };

template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_scoped_task(f, args...);
    };

std::string global_state;
int global_int;
}

// Lambda capturant par référence un non-synchronizable : refusé.
static_assert(!can_launch_task<decltype([&s = global_state] { s += "x"; })>);
// Même par référence à un int : toute lambda capturante est refusée
// (état de closure non réflexible => conservateur).
static_assert(!can_launch_task<decltype([&i = global_int] { ++i; })>);
// Capture par valeur d'un T* vers un non-synchronizable : refusé.
static_assert(!can_launch_task<decltype([p = &global_state] { *p += "x"; })>);
// Lambda mutable capturant un état : refusé.
static_assert(!can_launch_task<decltype([n = 0]() mutable { ++n; })>);

// Argument T* vers non-synchronizable : refusé (non sendable ET non lifetime-aware).
static_assert(!can_launch_task<decltype([](std::string*) {}), std::string*>);
static_assert(!can_launch_scoped_task<decltype([](std::string*) {}),
                                      std::string*>);
// shared_ptr<T> avec T non synchronizable : refusé (partage sans synchronisation).
static_assert(!can_launch_task<decltype([](std::shared_ptr<PlainCounter>) {}),
                               std::shared_ptr<PlainCounter>>);
static_assert(!can_launch_scoped_task<
              decltype([](std::shared_ptr<PlainCounter>) {}),
              std::shared_ptr<PlainCounter>>);
// Argument non sendable (copie utilisateur) : refusé même scoped.
static_assert(!can_launch_task<decltype([](NonSync) {}), NonSync>);
static_assert(!can_launch_scoped_task<decltype([](NonSync) {}), NonSync>);
// reference_wrapper vers non-sync : refusé partout.
static_assert(!can_launch_task<decltype([](std::string&) {}),
                               std::reference_wrapper<std::string>>);
static_assert(!can_launch_scoped_task<decltype([](std::string&) {}),
                                      std::reference_wrapper<std::string>>);
// std::function : état interne non synchronisé, refusé.
static_assert(!can_launch_task<std::function<void()>>);

// Contrôles positifs : le mécanisme n'est pas trivialement fermé.
static_assert(can_launch_task<decltype([](int) {}), int>);
static_assert(can_launch_task<decltype([](std::string) {}), std::string>);

int main() {}
```

Résultat : **accepté** — tous les rejets sont effectifs, et les contrôles positifs prouvent que le filtre n'est pas dégénéré (il ne refuse pas tout). Point notable : *toute* lambda capturante est refusée, y compris une capture par valeur inoffensive (`[x = 42]`), parce que l'état d'une closure n'est pas réflexible (`has_unreflectable_state`). C'est conservateur donc **sûr** ; le coût ergonomique est assumé par la bibliothèque (tests du dépôt à l'appui).

### 3.2 Rejets « durs » — échec de compilation constaté (scénarios s6 et s7)

Fichier : `scratchpad/s6_launcher_hardfail_refcapture.cpp` — **rejeté par le compilateur, comme attendu** (capture d'une locale par référence : à la fois non sendable et non lifetime-aware).

```cpp
// DOIT ÉCHOUER À COMPILER : capture d'une locale par référence.
#include <threadsafe/threadsafe.h>
#include <string>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::string local_text = "dangling soon";
    launcher.launch_task([&local_text] { local_text += "!"; });
}
```

Fichier : `scratchpad/s7_launcher_hardfail_sharedptr.cpp` — **rejeté par le compilateur, comme attendu** (`is_sendable<shared_ptr<T>>` exige `is_synchronizable<T>`, faux pour `std::string`).

```cpp
// DOIT ÉCHOUER À COMPILER : shared_ptr vers un T non-synchronizable.
#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    auto shared_text = std::make_shared<std::string>("racy");
    launcher.launch_task([](std::shared_ptr<std::string> text) { *text += "!"; },
                         shared_text);
}
```

### 3.3 Résultat non-sendable — OK par construction

`launch_task` retourne `void` et `std::jthread` détruit la valeur de retour du callable **sur le thread qui l'a produite** : aucun résultat ne traverse jamais de frontière de thread, donc aucune contrainte sur le type de retour n'est nécessaire. Vérifié par le scénario s8 ci-dessous (le callable retournant un type non sendable est accepté, et c'est correct).

### 3.4 SUGGESTION — l'invocabilité n'est pas contrainte (scénario s8)

Fichier : `scratchpad/s8_launcher_invocable_hole.cpp` — **compile** : la contrainte accepte une paire (callable, arguments) inappelable.

```cpp
// La contrainte de launch_task ne vérifie PAS l'invocabilité :
// le requires passe, l'erreur explose plus tard dans std::jthread.
#include <threadsafe/threadsafe.h>
#include <string>

namespace {
template <class F, class... Args>
constexpr bool can_launch_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_task(f, args...);
    };
}

// Une lambda sans paramètre avec un argument en trop : la contrainte accepte.
static_assert(can_launch_task<decltype([] {}), int>,
              "trou de contrainte : accepté par requires alors que l'appel réel "
              "échoue au fond de std::jthread");

// Un résultat non-sendable est accepté : le retour est jeté sur le thread
// même qui l'a produit (jthread ignore la valeur), donc rien ne traverse.
struct NonSendableResult {
    NonSendableResult(const NonSendableResult&);
    NonSendableResult() = default;
};
static_assert(can_launch_task<decltype([] { return NonSendableResult{}; })>);

int main() {}
```

Et l'appel réel correspondant (`scratchpad/s8b_hard.cpp`) échoue, mais avec un diagnostic profond dans libstdc++ au lieu d'un échec de contrainte lisible :

```cpp
#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([] {}, 42); // arg en trop : erreur profonde attendue
}
```

```text
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/thread: In instantiation of
'static std::thread std::jthread::_S_create(std::stop_source&, _Callable&&, _Args&& ...)
 [with _Callable = main()::<lambda()>; _Args = {int}]': ...
```

Ce n'est **pas** un trou de sûreté (l'appel invalide ne compile jamais), mais l'erreur est illisible et `can_launch_task` ment. Solution complète, calquée sur le contrat de `std::jthread` (qui essaie `f(stop_token, args...)` puis `f(args...)`) — fichier `asynchronous_task_launcher.h` corrigé intégral :

```cpp
#pragma once

#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

namespace detail {
// Le contrat d'appel de std::jthread : f(stop_token, args...) sinon f(args...).
template <class F, class... Args>
concept jthread_invocable =
    std::is_invocable_v<std::decay_t<F>, std::stop_token, std::decay_t<Args>...>
    || std::is_invocable_v<std::decay_t<F>, std::decay_t<Args>...>;
}

class asynchronous_task_launcher {
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");

public:
    template <typename F, typename... Args>
        requires sendable<F>
              && lifetime_aware<F>
              && (sendable<Args> && ...)
              && (lifetime_aware<Args> && ...)
              && detail::jthread_invocable<F, Args...>
    void launch_task(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }

    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it does
    // not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
    template <typename F, typename... Args>
        requires sendable<F>
              && (sendable<Args> && ...)
              && detail::jthread_invocable<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }

private:
    std::vector<std::jthread> threads_;
};

}
```

Remarque annexe (hors sûreté) : `launch_scoped_task` construit le `jthread` puis le `join()` immédiatement — l'appelant bloque pendant toute la tâche, il n'y a donc aucun parallélisme. C'est cohérent avec son rôle pédagogique (montrer pourquoi une référence peut traverser quand le lancement est borné), mais mérite d'être dit explicitement dans sa documentation.

---

## 4. Récapitulatif des scénarios

| Scénario | Attendu | Constaté |
|---|---|---|
| s1 — sémantique COW (runtime) | accepté, assertions vertes | accepté, assertions vertes ✔ |
| s2 — traits/API de `copy_on_write` | accepté | accepté ✔ |
| s3 — `lock()` sur `synchronized_value<Memo>` | accepté | **rejeté** ✘ → **BUG** ; accepté après correctif, suite du dépôt inchangée ✔ |
| s4 — fuite de référence / self-deadlock | accepté (limites connues) | accepté ✔ |
| s5 — rejets du launcher via `requires` | accepté (tous les `!can_launch...`) | accepté ✔ |
| s6 — capture d'une locale par référence | rejet de compilation | rejeté ✔ |
| s7 — `shared_ptr<std::string>` vers une tâche | rejet de compilation | rejeté ✔ |
| s8 — invocabilité non contrainte | accepté (démonstration du trou) | accepté ✔ (erreur profonde confirmée sur l'appel réel) |

Sources des scénarios : `/private/tmp/claude-501/-Users-amorrier-Programmation-ThreadSafe/59a30249-5e60-4d37-b21d-45c50a17e3cb/scratchpad/` (`s1_cow_semantics.cpp`, `s2_cow_traits.cpp`, `s3_sync_memo_lock.cpp`, `s4_sync_limits.cpp`, `s5_launcher_rejections.cpp`, `s6_launcher_hardfail_refcapture.cpp`, `s7_launcher_hardfail_sharedptr.cpp`, `s8_launcher_invocable_hole.cpp`, `s8b_hard.cpp`) ; header corrigé validé dans `include-fixed/threadsafe/details/synchronized_value.h` (même répertoire).
