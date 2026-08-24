# Diagnostics — la chaîne de causes remonte jusqu'à la racine

## Le constat, et ce qu'il est devenu

Le commit `8fd42a8` (« Explain trait failures instead of printing a bare false »)
a doté la bibliothèque de trois fonctions `assert_*` qui nomment le sous-objet
fautif au lieu d'afficher un `false` nu. Le mécanisme — lever une
`std::meta::exception` depuis la marche `consteval`, la rattraper dans
`default_*` pour répondre `false`, la laisser s'échapper depuis `assert_*` pour
qu'elle devienne le diagnostic — est élégant et parfaitement adapté à un propos
pédagogique.

Mais l'attribution s'arrêtait **à un seul niveau**. Sur un type imbriqué :

```cpp
#include <threadsafe/threadsafe.h>
struct Inner  { int* borrowed; };
struct Middle { Inner inner; };
struct Outer  { Middle middle; };
consteval bool ask() { threadsafe::assert_sendable<Outer>(); return true; }
static_assert(ask());
```

le message obtenu était :

```
'what()': 'member `middle` of type Middle is not sendable'
```

`Middle` n'est pas la cause : c'est `Inner::borrowed`, un `int*`. L'utilisateur
devait relancer la compilation autant de fois qu'il y avait de niveaux. Pour du
code destiné à être lu en conférence, c'est précisément le moment où la
démonstration se casse.

**C'est corrigé.** Le message est aujourd'hui :

```
'what()': 'Outer::middle (Middle)::inner (Inner)::borrowed (int*) is a pointer
or a reference: sending it shares its referent with the other thread, so the
referent must be synchronizable — and synchronizability is opt-in'
```

## Le piège : la correction naïve coûte 38×

La correction évidente — rattraper l'exception du sous-objet et la préfixer,
systématiquement — fonctionne, mais elle est **catastrophique en temps de
compilation**. `default_*` appelle la même marche que `assert_*` et *jette* le
message ; construire la chaîne fait donc re-marcher chaque sous-objet à chaque
niveau, pour un texte que personne ne lira.

Mesuré sur une chaîne de 60 niveaux dont chaque niveau répond `false`
(`static_assert(!is_sendable_v<L0..L59>)`), meilleur de 3 exécutions :

| version | temps |
|---|---|
| référence | **753 ms** |
| chaîne naïve (toujours active) | **28 564 ms** — ×38 |

## La correction : le chemin est à la fois le fil d'Ariane et le garde-fou

Les trois marches prennent un paramètre de plus :

```cpp
consteval void diagnose_default_is_sendable(std::meta::info type,
                                            std::u8string path = {});
```

- **`path` vide** — l'appel vient du trait. La marche s'arrête au premier maillon
  comme avant, et ne redescend nulle part. Le trait ne paie rien : le ×38 est
  écarté *par construction*, pas par une option qu'on pourrait oublier
  d'activer.
- **`path` non vide** — l'appel vient d'un `assert_*`, seul lecteur du message,
  qui amorce le chemin avec le nom du type. Chaque échec de sous-objet rappelle
  la marche sur le type fautif avec le chemin allongé d'une étape, jusqu'au
  motif terminal.

Deux helpers par trait suffisent :

```cpp
// Coming back from the walk means `inner` answers false through a
// specialization the walk cannot read — that is itself the reason.
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);
    reject(inner, u8"is not sendable: is_sendable is specialized to false for it",
           path);
}

// Reject `subject`; while a path is being built, continue into `inner` instead.
[[noreturn]] inline consteval void explain_sendable(std::meta::info subject,
                                                    std::u8string_view reason,
                                                    std::meta::info inner,
                                                    const std::u8string &path) {
    if (path.empty())
        reject(subject, reason);
    descend_sendable(inner, path + path_step(subject));
}
```

et `assert_sendable` se réduit à amorcer le chemin :

```cpp
template <class T>
consteval void assert_sendable() {
    if (is_sendable_v<T>)
        return;
    detail::descend_sendable(^^T, detail::type_name(^^T));
}
```

`path_step` (dans `utils.h`) nomme une étape — `::membre (Type)` pour un membre,
`::(base Type)` pour une base, **rien** pour un tableau ou une orthographe
cv-qualifiée, qui désignent le même objet sous un autre nom. Et `reject` prend le
chemin : sa dernière étape nomme déjà le sujet, elle remplace donc `describe()`.

### Le coût, mesuré

Chaîne de 60 niveaux, les trois traits interrogés à chaque niveau, meilleur de 3 :

| version | temps |
|---|---|
| avant (en-têtes de `e1e8330`) | **0,95 s** |
| avec le chemin | **0,96 s** — dans le bruit |

La descente ne parcourt qu'une fois de plus **la chaîne fautive**, et seulement
depuis un `assert_*`. La réponse `false` du trait, elle, coûte exactement ce
qu'elle coûtait.

## Les messages obtenus

Les trois traits en bénéficient. Sorties réelles, toutes reproduites par
[`scenarios/harness/diag_path_nested.cpp`](./scenarios/harness/diag_path_nested.cpp) :

```
LifeTop::mid (LifeMid)::leaf (LifeLeaf)::borrowed (int*) is a reference or a raw
pointer: it borrows its referent instead of keeping it alive — hold the object,
or a std::shared_ptr to it
```

```
const SyncTopM::mid (SyncMidM)::leaf (SyncLeafMutable)::cached (int) is mutable,
so it is written through a const reference: its type must be fully
synchronizable
```

Une base apparaît comme telle, et un tableau n'ajoute pas d'étape :

```
Derived::(base Base)::borrowed (int*) is a pointer or a reference: sending it …
Arr::cells (IntPtr [3])::ptr (int*) is a pointer or a reference: sending it …
```

Un type dont la réponse vient d'une spécialisation le dit :

```
HoldsHandle::handle (Handle) is not sendable: is_sendable is specialized to
false for it
```

(GCC échappe les retours à la ligne dans les diagnostics, donc le message est
émis sur une seule ligne ; un retour à la ligne apparaîtrait comme `\x0a`. C'est
pourquoi le chemin est une phrase et non une liste à puces.)

Deux motifs terminaux gardent délibérément leur place et ne descendent pas : le
membre `mutable` et le membre référence de la marche `const synchronizable`. La
question qu'ils soulèvent est le trait **complet**, qui est opt-in : y descendre
ne dirait rien de plus que « opt-in ».

**Non-régression : les 11 TU de la suite compilent sans modification.**
`test_diagnostics.cpp` vérifie en plus, sur un type imbriqué à deux niveaux, que
le trait répond toujours un `false` nu — c'est-à-dire que le chemin reste vide
quand l'appel vient du trait.

---

## Ce qui reste non résolu

### Le trait nu

`assert_sendable<T>()` donne désormais la chaîne complète, mais la question
directe reste muette :

```cpp
static_assert(threadsafe::is_sendable_v<Outer>);
```

```
error: static assertion failed
```

C'est une limite du langage, pas de la bibliothèque : un `static_assert` sur une
valeur `bool` n'a aucun moyen d'expliquer d'où vient le `false`. La conséquence
pratique est qu'il faut **enseigner `assert_sendable<T>()` comme la forme
normale** et présenter `is_sendable_v<T>` comme la forme à réserver aux
expressions booléennes (`if constexpr`, contraintes).

### Les spécialisations n'expliquent toujours pas leur « non »

La marche descend maintenant dans **tous** les types, y compris ceux dont la
réponse vient en réalité d'une spécialisation. Sur `std::vector<int*>` elle
répond donc par le motif structurel :

```
Vec::v (std::vector<int*>) has a user-written copy, move or destructor — or a
template that may be selected as one — …
```

C'est vrai de `std::vector`, mais ce n'est pas la raison pour laquelle
`is_sendable<std::vector<int*>>` répond `false` : la vraie raison est
`is_sendable_v<int*>`, dans la spécialisation de `containers.h`. Même chose pour
un itérateur, dont la marche déroule les membres de libstdc++ :

```
LifeIter::it (__gnu_cxx::__normal_iterator<int*, std::vector<int> >)::_M_current
(int*) is a reference or a raw pointer: it borrows …
```

C'est le constat déjà posé en [08-api-et-flexibilite.md](./08-api-et-flexibilite.md),
section 3b, et le chemin le rend **plus visible** puisqu'il traverse désormais
les membres imbriqués. Le correctif est le même principe : une règle qui répond
« non » par spécialisation doit porter **sa propre** explication.
