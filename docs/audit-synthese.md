# Audit ThreadSafe — synthèse

Audit complet de la bibliothèque sur les axes demandés : robustesse des traits et des helpers,
simplicité du code à vocation éducative, thread safety, performance à la compilation et au runtime,
ergonomie de l'API et flexibilité.

Le détail de chaque trouvaille — code fautif, sonde de reproduction, sortie du compilateur et
correction proposée — est dans [audit-details.md](audit-details.md). Les numéros entre crochets,
comme [#1], renvoient à ce document.

---

## Verdict

**Le modèle est sain ; c'est son application qui laisse passer des trous.**

La conception centrale — un walk conservateur, des traits fermés, un unique point d'extension `unsafe`
opt-in qui ne peut qu'accorder la confiance — tient l'analyse. Les tentatives de la contourner par un
type mal formé, une base privée, un membre `mutable`, un `void*`, un type incomplet ou un pointeur vers
base polymorphe ont toutes échoué : le walk répond correctement. Vingt-quatre pistes de faille explorées
par les agents ont été **réfutées** à la vérification, le plus souvent parce qu'une autre branche du walk
attrapait déjà le cas.

Un seul trou de soundness est **critique**, et il est réel : `is_sendable` traite une référence rvalue
comme une valeur. Un membre `int&&` est déclaré *sendable* alors que son jumeau `int&` est correctement
refusé. J'ai reproduit la data race qui en découle en n'utilisant que l'API publique : le compteur
partagé finit à 3 937 755 au lieu de 4 000 000. La correction tient en cinq lignes, ne casse que les deux
assertions de test qui encodaient l'ancien comportement, et rend au passage le code littéralement
conforme à l'équivalence annoncée dans `CLAUDE.md`.

Le reste se répartit entre des trous de soundness plus étroits mais réels (le deleter type-effacé de
`shared_ptr`, le guard qui survit à son `synchronized_value` temporaire), un piège de performance à
100× dans `synchronized_value`, un deadlock franc dans `launch_scoped_task`, et une longue traîne de
frictions d'ergonomie et de lisibilité qui comptent doublement pour un code destiné à être projeté
sur un écran devant une salle.

---

## Méthode

Quatorze agents d'exploration, un par trait et par helper, chacun muni du compilateur et chargé de
**prouver** ses trouvailles en écrivant des sondes `.cpp` et en les compilant. Chaque trouvaille est
ensuite passée à un agent indépendant chargé de la **réfuter** : recompiler la sonde, vérifier que le
code incriminé existe bien tel quel, refaire les mesures chiffrées, et appliquer le correctif proposé
sur une copie du dépôt pour vérifier que `cmake --build` passe toujours.

| | |
|---|---|
| Agents exécutés | 90 (88 aboutis, 2 interrompus par la limite de session) |
| Trouvailles soumises | 73 |
| **Confirmées** | **49** + 1 établie directement → **50 fiches** |
| Réfutées à la vérification | 24 (conservées en annexe avec leur motif) |

Les 50 fiches couvrent **43 problèmes distincts** : quatre grappes ont été trouvées par plusieurs
agents sous des angles différents et sont conservées séparément parce que chacune apporte une sonde et
un argument propres — la grappe `is_smart_pointer` [#3] [#5] [#7] [#11], la branche morte de
`diagnose_is_synchronizable` [#32] [#33] [#35], `std::chrono` [#4] [#20], et `const copy_on_write`
[#19] [#22].

**Ce que l'audit n'a pas couvert.** Les deux agents chargés des angles morts (combinaisons croisées de
features, couverture de la suite de tests) ont été interrompus par la limite de session. J'ai repris à
la main le point le plus saillant de leur périmètre — les tests négatifs [#12] — mais la revue de
couverture systématique reste à faire. Par ailleurs `-fsanitize=thread` n'est pas liable avec GCC 16 sur
cette machine (arm64/macOS, `ld: symbol(s) not found`) : les data races ont dû être établies par perte
de mises à jour observable et par raisonnement sur le modèle mémoire, pas par sanitizer.

---

## Tableau de bord

| Sévérité | Nombre | |
|---|---|---|
| Critique | 1 | trou de soundness exploitable |
| Majeur | 12 | soundness étroite, deadlock, piège de performance, ergonomie bloquante |
| Mineur | 30 | conservatisme, lisibilité, coût de compilation |
| Information | 7 | vérifications positives et mesures de référence |

| Axe | Fiches |
|---|---|
| Simplicité / valeur éducative | 13 |
| Flexibilité | 7 |
| API | 6 |
| Performance à la compilation | 6 |
| Soundness | 5 |
| Performance au runtime | 5 |
| Thread safety | 3 |
| Conservatisme | 3 |
| Tests | 2 |

---

## 1. Robustesse

### Le trou critique : la référence rvalue [#1]

`diagnose_is_sendable` teste `is_lvalue_reference_type` et non `is_reference_type`. Une référence
rvalue échappe donc à la branche indirection, et la ligne `type = remove_reference(type)` la réduit à
son référent : **`X&&` est répondu exactement comme `X`**.

```cpp
struct HoldsLvalueRef { int&  borrowed; };
struct HoldsRvalueRef { int&& borrowed; };

static_assert(!threadsafe::is_sendable_v<HoldsLvalueRef>);  // correct
static_assert( threadsafe::is_sendable_v<HoldsRvalueRef>);  // le trou
```

Or `int&& borrowed = std::move(counter)` laisse `counter` parfaitement vivant et modifiable par le
thread d'origine : c'est le même aliasing qu'une lvalue référence, que la bibliothèque refuse à juste
titre. Le trou se propage : `std::tuple<int&&>` est *sendable*, c'est-à-dire exactement ce que produit
`std::forward_as_tuple(std::move(x))`, et `synchronized_value<HoldsRvalueRef>` passe son
`static_assert(sendable<T>)`.

J'ai vérifié la sonde et la correction moi-même. Après le correctif, l'ensemble de la suite échoue sur
exactement deux assertions, celles qui encodaient l'ancien comportement :

- `tests/test_sendable.cpp:270` — `static_assert(is_sendable_v<int&&>);`, sans message ;
- `tests/test_polymorphic.cpp:74` — *« a final referent is exactly its static type »*.

À noter que la suite de tests actuelle porte déjà deux justifications contradictoires sur ce point :
`test_sendable.cpp` affirme *« an rvalue reference shares the referent too »* — ce que le code ne fait
précisément pas. `is_synchronizable`, lui, traite le cas correctement : son walk teste
`is_reference_type` sans distinguer les deux. Le trou est spécifique à `is_sendable`.

### Les autres trous de soundness

| Fiche | Problème | Sévérité |
|---|---|---|
| [#8] | Le deleter type-effacé de `shared_ptr` n'est jamais interrogé — un deleter capturant une référence passe `launch_task`, use-after-scope | Majeur |
| [#9] | `lock()` / `lock_shared()` ne sont pas ref-qualifiés : un guard obtenu d'un `synchronized_value` temporaire lui survit | Majeur |
| [#10] | `shared_readable` fait dépendre la **disposition mémoire** d'une réponse de trait : deux unités de traduction qui répondent différemment produisent des objets de tailles différentes et lient sans diagnostic | Majeur |
| [#11] | `unique_ptr` à deleter personnalisé : *sendable* mais jamais *lifetime_aware* | Majeur |

[#10] est le plus insidieux, et il est le pendant exact du piège d'ordre de déclaration déjà connu de
`CLAUDE.md` : là où celui-ci ne produit qu'une divergence de réponse, celui-là produit une **violation
d'ODR silencieuse** parce que `sizeof(synchronized_value<T>)` change avec la réponse.

### Trait par trait

**`is_synchronizable<T>`** — sain. Le garde `if (!is_const(type)) return false` n'est contournable par
aucun chemin trouvé. Les unions, bitfields, bases virtuelles, bases privées et types anonymes sont
correctement traités. Trois fiches signalent que la branche lvalue-reference est **inatteignable**
[#32] [#33] [#35] : du code mort à supprimer, sans conséquence sur les réponses.

**`is_synchronizable<const T>`** — sain, y compris sur les points les plus délicats. Le traitement des
membres `mutable` (interrogés sans `const`, donc devant être synchronizable par eux-mêmes) est correct ;
celui des membres référence l'est aussi. Une seule friction : un `mutable std::mutex`, l'idiome même du
type qui se protège lui-même, tue la lecture concurrente [#18].

**`is_sendable<T>`** — porte le trou critique [#1]. L'équivalence documentée
`is_sendable<T&> = is_sendable<T*> = is_synchronizable<T>` est tenue pour les lvalue références et les
pointeurs, et le garde polymorphe fonctionne aussi bien sur `Base*` que sur `Base&`.

**`is_lifetime_aware<T>`** — sain sur son cœur : `std::string_view`, `std::span` et les `subrange` sont
correctement rejetés via `borrowed_range`, et la possession par pointeur nu est bien barrée par
`is_default_type`. Le trou est au niveau des vouchs, pas du walk : `unique_ptr<T, D>` avec `D` explicite
n'est pas reconnu comme *smart pointer* [#11].

### Helper par helper

**`copy_on_write<T>`** — le point délicat est `as_mutable()`. La barrière acquire y est **nécessaire**,
et la vérification a corrigé au passage l'explication qu'on serait tenté d'en donner : ce qu'elle achète
n'est pas de se prémunir contre un objet à moitié écrit par le propriétaire disparu — l'invariant
interdit d'écrire le bloc tant qu'il est partagé — mais le sens inverse. Le propriétaire qui s'est
détaché a **lu** l'objet partagé avant son décrément release ; sans acquire, nos écritures ultérieures
peuvent remonter au-dessus du load relaxed et courser ces lectures [#42]. Elle coûte 0,38 ns par appel
[#46] : le prix est juste. C'est la ligne la plus subtile de la bibliothèque et elle ne porte aucun
commentaire, alors qu'elle finira sur une slide.

Un vrai défaut en revanche : `as_mutable()` rend une référence nue qui survit au contrôle d'exclusivité,
et le use-after-free correspondant est prouvé [#43].

**`synchronized_value<T>`** — l'enveloppe est correcte et **gratuite** : le corps assembleur d'un
incrément sous `synchronized_value` est identique instruction par instruction à celui d'un
`std::mutex` + valeur écrits à la main, le `value_guard` étant entièrement élidé [#45]. Les défauts sont
autour : le guard non ref-qualifié [#9], la disposition mémoire dépendante du trait [#10], le choix
de verrou aveugle à la charge [#6], et l'impossibilité d'attendre [#21] ou de verrouiller deux valeurs
ensemble [#14].

**`asynchronous_task_launcher`** — l'exigence « ne doit pas accepter de type unsafe » est **tenue**.
Les tentatives de la contourner par `std::ref`, par un pointeur nu, par un lambda capturant, par un
`std::function` ou par un pointeur de fonction membre ont toutes été refusées. Le défaut est ailleurs,
et il est franc : `launch_scoped_task` **se bloque à jamais** dès que le callable accepte le
`stop_token` que `jthread` lui injecte [#13].

---

## 2. Simplicité et valeur éducative

Treize fiches, aucune bloquante, mais l'axe compte double ici : ce code sera lu ligne à ligne par une
salle.

- **Du code mort** dans le trait le plus central : la branche lvalue-reference de
  `diagnose_is_synchronizable` est inatteignable [#32] [#33] [#35], et le court-circuit
  `is_function_type` à l'intérieur de la branche pointeur est redondant [#34].
- **Des noms qui ne disent pas ce que fait le corps** : `has_unreflectable_state` teste en réalité
  « n'est pas vide, n'est pas polymorphe, sans base, sans membre » [#38] ; `pointee_is_synchronizable`
  et `pointee_answer` posent deux questions **différentes** sous des noms interchangeables [#41].
- **Le pivot du trait n'a pas de nom** : `diagnose_is_synchronizable` compte huit branches et celle qui
  porte tout le sens — le garde `const` — passe inaperçue au milieu des autres [#37].
- **Des asymétries gratuites** : `is_lifetime_aware` écrit son `value` à la main là où ses deux jumelles
  dérivent de `std::bool_constant` [#39], et réimplémente le helper `trait_value` que `utils.h` expose
  déjà [#40].
- **Des en-têtes qui ne compilent pas seuls** : `smart_pointers.h` [#30] et
  `asynchronous_task_launcher.h` [#36] ne tiennent que par l'ordre d'inclusion de `threadsafe.h`.
- **Pas de `.clang-format`** : quatre en-têtes sur douze dérivent vers l'indentation à quatre
  espaces [#48].
- **Le détour `is_smart_pointer`** : vingt-quatre lignes d'API publique dont le seul effet net est de
  produire un faux négatif [#7].

Enfin, une remarque qui dépasse la lisibilité : **le dépôt ne contient aucun programme exécutable**
[#50]. Une bibliothèque de thread safety dont la suite de tests ne démarre jamais un thread se prive
de sa démonstration la plus convaincante.

---

## 3. Thread safety

**Pas de data race dans le code de la bibliothèque** — vérifié par lecture et par exécution sous
charge, à une exception près qui est un défaut d'API plutôt que de code : `as_mutable()` laisse fuir
une référence [#43].

Le vrai sujet est celui que la tâche formule comme « difficile d'avoir des race conditions », et la
réponse est nuancée. L'API laisse ouverts deux pièges de type *check-then-act* que rien ne signale :

```cpp
if (sv.lock()->empty())      // premier verrou, relâché au point-virgule
    sv.lock()->push_back(x); // second verrou : l'état a pu changer entre les deux
```

Le remède naturel — une méthode `with(F)` qui prend un lambda et garantit un seul verrou — n'existe pas.
Le même piège existe sur `copy_on_write` : entre `cow->size()` et `cow.as_mutable()`, une copie a pu
s'intercaler.

S'y ajoutent deux blocages francs : le deadlock de `launch_scoped_task` [#13], et l'absence de tout
moyen de verrouiller deux `synchronized_value` ensemble [#14], qui fait de l'inversion d'ordre des
verrous un deadlock accessible au premier utilisateur venu.

---

## 4. Performance à la compilation

**Conclusion : le coût de la bibliothèque, ce sont ses `#include`, pas sa réflection.** Sur une unité de
traduction qui inclut l'en-tête maître et ne fait rien, `-ftime-report` attribue **73 % du temps au
parsing et 2 % à l'évaluation d'expressions constantes** [#44]. La mémoïsation par `_v` fonctionne
parfaitement : interroger le même type mille fois ne coûte pas plus cher que l'interroger une fois.

Mes propres mesures le confirment : une sonde vide qui inclut `threadsafe.h` coûte **0,57 s**, et un
fichier de test complet **0,59 à 0,67 s**. Les traits eux-mêmes sont noyés dans le bruit.

Coût isolé des en-têtes standard, net d'une unité vide (34 ms), minimum sur sept exécutions :

| En-tête | Coût | | En-tête | Coût |
|---|---|---|---|---|
| `<mutex>` | 440 ms | | `<memory>` | 230 ms |
| `<thread>` | 436 ms | | `<functional>` | 206 ms |
| `<shared_mutex>` | 269 ms | | `<meta>` | 185 ms |
| `<ranges>` | 253 ms | | `<vector>` | 168 ms |
| `<stop_token>` | 248 ms | | `<algorithm>` | 94 ms |

Quatre gains, tous appliqués et mesurés par les agents :

1. **Découper en trois niveaux emboîtés** (`traits.h` → `core.h` → `threadsafe.h`) [#27]. Qui ne veut
   que `is_sendable` sur ses propres types paie aujourd'hui 628 ms et 463 en-têtes ; les traits seuls
   coûtent **248 ms** pour 299 en-têtes. C'est le gain le plus important, et il est bloqué tant que
   `smart_pointers.h` n'est pas auto-suffisant [#30].
2. **Retirer `<functional>` et `<memory>` de `lifetime_aware.h`** [#28] : aucun de leurs symboles n'y
   est utilisé, et ce fichier est tiré par tous les autres. **−66 ms (−17 %)**.
3. **Remplacer `<ranges>`** dans `lifetime_aware.h`, inclus pour le seul `borrowed_range` [#29].
   C'est le poste le plus lourd imputable à la bibliothèque elle-même.
4. **Remplacer les deux appels à `<algorithm>`** (`ranges::contains`, `ranges::all_of`) par des boucles
   `for` [#26] : **−25 ms**, et un code plus lisible — double gain vu l'objectif pédagogique.

---

## 5. Performance au runtime

**La trouvaille de l'axe : `synchronized_value` choisit son type de verrou sur un critère de
correction, pas de charge** [#6]. `shared_readable` vaut `is_synchronizable_v<const T>`, donc rendre son
`const T` sûr bascule silencieusement l'implémentation vers `std::shared_mutex`.

J'ai refait la mesure moi-même, sur des lecteurs seuls :

| Lecteurs | `shared_mutex` | `mutex` | Rapport |
|---|---|---|---|
| 1 | 91,1 Mops/s | 122,9 Mops/s | 1,35× |
| 2 | 52,3 Mops/s | 87,5 Mops/s | 1,67× |
| 4 | 8,9 Mops/s | 66,6 Mops/s | **7,44×** |
| 8 | 4,8 Mops/s | 59,1 Mops/s | **12,3×** |

Les agents mesurent des facteurs plus élevés encore — de l'ordre de 100× — sur des charges mixant
lectures et écritures. Les deux mesures concordent sur le sens ; je retiens le mien, plus conservateur,
pour l'ordre de grandeur, et je ne reprends pas le chiffre de 170× du titre de la fiche.

Le point décisif est que le choix est **aveugle** : `shared_mutex` ne gagne que si la section critique de
lecture est longue, auquel cas il l'emporte d'environ 9×. C'est donc un pile-ou-face à 10× dans un sens
et 9× dans l'autre, arbitré par une information — la sûreté du `const T` — qui ne dit rien de la charge.
Rien dans `CLAUDE.md` ni dans les tests n'en avertit.

Le reste de l'axe est rassurant :

- `synchronized_value` **ne coûte rien** de plus qu'un mutex écrit à la main ; le `value_guard` est
  entièrement élidé [#45].
- La barrière acquire de `copy_on_write` coûte **0,38 ns/appel** et `use_count()` ne provoque pas de
  cache miss [#46] — la fiche corrige au passage sa propre arithmétique, la machine ayant des lignes de
  cache de 128 octets et non 64.
- `launch_task` par valeur coûte **exactement un move supplémentaire par argument**, invisible face aux
  11 µs de création d'un thread [#47]. Le passage par valeur n'est pas à changer.
- Un bémol mesuré mais à confirmer : deux `synchronized_value` voisins partagent une ligne de cache, et
  le faux partage coûte plusieurs fois le débit sur un tableau de verrous indépendants [#31].

---

## 6. API et flexibilité

**Le point le plus coûteux à l'usage est le diagnostic** [#2]. Quand `is_sendable_v<MonType>` est faux,
l'utilisateur reçoit une dizaine de lignes de GCC qui disent seulement *« un argument n'est pas
sendable »* — jamais **quel membre**, jamais **pourquoi**. Sur un type à cinq membres dont un seul,
imbriqué à trois niveaux, est fautif, l'information utile est absente. C'est la friction qui décidera
de l'adoption, et c'est aussi celle qui se verra le plus en conférence, quand une erreur volontaire sera
affichée à l'écran.

La contrainte de `CLAUDE.md` — l'explication ne vit pas dans le trait — n'interdit pas la solution : une
fonction `explain_sendable<T>()` consteval **séparée**, que l'utilisateur appelle volontairement, nomme
le premier membre fautif sans que le trait cesse de rendre un `bool` nu.

Ce défaut a un corollaire sur les tests [#12] : les quinze fichiers de `tests/build_errors/` ne sont
compilés par **aucune cible** — `grep -rn build_errors --include=CMakeLists.txt` ne renvoie rien. La
moitié négative du contrat, celle qui compte le plus pour une bibliothèque dont la valeur est de dire
non, n'est donc jamais vérifiée. Je les ai compilés à la main : les quinze échouent bien, mais **dix
d'entre eux sur le même message générique**. Même une fois câblés, un simple `WILL_FAIL` ne
distinguerait pas un refus pour la bonne raison d'un refus pour une mauvaise : ancrer finement les
diagnostics attendus suppose d'abord de les rendre nominatifs.

**Un second point d'extension non voulu** existe : `threadsafe::is_smart_pointer` est publiquement
spécialisable et accorde la confiance sans que le mot `unsafe` apparaisse nulle part [#3]. C'est
précisément la règle que `CLAUDE.md` pose — *« le mot unsafe apparaît partout où la connaissance est
affirmée au lieu d'être prouvée »* — et le seul endroit du dépôt qui l'enfreint.

**Faux négatifs qui bloquent des usages quotidiens** — tous corrigeables par un vouch manquant :

| Fiche | Type refusé | Conséquence |
|---|---|---|
| [#4] [#20] | `std::chrono::duration`, `time_point` | tout lancement de tâche avec un délai est refusé |
| [#5] [#11] | `unique_ptr` à deleter personnalisé | `launch_task` le refuse |
| [#25] | `std::latch`, `std::barrier`, `std::counting_semaphore` | impossible de partager une barrière |
| [#24] | `std::bitset`, `std::complex`, `std::expected`, `flat_map` | types-valeurs purs refusés |
| [#23] | `std::array<std::atomic<int>, N>` | refusé alors que `std::atomic<int>[N]` passe |
| [#19] [#22] | `const copy_on_write<T>` | `copy_on_write` ne se compose pas avec lui-même |

Que `std::chrono` soit refusé est le plus gênant des six : c'est le vocabulaire même du threading.

**Lacunes d'API** : pas de concept `threadsafe::synchronizable` alors que ses deux jumeaux existent
[#16] ; les fonctions `is_unsafe_*_type`, purement internes, sont publiques [#17] ; on ne peut pas
attendre sur un `synchronized_value` faute que `value_guard` soit un Lock [#21] ; et le message qui
refuse un lambda capturant ne dit pas quoi faire à la place [#15].

---

## Ce qui est confirmé sain

À signaler autant que les défauts, parce que c'est ce que la conférence défendra :

- Le walk conservateur **tient** : types incomplets, `void`, `void*`, bases privées, membres `mutable`,
  unions, bitfields, pointeurs vers base polymorphe, constructeurs template détournant la copie —
  tous correctement traités. Vingt-quatre tentatives de faille ont été réfutées.
- La fermeture des traits est **effective** : à l'exception de `is_smart_pointer` [#3], aucun chemin ne
  permet d'accorder la confiance sans écrire le mot `unsafe`.
- `asynchronous_task_launcher` **remplit son contrat** : aucun type unsafe n'a réussi à passer.
- La mémoïsation par `_v` fonctionne exactement comme annoncé [#44].
- `synchronized_value` est une **abstraction à coût nul** [#45].
- Les quinze tests négatifs échouent tous correctement — même s'ils ne sont jamais exécutés [#12].

---

## Plan d'action

**À corriger avant toute présentation publique**

1. [#1] La référence rvalue dans `is_sendable`. Correctif de cinq lignes, vérifié ; inverser les deux
   assertions de test qui encodaient l'ancien comportement.
2. [#13] Le deadlock de `launch_scoped_task`.
3. [#12] Câbler `tests/build_errors/` dans le build — sans quoi aucune régression de la moitié négative
   du contrat ne sera détectée, y compris celles de cette liste.

**Ensuite, par rapport valeur / effort**

4. [#2] Le diagnostic nominatif — la friction la plus visible à l'usage comme sur scène, et le
   préalable à des tests négatifs précis.
5. [#9] [#10] Ref-qualifier `lock()` / `lock_shared()` ; traiter la dépendance de la disposition
   mémoire au trait.
6. [#4] [#5] [#25] Les vouchs manquants — `chrono` d'abord, puis `unique_ptr<T,D>` et les primitives de
   synchronisation. Peu de lignes, beaucoup d'usages débloqués.
7. [#6] Documenter ou rendre configurable le choix du verrou. Ne pas laisser un facteur 10 dépendre
   silencieusement d'une propriété de correction.
8. [#27] [#28] [#29] [#26] Le découpage en trois niveaux et les includes morts : `−66 ms` immédiats,
   `628 ms → 248 ms` pour qui ne veut que les traits.

**Passe de lisibilité avant la conférence**

9. [#32] [#33] [#35] [#34] Supprimer le code mort du trait le plus central.
10. [#37] [#38] [#41] [#49] Nommer le garde `const`, renommer `has_unreflectable_state`, distinguer les deux
    helpers `pointee_*`.
11. [#3] [#7] Retirer le détour `is_smart_pointer` : supprime le second point d'extension **et** le faux
    négatif sur `unique_ptr`.
12. [#48] [#50] Ajouter un `.clang-format` et au moins un exemple exécutable qui démarre des threads.

**Reste à faire**

La revue de couverture de la suite de tests et les combinaisons croisées de features n'ont pas été
menées : les deux agents qui en avaient la charge ont été interrompus. C'est le premier périmètre à
reprendre.
