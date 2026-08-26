# 02 — Robustesse des helpers

**Verdict.** Les trois helpers ne sont pas au même niveau. `copy_on_write<T>` a un cœur
concurrent *juste* — la fence acquire de `as_mutable()` est correctement appariée, et
l'invariant « deux threads ne peuvent pas observer `use_count() == 1` en même temps » a
résisté à 700 000 appels — mais il est adossé à une question structurelle qui ment
(`is_synchronizable<const T>`), il rend une `T&` qui se retarge ou dangle, et il n'a
aucun canal de publication. `synchronized_value<T>` a un corps sans course (TSan est
muet dessus) et une sélection de mutex *sûre au sens strict*, mais cette sélection lit
la même question structurelle : elle peut choisir un `std::shared_mutex` pour un `T` qui
court vraiment, et l'API qu'elle expose pousse l'utilisateur vers le check-then-act et
n'offre aucun moyen de verrouiller deux valeurs ensemble. `asynchronous_task_launcher`
est celui qui doit répondre à une exigence binaire, et la réponse est **non, il accepte
encore des types unsafe** — pour une raison qui n'est pas un bug de règle mais la limite
inhérente déclaration-vs-corps décrite dans [01](./01-robustesse-des-traits.md).

18 findings, **tous les 18** recompilés et rejoués par moi-même contre les en-têtes
**réels** — plus, en supplément, la vérification du mutant `use_count() != 1` → `false`
sur les 11 unités de traduction et du correctif COW-03 sur les mêmes 11. Les seuls
résultats que je reprends sans les avoir refaits sont ceux qui exigent ThreadSanitizer
(donc Apple clang, donc une extraction manuelle sans réflexion) : ils sont attribués à
l'agent qui les a produits partout où ils apparaissent. Toolchain : `g++-16` 16.2.0
(Homebrew), Apple M3 Pro, macOS 26.6.2. Rien sous `include/` ni `tests/` n'a été modifié ;
tous les correctifs essayés l'ont été sur des copies.

---

## Réponse directe : `asynchronous_task_launcher` accepte-t-il un type unsafe ?

**Oui.** Deux familles passent aujourd'hui, plus une troisième que le *lead* a vérifiée
personnellement. Aucune des trois n'est une règle fausse : dans les trois cas la
réflexion voit un type qui, *déclarativement*, est irréprochable — et l'état partagé est
nommé dans un **corps de fonction**, là où la réflexion ne va pas.

| Ce qui passe | Ce que la réflexion voit | Résultat à l'exécution | Fermable ? |
|---|---|---|---|
| Un foncteur dont le seul état mutable est un `static inline` (L1) | une classe **vide** : 0 base, 0 NSDM, aucun copy/move/destructeur écrit | `exit=134 / 133 / 134`, corruption du tas, ASan : *heap-use-after-free* | **Oui** — patch vérifié, 11/11 TU vertes |
| Une lambda **sans capture** qui atteint un `asynchronous_task_launcher` de portée namespace (L2) | une classe **vide** | `exit=138 / 133 / 133` (SIGBUS/SIGTRAP ; le *lead* a mesuré 139/133/139) | **Non** — et le patch L1 ne la ferme pas |
| Un handle *thread-affine* : un agrégat d'un seul `std::size_t` indexant un `thread_local` (vérifié par le lead) | un agrégat d'un entier | `SIGSEGV`, `exit=139` | **Non** |

Les deux dernières lignes sont la limite inhérente de [01](./01-robustesse-des-traits.md),
appliquée au lanceur : *la réflexion raisonne sur des déclarations, jamais sur des corps
de fonction*. La première ligne, en revanche, **est** fermable, parce qu'un membre de
donnée statique est une **déclaration** — et c'est le seul des trois trous qui mérite un
patch.

Deux autres défauts du lanceur sont des bugs de comportement, tous deux vérifiés par le
*lead* : `launch_scoped_task` **interblique** sur toute tâche coopérative (L3, `exit=42`),
et le destructeur **sérialise** l'arrêt (L5, 1020 ms au lieu de 255 ms). Ils sont traités
en section C.

Ce qui a **résisté**, et qui compte autant : `std::function<void()>` est refusé par les
deux portes ; une lambda capturant un `int*` **par valeur** est refusée ; `std::ref` d'un
type béni par `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` est refusé par `launch_task` ;
`std::span`, `std::string_view`, `const char*` et les pointeurs bruts sont tous refusés ;
un foncteur *sans état* mais avec un constructeur de copie **écrit à la main** est refusé ;
`synchronized_value` ne peut pas être envoyée par valeur, et `std::ref` d'une
`synchronized_value` est refusée avec un message exact ; le lanceur lui-même n'est ni
sendable, ni synchronizable, ni const-synchronizable. Sur les quatre sondes de rejet
attendues, **trois ont tenu** ; la quatrième est L1.

---

## Tableau des 18 findings

| # | Helper | Titre court | Sévérité | Fix proposé | Régression vérifiée |
|---|---|---|---|---|---|
| COW-01 | `copy_on_write` | la sendabilité repose sur `is_synchronizable<const T>`, et ici il n'y a **aucun** mutex | critique | aucun (celui qui marche casse la suite) | `no-fix-proposed` |
| COW-02 | `copy_on_write` | la `T&` de `as_mutable()` se retarge vers le snapshot, ou dangle | haute | aucun (changement d'API) | `no-fix-proposed` |
| COW-03 | `copy_on_write` | faux rejet : un `copy_on_write` ne peut pas contenir un `copy_on_write` | haute | 1 spécialisation, 3 lignes | `suite-passes` (revérifié ici) |
| SV-01 | `synchronized_value` | le `shared_mutex` est choisi sur une preuve structurelle qu'une méthode `const` peut démentir | critique | en-tête de remplacement complet | `suite-passes` |
| SV-02 | `synchronized_value` | check-then-act entre deux `lock()`, et aucun verrouillage multiple | haute | `with` / `with_all` | `suite-passes` |
| SV-03 | `synchronized_value` | les opérateurs rvalue supprimés ne ferment qu'une écriture littérale | haute | aucun qui ferme | `no-fix-proposed` |
| SV-04 | `synchronized_value` | le constructeur variadique glouton avale sa propre copie | moyenne | garde identique à `copy_on_write` | `suite-passes` |
| SV-05 | `synchronized_value` | l'amitié en bloc laisse forger un `value_guard` sur n'importe quoi | moyenne | amitié restreinte | `suite-passes` |
| SV-06 | `synchronized_value` | `synchronized_value<T&>` s'instancie et n'explose qu'au `lock()` | basse | 2 `static_assert` | `suite-passes` |
| SV-07 | `synchronized_value` | `[[nodiscard]]` attrape 2 fuites sur 4, et n'est jamais une erreur | basse | attribut sur le **type** | `suite-passes` |
| SV-08 | les deux | aucune construction par liste d'initialisation ; `{3, 0}` ment | basse | surcharge `initializer_list` | `suite-passes` |
| L1 | lanceur | un membre `static` dans le foncteur passe toutes les portes | critique | walk des membres statiques | `suite-passes` |
| L2 | lanceur | réentrance : course sur `threads_`, SIGSEGV | critique | aucun (hors de portée de la réflexion) | `no-fix-proposed` |
| L3 | lanceur | `launch_scoped_task` interblique sur une tâche coopérative | critique | `scoped_task_group` | `suite-passes` |
| L5 | lanceur | l'arrêt coûte la **somme** des temps de réaction, pas le **max** | haute | `request_stop` / `join_all` / futures | `suite-passes` |
| L7 | lanceur | la précondition documentée de `launch_scoped_task` n'est pas vérifiable | haute | aucun (documentation) | `no-fix-proposed` |
| L10 | lanceur | l'astuce à deux surcharges rend l'appel indétectable | moyenne | aucun (documentation) | `no-fix-proposed` |
| L11 | lanceur | un callable non-movable passé en lvalue court-circuite le beau message | moyenne | références universelles | `suite-passes` |

Périmètre du travail sous-jacent : 40 scénarios rejoués sur `copy_on_write` /
`synchronized_value` (38 reproduits, 2 impasses), 53 sur le lanceur (49 reproduits, 4
impasses).

---

## A. `copy_on_write<T>`

L'en-tête complet, tel qu'il est aujourd'hui, tient en quarante lignes :

```cpp
template <class T>
class copy_on_write {
public:
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_.get(); }

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};
```

### A.1 La fence acquire : la question, tranchée

C'est le point le plus subtil du type, celui qui mérite une diapositive, et il faut le
dire sans ambiguïté : **la fence est nécessaire, correctement appariée, et les rapports
TSan qui semblent la contredire sont des artefacts d'outil.**

Le commit qui l'a ajoutée (`643e3f5`) énonce l'intention exacte :

> The `use_count() == 1` check needs an acquire fence to synchronize with the release on
> another thread's last decrement before mutating in place.

L'intention est juste, et la preuve tient dans le code de libstdc++ que j'ai lu
directement (`/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/bits/shared_ptr_base.h`) :

- **ligne 231-235**, `_Sp_counted_base::_M_get_use_count()` — celui que
  `shared_ptr::use_count()` appelle — porte ce commentaire, textuel :

  ```cpp
  _M_get_use_count() const noexcept
  {
    // No memory barrier is used here so there is no synchronization
    // with other threads.
    auto __count = __atomic_load_n(&_M_use_count, __ATOMIC_RELAXED);
  ```

  La lecture est donc **relaxed**. Seule, elle n'établit aucun *happens-before*.

- **ligne 384**, `_M_release()` décrémente via `__exchange_and_add_dispatch`, qui est
  `__ATOMIC_ACQ_REL`. La partie *acquire* de cette RMW profite au thread **qui part**,
  et ne fait rien pour le thread **qui reste**.

Le survivant a donc besoin d'un acquire de son côté. Comme la lecture est relaxed, la
seule construction disponible est exactement `[atomics.fences]/4`, la synchronisation
*fence-atomic* : la lecture relaxed de `use_count()` est *sequenced before* la fence
acquire, et elle lit la valeur écrite par le décrément *release* de l'autre thread ; ce
décrément **synchronise avec** la fence. La mutation en place qui suit voit donc tout ce
que le thread partant avait écrit. Sans la fence, elle ne le voit pas. **La fence n'est
ni redondante ni un cache-misère : c'est la seule chose qui rend la branche « en place »
correcte.**

Pourquoi les mesures TSan semblent dire le contraire, et pourquoi il faut les écarter :
un scénario de *handoff* bâti sur une extraction fidèle d'`as_mutable()`, compilé une
fois avec la fence et une fois sans, produit **le même rapport de course dans les deux
modes**. Ce n'est pas une preuve que la fence est inutile : le contrôle du même corpus,
un *handoff* fence-fence textuellement conforme à `[atomics.fences]/2` et `/4`, est
**aussi** rapporté (`exit=134`, un avertissement dans chaque mode). Un contrôle
supplémentaire, isolé sur la forme exacte du compteur de références, tranche : *lecture
relaxed + fence acquire* → TSan rapporte ; *lecture acquire* avec exactement le même
*happens-before* → TSan est **muet**. Sur cette plateforme, ThreadSanitizer ne modélise
pas `std::atomic_thread_fence`. (TSan lui-même fonctionne : un `tsan_hello.cpp` avec une
vraie course est bien rapporté.) Ces trois contrôles sont l'œuvre de l'agent qui a
travaillé le helper ; les deux lignes de libstdc++ ci-dessus, elles, je les ai lues
moi-même.

**Deuxième moitié de la question, et elle a résisté :** *deux threads peuvent-ils
observer `use_count() == 1` en même temps ?* Non. Trois formes ont été martelées contre
l'en-tête **réel** — détachement simultané sur deux handles d'un même bloc ; *handoff* où
un lecteur lâche son handle 2→1 pendant qu'un écrivain tourne sur un drapeau **relaxed**,
de sorte que le compteur soit le seul candidat au *happens-before* ; et une mêlée à 4
handles / 4 threads — soit **700 000 appels à `as_mutable()`** sur 5 exécutions complètes,
la branche prise étant détectée par la stabilité d'adresse du `T` et non en touchant aux
membres privés. Résultat : environ 155 000 passages par la branche « en place »,
*« two threads in place at once : 0 »*, *« stale payload cells observed : 0 »*. Le
raisonnement qui l'explique est aussi propre que le chiffre : `use_count() == 1` signifie
qu'aucun autre handle n'existe, et un handle ne peut naître que de la copie d'un handle
vivant ; un second observateur exigerait donc un handle qu'on vient de prouver absent. Et
la cohérence écriture-lecture (`[intro.races]`) interdit la lecture périmée, parce que
tout handle détenu par un autre thread a été publié via une arête *happens-before* (la
création du thread) qui ordonne aussi l'incrément avant notre lecture.

**Une réserve, et elle est sérieuse.** Le test de mutation le montre : remplacer
`if (ptr_.use_count() != 1)` par `if (false)` — c'est-à-dire *`as_mutable()` ne détache
jamais* — **survit aux 11 unités de traduction de la suite**. Je l'ai revérifié
moi-même : les 11 TU compilent proprement contre l'en-tête muté. La discipline
copy-on-write, qui est l'unique argument de sûreté du type, n'est testée par rien. Voir
[03](./03-couverture-de-tests.md).

### A.2 COW-01 — la sendabilité repose sur une preuve structurelle, et ici il n'y a aucun mutex

`is_sendable<copy_on_write<T>>` se réduit à ceci :

```cpp
namespace detail {
template <class T>
consteval bool cow_is_sendable() {
    if constexpr (is_sendable_v<T>)
        return is_synchronizable_v<const T>;
    else
        return false;
}
}
```

`is_synchronizable<const T>` est une marche **structurelle** sur les membres de donnée
(`diagnose_default_is_const_synchronizable` dans `synchronizable.h`). Elle ne dit
strictement rien de ce qu'une **fonction membre `const`** écrit. C'est la même cause
racine que celle décrite dans [01](./01-robustesse-des-traits.md), et ici elle mord plus
fort que partout ailleurs : `synchronized_value` peut au moins *dégrader* vers un
`std::mutex`, alors que `copy_on_write` n'a **aucun** verrou à dégrader. La discipline
copy-on-write est l'unique protection — et elle ne se déclenche jamais, puisqu'aucun
lecteur n'appelle `as_mutable()`.

Programme complet :

```cpp
// La même tache aveugle, atteinte par copy_on_write au lieu de synchronized_value --
// et ici il n'y a aucun mutex sur lequel se rabattre.
//
// cow_is_sendable<T> = is_sendable<T> && is_synchronizable<const T>.  La marche
// structurelle voit un int dans HitCounter, donc `const HitCounter` est beni, donc
// copy_on_write<HitCounter> est sendable et launch_task l'accepte. Chaque worker
// appelle ensuite une fonction membre const qui ecrit un static local de fonction,
// a travers un const& non synchronise.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread cow_hitcounter.cpp -o cow_hc && ./cow_hc
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

struct HitCounter {
    int seed;
    int next() const {
        static int calls = 0;
        return seed + (++calls);
    }
};

using shared_counter = threadsafe::copy_on_write<HitCounter>;

static_assert(threadsafe::is_synchronizable_v<const HitCounter>,
              "the structural walk sees one int and blesses the const form");
static_assert(threadsafe::is_sendable_v<shared_counter>,
              "so the copy_on_write is sendable...");
static_assert(threadsafe::launchable_task<decltype([](shared_counter) {}),
                                          shared_counter>,
              "...and launch_task accepts it");

int main() {
    constexpr int worker_count = 8;
    constexpr int calls_per_worker = 20000;

    const shared_counter master(HitCounter{0});
    std::atomic<int> ready{0};
    std::atomic<int> highest_seen{0};

    {
        std::vector<std::jthread> workers;
        for (int index = 0; index < worker_count; ++index)
            workers.emplace_back([copy = master, &ready, &highest_seen] {
                ready.fetch_add(1);
                while (ready.load() < worker_count) {}
                int local_highest = 0;
                for (int call = 0; call < calls_per_worker; ++call)
                    local_highest = std::max(local_highest, copy->next());
                int previous = highest_seen.load();
                while (previous < local_highest
                       && !highest_seen.compare_exchange_weak(previous,
                                                              local_highest)) {}
            });
    }

    const int expected = worker_count * calls_per_worker;
    std::printf("expected highest ticket: %d\n", expected);
    std::printf("actual   highest ticket: %d\n", highest_seen.load());
    std::printf("increments lost to the race: %d\n",
                expected - highest_seen.load());
    return 0;
}
```

Le fichier compile **proprement** contre l'en-tête réel — les trois `static_assert`
tiennent, y compris `threadsafe::launchable_task`. À l'exécution :

```
expected highest ticket: 160000
actual   highest ticket: 25828
increments lost to the race: 134172
```

(mon exécution ; l'agent avait mesuré 26466 / 133534. Environ **83 %** des incréments
disparaissent, avec une variance normale entre exécutions sur une machine chargée.)

**Pourquoi c'est grave.** L'argument de sûreté de `copy_on_write`, tel qu'il est écrit
dans `tests/test_copy_on_write.cpp` — « les lecteurs ne voient jamais qu'un `const T`, et
un écrivain se détache avant de toucher un bloc partagé » — vaut *exactement* ce que vaut
`is_synchronizable<const T>`. La bibliothèque déclare cette configuration sûre **à la
compilation**, ce qui est pire que de ne rien déclarer.

**Fix : aucun de vérifié, et c'est un choix honnête.** Le fix évident — faire demander à
`cow_is_sendable` l'opt-in `shares_const_reads_v<T>` de SV-01 au lieu de
`is_synchronizable_v<const T>` — a été écrit puis **rejeté après mesure** : il casse la
suite de la bibliothèque elle-même. `tests/test_copy_on_write.cpp` affirme
`is_sendable_v<cow<SafeCounter>>` (où `SafeCounter` est un `mutable std::atomic<int> hits`,
authentiquement sûr en lecture const mais pas pleinement synchronizable, donc
`shares_const_reads` répond `false` par défaut) et `is_sendable_v<cow<Tagged<Cache>>>` ;
les deux deviendraient `false`. Exiger un opt-in explicite pour toute structure
utilisateur ordinaire viderait le helper de sa substance. **Le coût de fermer ce trou
dans `copy_on_write` est un faux rejet du cas courant** — c'est une décision de conception
pour l'auteur, pas un patch livrable.

*Ce que je recommande à la place, et c'est une suppression de prétention plutôt qu'une
fonctionnalité :* écrire la phrase dans l'en-tête, à côté de `cow_is_sendable`. Quelque
chose comme « `is_synchronizable<const T>` est une affirmation sur les **membres de
donnée** ; un `T` dont une fonction membre `const` écrit doit spécialiser le trait à
`false` lui-même ». Une phrase de documentation coûte zéro régression et dit la vérité.

### A.3 COW-02 — la `T&` de `as_mutable()` se retarge vers le snapshot, ou dangle

`as_mutable()` rend une `T&` dans le bloc **actuellement** possédé. Toute copie
ultérieure du handle fait détacher le prochain `as_mutable()` vers un bloc **neuf**,
laissant l'ancienne référence pointée sur le bloc que le **snapshot** possède désormais.
Écrire à travers cette référence mute le snapshot — c'est-à-dire l'exacte propriété que
le type est censé vendre.

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>
#include <vector>

using threadsafe::copy_on_write;

int main() {
    // ---- 1. the reference silently detaches from the handle ----
    copy_on_write<std::vector<int>> document(std::vector<int>{1, 2, 3});
    int& first_element = document.as_mutable()[0];

    copy_on_write<std::vector<int>> snapshot = document;

    document.as_mutable()[0] = 99;

    std::printf("document[0]        = %d   (expected 99)\n", (*document)[0]);
    std::printf("snapshot[0]        = %d   (expected 1)\n", (*snapshot)[0]);
    first_element = 4242;
    std::printf("after writing 4242 through the reference:\n");
    std::printf("document[0]        = %d\n", (*document)[0]);
    std::printf("snapshot[0]        = %d   <-- the SNAPSHOT was mutated\n",
                (*snapshot)[0]);

    // ---- 2. the reference dangles outright ----
    copy_on_write<std::string> text(std::string(64, 'a'));
    std::string& borrowed = text.as_mutable();
    text = copy_on_write<std::string>(std::string(64, 'b'));
    std::printf("borrowed.size()    = %zu   <-- read of a destroyed std::string\n",
                borrowed.size());
    return 0;
}
```

Sortie mesurée (`-g -O2 -pthread`, aucun avertissement) :

```
document[0]        = 99   (expected 99)
snapshot[0]        = 1   (expected 1)
after writing 4242 through the reference:
document[0]        = 99
snapshot[0]        = 4242   <-- the SNAPSHOT was mutated
borrowed.size()    = 0   <-- read of a destroyed std::string
```

Recompilé avec `-fsanitize=address` :

```
==82713==ERROR: AddressSanitizer: heap-use-after-free on address 0x604000000468
READ of size 8 at 0x604000000468 thread T0
    #0 in main r01_refinval.cpp:30
freed by thread T0 here:
    #1 std::_Sp_counted_ptr_inplace<std::string, ...>::_M_destroy()
    #2 in main r01_refinval.cpp:29
SUMMARY: AddressSanitizer: heap-use-after-free r01_refinval.cpp:30 in main
```

Le premier bloc est le point important : la référence obtenue **avant** que le snapshot
n'existe écrit **dans** ce snapshot après que l'écrivain se soit détaché. Le second est un
*heap-use-after-free* en code **monothread**, à `-O2`, sans le moindre diagnostic — il
provient de la réaffectation du handle, pas d'un `std::move` sur la source, et sort donc
du sujet explicitement exclu.

**Fix : aucun.** L'invalidation est **inhérente** à `T& as_mutable()`. La référence n'est
valide que jusqu'à la prochaine opération sur le handle, et C++ n'a aucun moyen de dire
cela dans un type de retour. La réponse complète serait un changement d'API de la même
forme que celui de SV-02 : remplacer `as_mutable()` par
`template <class Callable> decltype(auto) mutate(Callable&&)` qui détache, appelle le
callable avec un `T&`, et rend la main — de sorte qu'aucune référence ne survive à
l'appel. Ce n'est pas livré comme patch vérifié parce que cela **supprime** `as_mutable()`,
sur lequel `tests/test_copy_on_write.cpp` porte directement
(`std::same_as<decltype(std::declval<cow<int>&>().as_mutable()), int&>` et
`can_detach<cow<int>>`) : tout patch de cette forme casse la suite par construction, et
la forme du remplacement appartient à l'auteur.

*Au minimum*, la précondition doit figurer dans l'en-tête, à côté d'`as_mutable()`,
exactement comme `launch_scoped_task` documente déjà la sienne.

### A.4 COW-03 — un `copy_on_write` ne peut pas contenir un `copy_on_write`

`is_sendable<copy_on_write<T>>` est **énoncée** explicitement ; la question `const`, elle,
ne l'est pas. Elle retombe donc sur `diagnose_default_is_const_synchronizable`, qui refuse
tout type portant un **template de constructeur** (`may_hijack_copy_move`). Le
constructeur variadique de `copy_on_write` en est un — alors même qu'il est correctement
gardé contre le détournement de copie, garde que la réflexion ne sait pas lire. Et comme
`cow_is_sendable<T>` exige `is_synchronizable_v<const T>`, un `copy_on_write` de quoi que
ce soit contenant un `copy_on_write` n'est pas sendable.

```cpp
// Le faux rejet, dans la forme meme pour laquelle une bibliotheque copy-on-write
// existe : un document dont les paragraphes sont eux-memes copy-on-write.
//
// Compiler DEUX FOIS :
//   g++-16 -std=c++26 -freflection -I<include>       -fsyntax-only cow_nested.cpp
//   g++-16 -std=c++26 -freflection -I<include-fixed> -DEXPECT_FIXED -fsyntax-only cow_nested.cpp
#include <threadsafe/threadsafe.h>

#include <string>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

struct Paragraph {
    copy_on_write<std::string> text;
};

struct Document {
    std::vector<Paragraph> paragraphs;
};

static_assert(is_sendable_v<copy_on_write<std::string>>,
              "one level is fine");
static_assert(is_sendable_v<Paragraph>,
              "and a struct holding one is sendable");

#ifdef EXPECT_FIXED
static_assert(is_synchronizable_v<const copy_on_write<std::string>>);
static_assert(is_synchronizable_v<const Paragraph>);
static_assert(is_sendable_v<copy_on_write<copy_on_write<std::string>>>);
static_assert(is_sendable_v<copy_on_write<Document>>);
static_assert(threadsafe::launchable_task<
                  decltype([](copy_on_write<Document>) {}),
                  copy_on_write<Document>>);
#else
static_assert(!is_synchronizable_v<const copy_on_write<std::string>>,
              "TODAY: reading a const copy_on_write from N threads is refused");
static_assert(!is_synchronizable_v<const Paragraph>,
              "TODAY: and the refusal propagates to any struct holding one");
static_assert(!is_sendable_v<copy_on_write<copy_on_write<std::string>>>,
              "TODAY: a copy_on_write cannot be nested in a copy_on_write");
static_assert(!is_sendable_v<copy_on_write<Document>>,
              "TODAY: nor can a document made of copy-on-write paragraphs");
static_assert(!threadsafe::launchable_task<
                  decltype([](copy_on_write<Document>) {}),
                  copy_on_write<Document>>,
              "TODAY: so launch_task refuses the document");
#endif
int main() {}
```

Contre l'en-tête réel, le fichier compile : **les cinq refus sont réels**. Et le message
que l'utilisateur reçoit accuse un membre spécial que `copy_on_write` **n'a pas**, sans
jamais nommer le handle imbriqué. Diagnostic reproduit ici mot pour mot :

```
error: uncaught exception of type 'std::meta::exception'; 'what()':
  'threadsafe::copy_on_write<Document> has a user-written copy, move or
   destructor — or a template that may be selected as one — which can share
   state the members do not show; specialize is_sendable to state the intent'
```

**Fix, revérifié par moi-même :** une seule spécialisation, à ajouter dans
`include/threadsafe/details/copy_on_write.h`, immédiatement après la spécialisation
`is_sendable<copy_on_write<T>>` existante.

```cpp
// A const handle only ever hands out a `const T&`, and copying or destroying a
// handle touches nothing but the atomic use count — so several threads may read
// one const copy_on_write at once exactly when they may read one const T.
//
// Stated here because the structural walk cannot reach it: the variadic
// constructor is a constructor template, and may_hijack_copy_move blocks the
// default on any of those, so the walk answers "no" for the library's own type.
template <class T>
struct is_synchronizable<const copy_on_write<T>> : is_synchronizable<const T> {};
```

J'ai appliqué ce patch sur une copie de l'arbre `include/` et lancé
`g++-16 -std=c++26 -freflection -fsyntax-only` sur les **11** TU de `tests/` : aucune
régression, et le même `cow_nested.cpp` compilé avec `-DEXPECT_FIXED` valide les cinq
assertions inverses. La soundness est préservée dans le sens négatif :
`is_synchronizable_v<const copy_on_write<Cache>>` reste `false` parce que
`is_synchronizable_v<const Cache>` est `false` (`Cache` a un membre `mutable`), et la
récursion termine — `is_sendable<cow<cow<U>>>` se réduit à `is_synchronizable<const U>`,
jamais à elle-même.

**Mais il y a un prix mesuré, et il faut le dire.** Avec ce patch,
`synchronized_value<copy_on_write<std::string>>::mutex` passe de `std::mutex` à
`std::shared_mutex` — je l'ai vérifié dans les deux sens :

```cpp
#include <threadsafe/threadsafe.h>
#include <concepts>
#include <mutex>
#include <shared_mutex>
#include <string>
using threadsafe::copy_on_write;
using threadsafe::synchronized_value;
static_assert(std::same_as<synchronized_value<std::string>::mutex, std::shared_mutex>);
static_assert(std::same_as<synchronized_value<copy_on_write<std::string>>::mutex, std::mutex>);
int main() {}
```

compile contre l'en-tête réel, et échoue contre l'arbre patché avec
`'class std::shared_mutex' is not the same as 'class std::mutex'`. Or le mutex
« sémantiquement correct » est ici le **plus lent** : sur le scénario `p2_config.cpp`,
l'agent lanceur a mesuré **9× plus lent** (344 ns/lecture contre 40 ns) et a **révoqué**
son propre patch pour cette raison. Voir [07](./07-performance-execution.md). Les deux
constats sont vrais en même temps : COW-03 est un faux rejet réel, son correctif est
correct, et son correctif rend ce cas d'usage précis neuf fois plus lent parce que
`std::shared_mutex` est cher. **C'est un arbitrage à trancher, pas un patch à appliquer
les yeux fermés.**

### A.5 `copy_on_write` n'a aucun canal de publication — le point de soundness

`is_synchronizable_v<copy_on_write<T>>` est `false`, et c'est **correct** : `as_mutable()`
réaffecte `ptr_`, donc deux threads partageant un même handle courent sur le `shared_ptr`
lui-même, pas seulement sur le `T`. `tests/test_copy_on_write.cpp` le dit explicitement.
Par conséquent `is_sendable_v<copy_on_write<int>&>`, `<copy_on_write<int>*>` et
`<const copy_on_write<int>&>` sont tous `false`. J'ai vérifié :

```cpp
#include <threadsafe/threadsafe.h>
#include <string>
using threadsafe::copy_on_write;
static_assert(!threadsafe::is_synchronizable_v<copy_on_write<std::string>>);
static_assert(!threadsafe::is_sendable_v<copy_on_write<std::string>&>);
static_assert(!threadsafe::is_sendable_v<copy_on_write<std::string>*>);
static_assert(threadsafe::is_sendable_v<copy_on_write<std::string>>);
int main() {}
```

compile proprement. La conséquence est structurelle, et elle appartient à ce rapport et
non à celui sur l'ergonomie : **le type n'expose aucun moyen de publier une nouvelle
version.** Il n'a ni `store`, ni `load`, ni `exchange`, ni `compare_exchange`. Le seul
transport est la copie du handle par valeur — et une fois qu'un worker détient sa copie,
l'`as_mutable()` ultérieur du publieur se détache vers un bloc neuf que le worker ne
verra **jamais**. `copy_on_write` distribue des instantanés ; il ne peut pas distribuer
des instantanés *mis à jour*.

Le seul canal disponible est donc d'emballer le handle : `synchronized_value<copy_on_write<T>>`
est bien sendable (vérifié). Mais aujourd'hui, à cause de COW-03, cet emballage prend un
`std::mutex` exclusif, ce qui sérialise des lecteurs qui n'auraient jamais eu besoin de
l'être. Le type le plus « lecture concurrente » de la bibliothèque n'a donc pas de chemin
de lecture concurrente. C'est un manque de conception, pas un bug — mais un manque qui
change ce qu'on peut promettre depuis une scène. L'ergonomie de cette absence est traitée
dans [07](./07-performance-execution.md) et [08](./08-api-et-flexibilite.md).

### A.6 Ce qui a résisté sur `copy_on_write`

- La fence acquire est **portante et correctement appariée** (§A.1).
- Deux threads ne peuvent **jamais** observer `use_count() == 1` simultanément : 700 000
  appels, 0 violation, 0 lecture périmée (§A.1).
- `is_synchronizable_v<copy_on_write<T>>` est **laissé à `false` pour la bonne raison** —
  `as_mutable()` réaffecte `ptr_`.
- Le constructeur variadique **est** correctement gardé contre la construction par copie :
  `std::constructible_from<cow<int>, cow<int>&>` passe par le constructeur de copie, pas
  par le template, sur une lvalue non-const — précisément le cas contre lequel
  `may_hijack_copy_move` met en garde. C'est le garde que `synchronized_value` n'a pas
  (SV-04).
- Les réponses de durée de vie sont justes dans les deux sens : `is_lifetime_aware_v` tient
  pour `cow<std::string>` et `cow<std::vector<int>>`, et échoue pour `cow<int*>` et
  `cow<std::string_view>`.
- Les trois traits atteignent les spécialisations explicites de `copy_on_write` à travers
  les orthographes `const` et `volatile`.
- Sous TSan, des handles `copy_on_write` partagés entre threads sont **sans course**.

---

## B. `synchronized_value<T>`

### B.1 SV-01 — le `shared_mutex` est choisi sur une preuve que `const` peut démentir

**C'est le titre de cette section, et c'est la même cause racine que
[01](./01-robustesse-des-traits.md).** Le wrapper choisit son mutex ainsi :

```cpp
static consteval auto get_mutex_type() {
    if constexpr (is_synchronizable_v<const T>) {
        return ^^std::shared_mutex;
    } else {
        return ^^std::mutex;
    }
}
```

`is_synchronizable<const T>` est une **marche structurelle sur les membres de donnée**.
Elle répond à la question « les *champs* de ce `T` sont-ils lisibles depuis N threads ? »
Le wrapper, lui, s'en sert pour **autoriser l'exécution concurrente de fonctions membres
`const` arbitraires**. Ce n'est pas une approximation, c'est une **erreur de catégorie** :
une fonction membre `const` peut écrire un `static` local de fonction, une globale, ou
faire un `const_cast`, et la marche ne verra jamais rien.

La bibliothèque **connaît** pourtant la différence : `allowed_std_wrappers.h` cite
`[res.on.data.races]` comme l'autorité qui garantit que les fonctions membres `const` des
conteneurs standard sont sans course. Aucune autorité de ce genre n'existe pour une classe
utilisateur.

Programme complet, compilé et exécuté contre l'en-tête réel :

```cpp
// The trait blesses `const HitCounter`: reflection sees one int, and one int is
// read-safe. But next() is const and mutates a function-local static, which the
// structural walk cannot see. synchronized_value therefore picks a
// std::shared_mutex and lock_shared() lets every reader in at once.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread race_static_gcc.cpp -o race && ./race
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <concepts>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

struct HitCounter {
    int seed;
    int next() const {
        static int calls = 0;
        return seed + (++calls);   // written under a *shared* lock
    }
};

static_assert(threadsafe::is_sendable_v<HitCounter>);
static_assert(threadsafe::is_synchronizable_v<const HitCounter>,
              "the trait says a const HitCounter is safe to read concurrently");
static_assert(std::same_as<threadsafe::synchronized_value<HitCounter>::mutex,
                           std::shared_mutex>);
static_assert(std::same_as<
    threadsafe::synchronized_value<HitCounter>::const_guard,
    threadsafe::value_guard<const HitCounter,
                            std::shared_lock<std::shared_mutex>>>);

int main() {
    constexpr int reader_count = 8;
    constexpr int calls_per_reader = 20000;

    threadsafe::synchronized_value<HitCounter> shared_counter{HitCounter{0}};

    std::atomic<int> ready{0};
    int highest_seen = 0;
    std::mutex report_mutex;

    {
        std::vector<std::jthread> readers;
        for (int index = 0; index < reader_count; ++index)
            readers.emplace_back([&] {
                ready.fetch_add(1);
                while (ready.load() < reader_count) {}
                int local_highest = 0;
                for (int call = 0; call < calls_per_reader; ++call) {
                    const auto reader_guard = shared_counter.lock_shared();
                    local_highest = std::max(local_highest, reader_guard->next());
                }
                const std::lock_guard<std::mutex> report{report_mutex};
                highest_seen = std::max(highest_seen, local_highest);
            });
    }

    const int expected = reader_count * calls_per_reader;
    std::printf("expected highest ticket: %d\n", expected);
    std::printf("actual   highest ticket: %d\n", highest_seen);
    std::printf("increments lost to the race: %d\n", expected - highest_seen);
    return 0;
}
```

Les **quatre** `static_assert` tiennent — la bibliothèque distribue réellement un
`std::shared_lock` ici. Exécution :

```
expected highest ticket: 160000
actual   highest ticket: 159421
increments lost to the race: 579
```

(mon exécution ; l'agent avait mesuré 449 perdus. La perte est petite mais non nulle et
non déterministe : c'est une vraie course de données, pas un artefact.)

La même tache aveugle a été confirmée par ThreadSanitizer sur une seconde forme (un cache
paresseux via `const_cast`), à partir d'une extraction fidèle bâtie à `-O0` pour que les
frames se résolvent :

```
WARNING: ThreadSanitizer: data race (pid=87112)
  Read of size 1 at 0x00016d2d6bb0 by thread T7:
    #0 LazySquare::square() const tsan_constcast.cpp:89
  Previous write of size 1 at 0x00016d2d6bb0 by thread T6:
    #0 LazySquare::square() const tsan_constcast.cpp:91
```

L'extraction a été diffée contre `include/threadsafe/details/synchronized_value.h`
lignes 17-101 : les seules différences sont le `static_assert` retiré, `make()` retiré, le
texte du commentaire sur les opérateurs supprimés, et le splice `[:get_mutex_type():]`
remplacé par un `std::shared_mutex` concret — qui est **exactement** ce que `g++-16`
sélectionne pour `LazySquare` (prouvé par une sonde compilée contre l'en-tête réel).

**Corollaire important, et à l'avantage de la bibliothèque :** la sélection n'est jamais
*unsafe dans l'autre sens*. Elle ne choisira jamais un `shared_mutex` pour un `T` dont la
lecture `const` serait structurellement dangereuse. Le défaut est entièrement du côté
« la structure ne dit rien des corps ». C'est aussi la raison pour laquelle il est
fermable **par opt-in** et non par une règle plus fine.

#### Le correctif proposé : demander au lieu de supposer

Remplacement complet de `include/threadsafe/details/synchronized_value.h`. Il porte aussi
les correctifs SV-02, SV-03 (durcissement partiel), SV-04, SV-05 et SV-06. Statut de
régression : **`suite-passes`** — les 11 TU de `tests/` compilent proprement contre lui.

```cpp
#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <utility>

#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

// Opt-in, exactly like is_synchronizable<T>: may several threads run the *const
// member functions* of a T at the same time?
//
// is_synchronizable<const T> cannot answer that question. It is a structural
// claim about the members reflection can see, and a const member function may
// write to state the walk never saw — through const_cast, into a function-local
// static, into a global. Trading exclusion for throughput on a structural
// inference turns that blind spot into a data race, so the trade is asked for,
// never assumed.
//
// The default says "no" for a class type, which costs concurrency and no
// correctness. Two families say yes on their own authority: scalars, which have
// no member functions at all, and the standard wrappers, whose const member
// functions [res.on.data.races] promises are race-free.
template <class T>
struct shares_const_reads : std::bool_constant<is_synchronizable_v<T>> {};

template <class T>
constexpr bool shares_const_reads_v = shares_const_reads<T>::value;

template <class T>
    requires std::is_scalar_v<T>
struct shares_const_reads<T> : std::true_type {};

namespace detail {
// [res.on.data.races] covers the wrapper's own const member functions; the
// elements it hands back are the caller's problem, so ask them in turn.
//
// A stateless policy — the comparator, the hash, the allocator — is not an
// element: it has no members for a concurrent const call to tear, so it is not
// asked.
inline consteval bool std_wrapper_shares_const_reads(std::meta::info type) {
    if (is_synchronizable_type(type))
        return true;

    for (std::meta::info wrapped : wrapped_types_of(type)) {
        const bool is_stateless_policy = std::meta::is_class_type(wrapped)
                                      && std::meta::is_empty_type(wrapped);
        if (!is_stateless_policy && !trait_value(^^shares_const_reads_v, wrapped))
            return false;
    }

    return true;
}
}

template <detail::std_wrapper T>
struct shares_const_reads<T>
    : std::bool_constant<detail::std_wrapper_shares_const_reads(^^T)> {};

template <class T>
class synchronized_value;

// [[nodiscard]] on the type, not only on lock(): a guard dropped on the floor
// is a lock taken and released at the semicolon, however it was obtained — out
// of lock(), or out of any function a user writes that returns one.
template <class T, class Lock>
class [[nodiscard]] value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    // A temporary guard dies at the semicolon, so a reference taken out of one
    // dangles. `with` is the spelling for the one-liner these deletions forbid.
    T& operator*() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference — use with(...) for a one-liner");
    T* operator->() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference — use with(...) for a one-liner");

    T& operator*() const& noexcept { return *value_; }
    T* operator->() const& noexcept { return value_; }

private:
    // Exactly the one wrapper that hands this guard out — not every
    // synchronized_value there is, and in particular not one a user writes as
    // an explicit specialization.
    friend class synchronized_value<std::remove_const_t<T>>;

    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

namespace detail {
// The one door onto a synchronized_value's members, so the multi-lock free
// functions below need no blanket friendship.
struct synchronized_access {
    template <class T>
    static auto& mutex_of(const synchronized_value<T>& value) noexcept {
        return value.mutex_;
    }
    template <class T>
    static T& value_of(synchronized_value<T>& value) noexcept {
        return value.value_;
    }
    template <class T>
    static const T& value_of(const synchronized_value<T>& value) noexcept {
        return value.value_;
    }
};
}

template <class T>
class synchronized_value {
    static_assert(!std::is_reference_v<T>,
                  "synchronized_value holds its value; a reference member would "
                  "make it a borrow the mutex cannot protect");
    static_assert(!std::is_array_v<T>,
                  "synchronized_value holds its value; wrap the array in a "
                  "std::array");
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

public:
    using mutex = std::conditional_t<shares_const_reads_v<T>,
                                     std::shared_mutex, std::mutex>;

    // The lock a reader takes: shared when the T vouches for concurrent const
    // member functions, exclusive when it does not.
    using const_guard_lock =
        std::conditional_t<shares_const_reads_v<T>, std::shared_lock<mutex>,
                           std::unique_lock<mutex>>;

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<const T, const_guard_lock>;

    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>,
                                    synchronized_value>
                      && ...))
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static std::shared_ptr<synchronized_value>
    make(Args&&... args) {
        return std::make_shared<synchronized_value>(
            std::forward<Args>(args)...);
    }

    // The API to reach for: the lock is taken for the whole callable and
    // released after it, so no reference to the value can outlive it and no
    // second thread can slip between a check and the act it decided on.
    template <class Callable>
        requires std::invocable<Callable&&, T&>
    decltype(auto) with(Callable&& callable) {
        const std::unique_lock<mutex> exclusive_lock{mutex_};
        return std::invoke(std::forward<Callable>(callable), value_);
    }

    template <class Callable>
        requires std::invocable<Callable&&, const T&>
    decltype(auto) with_shared(Callable&& callable) const {
        const const_guard_lock read_lock{mutex_};
        return std::invoke(std::forward<Callable>(callable), value_);
    }

    // The escape hatch, for when a single expression genuinely will not do.
    // nodiscard is load-bearing: a discarded guard is a temporary destroyed at
    // the semicolon, i.e. a lock taken and immediately released.
    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    friend struct detail::synchronized_access;

    mutable mutex mutex_;
    T value_;
};

// Lock several synchronized_values at once. std::scoped_lock runs std::lock,
// whose ordering makes the acquisition deadlock-free whatever order the callers
// write their arguments in — the thing two nested lock() calls cannot do.
template <class Callable, class... Ts>
    requires std::invocable<Callable&&, Ts&...>
decltype(auto) with_all(Callable&& callable, synchronized_value<Ts>&... values) {
    const std::scoped_lock all_locks{
        detail::synchronized_access::mutex_of(values)...};
    return std::invoke(std::forward<Callable>(callable),
                       detail::synchronized_access::value_of(values)...);
}

template <class Callable, class... Ts>
    requires std::invocable<Callable&&, const Ts&...>
decltype(auto) with_all_shared(Callable&& callable,
                               const synchronized_value<Ts>&... values) {
    auto read_locks = std::tuple{
        typename synchronized_value<Ts>::const_guard_lock{
            detail::synchronized_access::mutex_of(values), std::defer_lock}...};

    std::apply(
        [](auto&... each_lock) {
            if constexpr (sizeof...(Ts) > 1)
                std::lock(each_lock...);
            else
                (each_lock.lock(), ...);
        },
        read_locks);

    return std::invoke(std::forward<Callable>(callable),
                       detail::synchronized_access::value_of(values)...);
}

template <class T>
struct is_synchronizable<synchronized_value<T>> : is_sendable<T> {};

template <class T>
struct is_lifetime_aware<synchronized_value<T>> : is_lifetime_aware<T> {};

template <class T, class Lock>
struct is_sendable<value_guard<T, Lock>> : std::false_type {};
template <class T, class Lock>
struct is_lifetime_aware<value_guard<T, Lock>> : std::false_type {};

}

// "Yes, the const member functions of this type may run concurrently." The
// counterpart of THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE, and just as much a
// promise reflection cannot check.
#define THREADSAFE_SHARE_CONST_READS(...)          \
    template <>                                    \
    struct threadsafe::shares_const_reads<__VA_ARGS__> : std::true_type {}
```

Avec ce fichier, `race_static_gcc.cpp` ci-dessus rapporte :

```
expected highest ticket: 160000
actual   highest ticket: 160000
increments lost to the race: 0
```

**Ce que je pense de ce correctif, franchement.** Il est correct et il est vérifié, mais
il ajoute un quatrième trait à une bibliothèque qui en a déjà trois, et l'exigence du
projet est de *challenger le besoin*. Deux lectures défendables :

1. **Le prendre.** `shares_const_reads` est *exactement* la même figure pédagogique que
   `is_synchronizable` : opt-in, `false` par défaut, coûteux en débit et gratuit en
   correction. Sur scène, la diapositive « la réflexion voit les champs, pas les corps —
   donc on **demande** » est meilleure que la diapositive actuelle, qui est fausse.
2. **Ne pas le prendre, et supprimer plutôt la sélection automatique.** Un
   `synchronized_value<T>` qui prend toujours un `std::mutex`, plus un
   `synchronized_value<T, std::shared_mutex>` explicite, dit la même vérité avec **zéro**
   trait supplémentaire, et supprime la magie que l'utilisateur ne peut de toute façon pas
   surcharger aujourd'hui. Les mesures de [07](./07-performance-execution.md) plaident
   d'ailleurs dans ce sens : `std::shared_mutex` ne devient rentable qu'au-delà d'environ
   **500 ns** de section critique, et perd jusqu'à **39×** en dessous.

Ce que la bibliothèque ne peut pas garder, c'est la situation actuelle : un choix
silencieux, non surchargeable, fondé sur une preuve qui ne prouve pas ce qu'on lui fait
dire.

#### La divergence ODR entre deux TU (vérifiée par le *lead*)

Même famille de problème, autre mécanisme, et il concerne directement ce wrapper :
écrire `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` dans une TU et pas dans une autre donne
à `synchronized_value<Cache>` **deux dispositions mémoire** — `sizeof` 72 contre 208,
parce que le mutex change de type. Le programme **édite les liens sans aucun
diagnostic**, puis abandonne dans `std::__shared_mutex_pthread::lock_shared` avec
`Assertion __ret == 0 failed`, `exit=134`. C'est la contrepartie directe de la sélection
automatique : dès que le type du mutex dépend d'un trait spécialisable, une spécialisation
tardive ou absente devient une violation d'ODR silencieuse. Toute variante retenue en
§B.1 doit répondre à cela ; l'option 2 (mutex explicite en paramètre de template) le
supprime par construction.

---

### B.2 SV-02 — check-then-act, et l'impossibilité de verrouiller deux valeurs

Les opérateurs rvalue supprimés interdisent
`if (registry.lock()->empty()) registry.lock()->push_back(...)` écrit en une ligne. Ils
n'offrent **aucun remplacement**. L'utilisateur écrit donc la version à deux gardes — et
**la scission est le bug**. Symétriquement, la seule façon de toucher deux
`synchronized_value` est d'imbriquer les `lock()`, et l'imbrication a un **ordre**.

Programme complet, les deux moitiés dans un seul fichier (un chien de garde avorte
l'interblocage après 2 s pour que le test se termine) :

```cpp
// A realistic check-then-act bug that the current API not only permits but
// pushes you towards, and the deadlock that follows from having no multi-lock.
// Both halves in one file; a watchdog aborts the deadlock half after 2s so the
// test terminates.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread cta_and_deadlock.cpp -o p && ./p
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using registry_type = threadsafe::synchronized_value<std::vector<std::string>>;
using account_type = threadsafe::synchronized_value<int>;

// "Register the default endpoint once, if nobody registered anything yet."
void register_default_once(registry_type& registry, std::atomic<int>& ready,
                           int thread_count) {
    ready.fetch_add(1);
    while (ready.load() < thread_count) {}

    bool is_empty = false;
    {
        const auto reader_guard = registry.lock_shared();   // lock #1: CHECK
        is_empty = reader_guard->empty();
    }                                                       // lock released

    if (is_empty) {
        const auto writer_guard = registry.lock();          // lock #2: ACT
        writer_guard->push_back("default-endpoint");
    }
}

void transfer(account_type& from_account, account_type& to_account, int amount) {
    const auto from_guard = from_account.lock();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto to_guard = to_account.lock();          // nested: order matters
    *from_guard -= amount;
    *to_guard += amount;
}

int main() {
    constexpr int thread_count = 8;
    int runs_with_duplicates = 0;

    for (int attempt = 0; attempt < 2000; ++attempt) {
        registry_type registry{};
        std::atomic<int> ready{0};
        {
            std::vector<std::jthread> workers;
            for (int index = 0; index < thread_count; ++index)
                workers.emplace_back([&] {
                    register_default_once(registry, ready, thread_count);
                });
        }
        const auto final_guard = registry.lock_shared();
        if (final_guard->size() != 1)
            ++runs_with_duplicates;
    }
    std::printf("runs where the \"once\" registration happened more than once: "
                "%d / 2000\n", runs_with_duplicates);

    account_type alice{100};
    account_type bob{100};
    std::atomic<bool> finished{false};

    std::thread watchdog{[&] {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline)
            if (finished.load()) {
                std::printf("no deadlock this run\n");
                return;
            }
        std::printf("DEADLOCK: both transfers still blocked after 2s\n");
        std::fflush(stdout);
        std::_Exit(3);
    }};
    watchdog.detach();

    std::thread first{[&] { transfer(alice, bob, 10); }};
    std::thread second{[&] { transfer(bob, alice, 10); }};
    first.join();
    second.join();
    finished.store(true);
    std::printf("completed\n");
    return 0;
}
```

Les deux moitiés compilent **sans le moindre avertissement** à `-Wall -Wextra`. Exécution
mesurée par moi-même :

```
runs where the "once" registration happened more than once: 844 / 2000
DEADLOCK: both transfers still blocked after 2s
exit=3
```

L'agent avait mesuré **236 / 2000** sur la même machine ; j'ai mesuré **844 / 2000**. La
fenêtre de course dépend fortement de l'ordonnancement, donc le **taux** n'est pas une
grandeur stable — c'est le fait qu'il soit **non nul et massif** qui compte. L'interblocage,
lui, s'est produit **3 fois sur 3** chez l'agent et à ma première exécution.

**Pourquoi c'est le défaut d'API le plus intéressant du lot.** Ce n'est pas un mésusage
exotique ; c'est ce que la **forme même** de l'API produit. La suppression de
`operator*() &&` rend la ligne atomique illégale et ne propose **rien** à la place, donc
l'utilisateur écrit la version en deux gardes : *le dispositif de sûreté de la
bibliothèque le pousse dans la course*. Et aucune prudence ne sauve le virement : avec
seulement `lock()`, une section critique sur deux objets **doit** s'imbriquer.

**Le correctif complet.** Il fait partie de l'en-tête de remplacement donné en §B.1 ;
statut de régression **`suite-passes`**. Les quatre pièces, verbatim :

```cpp
    // in class synchronized_value, before lock():
    // The API to reach for: the lock is taken for the whole callable and
    // released after it, so no reference to the value can outlive it and no
    // second thread can slip between a check and the act it decided on.
    template <class Callable>
        requires std::invocable<Callable&&, T&>
    decltype(auto) with(Callable&& callable) {
        const std::unique_lock<mutex> exclusive_lock{mutex_};
        return std::invoke(std::forward<Callable>(callable), value_);
    }

    template <class Callable>
        requires std::invocable<Callable&&, const T&>
    decltype(auto) with_shared(Callable&& callable) const {
        const const_guard_lock read_lock{mutex_};
        return std::invoke(std::forward<Callable>(callable), value_);
    }
```

```cpp
    // in class synchronized_value, private section:
    friend struct detail::synchronized_access;
```

```cpp
    // at namespace scope in threadsafe::detail, after the forward declaration
    // of synchronized_value:
    // The one door onto a synchronized_value's members, so the multi-lock free
    // functions below need no blanket friendship.
    struct synchronized_access {
        template <class T>
        static auto& mutex_of(const synchronized_value<T>& value) noexcept {
            return value.mutex_;
        }
        template <class T>
        static T& value_of(synchronized_value<T>& value) noexcept {
            return value.value_;
        }
        template <class T>
        static const T& value_of(const synchronized_value<T>& value) noexcept {
            return value.value_;
        }
    };
```

```cpp
    // at namespace scope in threadsafe, after class synchronized_value:
    // Lock several synchronized_values at once. std::scoped_lock runs std::lock,
    // whose ordering makes the acquisition deadlock-free whatever order the callers
    // write their arguments in — the thing two nested lock() calls cannot do.
    template <class Callable, class... Ts>
        requires std::invocable<Callable&&, Ts&...>
    decltype(auto) with_all(Callable&& callable, synchronized_value<Ts>&... values) {
        const std::scoped_lock all_locks{
            detail::synchronized_access::mutex_of(values)...};
        return std::invoke(std::forward<Callable>(callable),
                           detail::synchronized_access::value_of(values)...);
    }

    template <class Callable, class... Ts>
        requires std::invocable<Callable&&, const Ts&...>
    decltype(auto) with_all_shared(Callable&& callable,
                                   const synchronized_value<Ts>&... values) {
        auto read_locks = std::tuple{
            typename synchronized_value<Ts>::const_guard_lock{
                detail::synchronized_access::mutex_of(values), std::defer_lock}...};

        std::apply(
            [](auto&... each_lock) {
                if constexpr (sizeof...(Ts) > 1)
                    std::lock(each_lock...);
                else
                    (each_lock.lock(), ...);
            },
            read_locks);

        return std::invoke(std::forward<Callable>(callable),
                           detail::synchronized_access::value_of(values)...);
    }
```

L'en-tête a besoin de `<functional>` et `<tuple>` en plus, et `const_guard_lock` doit être
**public** pour que `with_all_shared` puisse le nommer. Réécrit avec cette API (`with()`
pour le check-then-act, `with_all()` pour le virement, 20 000 virements par thread dans des
ordres opposés, même chien de garde à 2 s), le programme rapporte :

```
with(): duplicate registrations in 2000 runs = 0
with_all(): no deadlock, conserved total = 200 (expected 200)
```

C'est le correctif que je recommanderais **en premier** de tout ce rapport, parce qu'il ne
répare pas seulement un bug : il fait de l'orthographe sûre la plus **courte**, ce qui est
le seul mécanisme de sûreté qui tient à l'échelle d'une vraie base de code — et sur scène,
`registry.with([](auto& r){ ... })` se lit mieux qu'une paire de gardes nommées.

### B.3 SV-03 — ce que les opérateurs rvalue supprimés ferment vraiment, et ce qu'ils ne ferment pas

**Ce qu'ils ferment**, et ils le font bien (vérifié) : `auto& r = *sv.lock();`,
`*sv.lock() = 5;` et `sv.lock()->push_back(1);` sont tous refusés, avec le message
annoncé :

```
error: use of deleted function 'T& threadsafe::value_guard<T, Lock>::operator*() &&':
  a temporary guard is destroyed at the semicolon, so it cannot hand out a reference
```

**Ce qu'ils ne ferment pas.** `operator*() const&` et `operator->() const&` rendent une
`T&` / `T*` nue, dont la validité est liée à la durée de vie de la garde, et **rien** ne
lie l'appelant à cette durée de vie. Cinq échappatoires compilent **sans un seul
diagnostic** à `-Wall -Wextra` : quatre avec une garde nommée — rendre un `int&` obtenu par
`(*guard)[0]`, rendre `guard.operator->()`, capturer `*guard` dans une `std::function` qui
s'échappe, et `leaked = &*guard` qui survit au bloc de la garde — et **une avec le
temporaire même** que les suppressions visaient. C'est celle-là qui est parlante, parce
qu'elle blanchit le temporaire à travers un simple paramètre `const&` :

```cpp
// The deleted rvalue operators stop `*sv.lock()` written literally. They do not
// stop the same temporary from being laundered through a const lvalue
// reference, where `operator*() const&` is viable again -- and the temporary
// still dies at the semicolon.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>
#include <vector>

using sync_vector = threadsafe::synchronized_value<std::vector<int>>;

// Any function taking the guard by const& will do; this one is three words long
// and looks entirely innocent.
template <class Guard>
auto* address_of(const Guard& guard) {
    return &*guard;                       // operator*() const& : not deleted
}

int main() {
    sync_vector shared_vector{std::vector<int>{0}};

    // `*shared_vector.lock()` is rejected. This is the same expression.
    std::vector<int>* escaped = address_of(shared_vector.lock());
    //                                                          ^ guard destroyed here

    std::jthread other{[&] {
        for (int step = 0; step < 100000; ++step) {
            const auto guard = shared_vector.lock();
            guard->push_back(step);
        }
    }};

    for (int step = 0; step < 100000; ++step)
        escaped->push_back(-step);        // no lock held: unsynchronised write

    other.join();
    const auto final_guard = shared_vector.lock_shared();
    std::printf("survived, size=%zu (expected 200001)\n", final_guard->size());
    return 0;
}
```

Compilé à `-Wall -Wextra` : **zéro diagnostic**, contre l'en-tête réel comme contre
l'en-tête corrigé. Exécuté trois fois : `exit=134`, `exit=134`, `exit=134` — aucune sortie
du tout, les deux `push_back` non synchronisés corrompent mutuellement leur réallocation.
(L'agent avait mesuré 133/134/133 ; même verdict, signaux voisins.) Recompilé avec
`-fsanitize=address` :

```
==91171==ERROR: AddressSanitizer: heap-use-after-free on address 0x60c0000001c0
    #0 in memmove
    #1 in std::thread::_State_impl<...>::_M_run() hole_temporary.cpp:255
freed by thread T0 here:
    #1 in main hole_temporary.cpp:35
```

**Fix : aucun qui ferme le trou, et il faut le dire ainsi.** Toute API qui rend une `T&`
depuis une garde peut être blanchie ; les suppressions sont un ralentisseur sur *une*
syntaxe, pas une barrière. La seule réponse complète est de **cesser de distribuer la
référence** — faire de `with` / `with_shared` (§B.2) toute l'interface et supprimer
`operator*` / `operator->` — et cela **régresse** `tests/test_synchronized_value.cpp`, qui
affirme `std::same_as<decltype(*std::declval<const sync_int::guard&>()), int&>` et
`decltype(*std::declval<const sync_memo::const_guard&>()) == const Memo&`. Deux
durcissements **partiels** sont vérifiés (`class [[nodiscard]] value_guard`, et l'API
`with` qui supprime la *pression* à atteindre `lock()`), et **aucun des deux ne ferme
l'échappatoire** : `sv.with([&](auto& v){ escaped = &v; })` fuit tout aussi bien.

**Ma recommandation est une suppression de prétention, pas du code.** Le message de
suppression se lit comme une garantie — « *a temporary guard is destroyed at the semicolon,
so it cannot hand out a reference* » — alors qu'il ferme une orthographe sur cinq. Le
changement honnête est de **reformuler ce message** pour qu'il cesse de promettre ce qu'il
ne tient pas, par exemple en le rédigeant comme un conseil (« *use with(...) for a
one-liner* », ce que fait l'en-tête de remplacement) plutôt que comme une impossibilité.
Sur scène, « voici ce que le compilateur attrape, et voici les quatre choses qu'il
n'attrape pas » est une meilleure histoire — et plus vraie — que « le compilateur vous en
empêche ».

### B.4 Les quatre défauts secondaires, et le seul qui mérite du code

| # | Ce qui se passe aujourd'hui (vérifié) | Verdict |
|---|---|---|
| SV-04 | `synchronized_value<Ledger> supposed_copy{original};` compile et affiche `"copy" balance = -1` | **corriger** : garde de 3 lignes |
| SV-05 | une spécialisation explicite écrite par l'utilisateur forge un `value_guard` sur n'importe quel mutex : `forged guard over an unprotected int: 7` | **corriger** : amitié restreinte |
| SV-06 | `synchronized_value<T&>` s'instancie, est **béni** par les traits, et n'explose qu'au `lock()` avec trois `forming pointer to reference type` **dans l'en-tête** | **corriger** : 2 `static_assert` |
| SV-07 | `[[nodiscard]]` produit **2 avertissements pour 4 sites d'appel**, et le build du projet n'active ni `-Wall`, ni `-Werror` | à documenter, correctif optionnel |
| SV-08 | `synchronized_value<std::vector<int>> v{{1,2,3}}` ne compile pas ; `v{3, 0}` compile et signifie `vector(3, 0)` | cosmétique, mais mauvais sur scène |

**SV-04**, le plus surprenant des quatre, et celui pour lequel `copy_on_write` a déjà la
bonne réponse. Programme complet :

```cpp
// The greedy variadic constructor is not guarded against a single argument of
// the wrapper's own type (copy_on_write is).  Against a *non-const* lvalue the
// template deduces Args = synchronized_value& and matches exactly, while the
// deleted copy constructor needs a qualification conversion -- so the template
// wins and silently builds a T out of the wrapper instead of reporting
// "synchronized_value is non-copyable".
#include <threadsafe/threadsafe.h>
#include <cstdio>

struct Ledger;
struct Ledger {
    int balance = 0;
    Ledger() = default;
    explicit Ledger(int starting_balance) : balance(starting_balance) {}
    // A perfectly ordinary converting constructor -- no template, so Ledger
    // stays sendable.
    Ledger(threadsafe::synchronized_value<Ledger>&) : balance(-1) {}
};

static_assert(threadsafe::is_sendable_v<Ledger>);

int main() {
    threadsafe::synchronized_value<Ledger> original{Ledger{100}};
    threadsafe::synchronized_value<Ledger> supposed_copy{original};  // compiles!

    const auto original_guard = original.lock_shared();
    const auto copy_guard = supposed_copy.lock_shared();
    std::printf("original balance = %d\n", original_guard->balance);
    std::printf("\"copy\" balance   = %d\n", copy_guard->balance);
    return 0;
}
```

Contre l'en-tête réel, compile sans diagnostic et affiche (mon exécution) :

```
original balance = 100
"copy" balance   = -1
```

La ligne `synchronized_value<Ledger> copy{original};` se lit comme une copie dans
n'importe quelle revue. Le type **affiche**
`synchronized_value(const synchronized_value&) = delete` ; le modèle mental de
l'utilisateur est « le compilateur va m'arrêter ». À la place, le second wrapper est
construit à partir d'un `Ledger` que personne n'a écrit, valant `-1`, et le mutex qui
protège `original` n'a **jamais été pris**. La bibliothèque documente *exactement* ce piège
de résolution de surcharge dans `utils.h` (`may_hijack_copy_move`) et rejette les types des
autres pour cette raison, tout en laissant son propre wrapper ouvert. Correctif complet —
la même garde que `copy_on_write` utilise déjà, statut **`suite-passes`** :

```cpp
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>,
                                    synchronized_value>
                      && ...))
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {}
```

**SV-05.** Le constructeur de `value_guard` est privé, mais
`template <class> friend class synchronized_value;` befriend **toutes** les
spécialisations — y compris une que l'utilisateur écrit. Spécialiser explicitement un
template de classe de bibliothèque sur un type utilisateur est du C++ parfaitement légal.
Programme complet :

```cpp
// value_guard's constructor is private, but `template <class> friend class
// synchronized_value;` befriends EVERY specialisation -- including one the user
// writes.  Explicitly specialising a library class template on a user type is
// legal, so the private constructor is reachable from outside the library.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <mutex>

struct MyTag;

namespace threadsafe {
template <>
class synchronized_value<MyTag> {
public:
    static value_guard<int, std::unique_lock<std::mutex>>
    forge(std::mutex& any_mutex, int& any_value) {
        return value_guard<int, std::unique_lock<std::mutex>>{any_mutex,
                                                              any_value};
    }
};
}

int main() {
    std::mutex unrelated_mutex;
    int unprotected_value = 7;
    const auto forged =
        threadsafe::synchronized_value<MyTag>::forge(unrelated_mutex,
                                                     unprotected_value);
    std::printf("forged guard over an unprotected int: %d\n", *forged);
    return 0;
}
```

Contre l'en-tête réel, compile et affiche (mon exécution) `forged guard over an
unprotected int: 7`. Une garde est le **jeton porteur de preuve** de la bibliothèque :
`value_guard<T, Lock>` est censé signifier « le mutex qui protège ce `T` est détenu ». Une
garde forgée ne signifie rien, et elle est indiscernable d'une vraie sur tout site d'appel
qui prend une garde. Correctif, statut **`suite-passes`** — remplacer

```cpp
    template <class>
    friend class synchronized_value;
```

par

```cpp
    // Exactly the one wrapper that hands this guard out — not every
    // synchronized_value there is, and in particular not one a user writes as
    // an explicit specialization.
    friend class synchronized_value<std::remove_const_t<T>>;
```

`remove_const_t` est ce qui permet à `synchronized_value<T>` de continuer à atteindre
`value_guard<const T, Lock>` pour `lock_shared()`. Cela ne rend pas le forgeage
impossible — qui spécialise `synchronized_value<Foo>` peut toujours forger un
`value_guard<Foo, ...>` — mais confine l'ensemble atteignable aux gardes sur **son propre
type** plutôt qu'à tous les types du programme.

**SV-06.** `static_assert(sendable<T>)` passe pour une référence vers un type
synchronizable (`is_sendable<T&>` renvoie à `is_synchronizable<T>`), donc le corps de la
classe est **accepté** et `is_synchronizable_v<synchronized_value<sync_int&>>` répond
`true`. L'échec ne survient qu'à l'instanciation des types de garde :

```cpp
#include <threadsafe/threadsafe.h>
using sync_int = threadsafe::synchronized_value<int>;
using sync_ref = threadsafe::synchronized_value<sync_int&>;
static_assert(sizeof(sync_ref) > 0, "the class itself instantiates");
static_assert(!threadsafe::is_lifetime_aware_v<sync_ref>);
static_assert(threadsafe::is_synchronizable_v<sync_ref>);
int main() {
    sync_int owned{7};
    sync_ref borrower{owned};      // constructs, binding the reference member
    (void)borrower;
    auto guard = borrower.lock();  // <-- only here does it fail
    (void)guard;
}
```

```
include/threadsafe/details/synchronized_value.h:30:8: error: forming pointer to reference type 'threadsafe::synchronized_value<int>&'
include/threadsafe/details/synchronized_value.h:33:8: error: forming pointer to reference type 'threadsafe::synchronized_value<int>&'
include/threadsafe/details/synchronized_value.h:43:8: error: forming pointer to reference type 'threadsafe::synchronized_value<int>&'
```

Toute la prémisse de la bibliothèque est qu'un refus **nomme le coupable**. Celui-ci pointe
trois fois au milieu de l'en-tête et ne mentionne jamais le type de l'utilisateur — et,
pire, les traits **cautionnent** le type avant qu'il n'échoue, donc la marche bénira
volontiers une structure qui en contient un. Correctif, statut **`suite-passes`** : deux
`static_assert` en tête de classe, au-dessus de celui sur `sendable<T>`.

```cpp
    static_assert(!std::is_reference_v<T>,
                  "synchronized_value holds its value; a reference member would "
                  "make it a borrow the mutex cannot protect");
    static_assert(!std::is_array_v<T>,
                  "synchronized_value holds its value; wrap the array in a "
                  "std::array");
```

**SV-07** est un constat plus qu'un défaut, et il vaut d'être dit tel quel. L'en-tête
qualifie `[[nodiscard]]` de « load-bearing ». Programme complet :

```cpp
// Every way to drop a guard on the floor, at the project's own warning level
// (CMakeLists.txt adds nothing but -freflection: no -Wall, no -Wextra, no
// -Werror).  Every line below takes a lock and releases it at the semicolon.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>
#include <vector>

using sync_vector = threadsafe::synchronized_value<std::vector<int>>;

// A perfectly ordinary accessor. Nothing here carries [[nodiscard]].
sync_vector::guard borrow(sync_vector& shared_vector) {
    return shared_vector.lock();
}

int main() {
    sync_vector shared_vector{std::vector<int>{}};

    shared_vector.lock();                        // (1) warns  -Wunused-result
    (void)shared_vector.lock();                  // (2) SILENT
    static_cast<void>(shared_vector.lock_shared()); // (3) SILENT
    borrow(shared_vector);                       // (4) SILENT: nodiscard is on
                                                 //     lock(), not on the guard

    // The bug this hides: the "critical section" holds no lock at all.
    std::vector<std::jthread> writers;
    for (int worker = 0; worker < 4; ++worker)
        writers.emplace_back([&] {
            for (int step = 0; step < 1000; ++step) {
                (void)shared_vector.lock();      // lock released here
                shared_vector.lock_shared();     // and here
            }
        });
    writers.clear();

    std::printf("built, linked and ran\n");
    return 0;
}
```

J'ai compté les avertissements : **exactement 2**, à `-Wall -Wextra` comme sans. Le binaire
s'édite et s'exécute. Les casts `(void)` explicites ne peuvent être attrapés par rien —
c'est l'échappatoire que la norme prévoit — mais le cas de l'accesseur, **lui**, est
attrapable, et c'est celui qu'une vraie base de code rencontre. Deux changements vérifiés
(`suite-passes`) : mettre l'attribut sur le **type**,

```cpp
// [[nodiscard]] on the type, not only on lock(): a guard dropped on the floor
// is a lock taken and released at the semicolon, however it was obtained — out
// of lock(), or out of any function a user writes that returns one.
template <class T, class Lock>
class [[nodiscard]] value_guard {
```

et faire de la chose une **erreur** pour tout consommateur de la cible d'interface, dans
`CMakeLists.txt` :

```cmake
target_compile_options(threadsafe INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Werror=unused-result>)
```

**SV-08** est cosmétique mais mauvais devant un public. `synchronized_value<std::vector<int>> endpoints{{"a", "b"}}`
est la première ligne que n'importe qui essaiera, et elle produit une page de résolution de
surcharge ; et `v{3, 0}` compile en signifiant **trois zéros**, ce qui est *le* piège
d'initialisation C++ que la salle connaît déjà. Le reproduire à l'intérieur d'une
bibliothèque de sûreté est un mauvais signal. Correctif, statut **`suite-passes`** (ajouter
`<initializer_list>` aux deux en-têtes) :

```cpp
    // in synchronized_value, immediately before the deleted copy constructor.
    // A braced list is a non-deduced context for Args..., so without this the
    // natural spelling `synchronized_value<std::vector<int>> v{{1, 2, 3}}` does
    // not compile at all, and `v{3, 0}` silently means vector(3, 0).
    template <class Element>
        requires std::constructible_from<T, std::initializer_list<Element>&>
    explicit synchronized_value(std::initializer_list<Element> elements)
        : value_(elements) {}
```

```cpp
    // in copy_on_write, immediately before `const T& operator*() const noexcept`.
    template <class Element>
        requires std::constructible_from<T, std::initializer_list<Element>&>
    explicit copy_on_write(std::initializer_list<Element> elements)
        : ptr_(std::make_shared<T>(elements)) {}
```

C'est un **changement de comportement délibéré** pour la seule orthographe qui compile
aujourd'hui : `sync_vec braced{3, 0}` passe de trois zéros aux deux éléments `{3, 0}`.
C'est la réponse de `std::vector` lui-même aux mêmes accolades, et rien dans `tests/` ne
dépend de l'ancienne signification.

### B.5 Ce qui a résisté sur `synchronized_value`

- **Le corps du wrapper est sans course.** Sous TSan, sur une extraction diffée contre
  l'en-tête, `synchronized_value` ne présente aucune course. Le défaut de SV-01 est
  entièrement dans le **choix** du mutex, jamais dans son usage.
- **La surcharge de la bibliothèque est essentiellement nulle** :
  `synchronized_value<std::map>` égale un `std::shared_mutex` écrit à la main.
- Les opérateurs rvalue supprimés **rejettent bien** les trois écritures littérales, avec
  le message annoncé.
- Le constructeur privé de `value_guard` bloque le forgeage **direct** depuis l'extérieur
  sur les deux en-têtes ; seule l'amitié était trop large (SV-05).
- `value_guard` n'est ni copiable ni déplaçable, tout en restant renvoyable en prvalue
  depuis une fonction utilisateur et liable à un `const auto&`. Une garde *déplaçable*
  pourrait être logée dans un agrégat et voyager ; celle-ci ne le peut pas.
- Un `const synchronized_value&` offre `lock_shared()` et **pas** `lock()`, pour les deux
  spécialisations de mutex.
- Le constructeur de copie supprimé **gagne bien** pour un `T` ordinaire :
  `synchronized_value<int> b{a};` est rejeté. SV-04 ne se déclenche que pour un `T`
  lui-même constructible depuis `synchronized_value<T>&`.
- Une `synchronized_value` sur un emprunt (pointeur brut ou `reference_wrapper` vers une
  autre `synchronized_value`) est sendable mais **pas** lifetime_aware, et la transitivité
  tient à travers `std::shared_ptr` : `launch_task` la refuse. Le chemin **vérifié** est
  fermé même si le type s'instancie.
- `launch_task` refuse `std::ref(synchronized_value)` avec un message exact et spécifique,
  et le chemin sanctionné — `synchronized_value::make()` + un `shared_ptr` partagé entre 8
  workers `launch_task` faisant 1000 `push_back` verrouillés chacun — se compile et
  s'exécute proprement.

### B.6 Le coût du `shared_mutex`, et l'impossibilité de le refuser

Résumé des mesures détaillées dans [07](./07-performance-execution.md), parce qu'elles
changent la lecture de §B.1 : le `std::shared_mutex` choisi automatiquement coûte, par
opération sous verrou, **jusqu'à 118×** un `std::mutex` (2 threads, 50 % de lectures :
1333,3 ns contre 11,2 ns), et il ne devient rentable qu'au-delà d'environ **500 ns** de
section critique (à 4 threads / 90 % de lectures : 38,8× perdant à 0 spin, encore 12,9×
perdant à 50 spins, et enfin gagnant 0,67× à 800 spins).

La sélection est donc **sûre** — jamais un `shared_mutex` pour un `T` dont la lecture
`const` serait structurellement dangereuse — mais elle est **silencieuse**, et
**l'utilisateur ne peut pas la surcharger**. C'est un argument de plus, indépendant de
SV-01, pour l'option 2 de §B.1 : rendre le mutex explicite.

---

## C. `asynchronous_task_launcher`

La réponse à l'exigence — *« le lanceur ne doit pas accepter de type unsafe »* — est donnée
en tête de rapport : **non, il en accepte encore trois familles**. Cette section les
détaille, puis traite les deux bugs de comportement (L3, L5) et les deux défauts de
diagnostic (L10, L11).

### C.1 L1 — un membre `static` dans le foncteur passe toutes les portes (fermable)

La réflexion voit une classe **vide** : pas de base, pas de membre de donnée **non
statique**, aucun copy/move/destructeur écrit à la main. `is_sendable`,
`is_lifetime_aware` et `is_synchronizable<const T>` répondent donc tous **oui**, et
`launchable_task` est satisfait. Huit copies de ce foncteur courent ensuite sur l'unique
objet statique partagé.

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {
// Reflection sees an EMPTY class: no bases, no non-static data members, and no
// user-written copy/move/destructor. Every trait therefore answers yes.
struct AppendsToASharedLog {
    static inline std::vector<std::string> shared_log;

    void operator()() const {
        for (int index = 0; index < 20'000; ++index)
            shared_log.push_back("line");
    }
};
}

static_assert(threadsafe::is_sendable_v<AppendsToASharedLog>);
static_assert(threadsafe::is_lifetime_aware_v<AppendsToASharedLog>);
static_assert(threadsafe::is_synchronizable_v<const AppendsToASharedLog>);
static_assert(threadsafe::launchable_task<AppendsToASharedLog>);
static_assert(threadsafe::launchable_scoped_task<AppendsToASharedLog>);

int main() {
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int index = 0; index < 8; ++index)
            launcher.launch_task(AppendsToASharedLog{});
    }
    std::printf("expected %d entries, got %zu\n", 8 * 20'000,
                AppendsToASharedLog::shared_log.size());
    return AppendsToASharedLog::shared_log.size() == 8 * 20'000 ? 0 : 1;
}
```

Compile proprement — **les cinq `static_assert` tiennent**. Exécuté trois fois à `-O2` :
`exit=134`, `exit=133`, `exit=134` — corruption du tas, aucune sortie atteinte. (L'agent
avait mesuré 133 trois fois sur trois.) Avec `-fsanitize=address` :

```
==81747==ERROR: AddressSanitizer: heap-use-after-free on address 0x611000000040 ...
READ of size 8 at 0x611000000040 thread T4
    #0 ... std::thread::_State_impl<...AppendsToASharedLog...>::_M_run() x02_static_state.cpp:255
freed by thread T2 here:
    #0 ... _ZdlPv
    #1 ... _M_run() x02_static_state.cpp:255
```

**C'est le contre-exemple le plus propre qui survit** : pas de cast, pas de `std::ref`, pas
de macro de bénédiction — un foncteur ordinaire avec un compteur ou un journal en membre
statique, ce qui est **exactement** la façon dont on écrit un worker « sans état ».

*Note méthodologique honnête :* la démonstration originale du corpus (`r01_static_race.cpp`,
un `shared_counter = shared_counter + 1` sur un million d'itérations) est une **impasse** à
`-O2` : GCC replie la boucle en un seul `add`, la fenêtre de course fait une instruction, et
le programme affiche `expected 8000000, got 8000000` trois fois sur trois. La course ne se
manifeste qu'à `-O0` (1 146 395 / 1 023 839 / 1 307 625). La **revendication** était juste,
la démonstration ne l'était pas ; elle a été remplacée par le programme ci-dessus, qui
plante à `-O2`.

**Correctif complet, statut `suite-passes`.** Trois éditions. (1) Dans
`include/threadsafe/details/utils.h`, ajouter `#include <vector>` et, immédiatement avant
`inline consteval bool is_copy_move_destroy_member(...)`, insérer :

```cpp
// A static data member is ONE object, shared by every instance of the type and
// therefore by every thread that holds one. The nonstatic walk never sees it, so
// a functor whose operator() writes a static looks empty to reflection: no
// bases, no members, every trait says yes -- and eight copies of it race on one
// object. These are the statics that have to be checked; `const` ones are only
// read, so they are asked the read-safe question instead.
inline consteval std::vector<std::meta::info>
shared_static_members(std::meta::info type) {
    std::vector<std::meta::info> shared;
    for (std::meta::info member :
         std::meta::members_of(type, std::meta::access_context::unchecked()))
        if (std::meta::is_variable(member) && std::meta::is_static_member(member))
            shared.push_back(member);
    return shared;
}
```

(2) Dans `include/threadsafe/details/sendable.h`, à l'intérieur de
`diagnose_default_is_sendable`, immédiatement avant la boucle
`for (info base : bases_of(type, context))` :

```cpp
    for (info member : shared_static_members(type)) {
        const auto member_type = type_of(member);
        const bool read_only = is_const_type(member_type);
        if (!is_synchronizable_type(read_only ? add_const(remove_cv(member_type))
                                              : remove_cv(member_type)))
            reject(type,
                   u8"has a static data member `" + member_name(member)
                       + u8"` of type " + type_name(member_type)
                       + u8", which is one object shared by every instance and "
                         u8"every thread; make it synchronizable (an atomic, a "
                         u8"synchronized_value) or const",
                   path);
    }
```

(3) Dans `include/threadsafe/details/synchronizable.h`, à l'intérieur de
`diagnose_default_is_const_synchronizable`, immédiatement avant **sa** boucle
`for (info base : bases_of(type, context))`, insérer le bloc **identique, octet pour
octet**, à celui de (2).

Après correctif, le message reçu est :

```
'what()': '{anonymous}::WritesAStatic has a static data member `shared_counter` of type
 long long int, which is one object shared by every instance and every thread; make it
 synchronizable (an atomic, a synchronized_value) or const'
```

et `static inline std::atomic<long long>`, `static constexpr int table[4]` et les foncteurs
sans statique **continuent tous de passer**.

**Deux prix à connaître avant de l'appliquer.** D'abord, une assertion de scénario du
corpus bascule volontairement (`s01_callables.cpp`, *« POLARITY: static-member functor
accepted »*) — c'est le but. Ensuite, et c'est plus gênant, la variante de ce correctif
mesurée sur le corpus adverse (voir [01](./01-robustesse-des-traits.md)) régresse
`static constexpr const char* label = "x"` : un pointeur `const` vers du `const char`
n'est pas synchronizable au sens du trait, alors qu'il ne présente aucun danger. Le coût
est isolé et mesuré, mais il est réel : c'est **une** forme courante de constante de
classe qui se met à échouer. À arbitrer avec [01](./01-robustesse-des-traits.md).

### C.2 L2 — la réentrance : le même trou, hors de portée de la réflexion

Une lambda **sans capture** est un type vide, donc `launchable_task` tient. Si son corps
atteint un `asynchronous_task_launcher` de portée namespace et appelle `launch_task`, deux
tâches font `push_back` **concurremment** dans le même `std::vector<std::jthread>` : une
réallocation non synchronisée.

```cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>

namespace {
// The launcher lives at namespace scope, so a CAPTURELESS lambda reaches it.
// A captureless closure is an empty type: is_sendable and is_lifetime_aware
// both say yes, and launchable_task is satisfied.
threadsafe::asynchronous_task_launcher shared_launcher;

std::atomic<int> tasks_started{0};

void reenter() {
    ++tasks_started;
    // Two tasks running this concurrently both push_back into shared_launcher's
    // std::vector<std::jthread>. No lock, no atomic: a data race, and a
    // reallocation under another thread's iterator.
    if (tasks_started.load() < 64)
        shared_launcher.launch_task([] { reenter(); });
}
}

static_assert(threadsafe::launchable_task<decltype([] { reenter(); })>,
              "the traits do NOT stop a task from launching into its own launcher");

int main() {
    for (int index = 0; index < 8; ++index)
        shared_launcher.launch_task([] { reenter(); });

    std::printf("main done, started=%d\n", tasks_started.load());
}
```

Compile proprement. Exécuté trois fois à `-O2` : `exit=138` (SIGBUS), `exit=133`,
`exit=133`. Le *lead* avait mesuré `139` (SIGSEGV), `133`, `139` — même verdict, le signal
exact dépend de l'endroit où la réallocation tombe. **Et le correctif L1 ne le ferme pas** :
`g++-16 -fsyntax-only` contre l'arbre patché accepte toujours le fichier (`ACCEPTED`), et
l'exécutable plante (`exit=133`).

**Correctif : aucun, et c'est structurel.** Un trait structurel lit des **types**, pas des
corps de fonction. La closure `[] { reenter(); }` n'a ni base ni membre, et `reenter` nomme
`shared_launcher` à l'intérieur de sa **définition** ; aucune requête de réflexion
disponible dans P2996 / GCC 16 ne voit à travers un appel de fonction libre les globales
qu'il touche. Fermer cela demanderait soit une analyse d'effets sur tout le programme, soit
de rendre toute variable globale « non-sendable par association », ce qui n'est pas
exprimable.

*L'atténuation réaliste est de niveau exécution* — mettre un mutex autour de `threads_`
pour que des `launch_task` concurrents soient au moins **memory-safe** — et je ne la
recommande **pas** : elle bénirait silencieusement la réentrance au lieu de la rejeter,
alors que le modèle annoncé de la bibliothèque est le rejet à la compilation. Le bon
geste ici est une **phrase** : `asynchronous_task_launcher` n'est pas réentrant, et un pool
qui redistribue du travail corrompt son propre vecteur. C'est un aveu qui fait une bonne
diapositive.

### C.3 Le handle *thread-affine* (vérifié par le *lead*)

Troisième famille, même cause : un agrégat d'un seul `std::size_t` qui **indexe un
stockage `thread_local`** est accepté par `launch_task`, et le programme part en
`SIGSEGV` (`exit=139`). La réflexion voit un entier ; l'affinité au thread est dans le
corps de la fonction qui déréférence l'index. Rien à corriger, tout à documenter.

### C.4 L3 — `launch_scoped_task` interblique sur toute tâche coopérative

`launch_scoped_task` crée un `std::jthread` et le rejoint **immédiatement** :

```cpp
    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }
```

`std::jthread` injecte un `stop_token`, et le `static_assert` de la classe **bénit**
explicitement cette injection. Mais le `jthread` — donc l'unique `stop_source` capable de
le signaler — est une **variable locale** de `launch_scoped_task` : aucun appelant ne peut
jamais demander l'arrêt. Toute tâche coopérative qui boucle jusqu'à `stop_requested()`
interblique l'appelant.

```cpp
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;

namespace {
// A perfectly ordinary cooperative task: it runs until it is asked to stop.
// std::jthread injects the stop_token, and the traits bless it (the class has
// the static_assert for exactly that).
struct RunsUntilStopped {
    void operator()(std::stop_token token) const {
        std::printf("task: waiting for a stop request...\n");
        std::fflush(stdout);
        while (!token.stop_requested())
            std::this_thread::sleep_for(10ms);
        std::printf("task: stop observed\n");
    }
};
}

static_assert(threadsafe::launchable_scoped_task<RunsUntilStopped>);

int main() {
    std::jthread watchdog{[](std::stop_token watchdog_token) {
        for (int slice = 0; slice < 200 && !watchdog_token.stop_requested(); ++slice)
            std::this_thread::sleep_for(10ms);
        if (!watchdog_token.stop_requested()) {
            std::printf("WATCHDOG: launch_scoped_task has not returned after 2 s -- deadlock\n");
            std::fflush(stdout);
            std::_Exit(42);
        }
    }};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(RunsUntilStopped{});

    std::printf("launch_scoped_task returned\n");
    watchdog.request_stop();
}
```

Exécution, reproduite ici à l'identique :

```
task: waiting for a stop request...
WATCHDOG: launch_scoped_task has not returned after 2 s -- deadlock
exit=42
```

`launch_scoped_task returned` **n'est jamais affiché**.

C'est celui des trois défauts critiques du lanceur qui est le plus embarrassant sur scène,
parce que la classe porte un `static_assert` qui **encourage** l'écriture de tâches à
`stop_token`, et que toutes ces tâches interbliquent — silencieusement, définitivement,
sans diagnostic ni délai de garde. `launch_scoped_task` est de surcroît le **seul** point
d'entrée qui accepte un argument emprunté, donc c'est celui vers lequel un utilisateur est
poussé pour du fan-out sur des données partagées.

**Correctif complet, statut `suite-passes`.** Ajouter `threadsafe::scoped_task_group` dans
`include/threadsafe/details/asynchronous_task_launcher.h`, immédiatement après le `};` de
`class asynchronous_task_launcher` et avant le `}` de `namespace threadsafe` :

```cpp
// The scoped counterpart of the launcher: the same borrow rule as
// launch_scoped_task — every argument must outlive the group — but the tasks
// overlap, the stop source stays reachable, and the joins happen once, at the
// end of the scope.
class scoped_task_group {
public:
    scoped_task_group() = default;

    scoped_task_group(const scoped_task_group&) = delete;
    scoped_task_group& operator=(const scoped_task_group&) = delete;

    // PRECONDITION: every borrowed argument must outlive this group.
    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }

    template <typename F, typename... Args>
    void launch(F, Args...) {
        detail::explain_launch_scoped_task<F, Args...>();
    }

    void request_stop() noexcept {
        for (std::jthread& task : threads_)
            task.request_stop();
    }

    void join_all() {
        for (std::jthread& task : threads_)
            if (task.joinable())
                task.join();
        threads_.clear();
    }

    void stop_and_join_all() {
        request_stop();
        join_all();
    }

    [[nodiscard]] std::size_t task_count() const noexcept {
        return threads_.size();
    }

    // Joining is the whole point: the borrows end here, so the group waits for
    // every task before it lets the scope close.
    ~scoped_task_group() { join_all(); }

private:
    std::vector<std::jthread> threads_;
};

template <>
struct is_sendable<scoped_task_group> : std::false_type {};
template <>
struct is_lifetime_aware<scoped_task_group> : std::false_type {};
```

et étendre le commentaire au-dessus de `launch_scoped_task` pour dire la vérité :

```cpp
    // Every call blocks until its own task is done, so N calls run one after
    // another and nothing can ever request the stop of the task being waited
    // for. Use scoped_task_group when the tasks should overlap or be
    // cancellable.
```

Avec `scoped_task_group`, la même `RunsUntilStopped` devient annulable :
`group.launch(RunsUntilStopped{});` puis `group.request_stop();`, le destructeur rejoint,
et le programme se termine.

**Contre-argument, à peser :** ajouter une classe entière à une bibliothèque
pédagogique est cher. La version minimale et honnête du correctif est de **deux lignes de
commentaire** — celles ci-dessus — plus, éventuellement, le renommage de
`launch_scoped_task` en quelque chose qui dise qu'il **bloque** (`run_scoped_task`,
`run_and_join`). Un lecteur qui voit `run_and_join(RunsUntilStopped{})` comprend
immédiatement pourquoi ça ne finit jamais. Si une seule chose doit être faite ici, c'est
celle-là.

### C.5 L7 — la précondition documentée de `launch_scoped_task` n'est vérifiable par rien

L'en-tête écrit :

```cpp
    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it does
    // not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
```

Rien ne la vérifie, et **rien dans les traits ne le pourrait** : l'emprunt est stocké dans
un pointeur de portée namespace que la marche structurelle n'inspecte jamais.

```cpp
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <functional>
#include <memory>

namespace {
// A type the user vouched for, so std::ref of it may cross into a scoped task.
struct Reading { int millivolts = 0; };
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Reading);

namespace {
// launch_scoped_task's PRECONDITION is "f must not store a reference beyond the
// call". Nothing checks it, and nothing in the traits could: the borrow is
// stashed in a global the structural walk never inspects.
Reading *escaped_reading = nullptr;

struct StashesTheBorrow {
    void operator()(Reading &reading) const { escaped_reading = &reading; }
};

void publish_a_reading() {
    auto reading = std::make_unique<Reading>(Reading{.millivolts = 1234});
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(StashesTheBorrow{}, std::ref(*reading));
    std::printf("while it is alive: escaped_reading->millivolts = %d\n",
                escaped_reading->millivolts);
    // reading dies here; escaped_reading keeps pointing at the freed storage.
}
}

static_assert(threadsafe::launchable_scoped_task<StashesTheBorrow,
                                                 std::reference_wrapper<Reading>>,
              "launch_scoped_task accepts it: the precondition is unchecked");

int main() {
    publish_a_reading();

    // Reuse the freed storage.
    auto replacement = std::make_unique<Reading>(Reading{.millivolts = -9999});
    std::printf("a new Reading{-9999} was allocated at %s address\n",
                replacement.get() == escaped_reading ? "THE SAME" : "a different");
    std::printf("after the free:    escaped_reading->millivolts = %d\n",
                escaped_reading->millivolts);
    return escaped_reading->millivolts == 1234 ? 1 : 0;
}
```

```
while it is alive: escaped_reading->millivolts = 1234
a new Reading{-9999} was allocated at a different address
after the free:    escaped_reading->millivolts = 0
```

et avec `-fsanitize=address` :

```
==79987==ERROR: AddressSanitizer: heap-use-after-free on address 0x6020000000d0 ...
READ of size 4 at 0x6020000000d0 thread T0
    #0 ... in main r06_scoped_dangling.cpp:45
freed by thread T0 here:
    #1 ... std::unique_ptr<Reading>::~unique_ptr()
    #2 ... in main r06_scoped_dangling.cpp:39
SUMMARY: AddressSanitizer: heap-use-after-free r06_scoped_dangling.cpp:45 in main
```

`launch_scoped_task` est délibérément la porte **faible** : elle abandonne
`is_lifetime_aware` précisément pour que `std::ref` et les pointeurs bruts vers des types
bénis puissent traverser, et **toute** la justification de cet abandon est la précondition.
Comme la précondition n'est pas exprimable dans le système de traits tel qu'il est conçu,
la seule API de la bibliothèque qui laisse un emprunt franchir une frontière de thread est
**exactement aussi sûre qu'un `std::thread` nu**.

**Correctif : aucun pour la vérification.** Ce que l'on peut faire, et ce que je
recommande, c'est de **cesser d'afficher la précondition comme si elle était vérifiée** :
la placer au même rang de « contrat non vérifié » que
`THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE`, y compris dans le **nom** si possible, pour que
le public sache laquelle des deux portes de la bibliothèque est portante et laquelle est
une promesse. C'est une phrase de documentation, pas du code — et c'est précisément le
genre d'aveu qui rend une conférence crédible.

### C.6 L5 — l'arrêt coûte la **somme** des temps de réaction, pas le **max**

*La prémisse habituelle est fausse dans le détail, et il faut le corriger :* le destructeur
**demande bien** l'arrêt. `threads_` est un `std::vector<std::jthread>` sans destructeur
utilisateur ; `~vector` détruit ses éléments **dans l'ordre**, et chaque `~jthread` fait
`request_stop()` puis `join()` **pour son propre thread**. Le vrai défaut, tout aussi
grave, est que l'arrêt est **sérialisé** : la tâche k+1 n'est même pas *prévenue* tant que
la tâche k n'a pas rejoint.

```cpp
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <stop_token>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using clock_type = std::chrono::steady_clock;

namespace {
clock_type::time_point program_start;

double elapsed_ms() {
    return std::chrono::duration<double, std::milli>(clock_type::now() - program_start).count();
}

// A realistic cooperative worker: it finishes a 250 ms chunk of work before it
// looks at the stop token again.
struct ChunkedWorker {
    int identifier;
    void operator()(std::stop_token token) const {
        while (!token.stop_requested())
            std::this_thread::sleep_for(250ms);
        std::printf("    [%7.1f ms] worker %d saw the stop\n", elapsed_ms(), identifier);
    }
};
}

int main() {
    constexpr int worker_count = 4;

    std::printf("A) threadsafe::asynchronous_task_launcher destructor\n");
    program_start = clock_type::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int identifier = 0; identifier < worker_count; ++identifier)
            launcher.launch_task(ChunkedWorker{identifier});
        std::this_thread::sleep_for(20ms);
        std::printf("    [%7.1f ms] leaving the scope\n", elapsed_ms());
    }
    std::printf("    [%7.1f ms] destructor returned\n\n", elapsed_ms());

    std::printf("B) bare vector<jthread>, stop requested on ALL of them first\n");
    program_start = clock_type::now();
    {
        std::vector<std::jthread> workers;
        for (int identifier = 0; identifier < worker_count; ++identifier)
            workers.emplace_back(ChunkedWorker{identifier});
        std::this_thread::sleep_for(20ms);
        std::printf("    [%7.1f ms] leaving the scope\n", elapsed_ms());
        for (std::jthread &worker : workers)
            worker.request_stop();
    }
    std::printf("    [%7.1f ms] all joined\n", elapsed_ms());
}
```

Mes mesures, identiques à celles de l'agent :

```
A) threadsafe::asynchronous_task_launcher destructor
    [   25.1 ms] leaving the scope
    [  255.1 ms] worker 0 saw the stop
    [  510.2 ms] worker 1 saw the stop
    [  765.1 ms] worker 2 saw the stop
    [ 1020.1 ms] worker 3 saw the stop
    [ 1020.2 ms] destructor returned

B) bare vector<jthread>, stop requested on ALL of them first
    [   25.1 ms] leaving the scope
    [  251.8 ms] worker 1 saw the stop
    [  251.8 ms] worker 0 saw the stop
    [  255.1 ms] worker 3 saw the stop
    [  255.1 ms] worker 2 saw the stop
    [  255.1 ms] all joined
```

**1020 ms contre 255 ms pour quatre workers.** La latence d'arrêt croît **linéairement**
avec le nombre de tâches : 32 workers avec une granularité de scrutation d'une seconde
mettent une demi-minute à fermer une portée. Corroboré par un second scénario (8 tâches,
scrutation 50 ms) : destructeur seul **310 / 425 ms**, contre **40 / 40 ms** avec un
`stop_source` propre passé en argument, et **42 / 40 ms** avec des `jthread` nus et une
diffusion.

Et l'API ne laisse **rien** faire de mieux. Une sonde de surface passe aujourd'hui :

```cpp
static_assert(!has_request_stop<launcher> && !has_join<launcher> && !has_wait<launcher>
              && !has_stop_source<launcher> && !has_size<launcher>,
              "the whole public surface is launch_task / launch_scoped_task");
```

Pas de `request_stop` à diffuser, pas de `join_all` à attendre, pas de `task_count` à
inspecter, pas de `future` pour ramener un résultat **ou une exception** — une tâche qui
lève termine le programme :

```
terminate called after throwing an instance of 'std::runtime_error'
  what():  task failed
exit=134
```

Le contournement que le corpus a trouvé — faire passer son propre `std::stop_source` en
argument, parce que `is_sendable<std::stop_source>` est vrai — est **10× plus rapide** et
vide le wrapper de son intérêt.

**Correctif complet, statut `suite-passes`.** Dans
`include/threadsafe/details/asynchronous_task_launcher.h`, ajouter `#include <exception>`
et `#include <future>`, et remplacer le corps de `class asynchronous_task_launcher` par
ceci (les quatre nouveaux membres, plus le destructeur et les membres spéciaux que ce
destructeur supprimerait sinon) :

```cpp
    asynchronous_task_launcher() = default;

    // A launcher owns running threads; copying it has no meaning, and letting
    // the copy be *declared* only moves the error into libstdc++.
    asynchronous_task_launcher(const asynchronous_task_launcher&) = delete;
    asynchronous_task_launcher&
    operator=(const asynchronous_task_launcher&) = delete;

    asynchronous_task_launcher(asynchronous_task_launcher&&) noexcept = default;

    asynchronous_task_launcher&
    operator=(asynchronous_task_launcher&& other) noexcept {
        if (this != &other) {
            request_stop();
            threads_ = std::move(other.threads_);
        }
        return *this;
    }

    // Broadcast the stop *before* the first join. ~vector<jthread> destroys its
    // elements one at a time, and each ~jthread requests the stop only for its
    // own thread and then joins it, so task k+1 is not even asked to stop until
    // task k has finished: shutdown costs the SUM of the tasks' reaction times
    // instead of the MAX. One broadcast pass first is what makes it the MAX.
    ~asynchronous_task_launcher() { request_stop(); }

    // Ask every task to stop, without waiting for any of them. Idempotent, and
    // safe to call on a launcher that has already been joined.
    void request_stop() noexcept {
        for (std::jthread& task : threads_)
            task.request_stop();
    }

    // Wait for every task to finish on its own terms. No stop is requested: a
    // task that runs to completion is allowed to.
    void join_all() {
        for (std::jthread& task : threads_)
            if (task.joinable())
                task.join();
        threads_.clear();
    }

    // The shutdown a user actually wants: one broadcast pass, then one join
    // pass, so the wait is the slowest task rather than their sum.
    void stop_and_join_all() {
        request_stop();
        join_all();
    }

    [[nodiscard]] std::size_t task_count() const noexcept {
        return threads_.size();
    }

    // The same task, with its result handed back. The future is the only way to
    // observe either the value or the exception: a task that throws on its own
    // thread otherwise calls std::terminate.
    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    [[nodiscard]] std::future<detail::launch_result_t<F, Args...>>
    launch_task_with_result(F f, Args... args) {
        using result_type = detail::launch_result_t<F, Args...>;

        std::promise<result_type> promise;
        std::future<result_type> result = promise.get_future();

        threads_.emplace_back(
            [](std::stop_token token, std::promise<result_type> own_promise,
               F callable, Args... arguments) {
                try {
                    if constexpr (std::invocable<F, std::stop_token, Args...>) {
                        if constexpr (std::is_void_v<result_type>) {
                            callable(std::move(token), std::move(arguments)...);
                            own_promise.set_value();
                        } else {
                            own_promise.set_value(callable(
                                std::move(token), std::move(arguments)...));
                        }
                    } else {
                        if constexpr (std::is_void_v<result_type>) {
                            callable(std::move(arguments)...);
                            own_promise.set_value();
                        } else {
                            own_promise.set_value(
                                callable(std::move(arguments)...));
                        }
                    }
                } catch (...) {
                    own_promise.set_exception(std::current_exception());
                }
            },
            std::move(promise), std::move(f), std::move(args)...);

        return result;
    }

    template <typename F, typename... Args>
    void launch_task_with_result(F, Args...) {
        detail::explain_launch_task<F, Args...>();
    }
```

et, dans `namespace threadsafe::detail` (à côté de `explain_launch_task`), l'auxiliaire de
type de résultat dont il a besoin :

```cpp
// The result the task produces, asked of whichever call shape jthread will use.
template <class F, class... Args>
consteval auto launch_result_type() {
    if constexpr (std::invocable<F, std::stop_token, Args...>)
        return ^^std::invoke_result_t<F, std::stop_token, Args...>;
    else
        return ^^std::invoke_result_t<F, Args...>;
}

template <class F, class... Args>
using launch_result_t = [: launch_result_type<F, Args...>() :];
```

Vérifié de bout en bout : `launch_task_with_result(ProducesAnInt{6}, 7).get() == 42` ;
l'exception d'une tâche qui lève arrive à `future::get()` au lieu de `std::terminate` ;
`stop_and_join_all()` laisse `task_count() == 0`. Après correctif, le scénario A rend la
main à **255,2 ms**, les quatre workers voyant l'arrêt à 255,1 ms.

**Challenge du besoin.** Ce correctif est le plus **gros** du rapport et le plus discutable
pour une bibliothèque pédagogique. Il se scinde nettement en deux :

- **`~asynchronous_task_launcher() { request_stop(); }`** — **une ligne**, plus les cinq
  membres spéciaux que déclarer un destructeur oblige à réécrire. Elle transforme 1020 ms
  en 255 ms et ne change **rien** à l'API publique. **À prendre.**
- **`request_stop` / `join_all` / `stop_and_join_all` / `task_count` /
  `launch_task_with_result`** — c'est un vrai *task group*, avec `<future>` en dépendance
  supplémentaire (et [06](./06-performance-compilation.md) montre que le coût de compilation
  de cette bibliothèque est **le parsing des en-têtes standard**, pas la réflexion : ajouter
  `<future>` n'est pas gratuit). Sur scène, `launch_task_with_result` distrait du sujet, qui
  est le système de traits. **Optionnel, et probablement à ne pas prendre** — sauf si un des
  six programmes de démonstration en a besoin.

### C.7 L11 et L10 — deux défauts de diagnostic, dont un seul est un défaut

**L11 (à corriger).** `launch_task(F f, Args... args)` prend **par valeur**. Un callable
non-movable en **prvalue** est élidé directement dans le paramètre et atteint bien la
surcharge explicative ; un **lvalue** ou un xvalue doit d'abord être move-construit, ce qui
échoue **avant** que le corps ne s'exécute. L'utilisateur voit alors le `use of deleted
function` brut :

```cpp
#include <threadsafe/threadsafe.h>
#include <utility>

struct NonMovable {
    NonMovable() = default;
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
    void operator()() const {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    NonMovable f;
    launcher.launch_task(std::move(f));
}
```

Diagnostic reproduit ici :

```
d01b_nonmovable_lvalue.cpp:14:25: error: use of deleted function 'NonMovable::NonMovable(NonMovable&&)'
   14 |     launcher.launch_task(std::move(f));
      |     ~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~
.../asynchronous_task_launcher.h:97:22: note: initializing argument 1 of 'void threadsafe::asynchronous_task_launcher::launch_task(F, Args ...) [with F = NonMovable; Args = {}]'
```

alors que la **même** intention écrite `launcher.launch_task(NonMovable{})` donne le beau
message :

```
'what()': 'the launcher owns its callable, so a non-movable one cannot cross;
 share it with std::ref instead'
```

Le beau message **est** l'argument de vente de la bibliothèque, et l'obtenir dépend de si
l'on a écrit `NonMovable{}` ou `std::move(f)` — une distinction qui n'a aucun sens pour un
utilisateur. La variante *« le lanceur vide l'objet de l'appelant »* est pire que
cosmétique : `launcher.launch_scoped_task<SharedCounterHandle&>(handle)` vide silencieusement
le handle de l'appelant **sans aucun `std::move` sur le site d'appel** —
`before: box.cell = 0xb96c00910` / `after: box.cell = 0x0`.

Correctif complet, statut `suite-passes` : remplacer les quatre surcharges par des formes à
référence universelle, pour qu'aucun paramètre ne soit initialisé avant l'entrée dans le
corps et qu'aucun `F = T&` explicite n'atteigne le `std::move`.

```cpp
    // Forwarding references, not by-value parameters: a by-value parameter is
    // initialized before the body is entered, so a non-movable callable dies in
    // that initialization -- "use of deleted function" -- and the explaining
    // fallback never runs. Deducing F&& also means an explicit F = T& cannot
    // turn the body's std::move into a move out of the CALLER's object.
    // std::jthread decay-copies what it is handed, so ownership is unchanged.
    template <typename F, typename... Args>
        requires launchable_task<std::remove_cvref_t<F>,
                                 std::remove_cvref_t<Args>...>
    void launch_task(F&& f, Args&&... args) {
        threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...);
    }

    // Fallback: the constrained overload is more constrained, so it always wins
    // when it applies. This one is instantiated only on a rejection, and exists
    // only to name it — instantiating it is always a compile error.
    template <typename F, typename... Args>
    void launch_task(F&&, Args&&...) {
        detail::explain_launch_task<std::remove_cvref_t<F>,
                                    std::remove_cvref_t<Args>...>();
    }

    template <typename F, typename... Args>
        requires launchable_scoped_task<std::remove_cvref_t<F>,
                                        std::remove_cvref_t<Args>...>
    void launch_scoped_task(F&& f, Args&&... args) {
        std::jthread task{std::forward<F>(f), std::forward<Args>(args)...};
        task.join();
    }

    template <typename F, typename... Args>
    void launch_scoped_task(F&&, Args&&...) {
        detail::explain_launch_scoped_task<std::remove_cvref_t<F>,
                                           std::remove_cvref_t<Args>...>();
    }
```

Les deux surcharges ont désormais les **mêmes** types de paramètres, donc l'ordonnancement
partiel retombe sur les contraintes et la surcharge contrainte gagne toujours là où elle
s'applique. Après correctif, la forme lvalue produit le beau message, la variante
« vidage » est **rejetée à la compilation**, et l'objet de l'appelant reste intact.
C'est un excellent rapport bénéfice/coût : le correctif est purement local et rend le
message de la bibliothèque **fiable**, ce qui est ce qu'on vient voir.

**L10 (à ne pas corriger, à documenter).** L'astuce à deux surcharges — une contrainte, une
non contrainte qui n'existe que pour lever une `std::meta::exception` — a été attaquée de
quatre façons pour la faire dégrader en `false` plutôt qu'en erreur dure : expression
`requires` nue, concept écrit par l'utilisateur, `std::is_invocable_v` sur une lambda
enveloppante, appel explicitement instancié dans un opérande non évalué. **Aucune** n'a
produit `false`. Le procédé tient. Le prix inévitable est que `requires { launcher.launch_task(x); }`
est **vrai pour tout `x`** :

```cpp
#include <threadsafe/threadsafe.h>

#include <functional>
#include <string>

namespace {
struct Bad { int* borrowed; void operator()() const {} };
}

using threadsafe::asynchronous_task_launcher;

// (a) The concept is the authority and it says no.
static_assert(!threadsafe::launchable_task<Bad>, "Bad is genuinely rejected by the concept");

// (b) The EXPRESSION, however, is well-formed: the fallback is viable and
//     unconstrained, so it wins and the requires-expression is satisfied.
static_assert(requires(asynchronous_task_launcher l) { l.launch_task(Bad{}); },
              "the requires-expression is satisfied even though the task is unsafe");
static_assert(requires(asynchronous_task_launcher l) { l.launch_scoped_task(Bad{}); });

// (c) A user's own detection idiom therefore reports 'launchable'.
template <class F, class... A>
concept looks_launchable =
    requires(asynchronous_task_launcher l, F f, A... a) { l.launch_task(f, a...); };

static_assert(looks_launchable<Bad>, "detection idiom lies");
static_assert(looks_launchable<int>, "even a non-callable 'looks launchable'");
static_assert(looks_launchable<std::string, int*>);
static_assert(looks_launchable<std::function<void()>>);

// (d) zero arguments is not ambiguous: overload resolution picks the constrained one
static_assert(requires(asynchronous_task_launcher l) { l.launch_task([]{}); });

int main() {}
```

Le fichier compile proprement contre l'en-tête réel — **toutes** les assertions tiennent
(vérifié). Les deux propriétés sont **la même** propriété : une surcharge qui existe pour
produire un message doit être une candidate viable, et une candidate viable rend
l'expression bien formée. La rendre compatible SFINAE (`= delete`, ou une repli contraint)
rétablirait la détection honnête mais **supprimerait le message**, qui est la
fonctionnalité. **Résolution : une phrase de documentation** — `threadsafe::launchable_task`
et `launchable_scoped_task` sont les prédicats ; les fonctions membres ne sont
**délibérément pas** compatibles SFINAE. Aucun code à écrire, et c'est le bon arbitrage.

### C.8 Ce qui a résisté sur le lanceur

- `std::function<void()>` est rejeté par `launch_task` **et** par `launch_scoped_task`.
- Une lambda capturant un `int*` **par valeur** est rejetée.
- `std::ref` d'un type béni par `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` est rejeté par
  `launch_task`.
- `std::span`, `std::string_view`, `const char*` et les pointeurs bruts sont tous rejetés
  par `launch_task` ; `span` et `string_view` le sont aussi par `launch_scoped_task`.
- Un foncteur **sans état** mais avec un constructeur de copie écrit à la main est rejeté ;
  une classe avec un destructeur écrit à la main aussi.
- `synchronized_value` ne peut pas être envoyée par valeur (ni copiable ni déplaçable), et
  `std::ref` d'une `synchronized_value` est rejetée avec un message exact.
- `asynchronous_task_launcher` **lui-même** n'est ni sendable, ni synchronizable, ni même
  const-synchronizable, et ne peut pas être déplacé dans une tâche.
- Le repli à deux surcharges est **le bon idiome** et il fonctionne : la surcharge
  contrainte n'est jamais sélectionnée pour un mauvais type, et `launch_scoped_task`
  explique aussi bien que `launch_task`.
- Sur les quatre sondes de rejet attendues, **trois ont tenu**. La quatrième est L1.

---

## D. Les trois helpers côte à côte

Chaque case répond à la question « **cette chose est-elle possible aujourd'hui, sur un
programme que la bibliothèque déclare sûr à la compilation ?** » Le finding qui l'établit
est nommé, et chaque cellule « oui » est adossée à une exécution reproduite dans ce
rapport.

| | course de données possible | *race condition* (logique) possible | référence pendante possible | manque d'API |
|---|---|---|---|---|
| **`copy_on_write<T>`** | **oui** — COW-01 : `is_synchronizable<const T>` ne voit pas les corps `const` ; 134 172 incréments perdus sur 160 000, et **aucun** mutex de secours | **non** — le type n'expose aucune séquence à casser (rien à publier, donc rien à publier de travers) | **oui** — COW-02 : la `T&` d'`as_mutable()` se retarge vers le snapshot ou dangle ; ASan *heap-use-after-free* en monothread, `-O2`, sans diagnostic | **oui** — **aucun canal de publication** (§A.5), et le type ne peut pas se contenir lui-même (COW-03) |
| **`synchronized_value<T>`** | **oui** — SV-01 : le `shared_mutex` est choisi sur la même preuve structurelle ; 579 incréments perdus sur 160 000, TSan confirme sur la forme `const_cast`. *Sinon, le corps du wrapper est sans course.* | **oui** — SV-02 : check-then-act entre deux `lock()` (844 / 2000 exécutions dupliquées) **et** interblocage à l'imbrication (3/3) | **oui** — SV-03 : `operator*() const&` blanchi par un paramètre `const&` ; `exit=134` ×3, ASan *heap-use-after-free*. Plus SV-05 (garde forgée) | **oui** — pas de `with`, pas de verrouillage multiple, mutex non surchargeable (§B.6), divergence ODR silencieuse entre TU |
| **`asynchronous_task_launcher`** | **oui** — L1 (`static` membre, fermable), L2 (réentrance, non fermable), handle thread-affine (non fermable) ; `exit=133/134/138/139` selon le cas | **oui** — L3 : interblocage **permanent** de toute tâche coopérative, `exit=42` | **oui** — L7 : la précondition de `launch_scoped_task` n'est vérifiable par aucun trait ; ASan *heap-use-after-free* | **oui** — L5 : ni `request_stop`, ni `join_all`, ni `task_count`, ni résultat ni exception observables ; L10 : les membres ne sont pas détectables |

**Lecture d'ensemble.** Toutes les cases « course de données » de la première colonne
remontent à **une** cause : `is_synchronizable<const T>` est une preuve **structurelle**
qu'on fait parler d'**exécution**. C'est le sujet de [01](./01-robustesse-des-traits.md), et
c'est aussi la meilleure diapositive de la conférence — à condition d'être énoncée, ce que
ni `CLAUDE.md` ni aucun en-tête ne fait aujourd'hui.

### Ce que je ferais, dans l'ordre

1. **Écrire la limite.** Une phrase dans `CLAUDE.md` et une à côté de `cow_is_sendable` :
   *la réflexion raisonne sur des déclarations, jamais sur des corps de fonction.* Zéro
   ligne de code, zéro régression, et cela transforme trois « bugs » en une **propriété
   assumée**.
2. **L11** — références universelles sur les quatre surcharges du lanceur. Purement local,
   `suite-passes`, et cela rend fiable le message qui est l'argument de vente.
3. **SV-04, SV-05, SV-06** — trois correctifs de 3 à 6 lignes, tous `suite-passes`, tous
   fermant un trou que la bibliothèque reproche déjà aux types des autres.
4. **`~asynchronous_task_launcher() { request_stop(); }`** (L5, moitié basse) — une ligne,
   1020 ms → 255 ms.
5. **SV-02 (`with` / `with_all`)** — le seul ajout d'API que je défendrais sans réserve :
   il rend l'orthographe sûre la plus courte, et supprime la pression qui pousse
   aujourd'hui l'utilisateur dans la course.
6. **Reformuler** le message des opérateurs supprimés (SV-03) et la précondition de
   `launch_scoped_task` (L7) pour qu'ils cessent de promettre ce qu'ils ne tiennent pas.
7. **Arbitrer** SV-01 : opt-in `shares_const_reads` (correct, +1 trait) *ou* mutex explicite
   en paramètre de template (correct, −1 magie, et cela supprime en prime la divergence
   ODR). Les deux sont défendables ; l'immobilisme ne l'est pas.
8. **Décider** de COW-01 et COW-03 en connaissance de cause : les deux correctifs
   « corrects » coûtent quelque chose de réel (un faux rejet du cas courant pour COW-01, un
   facteur 9 en lecture pour COW-03).

Ce qui **ne devrait pas** être fait : ajouter un mutex dans le lanceur pour rendre L2
« memory-safe » (cela bénirait la réentrance au lieu de la rejeter), rendre les membres du
lanceur compatibles SFINAE (cela supprimerait le message), et livrer
`launch_task_with_result` (`<future>` en dépendance, hors sujet pour une conférence sur les
traits).

---

*Rapports voisins : [00](./00-synthese.md) · [01](./01-robustesse-des-traits.md) ·
[03](./03-couverture-de-tests.md) · [04](./04-diagnostics.md) · [05](./05-simplicite.md) ·
[06](./06-performance-compilation.md) · [07](./07-performance-execution.md) ·
[08](./08-api-et-flexibilite.md) · [09](./09-methodologie.md)*

*Toolchain : GCC 16.2.0 (Homebrew), `-std=c++26 -freflection`. Apple clang 21.0.0 pour les
exécutions TSan (pas de réflexion, d'où les extractions diffées). Machine : Apple M3 Pro
(6 cœurs performance + 6 efficacité), macOS 26.6.2. Toutes les mesures de temps à `-O2`
sauf mention contraire. Rien sous `include/` ni `tests/` n'a été modifié ; tous les
correctifs ont été essayés sur des copies.*
