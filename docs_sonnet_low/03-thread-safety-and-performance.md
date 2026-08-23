# Audit — Sûreté d'exécution et performance (runtime + compile-time)

## Tâche A — Sûreté des threads (runtime)

**Aucune race trouvée à l'intérieur de `synchronized_value`, `copy_on_write` ou `asynchronous_task_launcher` elles-mêmes.** Leurs sections critiques internes sont correctes : `synchronized_value` associe toujours `mutex_` à `value_` via `unique_lock`/`shared_lock`, la séquence `use_count()`-puis-copie de `copy_on_write::as_mutable()` est sûre car `ptr_` est un membre par-objet, par-thread (jamais aliasé entre threads sous le modèle de traits — envoyer exige un transfert, pas un partage), et le launcher se contente de transférer des copies possédées vers `std::jthread`.

La première tentative a consisté à contourner directement la couche de *trait* : construire `synchronized_value<int*>` ou `shared_ptr<int>` et faire une race sur le pointee via deux wrappers verrouillés séparément. Les deux sont correctement **rejetés à la compilation** : `is_sendable<int*> == is_synchronizable<int>`, et `is_synchronizable<int>` non-const n'a pas de défaut structurel — c'est `false` sauf spécialisation explicite (atomiques, types fonction, ou échappatoire explicite). Donc un `int*`/`int&`/`shared_ptr<int>` brut ne peut pas entrer dans `synchronized_value` ni traverser les threads du tout. Cette partie de la conception est réellement solide et plus conservatrice qu'attendu.

### La vraie faille exploitable : l'évasion de guard

`[[nodiscard]] guard lock()` empêche seulement une temporaire *nue et non assignée* de se déverrouiller immédiatement — ça ne fait rien une fois le guard décomposé. `value_guard::operator*()` retourne `T&` sans aucun lien de durée de vie avec le guard, donc rien n'empêche un pointeur/référence brut obtenu à l'intérieur de la portée verrouillée d'être stocké et utilisé après que le guard (et son `unique_lock`) ait été détruit. À ce moment, toute synchronisation a disparu : le pointeur évadé peut être racé depuis n'importe quel thread sans aucun verrouillage.

C'est un trou classique d'évasion de référence, et c'est exactement le genre de chose qu'`is_lifetime_aware` a été conçu pour empêcher pour l'*ownership*, mais `value_guard` désactive les deux traits (`is_sendable = false`, `is_lifetime_aware = false`) — ce qui empêche le *guard lui-même* d'être envoyé ou stocké, mais ne fait rien pour empêcher un simple `T*`/`T&` extrait de lui d'être stocké, puisque ce pointeur/référence n'est pas le guard.

Vérifié avec un programme runtime (`g++-16 -std=c++26 -freflection`) :

```cpp
synchronized_value<int> sv(0);
int* p = nullptr;
{
    auto g = sv.lock();
    p = &*g;              // escapes the locked scope
}                          // mutex released — p now unguarded
// two std::thread workers each do `*p += 1;` 500000 times, no lock
```

Résultat sur trois runs : `expected=1000000 actual=525099 / 508583 / 503397` — environ la moitié des incréments sont perdus, confirmant une véritable race de données non synchronisée en `-O0` (le runtime de ThreadSanitizer n'est pas linkable sur cette config GCC 16/arm64, d'où l'utilisation d'un compteur à mise à jour perdue, qui est un témoin de race standard et suffisant).

### Correction

Deux changements complémentaires :

1. Faire du guard le seul moyen d'atteindre `T`, et le rendre non-copiable/non-déplaçable pour qu'il ne puisse vraiment pas être contrebandé au-delà du bloc ayant pris le verrou (déjà vrai), mais en plus **empêcher `operator*`/`operator->` de rendre une `T&`/`T*` non scopée** — donner à la place à `value_guard` une API à callback scopé pour que la référence ne puisse littéralement pas survivre au verrou :

```cpp
template <class F>
auto with(F&& f) & -> decltype(auto) { return std::forward<F>(f)(*value_); }
```

utilisé comme `sv.lock().with([](int& v) { v += 1; });` — la référence n'existe que dans la frame de la lambda, qui est prouvablement à l'intérieur de la durée de vie du verrou.

2. Comme supprimer `operator*`/`operator->` casserait l'API ergonomique/testée existante, ajouter au minimum une **annotation documentée du style `[[clang::lifetimebound]]`** (GCC 16 supporte `[[gnu::lifetime_bound]]`) sur la valeur de retour de `operator*`/`operator->`, qui transforme le pattern exact ci-dessus (`int* p = &*g;` survivant à `g`) en avertissement/erreur du compilateur pour le cas courant d'un local qui s'échappe de sa portée englobante. Ça n'attrape pas toute évasion (par ex. à travers un champ de `struct` comme dans la reproduction), mais c'est un gain incrémental peu coûteux, en plus de documenter explicitement la précondition à côté de `lock()`/`lock_shared()`, à l'image du style de commentaire de précondition déjà utilisé pour `launch_scoped_task` ailleurs dans le codebase.

`copy_on_write` et `asynchronous_task_launcher` n'ont aujourd'hui aucune évasion analogue : `copy_on_write::operator*`/`operator->` retournent `const T&`/`const T*` sans verrou à survivre, et la seule ressource non protégée de `as_mutable()` est le compte de référence du `shared_ptr`, qui est atomique par conception. Aucune correction nécessaire là-bas.

---

## Tâche B — Performance runtime

### `synchronized_value`

- `lock_shared()`'s `const_guard` choisit entre `shared_lock` et `unique_lock` via `is_synchronizable<const T>` — bien, ça évite déjà de sérialiser les lecteurs quand `T` permet un accès `const` concurrent. Aucun changement nécessaire.
- `std::shared_mutex` est plus lourd (typiquement deux atomiques + syscall possible en cas de contention) que `std::mutex`. Pour le cas courant où `T` n'est *pas* `is_synchronizable<const T>` (donc `const_guard` dégénère de toute façon en `unique_lock` — la distinction lecteur/écrivain n'est jamais exercée), `shared_mutex` n'apporte rien et coûte plus cher qu'un simple `mutex`.

**Amélioration concrète** : spécialiser le type de mutex selon `is_synchronizable<const T>` :

```cpp
using mutex_type = std::conditional_t<is_synchronizable<const T>,
                                       std::shared_mutex, std::mutex>;
```

avec le type de verrou de `const_guard` suivant le même principe. Ceci élimine entièrement le surcoût de `shared_mutex` dans le cas (probablement courant) où `T` n'a pas de chemin de lecture concurrente sûre.

### `copy_on_write`

- Le test `ptr_.use_count() != 1` de `as_mutable()` est un simple load atomique, peu coûteux et déjà minimal — pas de remarque là-dessus.
- Le vrai coût est `std::make_shared<T>(*ptr_)` à chaque écriture quand partagé : c'est une copie profonde inévitable étant donné la sémantique, mais noter que `ptr_ = std::make_shared<T>(*ptr_)` effectue la copie-construction *avant* que l'ancien `shared_ptr` soit relâché, donc le pic mémoire contient brièvement l'ancien et le nouveau `T` plus deux control blocks — acceptable, pas de correction nécessaire, juste à signaler comme coût COW attendu, pas un bug.
- Mineur : le comptage de référence du control-block de `shared_ptr` utilise un RMW atomique même dans le cas totalement mono-propriétaire (`use_count()==1` après `as_mutable()`), ce qui est inhérent à `shared_ptr` et n'est pas quelque chose que `copy_on_write` peut éviter sans passer à un compte de référence intrusif personnalisé — pas pertinent pour une bibliothèque « axée sécurité, éducative » selon le cadrage de `CLAUDE.md` ; signalé uniquement comme coût connu et accepté, pas comme recommandation de changement.

*Fichiers scratch supprimés : `race_demo.cpp`/binaire `race_demo` supprimés du scratchpad après le run ; rien ajouté au dépôt.*

---

## Tâche C — Performance à la compilation

### Méthode

Suite de tests complète (10 TU) construite avec `g++-16 -std=gnu++26 -O3 -freflection` :

```
cmake -B build -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_BUILD_TYPE=Release
cmake --build build --target clean
time cmake --build build -- -j1
```

Build séquentiel complet : **7.30s user, 10 TUs**. Timing `-fsyntax-only` par TU (tous denses en traits, taille similaire) : chaque fichier est **0.59–0.63s**, essentiellement plat — la suite est trop petite pour localiser le coût au seul temps d'horloge murale, donc `-ftime-report` sur le fichier le plus lourd (`test_synchronizable.cpp`) a été utilisé :

```
 name lookup             : 0.05 ( 8%)   6.7M ( 3%)
 overload resolution     : 0.12 (19%)    32M (13%)
 parser (global)         : 0.06 (10%)    55M (22%)
 parser struct body      : 0.08 (12%)    34M (14%)
 parser inl. meth. body  : 0.09 (14%)    47M (19%)
 template instantiation  : 0.20 (32%)    74M (30%)
 TOTAL                   : 0.64          251M
```

L'instanciation de templates domine à la fois en temps (32%) et en mémoire (30%), avec la résolution de surcharge en proche second — les deux pilotés par la même source : la machinerie récursive d'évaluation des traits.

### Le pire coupable : `detail::trait_value` (`include/threadsafe/details/utils.h`)

```cpp
inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
    return std::meta::extract<bool>(std::meta::substitute(trait, {type}));
}
```

Chacun des `is_synchronizable_type`, `is_sendable_type`, `is_lifetime_aware_type` (la « face niveau info » utilisée pour *chaque* base et *chaque* membre pendant la récursion) passe par cette fonction. `substitute()` construit une nouvelle liste d'arguments de template et force le compilateur à instancier le variable-template primaire `is_synchronizable<T>` (ou `is_sendable<T>`/`is_lifetime_aware<T>`) en tant que spécialisation fraîche ; `extract<bool>` relit ensuite la constante résultante à travers la machinerie d'évaluation constante. C'est une spécialisation complète de variable-template plus un aller-retour de réflexion **par sous-objet, par trait, par niveau de récursion** — pour un type avec B bases et M membres, le seul contrôle const-synchronizable émet jusqu'à `B+M` tels aller-retours, chacun ré-entrant la résolution de surcharge sur les cinq spécialisations partielles d'`is_synchronizable` (primaire, `T&`, `T*`, `T[N]`, `T[]`, `const T`, `atomic<T>`, types fonction...) avant d'atteindre la bonne. Cet ensemble de surcharges est parcouru depuis zéro à chaque fois — rien ici n'est mémoïsé entre appels frères même quand deux membres partagent le même type.

Comme le même `T` est re-dérivé structurellement (`add_const`, `remove_cv`, `remove_reference`, `remove_pointer`) à chaque site d'appel dans `synchronizable.h`/`sendable.h`/`lifetime_aware.h` plutôt que calculé une fois et réutilisé, et comme le cache de spécialisation propre d'`is_synchronizable<T>` (la seule chose que le compilateur mémoïse automatiquement) est indexé sur le `T` exact avec ses qualificateurs cv/référence, des orthographes légèrement différentes du « même » type (`const T` vs `T` vs `remove_cv_t<T>`) manquent ce cache et redéclenchent tout le parcours structurel.

### Idées concrètes et actionnables

1. **Sauter `substitute`+`extract`, épisser directement.** `trait_value(trait, type)` peut être remplacé par un template prenant le trait comme paramètre de template de template et épissant le type : `template<template<class> class Trait> consteval bool trait_value(std::meta::info type) { return Trait<[:type:]>; }`. Ceci retire un aller-retour complet de réflexion (construire une substitution, puis extraire une constante à travers l'évaluation `consteval`) par appel et laisse le compilateur aller directement à la recherche de spécialisation, le chemin le moins cher à travers la résolution de surcharge.

2. **Normaliser le type une seule fois par nœud de récursion.** Dans `default_is_const_synchronizable` / `default_is_sendable` / `default_is_lifetime_aware`, calculer `remove_cv(member_type)` (ou la transformation nécessaire) dans un `info` local une fois, et passer ce handle normalisé unique aux vérifications, au lieu d'appeler `remove_cv`/`add_const` en ligne à chacun des 2-3 sites d'appel touchant le même membre. Petit, mais multiplié par chaque membre de chaque type de la suite.

3. **Réordonner les branches pour que le rejet le moins cher se déclenche en premier.** Les trois fonctions `default_is_*` font actuellement `is_synchronizable_type(type)` (vérification structurelle récursive complète) *avant* les tests O(1) `is_scalar_type`/`is_void_type` dans `default_is_const_synchronizable`, mais `default_is_sendable` fait déjà le bon pattern inverse (`is_synchronizable_type(T) || is_scalar_type(T)` — scalar ne peut pas court-circuiter en premier puisque c'est un OR). Concrètement : hisser les vérifications `is_scalar_type`/`is_void_type`/complétude *avant* `has_only_default_copy_move_destroy`/`has_unreflectable_state`, qui elles-mêmes itèrent `members_of` — actuellement un membre scalaire force deux passes d'itération de membres dont il n'a jamais besoin.

4. **Mettre en cache les résultats de `has_only_default_copy_move_destroy` et `has_unreflectable_state` par type.** Les deux sont appelées une fois pour `is_sendable<T>` et à nouveau pour `is_synchronizable<const T>` sur le même `T` dans du code typique (par ex. un membre vérifié à la fois comme sendable et comme const-synchronizable dans une struct à membres mixtes mutable/plain). Un petit mémo `consteval` indexé par `std::meta::info` (un sondage linéaire adossé à un `std::array` construit à la compilation, puisque `consteval` interdit les hash maps persistantes entre TU mais autorise les statiques locales à une fonction au sein d'une TU) supprimerait le parcours `members_of` dupliqué.

### Bilan

À la taille actuelle de la suite (10 petites TU, ~0.6s chacune), le temps de compilation n'est pas encore un problème pratique — le vrai signal est dans `-ftime-report`, pas le temps d'horloge murale. Mais le profil à 32% en instanciation de templates confirme que les vérifications récursives de traits sont le centre de coût, et l'indirection substitute/extract de `trait_value` est le changement unique (item 1) le plus susceptible de montrer un gain mesurable à mesure que les types se profondront/s'élargiront, car il retire du surcoût de *chaque* arête récursive du graphe de traits, pas seulement d'un chemin de code.
