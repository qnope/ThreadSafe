# Audit 10 — Robustesse des quatre traits

Audit adversarial de `is_synchronizable<T>`, `is_synchronizable<const T>`, `is_lifetime_aware<T>` et `is_sendable<T>`.
Compilateur : g++-16 (Homebrew GCC 16.2.0), `-std=c++2c -freflection`. Le build de référence du dépôt passe intégralement avant audit.

Méthode : pour chaque trait, cas positifs (doivent être acceptés), cas négatifs (doivent être rejetés), cas piégeux (unions et unions anonymes, bitfields, membres `mutable`, types récursifs, types incomplets, héritage, lambdas, opérateurs de conversion, état statique). Tous les scénarios ci-dessous ont été compilés ; le résultat (accepté/rejeté, conforme ou non) est indiqué à chaque fois.

## Synthèse des verdicts

| # | Sujet | Verdict | Résumé |
|---|-------|---------|--------|
| 1 | `is_synchronizable<T>` (écriture concurrente) | OK | Comportement conforme sur tous les cas testés |
| 2 | `is_synchronizable<const T>` — « const derrière une indirection » | OK | La règle est appliquée partout : `const T*`, `const T&`, `reference_wrapper<const T>`, `string_view`, `span<const T>`, membres imbriqués, bases, `mutable`, unions |
| 3 | `is_lifetime_aware<T>` — transitivité | OK | Transitif via membres, bases, unions, conteneurs, lambdas à capture par référence |
| 4 | `is_sendable<T>` — règle `T&`/`T*` et types std | OK | `is_sendable<T&> == is_sendable<T*> == is_synchronizable<T>` vérifié ; `shared_ptr<const T>` correctement rejeté |
| 5 | État statique mutable accepté | PROBLEME | `static inline std::vector<int>` dans une classe → la classe reste sendable et const-synchronizable ; course de données possible sans `const_cast` ni `mutable`. Correctif complet fourni et validé contre la suite de tests |
| 6 | Types récursifs possédants | PROBLEME | `struct Node { std::unique_ptr<Node> next; }` et `struct Tree { std::vector<Tree> children; }` provoquent une erreur de compilation dure sur les trois traits, alors que ces types sont sûrs et très courants |
| 7 | `is_lifetime_aware<T[]>` incohérent | PROBLEME | `is_lifetime_aware<int*[]> == true` alors que `is_lifetime_aware<int*[3]> == false` : spécialisation `T[]` manquante. Correctif fourni |
| 8 | Primitives de synchronisation std non couvertes | SUGGESTION | `is_synchronizable<std::mutex>`, `std::shared_mutex`, `std::atomic_flag`, `std::condition_variable` valent `false` : une classe utilisateur à `mutable std::mutex` ne peut jamais être const-synchronizable |
| 9 | Lambdas capturantes toutes rejetées | SUGGESTION | GCC 16 ne réfléchit pas les captures ; `has_unreflectable_state` rejette même `[x]{}` (sûr mais très restrictif — à documenter pour la conférence) |
| 10 | `is_sendable<std::mutex> == true` | SUGGESTION | Vrai par analyse structurelle des internes pthread ; inoffensif (type non déplaçable) mais fragile et dépendant de la plateforme |
| 11 | Sync ⇒ Send câblé en dur | SUGGESTION | `default_is_sendable` retourne `true` dès que le type est synchronizable ; contrairement à Rust (où `MutexGuard` est `Sync` mais pas `Send`). Aucun type de la bibliothèque n'exploite la faille aujourd'hui, mais `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` rend un type sendable en silence |

---

## 1–2. `is_synchronizable<T>` et `is_synchronizable<const T>` — verdict OK

Scénario complet (`audit1_sync_const.cpp`), **accepté par le compilateur, conforme aux attentes** :

```cpp
// Audit trait 1 & 2: is_synchronizable<T> et is_synchronizable<const T>
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using threadsafe::is_synchronizable;

namespace {

struct Plain { int x; double y; };
struct WithMutable { mutable int cache; };
struct WithMutableAtomic { mutable std::atomic<int> cache; };
struct WithAtomic { std::atomic<int> counter; };
struct WithConstPtr { const int* p; };
struct WithPtrConst { int* const p; };
struct WithRef { const int& r; };
struct WithRefWrapperConst { std::reference_wrapper<const int> r; };
struct WithView { std::string_view sv; };
struct WithSpanConst { std::span<const int> sp; };
struct WithString { std::string s; };
struct Base { int b; };
struct Derived : Base { int d; };
struct BadBase { mutable int m; };
struct DerivedBad : BadBase { int d; };
union U { int i; float f; };
union UMut { mutable int i; float f; };
struct WithBitfield { int a : 3; unsigned b : 5; };
struct Empty {};

// --- is_synchronizable<T> (écriture concurrente) : presque rien ne passe ---
static_assert(!is_synchronizable<int>);
static_assert(!is_synchronizable<Plain>);
static_assert(!is_synchronizable<int*>);
static_assert(is_synchronizable<std::atomic<int>>);
static_assert(!is_synchronizable<std::atomic<int*>>); // pointee int non sync
static_assert(is_synchronizable<std::atomic<std::atomic<int>*>>);
static_assert(is_synchronizable<void(int)>); // type fonction = code
static_assert(!is_synchronizable<std::string>);
static_assert(!is_synchronizable<std::mutex>);        // surprenant ? cf. suggestion n°8
static_assert(!is_synchronizable<std::shared_mutex>); // idem

// tableaux
static_assert(is_synchronizable<std::atomic<int>[4]>);
static_assert(!is_synchronizable<int[4]>);

// --- is_synchronizable<const T> : lecture thread-safe ---
static_assert(is_synchronizable<const int>);
static_assert(is_synchronizable<const Plain>);
static_assert(is_synchronizable<const Empty>);
static_assert(is_synchronizable<const int[4]>);
static_assert(is_synchronizable<const WithBitfield>);
static_assert(is_synchronizable<const U>);
static_assert(is_synchronizable<const Derived>);
static_assert(is_synchronizable<const std::string>);
static_assert(is_synchronizable<const std::vector<int>>);

// const derrière une indirection : jamais fiable
static_assert(!is_synchronizable<const int*>);          // ptr non-const partagé
static_assert(!is_synchronizable<const int* const>);    // pointee int pas full-sync
static_assert(is_synchronizable<std::atomic<int>* const>); // pointee full-sync -> ok
static_assert(!is_synchronizable<const WithConstPtr>);  // membre const int*
static_assert(!is_synchronizable<const WithPtrConst>);  // membre int* const
static_assert(!is_synchronizable<const WithRef>);       // membre const int&
static_assert(!is_synchronizable<const WithRefWrapperConst>);
static_assert(!is_synchronizable<const std::reference_wrapper<const int>>);
static_assert(!is_synchronizable<const WithView>);      // string_view = ptr caché
static_assert(!is_synchronizable<const WithSpanConst>);
static_assert(!is_synchronizable<const std::vector<int*>>);
static_assert(!is_synchronizable<const std::vector<const int*>>);
static_assert(!is_synchronizable<const std::unique_ptr<int>>); // operator* -> int&
static_assert(!is_synchronizable<const std::shared_ptr<int>>);
static_assert(is_synchronizable<const std::shared_ptr<std::atomic<int>>>);

// mutable défait const
static_assert(!is_synchronizable<const WithMutable>);
static_assert(is_synchronizable<const WithMutableAtomic>);
static_assert(!is_synchronizable<const DerivedBad>);  // via la base
static_assert(!is_synchronizable<const UMut>);        // union à membre mutable

// membre atomic non-mutable : lecture seule via const -> ok
static_assert(is_synchronizable<const WithAtomic>);

// fonction pointeur const : code
static_assert(is_synchronizable<void(*const)(int)>);
static_assert(!is_synchronizable<void(*)(int)>); // le pointeur lui-même est mutable

// cv combinés
static_assert(is_synchronizable<const volatile int>);
static_assert(!is_synchronizable<volatile int>);

}
int main() {}
```

Points remarquables, tous corrects :
- « const derrière une indirection n'est jamais fiable » est appliqué à chaque forme demandée par la mission : `const T*`, `const T&`, `std::reference_wrapper<const T>`, membres pointeurs/références const, `span<const T>`, `string_view`. `string_view` et `span` sont rejetés doublement : leur pointeur membre exige la pleine synchronisabilité du pointee, et leurs constructeurs templates déclenchent le garde `may_hijack_copy_move`.
- `mutable` défait bien `const` (y compris via une base et dans une union) et exige le trait complet — ce qui permet le beau cas `mutable std::atomic<int>` accepté.
- La branche pointeur passe avant la branche scalaire dans `default_is_const_synchronizable` : un pointeur (qui est scalaire) n'est jamais blanchi par le cas scalaire.
- `int* const[3]`, `const volatile T`, pointeurs de fonction, unions anonymes : tous traités correctement.

## 3. `is_lifetime_aware<T>` — verdict OK (hors problèmes n°6 et n°7)

Scénario complet (`audit2_lifetime.cpp`), **accepté, conforme** :

```cpp
// Audit trait 3: is_lifetime_aware<T> — transitivité de l'ownership
#include <threadsafe/threadsafe.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using threadsafe::is_lifetime_aware;

namespace {

struct Owner { int x; std::string s; };
struct HoldsPtr { int* p; };
struct HoldsRef { int& r; };
struct HoldsConstRef { const std::string& r; };
struct HoldsRefWrap { std::reference_wrapper<int> r; };
struct HoldsView { std::string_view v; };
struct HoldsSpan { std::span<int> sp; };
struct HoldsUnique { std::unique_ptr<int> p; };
struct HoldsShared { std::shared_ptr<int> p; };
struct HoldsWeak { std::weak_ptr<int> p; };
struct HoldsVecPtr { std::vector<int*> v; };
struct DerivesPtr : HoldsPtr { int y; };
struct DerivesOwner : Owner {};
struct HoldsFnPtr { void (*f)(int); };
union UOwn { int i; float f; };
union UPtr { int* p; long l; };
struct SelfShared { std::shared_ptr<SelfShared> next; }; // pas de récursion: spec inconditionnelle

// positifs
static_assert(is_lifetime_aware<int>);
static_assert(is_lifetime_aware<Owner>);
static_assert(is_lifetime_aware<DerivesOwner>);
static_assert(is_lifetime_aware<std::string>);
static_assert(is_lifetime_aware<std::vector<std::string>>);
static_assert(is_lifetime_aware<HoldsUnique>);
static_assert(is_lifetime_aware<HoldsShared>);
static_assert(is_lifetime_aware<HoldsWeak>);
static_assert(is_lifetime_aware<HoldsFnPtr>); // pointeur de fonction = code, pas de durée de vie
static_assert(is_lifetime_aware<UOwn>);
static_assert(is_lifetime_aware<SelfShared>);
static_assert(is_lifetime_aware<int[4]>);
static_assert(is_lifetime_aware<std::unique_ptr<int>[3]>);

// négatifs (emprunts)
static_assert(!is_lifetime_aware<int*>);
static_assert(!is_lifetime_aware<int&>);
static_assert(!is_lifetime_aware<int&&>);
static_assert(!is_lifetime_aware<int* const>);
static_assert(!is_lifetime_aware<const int*>);
static_assert(!is_lifetime_aware<std::reference_wrapper<int>>);
static_assert(!is_lifetime_aware<std::string_view>);
static_assert(!is_lifetime_aware<std::span<int>>);
static_assert(!is_lifetime_aware<HoldsPtr>);
static_assert(!is_lifetime_aware<HoldsRef>);
static_assert(!is_lifetime_aware<HoldsConstRef>);
static_assert(!is_lifetime_aware<HoldsRefWrap>);
static_assert(!is_lifetime_aware<HoldsView>);
static_assert(!is_lifetime_aware<HoldsSpan>);
static_assert(!is_lifetime_aware<HoldsVecPtr>);
static_assert(!is_lifetime_aware<DerivesPtr>); // transitif via la base
static_assert(!is_lifetime_aware<UPtr>);       // union avec pointeur
static_assert(!is_lifetime_aware<int*[3]>);
static_assert(!is_lifetime_aware<std::vector<int*>>);
static_assert(!is_lifetime_aware<std::vector<std::string_view>>);

// lambdas
inline void lambda_checks() {
    int local = 0;
    auto by_value = [local] { return local; };
    auto by_ref = [&local] { return local; };
    auto capture_ptr = [p = &local] { return *p; };
    auto captureless = [] { return 42; };
    static_assert(is_lifetime_aware<decltype(captureless)>);
    // Les captures de closure ne sont pas réfléchies par GCC 16 :
    // has_unreflectable_state rejette TOUTE lambda capturante, même par valeur.
    static_assert(!is_lifetime_aware<decltype(by_value)>); // sûr mais restrictif
    static_assert(!is_lifetime_aware<decltype(by_ref)>);
    static_assert(!is_lifetime_aware<decltype(capture_ptr)>);
}

// std::function : état caché -> rejeté
static_assert(!is_lifetime_aware<std::function<void()>>);

}
int main() {}
```

La transitivité est correcte dans tous les cas demandés : membres, héritage, `unique_ptr`/`shared_ptr`/`weak_ptr` (specs inconditionnelles à `true` pour `shared_ptr`/`weak_ptr`, ce qui casse d'ailleurs la récursion pour `SelfShared` — bien vu), pointeurs bruts, références, conteneurs de pointeurs, lambdas à capture par référence.

## 4. `is_sendable<T>` — verdict OK (hors points 5, 6, 9, 10, 11)

Scénario complet (`audit3_sendable.cpp`), **accepté, conforme** :

```cpp
// Audit trait 4: is_sendable<T> — règle T&/T* == synchronizable, callables, types std
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

namespace {

struct Plain { int x; std::string s; };
struct WithPtr { int* p; };
struct WithRef { int& r; };
struct WithConstRef { const int& r; };
struct WithAtomicPtr { std::atomic<int>* p; };
struct Greedy { template <class U> Greedy(U&&) {} int x = 0; };
struct GreedyAssign { template <class U> GreedyAssign& operator=(U&&); int x; };
struct UserCopy { UserCopy(const UserCopy&); int x; };
struct DefaultedCopy { DefaultedCopy(const DefaultedCopy&) = default; int x; };
struct DeletedCopy { DeletedCopy(const DeletedCopy&) = delete; int x; };
struct PolyOk { virtual ~PolyOk() = default; int x; };
union UPtr { int* p; long l; };
union UVal { int i; float f; };
struct WithBitfield { int a : 3; };
struct WithMutableMember { mutable int m; }; // envoyé par valeur: mutable sans effet
struct SelfRawPtr { SelfRawPtr* next; };

// règle fondamentale: refs/pointeurs = synchronizable du référent
static_assert(is_sendable<int&> == is_synchronizable<int>);
static_assert(is_sendable<int*> == is_synchronizable<int>);
static_assert(!is_sendable<int&>);
static_assert(!is_sendable<const int&>);   // const derrière indirection non fiable
static_assert(!is_sendable<const int*>);
static_assert(!is_sendable<int&&>);
static_assert(is_sendable<std::atomic<int>&>);
static_assert(is_sendable<std::atomic<int>*>);
static_assert(is_sendable<void(*)(int)>);  // pointeur de fonction

// valeurs simples
static_assert(is_sendable<int>);
static_assert(is_sendable<const int>);
static_assert(is_sendable<volatile int>);
static_assert(is_sendable<Plain>);
static_assert(is_sendable<UVal>);
static_assert(is_sendable<WithBitfield>);
static_assert(is_sendable<WithMutableMember>);
static_assert(is_sendable<PolyOk>);
static_assert(is_sendable<int[4]>);
static_assert(is_sendable<const int[4]>);

// structurel: indirections membres
static_assert(!is_sendable<WithPtr>);
static_assert(!is_sendable<WithRef>);
static_assert(!is_sendable<WithConstRef>);
static_assert(is_sendable<WithAtomicPtr>);
static_assert(!is_sendable<UPtr>);
static_assert(!is_sendable<SelfRawPtr>);

// détournement copie/move
static_assert(!is_sendable<Greedy>);
static_assert(!is_sendable<GreedyAssign>);
static_assert(!is_sendable<UserCopy>);      // copie utilisateur = comportement inconnu
static_assert(is_sendable<DefaultedCopy>);
static_assert(is_sendable<DeletedCopy>);

// types std
static_assert(is_sendable<std::string>);
static_assert(is_sendable<std::vector<int>>);
static_assert(!is_sendable<std::vector<int*>>);
static_assert(is_sendable<std::vector<std::vector<std::string>>>);
static_assert(is_sendable<std::unique_ptr<int>>);
static_assert(is_sendable<std::unique_ptr<int[]>>);
static_assert(!is_sendable<std::unique_ptr<PolyOk>>); // polymorphe non-final: type dynamique inconnu
static_assert(!is_sendable<std::shared_ptr<int>>);        // partage -> il faut sync
static_assert(!is_sendable<std::shared_ptr<const int>>);  // const non fiable
static_assert(is_sendable<std::shared_ptr<std::atomic<int>>>);
static_assert(!is_sendable<std::weak_ptr<int>>);
static_assert(is_sendable<std::weak_ptr<std::atomic<int>>>);
static_assert(!is_sendable<std::reference_wrapper<int>>);
static_assert(!is_sendable<std::reference_wrapper<const int>>);
static_assert(is_sendable<std::reference_wrapper<std::atomic<int>>>);
static_assert(!is_sendable<std::function<void()>>); // état caché
static_assert(is_sendable<std::stop_token>);

// callables
inline void lambda_checks() {
    int local = 0;
    std::atomic<int> counter{0};
    auto captureless = [] {};
    auto by_value = [local] { return local; };
    auto by_ref = [&local] { return local; };
    auto mutable_by_value = [local]() mutable { return ++local; };
    auto by_ref_atomic = [&counter] { counter.fetch_add(1); };
    auto by_shared = [p = std::make_shared<int>(0)] { return *p; };
    static_assert(is_sendable<decltype(captureless)>);
    static_assert(!is_sendable<decltype(by_ref)>);            // attendu: rejet
    // Captures non réfléchies par GCC 16 -> tout est rejeté, même le sûr:
    static_assert(!is_sendable<decltype(by_value)>);
    static_assert(!is_sendable<decltype(mutable_by_value)>);
    static_assert(!is_sendable<decltype(by_ref_atomic)>);     // serait sûr en Rust
    static_assert(!is_sendable<decltype(by_shared)>);
}

}
int main() {}
```

Valeurs mesurées sur les primitives systèmes (programme `probe_values.cpp`) :

```
mutex send=1  thread send=0  jthread send=0  condvar send=0
const-mutex sync=1  shared_mutex send=1  atomic_flag send=1
atomic_flag sync=0  once_flag send=1
```

`std::thread`/`std::jthread` non sendables (le `pthread_t` interne est un pointeur vers un type au référent non synchronizable) : plus strict que le `JoinHandle: Send` de Rust, mais sûr. Voir aussi les suggestions n°8 et n°10.

---

## 5. PROBLEME — L'état statique mutable est invisible des traits (trou de soundness)

### Démonstration

Scénario (`audit9_escapes.cpp`), **accepté par le compilateur alors qu'il permet une course de données**, sans `const_cast`, sans `mutable`, sans variable globale hors classe :

```cpp
#include <threadsafe/threadsafe.h>
#include <vector>
using namespace threadsafe;

struct StaticState {
    static inline std::vector<int> cache;      // état partagé entre TOUTES les instances
    int x;
    void add() const { cache.push_back(x); }   // écriture via une méthode const
};

static_assert(is_sendable<StaticState>);                 // passe (course possible)
static_assert(is_synchronizable<const StaticState>);     // passe (course possible)
```

Deux threads recevant chacun leur copie d'un `StaticState` (parfaitement autorisé par `is_sendable`) et appelant `add()` écrivent le même `std::vector` sans synchronisation : comportement indéfini. Contrairement aux méthodes touchant une vraie variable globale (hors de portée de tout trait sur le type), **les membres statiques sont des données de la classe, énumérables par la réflexion** — le trait peut et devrait les vérifier.

### Code en cause

Les deux marches structurelles ne parcourent que les membres non statiques. Extrait complet concerné de `include/threadsafe/details/sendable.h` :

```cpp
inline consteval bool default_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_sendable_type(unqualified);

    if (is_synchronizable_type(type) || is_scalar_type(type))
        return true;

    if (is_void_type(type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        throw exception(
            u8"is_sendable<T> supports only scalar, class and union types",
            type);

    if (!is_complete_type(type))
        throw exception(
            u8"is_sendable<T> requires a complete type — specialize is_sendable "
            u8"for types holding a pointer to an incomplete type (the pimpl "
            u8"idiom)",
            type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            return false;

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_sendable_type(remove_cv(type_of(member))))
            return false;

    return true;
}
```

et de `include/threadsafe/details/synchronizable.h` :

```cpp
inline consteval bool default_is_const_synchronizable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    type = remove_cv(type);

    if (is_synchronizable_type(type))
        return true;

    // A pointee's const is a view restriction, not an object property — the
    // object may be written through another alias, so the full trait is asked.
    // A function pointee is code, and code is synchronizable.
    if (is_pointer_type(type))
        return is_synchronizable_type(remove_cv(remove_pointer(type)));

    if (is_scalar_type(type))
        return true;

    if (is_void_type(type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        throw exception(
            u8"is_synchronizable<const T> supports only scalar, class and "
            u8"union types", type);

    if (!is_complete_type(type))
        throw exception(
            u8"is_synchronizable<const T> requires a complete type — "
            u8"specialize is_synchronizable for types holding a pointer to an "
            u8"incomplete type (the pimpl idiom)", type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_synchronizable_type(add_const(type_of(base))))
            return false;

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = type_of(member);
        if (is_mutable_member(member)) {
            // mutable defeats const: this member is writable through a const&, so it
            // needs the full (write-safe) trait, not the const one.
            if (!is_synchronizable_type(remove_cv(member_type)))
                return false;
        } else if (is_reference_type(member_type)) {
            // a reference member's constness is unrelated to the referent's; the
            // referent may be shared and mutated through another alias.
            if (!is_synchronizable_type(remove_cvref(member_type)))
                return false;
        } else if (!is_synchronizable_type(add_const(member_type))) {
            return false; // ordinary value member: const propagates normally.
        }
    }

    return true;
}
```

### Correctif complet (validé)

Règle : un membre statique est partagé par toutes les instances sur tous les threads, quel que soit le trait interrogé. Un statique non-const doit donc être pleinement synchronizable ; un statique const doit être const-synchronizable. (`static const int` et `static constexpr` restent acceptés ; `static inline std::vector<int>` est rejeté ; `static std::atomic<int>` reste accepté.)

`include/threadsafe/details/synchronizable_base.h` corrigé, fichier complet :

```cpp
#pragma once

#include <cstddef>
#include <meta>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
constexpr bool is_synchronizable = false;

template <class T, std::size_t N>
constexpr bool is_synchronizable<T[N]> = is_synchronizable<T>;
template <class T>
constexpr bool is_synchronizable<T[]> = is_synchronizable<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_synchronizable<T>, for code written on the reflection side.
inline consteval bool is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable, type);
}

namespace detail {

// A static data member is shared by every instance on every thread, whatever
// the trait being asked about the instances. A non-const one must therefore be
// fully synchronizable; a const one must be read-safe.
inline consteval bool has_only_synchronizable_statics(std::meta::info type) {
    using namespace std::meta;
    const auto context = access_context::unchecked();
    for (info member : members_of(type, context)) {
        if (!is_variable(member))
            continue;
        const auto member_type = type_of(member);
        if (is_const(member_type)) {
            if (!is_synchronizable_type(member_type))
                return false;
        } else if (!is_synchronizable_type(remove_cv(member_type))) {
            return false;
        }
    }
    return true;
}

}

}

#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)         \
    template <>                                              \
    inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
```

Puis, dans `default_is_sendable` (sendable.h) **et** `default_is_const_synchronizable` (synchronizable.h), la garde devient — seule ligne modifiée dans chacun des deux fichiers reproduits ci-dessus :

```cpp
    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type)
        || !has_only_synchronizable_statics(type))
        return false;
```

### Validation du correctif

Compilé avec les en-têtes patchés (`audit10_fix_check.cpp`), **accepté** :

```cpp
#include <threadsafe/threadsafe.h>
#include <vector>
using namespace threadsafe;
namespace {
struct StaticState { static inline std::vector<int> cache; int x; };
struct StaticConst { static const int limit = 3; int x; };
struct StaticConstexpr { static constexpr double ratio = 1.5; int x; };
static_assert(!is_sendable<StaticState>);
static_assert(!is_synchronizable<const StaticState>);
static_assert(is_sendable<StaticConst>);
static_assert(is_synchronizable<const StaticConst>);
static_assert(is_sendable<StaticConstexpr>);
static_assert(is_synchronizable<const StaticConstexpr>);
}
int main() {}
```

Les **dix fichiers de tests du dépôt** ainsi que les scénarios d'audit 1 à 3 compilent tous sans erreur avec les en-têtes corrigés : le correctif ne casse aucun comportement existant.

Remarque honnête sur la portée : ce correctif ferme l'évasion « état statique de classe » ; une méthode touchant une variable globale libre (hors classe) reste indétectable — c'est la limite inhérente d'un modèle qui analyse les données et pas le code. Un `static thread_local` (par-thread, donc sûr) serait rejeté à tort par ce correctif si la réflexion ne permet pas de le distinguer ; c'est le prix conservateur.

---

## 6. PROBLEME — Les types récursifs possédants font échouer la compilation

### Démonstration

Deux scénarios, **rejetés par une erreur dure** (ni `true` ni `false` : le trait lui-même ne compile pas), alors que les types sont sûrs, sendables moralement, et omniprésents (listes, arbres) :

`audit4_recursion.cpp` :

```cpp
#include <threadsafe/threadsafe.h>
#include <memory>
namespace {
struct Node { int value; std::unique_ptr<Node> next; };
static_assert(threadsafe::is_sendable<Node>);
}
int main() {}
```

```
smart_pointers.h:17:5: error: the value of 'threadsafe::is_sendable<Node>'
                       is not usable in a constant expression
```

`audit6_vector_recursion.cpp` :

```cpp
#include <threadsafe/threadsafe.h>
#include <vector>
namespace {
struct Tree { int v; std::vector<Tree> children; };
static_assert(threadsafe::is_sendable<Tree>);
}
int main() {}
```

```
containers.h:23:49: error: the value of 'threadsafe::is_sendable<Tree>'
                    is not usable in a constant expression
```

Même échec avec `is_lifetime_aware<Node>` (`audit5_recursion_lifetime.cpp`). En revanche `struct SelfShared { std::shared_ptr<SelfShared> next; }` ne boucle pas pour `is_lifetime_aware` (spec inconditionnelle) ni pour `is_sendable` (le primaire de `is_synchronizable` vaut `false` sans récursion) — la boucle n'existe que sur les chemins qui redemandent le même trait pour le même type : `is_sendable<unique_ptr<Node>> → is_sendable<Node> → …` et `is_sendable<vector<Tree>> → is_sendable<Tree> → …`.

### Cause

Extrait concerné de `include/threadsafe/details/smart_pointers.h` :

```cpp
template <class T, class D>
constexpr bool is_sendable<std::unique_ptr<T, D>> =
    is_sendable<std::remove_all_extents_t<T>> && is_sendable<D>
    && detail::dynamic_type_is_known<std::remove_all_extents_t<T>>;

template <class T, class D>
constexpr bool is_lifetime_aware<std::unique_ptr<T, D>> =
    is_lifetime_aware<std::remove_all_extents_t<T>> && is_lifetime_aware<D>;
```

et de `include/threadsafe/details/containers.h` :

```cpp
template <class T, class A>
constexpr bool is_sendable<std::vector<T, A>> = is_sendable<T> && is_sendable<A>;
```

L'instanciation de la variable template `is_sendable<Node>` exige la valeur de `is_sendable<std::unique_ptr<Node>>`, qui exige la valeur de `is_sendable<Node>` — la même entité, encore en cours d'instanciation : le compilateur diagnostique une expression non constante.

### Pourquoi il n'y a pas de correctif simple, et le contournement

Rust résout ce cas par **coinduction** (un cycle est présumé vrai pendant sa propre évaluation). Des variables templates `constexpr` évaluées par consteval ne disposent d'aucun état de « calcul en cours » observable : on ne peut ni mémoïser ni détecter le cycle depuis la bibliothèque avec les outils actuels de `<meta>`. Trois options honnêtes :

1. **Documenter** le contournement (recommandé pour une bibliothèque pédagogique) : l'utilisateur affirme le trait pour son type récursif, ce qui coupe le cycle **avant** la première utilisation :

```cpp
struct Node;
template <>
inline constexpr bool threadsafe::is_sendable<Node> = true;
template <>
inline constexpr bool threadsafe::is_lifetime_aware<Node> = true;

struct Node { int value; std::unique_ptr<Node> next; };
```

(vérifié : compile et donne les bonnes valeurs — mais c'est une affirmation non vérifiée, du même niveau de confiance que `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE`).

2. Fournir un macro dédié (`THREADSAFE_ASSERT_SENDABLE(...)`) sur le modèle du macro existant, pour rendre le contournement discipliné et cherchable.

3. À plus long terme, si `<meta>` gagne un mécanisme d'annotation ou d'état de traduction, implémenter une vraie détection de cycle.

Au minimum, le message d'erreur actuel (« not usable in a constant expression », pointant dans les en-têtes) ne guide pas l'utilisateur ; c'est un vrai défaut de robustesse pour du code de conférence.

---

## 7. PROBLEME — `is_lifetime_aware<T[]>` : spécialisation manquante, réponse incohérente

### Démonstration

Mesuré (`audit8_tricky.cpp`) : `is_lifetime_aware<int*[]> == true` alors que `is_lifetime_aware<int*[3]> == false`. Un tableau de bornes inconnues d'éléments empruntants est déclaré « owner ». Le cas est marginal (un `T[]` n'est jamais membre), mais `is_sendable` et `is_synchronizable` ont tous deux la paire `T[N]`/`T[]` — `is_lifetime_aware` est le seul trait où elle manque, et le chemin par défaut (`!is_class && !is_union → true`) blanchit alors le tableau.

### Code en cause

Fichier complet `include/threadsafe/details/lifetime_aware.h` (version actuelle) :

```cpp
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

namespace detail {
consteval bool default_is_lifetime_aware(std::meta::info type);
}

template <class T>
constexpr bool is_lifetime_aware = detail::default_is_lifetime_aware(^^T);

template <class T>
constexpr bool is_lifetime_aware<T&> = false;
template <class T>
constexpr bool is_lifetime_aware<T&&> = false;
template <class T>
constexpr bool is_lifetime_aware<T*> = false;

template <class F>
    requires std::is_function_v<F>
constexpr bool is_lifetime_aware<F*> = true;

template <class T, std::size_t N>
constexpr bool is_lifetime_aware<T[N]> = is_lifetime_aware<std::remove_cv_t<T>>;

template <class T>
constexpr bool is_lifetime_aware<std::reference_wrapper<T>> = false;

template <class T>
constexpr bool is_lifetime_aware<std::shared_ptr<T>> = true;
template <class T>
constexpr bool is_lifetime_aware<std::weak_ptr<T>> = true;

template <class T>
concept lifetime_aware = is_lifetime_aware<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_lifetime_aware<T>, for code written on the reflection side.
inline consteval bool is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware, type);
}

namespace detail {

inline consteval bool default_is_lifetime_aware(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_lifetime_aware_type(unqualified);

    if (trait_value(^^std::ranges::borrowed_range, type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        return true;

    if (!is_complete_type(type))
        throw exception(
            u8"is_lifetime_aware<T> requires a complete type", type);

    if (has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_lifetime_aware_type(type_of(base)))
            return false;

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_lifetime_aware_type(remove_cv(type_of(member))))
            return false;

    return true;
}

}

}
```

### Correctif complet (validé)

Ajout de la spécialisation `T[]`, symétrique de `T[N]` ; le reste du fichier est inchangé :

```cpp
template <class T, std::size_t N>
constexpr bool is_lifetime_aware<T[N]> = is_lifetime_aware<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_lifetime_aware<T[]> = is_lifetime_aware<std::remove_cv_t<T>>;
```

Validation (`audit11_unbounded.cpp`), **accepté avec les en-têtes corrigés** :

```cpp
#include <threadsafe/threadsafe.h>
static_assert(!threadsafe::is_lifetime_aware<int*[]>);
static_assert(threadsafe::is_lifetime_aware<int[]>);
int main() {}
```

La suite de tests complète du dépôt compile toujours avec ce correctif.

---

## 8. SUGGESTION — Couvrir les primitives de synchronisation de la bibliothèque standard

Mesuré : `is_synchronizable<std::mutex>`, `std::shared_mutex`, `std::atomic_flag`, `std::condition_variable` valent tous `false`. Conséquence concrète : le motif canonique « classe à verrou interne »

```cpp
class Counter {
    mutable std::mutex m_;
    int value_ = 0;
public:
    int get() const { std::lock_guard lock{m_}; return value_; }
    void increment() { std::lock_guard lock{m_}; ++value_; }
};
```

ne pourra jamais être `is_synchronizable<const Counter>` : la branche `mutable` exige `is_synchronizable<std::mutex>`, qui est `false`. Or se faire verrouiller depuis plusieurs threads est précisément la fonction d'un mutex (en Rust, `Mutex<T>: Sync`). Suggestion — dans l'esprit de la spec `std::atomic<T>` existante :

```cpp
template <>
inline constexpr bool is_synchronizable<std::mutex> = true;
template <>
inline constexpr bool is_synchronizable<std::recursive_mutex> = true;
template <>
inline constexpr bool is_synchronizable<std::shared_mutex> = true;
template <>
inline constexpr bool is_synchronizable<std::atomic_flag> = true;
template <>
inline constexpr bool is_synchronizable<std::condition_variable> = true;
template <>
inline constexpr bool is_synchronizable<std::condition_variable_any> = true;
template <>
inline constexpr bool is_synchronizable<std::once_flag> = true;
```

Nuance assumée : `Counter` resterait non-synchronizable par défaut (le trait complet de `Counter` exigerait de prouver que *tout* accès passe par le verrou, ce que le modèle structurel ne peut pas voir) — mais l'utilisateur qui affirme `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Counter)` obtiendrait alors un modèle cohérent, et les compositions à base d'atomiques + mutex `mutable` cesseraient d'être rejetées pour la mauvaise raison.

## 9. SUGGESTION — Lambdas capturantes : tout est rejeté (limitation à documenter)

Vérifié directement contre la réflexion (`probe_closure.cpp`, **accepté**) :

```cpp
#include <meta>
int main() {
    int local = 0;
    auto by_value = [local] { return local; };
    constexpr auto n = std::meta::nonstatic_data_members_of(
        ^^decltype(by_value), std::meta::access_context::unchecked()).size();
    static_assert(n == 0); // GCC 16 ne réfléchit pas les captures
}
```

GCC 16 n'expose pas les captures comme membres ; `has_unreflectable_state` (type non vide, non polymorphe, sans bases ni membres visibles) rejette donc **toute** lambda capturante — y compris `[x]{}`, `[p = std::make_shared<int>(0)]{}` et `[&counter]` sur un `std::atomic`, tous sûrs et tous `Send` en Rust. C'est le bon choix (sûr par défaut), et le garde fait exactement son travail ; mais pour une bibliothèque de conférence, la conséquence pratique mérite d'être dite explicitement : **avec `asynchronous_task_launcher`, on passe une lambda sans capture et les données en arguments** — c'est d'ailleurs le style que les signatures `launch_task(F f, Args... args)` encouragent. À documenter, et à réévaluer quand la réflexion des closures arrivera.

## 10. SUGGESTION — `is_sendable<std::mutex> == true` par accident structurel

La marche structurelle traverse `std::mutex` jusqu'aux internes pthread (tous scalaires sur cette plateforme) et conclut `true`. Inoffensif aujourd'hui : `std::mutex` n'est ni copiable ni déplaçable, donc aucun `launch_task` ne peut en recevoir un par valeur. Mais la valeur du trait dépend des internes de la libc de la plateforme (un `pthread_mutex_t` contenant un pointeur donnerait `false` ailleurs), et un mutex **verrouillé** traversant un thread serait UB s'il devenait transportable. Une spec explicite fixerait la sémantique au lieu de la subir :

```cpp
template <>
inline constexpr bool is_sendable<std::mutex> = false;
```

(ou `true` si l'on suit Rust — l'important est que ce soit un choix écrit, pas un accident de représentation.)

## 11. SUGGESTION — Documenter « Sync ⇒ Send »

`default_is_sendable` retourne `true` dès `is_synchronizable_type(type)`. En Rust, `Sync` n'implique pas `Send` (`MutexGuard` est l'exemple canonique : partageable, mais son unlock doit se faire sur le thread propriétaire). Dans ThreadSafe, aucun type fourni n'exploite la faille (`value_guard` a une spec `is_sendable = false` **et** n'est pas synchronizable — cohérent), mais tout `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(X)` rend silencieusement `X` sendable par ricochet. Le macro « unsafe » devrait le dire dans sa documentation : *affirmer Sync, c'est aussi affirmer Send*.

---

## Annexe — scénarios piégeux restants (tous conformes)

`audit8_tricky.cpp` (compilé, valeurs mesurées à l'exécution puis re-vérifiées en `static_assert`) :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
using namespace threadsafe;
namespace {
struct AnonUnion { union { int i; int* p; }; };          // union anonyme avec pointeur
struct AnonStructUnion { union { struct { int* a; } s; long l; }; };
struct WithDtor { ~WithDtor() {} int x; };
struct WithNoexceptDtor { ~WithNoexceptDtor() = default; int x; };
struct RefToAtomic { std::atomic<int>& r; };
struct Unsafe { int* p; };
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Unsafe);
namespace {
static_assert(is_sendable<RefToAtomic>);
static_assert(is_synchronizable<const RefToAtomic>);
static_assert(is_sendable<Unsafe>);        // Sync => Send automatique (cf. n°11)
static_assert(is_sendable<Unsafe&>);
static_assert(is_synchronizable<const Unsafe>);
}
int main() {
    printf("AnonUnion send=%d AnonStructUnion send=%d WithDtor send=%d "
           "DefaultedDtor send=%d lifetime<int*[]>=%d lifetime<int*[3]>=%d\n",
        (int)is_sendable<AnonUnion>, (int)is_sendable<AnonStructUnion>,
        (int)is_sendable<WithDtor>, (int)is_sendable<WithNoexceptDtor>,
        (int)is_lifetime_aware<int*[]>, (int)is_lifetime_aware<int*[3]>);
}
```

Sortie : `AnonUnion send=0 AnonStructUnion send=0 WithDtor send=0 DefaultedDtor send=1 lifetime<int*[]>=1 lifetime<int*[3]>=0`.

- Les unions anonymes contenant des pointeurs **sont** vues par la réflexion et rejetées — pas de trou.
- Un destructeur utilisateur bloque `is_sendable` (conservateur, cohérent avec la règle « copies/moves/destroy par défaut uniquement »).
- Les deux dernières valeurs sont le problème n°7.
- Le pimpl à pointeur brut (`struct P { struct Impl* p; }`) donne proprement `false` partout sans erreur ; le pimpl à `unique_ptr<Impl>` donne l'erreur dure **voulue** avec le message pédagogique « requires a complete type — specialize … », vérifié (`audit7_pimpl.cpp`).
- Opérateurs de conversion, méthodes retournant des références : hors de portée d'un modèle qui analyse les données — impossible sans `const_cast`, `mutable`, statique (n°5) ou variable globale ; les trois premiers sont couverts, le dernier est la limite assumée du modèle.

Fichiers de scénarios : `audit1_sync_const.cpp` … `audit11_unbounded.cpp`, `probe_closure.cpp`, `probe_values.cpp`, `probe_static.cpp` dans le scratchpad de session ; en-têtes patchés validés dans `fixed-include/` (suite de tests du dépôt : 10/10 fichiers compilés sans erreur avec les correctifs n°5 et n°7 appliqués ensemble).
