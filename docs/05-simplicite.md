# Simplicité et valeur pédagogique

Le code existe pour être lu sur scène. Un code correct mais difficile à suivre
échoue à sa raison d'être. Cette section juge les douze en-têtes sur ce critère.

**Le verdict global est bon.** 1 283 lignes, dix seulement dépassent 80 colonnes,
exactement une macro, exactement une variable à lettre unique, et une forme de
trait rigoureusement constante (`struct` + `_v` + concept). Les défauts relevés
sont des *manques d'explication* aux endroits précis où un public va poser une
question, et une répétition qui enterre l'idée qu'elle enseigne.

---

## 1. La clé de voûte du design n'a aucun commentaire

`utils.h` lignes 42-44 :

```cpp
inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
    return std::meta::extract<bool>(std::meta::substitute(trait, {type}));
}
```

C'est **le** mécanisme qui fait que la bibliothèque marche : c'est lui qui permet
qu'une spécialisation écrite dans l'unité de traduction de l'utilisateur, bien
après la lecture de l'en-tête, soit malgré tout la réponse quand la récursion
atteint le membre. C'est aussi la première ligne qu'un public familier de la
réflexion va contester — « pourquoi pas simplement `is_sendable_v<[:type:]>` ? »

La réponse est excellente, et vérifiable :

```cpp
inline consteval bool direct_splice(std::meta::info type) {
    return my_trait<[:type:]>;
}
```

```
error: 'type' is not a constant expression
error: template argument 1 is invalid
```

L'alternative évidente n'est pas seulement moins bonne, elle est **mal formée** :
`type` est un paramètre de fonction, donc jamais une expression constante, et un
*splice* en exige une. Le choix réflexif est donc contraint, pas décoratif — et
c'est exactement le genre de point qui fait une bonne conférence. Il n'est écrit
nulle part.

### Correctif

```cpp
// The keystone of the design. `trait` is a reflection of a variable template
// such as ^^is_sendable_v; substitute() instantiates it with `type` here, at
// *evaluation* time. That is what lets a specialization written in the user's
// translation unit — long after this header was parsed — still be the answer
// when the recursion reaches it. Writing `is_sendable_v<[:type:]>` instead is
// not merely worse, it is ill-formed: `type` is a function parameter, so it is
// never a constant expression, and a splice needs one.
inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
    return std::meta::extract<bool>(std::meta::substitute(trait, {type}));
}
```

---

## 2. La ligne la plus subtile de la bibliothèque n'a aucun commentaire

`copy_on_write.h` lignes 32-36 :

```cpp
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
```

Cette barrière est **juste** — c'est le motif [atomics.fences]/3, et l'audit n'a
pas réussi à la mettre en défaut en 50 000 tours de handoff. Mais rien ne dit
quel transfert elle protège, et GCC signale que TSan ne sait pas la vérifier
(`'atomic_thread_fence' is not supported with '-fsanitize=thread'`). Le lecteur
n'a donc **aucun moyen** de contrôler l'affirmation : ni le commentaire, ni le
sanitizer.

### Correctif

```cpp
        } else {
            // Sole owner, so writing in place is safe — but "sole" may be new:
            // another thread may have dropped its share a moment ago. Only the
            // *last* release of a shared_ptr is guaranteed to synchronize with
            // the owner; a 2 -> 1 decrement leaves the survivor with none. This
            // fence is that missing acquire, so the writes made by the previous
            // owner happen-before the writes made here.
            std::atomic_thread_fence(std::memory_order_acquire);
        }
```

---

## 3. `containers.h` dit 39 fois la même phrase — et cache la seule fois où elle diffère

244 lignes de spécialisations quasi identiques. Le coût pédagogique est double :
l'idée unique qu'elles enseignent est noyée, et **la seule exception réelle est
invisible**.

Cette exception : `basic_string` ignore silencieusement son paramètre `Tr`
(`char_traits`), alors que `unordered_set` vérifie bien ses `H` et `Eq`. Aucun
commentaire ne dit pourquoi. La raison est bonne — `char_traits` est une politique
**sans état**, jamais stockée, donc juger la chaîne dessus serait un faux négatif
— mais elle n'est écrite nulle part, et une règle réflexive naïve « vérifie tous
les arguments de template » la casse aussitôt (vérifié : le `static_assert`
correspondant échoue).

Une règle réflexive écrivant l'idée **une fois** tient en 114 lignes, donne des
réponses **identiques** sur toute la suite, et coûte entre +1,5 % et +2,7 % de
temps de compilation.

Le remplacement complet proposé pour `include/threadsafe/details/containers.h` :

```cpp
#pragma once

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <meta>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

namespace detail {

// These containers all tell the same story: they own their elements and hold
// every policy they were given — allocator, comparator, hasher, equality — by
// value, and they reach nothing else. So each of them is safe exactly when all
// of its template arguments are, which is one rule per trait instead of one
// rule per container.
//
// std::basic_string is deliberately absent: its Tr parameter is a *stateless*
// policy, never stored, so judging the string on it would be a false negative.
// It gets its own rules below.
inline constexpr std::meta::info owning_container_templates[] = {
    ^^std::vector,        ^^std::deque,
    ^^std::list,          ^^std::forward_list,
    ^^std::map,           ^^std::multimap,
    ^^std::set,           ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset};

inline consteval bool is_owning_container(std::meta::info type) {
    if (!std::meta::has_template_arguments(type))
        return false;

    const auto container_template = std::meta::template_of(type);
    for (std::meta::info owning_template : owning_container_templates)
        if (owning_template == container_template)
            return true;
    return false;
}

// `read_arguments_as_const` is what makes the const rule read like the value
// rule: through a const container, elements and policies are reached as const.
inline consteval bool every_type_argument_satisfies(
    std::meta::info trait, std::meta::info type, bool read_arguments_as_const) {
    for (std::meta::info argument : std::meta::template_arguments_of(type)) {
        if (!std::meta::is_type(argument))
            continue;

        const auto asked = read_arguments_as_const
                               ? std::meta::add_const(argument)
                               : argument;
        if (!trait_value(trait, asked))
            return false;
    }
    return true;
}

}

template <class Container>
concept owning_container = detail::is_owning_container(^^Container);

template <owning_container Container>
struct is_sendable<Container>
    : std::bool_constant<detail::every_type_argument_satisfies(
          ^^is_sendable_v, ^^Container, false)> {};

// [res.on.data.races]: the const member functions of a standard container may
// run concurrently, so a const container is read-safe exactly when everything a
// reader reaches through it is. Naming the containers also keeps the recursion
// out of libstdc++ internals, whose mutable members (unordered_*'s rehash
// policy) are covered by that guarantee.
template <owning_container Container>
struct is_synchronizable<const Container>
    : std::bool_constant<detail::every_type_argument_satisfies(
          ^^is_synchronizable_v, ^^Container, true)> {};

template <owning_container Container>
struct is_lifetime_aware<Container>
    : std::bool_constant<detail::every_type_argument_satisfies(
          ^^is_lifetime_aware_v, ^^Container, false)> {};

// std::basic_string owns its characters and its allocator; Tr is a stateless
// policy it never stores, so it is not part of the question.
template <class Char, class Tr, class A>
struct is_sendable<std::basic_string<Char, Tr, A>>
    : std::bool_constant<is_sendable_v<Char> && is_sendable_v<A>> {};
template <class Char, class Tr, class A>
struct is_synchronizable<const std::basic_string<Char, Tr, A>>
    : std::bool_constant<is_synchronizable_v<const Char>
                         && is_synchronizable_v<const A>> {};
template <class Char, class Tr, class A>
struct is_lifetime_aware<std::basic_string<Char, Tr, A>>
    : std::bool_constant<is_lifetime_aware_v<Char>
                         && is_lifetime_aware_v<A>> {};

// std::allocator holds no state at all; it never reaches its T.
template <class T>
struct is_sendable<std::allocator<T>> : std::true_type {};
template <class T>
struct is_synchronizable<const std::allocator<T>> : std::true_type {};
template <class T>
struct is_lifetime_aware<std::allocator<T>> : std::true_type {};

}
```

**Recommandation nuancée.** Pour du code pédagogique, la répétition n'est pas
toujours un défaut : douze règles parallèles se lisent sans effort, là où une
règle réflexive demande de comprendre `substitute` avant de comprendre le
conteneur. Le vrai gain n'est donc pas la réduction de 244 à 114 lignes, mais le
fait que la version réflexive **oblige à écrire le commentaire sur `Tr`** — elle
transforme une exception invisible en une exception nommée. Si la répétition est
conservée, il faut au minimum ajouter ce commentaire à la règle `basic_string`.

---

## 4. Les trois règles de tableau sont écrites de trois façons, et la différence s'observe

`sendable.h:32` et `lifetime_aware.h:38` écrivent
`is_X<T[N]> : is_X<std::remove_cv_t<T>>`, tandis que `synchronizable_base.h:18`
écrit `is_synchronizable<T[N]> : is_synchronizable<T>`, sans le `remove_cv`.

Ce n'est pas qu'un détail de style — vérifié :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
namespace { struct Atomic { std::atomic<int> v; }; }
template <> struct threadsafe::is_synchronizable<Atomic> : std::true_type {};
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<volatile int[3]>);
static_assert(is_lifetime_aware_v<volatile int[3]>);
static_assert(is_synchronizable_v<Atomic[3]>,      "plain array of a vouched element: yes");
static_assert(!is_synchronizable_v<volatile Atomic[3]>,
              "but volatile-qualifying the element flips it -- the other two traits do not");
static_assert(!is_synchronizable_v<volatile int> && is_synchronizable_v<const volatile int>);
```

Le fichier compile : les trois traits sont bien en désaccord sur un élément
`volatile`.

### Correctif — vérifié, les 11 TU passent

```cpp
// The four array rules sit together on purpose: <const T> below also matches a
// const array, so without the const forms it would tie with <T[N]> and <T[]>.
// remove_cv_t on the first two mirrors is_sendable<T[N]> and
// is_lifetime_aware<T[N]>: cv on the element is not a property of the element.
template <class T, std::size_t N>
struct is_synchronizable<T[N]> : is_synchronizable<std::remove_cv_t<T>> {};
template <class T>
struct is_synchronizable<T[]> : is_synchronizable<std::remove_cv_t<T>> {};
template <class T, std::size_t N>
struct is_synchronizable<const T[N]> : is_synchronizable<const T> {};
template <class T>
struct is_synchronizable<const T[]> : is_synchronizable<const T> {};
```

La règle documentée « volatile n'ajoute pas d'écrivain, c'est le `const` manquant
qui le fait » reste vraie : `volatile int[3]` demeure non synchronizable.

---

## 5. Points plus légers

- **`synchronizable_base.h` existe pour casser un cycle d'inclusion réel** —
  `synchronizable.h` a besoin de `sendable.h` (pour `is_synchronizable<std::atomic<T>>
  : is_sendable<T>`) et `sendable.h` a besoin du squelette du trait. Rien dans le
  fichier ne le dit, et un lecteur y voit un découpage arbitraire.
- **Le bloc `try`/`catch` de `default_is_*` et son commentaire de quatre lignes sont
  recopiés trois fois, à l'octet près.** Pour trois histoires parallèles, c'est
  défendable ; il vaut la peine de le dire explicitement plutôt que de laisser croire
  à un oubli de factorisation.
- **`value_guard` porte un commentaire flottant, périmé et fautif** (« //but that would
  have been » sans espace), qui décrit une alternative de conception non retenue.
- **`has_unreflectable_state`, le prédicat le plus délicat de `utils.h`, est documenté par
  quatre mots** : « Mostly for closure type. »
- ~~**`dynamic_type_is_known` est la règle la moins évidente de la bibliothèque** et se
  trouve à 60 lignes de son unique point d'usage.~~ Corrigé : la règle vit dans
  `utils.h`, sous le commentaire qui dit *pourquoi* une réponse structurelle ne prouve
  rien à travers une indirection, et elle a désormais quatre points d'usage.
- **`f` est la seule variable à lettre unique** de la bibliothèque et apparaît deux fois,
  en violation directe de la règle « toujours des noms explicites » de `CLAUDE.md`.
- **`threadsafe.h`, le premier fichier qu'un lecteur ouvre, est onze `#include` nus dans
  un ordre arbitraire.** Le découpage proposé en
  [06-performance-compilation.md](./06-performance-compilation.md) y répond aussi : il
  fait apparaître le feuilletage traits → bibliothèque standard → helpers.

---

## Ce qui est réussi

- **Le design des diagnostics est la meilleure idée de la bibliothèque, et il
  fonctionne.** Écrire chaque règle comme une fonction qui *retourne* pour oui et
  *lance* une `std::meta::exception` portant la raison pour non donne, avec un seul
  corps de code, à la fois la réponse booléenne et le message.
- **Les messages sont écrits pour un humain.** `reject(subject, reason)` compose
  « member `x` of type T » + « is not sendable » en une phrase anglaise, et la
  convention tient partout.
- **Les passages difficiles qui *sont* commentés le sont excellemment.** Les dix-neuf
  lignes de `utils.h:63-76` sur `may_hijack_copy_move` expliquent pourquoi un
  constructeur templaté peut être choisi à la place d'une copie, et pourquoi la
  réflexion ne peut pas distinguer les formes inoffensives.
- **Les surcharges `= delete("message")` de `value_guard` sont un dispositif pédagogique
  de premier ordre** : `*counter.lock()` échoue en affichant « a temporary guard is
  destroyed at the semicolon, so it cannot hand out a reference ».
- **La suite de tests est de la bonne documentation.** `test_deferred_specialization.cpp`
  n'assène pas des assertions : il explique en prose pourquoi l'indirection par
  `substitute` existe et ce qui casserait sans elle.
- **Le mécanisme réflexif est correctement choisi, pas à la mode** : les deux
  alternatives évidentes ont été essayées et sont respectivement mal formée et plus
  lourde.
