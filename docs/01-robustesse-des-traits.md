# Robustesse des traits

Chaque trait a été attaqué individuellement, puis chaque défaut trouvé a été
soumis à un vérificateur indépendant chargé de le **réfuter**. Le tableau final
est nettement plus favorable que le premier jet : sur six failles annoncées
« critiques », **une seule survit** à la vérification. Les autres sont réelles
mais moins graves, ou déjà assumées par le code.

| # | défaut | trait | verdict | sévérité | nature |
|---|---|---|---|---|---|
| 1 | `std::stop_source` / `std::stop_token` déclarés synchronizable | `is_synchronizable<T>` | confirmé | **critique** | bug |
| 2 | une méthode `const` écrit là où la marche ne voit rien | `is_synchronizable<const T>` | confirmé | **élevée** | **inhérent** |
| 3 | `const unique_ptr<T,D>` sans garde de type dynamique | `is_synchronizable<const T>` | confirmé | **élevée** | bug |
| 4 | `shared_ptr` casse la transitivité de la possession | `is_lifetime_aware<T>` | confirmé, surévalué | **élevée** | bug |
| 5 | le garde copie/déplacement exclut `chrono`, `complex`, `bitset` | `is_sendable<T>` | confirmé, surévalué | moyenne | délibéré |
| 6 | `const unique_ptr` avec deleter non possédant | `is_synchronizable<const T>` | confirmé, surévalué | moyenne | délibéré |
| 7 | la règle `borrowed_range` rejette des vues qui n'empruntent pas | `is_lifetime_aware<T>` | confirmé, surévalué | moyenne | bug |

---

## 1. `std::stop_source` / `std::stop_token` — critique, confirmée

`vocabulary.h` lignes 69-72 :

```cpp
template <>
struct is_synchronizable<std::stop_token> : std::true_type {};
template <>
struct is_synchronizable<std::stop_source> : std::true_type {};
```

`is_synchronizable<T>` signifie, d'après `CLAUDE.md`, « un `T` peut être utilisé
depuis plusieurs threads **en même temps** » — écriture comprise. Or
[stoptoken.general] ne promet l'absence de course que pour `request_stop`,
`stop_requested` et `stop_possible`. Les deux types sont des poignées à
comptage de références dont **l'affectation** touche l'état partagé.

La bibliothèque enchaîne alors ses propres règles jusqu'à bénir le partage :

```
vocabulary.h:70-72    is_synchronizable<stop_source> : true_type
smart_pointers.h:27   is_sendable<shared_ptr<T>> : is_synchronizable<remove_cv_t<T>>
sendable.h:24-29      is_sendable<T&>/<T*> : is_synchronizable<remove_cv_t<T>>
```

### Le programme — accepté par la bibliothèque, il corrompt le tas

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <stop_token>

static_assert(threadsafe::is_synchronizable_v<std::stop_source>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<std::stop_source>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<std::stop_source>>);

void hammer(std::shared_ptr<std::stop_source> shared_source) {
    std::stop_source a, b;
    for (int i = 0; i < 2'000'000; ++i)
        *shared_source = (i & 1) ? a : b;
}

int main() {
    for (int round = 0; round < 20; ++round) {
        auto shared_source = std::make_shared<std::stop_source>();
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(&hammer, shared_source);
        launcher.launch_task(&hammer, shared_source);
    }
    std::printf("survived\n");
}
```

Aucun `const_cast`, aucun `new`/`delete` brut, aucun `THREADSAFE_UNSAFE_ASSERT_*`,
aucun thread écrit à la main : uniquement `make_shared`, `launch_task` et une
affectation à travers `shared_ptr::operator*`.

Observé, trois exécutions sur trois — le programme n'imprime **jamais**
`survived` :

```
run 1: exit=133      run 2: exit=133      run 3: exit=139
```

```
BUG IN CLIENT OF LIBMALLOC: not an allocated block
  libsystem_malloc.dylib  mfm_free
  hammer(std::shared_ptr<std::stop_source>)
  std::thread::_State_impl<...>::_M_run()
```

Sous ThreadSanitizer, deux rapports : une course sur le pointeur d'état, et une
course sur le compteur de références de `_Stop_state`.

### Pourquoi c'est un bug et non un compromis

Aucun test n'affirme la forme non qualifiée, aucun commentaire ne la défend. Les
deux assertions délibérées du dépôt portent sur d'autres questions :
`is_sendable_v<std::stop_token>` (`test_soundness_regressions.cpp:157`) et
`is_synchronizable_v<const std::stop_token>`. Ces deux-là sont **justes** : envoyer
une poignée est une copie, la lire à travers `const` est sans course. C'est la
forme non qualifiée — celle qui bénit l'écriture à travers une référence
partagée — qui est fausse.

Les autres spécialisations non qualifiées résistent, elles, à l'examen : les types
fonction sont du code ; `is_synchronizable<std::atomic<T>>` tient parce que
l'affectation d'un `atomic` est un store atomique ; `synchronized_value<T>` tient
parce que ses copie et affectation sont `= delete`. `stop_source` et `stop_token`
sont **les deux seuls types bénis dont l'affectation publique écrit un état
partagé**.

### Correctif — vérifié, les 11 TU passent

Remplacer les lignes 69-72 de `include/threadsafe/details/vocabulary.h` par :

```cpp
// [stoptoken.general] promises only that request_stop, stop_requested and
// stop_possible are race-free. Both types are refcounted handles whose copy
// assignment and swap touch the shared state's reference count, so the
// unqualified trait -- which blesses writing through a shared `T&` -- must stay
// false. Sending a handle to another thread is a copy, and reading one through
// const is race-free, so those two are stated directly.
template <>
struct is_sendable<std::stop_token> : std::true_type {};
template <>
struct is_sendable<std::stop_source> : std::true_type {};

template <>
struct is_synchronizable<const std::stop_token> : std::true_type {};
template <>
struct is_synchronizable<const std::stop_source> : std::true_type {};
```

Après correctif : les 11 TU compilent à l'identique, et le programme ci-dessus est
**rejeté à la compilation**.

---

## 2. Une méthode `const` est un écrivain que la marche ne peut pas voir — élevée, **inhérente**

C'est le résultat le plus important de l'audit, et il n'a pas de correctif.

`is_synchronizable<const T>` n'inspecte que les **données**. Un type sans membre
`mutable`, sans référence, sans pointeur, avec uniquement des membres spéciaux
implicites, est accepté — et une méthode `const` peut malgré tout écrire une
mémoire que l'objet ne contient pas.

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>

namespace {

int slab[64];

struct SlabHandle {
    int index;

    void bump() const { ++slab[index]; }
    int read() const { return slab[index]; }
};

}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<SlabHandle>);
static_assert(is_synchronizable_v<const SlabHandle>);
static_assert(std::is_same_v<threadsafe::synchronized_value<SlabHandle>::mutex,
                             std::shared_mutex>);

int main() {
    threadsafe::synchronized_value<SlabHandle> shared_handle{SlabHandle{7}};

    auto hammer = [&shared_handle] {
        for (int iteration = 0; iteration < 20000; ++iteration) {
            const auto reader_guard = shared_handle.lock_shared();
            reader_guard->bump();
        }
    };

    std::thread first_reader{hammer};
    std::thread second_reader{hammer};
    first_reader.join();
    second_reader.join();

    const auto reader_guard = shared_handle.lock_shared();
    std::printf("slab[7] = %d (expected 40000)\n", reader_guard->read());
    return 0;
}
```

Toutes les assertions tiennent, `synchronized_value` choisit donc un
`std::shared_mutex`, et `lock_shared()` laisse les deux lecteurs entrer ensemble :

```
WARNING: ThreadSanitizer: data race
  Read of size 4  ... by thread T2 (mutexes: read M0)
  Previous write of size 4 ... by thread T1 (mutexes: read M0)
  Location is global '(anonymous namespace)::slab'
ThreadSanitizer: reported 2 warnings
```

`mutexes: read M0` sur **les deux** threads : TSan confirme que le mutex de la
bibliothèque était bien tenu, en mode partagé, par les deux coureurs.

### Pourquoi il n'y a pas de correctif

C'est exactement le point où l'analogie avec Rust cesse d'être vraie, et c'est ce
qui en fait le meilleur passage possible d'une conférence sur ce sujet :

> `Sync` est sûr en Rust parce que `&T` **interdit** la mutation sans `UnsafeCell`.
> En C++, `const` n'interdit rien : une méthode `const` peut écrire n'importe
> quelle mémoire que l'objet ne possède pas.

La réflexion voit des déclarations, pas des corps de fonctions. Aucune version
de ce trait ne peut fermer ce trou. La recommandation n'est donc pas un
changement de code, mais une **phrase à ajouter** dans `CLAUDE.md` et en tête de
`synchronizable.h`, disant ce que le trait prouve et ce qu'il ne prouve pas :

> `is_synchronizable<const T>` vérifie que rien **dans l'objet** n'offre de chemin
> d'écriture. Il suppose — sans pouvoir le vérifier — que les méthodes `const` de
> `T` sont des lecteurs. Un `T` dont une méthode `const` écrit un état global,
> ou une mémoire atteinte par un indice ou une poignée, échappe au trait.

Ce que la marche *peut* voir, en revanche, tient : les règles `mutable`,
référence et pointeur ont résisté à huit formes structurelles distinctes
(unions anonymes, membres privés, bases virtuelles, héritage multiple), et les
règles `const` explicites des conteneurs standard ont résisté à quatre threads
martelant leurs méthodes `const` sous TSan.

---

## 3. `const std::unique_ptr<T,D>` : la garde de type dynamique manque — élevée, confirmée

`smart_pointers.h` applique `detail::dynamic_type_is_known` à
`is_sendable<std::unique_ptr<T,D>>` (ligne 16-19) mais **pas** à
`is_synchronizable<const std::unique_ptr<T,D>>`, 25 lignes plus bas :

```cpp
template <class T, class D>
struct is_synchronizable<const std::unique_ptr<T, D>>
    : std::bool_constant<is_synchronizable_v<std::remove_all_extents_t<T>>
                         && is_synchronizable_v<const D>> {};
```

`unique_ptr` est la **seule** indirection de la bibliothèque qui fasse confiance au
`const` du pointé : `shared_ptr`, `weak_ptr`, `reference_wrapper` et les branches
pointeur/référence de la marche `const` retirent tous les cv et posent le trait
complet, donc sont gardées par construction.

Le trou : `is_synchronizable_v<const PolyBase>` vaut **vrai** parce que le vptr
n'est pas un membre réfléchi — `has_unreflectable_state` exclut explicitement les
types polymorphes — donc la marche voit une classe vide. Un dérivé avec un membre
`mutable` passe alors sans être vu.

### Correctif minimal — vérifié, les 11 TU passent

```cpp
template <class T, class D>
struct is_synchronizable<const std::unique_ptr<T, D>>
    : std::bool_constant<is_synchronizable_v<std::remove_all_extents_t<T>>
                         && is_synchronizable_v<const D>
                         && detail::dynamic_type_is_known<
                                std::remove_cv_t<std::remove_all_extents_t<T>>>> {};
```

`remove_cv_t` est nécessaire parce que `T` est ici typiquement `const Report`.
Vérifié : ne sur-rejette pas — `const unique_ptr<const PolyFinal>`,
`<const int>`, `<const int[]>`, `<const Plain>` restent vrais.

Le commentaire du test existant (`test_smart_pointers.cpp`), qui affirme « the same
alias-free assumption the sendable rule makes », est à corriger en même temps :
la règle `sendable` fait une hypothèse de **plus**, précisément celle-ci.

---

## 4. `shared_ptr` casse la transitivité de la possession — élevée

`CLAUDE.md` énonce : « `is_lifetime_aware<T>` … Ownership is **transitive** ».
`lifetime_aware.h` lignes 46-49 :

```cpp
template <class T>
struct is_lifetime_aware<std::shared_ptr<T>> : std::true_type {};
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>> : std::true_type {};
```

`T` est jeté. Les dix autres possesseurs de la bibliothèque propagent tous —
`unique_ptr` (qui interroge pointé *et* deleter), `pair`, `tuple`, `optional`,
`variant`, `array`, les douze conteneurs, `synchronized_value`, `copy_on_write`,
et les règles `T[N]`/`T[]` du même fichier. `shared_ptr` et `weak_ptr` sont les
**deux seuls** possesseurs inconditionnellement vrais.

Conséquence : un emprunt devient « lifetime aware » dès qu'on l'enveloppe dans un
`shared_ptr`, et `launch_task` l'accepte. Reproduit sous TSan comme
use-after-free, y compris par le chemin `synchronized_value<T>::make()` de la
bibliothèque elle-même.

Vérifié directement :

```cpp
#include <threadsafe/threadsafe.h>
#include <memory>
using threadsafe::is_lifetime_aware_v;
struct Borrow { int* p; };

static_assert(!is_lifetime_aware_v<Borrow>);
static_assert(!is_lifetime_aware_v<std::unique_ptr<Borrow>>);   // transitif
static_assert(is_lifetime_aware_v<std::shared_ptr<Borrow>>);    // ne l'est pas
static_assert(is_lifetime_aware_v<std::weak_ptr<Borrow>>);      // ne l'est pas
```

### Correctif

```cpp
// Ownership is transitive: the control block keeps the T alive, but a T that
// only borrows still borrows. unique_ptr asks the same question one rule below.
template <class T>
struct is_lifetime_aware<std::shared_ptr<T>>
    : is_lifetime_aware<std::remove_cv_t<std::remove_all_extents_t<T>>> {};
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>>
    : is_lifetime_aware<std::remove_cv_t<std::remove_all_extents_t<T>>> {};
```

**Ce correctif casse exactement un test**, et c'est celui qui énonce la politique
inverse — `tests/test_lifetime_aware.cpp:59` :

```cpp
static_assert(is_lifetime_aware_v<std::shared_ptr<std::span<int>>>,
              "is_lifetime_aware — an owner of a borrowed range is still an owner");
```

Il faut donc **trancher**, et le rapport recommande de trancher pour la
transitivité : c'est ce que `CLAUDE.md` promet, c'est ce que font les dix autres
possesseurs, et c'est ce que `launch_task` a besoin d'être vrai pour tenir sa
propre garantie. Le test devient :

```cpp
static_assert(!is_lifetime_aware_v<std::shared_ptr<std::span<int>>>,
              "is_lifetime_aware — ownership is transitive: the control block "
              "keeps the span alive, not the storage the span points at");
```

### Une limite qui subsiste au correctif

Le constructeur *aliasing* de `std::shared_ptr` fabrique un
`shared_ptr<T>` qui pointe vers un objet qu'il ne possède pas. Le vérificateur a
montré que ce cas **traverse le correctif** : aucun trait de type ne peut le voir,
puisque le type est identique. C'est à documenter, pas à corriger.

---

## 5 à 7 — réels mais surévalués

- **Le garde copie/déplacement** (`may_hijack_copy_move`) rejette `std::chrono::duration`,
  `time_point`, `complex`, `bitset`, `valarray`, `expected`. Mesuré : **10 des 16**
  types-valeur autonomes testés sont refusés. C'est délibéré et le raisonnement du
  commentaire est juste (sept tentatives de contournement ont échoué), mais l'ampleur
  n'est nulle part énoncée. Traité en détail dans
  [08-api-et-flexibilite.md](./08-api-et-flexibilite.md).
- **`const unique_ptr<const T, D>` avec un deleter non possédant** fait confiance au
  `const` du pointé sans vérifier que `D` possède réellement. Réel, mais il faut un
  deleter écrit exprès pour ne rien détruire — le vérificateur l'a jugé délibéré.
- **La règle `borrowed_range`** rejette des vues calculées (`views::iota`) qui
  n'empruntent rien. Elle est par ailleurs **redondante** pour tous les types que la
  suite teste (`span`, `string_view`, `subrange` sont déjà pris par la marche sur les
  membres) et n'est indispensable que pour une vue **vide** — voir
  [03-couverture-de-tests.md](./03-couverture-de-tests.md).

---

## Ce qui a résisté

Il faut le dire aussi nettement que les défauts : sur ~200 scénarios exécutés, le
cœur des traits a tenu.

- **La règle référence/pointeur** — `is_sendable<T&>` = `is_sendable<T*>` =
  `is_synchronizable<remove_cv_t<T>>` — est le pivot de la bibliothèque et n'a jamais
  été prise en défaut. Le retrait des cv est uniformément **plus strict**, jamais plus
  permissif.
- **La marche structurelle** gère correctement membres privés, unions nommées et
  anonymes, champs de bits, tableaux multidimensionnels, bases virtuelles et héritage
  multiple.
- **Aucun emprunt standard manqué** sur ~45 types sondés : itérateurs, vues composées,
  `initializer_list`, `mdspan`, `coroutine_handle`, `any`, `smatch`/`ssub_match`,
  `filesystem::path`, `error_code`, `type_index` — tous rejetés.
- **`may_hijack_copy_move`** a résisté à sept tentatives de blanchiment, y compris la
  plus subtile : hériter du template via `using Base::Base`.
- **La cohérence cv** est parfaite sur 22 formes, et elle tient *à travers les
  spécialisations utilisateur*, parce que le renvoi passe par `is_sendable_v` et non par
  le template primaire.
- **`weak_ptr`** dit vrai : `lock()` est une fonction totale, et la tentative de le
  mettre en défaut à l'exécution a échoué proprement.
- **La récursion termine** sur les types auto-référentiels pour une raison *de principe*
  — le `const` d'un pointé n'est pas cru, donc le trait complet (opt-in, donc faux) est
  posé — et non par une limite de profondeur.
- **Les spécialisations écrites après l'en-tête** atteignent bien la récursion, y compris
  depuis une autre unité de traduction : l'indirection par `substitute` tient sa promesse.
