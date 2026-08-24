# Audit ThreadSafe — synthèse

Audit de la bibliothèque telle qu'elle est au commit `ab84799`. Chaque affirmation
chiffrée vient d'une compilation ou d'une exécution réelle ; **243 scénarios** ont
été écrits et exécutés, et sont conservés dans [`scenarios/`](./scenarios/) avec le
harnais qui permet de les rejouer.

Méthode en trois temps : sonder chaque trait et chaque helper séparément, puis
soumettre **chaque défaut trouvé à un vérificateur indépendant chargé de le
réfuter**, puis ne retenir que ce qui a survécu. Cette dernière étape a beaucoup
compté : sur six failles d'abord annoncées « critiques », **une seule** a passé la
vérification.

## Le verdict en une phrase

**Le cœur de la bibliothèque est solide** — sur ~200 tentatives d'attaque
documentées, les règles centrales n'ont pas cédé — **et sa limite la plus
intéressante n'est pas un bug mais une propriété du C++**, qu'il faut énoncer sur
scène plutôt que corriger dans le code.

## Ce qu'il faut corriger

| | défaut | sévérité | où |
|---|---|---|---|
| 1 | `std::stop_source` / `std::stop_token` déclarés `is_synchronizable` : corruption du tas reproductible | **critique** | [01](./01-robustesse-des-traits.md) |
| 2 | une poignée `copy_on_write` déplacée fait planter `as_mutable()` (`std::remove_if` suffit) | **élevée** | [02](./02-robustesse-des-helpers.md) |
| 3 | `const unique_ptr<T,D>` : la garde de type dynamique manque, contrairement à la règle sœur | **élevée** | [01](./01-robustesse-des-traits.md) |
| 4 | `shared_ptr` / `weak_ptr` cassent la transitivité de la possession que `CLAUDE.md` promet | **élevée** | [01](./01-robustesse-des-traits.md) |
| 5 | 5 mutants sur 19 survivent à la suite de tests | **élevée** | [03](./03-couverture-de-tests.md) |
| 6 | `launch_scoped_task` **sérialise** : quatre tâches de 200 ms prennent 817 ms | **élevée** | [08](./08-api-et-flexibilite.md) |
| 7 | l'idiome pimpl que la bibliothèque **recommande** est une erreur dure de 68 lignes | **élevée** | [08](./08-api-et-flexibilite.md) |
| 8 | le `shared_mutex` automatique perd contre `std::mutex` jusqu'à 90 % de lectures | **élevée** | [07](./07-performance-execution.md) |

Les quatre premiers ont un correctif **écrit, appliqué et vérifié** : les onze TU
de la suite existante recompilent. Le quatrième casse volontairement une
assertion, celle qui énonce la politique inverse — c'est un arbitrage à trancher,
et le rapport recommande de trancher pour la transitivité.

## Ce qu'il faut dire, et qu'aucun code ne peut corriger

**Une méthode `const` est un écrivain que la réflexion ne peut pas voir.**

```cpp
int slab[64];
struct SlabHandle {
    int index;
    void bump() const { ++slab[index]; }
};
```

Pas de membre `mutable`, pas de référence, pas de pointeur, que des membres
spéciaux implicites. `is_synchronizable_v<const SlabHandle>` est **vrai**,
`synchronized_value` choisit donc un `std::shared_mutex`, et deux `lock_shared()`
se coursent pour de bon — TSan le confirme, avec `mutexes: read M0` sur les deux
threads.

C'est exactement le point où l'analogie avec Rust cesse d'être vraie :

> `Sync` est sûr en Rust parce que `&T` **interdit** la mutation sans `UnsafeCell`.
> En C++, `const` n'interdit rien.

La réflexion voit des déclarations, pas des corps de fonctions. Aucune version de
ce trait ne peut fermer ce trou — et c'est le meilleur passage possible d'une
conférence sur le sujet. Il n'est écrit nulle part aujourd'hui.

La même limite revient trois fois ailleurs, toujours sous la même forme — **aucun
type C++ ne peut borner la durée de vie d'une référence qu'il rend** :
`copy_on_write::as_mutable()`, `value_guard::operator*`, et la précondition
explicite de `launch_scoped_task`. Le vérificateur l'a établi de la façon la plus
convaincante possible : il a **défait le correctif proposé** pour chacun des deux
premiers. La bonne conclusion n'est pas de multiplier les rustines, mais de dire
que la bibliothèque *rend les erreurs courantes difficiles* — ce qu'elle fait très
bien — sans les rendre impossibles.

## Ce que la mesure dit d'autre

**Le zéro-surcoût est prouvé au code machine, pas seulement en principe.**
`launch_task(f, args...)` et `std::jthread{f, args...}` compilés dans la même TU à
`-O2` produisent **161 instructions contre 161, dans le même ordre, avec le même
MD5**. Les concepts, les `assert_*` `consteval` et la surcharge de repli à
`std::meta::exception` ne laissent aucun résidu. En TU séparées, la version avec
traits est même 4 octets plus petite. De même, le `T*` stocké par `value_guard`
est entièrement dissous par SROA, y compris quand le guard reste vivant à travers
deux appels opaques.

**Le `shared_mutex` automatique est un piège de performance.**
`synchronized_value` choisit `std::shared_mutex` dès que
`is_synchronizable_v<const T>` — soit `int`, `double`, un agrégat, `vector`,
`string`, `map` : presque tout. Sur une section critique courte il **ne gagne
jamais**, à aucun nombre de threads, y compris à 99 % de lectures (jusqu'à
0,00× — deux ordres de grandeur). Il ne devient rentable qu'au-delà de ~100 ns
passées sous le verrou. La sélection est **sûre** — jamais un `shared_mutex` pour
un `T` dont la lecture `const` serait dangereuse — mais elle est silencieuse, et
l'utilisateur n'a aucun moyen de la contredire.

**La première heure d'un débutant, mesurée** : trois programmes naïfs, **7 rejets**.
Les messages sont courts et lisibles (14 à 22 lignes, cause sur l'avant-dernière
ligne) — un vrai point fort face aux centaines de lignes d'un échec de concept.
Mais **chacun des 7 désigne une cause autre que l'erreur commise** : « a closure
type with captures » quand l'utilisateur a capturé par référence, ou un membre
privé de libstdc++. Et le remède que le message le plus fréquent propose —
spécialiser le trait — **ne peut pas être écrit** pour une lambda de site d'appel.

## Deux idées reçues corrigées par la mesure

**Le coût de compilation n'est pas la réflexion.** Une TU **vide** qui ne fait
qu'inclure l'en-tête parapluie coûte 594 ms, sur les ~620 ms d'une TU de test
typique. `-ftime-report` est sans appel : la ligne « template instantiation » est
**identique** (0,20 s) dans les deux fichiers, et tout le travail des traits tient
dans 0,06 s de « constant expression evaluation », soit **moins de 9 %**.
L'audit précédent attribuait le coût à l'indirection `substitute`/`extract` —
« 32 % en instanciation de templates » — un chiffre présent dans un fichier qui ne
pose *aucune* question de trait. Le vrai levier est le découpage des en-têtes :
619 → 371 ms, vérifié. Détail en [06](./06-performance-compilation.md).

**Ce n'est pas la réflexion qui coûte, c'est `<ranges>`** : le seul concept
`borrowed_range` pèse ~135 ms, soit 36 % du coût « traits seuls ».

Un plafond existe malgré tout, à connaître : le trait répond jusqu'à **125 niveaux
d'imbrication**, au-delà desquels `-fconstexpr-depth` (512 par défaut) est épuisé —
et le message obtenu ne nomme ni le bon fichier ni le bon type. Hors de portée de
tout code réel, mais une ligne de documentation l'éviterait.

## Le frein réel à l'adoption

Le garde copie/déplacement refuse **10 types-valeur autonomes sur 16** testés —
`bitset`, `complex`, `chrono::milliseconds`, `time_point`, `valarray`,
`error_code`, `filesystem::path`, `future`, `promise`, `expected`. Le raisonnement
du garde est juste et n'a pas pu être contourné en sept tentatives ; c'est son
**ampleur** qui n'est nulle part énoncée. `std::string` et `std::vector` ne passent
que parce que `containers.h` les liste à la main : le défaut structurel refuse
l'essentiel de la bibliothèque standard, et les listes explicites la rattrapent.
`std::chrono::milliseconds` refusé est bloquant en pratique. Voir
[08](./08-api-et-flexibilite.md).

## Ce qui a résisté

À dire aussi nettement que les défauts :

- **La règle référence/pointeur** — `is_sendable<T&>` = `is_sendable<T*>` =
  `is_synchronizable<remove_cv_t<T>>` — n'a jamais été prise en défaut ; le retrait des
  cv est uniformément *plus strict*.
- **Aucun emprunt standard manqué** sur ~45 types : itérateurs, vues composées,
  `initializer_list`, `mdspan`, `coroutine_handle`, `any`, `smatch`, `path`, `error_code`.
- **La décision de détachement de `copy_on_write` est saine** : aucun faux « unique » en
  120 000 tours martelés contre quatre lecteurs. **La barrière acquire est exactement la
  bonne primitive dans la bonne branche** ([atomics.fences]/3).
- **`may_hijack_copy_move`** a résisté à sept tentatives de blanchiment.
- **Les helpers sont gratuits** : `lock()` à 6,18 ns contre 6,19 ns écrit à la main,
  `launch_task` à 12,6 µs contre 13,0 µs pour un `std::jthread` nu.
- **Le choix `shared_mutex`/`mutex` est mesurablement réel** : 4 lecteurs sur 4 en
  parallèle quand `T` le permet, 1 sur 4 sinon — la dégradation est la bonne réponse.
- **Le mécanisme réflexif est contraint, pas décoratif** : l'alternative évidente
  (`is_sendable_v<[:type:]>`) est **mal formée**, un paramètre de fonction n'étant jamais
  une expression constante.
- **La récursion est entièrement mémoïsée et linéaire** : poser mille fois la même
  question coûte le prix d'une seule (616 ms contre 614 ms), et le coût croît de
  ~0,4 ms par niveau d'imbrication et ~0,55 ms par membre. Aucun cache maison à
  ajouter — le compilateur en a déjà un.
- **Un test de bout en bout** enchaînant les cinq formes acceptées sur 40 tâches
  concurrentes tourne **propre sous ThreadSanitizer**.

## Les rapports

| | |
|---|---|
| [01 — Robustesse des traits](./01-robustesse-des-traits.md) | `is_synchronizable<T>`, `<const T>`, `is_sendable`, `is_lifetime_aware` |
| [02 — Robustesse des helpers](./02-robustesse-des-helpers.md) | `copy_on_write`, `synchronized_value`, `asynchronous_task_launcher` |
| [03 — Couverture de tests](./03-couverture-de-tests.md) | test par mutation, 5 survivants, deux fichiers de test proposés |
| [04 — Diagnostics](./04-diagnostics.md) | la chaîne de causes s'arrête au premier maillon |
| [05 — Simplicité](./05-simplicite.md) | valeur pédagogique, commentaires manquants, répétition de `containers.h` |
| [06 — Performance de compilation](./06-performance-compilation.md) | où va réellement le temps, et le découpage vérifié |
| [07 — Performance d'exécution](./07-performance-execution.md) | les helpers sont gratuits, chiffres à l'appui |
| [08 — API et flexibilité](./08-api-et-flexibilite.md) | asymétries de surface, faux négatifs, intégration CMake |
| [09 — Méthodologie](./09-methodologie.md) | le harnais, dont TSan sur du code à réflexion |
| [`scenarios/`](./scenarios/) | les 243 scénarios exécutés, rejouables |

## Si vous ne faites que quatre choses

1. **Corriger `stop_source`/`stop_token`** — c'est la seule corruption mémoire
   reproductible, et le correctif fait quatre lignes.
2. **Écrire la limite du `const`** dans `CLAUDE.md` et en tête de `synchronizable.h`.
   C'est gratuit, et cela transforme la faiblesse la plus profonde de l'approche en le
   meilleur moment de la présentation.
3. **Reformuler le message de la lambda capturante.** C'est le rejet n°1 d'un
   débutant, il accuse la réflexion plutôt que l'utilisateur, et son conseil est
   inapplicable au bloc. Une phrase à réécrire, pour le message le plus vu de la
   bibliothèque.
4. **Ajouter la TU de test d'exécution** de [03](./03-couverture-de-tests.md). Les trois
   helpers ont aujourd'hui un comportement qu'aucun `static_assert` ne peut atteindre —
   `std::make_shared` n'est pas `constexpr` — et deux mutants qui inversent la règle
   de détachement de `copy_on_write` passent la suite sans être vus.
