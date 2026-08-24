# Performance à la compilation

La tâche pose la bonne question : « les checks étant faits à la compilation,
c'est normal que ça prenne du temps, mais s'il y a des possibilités
d'améliorer, liste-les ». La réponse mesurée est contre-intuitive et mérite
d'être dite en conférence : **les vérifications ne coûtent presque rien ; ce sont
les en-têtes standard qui coûtent.**

## Référence

Suite complète, `cmake --build build --clean-first -j 12` : **1,83 s** au mur,
11,85 s CPU. Par unité de traduction, `-fsyntax-only`, meilleur de 3 :

| TU | temps |
|---|---|
| `test_synchronizable.cpp` | 648 ms |
| `test_sendable.cpp` | 644 ms |
| `test_diagnostics.cpp` | 636 ms |
| `test_soundness_regressions.cpp` | 634 ms |
| `test_asynchronous_task_launcher.cpp` | 632 ms |
| `test_containers.cpp` | 622 ms |
| `test_copy_on_write.cpp` | 620 ms |
| `test_smart_pointers.cpp` | 617 ms |
| `test_synchronized_value.cpp` | 604 ms |
| `test_lifetime_aware.cpp` | 600 ms |
| `test_deferred_specialization.cpp` | 589 ms |
| **somme série** | **6 846 ms** |

Les mesures reproduisent à ~1,5 % près d'une série à l'autre (contrôle en
meilleur-de-5 : `<meta>` 210 ms, inclusion seule 586 ms, TU la plus lourde
642 ms). L'écart entre le fichier le plus lourd et le plus léger est de 59 ms, alors que
`test_synchronizable.cpp` contient 144 lignes de `static_assert` et
`test_deferred_specialization.cpp` une trentaine. C'est le premier indice.

## La mesure décisive

Une TU **vide** qui ne fait qu'inclure l'en-tête parapluie :

| fichier | temps |
|---|---|
| `#include <threadsafe/threadsafe.h>` puis `int main(){}` | **594 ms** |
| `#include <meta>` puis `int main(){}` | **211 ms** |

Sur une TU typique à 620 ms :

- **211 ms** — `<meta>` lui-même, plancher incompressible de tout code à réflexion ;
- **383 ms** — les en-têtes standard tirés par le parapluie ;
- **30 à 60 ms** — *l'intégralité* du travail de traits du fichier, soit **5 à 9 %**.

`-ftime-report` confirme l'attribution, en comparant la TU la plus lourde à la TU
vide :

| phase | `test_synchronizable.cpp` | inclusion seule | écart |
|---|---|---|---|
| TOTAL | 0,67 s | 0,61 s | **0,06 s** |
| `constant expression evaluation` | 0,07 s (10 %) | 0,01 s (2 %) | **0,06 s** |
| `template instantiation` | 0,20 s (30 %) | 0,20 s (33 %) | **0,00 s** |
| `phase parsing` | 0,50 s (76 %) | 0,43 s (72 %) | 0,07 s |

Tout le travail des traits atterrit dans `constant expression evaluation`, et il
pèse 0,06 s. La ligne `template instantiation` est **identique** dans les deux
fichiers : elle est entièrement due à l'instanciation des en-têtes standard et ne
contient aucun travail de trait.

### Correction de l'audit précédent

`docs_sonnet_low/00-summary.md`, point 5, attribuait le coût à l'indirection
`substitute`/`extract` de `trait_value`, « dominant le profil de compilation
(32 % en instanciation de templates) ». Ce chiffre de 30-32 % est présent dans un
fichier qui ne pose **aucune** question de trait. L'attribution était erronée, et
optimiser `trait_value` reviendrait à travailler sur moins de 8 % du coût.

## Le vrai levier : la granularité des en-têtes

Coût d'une TU vide selon ce qu'elle inclut :

| inclusion | temps |
|---|---|
| `<meta>` seul (plancher) | 210 ms |
| `details/synchronizable_base.h` | 215 ms |
| `details/synchronizable.h` (+ `sendable.h`) | **235 ms** |
| `+ lifetime_aware.h` | 370 ms |
| `details/containers.h` seul | 397 ms |
| `+ synchronized_value.h` | 552 ms |
| `+ asynchronous_task_launcher.h` | 588 ms |
| `threadsafe/threadsafe.h` (ce que tout le monde inclut) | **592 ms** |

Les deux questions dont la bibliothèque *parle* — `is_sendable` et
`is_synchronizable` — coûtent 235 ms, soit 25 ms au-dessus du plancher
irréductible de `<meta>`. Le parapluie coûte 592 ms.

D'où viennent les paliers :

- `lifetime_aware.h` **+135 ms** : tire `<ranges>` pour `std::ranges::borrowed_range`,
  plus `<memory>` et `<functional>` ;
- `containers.h` **+162 ms** : tire dix en-têtes de conteneurs ;
- `synchronized_value.h` / `asynchronous_task_launcher.h` **+180 ms** : tirent
  `<thread>`, `<mutex>`, `<shared_mutex>`.

### Correctif proposé, mesuré

Le découpage existe déjà dans `details/` ; il n'est simplement pas offert à
l'utilisateur. Trois parapluies publics suffisent. **Vérifié : les 11 TU de la
suite existante compilent sans modification.**

`include/threadsafe/traits.h` :

```cpp
#pragma once

// The three questions the library is about, and nothing else: no helper, no
// standard-library specialisation. Include this when you only need to ask.
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/lifetime_aware.h>
```

`include/threadsafe/std.h` :

```cpp
#pragma once

// What the traits answer for the standard library. Costs the standard headers
// it speaks about, which is why it is separate from threadsafe/traits.h.
#include <threadsafe/traits.h>
#include <threadsafe/details/containers.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
```

`include/threadsafe/helpers.h` :

```cpp
#pragma once

// The three types that put the traits to work.
#include <threadsafe/std.h>
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/details/synchronized_value.h>
#include <threadsafe/details/copy_on_write.h>
```

`include/threadsafe/threadsafe.h` devient :

```cpp
#pragma once

// Everything. Prefer threadsafe/traits.h if you only need to ask the questions.
#include <threadsafe/helpers.h>
```

Gain mesuré :

| inclusion | temps |
|---|---|
| `threadsafe/traits.h` | **371 ms** |
| `threadsafe/std.h` | 445 ms |
| `threadsafe/helpers.h` | 588 ms |
| `threadsafe/threadsafe.h` | 619 ms |

**619 → 371 ms, soit 40 %** pour l'utilisateur qui ne veut que les traits. Pas les
60 % que suggèrent les chiffres bruts de `details/`, parce que `traits.h` inclut
`lifetime_aware.h`, qui tire `<ranges>` : ce seul concept `borrowed_range` pèse
~135 ms, soit 36 % du chiffre « traits seuls ».

C'est un fait qui vaut d'être dit sur scène : **dans cette bibliothèque, la partie
la moins chère est la réflexion ; la partie chère est `<ranges>`.**

## Le comportement de la récursion : mémoïsée et linéaire

Trois questions se posent sur une récursion réflexive, et la mesure y répond
clairement.

### Le résultat est-il mémoïsé ?

Oui, complètement. Poser **mille fois** la même question sur un type profond de 30
niveaux coûte le même prix que la poser une fois :

| même type, posé N fois | temps |
|---|---|
| N = 1 | 614 ms |
| N = 10 | 612 ms |
| N = 100 | 613 ms |
| N = 1000 | **616 ms** |

C'est la mémoïsation ordinaire de l'instanciation de templates qui opère :
`is_sendable_v<T>` est une variable template, donc instanciée une seule fois par
`T`. Le contrôle, avec des types **distincts**, montre bien le coût réel :

| N types distincts, 30 niveaux chacun | temps |
|---|---|
| N = 1 | 614 ms |
| N = 5 | 671 ms |
| N = 10 | 740 ms |
| N = 20 | 884 ms |

soit ~14 ms par type distinct de 30 niveaux. La crainte d'une marche recalculée à
chaque interrogation est donc infondée, et cela explique pourquoi la suite entière
coûte si peu au-delà de la lecture des en-têtes.

### Comment cela passe-t-il à l'échelle ?

Linéairement, sur les deux axes.

| profondeur (chaîne de D structs imbriqués) | temps | | largeur (W membres distincts) | temps |
|---|---|---|---|---|
| D = 1 | 614 ms | | W = 1 | 607 ms |
| D = 10 | 607 ms | | W = 10 | 629 ms |
| D = 50 | 631 ms | | W = 50 | 630 ms |
| D = 100 | 655 ms | | W = 100 | 674 ms |
| D = 125 | 664 ms | | W = 200 | 709 ms |
| | | | W = 400 | 829 ms |

**~0,4 ms par niveau de profondeur, ~0,55 ms par membre.** Aucun comportement
quadratique, aucune explosion.

### Y a-t-il un plafond ?

Oui, et il mérite d'être connu. Le trait répond jusqu'à **125 niveaux
d'imbrication** ; à **126**, il ne répond plus :

```
error: 'constexpr' evaluation depth exceeds maximum of 512
       (use '-fconstexpr-depth=' to increase the maximum)
```

Le plafond est celui de `-fconstexpr-depth` (512 par défaut), pas celui de
`-ftemplate-depth` — augmenter ce dernier ne change rien, alors que
`-fconstexpr-depth=4000` fait passer un type de 400 niveaux. La récursion consomme
donc ~4 niveaux d'évaluation constexpr par niveau d'imbrication.

125 niveaux est bien au-delà de tout code réel, donc ce n'est pas un problème
pratique. Ce qui l'est, c'est **le message** que reçoit celui qui l'atteint :

```
/opt/homebrew/.../c++/16/bits/allocator.h:199:41: error: 'constexpr' evaluation
    depth exceeds maximum of 512
include/threadsafe/details/sendable.h:21:48: error: 'value' is not a member of
    'threadsafe::is_sendable<N0>'
```

Ni `allocator.h`, ni `N0` — le type le plus *interne* de la chaîne — n'ont quoi que
ce soit à voir avec la cause. Une ligne dans la documentation suffit : *si vous
voyez « constexpr evaluation depth », votre type dépasse ~125 niveaux
d'imbrication ; compilez avec `-fconstexpr-depth=4000`.*

## Ce qu'il ne sert à rien d'optimiser

Compte tenu de ce qui précède, les pistes suivantes sont mesurément sans intérêt
pour cette bibliothèque :

- **Optimiser `detail::trait_value`** (l'aller-retour `substitute`/`extract`) : cible
  moins de 9 % du coût, et le mécanisme est de toute façon contraint — l'alternative
  évidente est mal formée, voir [05-simplicite.md](./05-simplicite.md).
- **Remplacer les exceptions `consteval` par un retour booléen** : le travail de traits
  entier pèse 0,06 s sur une TU ; l'exception achète des diagnostics nettement
  meilleurs pour une fraction de cela.
- **Ajouter un cache maison** : le compilateur en a déjà un, et il est parfait.

Le seul levier réel reste la granularité des en-têtes.

## Ce que le découpage ne change pas

Le parapluie reste disponible et inchangé pour la démonstration. Le découpage n'a
aucun coût de maintenance — les fichiers `details/` sont déjà correctement
factorisés — et il rend le **feuilletage visible** : traits, puis réponses sur la
bibliothèque standard, puis helpers. C'est en soi le propos pédagogique.
