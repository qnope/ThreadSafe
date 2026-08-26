# 06 — Performance de compilation

> Rapport d'audit ThreadSafe. Voir aussi [00 — synthèse](./00-synthese.md),
> [01 — robustesse des traits](./01-robustesse-des-traits.md),
> [02 — robustesse des helpers](./02-robustesse-des-helpers.md),
> [03 — couverture de tests](./03-couverture-de-tests.md),
> [04 — diagnostics](./04-diagnostics.md),
> [05 — simplicité](./05-simplicite.md),
> [07 — performance d'exécution](./07-performance-execution.md),
> [08 — API et flexibilité](./08-api-et-flexibilite.md),
> [09 — méthodologie](./09-methodologie.md).

## Verdict

**Le coût de compilation de ThreadSafe n'est pas le coût de la réflexion : c'est le coût du
`#include`.** Une unité de traduction *vide* qui ne fait qu'inclure `threadsafe/threadsafe.h`
coûte **586 ms**, sur les ~620 ms d'une TU de test typique — c'est-à-dire que 94 % du temps de
build de la suite est payé avant qu'une seule question ait été posée. `-ftime-report` range la
totalité de l'évaluation des traits dans la ligne `constant expression evaluation` : **2 % sur la
TU vide, 9 % sur la TU de test la plus lourde, soit environ 50 ms**. Tout le reste est du parsing
d'en-têtes standard. Deux corollaires : (1) l'audit précédent, qui attribuait ~135 ms / 36 % à
`<ranges>` et au test `borrowed_range`, est **faux** — remesuré aujourd'hui, retirer `<ranges>`
et la règle `borrowed_range` fait gagner **33 ms**, pas 135 ; (2) le seul levier qui change
vraiment l'ordre de grandeur n'est pas dans le code de la bibliothèque, c'est de **ne pas
re-parser l'en-tête onze fois** — une *header unit* GCC ramène la TU vide de 602 ms à **32 ms**,
et les 11 tests passent tous. Les leviers internes existent mais sont petits et honnêtes :
supprimer `<algorithm>` (−18 ms, légal, la suite reste verte), offrir un `threadsafe/core.h`
(−155 ms sur une TU utilisateur réelle, réponses identiques). Le forward-declare des templates
standard, qui aurait rapporté 55 ms, **ne marche pas** — pas seulement « UB en théorie », mais
120 erreurs de compilation dès que l'utilisateur inclut `<list>`. Enfin, la mémoïsation est
**déjà parfaite et gratuite** : mille fois la même question coûte le prix d'une seule. Aucun
cache maison n'est à écrire.

---

## Conditions de mesure

| | |
|---|---|
| Compilateur | GCC 16.2.0 (Homebrew), `-std=c++26 -freflection` |
| Machine | Apple M3 Pro, 12 cœurs, macOS 26.6.2 |
| Protocole temps | `-fsyntax-only`, meilleur de 5 à 8 exécutions **entrelacées** (toutes les variantes dans la même boucle, pour annuler la dérive de la machine) |
| Protocole déterministe | `g++ -E \| grep -cv '^#'` — nombre de lignes préprocessées, reproductible au caractère près |

Les chiffres de la table de référence ci-dessous sont ceux du lead, pris sur machine au repos ;
mes propres remesures tombent 3 à 6 % au-dessus (machine plus chargée), mais **tous les écarts
relatifs concordent**. Chaque fois que je donne un « avant / après », les deux nombres viennent
de la *même* exécution entrelacée et sont donc directement comparables entre eux.

---

## 1. Où part le temps

### 1.1 Coût de chaque en-tête, seul, dans une TU vide

| Ce qui est inclus | Temps (lead) | Ma remesure |
|---|---:|---:|
| `nothing.cpp` (`int main() {}`) | 31 ms | 31 ms |
| `<ranges>` seul | 246 ms | 268 ms |
| `details/utils.h` | 196 ms | 211 ms |
| `details/sendable.h` | 198 ms | 225 ms |
| `details/synchronizable.h` | 218 ms | 249 ms |
| `details/lifetime_aware.h` | 342 ms | 389 ms |
| `details/smart_pointers.h` | 345 ms | 391 ms |
| `details/copy_on_write.h` | 351 ms | 402 ms |
| `details/vocabulary.h` | 369 ms | 431 ms |
| `details/allowed_std_wrappers.h` | 407 ms | 471 ms |
| `details/synchronized_value.h` | 528 ms | 605 ms |
| `details/asynchronous_task_launcher.h` | 533 ms | 611 ms *(voir 1.4)* |
| **`threadsafe.h` (umbrella), TU VIDE** | **586 ms** | **614 ms** |
| les trois en-têtes de traits seuls | 389 ms | 370 ms |

### 1.2 Le coût des onze TU de test

| TU | Temps (lead) |
|---|---:|
| `test_deferred_specialization` | 592 ms |
| `test_lifetime_aware` | 604 ms |
| `test_synchronized_value` | 606 ms |
| `test_asynchronous_task_launcher` | 607 ms |
| `test_smart_pointers` | 618 ms |
| `test_diagnostics` | 619 ms |
| `test_copy_on_write` | 629 ms |
| `test_sendable` | 635 ms |
| `test_synchronizable` | 640 ms |
| `test_containers` | 643 ms |
| `test_soundness_regressions` | 655 ms |
| **Total** | **6 849 ms** |

`11 × 586 = 6 446 ms`, soit **94 % du total**. Les onze fichiers de test, avec leurs centaines de
`static_assert`, leurs hiérarchies, leurs lambdas et leurs spécialisations, ne représentent
ensemble que **403 ms** de travail propre. Le reste, c'est onze fois le même parsing.

### 1.3 `-ftime-report` : la réflexion ne coûte rien

Reproduit par mes soins, chiffres identiques à ceux du lead :

```
$ g++-16 -std=c++26 -freflection -I include -fsyntax-only -ftime-report umbrella.cpp
 phase setup                        :   0.00 (  0%)  3510k (  1%)
 phase parsing                      :   0.46 ( 72%)   201M ( 79%)
 phase lang. deferred               :   0.18 ( 28%)    50M ( 20%)
 template instantiation             :   0.21 ( 33%)    76M ( 30%)
 constant expression evaluation     :   0.01 (  2%)  1057k (  0%)
 TOTAL                              :   0.64          255M

$ g++-16 ... -ftime-report tests/test_soundness_regressions.cpp
 phase setup                        :   0.00 (  0%)  3510k (  1%)
 phase parsing                      :   0.54 ( 76%)   257M ( 83%)
 phase lang. deferred               :   0.17 ( 24%)    48M ( 16%)
 template instantiation             :   0.22 ( 30%)    80M ( 26%)
 constant expression evaluation     :   0.07 (  9%)    16M (  5%)
 TOTAL                              :   0.72          309M
```

`umbrella.cpp` est le fichier suivant, dans son intégralité :

```cpp
#include <threadsafe/threadsafe.h>
int main() {}
```

**Toute** l'évaluation des traits — chaque `consteval`, chaque `std::meta::substitute`, chaque
parcours structurel — vit dans la ligne `constant expression evaluation` : elle passe de 0,01 s
sur une TU qui ne demande rien à 0,07 s sur la TU de test la plus lourde. **Le budget total de la
réflexion dans le pire fichier du dépôt est de 60 ms.** C'est la phrase à mettre sur une slide.

### 1.4 Note d'hygiène : `asynchronous_task_launcher.h` n'est pas autonome

Découvert en mesurant les en-têtes un par un. Ce fichier compilé seul échoue :

```
$ g++-16 -std=c++26 -freflection -I include -fsyntax-only h_asynchronous_task_launcher.cpp
asynchronous_task_launcher.h:82:19: error: static assertion failed: std::jthread injects a
stop_token that the Args constraints never see; it must satisfy them on its own
   82 |     static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
```

où `h_asynchronous_task_launcher.cpp` est, en entier :

```cpp
#include <threadsafe/details/asynchronous_task_launcher.h>
int main() {}
```

Il lui manque `#include <threadsafe/details/vocabulary.h>`, qui est l'en-tête qui bénit
`std::stop_token`. Le chiffre de 533 ms de la table 1.1 est donc mesuré sur une compilation en
erreur. Ce n'est pas un problème de performance et personne n'inclut un en-tête `details/` à la
main — mais c'est une ligne à ajouter, et elle est gratuite :

```cpp
#pragma once

#include <concepts>
#include <meta>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/vocabulary.h>
```

(le reste du fichier est inchangé). Vérifié : avec cette seule ligne ajoutée,
`h_asynchronous_task_launcher.cpp` compile proprement, et les 11 TU restent vertes.

---

## 2. Correction de l'audit précédent : `<ranges>` ne coûte pas 135 ms

L'audit précédent attribuait **~135 ms, soit 36 %**, à `<ranges>` et au test
`std::ranges::borrowed_range` de `lifetime_aware.h`. **C'est faux.** Voici la mesure d'aujourd'hui.

### 2.1 Le patch utilisé pour mesurer

Deux modifications dans `include/threadsafe/details/lifetime_aware.h` : supprimer la ligne
`#include <ranges>`, et supprimer ce bloc (lignes 156-160 de l'original) :

```cpp
    if (trait_value(^^std::ranges::borrowed_range, type))
        reject(type,
               u8"is a borrowed range: a view over someone else's storage, it "
               u8"does not keep its elements alive",
               path);
```

C'est la seule occurrence de `<ranges>` dans tout `lifetime_aware.h` ; l'autre usage de
`std::ranges` dans la bibliothèque est `std::ranges::contains` dans `allowed_std_wrappers.h`
(section 3), qui vient de `<algorithm>`, pas de `<ranges>`.

### 2.2 Le résultat

| Variante | Temps | Lignes préprocessées |
|---|---:|---:|
| `threadsafe.h` de référence | **636 ms** | 136 504 |
| sans `<ranges>` ni `borrowed_range` | **603 ms** | 125 777 |
| **Gain** | **33 ms (5,2 %)** | −10 727 (−7,9 %) |

Le lead mesure le même écart : 583 → 555 ms, soit 28 ms. Les deux mesures s'accordent à
**30 ms ± 3**, pas 135.

### 2.3 Pourquoi l'audit précédent s'est trompé

Parce qu'il a mesuré `<ranges>` **isolément** au lieu de le mesurer **marginalement**. Les deux
chiffres n'ont rien à voir :

| TU | Lignes préprocessées | Coût marginal de `<ranges>` |
|---|---:|---:|
| `<meta>` seul | 45 655 | — |
| `<meta>` + `<ranges>` | 72 345 | **+26 690** |
| `<meta> <functional> <memory> <string>` | 83 854 | — |
| les mêmes + `<ranges>` | 96 857 | **+13 003** |
| `threadsafe.h` sans `<ranges>` | 125 777 | — |
| `threadsafe.h` complet | 136 504 | **+10 727** |

`<ranges>` seul dans une TU vide coûte 236 ms (268 − 32). Dans la bibliothèque réelle, il coûte
33 ms, parce que `<functional>`, `<memory>`, `<meta>` et `<string>` — que la bibliothèque inclut
de toute façon — en tirent déjà les deux tiers. **Mesurer un en-tête isolément ne dit rien de ce
qu'il coûte dans un projet.** C'est la leçon de méthode de ce rapport.

### 2.4 Et il ne faut *pas* le supprimer

Le patch de la section 2.1 laisse la suite verte — **11/11**, vérifié. C'est même une survivance
de mutation supplémentaire pour [03](./03-couverture-de-tests.md) : rien dans les tests
n'épingle la règle `borrowed_range`, parce que le parcours structurel rattrape `string_view` et
`span` par leur membre pointeur. Mais ce qui se perd, c'est le **message**, et c'est le meilleur
message de la bibliothèque ([04](./04-diagnostics.md) le classe premier). Fichier de test complet :

```cpp
#include <threadsafe/threadsafe.h>

#include <string_view>

int main() { threadsafe::assert_lifetime_aware<std::string_view>(); }
```

Avec `<ranges>` (référence) :

```
error: uncaught exception of type 'std::meta::exception'; 'what()':
'std::basic_string_view<char> is a borrowed range: a view over someone else's storage,
 it does not keep its elements alive'
```

Sans `<ranges>` :

```
error: uncaught exception of type 'std::meta::exception'; 'what()':
'std::basic_string_view<char>::_M_str (const char*) is a reference or a raw pointer:
 it borrows its referent instead of keeping it alive — hold the object, or a std::shared_ptr to it'
```

Le message passe de « c'est une vue » à « son membre privé `_M_str` de libstdc++ ». **33 ms ne
paient pas ça.** Verdict : garder `<ranges>`, et corriger le chiffre de l'audit précédent.

---

## 3. `allowed_std_wrappers.h` : le plus gros chiffre du dossier, et il est plus petit qu'annoncé

### 3.1 Le chiffre de 407 ms compare deux choses différentes

`details/allowed_std_wrappers.h` coûte 407 ms là où `details/sendable.h` coûte 198 ms. Mais
`allowed_std_wrappers.h` inclut *aussi* `sendable.h`, `synchronizable.h` et `lifetime_aware.h`.
La bonne comparaison est marginale, et elle est beaucoup plus modeste :

| TU | Temps | Lignes préprocessées |
|---|---:|---:|
| les trois en-têtes de traits | 389 ms | 99 424 |
| les trois + les 14 `#include` de conteneurs | 439 ms | 120 630 |
| `details/allowed_std_wrappers.h` (= les trois + les 14 + son propre corps) | 457 ms | 120 734 |

Donc : **les 14 `#include` coûtent 50 ms**, et le corps réflexif de l'allow-list (la liste de 18
`^^std::…`, le concept, les trois spécialisations) coûte **18 ms et 104 lignes**. Le « 209 ms »
qu'on obtient en soustrayant 198 de 407 n'existe pas.

### 3.2 Sept des quatorze `#include` sont déjà gratuits

Mesure déterministe : lignes ajoutées par chaque `#include` **au-dessus** des trois en-têtes de
traits (base = 99 424 lignes).

| `#include` | Lignes ajoutées |
|---|---:|
| `<array>` | +1 |
| `<optional>` | +1 |
| `<string>` | +1 |
| `<tuple>` | +1 |
| `<unordered_map>` | +1 |
| `<variant>` | +1 |
| `<vector>` | +1 |
| `<unordered_set>` | +1 569 |
| `<forward_list>` | +1 887 |
| `<list>` | +2 485 |
| `<deque>` | +3 173 |
| `<set>` | +4 414 |
| `<map>` | +4 901 |
| `<algorithm>` | +5 923 |
| *les 14 ensemble* | *+21 206* |

Sept d'entre eux sont **déjà tirés** par `<meta>`, `<ranges>`, `<memory>`, `<functional>` et
`<string>` que les traits incluent de toute façon. Le budget réel se concentre sur sept en-têtes,
et le plus cher de tous — `<algorithm>` — n'est **pas** un conteneur.

### 3.3 L'expérience du forward-declare : elle échoue, et pas seulement en théorie

Pour écrire `^^std::vector`, il suffit que le template soit **déclaré**. J'ai donc écrit la
version qui déclare au lieu d'inclure. Voici le fichier complet de l'expérience, tel que je l'ai
compilé (`allowed_std_wrappers.h`, version « forward-declare ») :

```cpp
#pragma once

#include <meta>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

// EXPÉRIENCE : déclarer au lieu de définir. UB d'après [namespace.std]/7.
namespace std {
template <class, class> class deque;
template <class, class> class forward_list;
template <class, class> class list;
template <class, class, class> class map;
template <class, class, class> class multimap;
template <class, class, class> class set;
template <class, class, class> class multiset;
template <class, class, class, class> class unordered_set;
template <class, class, class, class> class unordered_multiset;
}

namespace threadsafe {

namespace detail {

inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,        ^^std::deque,             ^^std::list,
    ^^std::forward_list,  ^^std::basic_string,      ^^std::map,
    ^^std::multimap,      ^^std::set,               ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset,
    ^^std::pair,          ^^std::tuple,             ^^std::optional,
    ^^std::variant,       ^^std::array,
};

inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
    type = std::meta::dealias(type);
    if (!std::meta::has_template_arguments(type))
        return false;
    const std::meta::info wrapper_template = std::meta::template_of(type);
    for (std::meta::info allowed : allowed_std_wrappers)
        if (allowed == wrapper_template)
            return true;
    return false;
}

template <class T>
concept std_wrapper = is_allowed_std_wrapper(^^T);

inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
    std::vector<std::meta::info> wrapped;
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type)))
        if (std::meta::is_type(argument))
            wrapped.push_back(std::meta::remove_cv(argument));
    return wrapped;
}

inline consteval bool std_wrapper_is_sendable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return true;
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_sendable_type(wrapped))
            return false;
    return true;
}

inline consteval bool
std_wrapper_is_const_synchronizable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return true;
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_synchronizable_type(std::meta::add_const(wrapped)))
            return false;
    return true;
}

inline consteval bool std_wrapper_is_lifetime_aware(std::meta::info type) {
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_lifetime_aware_type(wrapped))
            return false;
    return true;
}

}

template <detail::std_wrapper T>
struct is_sendable<T>
    : std::bool_constant<detail::std_wrapper_is_sendable(^^T)> {};

template <detail::std_wrapper T>
struct is_synchronizable<const T>
    : std::bool_constant<detail::std_wrapper_is_const_synchronizable(^^T)> {};

template <detail::std_wrapper T>
struct is_lifetime_aware<T>
    : std::bool_constant<detail::std_wrapper_is_lifetime_aware(^^T)> {};

}
```

**Ça compile, et le gain est réel** — tant que personne n'inclut de conteneur :

| TU | Temps | Lignes préprocessées |
|---|---:|---:|
| `threadsafe.h` de référence | 614 ms | 136 504 |
| `threadsafe.h` avec forward-declare | **559 ms** | **115 299** |
| Gain | **55 ms (9 %)** | −21 205 (−15,5 %) |

Maintenant le fichier de vérification, complet, qui fait ce que fait n'importe quel utilisateur :

```cpp
#include <threadsafe/threadsafe.h>

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static_assert(threadsafe::is_sendable_v<std::vector<int>>);
static_assert(threadsafe::is_sendable_v<std::deque<int>>);
static_assert(threadsafe::is_sendable_v<std::list<int>>);
static_assert(threadsafe::is_sendable_v<std::forward_list<int>>);
static_assert(threadsafe::is_sendable_v<std::map<int, int>>);
static_assert(threadsafe::is_sendable_v<std::multimap<int, int>>);
static_assert(threadsafe::is_sendable_v<std::set<int>>);
static_assert(threadsafe::is_sendable_v<std::multiset<int>>);
static_assert(threadsafe::is_sendable_v<std::unordered_set<int>>);
static_assert(threadsafe::is_sendable_v<std::unordered_multiset<int>>);
static_assert(threadsafe::is_sendable_v<std::string>);
static_assert(!threadsafe::is_sendable_v<std::vector<int *>>);
static_assert(!threadsafe::is_sendable_v<std::list<int *>>);
int main() {}
```

Avec les en-têtes réels : **0 erreur**. Avec le forward-declare : **120 erreurs**, dont la
première :

```
In file included from /opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/list:67,
                 from fwd_check.cpp:5:
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/bits/stl_list.h:413:38:
error: reference to 'list' is ambiguous [-Wtemplate-body]
  413 |         friend class _GLIBCXX_STD_C::list;
      |                                      ^~~~
  • there are 2 candidates
    • candidate 1: 'template<class _Tp, class _Allocator> class std::__cxx11::list'
      /opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/bits/stl_list.h:184:53
    • candidate 2: 'template<class, class> class std::list'
      fwd/threadsafe/details/allowed_std_wrappers.h:15:31
```

**Verdict, sans ambiguïté : non.** `[namespace.std]/7` interdit d'ajouter des déclarations dans
`std`, et ici l'interdit n'est pas décoratif : libstdc++ met `std::list` et
`std::basic_string` dans le namespace inline `std::__cxx11`. Une redéclaration « à côté »
n'est donc pas une redéclaration, c'est un **second template homonyme**, et l'ambiguïté explose
dans l'en-tête standard de l'utilisateur, pas dans le nôtre. 55 ms contre 120 erreurs de
compilation chez le client, et un comportement indéfini par-dessus. Sur une slide, l'expérience
vaut mieux que le gain : *« la réflexion a besoin de déclarations, pas de définitions — mais
`std` n'est pas à vous »*.

### 3.4 Ce qui est légal : supprimer `<algorithm>`

`<algorithm>` est le plus cher des quatorze (+5 923 lignes) et il n'est là que pour un
`std::ranges::contains` sur un tableau de 18 éléments. Une boucle `for` fait la même chose.
Voici le fichier `include/threadsafe/details/allowed_std_wrappers.h` **complet** tel que je le
propose — il intègre aussi le correctif cv du lead pour [TC-1](./01-robustesse-des-traits.md)
(`std::same_as<T, std::remove_cv_t<T>> &&`), pour que les deux changements se composent :

```cpp
#pragma once

#include <array>
#include <concepts>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <meta>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

namespace detail {

// The standard templates that add no state of their own: a specialization is
// nothing but its arguments, held by value. The three rules at the bottom of
// this file therefore read the arguments instead of letting the structural walk
// see the members — a std::vector<T> holds a T*, not a T, and its constructor
// templates block the structural default anyway (see may_hijack_copy_move).
//
// An allow-list, not a deduction: nothing in reflection tells std::vector<T>
// apart from a type that hides sharing behind the same arguments. This list is
// only the membership test; the answer itself is written the way every other
// answer in this library is written — as a specialization of the trait.
inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,        ^^std::deque,             ^^std::list,
    ^^std::forward_list,  ^^std::basic_string,      ^^std::map,
    ^^std::multimap,      ^^std::set,               ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset,
    ^^std::pair,          ^^std::tuple,             ^^std::optional,
    ^^std::variant,       ^^std::array,
};

// A linear scan over eighteen entries, written by hand so that this header does
// not have to include <algorithm> for one call: the container library it names
// is 21 000 preprocessed lines, and <algorithm> alone is 5 900 of them.
inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
    type = std::meta::dealias(type);
    if (!std::meta::has_template_arguments(type))
        return false;

    const std::meta::info wrapper_template = std::meta::template_of(type);
    for (std::meta::info allowed : allowed_std_wrappers)
        if (allowed == wrapper_template)
            return true;

    return false;
}

// The family as a concept: what the three specializations are keyed on. A
// constrained partial specialization over the same argument list as the primary
// template — the shape is_synchronizable<F> already takes for function types.
//
// The unqualified-spelling requirement is load-bearing: without it a `const
// std::vector<T>` reaches this specialization instead of the primary template's
// cv-forwarding, and the answer is recomputed from the weaker read-only
// question — which launders an explicit is_sendable<T> opt-out.
template <class T>
concept std_wrapper =
    std::same_as<T, std::remove_cv_t<T>> && is_allowed_std_wrapper(^^T);

// The arguments that carry a value: the type arguments. A std::array<T, N>
// wraps Ts, not an N.
inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
    std::vector<std::meta::info> wrapped;
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type)))
        if (std::meta::is_type(argument))
            wrapped.push_back(std::meta::remove_cv(argument));
    return wrapped;
}

// Sending a wrapper sends everything it holds. The synchronizable question
// comes first for the same reason it comes first in the structural walk: a type
// that synchronizes itself is sendable whatever it holds, and a rule written as
// a specialization is the only thing standing between that invariant and a
// container someone vouched for.
inline consteval bool std_wrapper_is_sendable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return true;

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_sendable_type(wrapped))
            return false;

    return true;
}

// [res.on.data.races]: the const member functions of a standard container may
// run concurrently, so a const wrapper is read-safe exactly when everything a
// reader reaches through it — elements and stored policies — is. Reading the
// arguments also keeps the recursion out of libstdc++ internals, whose mutable
// members (unordered_*'s rehash policy) are covered by that guarantee.
inline consteval bool
std_wrapper_is_const_synchronizable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return true;

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_synchronizable_type(std::meta::add_const(wrapped)))
            return false;

    return true;
}

// A wrapper owns what it wraps. No borrowed_range test here, unlike the
// structural walk: not one of the templates listed above is a view over
// someone else's storage.
inline consteval bool std_wrapper_is_lifetime_aware(std::meta::info type) {
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_lifetime_aware_type(wrapped))
            return false;

    return true;
}

}

template <detail::std_wrapper T>
struct is_sendable<T>
    : std::bool_constant<detail::std_wrapper_is_sendable(^^T)> {};

template <detail::std_wrapper T>
struct is_synchronizable<const T>
    : std::bool_constant<detail::std_wrapper_is_const_synchronizable(^^T)> {};

template <detail::std_wrapper T>
struct is_lifetime_aware<T>
    : std::bool_constant<detail::std_wrapper_is_lifetime_aware(^^T)> {};

}
```

**Mesure et régression :**

| | Temps | Lignes préprocessées | Suite |
|---|---:|---:|---|
| `threadsafe.h` de référence | 577 ms | 136 504 | 11/11 |
| avec l'en-tête ci-dessus | **559 ms** | **130 578** | **11/11** |
| gain | 18 ms (3,1 %) | −5 926 (−4,3 %) | — |

`fix_regression_checked: suite-passes` — vérifié par moi-même, les onze TU compilent sans une
erreur. Et le correctif cv fonctionne : le fichier

```cpp
#include <threadsafe/threadsafe.h>
#include <vector>
struct Affine { int handle; };
template <> struct threadsafe::is_sendable<Affine> : std::false_type {};
static_assert(!threadsafe::is_sendable_v<std::vector<Affine>>);
static_assert(!threadsafe::is_sendable_v<const std::vector<Affine>>);
int main() {}
```

donne **1 erreur** avec les en-têtes actuels (le `const` blanchit l'opt-out) et **0 erreur**
avec celui ci-dessus.

18 ms, c'est 3 %. Mais c'est gratuit, c'est légal, ça supprime une dépendance à un en-tête
énorme pour un `for` de trois lignes, et ça se fait dans le même commit que le correctif de
sûreté TC-1. **À faire.**

---

## 4. Un point d'entrée plus léger : `threadsafe/core.h`

### 4.1 La version naïve est fausse

L'idée évidente — un `core.h` qui n'expose que les trois traits — coûte 370 ms au lieu de 614.
Mais elle **change les réponses**. Fichier complet :

```cpp
#include <threadsafe/core.h>

#include <string>
#include <vector>

struct Owning {
    int value;
    double weight;
};

static_assert(threadsafe::is_sendable_v<Owning>);
static_assert(!threadsafe::is_sendable_v<int *>);
static_assert(threadsafe::is_sendable_v<std::vector<int>>);
static_assert(threadsafe::is_sendable_v<std::string>);
int main() {}
```

avec

```cpp
#pragma once

#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/lifetime_aware.h>
```

donne :

```
core_answers.cpp:12:27: error: static assertion failed
   12 | static_assert(threadsafe::is_sendable_v<std::vector<int>>);
core_answers.cpp:13:27: error: static assertion failed
   13 | static_assert(threadsafe::is_sendable_v<std::string>);
```

Et ajouter `allowed_std_wrappers.h` **ne suffit pas** : `std::vector<int>` reste faux. La raison
est instructive et mérite une slide — l'allow-list lit *tous* les arguments de type, donc elle
pose la question sur `std::allocator<int>`, et `std::allocator` a un constructeur template
convertissant, que `may_hijack_copy_move` refuse. C'est `vocabulary.h` qui le débloque :

```cpp
// std::allocator is stateless -- allowed_std_wrappers cannot say this,
// because it is true even for a T that answers no.
template <class T>
struct is_sendable<std::allocator<T>> : std::true_type {};
```

Autrement dit : **le plancher d'un `core.h` honnête n'est pas les trois traits, c'est les trois
traits plus toute la surface `std`** — `allowed_std_wrappers.h`, `vocabulary.h`,
`smart_pointers.h`. Ce qu'on peut retirer, ce sont les trois *helpers*.

### 4.2 La version honnête

`include/threadsafe/core.h`, complet :

```cpp
#pragma once

// The questions, and nothing that answers them for the library's own helper
// types. Same answers as <threadsafe/threadsafe.h> for every type a user of
// this header can name — copy_on_write, synchronized_value and
// asynchronous_task_launcher are the only types the umbrella adds rules for,
// and they are declared by the headers this one leaves out.
#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
```

L'affirmation « mêmes réponses » est vérifiable et vérifiée : les seules spécialisations de
traits que les trois en-têtes omis apportent sont pour **leurs propres types** —

```
copy_on_write.h:54        struct is_sendable<copy_on_write<T>>
copy_on_write.h:58        struct is_lifetime_aware<copy_on_write<T>>
synchronized_value.h:103  struct is_synchronizable<synchronized_value<T>>
synchronized_value.h:106  struct is_lifetime_aware<synchronized_value<T>>
synchronized_value.h:109  struct is_sendable<value_guard<T, Lock>>
synchronized_value.h:111  struct is_lifetime_aware<value_guard<T, Lock>>
asynchronous_task_launcher.h   (aucune)
```

Un utilisateur de `core.h` ne peut pas nommer ces types, donc il ne peut pas observer de
divergence. **Il n'y a pas de piège ODR ici**, contrairement à la version naïve de 4.1 — qui,
elle, ferait diverger `sizeof` d'un template dépendant de `is_sendable_v<std::vector<T>>` entre
deux TU, exactement le mécanisme de [TC-4 / ADV-05](./01-robustesse-des-traits.md).

### 4.3 Ce que ça rapporte, sur une TU utilisateur réelle

```cpp
#include <threadsafe/threadsafe.h>   // ou <threadsafe/core.h>

#include <map>
#include <string>
#include <vector>

namespace app {

struct Measure {
    std::string sensor;
    double value;
    long timestamp;
};

struct Batch {
    std::vector<Measure> measures;
    std::map<std::string, long> counters;
};

}

static_assert(threadsafe::is_sendable_v<app::Batch>);
static_assert(threadsafe::is_lifetime_aware_v<app::Batch>);
static_assert(threadsafe::is_synchronizable_v<const app::Batch>);
int main() {}
```

| Point d'entrée | Temps | Lignes préprocessées |
|---|---:|---:|
| `threadsafe/threadsafe.h` | 591 ms | 136 504 |
| `threadsafe/core.h` | **436 ms** | **124 058** |
| gain | **155 ms (26 %)** | −12 446 (−9 %) |

*(TU vide : 614 ms → 447 ms.)* Les trois `static_assert` donnent exactement le même résultat des
deux côtés — vérifié.

### 4.4 Faut-il le faire ? — je challenge

155 ms par TU, c'est le deuxième plus gros levier interne du dossier, et c'est purement additif :
ajouter `core.h` ne change pas une ligne de code existant et ne peut pas casser la suite. Mais
c'est **un deuxième point d'entrée à documenter, à maintenir cohérent, et à expliquer sur scène**
— et la bibliothèque est éducative avant d'être un produit. Un public de conférence à qui l'on
présente deux en-têtes va passer les trois premières minutes à se demander lequel prendre.

**Ma recommandation : oui, mais après la conférence.** Pour le talk, un seul `#include` est un
argument, pas un défaut. Pour un utilisateur réel qui n'utilise que les traits, 26 % par TU se
justifie. Et si l'on ne fait qu'une chose, la section 5 rapporte dix fois plus pour zéro ligne
de bibliothèque.

---

## 5. Le vrai levier : ne pas re-parser l'umbrella onze fois

94 % du temps de build, c'est le même en-tête parsé onze fois. Trois techniques existent pour ne
le parser qu'une seule fois. Je les ai toutes les trois essayées sur GCC 16.2.0.

### 5.1 En-tête précompilé (PCH) — ça marche

```bash
g++-16 -std=c++26 -freflection -I include \
       -x c++-header include/threadsafe/threadsafe.h \
       -o include/threadsafe/threadsafe.h.gch
```

Preuve que le `.gch` est bien pris (le `!` de `-H` signifie « précompilé utilisé ») :

```
$ g++-16 -std=c++26 -freflection -Ipchinc -H -fsyntax-only umbrella.cpp
! pchinc/threadsafe/threadsafe.h.gch
 umbrella.cpp
```

**Les 11 TU de test compilent sans une erreur avec le PCH** — vérifié une par une, y compris
`test_deferred_specialization`, qui est celle qui aurait le plus de raisons de casser puisqu'elle
écrit une spécialisation *après* l'en-tête et compte sur la récursion réflexive pour la voir.

Intégration CMake — une ligne dans `tests/CMakeLists.txt` :

```cmake
target_precompile_headers(threadsafe_tests PRIVATE <threadsafe/threadsafe.h>)
```

Testé sur une copie complète du projet : le build passe, `cmake_pch.hxx.gch` fait 135 Mo.

### 5.2 Header unit C++20 — ça marche mieux

```bash
g++-16 -std=c++26 -freflection -fmodules -fmodule-header=system \
       -I include include/threadsafe/threadsafe.h
```

puis dans la TU :

```cpp
import <threadsafe/threadsafe.h>;
```

**Les 11 TU passent également**, converties par simple substitution de la première ligne. Le
`.gcm` fait **20,6 Mo** (contre 135 Mo pour le PCH) et se construit en 1 117 ms.

### 5.3 Module nommé — ça ne marche pas

Fichier `threadsafe.cppm` complet, tel que compilé :

```cpp
module;

#include <threadsafe/threadsafe.h>

export module threadsafe;

export namespace threadsafe {
using threadsafe::is_sendable;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware;
using threadsafe::is_lifetime_aware_v;
using threadsafe::sendable;
using threadsafe::lifetime_aware;
using threadsafe::assert_sendable;
using threadsafe::assert_synchronizable;
using threadsafe::assert_lifetime_aware;
using threadsafe::copy_on_write;
using threadsafe::synchronized_value;
using threadsafe::asynchronous_task_launcher;
}
```

Le module **se construit** (1 026 ms). C'est le consommateur qui casse. TU complète :

```cpp
import threadsafe;

struct Owning { int value; };

static_assert(threadsafe::is_sendable_v<Owning>);
static_assert(!threadsafe::is_sendable_v<int *>);

struct Affine { int handle; };
template <> struct threadsafe::is_sendable<Affine> : std::false_type {};
static_assert(!threadsafe::is_sendable_v<Affine>);

struct Holder { Affine a; };
static_assert(!threadsafe::is_sendable_v<Holder>);

int main() {}
```

donne **11 erreurs**, dont celle-ci, au cœur exact de la machinerie réflexive :

```
/Users/.../threadsafe/details/synchronizable_base.h:25:31:
   in 'constexpr' expansion of 'threadsafe::detail::trait_value(^^threadsafe::is_synchronizable_v<T>, type)'
   25 |     return detail::trait_value(^^is_synchronizable_v, type);
/Users/.../threadsafe/details/utils.h:99:58: error: missing 'std::ranges::begin' or 'std::ranges::end'
```

Ajouter `#include <ranges>` avant l'`import` déplace l'erreur sans la résoudre :

```
/Users/.../threadsafe/details/utils.h:142:56: error: couldn't look up 'std::vector'
/Users/.../threadsafe/details/sendable.h:24:48: error: 'value' is not a member of 'threadsafe::is_sendable<Owning>'
```

Ajouter `#include <meta>` avant l'`import` fait passer cette TU-là (236 ms contre 578 ms) — mais
c'est un aveu : le consommateur repaie textuellement les 45 655 lignes de `<meta>`. Et sur la
vraie suite de tests, converties en `#include <meta>` + `import threadsafe;`, c'est un massacre :

| TU | Erreurs |
|---|---:|
| `nm_test_synchronizable` | 708 |
| `nm_test_containers` | 563 |
| `nm_test_sendable` | 1 186 |
| `nm_test_deferred_specialization` | 2 123 |
| `nm_test_diagnostics` | 2 124 |
| `nm_test_asynchronous_task_launcher` | 2 167 |
| `nm_test_copy_on_write` | 2 605 |
| `nm_test_smart_pointers` | 2 707 |
| `nm_test_lifetime_aware` | 2 831 |
| `nm_test_synchronized_value` | 4 061 |
| `nm_test_soundness_regressions` | 4 777 |

Première erreur, typique du problème classique de fragment de module global de GCC :

```
/opt/homebrew/.../bits/gthr-default.h:346:1: error: redefinition of 'int __gthread_active_p()'
```

**Verdict : le module nommé n'est pas utilisable aujourd'hui sur GCC 16.2.0 avec cette
bibliothèque.** C'est une information utile en soi — et une bonne slide, parce qu'elle dit
exactement où en est l'outillage C++26 : réflexion ✅ + header unit ✅, réflexion + module nommé ❌.

### 5.4 Les chiffres

| | Construction du cache | TU vide | 11 TU en série | 11 TU sur 12 cœurs | 1 TU modifiée |
|---|---:|---:|---:|---:|---:|
| `#include` (référence) | — | 614 ms | 8 233 ms | 1 290 ms | 738 ms |
| PCH | 1 414 ms / 135 Mo | **224 ms** | **3 647 ms** | 629 ms | **364 ms** |
| header unit | 1 117 ms / 20,6 Mo | **32 ms** | **3 463 ms** | 583 ms | — |
| module nommé | 1 026 ms | *ne compile pas* | *ne compile pas* | *ne compile pas* | — |

*(les colonnes « 11 TU » excluent la construction du cache ; « 1 TU modifiée » est mesurée via
`cmake --build -j 12` après édition d'un fichier de test, en ne retenant que les exécutions qui
ont réellement recompilé.)*

**Trois lectures, et elles ne disent pas la même chose :**

1. **La TU vide passe de 614 ms à 32 ms avec une header unit.** Le coût d'inclusion de
   ThreadSafe devient celui d'un `int main() {}`. C'est un facteur **19**.
2. **Sur un build propre parallèle, PCH et header unit sont des pertes nettes.** Build de
   référence : 1 290 ms. Avec PCH : 1 853 ms mesuré de bout en bout via CMake (la construction du
   PCH est sérielle et bloque les douze cœurs). **Il faut le dire :** sur une CI qui compile à
   froid sur douze cœurs, ajouter un PCH *ralentit* le projet de 44 %.
3. **Sur le build sériel et sur la boucle de développement, c'est massif.** Sériel :
   7 795 ms → 4 681 ms (−40 %, construction du PCH incluse). Une seule TU de test rééditée :
   738 ms → 364 ms (−51 %).

**Recommandation.** Ne pas mettre `target_precompile_headers` inconditionnellement. Le mettre
derrière une option, par défaut à `OFF`, documentée d'une phrase :

```cmake
option(THREADSAFE_USE_PCH
       "Precompile <threadsafe/threadsafe.h> for the test target. Halves an \
incremental rebuild and a serial build; slows a cold parallel build down, \
because building the 135 MB PCH is serial." OFF)
if(THREADSAFE_USE_PCH)
    target_precompile_headers(threadsafe_tests PRIVATE <threadsafe/threadsafe.h>)
endif()
```

Et, pour la conférence, montrer la header unit : `import <threadsafe/threadsafe.h>;`, 32 ms,
11/11 vertes. C'est le résultat le plus spectaculaire de tout ce rapport et il ne demande **aucun
changement** dans la bibliothèque.

---

## 6. Le plafond de profondeur (TC-15)

### 6.1 Où il tombe aujourd'hui

Le constat de [01](./01-robustesse-des-traits.md) situait le plafond « entre 120 et 240 niveaux ».
Je l'ai encadré exactement. Générateur complet :

```bash
gen() {
  n=$1
  {
    echo '#include <threadsafe/threadsafe.h>'
    echo 'namespace {'
    echo 'struct L0 { int v; };'
    for i in $(seq 1 $n); do echo "struct L$i { L$((i-1)) m; };"; done
    echo '}'
    echo "static_assert(threadsafe::is_sendable_v<L$n>);"
  } > deep_$n.cpp
}
```

qui produit, pour `n = 3`, exactement ceci (la forme est la même à toute profondeur) :

```cpp
#include <threadsafe/threadsafe.h>
namespace {
struct L0 { int v; };
struct L1 { L0 m; };
struct L2 { L1 m; };
struct L3 { L2 m; };
}
static_assert(threadsafe::is_sendable_v<L3>);
```

Résultats, `-fconstexpr-depth` à sa valeur par défaut de 512 :

| Profondeur | 60 | 100 | 110 | 115 | 118 | 120 | 121 | 122 | **125** | **126** | 130 | 240 | 480 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Résultat | OK | OK | OK | OK | OK | OK | OK | OK | **OK** | **ÉCHEC** | ÉCHEC | ÉCHEC | ÉCHEC |

**Le plafond est exactement 125 niveaux d'imbrication pure.** Soit `512 / 125 ≈ 4,1` frames
d'évaluation constante par niveau. La relation se vérifie en montant la limite :

| `-fconstexpr-depth` | 512 | 1024 | 2048 |
|---|---|---|---|
| Profondeur maximale | 125 | 240 | 480 |

Règle pratique : **il faut `-fconstexpr-depth ≈ 4 × profondeur`.**

### 6.2 Le message est mauvais, et ce n'est pas la faute de la bibliothèque

À 126 niveaux, GCC produit **129 erreurs**. La première :

```
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/bits/allocator.h:199:41: error:
'constexpr' evaluation depth exceeds maximum of 512
(use '-fconstexpr-depth=' to increase the maximum)
```

puis 127 lignes de la forme :

```
.../threadsafe/details/sendable.h:24:48: error: 'value' is not a member of
'threadsafe::is_sendable<{anonymous}::L0>'
...
.../threadsafe/details/sendable.h:24:48: error: 'value' is not a member of
'threadsafe::is_sendable<{anonymous}::L126>'
```

et enfin `deep_126.cpp:131:27: error: non-constant condition for static assertion`. Le fichier
nommé en premier — `bits/allocator.h` — est un en-tête de libstdc++ que l'utilisateur n'a jamais
écrit ; il apparaît parce que la frame la plus profonde au moment de l'épuisement se trouve dans
le `std::vector<std::meta::info>` que `wrapped_types_of` construit. Ni le type fautif ni le
fichier de l'utilisateur n'apparaissent avant la 129ᵉ erreur.

### 6.3 Le correctif : une phrase, pas une ligne de code

`fix_regression_checked: no-fix-proposed` sur le fond, et je suis d'accord avec ce choix : le
plafond est celui du **compilateur**, pas de la bibliothèque, et tout code ajouté ici échangerait
une limite que personne n'atteint contre un coût que tout le monde paie (voir 8.2). La seule
chose à faire est de le documenter là où quelqu'un le lira, dans
`include/threadsafe/details/sendable.h`, juste au-dessus de la déclaration de `descend_sendable`.
Le bloc complet, tel que je le propose :

```cpp
namespace detail {
consteval void diagnose_default_is_sendable(std::meta::info type,
                                            std::u8string path = {});
consteval bool default_is_sendable(std::meta::info type);

// Depth: the structural walk spends about four constexpr frames per level of
// member nesting, so GCC's default -fconstexpr-depth=512 stops at 125 levels —
// past that the first error names bits/allocator.h and not your type. A chain
// that deep is generated code, never handwritten; raise the limit with
// -fconstexpr-depth=$((4 * levels)).
[[noreturn]] consteval void descend_sendable(std::meta::info inner,
                                             const std::u8string &path);
}
```

Une phrase dans `CLAUDE.md` ferait aussi bien. **Sévérité : faible. À documenter, pas à corriger.**

---

## 7. Mémoïsation : il n'y a rien à faire, et c'est un résultat

La question posée était : poser mille fois la même question coûte-t-il le prix de mille questions
ou celui d'une seule ? **Celui d'une seule, exactement.**

### 7.1 Le banc

Base commune des cinq fichiers (`memo_0.cpp` en entier ; les autres ne diffèrent que par la queue) :

```cpp
#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>

namespace {
struct Leaf { int a; double b; std::string c; std::vector<int> d; };
struct N0 { Leaf leaf; };
struct N1 { N0 m; int extra; };
struct N2 { N1 m; int extra; };
struct N3 { N2 m; int extra; };
struct N4 { N3 m; int extra; };
struct N5 { N4 m; int extra; };
struct N6 { N5 m; int extra; };
struct N7 { N6 m; int extra; };
struct N8 { N7 m; int extra; };
struct N9 { N8 m; int extra; };
struct N10 { N9 m; int extra; };
struct N11 { N10 m; int extra; };
struct N12 { N11 m; int extra; };
struct N13 { N12 m; int extra; };
struct N14 { N13 m; int extra; };
struct N15 { N14 m; int extra; };
struct N16 { N15 m; int extra; };
struct N17 { N16 m; int extra; };
struct N18 { N17 m; int extra; };
struct N19 { N18 m; int extra; };
struct N20 { N19 m; int extra; };
}
int main() {}
```

Les quatre variantes, toutes construites sur ce préambule :

* **1 question** — une seule ligne ajoutée :
  ```cpp
  static_assert(threadsafe::is_sendable_v<N20>);
  ```
* **1 000 fois la même question** — la même ligne répétée mille fois :
  ```cpp
  static_assert(threadsafe::is_sendable_v<N20>);
  static_assert(threadsafe::is_sendable_v<N20>);
  // … mille lignes identiques …
  ```
* **1 000 questions distinctes** — mille types de même forme et de même profondeur, ajoutés dans
  le `namespace { }` :
  ```cpp
  struct D0 { N19 m; int extra; };
  struct D1 { N19 m; int extra; };
  // … jusqu'à D999 …
  ```
  puis mille `static_assert(threadsafe::is_sendable_v<D0>);` … `<D999>`.
* **témoin** — les mille types `D0`…`D999` déclarés et **aucune** question posée.

### 7.2 Le résultat

| Fichier | Temps | Coût marginal |
|---|---:|---:|
| préambule seul, 0 question | 636 ms | — |
| 1 question | 660 ms | +24 ms |
| **1 000 fois la même question** | **658 ms** | **+22 ms** |
| 1 000 types déclarés, 0 question | 644 ms | +8 ms |
| 1 000 questions **distinctes** | 1 282 ms | +638 ms *(0,64 ms/type)* |

**Mille fois la même question coûte 22 ms ; une seule fois, 24 ms.** L'écart est du bruit. La
mémoïsation est totale.

### 7.3 Pourquoi — et c'est un argument pour la forme choisie

Parce que `is_sendable` est un **template de classe** et `is_sendable_v` une **variable
template**. Le compilateur instancie `is_sendable<T>` une fois par `T` et initialise
`is_sendable_v<T>` une fois ; toute question ultérieure est une consultation de table. C'est
la forme `std::is_same` / `std::is_same_v` que `CLAUDE.md` revendique — et il se trouve qu'elle
est aussi la forme rapide.

La preuve par l'absurde, en appelant la fonction `consteval` sous-jacente au lieu du trait —
mille fois la ligne `static_assert(threadsafe::detail::default_is_sendable(^^N20));` sur le même
préambule :

| Fichier | Temps | Coût marginal |
|---|---:|---:|
| préambule seul | 635 ms | — |
| 1× `default_is_sendable(^^N20)` | 657 ms | +22 ms |
| **1 000× `default_is_sendable(^^N20)`** | **1 216 ms** | **+581 ms** |
| 1 000× `is_sendable_v<N20>` | 660 ms | +25 ms |

**Facteur 23.** Une fonction `consteval` nue n'est pas mémoïsée ; le trait l'est. Si la
bibliothèque avait été écrite comme `constexpr bool is_sendable(std::meta::info)`, elle serait
vingt-trois fois plus lente sur ce banc. **Sur une slide : « la forme `is_same_v` n'est pas de la
nostalgie de `<type_traits>`, c'est le cache. »**

### 7.4 Conclusion

**Aucun cache maison n'est justifié.** Ni table de mémoïsation `consteval`, ni `std::map<info,
bool>` à l'intérieur des fonctions du parcours, ni astuce de type friend-injection. Le compilateur
le fait déjà, gratuitement, complètement. Le dire explicitement épargne à l'auteur une journée de
travail inutile, et lui évite d'ajouter une complexité qui rendrait la démonstration moins claire
au tableau.

---

## 8. Ce qui a résisté

Les mesures de tenue en charge, prises par les agents et recoupées par mes propres compilations,
sont toutes bonnes. Elles méritent d'être dites aussi fort que les défauts.

### 8.1 Le parcours ne dégénère pas en profondeur

Une chaîne de 60 niveaux de membres imbriqués répond en **0,76 s**, et un fichier plein de
réponses fausses en **1,42 s**. Le diagnostic nomme **les 60 niveaux**, sans troncature, sans
« … », sans atteindre aucune limite. Un `struct Node { int value; Node* next; }` répond `false`
proprement, sans explosion de récursion, parce que `is_sendable<T*>` court-circuite vers
`is_synchronizable<T>` sans descendre.

### 8.2 Le choix de laisser le chemin vide dans le trait est le bon, et il est chiffré

Le commentaire de `sendable.h` (lignes 74-80) justifie de ne construire le chemin de diagnostic
que depuis `assert_sendable`, jamais depuis le trait, sous peine de rendre chaque réponse
« false » quadratique — « measured at 38x on a 60-level chain ». **Cette mesure a été refaite et
elle tient.** L'explosion quadratique est réelle, et le design l'évite. C'est une décision
d'ingénierie documentée par un nombre, et elle est correcte.

### 8.3 Ajouter une règle coûte ce qu'elle a l'air de coûter

Le correctif de diagnostic proposé en [04](./04-diagnostics.md) (Q2, le parcours des wrappers
`std`) fait passer un banc chargé en traits de **0,66 s à 0,70 s, soit +6 %**. Les évolutions
proposées par cet audit ne mettent pas le budget de compilation en danger.

### 8.4 La mémoïsation, encore

Section 7 : mille questions identiques au prix d'une. C'est un « resisted » à part entière — la
première optimisation qu'un lecteur pressé proposerait est déjà là.

---

## 9. Classement final

### À faire

| # | Action | Gain mesuré | Coût | Régression |
|---|---|---:|---|---|
| 1 | **Documenter que le coût est le parsing, pas la réflexion** (une slide, une section de `CLAUDE.md`) : `-ftime-report` donne 2 % / 9 % à `constant expression evaluation` | pédagogique | 5 lignes | — |
| 2 | **Corriger le chiffre de l'audit précédent** sur `<ranges>` : 33 ms, pas 135 ms | pédagogique | 1 phrase | — |
| 3 | **Supprimer `<algorithm>`** de `allowed_std_wrappers.h` (section 3.4), dans le même commit que le correctif cv de TC-1 | −18 ms/TU, −5 926 lignes | 8 lignes | **11/11 vertes** |
| 4 | **Documenter le plafond de profondeur** (section 6.3) : 125 niveaux, `-fconstexpr-depth ≈ 4 × profondeur` | pédagogique | 5 lignes de commentaire | — |
| 5 | **Ajouter `#include <threadsafe/details/vocabulary.h>`** à `asynchronous_task_launcher.h` (section 1.4) | hygiène | 1 ligne | **11/11 vertes** |
| 6 | **Écrire noir sur blanc qu'aucun cache n'est à ajouter** (section 7) | épargne une journée à l'auteur | 1 phrase | — |
| 7 | **Montrer `import <threadsafe/threadsafe.h>;` sur scène** : 614 ms → 32 ms, 11/11 vertes, zéro ligne changée | ×19 sur l'inclusion | 0 | **11/11 vertes** |

### À décider

| # | Action | Gain | Le compromis |
|---|---|---:|---|
| 8 | **`threadsafe/core.h`** (section 4.2) | −155 ms sur une TU utilisateur réelle (−26 %) | Un deuxième point d'entrée à documenter et à expliquer, dans une bibliothèque dont la clarté est le produit. Purement additif, aucun risque technique. **Mon avis : après la conférence.** |
| 9 | **PCH derrière une option `OFF` par défaut** (section 5.4) | −51 % sur une TU rééditée, −40 % en build sériel | **+44 % sur un build propre parallèle**, et 135 Mo sur disque. Ne jamais l'activer par défaut. |

### À ne pas faire

| # | Action | Pourquoi non |
|---|---|---|
| 10 | **Forward-declarer les templates standard** (section 3.3) | Rapporterait 55 ms. Produit **120 erreurs** dès que l'utilisateur inclut `<list>`, à cause de `std::__cxx11`, et c'est un comportement indéfini d'après `[namespace.std]/7`. Non. |
| 11 | **Supprimer `<ranges>` et la règle `borrowed_range`** (section 2.4) | Rapporterait 33 ms et laisserait la suite verte. Détruirait le meilleur message de diagnostic de la bibliothèque, qui passerait de « is a borrowed range » à « `_M_str (const char*)` ». Non. |
| 12 | **Écrire un cache de mémoïsation** (section 7) | Le compilateur le fait déjà, à 100 %. Mille questions identiques coûtent le prix d'une. Non. |
| 13 | **Modulariser en module nommé** (section 5.3) | Ne compile pas sur GCC 16.2.0 : `missing 'std::ranges::begin' or 'std::ranges::end'` dans `trait_value`, puis de 563 à 4 777 erreurs par TU de test. La *header unit*, elle, marche parfaitement. Non — pour l'instant. |
| 14 | **Semer le chemin de diagnostic depuis le trait** (section 8.2) | Mesuré à ×38 sur une chaîne de 60 niveaux, et cela abaisserait encore le plafond de la section 6. Le design actuel a raison. Non. |
