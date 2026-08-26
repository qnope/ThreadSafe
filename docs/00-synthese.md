# Audit ThreadSafe — synthèse

Audit de la bibliothèque au commit `64f9c06`. Chaque affirmation chiffrée vient d'une
compilation ou d'une exécution réelle : **357 scénarios** conservés et rejouables dans
[`scenarios/`](./scenarios/), **260 mutations** appliquées aux en-têtes, et une
relecture personnelle de chaque défaut critique. La méthode, et surtout ses limites,
sont dans [09](./09-methodologie.md).

## Le verdict en une phrase

**Le cœur est solide et gratuit** — dix formes de contrebande structurelle refusées,
zéro surcoût à l'exécution mesuré au dixième de milliseconde — **et les défauts qui
restent se rangent en trois familles très différentes** : quelques trous de forme,
réparables en une ou deux lignes ; une classe de problèmes d'ODR que la conception
réflexive rend structurelle ; et **une limite qui n'est pas un bug**, qu'il faut
énoncer sur scène plutôt que corriger.

## Ce qu'il faut corriger

| | défaut | sévérité | où |
|---|---|---|---|
| 1 | un `const` en tête **blanchit** une désinscription explicite : `is_sendable_v<const std::vector<X>>` est vrai quand `is_sendable_v<std::vector<X>>` est faux | **critique** | [01](./01-robustesse-des-traits.md) |
| 2 | une spécialisation écrite dans **une seule TU** donne deux `sizeof` (72 et 208) à `synchronized_value<Cache>` : lien silencieux, puis `abort` | **critique** | [08](./08-api-et-flexibilite.md) |
| 3 | le deleter type-effacé de `shared_ptr` n'est jamais examiné : use-after-free prouvé à l'ASan | **critique** | [01](./01-robustesse-des-traits.md) |
| 4 | la garde de type dynamique n'est posée que sur `unique_ptr` ; `shared_ptr`, `weak_ptr`, `T*`, `T&` et `reference_wrapper` y échappent | **élevée** | [01](./01-robustesse-des-traits.md) |
| 5 | `is_sendable<shared_ptr<T>>` ignore une désinscription explicite de `T`, là où `unique_ptr` la respecte | **élevée** | [01](./01-robustesse-des-traits.md) |
| 6 | `launch_scoped_task` **interbloque** sur une tâche coopérative : la `stop_source` est inatteignable | **élevée** | [02](./02-robustesse-des-helpers.md) |
| 7 | la réentrance du lanceur segfault (`threads_` est un `vector` nu) — et les traits ne peuvent pas l'empêcher | **élevée** | [02](./02-robustesse-des-helpers.md) |
| 8 | **77 mutations réelles survivent** aux onze TU : score 68 % | **élevée** | [03](./03-couverture-de-tests.md) |
| 9 | **13 messages sur 28** nomment le bon coupable | **élevée** | [04](./04-diagnostics.md) |
| 10 | `asynchronous_task_launcher.h` **n'est pas autonome** : inverser l'ordre du parapluie casse les onze TU | moyenne | [05](./05-simplicite.md) |
| 11 | le `shared_mutex` automatique perd de **5,8× à 118,8×** sur une section critique courte | moyenne | [07](./07-performance-execution.md) |

Le n°1 et le n°10 ont un correctif que **j'ai écrit, appliqué et vérifié moi-même** :
les onze TU restent vertes. Deux lignes chacun.

```cpp
// allowed_std_wrappers.h — ferme le blanchiment par cv (défaut n°1).
// Une orthographe cv-qualifiée ne doit PAS correspondre : elle court-circuite le
// renvoi cv du template primaire et recalcule la réponse — et le raccourci
// is_synchronizable_type lit alors un `const` comme la question de LECTURE SEULE.
template <class T>
concept std_wrapper =
    std::same_as<T, std::remove_cv_t<T>> && is_allowed_std_wrapper(^^T);
```

## Ce qu'il faut dire, et qu'aucun code ne peut corriger

**La réflexion lit des déclarations, jamais des corps de fonction.**

```cpp
class LookupTable {
public:
    int find(int key) const {
        ++probe_count_;              // écrit, à travers une méthode CONST
        return key * 2;
    }
private:
    static inline long probe_count_ = 0;
};
```

Pas de membre `mutable`, pas de référence, pas de pointeur, aucune fonction spéciale
écrite à la main. `is_sendable`, `is_synchronizable<const T>` et `launchable_task`
répondent **oui** tous les trois, `synchronized_value<LookupTable>` choisit donc un
`std::shared_mutex`, et ThreadSanitizer confirme une course réelle — **les deux
threads tenant un `shared_lock`**. Je l'ai vérifié personnellement.

C'est le point exact où l'analogie avec Rust cesse d'être vraie :

> `Sync` est sûr en Rust parce que `&T` **interdit** la mutation sans `UnsafeCell`.
> En C++, `const` n'interdit rien.

La même limite revient sous quatre déguisements — le membre `static`, le `const_cast`
dans une méthode `const`, le handle lié à un thread, la lambda dont les captures sont
invisibles — et elle explique à elle seule la majorité des constats « critiques » de
[01](./01-robustesse-des-traits.md) et [02](./02-robustesse-des-helpers.md). Aucune
version de ces traits ne peut la fermer. **Elle n'est écrite nulle part aujourd'hui**,
ni dans `CLAUDE.md` ni dans un en-tête, et c'est le meilleur passage possible d'une
conférence sur le sujet.

## Le message qui conduit à ouvrir le trou

Le plus beau défaut de l'audit tient en trois lignes, et il a été vérifié par
compilation. La bibliothèque refuse `std::vector<Foo*>` en conseillant
« *specialize `is_sendable` to state the intent* ». On suit le conseil :

```cpp
template <> struct threadsafe::is_sendable<std::vector<Foo*>> : std::true_type {};
```

Zéro erreur. `is_sendable_v<std::vector<Foo*>>` devient **vrai** alors que
`is_sendable_v<Foo*>` reste **faux**. Le conseil de la bibliothèque, suivi à la
lettre, ouvre exactement le trou de sûreté qu'elle existe pour fermer. Détail en
[04](./04-diagnostics.md).

## Une affirmation fausse est écrite dans le dépôt

`tests/test_diagnostics.cpp` affirme que la moitié « qui rejette » des traits
« *is a compile error by design* » et donc n'est pas testable. **C'est faux**, et le
contre-exemple est déjà dans la bibliothèque : `default_is_sendable` attrape la
`std::meta::exception` dans une fonction `consteval`. `u8what()` étant `consteval`,
**chaque message de rejet est une valeur de compilation** et se teste au
`static_assert`. Un helper de huit lignes débloque 25 des 79 survivants de mutation.

C'est pourquoi dix messages faux ont pu survivre dans une bibliothèque dont les onze
TU sont vertes : **aucun test ne fige aucun message.**

## Deux idées reçues corrigées par la mesure

**Le coût de compilation n'est pas la réflexion, c'est le `#include`.** Une TU *vide*
qui n'inclut que l'en-tête parapluie coûte **586 ms**, sur les ~620 ms d'une TU de
test — 94 % du temps de build est payé avant la première question. `-ftime-report`
range toute l'évaluation des traits dans `constant expression evaluation` : **2 % sur
la TU vide, 9 % sur la plus lourde**, soit ~50 ms. Au passage, l'audit précédent
attribuait ~135 ms / 36 % à `<ranges>` et à `borrowed_range` : **remesuré, c'est
28 ms.** D'autres en-têtes tirent `<ranges>` transitivement. Détail en
[06](./06-performance-compilation.md).

**Le zéro-surcoût n'est pas un principe, c'est une mesure.** `lock()` coûte 6,32 ns
contre 6,33 ns pour le `std::unique_lock` écrit à la main sur le même mutex ;
`lock_shared()` 6,37 contre 6,41 ; `launch_task` 205,1 ms contre 205,1 ms pour un
`vector<jthread>` nu sur quatre tâches de 200 ms. Une bibliothèque de sûreté
entièrement compile-time ne coûte **rien** à l'exécution. C'est une phrase que la
conférence peut prononcer telle quelle.

Ce que la bibliothèque fait payer, ce n'est pas l'emballage, c'est la **politique** :
le `shared_mutex` choisi automatiquement perd contre `std::mutex` à tous les nombres
de threads et tous les taux de lecture mesurés, et ne se rembourse qu'au-delà
d'environ **500 ns** passées sous le verrou. La sélection est **sûre** — jamais un
`shared_mutex` pour un `T` dont la lecture `const` serait dangereuse — mais elle est
silencieuse et l'utilisateur ne peut pas la contredire.

## Le frein réel à l'adoption

Les deux gardes conservateurs — `may_hijack_copy_move`, qui refuse tout constructeur
*template*, et le garde copie/déplacement/destructeur — refusent ensemble **huit
types-vocabulaire standard**, **dix types de bibliothèque réalistes sur dix**, et,
mesuré, **dix-sept des dix-huit templates de la liste blanche eux-mêmes**. Si
`std::vector<T>` passe, c'est uniquement parce que `allowed_std_wrappers.h` le nomme
à la main : le défaut structurel refuse l'essentiel de la bibliothèque standard, et
une liste explicite la rattrape. Le raisonnement des gardes est **juste** et n'a pas
été contourné ; c'est son **ampleur** qui n'est énoncée nulle part. Voir
[08](./08-api-et-flexibilite.md).

## Ce qui a résisté

À dire aussi nettement que les défauts :

- **La marche structurelle n'a pas cédé.** Dix formes de contrebande refusées pour
  `is_sendable` *et* `is_synchronizable<const T>` : union anonyme, `[[no_unique_address]]`,
  bit-field, base virtuelle en diamant, base privée, structure imbriquée sans nom,
  tableau de structures empruntantes, membre référence. `access_context::unchecked()`
  atteint bien les sous-objets privés.
- **`has_unreflectable_state` est la défense portante, et elle tient.** GCC rapporte
  zéro membre pour une lambda de 8 octets qui a capturé par référence ; sans ce test,
  `[&local]{ ... }` serait béni et le modèle trivialement cassable.
- **`mutable` est attrapé**, et le `const` derrière une indirection n'est jamais cru.
- **La barrière `acquire` de `as_mutable()` est portante et correctement appariée** —
  avec le `__ATOMIC_ACQ_REL` du `_M_release` de libstdc++, la lecture de `use_count()`
  étant `relaxed`. Les rapports TSan qui semblent la contredire sont des artefacts :
  TSan ne modélise pas `atomic_thread_fence` sur cette plateforme.
- **La règle référence/pointeur** — `is_sendable<T&>` = `is_sendable<T*>` =
  `is_synchronizable<remove_cv_t<T>>` — n'a pas été prise en défaut.
- **Le chemin de diagnostic est exact et sans limite de profondeur** : il nomme les
  soixante niveaux d'une chaîne de soixante structures. Quand il tombe sur la bonne
  cause, la phrase est excellente.
- **La récursion est mémoïsée par le compilateur** : mille questions identiques
  coûtent le prix d'une. Aucun cache maison à écrire.
- **Aucune assertion de la suite ne fige un comportement buggé** : les deux correctifs
  que j'ai appliqués passent les onze TU sans en toucher une.

## Les rapports

| | |
|---|---|
| [01 — Robustesse des traits](./01-robustesse-des-traits.md) | `is_synchronizable<T>`, `<const T>`, `is_sendable`, `is_lifetime_aware`, un par un |
| [02 — Robustesse des helpers](./02-robustesse-des-helpers.md) | `copy_on_write`, `synchronized_value`, `asynchronous_task_launcher` |
| [03 — Couverture de tests](./03-couverture-de-tests.md) | 260 mutations, 77 survivants, 68 % → 99 % |
| [04 — Diagnostics](./04-diagnostics.md) | 13 bons coupables sur 28, et le conseil qui ouvre le trou |
| [05 — Simplicité](./05-simplicite.md) | la duplication qu'il faut garder, l'en-tête non autonome |
| [06 — Performance de compilation](./06-performance-compilation.md) | où va réellement le temps, et un chiffre d'audit corrigé |
| [07 — Performance d'exécution](./07-performance-execution.md) | les helpers sont gratuits, la politique ne l'est pas |
| [08 — API et flexibilité](./08-api-et-flexibilite.md) | le rayon d'action des gardes, le piège ODR |
| [09 — Méthodologie](./09-methodologie.md) | le harnais, et ce qu'il ne peut pas atteindre |
| [`scenarios/`](./scenarios/) | les 357 scénarios, rejouables par `./replay.sh` |

## Si vous ne faites que cinq choses

1. **Écrire la limite de la réflexion** dans `CLAUDE.md` et en tête de
   `synchronizable.h`. C'est gratuit, et cela transforme la faiblesse la plus profonde
   de l'approche en le meilleur moment de la présentation.
2. **Appliquer les deux correctifs d'une ligne** — le `std_wrapper` contraint sur
   `remove_cv` et le `#include <threadsafe/details/vocabulary.h>` manquant. Tous deux
   vérifiés, onze TU vertes.
3. **Ajouter la TU d'exécution** de [03](./03-couverture-de-tests.md). Les trois
   helpers ont aujourd'hui zéro test de comportement, et `if (false)` à la place de
   `if (ptr_.use_count() != 1)` passe la suite.
4. **Reformuler le conseil « specialize `is_sendable` »**, qui est le message le plus
   vu de la bibliothèque et qui, suivi à la lettre, ouvre un trou de sûreté.
5. **Donner un second paramètre de template à `synchronized_value`**, défaut =
   sélection automatique actuelle. Un seul patch règle à la fois le grief de
   performance de [07](./07-performance-execution.md) et celui d'extensibilité de
   [08](./08-api-et-flexibilite.md).
