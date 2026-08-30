# Audit — Simplification du code et réduction du nombre de lignes (synthèse)

Date : 2026-08-30 · Périmètre : `include/threadsafe/` (1 301 lignes) et `tests/` (1 647 lignes) · Détails : [audit-simplification-details.md](audit-simplification-details.md)

## Résultat

**18 simplifications validées** (chacune vérifiée adversarialement : code cité exact, comportement inchangé, architecture CLAUDE.md respectée), pour un gain estimé de **~127 lignes** (~95 dans la bibliothèque, ~32 dans les tests). 9 pistes supplémentaires ont été examinées puis réfutées (voir détails).

## Priorités hautes

| Fichier | Simplification | Gain |
|---|---|---|
| `allowed_std_wrappers.h` | Trois boucles identiques sur `wrapped_types_of` factorisables en une seule | ~18 |
| `synchronizable_base.h` | `detail::diagnose_is_synchronizable` (un seul appelant) à replier dans `is_synchronizable::diagnose()` — lit alors le memo `_v`, plus conforme à l'architecture | ~8 |

## Priorités moyennes

| Fichier | Simplification | Gain |
|---|---|---|
| `sendable.h` | Tronc structurel dupliqué entre `diagnose_is_sendable` et `diagnose_is_const_synchronizable` | ~17 |
| `synchronized_value.h` | `get_mutex_type` / `get_const_guard_type` remplaçables par `std::conditional_t` | ~14 |
| `smart_pointers.h` | Trois `diagnose()` de `unique_ptr` structurés à l'identique | ~10 |
| `asynchronous_task_launcher.h` | Deux helpers d'assertion jumeaux fusionnables | ~5 |
| `lifetime_aware.h` | `shared_ptr` / `weak_ptr` : `diagnose()` dupliqué mot pour mot | ~4 |
| `sendable.h` | `is_unsafe_sendable<T&>` / `<T&&>` : corps identiques, déléguer via le memo | ~3 |
| `utils.h` | `prepend_path` : construction du vecteur en 2 étapes (`append_range`) au lieu de 4 | ~3 |
| `synchronizable.h` | Concept `function_type` à usage unique → clause `requires` inline | ~2 |
| tests (4 fichiers) | Alias `can_launch_task` / `can_launch_scoped_task` : renommage pur d'un concept existant | ~12 |
| `test_synchronized_value.cpp` | Requires-expression maison au lieu du concept `launchable_scoped_task` | ~2 |

## Priorités basses

Boilerplate de tests dupliqué (`SyncType` ×4, `NonSendable` ×3, ~16 lignes), helpers à usage unique dans `utils.h` (~6), quatre spécialisations tableau réductibles dans `synchronizable_base.h` (~4), includes morts (`<type_traits>` dans `vocabulary.h`, `<span>`/`<atomic>` dans `test_soundness_regressions.cpp`, ~3).

## Ce qu'il ne faut PAS simplifier

La vérification a rejeté notamment : factoriser les 9 spécialisations « vouch » de `vocabulary.h` par macro (contraire à l'esprit pédagogique et explicite de l'architecture), fusionner par héritage les spécialisations `T&`/`T&&` et `T[N]`/`T[]` de `lifetime_aware.h` (compilerait, mais l'architecture interdit l'héritage entre traits), et replier `diagnose_task_participant` en ternaire (gain illusoire).
