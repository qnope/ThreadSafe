# Performance à l'exécution

Résumé : **les traits ne coûtent littéralement rien à l'exécution — c'est vérifié
au niveau du code machine — et les helpers non plus.** Une seule décision de
conception coûte cher, et elle est prise automatiquement à la place de
l'utilisateur : le choix du mutex dans `synchronized_value`.

Toutes les mesures sont des exécutions réelles à `-O2` sur Apple M3 Pro (6P+6E),
GCC 16.2.0, décrites en [09-methodologie.md](./09-methodologie.md).

## Zéro surcoût, prouvé au code machine

C'est l'affirmation sur laquelle repose toute la bibliothèque, et elle tient sans
réserve. En compilant dans la **même** unité de traduction
`launcher.launch_task(worker, index, weight)` et
`threads_.emplace_back(worker, index, weight)` :

```
SAME-TU  run_traits insns:      161   run_plain insns:      161
=== opcode histogram diff ===
OPCODE HISTOGRAMS IDENTICAL
=== byte-identical after folding local label numbers? ===
IDENTICAL: run_traits and run_plain emit the same 161 instructions in the same order.

md5 traits body: f1616110ed0407f8d9618413ed095492
md5 plain  body: f1616110ed0407f8d9618413ed095492
```

**161 instructions contre 161, même ordre, même MD5.** Les concepts, la machinerie
`consteval` des `assert_*` et la surcharge de repli à `std::meta::exception` ne
laissent **aucun résidu**. En unités de traduction séparées, la section `__text`
de la version avec traits fait même 864 octets contre 868 — 4 octets de *moins*.

`nm -a` sur l'objet ne trouve qu'un seul symbole `threadsafe`, et c'est la
signature de la sonde elle-même.

## Les helpers sont gratuits

| opération | ThreadSafe | équivalent manuel | écart |
|---|---|---|---|
| `synchronized_value<int>::lock()` | 6,18 ns | `shared_mutex` + `unique_lock` : 6,19 ns | nul |
| `synchronized_value<Memo>::lock()` | 4,63 ns | `mutex` + `lock_guard` : 4,65 ns | nul |
| `launch_task(f, args...)` | 12,6 µs | `std::jthread{f, args...}` : 13,0 µs | nul |

Le premier chiffre a été **reproduit indépendamment**, avec un banc écrit
séparément (20 M itérations, médiane de 5 séries) :

```
synchronized_value<int>::lock()  5.82 ns/op
hand-written shared_mutex        5.83 ns/op
difference                       -0.01 ns  (-0.2%)
```

Les valeurs absolues varient avec la charge de la machine ; c'est **l'écart** qui
compte, et il est identique : 0,01 ns.

### `value_guard` : 24 octets, mais l'indirection disparaît

`sizeof(value_guard)` vaut 24 contre 16 pour le `unique_lock`/`shared_lock` nu —
+50 %, à cause du `T*` stocké. Mais ce pointeur **n'atteint jamais le code
généré** : SROA le dissout dans toutes les formes testées, y compris lorsque le
guard reste vivant à travers deux appels opaques.

```
__Z18read_shared_traits : 19 instructions
__Z17read_shared_plain  : 19 instructions
=== read_shared_traits vs read_shared_plain ===
IDENTICAL
```

Dans le corps généré, `ldr x0, [x19, 200]` lit le pointeur du vecteur à un
décalage **statique** depuis la base du `synchronized_value` : aucun `T*` n'est
stocké, chargé ni sauvegardé.

`[[nodiscard]]` et les `= delete("…")` sont également absents de l'assembleur.

## `copy_on_write` fait ce que le design promet

| charge | `copy_on_write<T>` | `shared_ptr<const T>` | membre simple |
|---|---|---|---|
| 20 M lectures | 12,0 ms | 11,1 ms | 8,3 ms |
| 2 M écritures non partagées | 1,1 ms | — | 0,5 ms |

| charge | `copy_on_write<T>` | copie de `shared_ptr` | copie profonde du `T` |
|---|---|---|---|
| 5 M partages de handle | 24,1 ms | 18,8 ms | 56 ns par copie réelle |

Partager un handle coûte ~4,8 ns contre ~56 ns pour copier l'objet : le rapport
~12× est exactement ce qui justifie le type. La barrière acquire coûte
0,29 → 0,69 ns par appel.

---

## Le point qui fâche : le `shared_mutex` automatique est un piège

`synchronized_value<T>::get_mutex_type()` choisit `std::shared_mutex` dès que
`is_synchronizable_v<const T>` — c'est-à-dire pour `int`, `double`, un agrégat
ordinaire, `vector`, `string`, `map` : **pratiquement tout ce qu'un utilisateur
va envelopper**. Or un `shared_mutex` est nettement plus lent qu'un `std::mutex`
quand la section critique est courte.

Mesuré, puis **reproduit indépendamment** avec un banc écrit séparément. Section
critique courte (somme de 64 `int`, ~40 ns) ; `speedup` = `mutex_ns / shared_ns`,
donc **en dessous de 1,00× le `shared_mutex` choisi automatiquement perd** :

| threads | % lectures | `shared_mutex` | `mutex` | speedup |
|---|---|---|---|---|
| 2 | 0 % | 1 644,8 ns | 7,2 ns | **0,00×** |
| 2 | 50 % | 873,4 ns | 21,5 ns | 0,02× |
| 2 | 90 % | 202,7 ns | 25,2 ns | 0,12× |
| 2 | 99 % | 44,0 ns | 26,5 ns | 0,60× |
| 4 | 90 % | 229,2 ns | 34,5 ns | 0,15× |
| 8 | 90 % | 570,5 ns | 52,8 ns | 0,09× |
| 12 | 90 % | 624,0 ns | 58,9 ns | 0,09× |
| 12 | 99 % | 502,9 ns | 61,1 ns | 0,12× |

**Avec une section critique courte, le `shared_mutex` ne gagne jamais** — à aucun
nombre de threads, à aucun taux de lecture, y compris 99 %.

Il finit par gagner, mais il faut une section critique **longue** (somme de
1 024 `int`, ~250-500 ns) *et* 99 % de lectures :

| threads | % lectures | `shared_mutex` | `mutex` | speedup |
|---|---|---|---|---|
| 2 | 90 % | 515,8 ns | 250,8 ns | 0,49× |
| 2 | 99 % | 282,9 ns | 422,1 ns | **1,49×** |
| 4 | 99 % | 155,6 ns | 548,3 ns | **3,52×** |
| 8 | 99 % | 300,3 ns | 511,6 ns | **1,70×** |
| 12 | 99 % | 434,3 ns | 519,3 ns | **1,20×** |

En balayant la longueur de la section critique à **100 % de lectures**, le point
de bascule se situe vers 256 `int`, soit ~100 ns sous le verrou :

| N `int` lus sous le verrou | `shared_mutex` | `mutex` | speedup |
|---|---|---|---|
| 16 | 97,1 ns | 24,1 ns | 0,25× |
| 64 | 58,6 ns | 37,6 ns | 0,64× |
| **256** | 102,5 ns | 135,8 ns | **1,32×** |
| 1 024 | 81,5 ns | 956,9 ns | 11,73× |
| 65 536 | 4 636,9 ns | 18 786,9 ns | 4,05× |

### Ce que cela veut dire

La sélection automatique optimise pour un cas — lectures longues et très
majoritaires — et **pénalise lourdement le cas courant**, une petite valeur lue et
écrite rapidement. Et l'utilisateur n'a **aucun moyen de la contredire** : le type
n'expose ni paramètre de politique ni point de personnalisation.

Il faut être juste sur un point : la sélection est **sûre** dans la direction qui
compte. Un `shared_mutex` n'est jamais choisi pour un `T` dont les lectures
`const` seraient dangereuses ; ajouter un membre `mutable` bascule immédiatement
vers le mutex exclusif. Ce n'est donc pas un défaut de correction, c'est un
défaut de performance — mais il est silencieux, et pour une bibliothèque
pédagogique, un défaut silencieux enseigne quelque chose de faux.

### Correctif — un second paramètre optionnel

Le correctif proposé et vérifié ajoute un paramètre de template qui vaut `void`
par défaut, c'est-à-dire « laisse les traits deviner » ; **tous les usages
existants compilent sans changement, et les onze TU de la suite passent**.
`synchronized_value<int, std::mutex>` devient exprimable.

Nommer `std::mutex` sur un `T` par ailleurs `const`-synchronisable reste **sûr** :
`get_const_guard_type()` retombe alors sur un `unique_lock`, strictement plus
fort que le `shared_lock` qu'il remplace.

Le code complet du remplacement de `synchronized_value.h` figure dans les
résultats de sonde ; l'essentiel tient en trois points : le paramètre
`class Mutex = void`, un `get_mutex_type()` qui rend `^^Mutex` quand il n'est pas
`void`, et la mise à jour de la déclaration `friend` de `value_guard` ainsi que
des deux spécialisations de traits pour le paramètre supplémentaire.

**Recommandation.** Pour une bibliothèque de conférence, l'ajout d'un paramètre au
type le plus utilisé a un coût pédagogique réel. Deux options défendables :
ajouter le paramètre (l'utilisateur garde le contrôle), ou **garder la sélection
automatique et l'énoncer à voix haute**, chiffres à l'appui — « nous choisissons
un `shared_mutex` dès que la lecture partagée est sûre ; sachez que sur une
section critique courte cela coûte un facteur 10, et voici la mesure ». La
seconde option est probablement la meilleure ici : elle transforme un piège
silencieux en un enseignement, ce qui est exactement l'objet du projet.

---

## Ce qui ne pose pas de problème

`asynchronous_task_launcher::threads_` est un `std::vector<std::jthread>` qui ne
fait que croître, joint au destructeur. Une réallocation déplace des `jthread`
(move-only, un handle chacun), pas des threads ; le coût est celui d'un `vector`
ordinaire, invisible devant les 12,6 µs de création d'un thread.
