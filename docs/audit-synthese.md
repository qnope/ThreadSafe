# Audit ThreadSafe — synthèse

Audit complet de la bibliothèque selon `Task.md`, mené sur GCC 16.2.0 (`-std=c++26 -freflection`).
Douze dimensions auditées en parallèle, chaque constat ensuite soumis à un vérificateur
chargé de le **réfuter** par recompilation indépendante : 141 sondes compilées, 72 constats
retenus, tous confirmés. Le détail, avec le code fautif et la correction, est dans
[audit-details.md](audit-details.md).

État de départ : `cmake --build build` passe — les douze fichiers de tests compile-time compilent.

## Verdict

**Le modèle de traits est sain.** Sur un front adversarial large — unions nommées et anonymes,
bitfields, `[[no_unique_address]]`, héritage virtuel et en diamant, types polymorphes,
`mutable`, indirections multiples, shallow-const à tous les niveaux, closures, vues empruntantes,
allocateurs et deleters malveillants, `atomic<shared_ptr<T>>` — **aucun type non-thread-safe
n'est accepté par le walk**. La règle « const derrière une indirection n'est jamais cru » est
appliquée avec une cohérence exemplaire, la mémoïsation par `_v` fonctionne (un DAG de
profondeur 60, soit 2^60 chemins naïfs, compile au temps de base), et le point de customisation
`is_unsafe_*` tient ses promesses pour un tiers, claim conditionnel compris.

**Une seule faille de sûreté a été trouvée, et elle est dans la couche de vouching, pas dans le
walk** : `const std::unique_ptr<T, D>` est vouché pour *n'importe quel* deleter, or un deleter
no-op est précisément l'idiome du pointeur observateur non propriétaire. Un `unique_ptr<const int,
NoOpDeleter>` aliasant une variable écrite ailleurs est béni de bout en bout, jusqu'à
`synchronized_value::shared_readable == true` et des lectures concurrentes sous `shared_lock`.
Correction validée : une ligne.

Le reste des constats sérieux ne concerne pas les traits mais les trois helpers et la qualité
des diagnostics : un `noexcept` mensonger qui transforme un `bad_alloc` en `std::terminate`,
deux références brutes (`as_mutable()`, `value_guard`) dont l'exclusivité n'est garantie qu'à
l'instant de l'appel, et `launch_scoped_task` qui est en réalité synchrone.

## Répartition

| Sévérité | Nombre | Nature |
|---|---|---|
| Critique | 1 | Faille de sûreté : un type unsafe accepté |
| Majeur | 8 | Bug réel, API piégeuse ou faux négatif bloquant |
| Mineur | 26 | Ergonomie, faux négatifs, hygiène, perf |
| Info | 37 | Limites assumées, constats positifs, suggestions |

## Les neuf constats à traiter

Par ordre de priorité. Les corrections sont détaillées et validées par compilation dans le
rapport de détails ; celles qui demandent un ajustement par rapport à la proposition initiale
sont signalées.

1. **[Critique] `const unique_ptr<T, D>` vouché pour tout deleter** — `smart_pointers.h:81`.
   Un deleter no-op rend la lecture partagée racée, sans UB ni `release()`. Correction d'une
   ligne : n'accorder la confiance au `const` du pointé que pour `default_delete` (propriété
   réelle), sinon exiger un pointé pleinement synchronizable.

2. **[Majeur] `copy_on_write::operator*() &&` est `noexcept` alors qu'il copie `T`** —
   `copy_on_write.h:27`. Démontré à l'exécution : `terminate called`, exit 134, au lieu de
   propager. Le même appel copie aussi inconditionnellement, là où un handle unique pourrait
   déplacer. Une seule correction traite les deux.

3. **[Majeur] La `T&` de `as_mutable()` n'est exclusive qu'à l'instant de l'appel** —
   `copy_on_write.h:30`. Copier le handle après avoir pris la référence re-partage le bloc ;
   les traits acceptent alors d'envoyer la copie à un autre thread. Chaque étape est légitime
   prise isolément. Non fermable sans borrow checker : à documenter dans l'en-tête, et à
   présenter — c'est exactement ce que `Arc::make_mut` ferme côté Rust.

4. **[Majeur] Une référence fuit hors de la section critique via un guard nommé** —
   `synchronized_value.h:23`. Les surcharges `&&` supprimées ne couvrent que le guard
   temporaire ; l'élision de copie garantie permet même de loger le guard dans un paramètre
   par valeur ou un agrégat. Le canal inter-thread reste fermé (rien de tout cela n'est
   sendable), donc pas de faille — mais le dispositif suggère une protection qu'il n'offre pas.

5. **[Majeur] `launch_scoped_task` est entièrement synchrone** — `asynchronous_task_launcher.h:70`.
   Mesuré : deux tâches de 300 ms prennent 604 ms. C'est ce `join()` immédiat qui rend sûr
   l'emprunt par `std::ref`, mais l'invariant n'est écrit nulle part et le nom dit le contraire.
   ⚠ Le `task_scope` proposé par l'auditeur **introduit une faille** (ordre de destruction) :
   s'en tenir au commentaire et au renommage.

6. **[Majeur] Aucune synchronisabilité pleine prouvée structurellement** —
   `synchronizable_base.h:53`. `struct Counters { std::atomic<int> a, b; }` n'est pas
   synchronizable alors que `std::atomic<int>[4]` l'est : `Counters&` n'est donc pas sendable.
   ⚠ Le correctif proposé rend les classes vides synchronizables et casse 4 assertions
   existantes ; le garde correctif a été identifié et validé.

7. **[Majeur] Un pointeur vers type incomplet donne une erreur brute de réflexion** —
   `sendable.h:47`. `is_dynamic_type_known` est appelé avant la question, court-circuitant le
   `static_assert` pédagogique documenté. Correction : inverser les deux opérandes du `&&`,
   comme le fait déjà `pointee_answer`.

8. **[Majeur] Un type possédant récursif (liste, arbre, AST) ne peut pas être interrogé** —
   `lifetime_aware.h:68`. Cycle d'instanciation → « is not usable in a constant expression ».
   Rust résout ce cas nativement (auto-traits coinductifs). Le contournement par vouch
   fonctionne ; à documenter, faute d'un fix sans détection de cycle.

9. **[Majeur] Un constructeur forwarding rend le type silencieusement non-sendable** —
   `utils.h:66`. C'est le faux négatif le plus probable en usage réel, et le diagnostic ne
   nomme jamais la cause. ⚠ L'affinement proposé de `may_hijack_copy_move` est
   **non implémentable** (`can_substitute` produit une erreur dure dès que le constructeur a
   un corps) : la voie réaliste est le diagnostic.

## Par dimension de `Task.md`

**Robustesse des traits** — Excellente. `is_synchronizable`, sa forme const, `is_sendable` et
`is_lifetime_aware` résistent à tout, la seule faille étant dans le vouching `unique_ptr`.
Les faux négatifs identifiés sont réalistes mais assumés par la philosophie conservatrice :
struct d'atomics, `std::expected` absent de la liste des wrappers, closures à état (limite de
la réflexion GCC, pas de la bibliothèque), types récursifs.

**Helpers** — `synchronized_value` est le plus solide : le verrou couvre tout accès offert par
l'API, le guard est infalsifiable, et le vouch Sync-si-Send reproduit fidèlement
`impl<T: Send> Sync for Mutex<T>`. `copy_on_write` concentre la moitié des constats majeurs.
Le launcher tient son exigence — sur ~25 cas adversariaux, **aucun type unsafe n'est accepté**,
y compris le launcher passé à sa propre tâche — mais il manque une contrainte d'invocabilité,
une API d'attente, et sa variante « scoped » n'est pas concurrente.

**Simplicité** — Le code est de très bonne qualité pédagogique : fichiers de 11 à 104 lignes,
une fonction consteval par idée, messages de `static_assert` qui expliquent le *pourquoi*.
Deux en-têtes ne compilent que grâce à l'ordre d'inclusion de `threadsafe.h`
(`smart_pointers.h`, `asynchronous_task_launcher.h`) — le genre de fragilité qu'on ne veut pas
projeter sur scène. Le fichier le plus important de la bibliothèque s'appelle
`synchronizable_base.h` tandis que `synchronizable.h` ne contient que le voucher `std::atomic`.

**Thread safety** — Aucune data race dans l'implémentation tant qu'on reste dans l'API.
Les trous résiduels sont tous des échappements de référence brute (constats 3 et 4), plus les
races logiques que l'API rend faciles : pas de combinateur « une opération sous un seul
verrou », donc `if (sv.lock_shared()->empty()) sv.lock()->push_back(x);` compile avec son
check-then-act. Les deadlocks (réentrance, verrouillage croisé) ne sont ni détectés ni
documentés — parité exacte avec Rust, à dire sur scène.

**Performance de compilation** — Saine algorithmiquement : mémoïsation intacte, scaling
linéaire sur tous les axes, ~0,7 ms par type et par trait. Le coût dominant n'est pas le walk
mais les inclusions : 0,58 s par TU, soit 97 % du temps d'un test typique, dont 0,17 s de
`<mutex>`/`<thread>` imposés à qui ne veut que les traits. Un en-tête `traits.h` sans les
helpers fait gagner 30 % sur un TU utilisateur (mesuré).

**Performance runtime** — Les traits sont gratuits. `synchronized_value` est propre (choix
adaptatif du mutex, guard réduit à un lock et un pointeur). Les deux gaspillages réels sont
dans `copy_on_write` : `as_mutable() &&` sur bloc partagé alloue un bloc entier pour l'abandonner
aussitôt, et `operator*() &&` copie là où il pourrait déplacer.

**API et flexibilité** — Les trois programmes réalistes écrits pour l'audit compilent et se
lient du premier coup en n'incluant que `threadsafe.h`. Les frictions sont ergonomiques :
l'idiome sûr en une expression (`sv.lock()->push_back(x)`) est interdit, le message du `delete`
qui l'interdit est factuellement inexact pour cet usage, la copie supprimée de
`synchronized_value` ne guide pas vers `make()`, et aucun diagnostic ne dit *quel* trait a
échoué ni *quelle* capture est fautive.

## Corrections d'une ligne, sans risque

À appliquer sans hésiter — toutes validées par compilation, aucune ne casse les tests existants :

- `concept synchronizable` (absent alors que `sendable` et `lifetime_aware` existent) ;
- `#include <threadsafe/details/allowed_std_wrappers.h>` et `<algorithm>` dans `smart_pointers.h` ;
- `#include <threadsafe/details/vocabulary.h>` dans `asynchronous_task_launcher.h` ;
- `^^std::expected` dans `allowed_std_wrappers` (avec son include) ;
- `std::as_const(*ptr_)` dans le détach de `copy_on_write` ;
- `lock_shared() const && = delete` (le `&&` nu laisse passer les rvalues const) ;
- suppression de `<functional>` et `<memory>`, inutilisés dans `lifetime_aware.h` ;
- suppression du `bool()` redondant sur `is_synchronizable_v<const T>`.
