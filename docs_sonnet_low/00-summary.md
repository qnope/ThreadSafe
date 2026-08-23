# Audit ThreadSafe — Résumé exécutif

Audit couvrant cinq angles : robustesse des traits/helpers, simplicité d'implémentation, ergonomie de l'API, sûreté d'exécution et performance (compile-time + runtime). Rapports détaillés :

- [01-traits-and-helpers.md](./01-traits-and-helpers.md) — robustesse de `is_synchronizable<T>`, `is_synchronizable<const T>`, `is_lifetime_aware<T>`, `is_sendable<T>`, `copy_on_write<T>`, `synchronized_value<T>`, `asynchronous_task_launcher`.
- [02-simplicity-and-api.md](./02-simplicity-and-api.md) — simplicité d'implémentation et ergonomie de l'API pour un nouvel utilisateur.
- [03-thread-safety-and-performance.md](./03-thread-safety-and-performance.md) — sûreté runtime, performance runtime, performance à la compilation.

## Constats par sévérité

1. **[Élevé] Évasion de référence via `value_guard` de `synchronized_value`.** `operator*`/`operator->` rendent une `T&`/`T*` sans lien de durée de vie avec le verrou ; un pointeur extrait dans le scope verrouillé peut être stocké et utilisé après déverrouillage, provoquant une vraie data race (reproduit en runtime : ~50% de mises à jour perdues sur un compteur partagé). C'est la seule faille de sûreté runtime trouvée dans l'audit — voir §3.

2. **[Moyen] Piège d'ergonomie sur les constructeurs forwarding.** `may_hijack_copy_move` marque tout type ayant un constructeur templaté (même contraint, style `requires !same_as<...>`) comme entièrement non-sendable/non-synchronizable, sans aucun diagnostic explicatif — un `static_assertion failed` nu. C'est l'idiome C++ moderne le plus courant qui casse silencieusement, et c'est le point de friction API le plus aigu identifié.

3. **[Moyen] Absence totale d'attribution dans les échecs de `static_assert`.** Chaque échec de trait se réduit à `is_sendable<T> evaluated to false`, sans indiquer quel membre/base/règle est en cause. Problème transversal à toute la bibliothèque, aggravé par le caractère éducatif du projet.

4. **[Faible] Usage incorrect de `std::forward`** dans `launch_task`/`launch_scoped_task` (`utils.h`) sur des paramètres par valeur, pas des références universelles — trompeur pour un lecteur pédagogique, sans impact fonctionnel (équivalent à `std::move`).

5. **[Faible] Surcoûts de performance identifiés, non bloquants** : `std::shared_mutex` toujours utilisé dans `synchronized_value` même quand `T` n'est jamais const-synchronizable (dégrade en `unique_lock` de toute façon) ; indirection `substitute`/`extract` de `trait_value` qui ajoute un aller-retour de réflexion complet par sous-objet/trait/niveau de récursion, dominant le profil de compilation (32% en instanciation de templates).

6. **[Info] Aucune faille de sûreté trouvée** dans les quatre traits eux-mêmes (`is_synchronizable`, `is_synchronizable<const T>`, `is_sendable`, `is_lifetime_aware`) ni dans `copy_on_write`/`asynchronous_task_launcher`. Le seul comportement surprenant (closures capturant une globale statique ⇒ capture élidée ⇒ sendable=true) est correct, pas un bug.

## Top recommandations

- **Corriger l'évasion de `value_guard`** : ajouter une API scopée type `guard.with([](T& v){...})` et/ou annoter `operator*`/`operator->` avec `[[gnu::lifetime_bound]]` (supporté par GCC 16), documenter la précondition près de `lock()`/`lock_shared()`.
- **Documenter ou assouplir `may_hijack_copy_move`** : reconnaître le pattern `requires !same_as<remove_cvref_t<U>, T>`, ou à défaut exiger un opt-in explicite plutôt qu'une détection silencieuse.
- **Ajouter de l'attribution de diagnostic** aux échecs de traits (nom/type du premier sous-objet en échec via réflexion à la compilation) — priorité n°1 pour l'usage pédagogique.
- **Remplacer `std::forward` par `std::move`** dans `launch_task`/`launch_scoped_task`.
- **Optimiser `trait_value`** en épissant directement le type plutôt que `substitute`+`extract`, et conditionner `mutex_type` sur `is_synchronizable<const T>` pour éviter `shared_mutex` quand il n'apporte rien.
