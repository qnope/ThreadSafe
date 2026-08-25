# API, ergonomie et flexibilité

## 1. La surface publique est asymétrique

Trois traits, mais pas trois de chaque chose :

| | `is_synchronizable` | `is_sendable` | `is_lifetime_aware` |
|---|---|---|---|
| trait `_v` | oui | oui | oui |
| fonction `assert_*` | oui | oui | oui |
| face réflexive `*_type` | oui | oui | oui |
| **concept** | **non** | `sendable` | `lifetime_aware` |
| **macro de vouch** | `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` | **non** | **non** |

Les deux trous sont exactement complémentaires, ce qui les rend d'autant plus
visibles pour qui lit les en-têtes en conférence. Vérifié :

```
error: 'synchronizable' is not a member of 'threadsafe';
       did you mean 'is_synchronizable'?
```

Un utilisateur ayant un type qu'il synchronise à la main doit écrire une
spécialisation brute pour `is_sendable` ou `is_lifetime_aware`, alors que la
bibliothèque lui offre une macro pour le troisième.

### Correctif — vérifié, les 11 TU passent

Dans `include/threadsafe/details/synchronizable_base.h`, avant la face réflexive :

```cpp
template <class T>
concept synchronizable = is_synchronizable_v<T>;
```

et, en remplacement du bloc de macros en fin de fichier :

```cpp
// The three vouches. Each says "I have checked this myself"; none is verified.
// NOTE: an explicit specialization must be written in a namespace enclosing
// `threadsafe`, so these cannot appear inside your own namespace -- close it
// first, and spell the type qualified.
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)  \
    template <>                                       \
    struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}

#define THREADSAFE_UNSAFE_ASSERT_SENDABLE(...)        \
    template <>                                       \
    struct threadsafe::is_sendable<__VA_ARGS__> : std::true_type {}

#define THREADSAFE_UNSAFE_ASSERT_LIFETIME_AWARE(...)  \
    template <>                                       \
    struct threadsafe::is_lifetime_aware<__VA_ARGS__> : std::true_type {}
```

Le programme suivant compile et se comporte correctement contre ces en-têtes :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>

namespace app {
struct HandRolled { HandRolled(const HandRolled&); };   // user-written copy: refused by default
struct Borrowed  { int* p; };                            // a borrow: refused by default
}

// A vouch is an explicit specialisation, so it must be written in a namespace
// enclosing `threadsafe` -- close your own namespace first and spell the type
// qualified. That is a language rule, not a library choice.
THREADSAFE_UNSAFE_ASSERT_SENDABLE(app::HandRolled);
THREADSAFE_UNSAFE_ASSERT_LIFETIME_AWARE(app::Borrowed);

static_assert(threadsafe::is_sendable_v<app::HandRolled>);
static_assert(threadsafe::is_lifetime_aware_v<app::Borrowed>);

// The missing concept, now symmetric with sendable / lifetime_aware.
template <threadsafe::synchronizable T> void share(T&) {}

template <class T> concept shareable = requires(T& value) { share(value); };
static_assert(shareable<std::atomic<int>>, "an atomic may be shared");
static_assert(!shareable<int>, "a plain int may not");
```

### Une limite du langage, à documenter

Le vouch ne peut pas être écrit dans le namespace de l'utilisateur :

```cpp
namespace app {
struct Widget { int x; };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Widget);   // ne compile pas
}
```

```
error: declaration of 'struct threadsafe::is_synchronizable<app::Widget>'
       in namespace 'app' which does not enclose 'threadsafe'
```

Une spécialisation explicite doit vivre dans un namespace englobant celui du
template. Il n'y a pas de correctif possible ; c'est au commentaire de la macro
de le dire, ce que fait le correctif ci-dessus.

---

## 2. Le vrai frein à la flexibilité : la bibliothèque standard passe mal

Mesuré sur seize types **qui possèdent tout ce qu'ils contiennent** — aucun
n'emprunte, aucun ne partage :

| type-valeur autonome | sendable |
|---|---|
| `std::bitset<8>` | **non** |
| `std::complex<double>` | **non** |
| `std::chrono::milliseconds` | **non** |
| `std::chrono::system_clock::time_point` | **non** |
| `std::valarray<int>` | **non** |
| `std::error_code` | **non** |
| `std::error_condition` | **non** |
| `std::filesystem::path` | **non** |
| `std::future<int>` | **non** |
| `std::promise<int>` | **non** |
| `std::expected<int, std::string>` | **non** |
| `std::chrono::year_month_day` | oui |
| `std::filesystem::file_status` | oui |
| `std::string` | oui |
| `std::vector<int>` | oui |
| `std::pair<int, double>` | oui |

**Dix sur seize sont refusés.** La bibliothèque donne elle-même la raison, la
même pour neuf des dix :

```
'std::bitset<8> has a user-written copy, move or destructor — or a template that
 may be selected as one — which can share state the members do not show'
```

Vérifié directement contre le garde :

```
has_only_default_copy_move_destroy(^^std::bitset<8>)            = false
has_only_default_copy_move_destroy(^^std::complex<double>)      = false
has_only_default_copy_move_destroy(^^std::chrono::milliseconds) = false
has_only_default_copy_move_destroy(^^std::valarray<int>)        = false
```

Le raisonnement du garde est juste — sept tentatives de le contourner ont échoué,
y compris hériter du template par `using Base::Base` — et il est *délibéré*. Ce
qui n'est nulle part énoncé, c'est son **ampleur**.

Le point important est ailleurs : `std::string` et `std::vector` ne passent que
parce que `containers.h` les liste à la main. Autrement dit, **le défaut
structurel refuse l'essentiel de la bibliothèque standard, et ce sont les listes
explicites qui la rattrapent.** Cela ne passe pas à l'échelle, et c'est invisible
pour l'utilisateur, qui ne peut pas deviner pourquoi `std::vector<int>` marche et
`std::bitset<8>` non.

`std::chrono::milliseconds` refusé est, en pratique, bloquant : les durées et les
horodatages sont partout dans du code concurrent.

**Recommandation.** Ne pas assouplir le garde — il est correct et il est le cœur
du propos. Faire deux choses à la place :

1. Ajouter à `vocabulary.h` les quelques règles explicites qui débloquent les
   types-valeur les plus courants : `chrono::duration`, `chrono::time_point`,
   `complex`, `bitset`, `error_code`. C'est mécanique et cohérent avec la manière
   dont `string` et `vector` sont déjà traités.
2. **Dire la règle à voix haute** : « un type ayant un constructeur templaté est
   refusé par défaut, y compris quand ce template ne pourrait jamais détourner la
   copie — parce que `parameters_of` refuse un template et que la réflexion ne
   peut donc pas distinguer les deux cas. » Le commentaire de `utils.h` explique
   déjà très bien *pourquoi* ; c'est le *combien* qui manque.

---

## 3. La première heure, mesurée

Trois programmes réalistes ont été écrits **deux fois** : d'abord la version
naïve qu'un développeur C++ écrit sans lire la documentation, puis la version qui
compile. Résultat : **7 rejets**, tous courts — 14 à 22 lignes, la cause sur
l'avant-dernière ligne, là où un échec de concept ou de SFINAE en produirait des
centaines. C'est un vrai point fort.

Le problème est ailleurs : **chacun de ces 7 messages désigne une cause autre que
l'erreur de l'utilisateur.**

| programme naïf | ce que l'utilisateur a écrit | lignes | ce que le message accuse |
|---|---|---|---|
| compteur | `[&counter]` | 18 | « a closure type with captures » |
| compteur, essai 2 | `atomic` + capture | 18 | « a closure type with captures » |
| compteur, essai 3 | `std::ref` | 22 | ``member `_M_data` of type std::atomic<int>*`` |
| config | `[&config]` | 16 | « a closure type with captures » |
| config, essai 2 | `shared_ptr<const T>` | 20 | « std::shared_ptr … has a user-written copy » |
| config, essai 3 | `std::cref` | 20 | « std::reference_wrapper … user-written copy » |
| producteur/consommateur | `[&]` ×2 | 62 | « a closure type with captures » (deux fois) |
| prod/cons, essai 2 | `synchronized_value<queue>` | 20 | « is_sendable_v\<T\> … evaluated to 'false' » |

Les trois versions correctes compilent, s'exécutent et sont propres sous
ThreadSanitizer — mais elles exigent de connaître `copy_on_write`,
`synchronized_value::make` et le remplacement d'une référence par un
`shared_ptr`.

### 3a. Le rejet n°1 accuse la réflexion et donne un conseil inapplicable

Toute lambda capturante est rejetée par :

> `holds state reflection cannot see (a closure type with captures); specialize is_sendable to state the intent`

Deux problèmes. D'abord le message nomme une **limite de la réflexion**, pas
l'erreur de l'utilisateur — qui a simplement capturé par référence. Ensuite le
remède proposé **ne peut pas être écrit** pour une lambda de site d'appel :

```
error: a template declaration cannot appear at block scope
   13 |     template <>
      |     ^~~~~~~~
```

Une spécialisation explicite n'est pas autorisée au bloc. Le conseil est donc
littéralement inapplicable dans le cas où il est le plus souvent donné.

Reformulation proposée, qui nomme la sortie plutôt que la limite :

```cpp
    if (has_unreflectable_state(type))
        reject(type,
               u8"is a closure and reflection cannot see its captures, so the "
               u8"library cannot judge what crosses with it: take the shared "
               u8"state as a parameter of the task instead — a std::shared_ptr "
               u8"for launch_task, a std::ref for launch_scoped_task — or "
               u8"specialize is_sendable to vouch for it at namespace scope");
```

### 3b. `assert_*` invente une raison pour les types qui répondent par spécialisation

Quand `is_sendable` ou `is_lifetime_aware` a répondu « non » **par une
spécialisation** (`std::reference_wrapper`, `std::shared_ptr`, `copy_on_write`),
`assert_*` ignore cette spécialisation et déroule quand même la marche
structurelle. Le message accuse alors un membre privé de libstdc++, ou reproche à
`std::shared_ptr` d'avoir un constructeur de copie écrit à la main — et conseille
à l'utilisateur de spécialiser `is_sendable` pour un type `std::`.

C'est le pendant, côté spécialisations, du problème de profondeur traité en
[04-diagnostics.md](./04-diagnostics.md) : là, la chaîne s'arrêtait trop tôt ;
ici, elle part dans la mauvaise direction. Le correctif est le même principe :
une règle qui répond « non » par spécialisation doit porter **sa propre**
explication.

### 3c. Partager une donnée immuable par référence n'a aucune orthographe

`is_synchronizable_v<const std::map<…>>` est **vrai** : lire une map `const`
depuis plusieurs threads est sûr, la bibliothèque le dit. Pourtant **aucune**
forme référencée n'est acceptée — `const T&`, `const T*`,
`shared_ptr<const T>`, `weak_ptr<const T>`, `reference_wrapper<const T>` — parce
que toutes les règles d'indirection appliquent `std::remove_cv_t` au référent.

Le cas d'usage « une configuration lue par tout le monde » n'a donc **pas de
réponse en forme de référence**. La réponse existante est `copy_on_write`, ce qui
est correct et efficace, mais le nom ne dit pas cela à quelqu'un qui cherche
« partager une valeur immuable ». Un alias suffirait :

```cpp
// The name a reader looks for when they want "one immutable value, many
// threads": copy_on_write is that, read through const only.
template <class T>
using shared_immutable = copy_on_write<T>;
```

### 3d. `launch_scoped_task` sérialise les tâches

```cpp
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }
```

Le `join()` est **dans l'appel**. N tâches lancées ainsi s'exécutent l'une après
l'autre. Vérifié :

```
four 200ms scoped tasks took 817ms (parallel would be ~200)
```

C'est le seul point d'entrée qui accepte un emprunt (`std::ref`), donc le motif
classique — plusieurs threads travaillant sur un objet de la pile, tous joints en
fin de portée — **n'est pas exprimable** avec cette API. Le nom « scoped task »
suggère par ailleurs exactement l'inverse de ce qui se passe.

Un `scoped_task_group` qui lance dans les `launch` et joint dans le destructeur
comblerait le manque avec la même précondition, et rendrait le nom honnête.

### 3e. Le launcher n'a ni `join()` ni `wait()`

`asynchronous_task_launcher` ne joint qu'à la destruction. Un utilisateur qui lit
les résultats avant la fin de portée les lit **silencieusement faux**, sans
avertissement.

---

## 4. L'idiome pimpl que la bibliothèque recommande ne compile pas — **corrigé**

Le diagnostic de `sendable.h` dit :

> `is_sendable<T> requires a complete type — specialize is_sendable for types holding a pointer to an incomplete type (the pimpl idiom)`

Or `detail::dynamic_type_is_known` instancie `std::is_polymorphic_v<T>` et
`std::is_final_v<T>`, tous deux mal formés sur un type incomplet. **Poser la
question sur un pimpl est donc une erreur dure**, pas une réponse `false` :

```cpp
#include <threadsafe/threadsafe.h>
#include <memory>
namespace { class Implementation; }
struct Widget { std::unique_ptr<Implementation> impl; };
static_assert(!threadsafe::is_sendable_v<Widget>);   // on ne fait que POSER la question
```

```
exit=1, 68 lines of diagnostic
/opt/homebrew/.../c++/16/type_traits:3675:44: error: invalid use of incomplete
    type 'class {anonymous}::Implementation'
```

68 lignes d'internals de libstdc++ pour une question à laquelle la bibliothèque
prétend savoir répondre. Le conseil qu'elle imprime n'atteint donc jamais le cas
pour lequel il a été écrit.

Le correctif est de n'interroger `is_polymorphic`/`is_final` qu'une fois la
complétude vérifiée. C'est ce qui est en place dans `utils.h` :

```cpp
template <class T>
consteval bool compute_dynamic_type_is_known() {
    // void erases the type outright: nothing was read off a static type, so
    // the object behind it is of some other type entirely, and nothing here
    // names it. That is the question this guard asks, so the answer is no.
    if constexpr (std::is_void_v<T>)
        return false;
    else if constexpr (!std::meta::is_complete_type(^^T))
        return false;
    else
        return !std::is_polymorphic_v<T> || std::is_final_v<T>;
}
```

Le cas `void` n'était pas dans l'analyse initiale : il tombait dans la branche
« type incomplet », qui donne la bonne réponse pour la mauvaise raison. Il a
donc sa branche à lui, qui répond **non** — un type effacé n'est pas un type
connu. `shared_ptr<void>` n'est donc pas `lifetime_aware` : le bloc de contrôle
garde bien *un* objet en vie, mais rien dans le type statique ne dit que cet
objet n'est pas lui-même un emprunteur — c'est le cas `shared_ptr<span<int>>`,
en pire.

L'exemple ci-dessus produisait 68 lignes d'internals de libstdc++ ; il compile
désormais proprement, et `is_sendable_v<Widget>` répond `false` avec le diagnostic
qui recommande de spécialiser le trait — le conseil atteint enfin le cas pour
lequel il a été écrit. Ce correctif était le prérequis de l'élargissement de la
garde décrit en [01-robustesse-des-traits.md](./01-robustesse-des-traits.md) §3 :
l'appliquer à `shared_ptr`/`weak_ptr` aurait sinon répandu l'erreur dure sur la
forme de pimpl la plus courante.

## 5. Les adaptateurs de conteneurs sont refusés

`containers.h` couvre toutes les séquences et tous les associatifs, mais **aucun
adaptateur**. Le défaut structurel voit alors leurs constructeurs templatés à
allocateur et les refuse, bien qu'un `std::queue<int>` soit exactement aussi
sendable que le `std::deque<int>` qu'il contient.

Ce que l'utilisateur rencontre en premier, en écrivant le producteur/consommateur
évident :

```
error: static assertion failed: the mutex serializes access, but the T still
    crosses thread boundaries — one thread at a time — so T must be sendable
  • the expression 'is_sendable_v<T> [with T = std::queue<int, std::deque<int,
    std::allocator<int>>>]' evaluated to 'false'
```

Trois règles de trois lignes (`queue`, `stack`, `priority_queue`) suffisent, dans
le même style que celles qui existent déjà.

---

## 6. Frictions plus légères

**L'écriture naturelle est refusée.** `++*counter.lock();` ne compile pas — un
guard temporaire mourant au point-virgule ne peut pas rendre de référence. Il faut
nommer le guard. C'est justifié, et cela mérite d'être enseigné plutôt que
découvert : l'auteur de ce rapport s'y est fait prendre en écrivant un banc de
performance, **après** avoir documenté la règle.

Le corollaire moins heureux est que le une-ligne *sûr*
`sv.lock()->push_back(x)` est refusé lui aussi, alors qu'il ne fait échapper
aucune référence.

**`[[nodiscard]]` sur `lock()` n'est qu'un avertissement**, et le projet ne
compile pas avec `-Werror`.

**`copy_on_write<std::vector<int>>{{1, 2, 3}}` ne compile pas**, et le diagnostic
pointe au mauvais endroit.

**`synchronized_value` et `copy_on_write` court-circuitent le mécanisme
explicatif** : le premier imprime un `evaluated to false` nu, le second ne vérifie
rien — alors que le launcher, lui, explique. Les trois helpers devraient parler la
même langue.

**Le trait nu ne s'explique pas.** `static_assert(is_sendable_v<T>)` donne un
`static assertion failed` sans raison ; seul `assert_sendable<T>()` explique. Voir
[04-diagnostics.md](./04-diagnostics.md).

---

## 7. Découpage des en-têtes

Un utilisateur qui ne veut que les traits paie aujourd'hui 619 ms au lieu de
371 ms, parce que l'en-tête parapluie tire les helpers et les spécialisations de
conteneurs. Le découpage public est écrit et vérifié en
[06-performance-compilation.md](./06-performance-compilation.md) — il rend en plus
le feuilletage de la bibliothèque visible, ce qui est en soi le propos.

---

## Intégration CMake — `INSTALL_INTERFACE` promet une installation qui n'existe pas

`CMakeLists.txt` déclare :

```cmake
target_include_directories(threadsafe INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
```

`$<INSTALL_INTERFACE:include>` ne sert que lorsque la cible est **exportée**. Or
le projet n'a **aucune règle `install()`** et aucun fichier de configuration de
paquet : cette expression génératrice est morte, et `find_package(ThreadSafe)`
ne peut jamais fonctionner.

Ce qui marche déjà, vérifié jusqu'à l'exécution du binaire consommateur :

```cmake
cmake_minimum_required(VERSION 4.0)
project(Consumer LANGUAGES CXX)
add_subdirectory(/chemin/vers/ThreadSafe threadsafe_build)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE ThreadSafe::threadsafe)
```

`-freflection` se propage correctement et `THREADSAFE_BUILD_TESTS` reste bien à
`OFF` pour un consommateur (grâce à `PROJECT_IS_TOP_LEVEL`). C'est propre.

### Correctif — `CMakeLists.txt` complet

```cmake
cmake_minimum_required(VERSION 4.0)
project(ThreadSafe VERSION 0.1.0 LANGUAGES CXX)

add_library(threadsafe INTERFACE)
add_library(ThreadSafe::threadsafe ALIAS threadsafe)

target_include_directories(threadsafe INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(threadsafe INTERFACE cxx_std_26)
target_compile_options(threadsafe INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>)

option(THREADSAFE_BUILD_TESTS "Build compile-time tests" ${PROJECT_IS_TOP_LEVEL})
if(THREADSAFE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# The INSTALL_INTERFACE above promises an installed layout; these rules deliver
# it, so `find_package(ThreadSafe)` works for a consumer outside the tree.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS threadsafe EXPORT ThreadSafeTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ThreadSafeTargets
        FILE ThreadSafeTargets.cmake
        NAMESPACE ThreadSafe::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ThreadSafe)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeConfig.cmake"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/ThreadSafeTargets.cmake\")\n")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ThreadSafe)
```

Vérifié : `cmake --install` produit bien

```
<prefix>/lib/cmake/ThreadSafe/ThreadSafeTargets.cmake
<prefix>/lib/cmake/ThreadSafe/ThreadSafeConfig.cmake
<prefix>/lib/cmake/ThreadSafe/ThreadSafeConfigVersion.cmake
```

et un consommateur écrit ainsi compile et s'exécute :

```cmake
cmake_minimum_required(VERSION 4.0)
project(FindPkgConsumer LANGUAGES CXX)
find_package(ThreadSafe REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE ThreadSafe::threadsafe)
```

C'est une correction mineure — la bibliothèque étant *header-only*, un
utilisateur peut toujours se contenter d'un `-I`. Mais l'expression
`INSTALL_INTERFACE` présente dans le fichier annonce le contraire, et un
lecteur de code pédagogique la prendra pour un exemple à suivre.
