# Audit de simplification — synthèse

**Date** : 2026-08-30
**Objet** : simplification du code au maximum et réduction du nombre de lignes.
**Méthode** : workflow multi-agents — 14 agents de détection (un par header, tests par lots),
puis vérification adversariale de chaque proposition (relecture du fichier réel, grep des
usages, compilation complète GCC 16 avec le patch appliqué). 14 propositions examinées,
**10 confirmées**, 4 rejetées.

Détails complets (code + correction) : [audit-simplification-details.md](audit-simplification-details.md).

## Bilan

**≈ 34 lignes économisées** au total, mais l'essentiel du gain est la suppression de
duplication : quatre corps de `diagnose()` dupliqués mot pour mot disparaissent, la chaîne
`"borrows its referent instead of keeping it alive"` passe de trois occurrences à une seule,
et six spécialisations jumelles deviennent trois. Le code est déjà très sobre — aucun agent
n'a trouvé de simplification dans `utils.h`, `synchronizable.h`, `synchronized_value.h` ni
`copy_on_write.h`.

## Propositions confirmées (par impact)

| # | Fichier | Simplification | Lignes | Impact |
|---|---------|----------------|--------|--------|
| 1 | `vocabulary.h` | Fusionner les 6 spécialisations stop_token/stop_source via un concept `stop_handle` | −11 | moyen |
| 2 | `smart_pointers.h` | Les specs *sendable* de `shared_ptr`/`weak_ptr`/`reference_wrapper` délèguent à `is_synchronizable_v<const X>` au lieu de dupliquer son corps | −5 | moyen |
| 3 | `sendable.h` | `T[N]` délègue à `T[]` (corps identiques) | −4 | moyen |
| 4 | `allowed_std_wrappers.h` | Fusionner `wrapped_types_of` dans son unique appelant `all_wrapped_types` (supprime aussi une allocation `std::vector` consteval) | −4 | moyen |
| 5 | `asynchronous_task_launcher.h` | Garde morte `if (answer) return {};` dans `detail::explain` (P2741 : le message d'un static_assert n'est évalué que si la condition est fausse) | −3 | faible |
| 6 | `lifetime_aware.h` | `T&&` délègue au mémo de `T&` au lieu de recopier la raison | 0 | faible |
| 7 | `lifetime_aware.h` | `reference_wrapper<T>` délègue au mémo de `T&` (la raison n'existe plus qu'à un endroit) | 0 | faible |
| 8 | `synchronizable_base.h` | `#include <type_traits>` inutilisé | −1 | faible |
| 9 | `test_sendable.cpp` | Corps `diagnose() { return {}; }` sur une ligne, style des headers | −4 | faible |
| 10 | `test_diagnostics.cpp` | Factoriser `reasons_match` sur `reason_is` | −2 | faible |

Chaque proposition confirmée a été vérifiée par compilation réelle : patch appliqué,
`cmake --build build` vert (tous les `static_assert` passent, y compris `test_diagnostics.cpp`
qui verrouille les chemins et messages exacts), puis arbre restauré. **Aucune modification
n'a été appliquée au dépôt** — c'est un audit.

Note sur la proposition 9 : pour rester cohérent, la même compaction devrait couvrir les
13 sites équivalents de tous les fichiers de tests d'un coup, pas seulement `test_sendable.cpp`.

## Propositions rejetées

- **`lifetime_aware.h` : `T[N]` → `T[]`** — même idée que la proposition 3, mais le
  vérificateur a jugé que la délégation unsafe→unsafe y change la surface de
  personnalisation (une spécialisation utilisateur de `T[]` répondrait pour tous les
  `T[N]`) pour ~1 ligne de gain. Tension assumée avec la proposition 3, dont le précédent
  (`T&&` → `T&`) existe déjà dans `sendable.h` ; à trancher ensemble.
- **`launch_scoped_task` : jthread temporaire** — techniquement correct mais contredit la
  règle du projet « always use explicit name for variables ».
- **`test_synchronizable.cpp` : compaction des corps** — doublon partiel de la
  proposition 9, rejeté pour périmètre trop étroit (il faut uniformiser les 13 sites).
- **`vocabulary.h` : fusion stop_token/stop_source (doublon)** — deux finders ont trouvé
  la même chose ; ce verdict de rejet prétendait la fusion déjà commitée, ce qui est
  faux (vérifié sur le fichier réel : les six spécialisations existent toujours). La
  version confirmée (proposition 1) fait foi.
