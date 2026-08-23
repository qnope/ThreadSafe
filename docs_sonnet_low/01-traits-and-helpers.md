# Audit — Robustesse des traits et des helpers

Ce document couvre les résultats de robustesse pour `is_synchronizable<T>`, `is_synchronizable<const T>`, `is_lifetime_aware<T>`, `is_sendable<T>`, ainsi que pour les trois helpers de haut niveau : `copy_on_write<T>`, `synchronized_value<T>` et `asynchronous_task_launcher` (`launch_task` / `launch_scoped_task`).

Vérifications faites via `static_assert` scratch (fichiers supprimés après coup) et via compilation/exécution réelle avec `g++-16 -std=c++26 -freflection -Iinclude`.

---

## 1. Les quatre traits

### `is_synchronizable<T>` (non-const)

- Confirmé : pas de spécialisation structurelle par défaut pour un `T` brut — `is_synchronizable<int>` est **false** ; seul `is_synchronizable<const int>` est vrai.
- Seules exceptions : `std::atomic<T>` et les types fonction, qui possèdent des spécialisations dédiées.
- Reflète correctement le principe documenté « pas de sécurité par défaut sur le chemin d'écriture ».

### `is_synchronizable<const T>`

- Membres `mutable`/référence traités correctement :
  - `struct { mutable int* p; }` → la forme `const` est **false** (un pointeur `mutable` exige `is_synchronizable<int*>` complet, sans spécialisation → false).
  - Un membre pointeur non-`mutable` ne demande que `is_synchronizable<const int*>`.
  - Un membre référence perd sa référence et exige le trait **complet** du référent (`int& r` se comporte comme `is_synchronizable<int>`, pas `<const int>`) — voulu et cohérent avec « const derrière une indirection n'est jamais fait confiance ».

### `is_sendable<T>`

Règles structurelles vérifiées pour classes/membres ordinaires : pointeurs bruts, `unique_ptr<Poly>` vs `unique_ptr<FinalPoly>`, `vector<int*>` vs `vector<unique_ptr<int>>`, tableaux de pointeurs, `shared_ptr<int>` (qui reflète `is_synchronizable<int>` et non `<const int>`), constructeurs de copie non-défaultés — tout se comporte comme prévu par le code et la documentation. **Aucune faille de sûreté trouvée ici.**

### `is_lifetime_aware<T>`

Propriété transitive vérifiée à travers `unique_ptr`, tableaux, et `copy_on_write<Inner>` imbriqué — tout se propage correctement. Les membres pointeurs bruts ne sont (à raison) jamais lifetime-aware.

### Cas intéressant/surprenant : closures capturant par référence

Ce cas a nécessité trois tours d'investigation car le premier résultat était trompeur.

1. Une lambda `[&global_v]` où `global_v` a une durée de stockage statique compile avec `is_sendable == true`. En creusant `std::meta::nonstatic_data_members_of` sur le type de la closure : **zéro membre** — car le C++ n'exige pas de capturer une entité à durée de stockage statique (directement nommable), donc le compilateur élide complètement la capture. La closure ne détient réellement aucun état, donc « sendable » est *effectivement correct*, juste surprenant si l'on suppose que `[&x]` crée toujours une référence stockée.

2. En retestant avec une variable locale réelle à durée de stockage automatique (`void probe(NotSendable&) { NotSendable local_v; auto lam = [&local_v]{...}; ... }`), `nonstatic_data_members_of` rapporte **toujours 0 membre**, alors que la closure détient bien une référence réelle. C'est une **lacune de l'implémentation actuelle de la réflexion de GCC** pour les membres de capture de closure (`nonstatic_data_members_of` ne les énumère pas), pas un bug de la bibliothèque — et comme `has_unreflectable_state` traite « pas de membres visibles + pas vide » comme non-réflectable, `default_is_sendable` retourne prudemment **false** dans ce cas. Même résultat pour une capture de paramètre de fonction typé référence.

**Conclusion** : le mécanisme de défense en profondeur de la bibliothèque (`has_unreflectable_state`) absorbe la lacune de réflexion et échoue fermé (fail-closed), pas ouvert — pas d'insécurité, juste un faux négatif supplémentaire en plus du conservatisme déjà connu sur les closures par référence.

**Aucune faille de sûreté trouvée** dans les quatre traits, à travers pointeurs, références, atomiques, smart pointers, conteneurs, tableaux de pointeurs et ownership imbriquée. Le comportement surprenant (référence-vers-global-élidée-en-closure-vide ⇒ sendable) est correct.

**Recommandation** : ajouter un commentaire d'une ligne près de `has_unreflectable_state` notant que la réflexion de GCC ne peut actuellement pas voir les membres de capture de closure du tout (vérifié : `nonstatic_data_members_of` renvoie une taille de 0 pour une closure avec une vraie capture par référence), donc le rejet conservateur actuel est correct par accident plutôt que par conception, et devra être re-vérifié quand le support de réflexion mûrira.

---

## 2. Les helpers

Tous les trois se comportent conformément à leurs contrats documentés dans `CLAUDE.md`. Testés individuellement (fichiers `test_scratch_audit.cpp`, `sv_positive.cpp`, `sv_negative.cpp`, `cow_runtime.cpp` sous scratchpad), compilés avec `g++-16 -std=c++26 -freflection -Iinclude`.

### `asynchronous_task_launcher` — exigence stricte vérifiée

`launch_task` et `launch_scoped_task` refusent tous deux correctement :
- un type argument non-sendable (`NonSendable`, avec constructeur de copie fourni par l'utilisateur),
- un callable non-sendable,

via des clauses `requires` SFINAE-friendly — confirmé avec `static_assert(!can_launch_task<...>)`. Cohérent avec la suite de tests existante (`tests/test_asynchronous_task_launcher.cpp`, `tests/test_soundness_regressions.cpp`), qui couvre en plus le rejet de `reference_wrapper`/pointeur brut pour `launch_task`, les closures capturant par référence, et le laundering de constructeur de copie par classe dérivée.

Aucun chemin d'acceptation trouvé pour un type non sûr. La classe elle-même est correctement `!is_synchronizable` (simple `vector<jthread>`), donc lancer depuis deux threads concurremment est lui-même une mauvaise utilisation détectable à la compilation si englobé par exemple dans `synchronized_value`.

### `copy_on_write<T>` — conforme au contrat, au niveau trait et à l'exécution

**Trait** : `is_sendable<copy_on_write<int>>` est vrai, `is_sendable<copy_on_write<NonSendable>>` est faux — la conjonction `sendable<T> && synchronizable<const T>` fonctionne comme documenté.

**Runtime** (`cow_runtime.cpp`) :
- Deux objets `copy_on_write` partageant un bloc via le constructeur de copie implicite pointent vers le même objet (`&*a == &*b`).
- Appeler `as_mutable()` sur l'un copie d'abord (`&*a != &*b` ensuite, `a` non affecté).
- Un second appel `as_mutable()` sur l'objet désormais non partagé retourne la même adresse (pas de copie redondante).
- Un stress test à 8 threads où chaque thread possède son propre `copy_on_write` privé (jamais partagé entre threads — le système de traits l'interdit puisque `copy_on_write<T>` n'a pas de spécialisation `is_synchronizable`) n'a montré aucune incohérence.

Conforme à la doc : « a shared `T` read through `const` only; `as_mutable()` copies first whenever the block is shared. »

### `synchronized_value<T>` — exigence stricte vérifiée avec compilation accept/reject réelle

`static_assert(sendable<T>, ...)` au scope de la classe est un échec dur (non-SFINAE), donc les deux directions ont été vérifiées comme unités de traduction séparées plutôt qu'avec une `requires`-expression :
- `sv_positive.cpp` : `synchronized_value<Sendable>` compile et s'exécute.
- `sv_negative.cpp` : `synchronized_value<NonSendable>` échoue à la compilation avec exactement le diagnostic documenté : `"the mutex serializes access, but the T still crosses thread boundaries — one thread at a time — so T must be sendable"`.

Cela confirme que la valeur protégée par mutex ne laisse jamais un `T` non-sendable atteindre `lock()`/`lock_shared()`, et `value_guard` lui-même est correctement forcé non-sendable/non-lifetime-aware dans les deux branches de la sélection shared/unique-lock de `const_guard`, si bien qu'un verrou (ou le pointeur qu'il protège) ne peut jamais lui-même traverser une frontière de thread.

### Conclusion

Aucun cas trouvé où `asynchronous_task_launcher` accepte un type non-sûr/non-sendable, et aucun cas trouvé où `copy_on_write` ou `synchronized_value` permettent une data race ou un partage inter-thread non voulu. Les trois correspondent à leurs contrats CLAUDE.md. **Aucun fix de code nécessaire** — pas de code problématique à rapporter.

La suite de tests existante (`tests/test_asynchronous_task_launcher.cpp`, `tests/test_soundness_regressions.cpp`, `tests/test_copy_on_write.cpp`, `tests/test_synchronized_value.cpp`) couvre déjà indépendamment presque toutes les mêmes limites accept/reject sondées ici.

**Lacune de couverture (pas un bug)** : aucun test ne construit deux instances `copy_on_write` partageant un bloc pour vérifier le comportement runtime réel de copy-on-write de `as_mutable()`. Ce serait un ajout permanent raisonnable à `tests/test_copy_on_write.cpp`.

---

*Fichiers scratch : aucun laissé dans le dépôt — tout le travail scratch était sous `/private/tmp/claude-501/-Users-amorrier-Programmation-ThreadSafe/a9241fcc-858d-44aa-8b7c-232bb71372a4/scratchpad/` (`test_scratch_audit.cpp`, `sv_positive.cpp`, `sv_negative.cpp`, `cow_runtime.cpp`).*
