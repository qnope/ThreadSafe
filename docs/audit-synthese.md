# Audit ThreadSafe — synthèse

Audit complet de la bibliothèque selon [`Task.md`](../Task.md) : robustesse trait par
trait et helper par helper, simplicité du code éducatif, thread safety, performance à
la compilation et à l'exécution, facilité d'usage, flexibilité.

Le détail de chaque constat — code incriminé, correction, sonde de preuve — est dans
[`audit-details.md`](audit-details.md). Ce document-ci donne la lecture d'ensemble et
l'ordre des travaux.

---

## Verdict

**Le modèle est sain, son implémentation a deux trous de sûreté, et la clôture des
traits que le projet revendique n'existe pas dans le code.**

La marche réflective fait ce qu'elle promet : elle est conservatrice, elle refuse ce
qu'elle ne sait pas prouver, et sur la très grande majorité des formes que nous lui
avons soumises — unions, bitfields, classes vides, bases privées, héritage virtuel,
lambdas, énumérations, tableaux, types polymorphes — elle répond juste. Les deux
défauts de sûreté trouvés ne viennent pas du modèle mais d'un chemin de code oublié
(`T&&`) et d'un point d'extension qui a échappé à la discipline `is_unsafe_` du reste
de la bibliothèque (`is_smart_pointer`). Les deux se corrigent en quelques lignes.

Le reste des constats relève du polissage, et deux d'entre eux comptent plus que leur
sévérité ne le suggère parce que la bibliothèque est un **support de conférence** :
l'exigence de `Task.md` « `asynchronous_task` ne doit pas accepter de type unsafe »
n'est aujourd'hui **pas vérifiable par un test**, et l'exemple canonique de tout cours
de thread safety — verrouiller deux valeurs à la fois — n'a pas de réponse dans
l'API.

| Sévérité | Nombre |
|---|---|
| Critique — trou de sûreté, ou course de données réelle | 3 |
| Majeur — faux négatif bloquant, piège d'API silencieux, exigence non satisfaite, gain de performance mesuré | 12 |
| Mineur — friction | 37 |
| Détail — cosmétique, ou constat informatif à conserver | 14 |

---

## Ce qui a été vérifié et qui tient

Ces points ont été attaqués délibérément et ont résisté. Ils ne demandent aucune
action, et plusieurs méritent d'être dits à voix haute en conférence.

- **Les traits ne coûtent rien à l'exécution.** Vérifié en lisant l'assembleur `-O2` :
  il ne reste aucune trace du walk, et `value_guard` est entièrement élidé
  (détail `PR-04`).
- **La mémoïsation par `_v` fonctionne réellement**, y compris à travers les arêtes du
  graphe de types — ce n'était pas acquis, puisque la récursion passe par
  `substitute` / `extract` (`PC-06`). De même, `assert_queryable_type` et
  `std::remove_all_extents_t`, instanciés à chaque nœud du walk, ne coûtent rien de
  mesurable (`PC-07`).
- **La barrière `acquire` de `copy_on_write::as_mutable` coûte 0,34 ns par appel** et
  doit être conservée : la proposition de la retirer a été réfutée.
- **Le passage par valeur de `launch_task` a un coût mesuré nul** après optimisation ;
  c'est de plus un choix de sûreté (un perfect-forwarding ferait traverser une
  référence à la frontière de thread). À garder tel quel (`PR-03`).
- **Le piège d'ordre d'inclusion est diagnostiqué par GCC à l'intérieur d'une unité de
  traduction** (`specialization ... after instantiation`). `CLAUDE.md` laisse croire à
  un changement silencieux : c'est faux intra-TU, et seul le cas inter-TU pose un vrai
  problème (voir `TEST-01`).
- **`launch_scoped_task` est sûr.** Deux auditeurs ont affirmé y avoir prouvé un
  use-after-free et un défaut de conception ; les deux ont été réfutés par
  compilation. Le `join` justifie bien l'abandon de `is_lifetime_aware`.
- **`value_guard` ne laisse pas fuir de référence de façon anormale.** L'affirmation
  contraire a été réfutée : la fuite possible est celle, inévitable, de tout
  `lock_guard` du standard, et les surcharges `&&` supprimées bloquent la seule forme
  réellement piégeuse.

---

## Les trois constats critiques

### 1. Le chemin `T&&` contourne toute la logique de `is_sendable` — `SEND-01`

`diagnose_is_sendable` traite les lvalue references comme des références, puis fait
`type = remove_reference(type)` et traite tout le reste — donc les **rvalue
references** — comme des valeurs. Un agrégat `struct A { int&& r; };` est donc déclaré
*sendable*, alors que le même agrégat écrit avec `&` est refusé. Le trait dit oui sur
un type qui partage de la mémoire mutable, et le trou est atteignable depuis l'API
publique : `launch_scoped_task` l'accepte, `synchronized_value` aussi.

Ce n'est pas un comportement voulu : l'auteur a écrit lui-même la règle attendue dans
`tests/test_sendable.cpp:135` (« *an rvalue reference shares the referent too* »).
Trois auditeurs indépendants ont trouvé ce défaut par trois chemins différents.

**Correction** : fusionner les deux branches de référence — deux lignes en moins, et
la suite de la fonction ne voit alors plus jamais de référence. Quatre assertions de
test encodent la posture actuelle et doivent être inversées ; la recompilation des 12
fichiers de tests confirme qu'il n'y en a pas une cinquième.

### 2. `is_smart_pointer` est une porte dérobée non marquée `unsafe` — `LIFE-01`

`CLAUDE.md` affirme que `is_unsafe_<trait>` est *the one customization point*. C'est
faux : `is_smart_pointer` est un trait ouvert, spécialisable par l'utilisateur, sans
le mot `unsafe` dans son nom, et la spécialisation groupée
`template <smart_pointer T> struct is_unsafe_lifetime_aware<T>` en fait un second
point d'octroi de confiance. Un type qui ne possède rien devient `lifetime_aware` dès
qu'il s'y inscrit — un emprunt brut traverse alors `launch_task` et le thread écrit
dans un objet mort. Trouvé indépendamment par trois auditeurs.

S'y ajoute une ambiguïté d'instanciation : un type à la fois `std_wrapper` et
`smart_pointer` rend `is_unsafe_lifetime_aware<T>` ambigu, et un type non-template
inscrit dans `is_smart_pointer` fait remonter une `std::meta::exception` brute
(`LIFE-04`).

### 3. Divergence inter-TU des spécialisations tardives — `TEST-01`

Intra-TU, GCC protège l'utilisateur. Mais si la TU A pose `is_sendable_v<Late>` sans
inclure l'en-tête qui vouche pour `Late`, et que la TU B le fait après l'avoir inclus,
les deux TU définissent le même symbole avec deux valeurs : violation d'ODR
silencieuse, aucun diagnostic, comportement dépendant de l'éditeur de liens. C'est le
seul défaut de l'audit qu'aucune correction de code ne referme — il se traite par la
discipline d'inclusion et par la documentation.

---

## Les douze constats majeurs

**Sûreté et clôture du modèle**

- `SEND-02` — **Les traits sûrs ne sont pas clos.** `is_sendable`, `is_synchronizable`
  et `is_lifetime_aware` sont trois templates ordinaires, donc spécialisables *dans
  les deux sens*. Un `: std::false_type` révoque une confiance que la couche
  `is_unsafe_` est précisément censée rendre irrévocable, et la contrefaçon se propage
  par la récursion à tout type qui contient le type falsifié.
- `WALK-01` — Un **pointeur vers type incomplet** (le PImpl, la forme la plus banale)
  échappe au `static_assert` de la bibliothèque et sort une erreur de précondition
  interne de GCC. La promesse « empoisonner la question » est tenue, mais avec le
  mauvais message.
- `LIFE-02` — Un **type récursif possédant** (`Node` contenant `unique_ptr<Node>` ou
  `vector<Node>`) rend les trois traits ininterrogeables, avec une cascade d'erreurs
  qui ne nomme jamais le coupable. Arbres et listes chaînées possédantes sont hors
  d'atteinte de la bibliothèque.

**Helpers**

- `COW-01` — `copy_on_write::operator*() &&` est `noexcept` alors qu'il renvoie une
  **copie** de `T` : `std::terminate` prouvé dès que la copie peut lancer. Le même
  constat côté performance montre une copie profonde silencieuse de +160 µs sur un
  vecteur de 8 Mio.
- `COW-02` — `as_mutable() &&` n'a **aucune contrainte** : la détection répond oui pour
  un `T` move-only, puis l'appel produit une erreur dure dans le corps — exactement ce
  que `tests/test_copy_on_write.cpp:131` prétend avoir exclu, en ne testant que la
  moitié lvalue de l'API.
- `SV-01` — **Aucun moyen de verrouiller deux `synchronized_value` ensemble.**
  `mutex_` est privé, il n'existe pas d'équivalent de `std::scoped_lock`, et le
  virement entre deux comptes — l'exemple canonique — se bloque en interblocage sans
  un mot du compilateur. Pour une bibliothèque dont l'argument est « difficile d'avoir
  des race conditions », c'est la question du public à laquelle il faut pouvoir
  répondre.

**Exigences de `Task.md` non satisfaites**

- `TEST-02` — **`asynchronous_task_launcher` accepte un type unsafe au sens de la
  surcharge.** La surcharge de diagnostic non contrainte rend
  `l.launch_task(f, args...)` *bien formée* pour un `F` unsafe ; les `static_assert`
  ne se déclenchent qu'à l'instanciation du corps. L'appel réel échoue toujours — ce
  n'est donc pas un trou de sûreté — mais l'exigence n'est **pas testable**, et tout
  code générique qui sonde `requires { l.launch_task(f); }` obtient une réponse
  fausse. La correction (`= delete("raison")`, C++26) rend l'expression détectable en
  préservant le message pédagogique ; le test manquant a été écrit et vérifié.
- `TEST-03` — **Zéro test d'exécution.** La cible est une *object library* sans
  exécutable. La branche `use_count() != 1` de `copy_on_write` — c'est-à-dire *la*
  sémantique du type — n'est jamais exécutée, ni l'exclusion effective de
  `value_guard`, ni la création des threads. Un bug qui ferait détacher
  systématiquement, ou jamais, passerait toute la suite.

**Performance**

- `PC-01` — **83 % du temps d'une TU de test est du re-parsing d'en-têtes standard**,
  pas du walk réflectif. Deux lignes de CMake (`CXX_SCAN_FOR_MODULES OFF`, le projet
  n'a aucun module ; puis `target_precompile_headers`) rendent 64 % du temps de build.
  C'est le meilleur rapport gain/effort de tout l'audit.
- `PC-02` — `threadsafe.h` impose `<mutex>`, `<shared_mutex>` et `<thread>` — les
  en-têtes les plus chers du graphe — à qui ne veut que les traits : +36 % par TU.
  Un `threadsafe/traits.h` séparé suffit.
- `PR-01` — **`synchronized_value` choisit `std::shared_mutex` par défaut** dès que
  `is_synchronizable_v<const T>`, c'est-à-dire presque toujours. Mesuré : 1,3× à 163×
  plus lent en section critique courte, et `sizeof(synchronized_value<int>) = 208`
  octets dont 200 de mutex. Le trait répond « le partage en lecture est-il *sûr* ? »,
  pas « est-il *rentable* ? » — le défaut doit être `std::mutex`, le partage en
  opt-in.
- `PR-02` — `threads_` n'est **jamais purgé** : un `jthread` terminé mais non joint
  retient toute sa structure OS. Mesuré à 16,1 Ko de RSS par tâche terminée, soit
  322 Mo pour 20 000 tâches courtes. Un launcher à durée de vie applicative fuit.

---

## Couverture

`Task.md` demande de tester chaque trait et chaque helper **individuellement**. Le
document de détail est organisé ainsi, et voici ce que chacun a donné.

| Composant | Total | Crit. | Maj. | État |
|---|--:|--:|--:|---|
| `is_sendable<T>` | 3 | 1 | 1 | Un trou de sûreté, fermé par 2 lignes |
| `is_synchronizable<T>` / `<const T>` | 5 | — | — | Aucun trou de sûreté trouvé |
| `is_lifetime_aware<T>` | 6 | 1 | 1 | Un trou, et les types récursifs inutilisables |
| Fondations du walk (`utils.h`, wrappers std) | 3 | — | 1 | Diagnostics à reprendre |
| `copy_on_write<T>` | 5 | — | 2 | Move et use-after-move exclus, comme demandé |
| `synchronized_value<T>` | 4 | — | 1 | Manque le verrouillage de plusieurs valeurs |
| `asynchronous_task_launcher` | 5 | — | — | Voir aussi `TEST-02`, l'exigence non testable |
| En-tête public et documentation | 2 | — | — | `CLAUDE.md` à aligner sur le code |
| Simplicité du code éducatif | 16 | — | — | Style, noms, invariants non écrits |
| Performance à la compilation | 7 | — | 2 | −64 % disponibles, chiffrés |
| Performance à l'exécution | 4 | — | 2 | Deux mauvais défauts, tout le reste sain |
| Couverture de tests | 6 | 1 | 2 | Deux exigences de `Task.md` non satisfaites |

Et la même population vue selon les axes de `Task.md` :

| Axe | Constats |
|---|--:|
| Robustesse | 9 |
| API facile à utiliser | 17 |
| Simplicité du code éducatif | 16 |
| Flexibilité | 7 |
| Performance à la compilation | 7 |
| Couverture de tests | 5 |
| Performance à l'exécution | 4 |
| Thread safety — pas de data race | 1 |

Un mot sur la ligne « thread safety ». Aucune **course de données** n'a été trouvée
dans les trois helpers ; le seul constat de cet axe (`COW-04`, mineur) note que
`copy_on_write::as_mutable` construit son *happens-before* sur `use_count()`, que la
norme déclare explicitement approximatif. L'autre moitié de l'exigence — « difficile
d'avoir des race conditions » — est en revanche prise en défaut par `SV-01` :
l'interblocage à deux verrous s'écrit sans le moindre diagnostic.

Hors périmètre comme demandé : `std::chrono`, `std::latch`, `std::barrier`,
`std::mutex`, `std::semaphore` n'ont pas été audités.

---

## Ce qui a été réfuté

16 constats ont été produits par un auditeur puis **détruits** par le contre-audit.
Ils sont listés ici parce qu'un chemin déjà exploré et fermé vaut la peine d'être
connu. Les plus instructifs :

- *« `launch_scoped_task` : use-after-free prouvé »* — les faits étaient exacts, la
  conclusion mal attribuée. Le `join` rend bien l'abandon de `is_lifetime_aware` sûr.
- *« `value_guard` laisse échapper une référence »* — la fuite existe mais elle est
  celle de tout `lock_guard` du standard ; les surcharges `&&` supprimées bloquent la
  forme réellement piégeuse.
- *« Les membres statiques sont invisibles au walk const »* — vrai, mais la correction
  proposée cassait du C++ légal et courant.
- *« Renommer `is_unsafe_<trait>` en `unsafe_assert_<trait>` »* — réfuté sur le fond
  après compilation ; le nom actuel est défendable.
- *« `copy_on_write` ne répond pas au cas lu-souvent/écrit-rarement »* — la cause
  annoncée était fausse.
- Deux « constats » de performance concluaient eux-mêmes à l'absence de problème (la
  barrière `acquire` d'`as_mutable` à 0,34 ns, le `push_back` de `threads_` à 3,5 ns)
  et ont été écartés comme non-constats. Leurs mesures restent valides et sont
  reprises plus haut : il n'y a rien à changer sur ces deux points.

Aucune de ces réfutations n'est reprise dans le document de détail — seuls les
constats ayant survécu y figurent.

---

## Ordre des travaux recommandé

**D'abord, ce qui ferme un trou** (petit, local, testé)

1. `SEND-01` — fusionner les branches de référence dans `diagnose_is_sendable`, et
   inverser les 4 assertions de test concernées. *2 lignes de code.*
2. `LIFE-01` — soit renommer `is_smart_pointer` en `is_unsafe_smart_pointer` et le
   documenter comme point d'octroi, soit le fermer et voucher les trois pointeurs
   standard un par un. Corrige aussi `LIFE-04`.
3. `SEND-02` — rendre les trois traits sûrs réellement clos, pour que `CLAUDE.md` dise
   vrai.

**Ensuite, ce qui rend l'exigence de `Task.md` vérifiable**

4. `TEST-02` — remplacer les surcharges de diagnostic par `= delete("raison")`, puis
   ajouter les tests de non-acceptation (écrits et vérifiés dans le détail).
   Attention : sous GCC 16 le test doit passer par une variable template dépendante.
5. `TEST-03` — ajouter une poignée de tests d'exécution pour les trois helpers.

**Puis le gain gratuit**

6. `PC-01` — deux lignes de CMake, −64 % de temps de build. À faire en premier si
   l'itération pendant la préparation de la conférence compte.
7. `PC-02` — scinder `threadsafe/traits.h` de l'en-tête complet.

**Puis les défauts d'usage visibles en conférence**

8. `PR-01` — `std::mutex` par défaut, partage en lecture en opt-in explicite.
9. `SV-01` — offrir un verrouillage de plusieurs valeurs, ou assumer et documenter
   l'absence, pour ne pas être pris de court sur la question.
10. `COW-01` et `COW-02` — deux corrections locales.
11. `PR-02` — purger `threads_`.

**Enfin, la lisibilité du support**

Les 16 constats de l'axe simplicité ne changent aucun comportement. Les trois qui
comptent le plus pour un public : l'**ordre des branches de `diagnose_is_sendable` est
un invariant de sûreté silencieux** (déplacer une ligne rend `int*` sendable, et rien
ne le dit), le préfixe `diagnose_` **ment** puisque ces fonctions ne diagnostiquent
rien, et `asynchronous_task_launcher.h` est le seul fichier du projet indenté à 4
espaces.

---

## Méthode et limites

**Méthode.** 13 auditeurs spécialisés — un par trait, un par helper, un par axe de
`Task.md` — ont travaillé en parallèle, chacun tenu de prouver ses affirmations en
compilant de vraies sondes avec `g++-16` plutôt qu'en raisonnant sur le code. Chaque
constat a ensuite été confié à un agent distinct chargé de le **réfuter** : vérifier
que le code cité existe bien à cette ligne, refaire la sonde, appliquer la correction
sur une copie des en-têtes et recompiler les 12 fichiers de tests. Les mesures de
performance ont été refaites par le vérificateur. Un dernier agent a cherché ce que
l'audit avait manqué.

108 constats produits, 16 réfutés, 92 retenus, ramenés à 66 après fusion des doublons
— plusieurs défauts ayant été trouvés indépendamment par deux ou trois auditeurs, ce
qui est noté dans le détail et renforce leur crédit.

Environnement : `g++-16 (Homebrew GCC 16.2.0)`, `-std=c++26 -freflection`, Apple
Silicon. La suite de tests compile proprement en 8,4 s (12 unités de traduction,
`static_assert` uniquement) avant comme après l'audit : aucun constat de ce rapport ne
provient d'un test en échec, et aucun fichier du dépôt n'a été modifié — les agents
ont travaillé sur des copies.

**Limites.**

- Les mesures de performance à l'exécution et à la compilation valent **pour cette
  machine et ce compilateur**. `std::shared_mutex` y est un `pthread_rwlock_t` ; le
  rapport 163× de `PR-01` serait différent sous libc++ ou sous Linux. La direction
  (mutex exclusif par défaut) reste juste, l'amplitude est à reprendre ailleurs.
- L'audit couvre le code tel qu'il est. Il ne juge pas les choix de périmètre — pas de
  canal, pas de `future`, pas de pool de threads — sauf lorsque l'absence rend un
  discours de conférence bancal, auquel cas c'est dit explicitement.
- Les constats marqués *DÉTAIL* incluent des **mesures négatives** : des points
  vérifiés qui ne doivent **pas** être changés. Ils sont dans le rapport pour éviter
  qu'un futur lecteur ne « corrige » ce qui est déjà juste.
