# 00 — Synthèse de l'audit ThreadSafe

Synthèse des quatre rapports détaillés :

- [10 — Robustesse des traits](10-robustesse-traits.md)
- [11 — Robustesse des helpers](11-robustesse-helpers.md)
- [12 — Simplicité du code et de l'API](12-simplicite-api.md)
- [13 — Thread safety réelle et performance](13-thread-safety-et-performance.md)

Environnement commun : g++-16 (Homebrew GCC 16.2.0), `-std=c++2c -freflection`, macOS arm64. Tous les scénarios cités ont été compilés et, le cas échéant, exécutés ; les correctifs proposés ont été validés contre la suite de tests complète du dépôt.

## Verdict global

Le cœur de la bibliothèque — les quatre traits et leur marche structurelle par réflexion — est **remarquablement robuste** : aucun trou de soundness n'a été trouvé sur les indirections, `mutable`, unions (y compris anonymes), bitfields, héritage, détournement de copie/move, pimpl ou conversions, à l'exception d'un seul cas (l'état statique de classe). En revanche, un bug bloquant rend `synchronized_value<T>` **inutilisable** pour tout `T` non const-synchronizable, invisible pour une suite de tests 100 % `static_assert` qui n'instancie jamais un corps de fonction runtime — c'est la leçon transverse de l'audit. Les trois helpers sont sains en usage nominal (zéro data race, résultats exacts sous stress 8 threads), et des correctifs complets et validés existent pour chaque problème identifié.

## Tableau des findings par sévérité

### Bloquant

| Finding | Détail | Rapport(s) |
|---|---|---|
| `synchronized_value` : mutex codé en dur | `mutex_` est `std::shared_mutex` en dur et le ctor de `value_guard` prend `std::shared_mutex&`, alors que `get_mutex_type()` choisit `std::mutex` quand `is_synchronizable<const T>` est faux → `lock()`/`lock_shared()` **ne compilent pas** pour tout `T` non const-synchronizable (la branche `std::mutex` est morte). Reproduit indépendamment par trois auditeurs. Correctif validé sans régression. | [11](11-robustesse-helpers.md) §2, [12](12-simplicite-api.md) §1, [13](13-thread-safety-et-performance.md) §1 |

### Important

| Finding | Détail | Rapport(s) |
|---|---|---|
| État statique mutable invisible des traits (trou de soundness) | `static inline std::vector<int>` dans une classe la laisse sendable et const-synchronizable → course de données via une méthode `const`, sans `const_cast` ni `mutable`. Correctif (`has_only_synchronizable_statics` + garde dans les deux marches structurelles) validé contre les 10 fichiers de tests. | [10](10-robustesse-traits.md) §5 |
| Évasion de référence hors d'un guard temporaire | `int& r = *sv.lock();` compile ; race **réelle démontrée** (TSan WARNING sur shim + pertes d'incréments 100000/400000 sur la vraie bibliothèque, reproductible) ; `-Wdangling-reference` ne détecte rien. Mitigation : API `apply()`/`with_lock` à portée fermée + documentation ; non fermable totalement sans borrow checker. | [13](13-thread-safety-et-performance.md) §2, [11](11-robustesse-helpers.md) §2.6 |
| `copy_on_write::as_mutable` : `use_count()` relaxed | Mutation en place après `use_count()==1` sans arête acquire face au décrément release du dernier autre propriétaire : data race selon le modèle mémoire (raison du retrait de `shared_ptr::unique()` en C++20), flaggée par TSan sur le shim. Fix : `atomic_thread_fence(acquire)` (pattern `Arc::make_mut`), validé. | [13](13-thread-safety-et-performance.md) §3 |
| Types récursifs possédants → erreur de compilation dure | `struct Node { unique_ptr<Node> next; }`, `struct Tree { vector<Tree> children; }` : auto-dépendance de la variable template, erreur illisible sur les trois traits. Vrai fix impossible sans coinduction ; contournement (spécialisation anticipée) vérifié, à documenter. | [10](10-robustesse-traits.md) §6 |
| Suite de tests : aucun corps runtime jamais instancié | Cause racine du bug bloquant : les `static_assert` et `requires`-expressions ne voient que les signatures, jamais les corps. Recommandation : un TU instanciant réellement `lock()`, `as_mutable()`, `launch_task` (dont un `T` non const-synchronizable) + CI Linux `-fsanitize=thread` (pas de libtsan dans Homebrew GCC arm64, aucun clang ne compile la réflexion). | [13](13-thread-safety-et-performance.md) §8 & méthodologie |
| `is_lifetime_aware<T[]>` incohérent | `is_lifetime_aware<int*[]> == true` vs `int*[3] == false` : spécialisation `T[]` manquante. Cas marginal ; correctif d'une ligne validé. | [10](10-robustesse-traits.md) §7 |

### Suggestion

| Finding | Une ligne | Rapport |
|---|---|---|
| Primitives std non couvertes | `is_synchronizable<std::mutex/shared_mutex/atomic_flag/condition_variable> == false` → le motif « `mutable std::mutex` » ne peut jamais être const-synchronizable ; specs proposées. | [10](10-robustesse-traits.md) §8 |
| Lambdas capturantes toutes rejetées | GCC 16 ne réfléchit pas les captures → rejet conservateur (sûr) même par valeur ; **friction n° 1 de l'API** (premier code utilisateur rejeté avec « constraints not satisfied ») ; documenter la règle + le style captureless-lambda + args. | [10](10-robustesse-traits.md) §9, [12](12-simplicite-api.md) §3 |
| `is_sendable<std::mutex> == true` par accident structurel | Dépend des internes pthread de la plateforme ; inoffensif (type non déplaçable) mais à fixer par spec explicite. | [10](10-robustesse-traits.md) §10 |
| Sync ⇒ Send câblé en dur | Contrairement à Rust ; `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` rend un type sendable en silence — à documenter. | [10](10-robustesse-traits.md) §11 |
| `copy_on_write` : handle moved-from | `operator*`/`as_mutable` sur un handle move-from déréférencent un `shared_ptr` nul (UB) ; documenter ou supprimer le move. | [11](11-robustesse-helpers.md) §1.3 |
| Deadlocks non détectables | Self-lock et `lock_shared` puis `lock` dans le même thread compilent et bloquent ; pas d'équivalent `scoped_lock` multi-objets (`lock_all` proposé) ; à documenter. | [11](11-robustesse-helpers.md) §2.6, [13](13-thread-safety-et-performance.md) §4 |
| TOCTOU : la forme fautive est la plus courte à écrire | Invariant limite=1000 violé (1002–1003 vendus) avec deux locks séparés ; `apply()` rendrait la forme correcte la plus courte. | [13](13-thread-safety-et-performance.md) §4 |
| Invocabilité non contrainte du launcher | `launch_task([]{}, 42)` passe le `requires` puis explose au fond de `jthread` ; concept `jthread_invocable` proposé, validé. | [11](11-robustesse-helpers.md) §3.4 |
| Réflexion accidentelle dans `synchronized_value` | `get_mutex_type`/`get_const_guard_type` (17 lignes de `^^`/splice) = complexité où le bug bloquant s'est caché ; un `std::conditional_t` d'une ligne suffit ; alias `mutex` public = code mort. | [12](12-simplicite-api.md) §2 |
| Diagnostic « pourquoi non sendable » | Helper consteval `assert_sendable(^^T)` prototypé et validé : nomme le sous-objet fautif (`this.inner.borrowed` de type `int*`) au lieu de « constraints not satisfied ». | [12](12-simplicite-api.md) §4 |
| `containers.h` : ~40 spécialisations dupliquées | Remplacement par une macro variadique par conteneur (205 → 64 lignes), les 10 fichiers de tests compilent inchangés ; ou assumer la duplication par un commentaire pédagogique. | [12](12-simplicite-api.md) §5 |
| Opt-in asymétrique | Une seule macro, pour un seul trait ; uniformiser (3 macros ou aucune — préférence de l'auditeur : aucune). | [12](12-simplicite-api.md) §6 |
| Headers `details/` inclus isolément | Les traits changent de valeur selon le sous-ensemble inclus (`is_sendable<optional<int>>` false vs true) → risque ODR/IFNDR ; garde-fou `#error` proposé. | [12](12-simplicite-api.md) §7 |
| Performance runtime | `synchronized_value` : +87 % vs mutex+T brut (9,2 vs 4,9 ns/op), dont +28 % dus au `shared_mutex` systématique (récupéré par le fix bloquant) ; `copy_on_write` : zéro surcoût vs `shared_ptr` ; `launch_scoped_task` ~16 µs/appel (un thread OS par tâche, join immédiat, zéro parallélisme) ; `vector<jthread>` non borné, pas de `join_all()`. | [13](13-thread-safety-et-performance.md) §7 |

## Divergences entre rapports (à arbitrer)

1. **Sévérité de l'évasion de référence** (`int& r = *sv.lock();`) : le rapport 11 la classe en SUGGESTION (« limitation connue, documentée dans le code »), le rapport 13 en PROBLÈME MAJEUR — parce qu'il a **démontré la race à l'exécution** sur la vraie bibliothèque (75 % d'incréments perdus). La synthèse retient « important » : indétectable statiquement sans borrow checker, mais atteignable par l'API publique seule, ce qui contredit la promesse « safety checked entirely at compile time ».
2. **Deux correctifs différents pour le bug bloquant** : les rapports 11 et 13 proposent `mutable mutex mutex_;` + `value_guard(typename Lock::mutex_type&)` (le mutex suit le type calculé — `std::mutex` quand possible) ; le rapport 12 propose l'inverse : garder `shared_mutex` partout et ne faire varier que le type de verrou via `std::conditional_t` (ce qui supprime aussi la réflexion accidentelle, mais nécessite d'ajuster un `static_assert` de `test_synchronized_value.cpp`). Les deux sont validés contre la suite du dépôt. Différence concrète : seul le fix 11/13 récupère les **+28 %** mesurés du `shared_mutex` systématique (rapport 13 §7) ; seul le fix 12 élimine `get_mutex_type`/`get_const_guard_type`. Les deux objectifs sont combinables (conditional_t **et** mutex conditionnel) — à trancher selon la priorité perf vs pédagogie.

## Points forts

- **Aucun trou de soundness dans les traits** hors le cas des membres statiques : « const derrière une indirection » appliqué partout (`const T*`, `const T&`, `reference_wrapper`, `string_view`, `span<const T>`, bases, `mutable`, unions anonymes, bitfields, cv combinés) ; transitivité de `is_lifetime_aware` correcte ; règle `is_sendable<T&> == is_sendable<T*> == is_synchronizable<T>` vérifiée ; garde anti-détournement copie/move efficace.
- **Usage nominal des trois helpers irréprochable** : stress 8 threads, zéro data race (TSan shim + empirique x3), compteurs exacts, original du COW jamais modifié.
- **Rejets de sûreté du launcher tous effectifs** : captures par référence, `T*` non-sync, `shared_ptr<T>` non-sync, lambda mutable, args non-sendables, `reference_wrapper`, `std::function` — tous refusés à la compilation, contrôles positifs à l'appui.
- **Performance de compilation excellente** : ~0,62 s/TU, coût dominé par les includes std ; réflexion et récursion des traits négligeables. `copy_on_write` : zéro surcoût runtime vs `shared_ptr`.
- **Qualité rédactionnelle** : commentaires excellents (`may_hijack_copy_move`, `[res.on.data.races]`, « nodiscard is load-bearing »), noms explicites, réflexion localisée et motivée, tests-comme-documentation ; bons messages consteval pour les types incomplets (pimpl).
- **Extension simple** : opt-in pour types propres et tiers vérifié, composition des traits cohérente ; `copy_on_write` est l'API la plus fluide.

## Recommandations priorisées

1. **Corriger `synchronized_value`** (bloquant) — choisir/combiner les deux correctifs validés (voir divergence n° 2), et ajuster le `static_assert` du test si la variante du rapport 12 est retenue. [11 §2.4, 12 §1, 13 §1]
2. **Ajouter un TU instanciant les corps runtime** (`lock()`, `lock_shared()`, `as_mutable()`, `launch_task`, dont un `T` non const-synchronizable) + **CI Linux avec `-fsanitize=thread`** — c'est ce qui aurait attrapé le bloquant. [13 §8]
3. **Fermer le trou de soundness des membres statiques** — correctif `has_only_synchronizable_statics` fourni et validé. [10 §5]
4. **Ajouter la fence acquire dans `copy_on_write::as_mutable`** — correctif fourni et validé (caveat : TSan ne modélise pas les fences). [13 §3]
5. **Ajouter `apply()`/`with_lock` à `synchronized_value`** — réduit à la fois l'évasion de référence et le TOCTOU en rendant la forme correcte la plus courte. [13 §2 & §4, 11 §2.6]
6. **Appliquer le correctif `is_lifetime_aware<T[]>`** (une ligne) et **contraindre l'invocabilité du launcher** (`jthread_invocable`, fourni). [10 §7, 11 §3.4]
7. **Documenter les limites assumées** — lambdas capturantes rejetées (avec l'idiome captureless + args en tête de header), types récursifs (contournement par spécialisation anticipée), deadlocks/self-lock, handle moved-from du COW, Sync ⇒ Send, `launch_scoped_task` « éducatif, pas un pool ». [10 §6/9/11, 11 §1.3/2.6, 12 §3, 13 §4/7]
8. **Améliorations d'API et d'hygiène** (au fil de l'eau) — diagnostic `assert_sendable`, specs pour les primitives std (`mutex`, `atomic_flag`…) et `is_sendable<std::mutex>` explicite, garde-fou `#error` des headers `details/`, factorisation ou assomption de `containers.h`, uniformisation de l'opt-in. [12 §4–7, 10 §8/10]
