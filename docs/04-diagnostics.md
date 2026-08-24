# Diagnostics — la chaîne de causes s'arrête au premier maillon

## Le constat

Le commit `8fd42a8` (« Explain trait failures instead of printing a bare false »)
a doté la bibliothèque de trois fonctions `assert_*` qui nomment le sous-objet
fautif au lieu d'afficher un `false` nu. C'est un vrai progrès, et le mécanisme
— lever une `std::meta::exception` depuis la marche `consteval`, la rattraper
dans `default_*` pour répondre `false`, la laisser s'échapper depuis `assert_*`
pour qu'elle devienne le diagnostic — est élégant et parfaitement adapté à un
propos pédagogique.

Mais l'attribution s'arrête **à un seul niveau**. Sur un type imbriqué :

```cpp
#include <threadsafe/threadsafe.h>
struct Inner  { int* borrowed; };
struct Middle { Inner inner; };
struct Outer  { Middle middle; };
consteval bool ask() { threadsafe::assert_sendable<Outer>(); return true; }
static_assert(ask());
```

le message obtenu est :

```
error: uncaught exception of type 'std::meta::exception';
  'what()': 'member `middle` of type Middle is not sendable'
```

`Middle` n'est pas la cause : c'est `Inner::borrowed`, un `int*`. Mesuré en
posant la question à chaque niveau :

| question | réponse |
|---|---|
| `assert_sendable<Outer>()` | ``member `middle` of type Middle is not sendable`` |
| `assert_sendable<Middle>()` | ``member `inner` of type Inner is not sendable`` |
| `assert_sendable<Inner>()` | ``member `borrowed` of type int* is not sendable`` |

L'utilisateur doit relancer la compilation autant de fois qu'il y a de niveaux
pour atteindre la cause réelle. Pour du code destiné à être lu en conférence,
c'est précisément le moment où la démonstration se casse.

## Le piège : la correction naïve coûte 38×

La correction évidente — rattraper l'exception du sous-objet et la préfixer —
fonctionne, mais elle est **catastrophique en temps de compilation**. `default_*`
appelle la même marche que `assert_*` et *jette* le message ; construire la
chaîne fait donc re-marcher chaque sous-objet à chaque niveau, pour un texte que
personne ne lira.

Mesuré sur une chaîne de 60 niveaux dont chaque niveau répond `false`
(`static_assert(!is_sendable_v<L0..L59>)`), meilleur de 3 exécutions :

| version | temps |
|---|---|
| référence | **753 ms** |
| chaîne naïve (toujours active) | **28 564 ms** — ×38 |

## La correction : ne construire la chaîne que là où elle est lue

Un paramètre `deep`, faux par défaut, mis à `true` uniquement par les `assert_*`.
Le trait lui-même ne paie rien.

Mesuré, même fichier, meilleur de 3 exécutions :

| version | temps |
|---|---|
| référence | **753 ms** |
| chaîne conditionnée | **760 ms** — dans le bruit |

Et le message devient :

```
member `mid` of type Mid is not sendable
  -> member `leaf` of type Leaf is not sendable
  -> member `borrowed` of type int* is not sendable
  -> sending a reference or a pointer shares its referent with the other thread,
     so the referent must be synchronizable — and synchronizability is opt-in
```

(GCC échappe les retours à la ligne dans les diagnostics, donc la chaîne est
émise sur une seule ligne avec ` -> ` comme séparateur ; le retour à la ligne
apparaîtrait comme `\x0a`.)

Les trois traits en bénéficient. Exemples réels obtenus après correctif :

```
member `mid` of type SyncMid is not readable from several threads at once
  -> member `leaf` of type SyncLeaf is not readable from several threads at once
  -> member `cached` of type int is mutable, so it is written through a const
     reference: its type must be fully synchronizable
```

```
member `mid` of type LifeMid is not lifetime aware
  -> member `leaf` of type LifeLeaf is not lifetime aware
  -> member `it` of type __gnu_cxx::__normal_iterator<int*, std::vector<int> >
     is not lifetime aware
  -> member `_M_current` of type int* is not lifetime aware
  -> a reference or a raw pointer borrows its referent instead of keeping it
     alive — hold the object, or a std::shared_ptr to it
```

Ce dernier exemple montre au passage une vraie qualité de la bibliothèque : un
`std::vector<int>::iterator` est correctement vu comme un emprunt.

**Vérification de non-régression : les 11 TU de la suite existante compilent sans
modification contre les en-têtes corrigés.**

---

## Code complet du correctif

### `include/threadsafe/details/utils.h` — ajouter après `reject`

```cpp
// Same as reject, but continues with the reason the subobject itself gave, so a
// nested failure names the root cause instead of stopping at the first hop.
[[noreturn]] inline consteval void reject_because(std::meta::info subject,
                                                  std::u8string_view reason,
                                                  const std::meta::exception &cause) {
    throw std::meta::exception(describe(subject) + u8" " + std::u8string(reason)
                                  + u8" -> " + std::u8string(cause.u8what()),
                              subject);
}
```

### `include/threadsafe/details/sendable.h`

Déclaration anticipée, en haut du fichier :

```cpp
namespace detail {
consteval void diagnose_default_is_sendable(std::meta::info type, bool deep = false);
consteval bool default_is_sendable(std::meta::info type);
}
```

`assert_sendable` demande la version profonde :

```cpp
template <class T>
consteval void assert_sendable() {
    if (is_sendable_v<T>)
        return;

    detail::diagnose_default_is_sendable(^^T, true);

    throw std::meta::exception(
        u8"is_sendable is specialized to false for this type", ^^T);
}
```

Le helper, juste avant `diagnose_default_is_sendable` :

```cpp
// Rejects `subject`; when `deep`, first walks `inner` so the message continues
// with the reason the subobject itself gives, down to the root cause.
[[noreturn]] inline consteval void explain(std::meta::info subject,
                                           std::u8string_view reason,
                                           std::meta::info inner, bool deep) {
    if (deep) {
        try {
            diagnose_default_is_sendable(inner, true);
        } catch (const std::meta::exception &cause) {
```

La signature prend le paramètre, et la queue de la marche l'utilise :

```cpp
inline consteval void diagnose_default_is_sendable(std::meta::info type, bool deep) {
    // ... inchangé jusqu'aux deux boucles finales ...

    // `deep` is set only by assert_sendable, where the message is actually read.
    // The trait itself discards it, and walking the subobject again to build a
    // chain nobody reads would make every "false" answer quadratic.
    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            explain(base, u8"is not sendable", type_of(base), deep);

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_sendable_type(remove_cv(type_of(member))))
            explain(member, u8"is not sendable", remove_cv(type_of(member)), deep);
}
```

### `include/threadsafe/details/synchronizable.h`

Même schéma. Le helper :

```cpp
[[noreturn]] inline consteval void explain_const_sync(std::meta::info subject,
                                                      std::u8string_view reason,
                                                      std::meta::info inner,
                                                      bool deep) {
    if (deep) {
        try {
            diagnose_default_is_const_synchronizable(inner, true);
        } catch (const std::meta::exception &cause) {
```

Et les deux points d'appel dans `diagnose_default_is_const_synchronizable` — la
boucle sur les bases :

```cpp
    for (info base : bases_of(type, context))
        if (!is_synchronizable_type(add_const(type_of(base))))
            explain_const_sync(base,
                               u8"is not readable from several threads at once",
                               add_const(type_of(base)), deep);
```

et la dernière branche de la boucle sur les membres (les branches `mutable` et
`référence` gardent leur `reject` : leur message est déjà terminal) :

```cpp
        } else if (!is_synchronizable_type(add_const(member_type))) {
            // ordinary value member: const propagates normally.
            explain_const_sync(member,
                               u8"is not readable from several threads at once",
                               add_const(member_type), deep);
        }
```

### `include/threadsafe/details/lifetime_aware.h`

```cpp
[[noreturn]] inline consteval void explain_lifetime(std::meta::info subject,
                                                    std::u8string_view reason,
                                                    std::meta::info inner, bool deep) {
    if (deep) {
        try {
            diagnose_default_is_lifetime_aware(inner, true);
        } catch (const std::meta::exception &cause) {
```

```cpp
    for (info base : bases_of(type, context))
        if (!is_lifetime_aware_type(type_of(base)))
            explain_lifetime(base, u8"is not lifetime aware", type_of(base), deep);

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_lifetime_aware_type(remove_cv(type_of(member))))
            explain_lifetime(member, u8"is not lifetime aware",
                             remove_cv(type_of(member)), deep);
```

---

## Ce qui reste non résolu : le trait nu

`assert_sendable<T>()` donne désormais la chaîne complète, mais la question
directe reste muette :

```cpp
static_assert(threadsafe::is_sendable_v<Outer>);
```

```
error: static assertion failed
    5 | static_assert(threadsafe::is_sendable_v<Outer>);
      |               ~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~
```

C'est une limite du langage, pas de la bibliothèque : un `static_assert` sur une
valeur `bool` n'a aucun moyen d'expliquer d'où vient le `false`. La conséquence
pratique est qu'il faut **enseigner `assert_sendable<T>()` comme la forme
normale** et présenter `is_sendable_v<T>` comme la forme à réserver aux
expressions booléennes (`if constexpr`, contraintes). Une mention explicite dans
le message des `assert_*` — « pour cette raison, préférez `assert_sendable<T>()`
à `static_assert(is_sendable_v<T>)` » — coûterait une ligne et éviterait la
question en salle.
