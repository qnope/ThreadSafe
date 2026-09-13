# ThreadSafe — le lore de la présentation

Narration pour Meeting C++. Ce document ne décrit pas *ce que fait* la bibliothèque
(`CLAUDE.md` et `docs/audit-synthese.md` s'en chargent) : il décrit **l'histoire qu'on
raconte avec**, dans l'ordre où on la raconte.

La colonne vertébrale est celle des notes de l'auteur : on part d'une API qui refuse du
code, pas d'un catalogue de traits. Les traits arrivent comme **la réponse à une question
que la salle s'est déjà posée**.

## La thèse

> Je ne suis pas expert en multithreading. C'est précisément le sujet :
> personne ne devrait avoir à l'être pour que son code soit correct.

Ce n'est pas un avertissement d'humilité, c'est la thèse. Dans du logiciel legacy, on
introduit des threads pour trois raisons — **prendre un risque minimal**, **supprimer les
freezes d'UI**, **gagner en performance** — et la première est la plus importante des trois.
Un expert peut relire. Un expert ne peut pas relire chaque merge, pendant cinq ans.

Le compilateur, si.

**Titres candidats (slides en anglais)**
- *I Am Not a Threading Expert — and Neither Is Your Compiler, Yet*
- *Let the Compiler Check Your Threads — Send and Sync in C++26*
- *It Compiles — Thread Safety as a Property of Your Types*

## ⚠ À trancher avant d'écrire les slides

Trois points bloquants, vérifiés dans le code et les tests.

### 1. Les lambdas capturantes sont toutes refusées

```cpp
static_assert(!is_sendable_v<decltype([x = 42] {})>);       // tests/test_sendable.cpp:262
static_assert(!launchable_task<decltype([x = 42] {})>);     // test_asynchronous_task_launcher.cpp:50
```

GCC ne réfléchit **aucun membre** pour un type closure : `has_unreflectable_state` la
rejette, quelle que soit la capture. Même `[x = 42]`, trivialement sûre.

**Conséquence sur le plan initial** : « les deux fonctions prennent juste une fonction »
implique que les captures portent les données — donc *tout* est rouge, y compris les
corrections. Le talk n'aurait jamais de vert.

**La forme qui marche** : les données voyagent en **arguments**, les lambdas restent sans
capture. C'est la signature réelle de la bibliothèque et c'est la forme de tous les tests :

```cpp
launcher.launch_task([](std::shared_ptr<Counter> c) { ... }, counter);   // vert
launcher.launch_task([](Counter& c) { ... }, std::ref(counter));         // rouge
```

Bonus : ça rend le refus *lisible*. Le message parle de l'argument fautif, pas d'une
closure opaque. Et la limite closure devient un beat honnête de l'Acte X, pas un mur.

### 2. `launch_and_wait` est aujourd'hui séquentiel

`launch_scoped_task` construit un `jthread` et le `join()` immédiatement :
deux tâches de 300 ms prennent 604 ms (mesuré, audit §5). Si `launch_and_wait` est sur la
slide 1, quelqu'un dans la salle chronométrera. À corriger avant la conf — **et attention**,
le `task_scope` naïf proposé par l'audit introduit une faille d'ordre de destruction.

C'est pourtant ce `join()` immédiat qui rend l'emprunt par `std::ref` légitime : l'invariant
est bon, c'est l'implémentation qui ne le réalise qu'à une tâche. Il faut lancer toutes les
tâches, puis toutes les joindre.

### 3. `Sync = Send&` est le bon slogan, mais pas le sens du code

La bibliothèque calcule `is_sendable<T&> == is_synchronizable<T>` : `synchronizable` est le
walk primitif, `sendable` lui délègue. Sur scène, présenter l'équation comme une
**définition** dans l'autre sens (« Sync, c'est pouvoir envoyer une référence ») est
pédagogiquement supérieur — Send est intuitif, Sync ne l'est pas. Il suffit de ne pas
laisser croire que c'est l'ordre d'implémentation.

---

## Acte I — Deux fonctions, deux refus

On ouvre sur l'API, pas sur un data race. Une structure, deux fonctions :

```cpp
task_launcher launcher;
launcher.launch(f, args...);            // je n'attends pas
launcher.launch_and_wait(f, args...);   // j'attends
```

Puis **un cas qui ne compile pas pour chacune**. Ce sont les deux seules erreurs du talk :

```cpp
int counter = 0;
launcher.launch_and_wait([](int& c) { ++c; }, std::ref(counter));  // ✗ data race
launcher.launch([](int& c) { ++c; }, std::ref(counter));           // ✗ dangling
```

Le premier échoue parce que deux tâches écrivent le même `int` **en même temps**.
Le second parce que `counter` meurt **avant** la tâche.

Sentiment visé : **deux bugs, deux natures.** La salle sent déjà qu'il y a deux axes, sans
qu'on ait prononcé un seul nom de trait.

## Acte II — Ce sont les deux seules erreurs qu'on fait

L'échelle classique, et son intérêt : les quatre marches ne sont que deux problèmes.

| Bug | Correction réflexe | Question sous-jacente |
|---|---|---|
| `int` partagé, incrémenté par deux threads | `std::atomic<int>` | *peut-on **partager** ce type ?* |
| référence qui survit à son objet | `std::shared_ptr` | *ce type **possède**-t-il ce qu'il désigne ?* |

Tout le monde connaît les deux corrections. Personne ne les applique systématiquement,
**parce que rien ne les réclame**. La bibliothèque est exactement ça : le truc qui les
réclame.

## Acte III — Les contraintes

On réécrit les deux signatures. C'est le pivot du talk : le code passe de « prend n'importe
quoi » à « prend ce qu'il peut prouver ».

```cpp
template <class F, class... Args>
    requires launchable_task<F, Args...>
void launch(F f, Args... args);

template <class F, class... Args>
    requires launchable_scoped_task<F, Args...>
void launch_and_wait(F f, Args... args);
```

Et trois notions apparaissent, dans cet ordre — c'est l'ordre de la douleur, pas l'ordre
logique :

1. **un callable thread-safe** — la fonction elle-même traverse ;
2. **une variable thread-safe** — ce qu'elle touche est partageable ;
3. **un type lifetime-aware** — ce qu'elle touche est encore vivant.

## Acte IV — Trois questions, et une équation

| Question | Trait | Rust |
|---|---|---|
| Puis-je **envoyer** ce `T` d'un thread à un autre ? | `is_sendable<T>` | `Send` |
| Puis-je l'utiliser **depuis plusieurs threads à la fois** ? | `is_synchronizable<T>` | `Sync` |
| Ce `T` **possède**-t-il ce qu'il désigne ? | `is_lifetime_aware<T>` | *(le borrow checker)* |

Puis l'équation, en grand, seule sur sa slide :

> **Sync = Send&**
>
> Un type est partageable quand on peut envoyer une référence vers lui.

C'est la slide qui fait le plus de travail du talk. Envoyer une référence *est* un partage :
la moitié des bugs de l'Acte I tombe avec cette seule ligne.

Et le troisième trait est le moment d'honnêteté : **Rust n'en a pas besoin, son borrow
checker le fait en continu ; nous n'en avons pas.** Alors on remplace un vérificateur
permanent par une question posée une fois, à la frontière du thread. On reproduit deux
traits et demi, et on le dit.

### Les tables de référence

**Lifetime aware — « est-ce que je possède ? »**

| Type | | Pourquoi |
|---|---|---|
| `std::vector<T>`, `std::string` | ✅ | le buffer leur appartient |
| `unique_ptr` / `shared_ptr` / `weak_ptr` | ✅ | c'est leur métier (si le pointé l'est aussi) |
| `T*` | ❌ | un pointeur ne prolonge rien |
| `T&` | ❌ | idem |
| `std::reference_wrapper<T>` | ❌ | une référence avec de meilleures manières reste une référence |
| `std::string_view`, `std::span` | ❌ | `borrowed_range` : emprunter est écrit dans le nom |

**Sendable — « est-ce que ça peut traverser ? »**

| Type | Condition |
|---|---|
| `T` | `sendable<T>` (récursivement, membre par membre) |
| `T&`, `T*` | `synchronizable<T>` — *c'est l'équation* |
| `unique_ptr<T>` | `sendable<T>` : possession exclusive, l'ancien thread n'a plus rien |
| `vector<T>` | `sendable<T>` : le conteneur transporte, il ne partage pas |
| `shared_ptr<T>`, `weak_ptr<T>` | `synchronizable<T>` : *shared* veut dire ce qu'il veut dire |

La ligne `shared_ptr` mérite dix secondes de silence. Tout le monde l'utilise comme
« le pointeur qui règle les problèmes de thread ». Il règle la durée de vie. Il **crée** le
partage.

**Le piège à montrer** : `unique_ptr<PolyFinal>` est accepté, `unique_ptr<PolyBase>` refusé.
Derrière une base polymorphe non `final`, le type dynamique est inconnu : on ne peut rien
prouver sur un objet qu'on ne sait pas nommer.

## Acte V — Bloquant ou non bloquant : la charnière

C'est la meilleure idée du plan, et elle mérite d'être la charnière du talk. Les deux
fonctions de l'Acte I n'ont **pas les mêmes contraintes**, et la différence est exactement
un trait :

|  | doit être Send | doit être lifetime-aware |
|---|---|---|
| `launch_and_wait` (bloquant) | ✅ | — |
| `launch` (non bloquant) | ✅ | ✅ |

> **Bloquant : tu peux emprunter. Non bloquant : tu dois posséder.**

Parce que si j'attends la tâche, je garantis moi-même que l'objet survit ; si je n'attends
pas, plus personne ne le garantit. Le trait de durée de vie n'est pas une décoration :
c'est **le prix exact de ne pas attendre**.

La démo, deux lignes qui ne diffèrent que par le nom de la fonction :

```cpp
launcher.launch_and_wait([](Counter& c) { ... }, std::ref(counter));  // ✓ compile
launcher.launch          ([](Counter& c) { ... }, std::ref(counter));  // ✗ refusé
```

(`tests/test_asynchronous_task_launcher.cpp:41` et `:34` — le test existe déjà.)

Sentiment visé : **ça clique.** C'est le moment où les trois traits cessent d'être une liste
et deviennent un système.

## Acte VI — `const` est un mensonge

Le pic émotionnel. Un seul type au tableau :

```cpp
struct Widget { int* counter_; };
const Widget w;   // ça compile
```

`w` est `const`. La salle est prête à le partager entre threads. `*w.counter_` est
modifiable par n'importe qui.

D'où la règle qui traverse toute la bibliothèque, écrite en grand :

> **Le `const` derrière une indirection n'est jamais cru.**

La salle remporte cette phrase chez elle même si elle n'écrit jamais une ligne de réflexion.
C'est la meilleure ligne du talk.

## Acte VII — Satisfaire les contraintes

Le reste du talk, et la promesse tenue : on a un compilateur qui refuse, il faut maintenant
lui donner de quoi accepter. Un outil par ligne du tableau de l'Acte II.

- **`std::atomic<T>`** — la réponse minimale au data race. Et la première `is_unsafe_*` :
  aucun walk ne prouve qu'un `atomic` est partageable, c'est un fait d'ingénierie.
- **`synchronized_value<T>`** — la donnée et son verrou dans le même objet, impossibles à
  dissocier. Et le détail qui fait sourire la salle : `shared_mutex` si `const T` est
  synchronizable, `mutex` sinon. **Le type system choisit le verrou.**
- **`shared_ptr<T>`** — la réponse au dangling, à condition d'assumer ce qu'il partage.
- **`copy_on_write<T>`** — partagé en lecture, copié à la première écriture. La sortie quand
  `T` n'est pas partageable et qu'on ne veut pas d'un verrou.

Et la leçon de design qui dépasse le sujet : le `static_assert(sendable<T>)` de
`synchronized_value` est dans le **constructeur**, pas dans le corps de la classe. Poser un
trait sur `X<T>` *complète* `X<T>` ; un invariant écrit dans le corps rendrait le type
inquestionnable. Les invariants d'usage vont dans les constructeurs.

## Acte VIII — Comment ça marche, pour ceux qui veulent

`diagnose_is_sendable` tient sur une slide. C'est tout le moteur, et il n'y a rien à cacher :
une suite de `if` qui se lit à voix haute.

1. **`_v` est la mémoire.** Une variable template n'est instanciée qu'une fois par `T` : le
   walk ne tourne qu'une fois par unité de traduction, quel que soit le nombre de questions.
   (Un DAG de profondeur 60 — 2⁶⁰ chemins naïfs — compile au temps de base.)
2. **La récursion passe par `_v`, jamais par la fonction.** Une spécialisation écrite dans
   le `.cpp` de l'utilisateur, bien après la bibliothèque, est quand même atteinte.
3. **Le walk est conservateur : tout ce qu'il ne peut pas prouver est un non.** État non
   réflectable, vue empruntante, copie écrite à la main, constructeur template qui pourrait
   détourner la copie — refusés avant même de regarder les membres.

> **Un « non » n'a jamais besoin d'être affirmé. Seule la confiance doit l'être.**

D'où l'unique point de personnalisation, `is_unsafe_<trait>` : les traits sûrs sont
**fermés**, une spécialisation ne peut que *donner* sa confiance, jamais forcer un refus
(`false` et « rien affirmé » sont la même chose). Le mot `unsafe` apparaît exactement là où
une connaissance remplace une preuve : `grep unsafe` donne la liste complète de ce qu'il
faut croire sur parole.

Le clou, sans commentaire, les deux côte à côte :

```cpp
template <class T> struct is_unsafe_synchronizable<synchronized_value<T>>
    : std::bool_constant<is_sendable_v<T>> {};
```
```rust
unsafe impl<T: Send> Sync for Mutex<T> {}
```

## Acte IX — Le procès

Audit adversarial : unions anonymes, bitfields, `[[no_unique_address]]`, héritage en
diamant, types polymorphes, `mutable`, shallow-const à tous les étages, closures, vues
empruntantes, deleters malveillants, `atomic<shared_ptr<T>>`. 141 sondes compilées, chaque
constat soumis à un vérificateur chargé de le **réfuter**.

Résultat : **aucun type non thread-safe n'est accepté par le walk.**

Et la seule faille trouvée est la meilleure diapositive du talk : `const unique_ptr<T, D>`
était vouché **pour n'importe quel deleter** — or un deleter no-op, c'est exactement
l'idiome du pointeur observateur qui ne possède rien.

> La machine avait raison partout. C'est l'humain, dans la couche où il affirme au lieu de
> prouver, qui s'est trompé.

Justification rétrospective de tout l'Acte VIII : on isole le mot `unsafe` parce que c'est là
que les bugs vivent.

## Acte X — Ce que ça n'attrape pas

On finit par les limites. Une audience de Meeting C++ pardonne une limite annoncée et ne
pardonne pas une limite découverte.

- **Les lambdas capturantes sont toutes refusées** — GCC ne réfléchit pas les captures.
  C'est la limite la plus visible, et c'est celle de l'outil, pas du modèle : le jour où la
  réflexion voit les captures, le walk marche sans changer une ligne.
- **Pas de borrow checker.** La `T&` de `as_mutable()` n'est exclusive qu'à l'instant de
  l'appel : copier le handle juste après re-partage le bloc. Chaque étape est légitime
  isolément. C'est ce que `Arc::make_mut` ferme côté Rust, et qu'on ne peut pas fermer ici.
- **Les deadlocks ne sont pas détectés** — parité exacte avec Rust, à dire explicitement.
- **Les race conditions logiques restent possibles** :
  `if (sv.lock_shared()->empty()) sv.lock()->push_back(x);` compile.
- **Des faux négatifs assumés** : une `struct` d'atomics, un type possédant récursif, un
  constructeur forwarding.

> Un faux négatif coûte un `static_assert`. Un faux positif coûte un data race en production.
> On sait lequel des deux on veut.

**Pour aller plus loin** — `define_static_array` / `define_static_string` pour construire des
messages d'erreur qui **nomment le membre fautif et le trait qui a échoué**. C'est le premier
manque relevé par l'audit côté API : aujourd'hui un refus dit *que*, jamais *pourquoi ici*.
Le cas le plus rentable est le constructeur forwarding, faux négatif le plus probable en
usage réel et jamais nommé par le diagnostic.

## La dernière slide

Revenir aux deux lignes de l'Acte I. Les deux ne compilent plus. Les deux messages
expliquent pourquoi, et ils ne parlent pas de threads : ils parlent de possession et de
partage.

> Je ne suis toujours pas expert en multithreading.
> Je n'en ai plus besoin.

---

## Rythme et fils rouges

**Répliques récurrentes** — « Ça compile. » (Actes I, VI, IX) · « Bloquant : tu peux
emprunter. Non bloquant : tu dois posséder. » · « Le `const` derrière une indirection n'est
jamais cru. »

**Découpage indicatif (60 min)** — I-II : 8 min · III-IV : 12 min · V : 6 min · VI : 5 min ·
VII : 10 min · VIII : 10 min · IX : 4 min · X + fin : 5 min.

**Formats courts** — en 30 min : I, IV, V, VII, X. En lightning : l'Acte V seul (bloquant vs
non bloquant), il se suffit et c'est l'idée la plus neuve.

**À ne pas faire** — ouvrir sur la réflexion (c'est l'outil, pas le sujet) ; opposer C++ et
Rust (on emprunte une idée, on ne fait pas un match) ; présenter les traits avant le
launcher qui les réclame ; cacher l'Acte X pour finir sur un applaudissement.
