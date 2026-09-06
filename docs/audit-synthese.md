# Audit ThreadSafe — Synthèse

_Audit multi-agents du 2026-09-06 (46 agents, findings vérifiés adversarialement par compilation g++-16). Détails complets, code problématique et corrections : [audit-details.md](audit-details.md)._

**Bilan** : 32 findings confirmés — 3 critiques, 12 majeurs, 17 mineurs — plus 16 observations informatives. 7 findings ont été réfutés à la vérification et écartés.

## Les 3 critiques (trous de soundness, vérifiés au compilateur)

1. **Pointeur de fonction mutable déclaré synchronizable** (`synchronizable_base.h`) — le raccourci `is_function_type(remove_pointer(type))` précède le garde `!is_const`, donc `is_synchronizable_v<void(*)()> == true`. Deux threads peuvent lire/réassigner le même pointeur de fonction : data race. Fix : ne court-circuiter que les vrais types fonction, laisser les pointeurs de fonction passer par le garde const.

2. **`const` derrière une indirection est cru par `is_sendable`** (`sendable.h`) — `is_sendable_v<const int*>` et `is_sendable_v<const int&>` valent `true` car la branche référence ne fait pas `remove_cv` sur le référent, contrairement à la couche smart-pointer (`shared_ptr<const int>` est correctement refusé). Viole la règle « const behind an indirection is never trusted ».

3. **`is_sendable_v<int&&> == true`** (`sendable.h`) — les rvalue references sont traitées comme des valeurs alors qu'elles alias un objet d'un autre thread.

## Majeurs — l'essentiel

- **Traits** : `is_synchronizable_v<T>` implique `is_sendable_v<T>` (divergence avec Rust où Sync ⇏ Send) ; `weak_ptr` vouché `lifetime_aware` alors qu'il ne garde rien en vie.
- **Helpers** : `synchronized_value::lock()` appelable sur un temporaire (garde pendante) ; `copy_on_write` déréférençable après move (nullptr) et `as_mutable()` rend une `T&` qui survit à la garantie d'unicité (aliasing race si le bloc est repartagé ensuite).
- **Éducatif** : la `atomic_thread_fence(acquire)` de `copy_on_write` n'est pas expliquée ; dépendance d'include cachée dans `smart_pointers.h`.
- **API / compile-time** : les fonctions `diagnose_*` ne diagnostiquent rien (aucun chemin vers le membre fautif) ; pas de concept `synchronizable` alors que `sendable` et `lifetime_aware` existent ; vouch complet = 3–4 spécialisations manuelles ; `members_of` itéré jusqu'à 3 fois par type (`is_default_type`/`is_walkable_type` recalculés par trait).

## Mineurs — thèmes récurrents

- Code mort : `is_smart_pointer_v`, branche lvalue-reference inatteignable dans le walk synchronizable, `remove_pointer` inutile dans `sendable.h`.
- Duplication : `pointee_is_synchronizable` répété 4×, `trait_value` réimplémenté dans `lifetime_aware.h`.
- Style incohérent entre headers (indentation, `bool_constant`, nommage du launcher).
- Launcher : `threads_` croît sans borne ; `launch_scoped_task` spawn un thread pour le joindre aussitôt ; passage par valeur + move au lieu d'une decay-copy.
- Spécialisations utilisateur `is_unsafe_sendable<const T>` silencieusement mortes.

## Points solides

- `is_lifetime_aware` : aucun type empruntant accepté à tort (string_view, span, itérateurs, lambdas à capture par référence — tout est refusé).
- La fence acquire de `copy_on_write::as_mutable` est correcte (mais mérite un commentaire).
- La couche smart-pointer applique correctement la règle du const-derrière-indirection — c'est le walk brut qui ne la suit pas.

## Priorités recommandées

1. Corriger les 3 critiques + régressions `static_assert` associées.
2. `copy_on_write` : bloquer le use-after-move et repenser `as_mutable()` (retourner une garde plutôt qu'une `T&`).
3. Qualifier `lock()` en `&` (ref-qualifier) sur `synchronized_value`.
4. Mutualiser le walk (`members_of` une seule fois) et brancher de vrais diagnostics sur `diagnose_*`.
