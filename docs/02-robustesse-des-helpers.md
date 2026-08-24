# Robustesse des helpers

`copy_on_write<T>`, `synchronized_value<T>` / `value_guard`, et
`asynchronous_task_launcher`. Comme pour les traits, chaque défaut a été soumis à
un vérificateur indépendant chargé de le réfuter.

| # | défaut | helper | verdict | sévérité | nature |
|---|---|---|---|---|---|
| 1 | une `T&` échappée survit à l'unicité qui la justifiait | `copy_on_write` | confirmé, surévalué | **élevée** | **inhérent** |
| 2 | une poignée déplacée fait planter `as_mutable()` | `copy_on_write` | confirmé | **élevée** | bug |
| 3 | une référence échappe du verrou par `const&` | `value_guard` | confirmé, surévalué | moyenne | bug (classe inhérente) |
| 4 | `synchronized_value<T&>` explose trois niveaux plus bas | `synchronized_value` | confirmé | moyenne | bug |
| 5 | aucun verrouillage multiple ordonné : interblocage immédiat | `synchronized_value` | confirmé | moyenne | manque |
| 6 | `copy_on_write` n'est pas composable (`const` non réglé) | `copy_on_write` | confirmé, surévalué | moyenne | bug |
| 7 | la précondition de `launch_scoped_task` est violable | launcher | confirmé | moyenne | **inhérent** |

---

## 1. La référence échappée — le problème central, et il est inhérent

`as_mutable()` rend une `T&` brute dans le bloc partagé. La preuve « ce handle est
unique » sur laquelle elle repose est invalidée par **la copie suivante du
handle** :

```cpp
Doc& reference = document.as_mutable();   // légitime : le handle est unique
copy_on_write<Doc> snapshot = document;   // le bloc devient partagé...
// `reference` écrit maintenant dans un bloc que `snapshot` lit.
```

Reproduit sous TSan, toutes les assertions tenant, sans `const_cast`, sans
`new`/`delete`, sans macro `UNSAFE`.

Une seconde forme, encore plus déroutante, se reproduit **sans aucun thread** :

```
document : original
snapshot : original-MUTATED     <-- écrit à travers la référence échappée
escaped aliases snapshot: 1
escaped aliases document: 0
```

Après un second `as_mutable()`, `document` a migré vers un nouveau bloc et la
référence gardée aliase silencieusement l'objet de `snapshot` — dont toute la
surface publique est pourtant `const`.

### Pourquoi c'est inhérent, et pourquoi le correctif proposé ne suffit pas

Le vérificateur a écrit un **témoin** : le même programme, octet pour octet, mais
où la `T&` ne survit pas à l'expression qui l'a produite. Il est **propre sous
TSan**. Le design de `copy_on_write` — la décision de détachement, la barrière, le
trait — n'est donc pas en cause :

> Le défaut n'est pas « `copy_on_write<std::string>` est dangereux à envoyer ».
> C'est « une `T&` liée hors de `as_mutable()` et utilisée après une copie du
> handle est dangereuse » — l'analogue en concurrence de
> `int& r = v[0]; v.push_back(x); r = 1;`.

Le correctif proposé (remplacer `as_mutable()` par un `modify(callable)` scopé) a
été appliqué : il casse deux lignes de `test_copy_on_write.cpp`, et surtout **il
ne ferme pas le trou** — le vérificateur l'a défait avec une lambda identité qui
laisse fuir la référence, et TSan signale à nouveau la course.

C'est la même conclusion que pour `value_guard` (§3) : **aucun type C++ ne peut
borner la durée de vie d'une référence qu'il rend**. Rust y arrive parce que
`&mut self` emprunte l'`Arc` pour la durée de la `&mut T` rendue ; C++ n'a pas
cette règle. La bibliothèque le concède déjà ailleurs, dans la précondition de
`launch_scoped_task` : « The traits cannot check this ».

**Recommandation :** garder `as_mutable()`, et documenter la règle en une ligne au
point d'usage — *la référence rendue est valide jusqu'à la prochaine copie du
handle*. Un `modify()` scopé peut être ajouté **en plus** comme forme
recommandée, mais il ne doit pas être présenté comme une garantie.

---

## 2. Une poignée déplacée fait planter `as_mutable()` — élevée, bug

Le constructeur de déplacement de `copy_on_write` est implicite, donc une poignée
déplacée détient un `shared_ptr` nul. `use_count()` rend alors **0**, jamais 1 :
`as_mutable()` prend la branche copiante et évalue `*ptr_` sur `nullptr`.

Aucun code exotique n'est requis — un `std::remove_if` suffit :

```cpp
#include <threadsafe/threadsafe.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using threadsafe::copy_on_write;

int main() {
    std::vector<copy_on_write<std::string>> documents;
    documents.emplace_back(std::string("alpha"));
    documents.emplace_back(std::string("beta"));
    documents.emplace_back(std::string("gamma"));

    // std::remove_if MOVES the survivors forward; the tail holds moved-from
    // handles. Nothing here is unusual C++.
    auto tail = std::remove_if(documents.begin(), documents.end(),
                               [](const copy_on_write<std::string>& doc) {
                                   return *doc == "beta";
                               });
    std::printf("kept %zu of 3\n", static_cast<std::size_t>(tail - documents.begin()));
    std::printf("moved-from operator-> == %p\n",
                static_cast<const void*>(documents.back().operator->()));
    std::printf("about to call as_mutable() on a moved-from handle\n");
    std::fflush(stdout);
    documents.back().as_mutable() += "!";
    std::printf("survived\n");
}
```

Observé :

```
kept 2 of 3
moved-from operator-> == 0x0
about to call as_mutable() on a moved-from handle
exit=139                            <-- SIGSEGV
```

### Correctif — vérifié

Le déplacement d'un `copy_on_write` n'apporte rien : copier, c'est incrémenter un
compteur de références. Supprimer l'état vide en déclarant les membres de copie,
ce qui supprime les membres de déplacement implicites. À ajouter dans la classe,
juste après le constructeur variadique :

```cpp
    // Copying is a reference count increment, which is already what a move
    // would cost. Declaring the copy members suppresses the implicit move
    // members, so a moved-from handle -- whose null ptr_ makes use_count()
    // return 0, sending as_mutable() down the copying branch to dereference
    // nullptr -- cannot exist. An rvalue binds to the copy constructor, and
    // std::move_constructible still holds.
    copy_on_write(const copy_on_write&) = default;
    copy_on_write& operator=(const copy_on_write&) = default;
```

Vérifié : le programme ci-dessus affiche désormais `survived` et sort en 0 ; les
onze TU passent ; et `std::move_constructible`, `std::copy_constructible`,
`std::is_nothrow_move_constructible_v`, `is_sendable_v`, `is_lifetime_aware_v` et
`launchable_task` sont tous préservés.

---

## 3. `value_guard` : une référence échappe encore par `const&`

Le commit `e760aa5` a supprimé `operator*` et `operator->` sur *rvalue*, avec un
message excellent qui s'affiche verbatim dans le diagnostic. Cette barrière est
efficace contre toutes les écritures directes — y compris les deux que C++23 rend
subtiles : un `for` sur `*sv.lock()` (P2718 ne rattrape pas le cas) et une
liaison structurée `auto&& [a, b] = *sv.lock()`.

Il reste une porte : `operator*() const&` est appelable sur un temporaire lié à un
`const value_guard&`. `int& escaped = peek_under_lock(counter->lock());` compile
**sans aucun diagnostic**, y compris avec `-Wdangling-reference`. TSan confirme la
course, avec pertes de mises à jour (199 996 au lieu de 200 000 sur un run).

**Mais fermer cette porte ne restaure pas la garantie.** Le vérificateur atteint la
même course sans temporaire et sans `const&`, par l'idiome béni :

```cpp
{ auto guard = counter->lock(); escaped = &*guard; }   // puis on utilise `escaped`
```

Cela compile proprement avec `-Wall -Wextra`, TSan le signale, et **cela compile
toujours contre le correctif proposé**. La classe de fuite est donc inhérente,
exactement comme au §1.

**Recommandation :** fermer quand même la porte `const&` — c'est peu coûteux et
cela aligne le comportement sur le message que le code imprime déjà — mais
présenter la garantie pour ce qu'elle est : *rendre les erreurs courantes
difficiles*, pas *les rendre impossibles*.

---

## 4. `synchronized_value<T&>` — accepté, puis explose trois niveaux plus bas

`sendable<T&>` vaut `is_synchronizable<T>`, donc
`synchronized_value<std::atomic<int>&>` passe le seul `static_assert` de la
classe, s'instancie, et rapporte `is_synchronizable_v == true`. L'échec ne
survient qu'au premier `lock()`, sous la forme d'un « forming pointer to
reference type » dans `value_guard`.

### Correctif — vérifié, les 11 TU passent

Comme **premier** membre de `synchronized_value`, au-dessus du `static_assert`
existant :

```cpp
    static_assert(!std::is_reference_v<T>,
                  "synchronized_value stores its value; a reference member "
                  "would borrow, and the mutex would guard the borrow, not "
                  "the referent — wrap the referent instead");
```

---

## 5. Aucun verrouillage multiple ordonné

Deux `synchronized_value` verrouillés en ordre inverse par deux tâches
s'interbloquent en quelques microsecondes :

```
WATCHDOG: still blocked after 2s -> deadlock
```

Et l'utilisateur ne peut pas se rabattre sur `std::scoped_lock`, le mutex étant
privé sans accesseur :

```
error: 'threadsafe::synchronized_value<int>::mutex
        threadsafe::synchronized_value<int>::mutex_' is private within this context
```

C'est la limite du critère « difficile d'avoir des race conditions » posé par la
tâche : la bibliothèque rend les *data races* très difficiles, et ne dit rien des
*interblocages*. Pour un support pédagogique, c'est un point à énoncer
explicitement plutôt qu'à laisser découvrir. Un `with_all_locked(body, values...)`
ami, qui délègue à `std::scoped_lock` (donc à l'algorithme d'évitement
d'interblocage de la bibliothèque standard), comblerait le manque en une
vingtaine de lignes — à mettre en balance avec la volonté de garder la
bibliothèque petite.

---

## 6. `copy_on_write` n'est pas composable

`copy_on_write<T>` est spécialisé pour `is_sendable` et `is_lifetime_aware`, mais
**pas** pour `is_synchronizable<const …>`. La marche structurelle décide donc, et
le rejette sur son constructeur variadique templaté — bien que toute son
interface `const` soit précisément faite pour la lecture partagée.

Conséquence : `copy_on_write` ne peut être ni imbriqué, ni tenu par un type que la
marche `const` visite. Le type phare de la bibliothèque ne compose pas avec
elle-même.

```cpp
template <class T>
struct is_synchronizable<const copy_on_write<T>>
    : std::bool_constant<is_synchronizable_v<const T>> {};
```

---

## 7. La précondition de `launch_scoped_task` est violable, et c'est inhérent

L'en-tête est honnête :

```
// PRECONDITION: f must not outlive its own invocation ... The traits cannot
// check this; the join bounds the invocation, not the borrow.
```

Le vérificateur a mesuré à quel point la violation est bon marché : un appelable
sans capture qui range l'adresse de son argument `reference_wrapper` dans une
globale satisfait tous les traits et laisse un pointeur pendant après le `join`.
Sous AddressSanitizer :

```
ERROR: AddressSanitizer: stack-use-after-scope
WRITE of size 4 ... in main helpers_scoped_precondition.cpp:32
```

Aucun correctif : la seule vérification structurelle possible serait d'interdire
un appelable capable d'atteindre un état global mutable, ce que la réflexion ne
voit pas davantage. La valeur du résultat est de **savoir exactement** combien la
violation coûte, pour pouvoir le dire.

---

## Ce qui a résisté

- **La décision de détachement est saine**, et pour une raison plus forte que ne le
  laisse penser le code : un compteur de `shared_ptr` est exact, et à 1 ce thread
  détient la seule poignée qui pourrait le faire monter. Aucun faux « unique » en
  **120 000 tours** martelés contre quatre lecteurs concurrents.
- **La barrière acquire du commit `643e3f5` est exactement la bonne primitive dans la
  bonne branche.** Charge `use_count()` relaxée + barrière acquire, c'est le motif
  [atomics.fences]/3, et les RMW `acq_rel` du compteur de libstdc++ fournissent la
  moitié release gratuitement. Rien de plus n'est nécessaire, une ligne de moins
  serait faux. Coût mesuré : 0,29 ns/appel sans acquire, 0,69 ns/appel avec.
- **La clause `requires` du constructeur variadique** a résisté à toutes les tentatives
  de détournement de la copie : les quatre catégories de valeur, le pack vide, une
  classe dérivée, et `copy_on_write<copy_on_write<T>>`.
- **`value_guard` est étanche comme type** : constructeur privé avec un seul ami, `Lock`
  et `T*` privés, copie et déplacement supprimés, `is_sendable` et `is_lifetime_aware`
  spécialisés à faux. Aucun moyen d'en extraire le verrou.
- **Le repli explicatif du launcher ne peut jamais être choisi silencieusement** : les
  deux fonctions `explain_*` se terminent par un `throw` inconditionnel.
- **La récursion du launcher tient sous attaque soutenue** : pointeurs bruts enveloppés
  dans une structure, `reference_wrapper` enveloppés, conteneurs d'emprunts, conteneurs
  de conteneurs d'emprunts — tous rejetés, à deux et trois niveaux d'imbrication, à
  travers les bases comme à travers les membres.
- **Le choix `shared_mutex`/`mutex` est mesurablement réel** : 4 lecteurs sur 4 en
  parallèle pour `synchronized_value<std::vector<int>>`, 1 sur 4 pour un `T` à membre
  `mutable` — la dégradation silencieuse en mutex exclusif est la bonne réponse.
- **Les deux helpers sont gratuits à l'exécution** : `lock()` à 6,18 ns contre 6,19 ns
  écrit à la main, `launch_task` à 12,6 µs contre 13,0 µs pour un `std::jthread` nu.
