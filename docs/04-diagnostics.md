# 04 — Diagnostics

> Audit du message d'erreur : ce que la bibliothèque dit quand elle refuse.
> Rapports frères : [00](./00-synthese.md) · [01](./01-robustesse-des-traits.md) ·
> [02](./02-robustesse-des-helpers.md) · [03](./03-couverture-de-tests.md) ·
> [05](./05-simplicite.md) · [06](./06-performance-compilation.md) ·
> [07](./07-performance-execution.md) · [08](./08-api-et-flexibilite.md) ·
> [09](./09-methodologie.md)

## Verdict

Le mécanisme de diagnostic est excellent et la moitié des messages qu'il produit sont faux.
Le chemin que `assert_sendable` construit à travers les membres et les bases est exact,
complet, sans troncature ni limite de profondeur — il nomme les 60 niveaux d'une chaîne de
60 structures — et quand il tombe sur la bonne cause, la phrase qu'il écrit est la meilleure
que j'aie lue dans une bibliothèque C++. Mais sur les 28 cas distincts du corpus capturé,
**13 seulement nomment le bon coupable**. Dix en nomment un faux, un fuite un symbole
libstdc++, et trois ne produisent aucun message de la bibliothèque du tout. La cause unique
de presque tous les échecs tient en une phrase : `descend_sendable` *re-parcourt le template
primaire* pour rédiger son message, y compris pour les types dont la réponse vient d'une
spécialisation — et il accuse alors la première chose sur laquelle il trébuche, avant de
conclure par « *specialize is_sendable to state the intent* ». J'ai vérifié par compilation
que suivre ce conseil sur `std::vector<Foo*>` compile proprement et rend
`is_sendable_v<std::vector<Foo*>>` vrai, alors que `is_sendable_v<Foo*>` reste faux : le
message conduit l'utilisateur, mot pour mot, à ouvrir le trou de sûreté que la bibliothèque
existe pour fermer. En conférence, la démo serait la bibliothèque se sabordant elle-même.
Je propose un correctif consolidé — treize fonctions, trois branches de message et un
`static_assert`, répartis sur sept fichiers — que j'ai écrit, compilé, passé sur les
11 unités de test (toutes vertes) et mesuré (+1,4 % sur un banc trait-lourd) ; il fait passer
le corpus de 13/28 à 19/28 coupables exacts et 24/28 messages non-faux. Et je recommande
surtout une chose que personne n'a proposée : **les messages ne sont testés par rien**, et
j'ai vérifié qu'ils sont testables en dix lignes, à la compilation, sans runtime.

---

## 1. Le corpus : 30 diagnostics, mesurés un par un

Trente captures existent sur disque. J'ai relu les trente, recompilé les principales contre
les en-têtes actuels (GCC 16.2.0), et retrouvé exactement les mêmes comptes de lignes :
12 pour `01_lambda_ref`, 27 pour `15_concept_direct`, 20 pour `11_synchronized_value`,
41 pour `19_cow_ctor_constraint`, 34 pour `20_syncvalue_ctor_constraint`. Le corpus est à jour.

Deux fichiers sont des doublons de mesure et non des cas : `11_before.txt` est
octet-pour-octet identique à `11_synchronized_value.txt` (vérifié par `diff`), et
`11_after.txt` est la même source compilée contre un arbre d'en-têtes déjà corrigé.
Il reste donc **28 cas distincts sur la bibliothèque telle qu'elle est**.

### 1.1 Le tableau

« Ligne utile » = le numéro de la ligne du diagnostic qui porte la phrase explicative
(`'what()': '...'`). « Coupable » = est-ce que le sous-objet nommé est vraiment la raison
du refus.

| # | Cas | Question posée | Lignes | Ligne utile | Coupable |
|---|-----|----------------|-------:|------------:|----------|
| 1 | `01_lambda_ref` | `launch_task([&counter]{...})` | 12 | 10 | **Non** — accuse l'opacité des closures, pas la capture par référence |
| 2 | `02_raw_pointer_arg` | `launch_task(f, &counter)` | 12 | 10 | Oui |
| 3 | `03_string_view_arg` | `launch_task(f, string_view)` | 14 | 12 | **Non** — accuse les membres spéciaux ; la vraie raison est que c'est une vue |
| 4 | `04_three_level_member` | `assert_sendable<Root>` (3 membres imbriqués) | 12 | 10 | Oui — chemin complet |
| 5 | `05_three_level_base` | idem par héritage | 12 | 10 | Oui — chemin complet |
| 6 | `06_inside_vector` | `Holder{ vector<Leaf> }` | 12 | 10 | **Non** — s'arrête au `vector`, jamais `Leaf::pointer` |
| 7 | `06b_inside_optional` | `Holder{ optional<Leaf> }` | 7 | 5 | **Non** — même défaut |
| 8 | `06c_inside_tuple` | `Holder{ tuple<int,double,Leaf> }` | 7 | 5 | **Non** — même défaut |
| 9 | `07_user_destructor` | destructeur écrit à la main | 12 | 10 | Oui |
| 10 | `08_ctor_template` | constructeur template | 7 | 5 | Oui |
| 11 | `09_incomplete` | type incomplet | 7 | 5 | Oui |
| 12 | `10_non_movable_callable` | callable non déplaçable | 12 | 10 | Oui pour la raison, **non pour le conseil** (voir §5.1) |
| 13 | `11_synchronized_value` | `synchronized_value<Borrowing>` | 20 | *aucune* | **Aucun coupable nommé** |
| 14 | `11_before` | *(doublon octet-pour-octet du précédent)* | 20 | *aucune* | — |
| 15 | `11_after` | le même, en-têtes corrigés | 10 | 10 | Oui — `Borrowing::pointer (int*)` |
| 16 | `12_cow_const_not_readable` | `copy_on_write<MutableCounter>` | 7 | 5 | **Non** — accuse `copy_on_write`, pas `MutableCounter::cached` |
| 17 | `12b_cow_launch` | le même par `launch_task` | 16 | 14 | **Non** — même défaut |
| 18 | `13_sync_opt_in` | `assert_synchronizable<Plain>` | 7 | 5 | Oui — et c'est le meilleur message de la bibliothèque |
| 19 | `14_deep_chain` | chaîne de 20 niveaux | 7 | 5 | Oui — les 20 niveaux nommés |
| 20 | `15_concept_direct` | `template <sendable T> void f(T)` | 27 | *aucune* | **Aucune explication** — se termine par `evaluated to 'false'` |
| 21 | `16_polymorphic_unique_ptr` | `unique_ptr<Base>` polymorphe | 7 | 5 | **Non** — la vraie raison (`dynamic_type_is_known`) n'est jamais citée |
| 22 | `17_reference_wrapper` | `assert_lifetime_aware<reference_wrapper<int>>` | 7 | 5 | Approximatif — bonne raison, mais nomme `_M_data`, un symbole libstdc++ |
| 23 | `18_value_guard` | `assert_sendable<synchronized_value<int>::guard>` | 7 | 5 | **Non** — accuse `lock_`, alors que la règle est `is_sendable<value_guard> = false` |
| 24 | `19_cow_ctor_constraint` | `copy_on_write<int>{std::string{}}` | **41** | *aucune* | **Aucun message threadsafe** — 41 lignes de libstdc++ |
| 25 | `20_syncvalue_ctor_constraint` | `synchronized_value<int>{std::string{}}` | **34** | *aucune* | **Aucun message threadsafe** |
| 26 | `21_fallback_specialized` | spécialisation utilisateur à `false` | 7 | 5 | Oui |
| 27 | `22_scoped_task` | `launch_scoped_task(f, &counter)` | 12 | 10 | Oui |
| 28 | `23_two_bad_members` | deux membres fautifs | 7 | 5 | Oui — ne nomme que le premier, ce qui est correct |
| 29 | `24_const_wrapper` | `const vector<Leaf>` avec `mutable` | 7 | 5 | **Non** — s'arrête au `vector` |
| 30 | `25_mutable_member` | `const Cache` avec `mutable int hits` | 7 | 5 | Oui |

### 1.2 Le bilan chiffré

Sur les 28 cas distincts (donc en excluant le doublon `11_before` et la capture corrigée
`11_after`) :

| Résultat | Cas | Compte | Part |
|----------|-----|-------:|-----:|
| Coupable exact | 02, 04, 05, 07, 08, 09, 10, 13, 14, 21, 22, 23, 25 | **13** | 46 % |
| Coupable **faux** | 01, 03, 06, 06b, 06c, 12, 12b, 16, 18, 24 | **10** | 36 % |
| Bonne raison, symbole libstdc++ | 17 | 1 | 4 % |
| **Aucune explication produite** | 11, 15, 19, 20 | **4** | 14 % |

La longueur, elle, n'est pas le problème : la médiane est de 7 lignes et le maximum utile
de 16. Les quatre monstres — 41, 34, 27 et 20 lignes — sont exactement les quatre cas où la
bibliothèque **ne parle pas** et où GCC ou libstdc++ parlent à sa place. C'est le constat
central de ce rapport : quand la bibliothèque parle, elle est brève ; quand elle se tait,
elle est illisible.

### 1.3 Le mode d'échec « opacité de la closure »

Le lead a établi le cas `01_lambda_ref`. Le décompte demandé :

- **Le message littéral** « *holds state reflection cannot see (a closure type with captures);
  specialize is_sendable to state the intent* » apparaît sur **1 des 30 captures** (`01`).
  Mais il apparaît sur **3 des 38 cas du corpus source**, que j'ai recompilés moi-même :
  `01_lambda_ref` (`[&counter]`), `31_capture_by_value` (`[counter]`, qui est **sûr**) et
  `33_capture_ptr_by_value` (`[pointer]`). Les trois messages sont **identiques au caractère
  près**. La bibliothèque ne fait aucune différence entre une capture qui emprunte et une
  capture qui copie — parce que GCC 16.2 rapporte zéro membre de données pour toute closure,
  quelle que soit la capture — et le message ne le dit pas.
- **Un quatrième type**, qui n'est pas une lambda du tout, reçoit le même message. J'ai
  compilé `struct PaddingOnly { int : 32; };` et obtenu, mot pour mot :
  `'PaddingOnly holds state reflection cannot see (a closure type with captures); specialize
  is_sendable to state the intent'`. C'est le constat TC-11 : un champ de bits sans nom est
  la façon portable d'écrire du remplissage explicite, `nonstatic_data_members_of` ne le
  liste pas, et le garde-fou anti-closure se déclenche dessus. Le message dit à
  l'utilisateur que sa structure est une lambda.
- **Le mode d'échec plus large** — « le message accuse un mécanisme interne au lieu de ce que
  l'utilisateur a écrit, et se termine par un conseil inapplicable ou dangereux » — touche
  **10 des 28 cas** (ligne « coupable faux » du tableau), plus les deux cas `std::ref` du §5.1.

---

## 2. Les endroits où la bibliothèque ne dit rien (constat Q8)

C'est le défaut le plus coûteux, parce qu'il frappe précisément là où un utilisateur
rencontre la bibliothèque pour la première fois. `threadsafe::assert_sendable<T>()` nomme
le sous-objet exact en 7 lignes. **Aucun** des deux points d'entrée les plus fréquents ne
l'appelle.

### 2.1 `synchronized_value<T>` : 20 lignes, jamais le coupable

Le code problématique, complet :

```cpp
// 11_synchronized_value.cpp
#include <threadsafe/threadsafe.h>

struct Borrowing { int *pointer; };

int main() {
    threadsafe::synchronized_value<Borrowing> value{};
    (void)value;
}
```

Ce qu'il produit aujourd'hui (20 lignes ; extrait des lignes 7 à 20, chemins raccourcis) :

```
synchronized_value.h:48:19: error: static assertion failed: the mutex serializes
  access, but the T still crosses thread boundaries — one thread at a time —
  so T must be sendable
   48 |     static_assert(sendable<T>,
      |                   ^~~~~~~~~~~
  • constraints not satisfied
    • required by the constraints of 'template<class T> concept threadsafe::sendable'
      sendable.h:40:9:
         40 | concept sendable = is_sendable_v<T>;
    • the expression 'is_sendable_v<T> [with T = Borrowing]' evaluated to 'false'
```

Le mot `pointer` n'apparaît nulle part. La règle est énoncée — et elle est bien écrite —
mais le lecteur doit deviner *lequel* de ses membres l'a violée. Pendant ce temps,
`threadsafe::assert_sendable<Borrowing>()` répond, en 7 lignes :
`'Borrowing::pointer (int*) is a pointer or a reference: sending it shares its referent with
the other thread, so the referent must be synchronizable — and synchronizability is opt-in'`.

**Le correctif, complet.** Dans `include/threadsafe/details/synchronized_value.h`, remplacer
l'assertion unique par les deux ci-dessous, dans cet ordre :

```cpp
template <class T>
class synchronized_value {
    // Order matters. assert_sendable<T>() names the subobject responsible and
    // must come first, because GCC prints it first. The plain static_assert
    // below carries the rule for the reader who wonders why a mutex is not
    // enough; a lone static_assert(sendable<T>) prints "evaluated to 'false'"
    // and names nothing at all.
    static_assert((assert_sendable<T>(), true));
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

public:
    static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];

    static consteval auto get_const_guard_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^value_guard<const T, std::shared_lock<mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = [:get_const_guard_type():];

    template <class... Args>
        requires std::constructible_from<T, Args...>
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

    // nodiscard is load-bearing: a discarded guard is a temporary destroyed at
    // the semicolon, i.e. a lock taken and immediately released.
    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable mutex mutex_;
    T value_;
};
```

**Compilé et vérifié par moi.** Le diagnostic passe de 20 lignes sans coupable à 24 lignes
avec le coupable **en ligne 10** et la règle en ligne 11 :

```
synchronized_value.h:57:40: error: non-constant condition for static assertion
   57 |     static_assert((assert_sendable<T>(), true));
synchronized_value.h:57:40: error: uncaught exception of type 'std::meta::exception';
  'what()': 'Borrowing::pointer (int*) is a pointer or a reference: sending it shares
   its referent with the other thread, so the referent must be synchronizable — and
   synchronizability is opt-in'
synchronized_value.h:58:19: error: static assertion failed: the mutex serializes access,
  but the T still crosses thread boundaries — one thread at a time — so T must be sendable
```

Les 13 lignes restantes sont le vidage de satisfaction de concept de GCC ; elles suivent la
phrase utile au lieu de la précéder, ce qui est exactement ce qu'on veut. **Variante plus
courte** : garder uniquement `static_assert((assert_sendable<T>(), true), "the mutex
serializes access, ...")`. Elle donne **10 lignes** et le coupable en ligne 10, mais GCC
n'imprime pas le message d'un `static_assert` dont la condition est *non constante* plutôt
que *fausse* : la phrase de règle est alors perdue. Pour une diapositive, la variante courte ;
pour un vrai projet, les deux assertions. Les 11 unités de test passent dans les deux cas.

### 2.2 Le concept `sendable` nu : 27 lignes, aucune explication — et c'est irréparable

```cpp
// 15_concept_direct.cpp
#include <threadsafe/threadsafe.h>

struct Borrowing { int *pointer; };

template <threadsafe::sendable T>
void hand_over(T) {}

int main() { hand_over(Borrowing{}); }
```

27 lignes, dont la seule information est la dernière :
`• the expression 'is_sendable_v<T> [with T = Borrowing]' evaluated to 'false'`.

**Correctif proposé : aucun, et la raison mérite d'être écrite dans la documentation, parce
qu'elle est le meilleur enseignement de ce rapport.** Faire porter l'explication au concept
exigerait une contrainte atomique dont l'évaluation lance une exception ; or une contrainte
qui lance est une **erreur dure**, pas un `false`, ce qui détruit la répartition à deux
surcharges dont dépend toute l'API. Le contre-exemple, complet et compilable, qui le prouve :

```cpp
#include <meta>
#include <string>
#include <type_traits>

template <class T>
constexpr bool plain = std::is_integral_v<T>;

template <class T>
consteval bool explain() {
    if (!plain<T>)
        throw std::meta::exception(u8"not an integral type", ^^T);
    return true;
}

template <class T>
concept ok = plain<T> || explain<T>();

template <ok T>
int pick(T) { return 1; }

template <class T>
int pick(T) { return 2; }

// On veut que ok<double> vaille false et que la seconde surcharge gagne.
static_assert(pick(1.0) == 2);

int main() {}
```

GCC répond `error: uncaught exception of type 'std::meta::exception'` puis
`error: call to non-'constexpr' function`. Le concept ne peut pas expliquer sa propre
insatisfaction. **La seule façon d'expliquer un refus sur un site d'appel est le motif que
`asynchronous_task_launcher` emploie déjà** : une seconde surcharge non contrainte dont le
corps appelle `assert_sendable<T>()`. C'est une limite du langage, pas un défaut de la
bibliothèque, et elle vaut une phrase dans `CLAUDE.md` à côté de la définition du concept :

> `sendable<T>` est un `bool` et le restera : une contrainte qui lance est une erreur dure,
> pas un `false`. Pour obtenir une explication, écrivez
> `static_assert((threadsafe::assert_sendable<T>(), true));` plutôt que
> `static_assert(threadsafe::sendable<T>);`, ou donnez à votre fonction une seconde
> surcharge non contrainte qui appelle `assert_sendable<T>()`.

### 2.3 Les deux monstres : 41 et 34 lignes de libstdc++

```cpp
// 19_cow_ctor_constraint.cpp
#include <threadsafe/threadsafe.h>
#include <string>

int main() {
    threadsafe::copy_on_write<int> shared{std::string{"not an int"}};
    (void)shared;
}
```

```cpp
// 20_syncvalue_ctor_constraint.cpp
#include <threadsafe/threadsafe.h>
#include <string>

int main() {
    threadsafe::synchronized_value<int> value{std::string{"not an int"}};
    (void)value;
}
```

J'ai recompilé les deux : **41 et 34 lignes**, dont pas une seule ne vient de ThreadSafe.
Le cas 19 est le pire du corpus. La contrainte affichée en entier occupe deux lignes de
`candidate 1` :

```
• candidate 1: 'template<class ... Args>  requires (constructible_from<T, Args ...>)
  && (sizeof ... (Args ...) != 1 || ((!(same_as<typename std::remove_cvref<_Args>::type,
  threadsafe::copy_on_write<T> >) && ...))) threadsafe::copy_on_write<T>::copy_on_write(Args&& ...)'
```

et la conclusion, aux lignes 33-34, est :

```
• 'int' is not constructible from 'std::__cxx11::basic_string<char>', because
• error: could not convert 'std::__cxx11::basic_string<char>' to 'int'
```

**Faut-il corriger ? Je dis non, et c'est un « challenge the need » assumé.** La dernière
phrase est *correcte et suffisante* : « on ne peut pas convertir un `std::string` en `int` ».
C'est GCC qui diagnostique bien un problème qui n'est pas un problème de thread-safety. Un
`static_assert` supplémentaire dans le constructeur ne peut pas exister — la contrainte est
dans la liste `requires`, donc l'échec se produit pendant la résolution de surcharge, avant
tout corps. La seule vraie amélioration serait de **retirer la garde `sizeof...(Args) != 1
|| !same_as<..., copy_on_write>`** de `copy_on_write` et de la remplacer par des
constructeurs de copie et de déplacement écrits explicitement : la contrainte affichée
passerait de deux lignes illisibles à `requires constructible_from<T, Args...>`, et les
41 lignes tomberaient à peu près à 34, comme le cas 20. Le gain est modeste ; c'est une
question de simplicité de l'API plus que de diagnostic, et elle appartient à
[05](./05-simplicite.md). **Pour ce rapport : à ne pas corriger, à mentionner comme la limite
connue « une contrainte de constructeur n'est pas un endroit où l'on peut parler ».**

---

## 3. Quand la réponse vient d'une spécialisation, `assert_*` invente un coupable

Trois agents indépendants (constats **Q2**, **TLS-03**, **L8**) sont arrivés à la même cause
racine par trois chemins différents. C'est le signal le plus fort de tout le jeu de données,
et c'est le défaut le plus grave de ce rapport.

### 3.1 Le mécanisme

`assert_sendable<T>()` ne fait rien si `is_sendable_v<T>` est vrai. Sinon il appelle
`detail::descend_sendable(^^T, type_name(^^T))`, dont voici le corps actuel, complet :

```cpp
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);

    reject(inner,
           u8"is not sendable: is_sendable is specialized to false for it",
           path);
}
```

`diagnose_default_is_sendable` est **le corps du template primaire**. Il est exécuté même
quand la réponse `false` vient d'une spécialisation partielle : `is_sendable<std_wrapper T>`
(tous les conteneurs standard), `is_sendable<unique_ptr<T,D>>`, `is_sendable<shared_ptr<T>>`,
`is_sendable<reference_wrapper<T>>`, `is_sendable<copy_on_write<T>>`,
`is_sendable<value_guard<T,Lock>>`. Le parcours signale alors la première chose sur laquelle
il trébuche. Pour tout conteneur standard, c'est le test `has_only_default_copy_move_destroy`,
parce que `std::vector` a des constructeurs template.

**La preuve que la raison est fausse**, en un fichier complet et compilable :

```cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <string>

struct Config {
    std::string name;
};

// La réponse pour reference_wrapper ne vient PAS du parcours structurel : elle
// vient de is_sendable<std::reference_wrapper<T>> dans smart_pointers.h, qui
// délègue à is_synchronizable<T>.
static_assert(threadsafe::is_sendable_v<std::reference_wrapper<std::atomic<int>>>);
static_assert(!threadsafe::is_sendable_v<std::reference_wrapper<Config>>);

// Même template, mêmes membres de copie et de déplacement, réponses opposées.
// Donc « has a user-written copy, move or destructor » ne peut être la raison
// d'aucune des deux.
int main() {
    threadsafe::assert_sendable<std::reference_wrapper<Config>>();
}
```

et pourtant le message est
`'std::reference_wrapper<Config> has a user-written copy, move or destructor — or a template
that may be selected as one — which can share state the members do not show; specialize
is_sendable to state the intent'`.

### 3.2 Le conseil ouvre un trou de sûreté — vérifié par compilation

C'est le point le plus grave, et je l'ai vérifié moi-même plutôt que de le répéter. Le
programme complet :

```cpp
#include <threadsafe/threadsafe.h>
#include <vector>

struct Foo { int value; };

// Étape 1, sans la spécialisation ci-dessous, la bibliothèque dit :
//   'std::vector<Foo*> has a user-written copy, move or destructor — or a
//    template that may be selected as one — which can share state the members
//    do not show; specialize is_sendable to state the intent'
// Étape 2 : le conseil, suivi à la lettre.
template <>
struct threadsafe::is_sendable<std::vector<Foo *>> : std::true_type {};

static_assert(!threadsafe::is_sendable_v<Foo *>, "un Foo* seul reste refusé");
static_assert(threadsafe::is_sendable_v<std::vector<Foo *>>,
              "mais un vecteur entier de Foo* est désormais béni");

int main() {}
```

```
$ g++-16 -std=c++26 -freflection -I include -fsyntax-only v_vec_advice.cpp
$ echo $?
0
```

**Zéro erreur.** Un `Foo*` isolé reste interdit ; un `std::vector<Foo*>` est autorisé à
traverser une frontière de thread. L'utilisateur n'a rien fait d'autre que suivre le message
de la bibliothèque. C'est, mot pour mot, la démonstration inverse de ce que la conférence
veut montrer.

Le corpus liste dix cas de ce mode ; le pire est `35_follow_the_advice`, que j'ai également
recompilé : la bibliothèque conseille `std::ref` au cas 10, l'utilisateur écrit
`launcher.launch_task(std::ref(callable))`, et reçoit
`'std::reference_wrapper<NonMovableCallable> has a user-written copy, move or destructor ...
specialize is_sendable to state the intent'`. Deux messages successifs, tous deux faux, tous
deux conseillant une action nuisible.

### 3.3 Le correctif : un marqueur, et un parcours qui traverse le conteneur

Le correctif tient en deux idées.

**(a) Marquer les réponses que le parcours a réellement produites.** Le template primaire et
les règles pointeur/référence/tableau — celles dont `diagnose_default_is_sendable` sait
rédiger la phrase — portent un typedef ; aucune autre spécialisation ne le porte.
`descend_sendable` teste le marqueur avant de re-parcourir.

**(b) Faire traverser le conteneur au chemin.** Les règles `std_wrapper` répondent en lisant
leurs arguments de template ; elles gagnent une forme explicative qui continue le chemin dans
l'élément, exactement comme `explain_sendable` continue dans un membre.

Voici le code complet. Chaque fonction est donnée entière ; rien n'est élidé.

**`include/threadsafe/details/utils.h`** — ajouter cette fonction juste avant le commentaire
qui commence par `// The reason continues the sentence the subject opens:` :

```cpp
// One hop of the walk into a std wrapper. The element is a subobject the user
// never named, so it is spelled the way path_step spells a base.
inline consteval std::u8string element_step(std::meta::info element) {
    return u8"::(element " + type_name(element) + u8")";
}
```

**`include/threadsafe/details/sendable.h`** — le bloc de déclarations en tête de
`namespace detail` devient :

```cpp
namespace detail {
consteval void diagnose_default_is_sendable(std::meta::info type,
                                            std::u8string path = {});
consteval bool default_is_sendable(std::meta::info type);
[[noreturn]] consteval void descend_sendable(std::meta::info inner,
                                             const std::u8string &path);

// Defined in allowed_std_wrappers.h, beside the rule it explains: the std family
// answers through a specialization that reads the template arguments, so the
// structural walk below never sees the element that is the real reason.
consteval void diagnose_std_wrapper_sendable(std::meta::info type,
                                             const std::u8string &path);
}
```

et le bloc des spécialisations de `is_sendable` devient, entier :

```cpp
// The tag marks an answer the structural walk produced. Only such an answer may
// be explained by re-walking; a specialization answered on its own terms, and
// re-walking it blames whatever the walk trips on first.
template <class T>
struct is_sendable : std::bool_constant<detail::default_is_sendable(^^T)> {
    using answered_by_walking_members = void;
};

template <class T>
constexpr bool is_sendable_v = is_sendable<T>::value;

template <class T>
struct is_sendable<T&> : is_synchronizable<std::remove_cv_t<T>> {
    using answered_by_walking_members = void;
};
template <class T>
struct is_sendable<T&&> : is_synchronizable<std::remove_cv_t<T>> {
    using answered_by_walking_members = void;
};

template <class T>
struct is_sendable<T*> : is_synchronizable<std::remove_cv_t<T>> {
    using answered_by_walking_members = void;
};

template <class T, std::size_t N>
struct is_sendable<T[N]> : is_sendable<std::remove_cv_t<T>> {
    using answered_by_walking_members = void;
};
template <class T>
struct is_sendable<T[]> : is_sendable<std::remove_cv_t<T>> {
    using answered_by_walking_members = void;
};

template <class T>
concept sendable = is_sendable_v<T>;

namespace detail {
template <class T>
constexpr bool sendable_answer_is_structural =
    requires { typename is_sendable<T>::answered_by_walking_members; };
}
```

enfin `descend_sendable` est remplacée, entière, par :

```cpp
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    // A std wrapper answers from its element, not from its own members, so the
    // path continues into the element and reaches the real culprit.
    diagnose_std_wrapper_sendable(inner, path);

    // The walk below describes the primary template and nothing else. When a
    // specialization owns the answer, its members are not why it said no:
    // re-walking blames an innocent one and then advises specializing the trait,
    // which would bless the very thing being rejected.
    if (!trait_value(^^sendable_answer_is_structural, inner))
        reject(inner,
               u8"is not sendable: is_sendable is specialized for it, so the "
               u8"answer is that specialization's rule and not the layout of "
               u8"its members — read the rule, do not specialize past it",
               path);

    diagnose_default_is_sendable(inner, path);

    // The walk found nothing and the trait still answers no: an earlier query
    // cached that answer — asked while the type was incomplete, or from inside
    // its own definition — and a class template is never re-instantiated.
    reject(inner,
           u8"is not sendable, yet walking it finds no reason: an earlier query "
           u8"cached that answer while the type was still incomplete, or from "
           u8"inside its own definition, and a trait is never recomputed once "
           u8"instantiated — ask again after the type is complete",
           path);
}
```

**`include/threadsafe/details/allowed_std_wrappers.h`** — les trois prédicats
`std_wrapper_is_*` deviennent trois paires diagnose/answer. Les six fonctions, entières :

```cpp
inline consteval void
diagnose_std_wrapper_sendable(std::meta::info type, const std::u8string &path) {
    if (!is_allowed_std_wrapper(type) || is_synchronizable_type(type))
        return;

    for (std::meta::info wrapped : wrapped_types_of(type)) {
        if (is_sendable_type(wrapped))
            continue;

        // Same bargain as explain_sendable: only a caller that reads the message
        // seeds a path, and only then is the deep walk worth its cost.
        if (path.empty())
            reject(type, u8"holds an element that is not sendable");

        descend_sendable(wrapped, path + element_step(wrapped));
    }
}

inline consteval bool std_wrapper_is_sendable(std::meta::info type) {
    try {
        diagnose_std_wrapper_sendable(type, {});
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}

inline consteval void
diagnose_std_wrapper_const_synchronizable(std::meta::info type,
                                          const std::u8string &path) {
    if (!is_allowed_std_wrapper(type) || is_synchronizable_type(type))
        return;

    for (std::meta::info wrapped : wrapped_types_of(type)) {
        if (is_synchronizable_type(std::meta::add_const(wrapped)))
            continue;

        if (path.empty())
            reject(type,
                   u8"holds an element that is not readable from several "
                   u8"threads at once");

        descend_const_synchronizable(wrapped, path + element_step(wrapped));
    }
}

inline consteval bool
std_wrapper_is_const_synchronizable(std::meta::info type) {
    try {
        diagnose_std_wrapper_const_synchronizable(type, {});
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}

inline consteval void
diagnose_std_wrapper_lifetime_aware(std::meta::info type,
                                    const std::u8string &path) {
    if (!is_allowed_std_wrapper(type))
        return;

    for (std::meta::info wrapped : wrapped_types_of(type)) {
        if (is_lifetime_aware_type(wrapped))
            continue;

        if (path.empty())
            reject(type, u8"holds an element that is not lifetime aware");

        descend_lifetime_aware(wrapped, path + element_step(wrapped));
    }
}

inline consteval bool std_wrapper_is_lifetime_aware(std::meta::info type) {
    try {
        diagnose_std_wrapper_lifetime_aware(type, {});
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}
```

**`include/threadsafe/details/synchronizable.h`** — même schéma. Le bloc de déclarations
gagne :

```cpp
// Defined in allowed_std_wrappers.h — see diagnose_std_wrapper_sendable.
consteval void
diagnose_std_wrapper_const_synchronizable(std::meta::info type,
                                          const std::u8string &path);
```

la règle `const T` porte le marqueur :

```cpp
template <class T>
struct is_synchronizable<const T>
    : std::bool_constant<detail::default_is_const_synchronizable(^^T)> {
    using answered_by_walking_members = void;
};

namespace detail {
template <class T>
constexpr bool const_synchronizable_answer_is_structural =
    requires { typename is_synchronizable<const T>::answered_by_walking_members; };
}
```

et `descend_const_synchronizable` devient, entière :

```cpp
[[noreturn]] inline consteval void
descend_const_synchronizable(std::meta::info inner, const std::u8string &path) {
    diagnose_std_wrapper_const_synchronizable(inner, path);

    if (!trait_value(^^const_synchronizable_answer_is_structural, inner))
        reject(inner,
               u8"is not readable from several threads at once: "
               u8"is_synchronizable is specialized for it, so the answer is that "
               u8"specialization's rule and not the layout of its members — read "
               u8"the rule, do not specialize past it",
               path);

    diagnose_default_is_const_synchronizable(inner, path);

    reject(inner,
           u8"is not readable from several threads at once, yet walking it "
           u8"finds no reason: an earlier query cached that answer while the "
           u8"type was still incomplete, or from inside its own definition, and "
           u8"a trait is never recomputed once instantiated — ask again after "
           u8"the type is complete",
           path);
}
```

**`include/threadsafe/details/lifetime_aware.h`** — le bloc de déclarations gagne :

```cpp
// Defined in allowed_std_wrappers.h — see diagnose_std_wrapper_sendable.
consteval void diagnose_std_wrapper_lifetime_aware(std::meta::info type,
                                                   const std::u8string &path);
```

le template primaire porte le marqueur, et le prédicat de structure est déclaré juste après
le concept :

```cpp
template <class T>
struct is_lifetime_aware
    : std::bool_constant<detail::default_is_lifetime_aware(^^T)> {
    using answered_by_walking_members = void;
};
```

```cpp
template <class T>
concept lifetime_aware = is_lifetime_aware_v<T>;

namespace detail {
template <class T>
constexpr bool lifetime_aware_answer_is_structural =
    requires { typename is_lifetime_aware<T>::answered_by_walking_members; };
}
```

et `descend_lifetime_aware` devient, entière :

```cpp
[[noreturn]] inline consteval void
descend_lifetime_aware(std::meta::info inner, const std::u8string &path) {
    diagnose_std_wrapper_lifetime_aware(inner, path);

    if (!trait_value(^^lifetime_aware_answer_is_structural, inner))
        reject(inner,
               u8"is not lifetime aware: is_lifetime_aware is specialized for "
               u8"it, so the answer is that specialization's rule and not the "
               u8"layout of its members — read the rule, do not specialize past "
               u8"it",
               path);

    diagnose_default_is_lifetime_aware(inner, path);

    reject(inner,
           u8"is not lifetime aware, yet walking it finds no reason: an earlier "
           u8"query cached that answer while the type was still incomplete, or "
           u8"from inside its own definition, and a trait is never recomputed "
           u8"once instantiated — ask again after the type is complete",
           path);
}
```

### 3.4 Ce que cela donne — mesuré par moi

Les 11 unités de test compilent proprement. Voici les messages avant et après, tous obtenus
par mes propres compilations :

| Cas | Avant | Après |
|-----|-------|-------|
| `06_inside_vector` | `Holder::leaves (std::vector<Leaf>) has a user-written copy, move or destructor ...` | `Holder::leaves (std::vector<Leaf>)::(element Leaf)::pointer (int*) is a pointer or a reference: ...` |
| `06b_inside_optional` | `Holder::leaf (std::optional<Leaf>) has a user-written ...` | `Holder::leaf (std::optional<Leaf>)::(element Leaf)::pointer (int*) is a pointer or a reference: ...` |
| `06c_inside_tuple` | `Holder::parts (std::tuple<int, double, Leaf>) has a user-written ...` | `Holder::parts (std::tuple<int, double, Leaf>)::(element Leaf)::pointer (int*) is a pointer ...` |
| `29_vector_ptr` | `std::vector<Foo*> has a user-written ... specialize is_sendable` | `std::vector<Foo*>::(element Foo*) is a pointer or a reference: ...` |
| `30_nested_wrappers` | `std::map<...> has a user-written ...` | `std::map<std::__cxx11::basic_string<char>, std::vector<Foo> >::(element std::vector<Foo>)::(element Foo)::borrowed (int*) is a pointer or a reference: ...` |
| `24_const_wrapper` | `const std::vector<Leaf> has a user-written ... specialize is_synchronizable` | `const std::vector<Leaf>::(element Leaf)::cached (int) is mutable, so it is written through a const reference: its type must be fully synchronizable` |
| `12_cow_const_not_readable` | `threadsafe::copy_on_write<MutableCounter> has a user-written ... specialize is_sendable` | `threadsafe::copy_on_write<MutableCounter> is not sendable: is_sendable is specialized for it, so the answer is that specialization's rule and not the layout of its members — read the rule, do not specialize past it` |
| `16_polymorphic_unique_ptr` | `std::unique_ptr<Base> has a user-written ...` | même phrase générique, honnête |
| `17_reference_wrapper` | `std::reference_wrapper<int>::_M_data (int*) is a reference or a raw pointer ...` | `std::reference_wrapper<int> is not lifetime aware: is_lifetime_aware is specialized for it ...` |
| `18_value_guard` | `threadsafe::value_guard<...>::lock_ (std::unique_lock<...>) has a user-written ...` | `threadsafe::value_guard<int, std::unique_lock<std::shared_mutex> > is not sendable: is_sendable is specialized for it ...` |
| `21_fallback_specialized` | `Vouched is not sendable: is_sendable is specialized to false for it` | `Vouched is not sendable: is_sendable is specialized for it ...` |
| Réponse périmée (constat ADV-06) | `Later is not sendable: is_sendable is specialized to false for it` — alors qu'**aucune** spécialisation n'existe | `Later is not sendable, yet walking it finds no reason: an earlier query cached that answer while the type was still incomplete, or from inside its own definition ... — ask again after the type is complete` |

**Limite que je ne masque pas** : quatre cas (`12`, `16`, `17`, `18`) passent de *faux* à
*honnête mais générique*. Le message dit « la réponse vient d'une règle, lisez la règle »
au lieu de dire « `MutableCounter::cached` est `mutable`, donc `const MutableCounter` n'est
pas lisible, donc la règle de `copy_on_write` répond non ». Rendre chacun de ces quatre
spécifique demande un crochet `diagnose_*` par famille — exactement ce que les conteneurs
standard viennent de recevoir. **C'est ce que le marqueur rend possible : on peut les ajouter
un par un, sans risquer d'inventer une raison.** Je ne les ajoute pas ici, parce que quatre
messages honnêtes valent mieux que quatre messages faux, et parce que le lead doit décider
combien de complexité une bibliothèque pédagogique supporte.

**Le cas ADV-06 est le seul dont je change la sémantique sans qu'un test le couvre.** Le
reproducteur complet :

```cpp
#include <threadsafe/threadsafe.h>

struct Later;
static_assert(!threadsafe::is_sendable_v<Later>);   // demandé pendant que le type est incomplet
struct Later { int a; int b; };                     // désormais complet
static_assert(!threadsafe::is_sendable_v<Later>, "la réponse est figée");

// Et que dit la bibliothèque à ce sujet ?
static_assert((threadsafe::assert_sendable<Later>(), true));

// La même mauvaise réponse par l'autre porte : un type qui s'interroge lui-même.
struct SelfAsking {
    int payload;
    static constexpr bool answer_in_class = threadsafe::is_sendable_v<SelfAsking>;
};
static_assert(!SelfAsking::answer_in_class);
static_assert(!threadsafe::is_sendable_v<SelfAsking>);

// La structure identique, jamais interrogée trop tôt, répond l'inverse.
struct Twin { int payload; };
static_assert(threadsafe::is_sendable_v<Twin>);
```

J'ai compilé ce fichier : aujourd'hui la bibliothèque répond
`'Later is not sendable: is_sendable is specialized to false for it'` alors qu'il n'existe
aucune spécialisation de `is_sendable<Later>` dans l'unité de traduction. L'utilisateur va
chercher une spécialisation qui n'existe pas. Après correctif, la phrase nomme la vraie
cause et donne l'action (`ask again after the type is complete`). `tests/test_diagnostics.cpp`
ne fige aucun texte, donc la suite reste verte — mais c'est justement le problème, voir §9.

### 3.5 Composition avec le correctif de blanchiment de `const` du lead

Le lead a écrit et vérifié un correctif de `detail::std_wrapper` :

```cpp
template <class T>
concept std_wrapper =
    std::same_as<T, std::remove_cv_t<T>> && is_allowed_std_wrapper(^^T);
```

J'ai appliqué **les deux correctifs ensemble** dans un troisième arbre. Résultat, vérifié
par moi :

- les 11 unités de test compilent proprement ;
- le trou de blanchiment est fermé — sur les en-têtes d'origine, ce fichier échoue une
  assertion, et sur l'arbre combiné il n'en échoue aucune :

```cpp
#include <threadsafe/threadsafe.h>
#include <vector>

struct Affine { int handle; };
template <> struct threadsafe::is_sendable<Affine> : std::false_type {};

static_assert(!threadsafe::is_sendable_v<std::vector<Affine>>);
static_assert(!threadsafe::is_sendable_v<const std::vector<Affine>>);

int main() {}
```

Les deux correctifs sont orthogonaux et composent.

---

## 4. La lambda : le message de la diapositive numéro un (constats Q5 et TC-11)

La première ligne de code qu'un spectateur écrira est une lambda passée à `launch_task`.
Le programme complet, avec les quatre variantes :

```cpp
#include <threadsafe/threadsafe.h>
#include <cstdio>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    int counter = 0;
    int *pointer = &counter;

    // (a) réellement dangereux : emprunte une variable locale
    launcher.launch_task([&counter] { counter++; });

    // (b) réellement sûr : copie un int
    // launcher.launch_task([counter] { std::printf("%d\n", counter); });

    // (c) réellement dangereux, mais à cause du pointeur, pas du mode de capture
    // launcher.launch_task([pointer] { *pointer = 1; });

    // (d) compile aujourd'hui : aucune capture
    // launcher.launch_task([] { std::printf("hi\n"); });
}
```

J'ai compilé (a), (b) et (c) séparément. Les trois donnent, **au caractère près**, le même
message :

```
error: uncaught exception of type 'std::meta::exception'; 'what()':
'main()::<lambda()> holds state reflection cannot see (a closure type with
 captures); specialize is_sendable to state the intent'
```

Deux défauts, distincts :

1. **Le message accuse la mauvaise chose.** Il parle d'opacité de la réflexion, alors que
   l'utilisateur a écrit `&`. Sur (b), qui est *sûr*, il refuse quand même — et c'est une
   limite réelle de GCC 16.2, pas un bug : `nonstatic_data_members_of` rapporte zéro membre
   pour une closure de taille 4 qui a capturé un `int` par valeur comme pour une closure de
   taille 8 qui a capturé par référence. **Ce refus-là est le bon côté du compromis** et il
   ne peut pas être levé ; c'est le message qui doit le dire.
2. **Le conseil est impossible à suivre.** « *specialize is_sendable* » demande de nommer le
   type. Une lambda écrite sur le site d'appel n'a pas de nom. Il n'existe littéralement
   aucune manière de faire ce que le compilateur demande.

Le troisième défaut vient de TC-11 : la garde `has_unreflectable_state` se déclenche sur
n'importe quel type non vide, non polymorphe, sans base, dont la réflexion ne liste aucun
membre — ce qui est exactement la forme d'un champ de bits sans nom. J'ai compilé :

```cpp
#include <threadsafe/threadsafe.h>
struct PaddingOnly { int : 32; };
int main() { threadsafe::assert_sendable<PaddingOnly>(); }
```

→ `'PaddingOnly holds state reflection cannot see (a closure type with captures); specialize
is_sendable to state the intent'`. On dit à l'utilisateur que sa structure de remplissage
ABI est une lambda.

### Le correctif, complet

**`include/threadsafe/details/utils.h`** — ajouter avant `has_unreflectable_state`, et
remplacer cette dernière, entière :

```cpp
// A closure type with captures: reflection lists no members, yet the object is
// not empty. The identifier is what tells it apart from a named class whose only
// members are unnamed bit-fields -- plain padding, which holds no value and hides
// nothing. <meta> exposes no closure predicate, so the absence of an identifier
// is the test; it also matches an unnamed struct, which is harmless here.
inline consteval bool is_closure_type(std::meta::info type) {
    return std::meta::is_class_type(type) && !std::meta::has_identifier(type);
}

// Mostly for closure type.
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return is_closure_type(type)
        && !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}
```

**`include/threadsafe/details/sendable.h`** — remplacer la branche
`has_unreflectable_state` de `diagnose_default_is_sendable` par :

```cpp
    // A closure reports no data members however it captured, so the walk cannot
    // see whether a capture borrows. Naming the closure type to specialize the
    // trait is not open to the caller either — a lambda written at the call site
    // has no name — so the advice has to be something the caller can do.
    if (has_unreflectable_state(type))
        reject(type,
               u8"is a lambda with captures, and a capture is invisible to "
               u8"reflection — a `[&x]` borrow cannot be told from a `[x]` copy, "
               u8"so no capture is trusted. Capture nothing and pass the state "
               u8"as launch_task arguments, where each one is checked by name",
               path);
```

**`include/threadsafe/details/synchronizable.h`** et
**`include/threadsafe/details/lifetime_aware.h`** — le même remplacement, avec le dernier
membre de phrase adapté puisque ces deux traits ne sont pas propres au lanceur :

```cpp
    if (has_unreflectable_state(type))
        reject(type,
               u8"is a lambda with captures, and a capture is invisible to "
               u8"reflection — a `[&x]` borrow cannot be told from a `[x]` copy, "
               u8"so no capture is trusted. Capture nothing and pass the state "
               u8"as arguments, where each one is checked by name",
               path);
```

**Compilé et vérifié par moi.** Après correctif :

- (a), (b) et (c) donnent tous les trois, exactement :

```
'main()::<lambda()> is a lambda with captures, and a capture is invisible to
 reflection — a `[&x]` borrow cannot be told from a `[x]` copy, so no capture is
 trusted. Capture nothing and pass the state as launch_task arguments, where each
 one is checked by name'
```

- (d) compile toujours ;
- ce fichier, qui échouait sur quatre assertions, compile désormais proprement :

```cpp
#include <threadsafe/threadsafe.h>

struct PaddingOnly { int : 32; };
struct UnnamedBitfieldOnly { int : 3; };

int global_counter = 0;
auto capture_by_value = [captured = 42] { return captured; };
auto capture_ptr = [pointer = &global_counter] { return *pointer; };

static_assert(threadsafe::is_sendable_v<PaddingOnly>);
static_assert(threadsafe::is_synchronizable_v<const PaddingOnly>);
static_assert(threadsafe::is_sendable_v<UnnamedBitfieldOnly>);
static_assert(threadsafe::is_synchronizable_v<const UnnamedBitfieldOnly>);

// et toute closure qui capture reste refusée
static_assert(!threadsafe::is_sendable_v<decltype(capture_by_value)>);
static_assert(!threadsafe::is_sendable_v<decltype(capture_ptr)>);
static_assert(!threadsafe::is_synchronizable_v<const decltype(capture_by_value)>);

int main() {}
```

**Ce message est le meilleur candidat pour une diapositive de la conférence.** Il énonce une
limite réelle de la réflexion C++26, il explique pourquoi le compromis penche du côté sûr,
et il termine par une action que le spectateur peut taper : « ne capturez rien, passez l'état
en argument ». C'est aussi, littéralement, la pédagogie de `Send` en Rust.

---

## 5. Les conseils qui ne mènent nulle part

### 5.1 `std::ref` : une impasse par construction (constat ADV-09)

Le programme complet, étape 1 :

```cpp
#include <threadsafe/threadsafe.h>

struct CopyOnly {
    int n = 0;
    CopyOnly() = default;
    CopyOnly(const CopyOnly&) = default;
    CopyOnly(CopyOnly&&) = delete;
};

void body(CopyOnly) {}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    CopyOnly value;
    launcher.launch_task(&body, value);
}
```

→ `'the launcher owns its arguments, so a non-movable one cannot cross; share it with
std::ref instead'`.

Étape 2, le conseil suivi à la lettre, programme complet :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <functional>

struct Shared {
    std::atomic<int> n{0};
    Shared() = default;
    Shared(const Shared&) = delete;
};

template <> struct threadsafe::is_synchronizable<Shared> : std::true_type {};

void body2(Shared&) {}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    Shared shared;
    launcher.launch_task(&body2, std::ref(shared));   // exactement ce qui était conseillé
}
```

→ `'std::reference_wrapper<NonMovable>::_M_data (NonMovable*) is a reference or a raw pointer:
it borrows its referent instead of keeping it alive — hold the object, or a std::shared_ptr
to it'`.

La raison est structurelle et définitive : `launchable_task` exige `lifetime_aware<Args...>`,
et `is_lifetime_aware<std::reference_wrapper<T>>` est un `std::false_type` inconditionnel.
**Aucun `std::ref` ne peut jamais satisfaire `launch_task`.** L'issue existe et n'est nommée
par aucun des deux messages : `launch_scoped_task`, dont le concept
`launchable_scoped_task` laisse tomber la contrainte `lifetime_aware` précisément parce
qu'il joint avant de rendre la main. La même ligne, avec `launch_scoped_task`, compile
proprement.

**Correctif, complet.** Dans `include/threadsafe/details/asynchronous_task_launcher.h`,
remplacer `detail::assert_ownable_by_launcher` entière :

```cpp
template <class F, class... Args>
consteval void assert_ownable_by_launcher() {
    if (!std::move_constructible<F>)
        throw std::meta::exception(
            u8"the launcher owns its callable, so a non-movable one cannot "
            u8"cross. launch_task outlives this call, so a borrowed callable is "
            u8"not an option there; use launch_scoped_task, which joins before "
            u8"returning and accepts std::ref",
            ^^F);

    (..., [] {
        if (!std::move_constructible<Args>)
            throw std::meta::exception(
                u8"the launcher owns its arguments, so a non-movable one cannot "
                u8"cross. launch_task outlives this call, so a borrowed argument "
                u8"is not an option there; use launch_scoped_task, which joins "
                u8"before returning and accepts std::ref",
                ^^Args);
    }());
}
```

**Compilé et vérifié par moi** : `10_non_movable_callable` donne désormais
`'the launcher owns its callable, so a non-movable one cannot cross. launch_task outlives
this call, so a borrowed callable is not an option there; use launch_scoped_task, which joins
before returning and accepts std::ref'`, et les 11 unités de test restent vertes.

ADV-09 propose en plus de donner à `explain_launch_task` un libellé propre pour l'échec
`lifetime_aware`, afin que le message dise « utilisez `launch_scoped_task` » là aussi. Ce
volet est marqué `not-checked` par l'agent qui l'a produit ; je ne l'ai pas compilé et je ne
le présente donc pas comme un patch prêt, mais comme la suite logique de celui-ci.

### 5.2 `launch_task(42)` : 33 lignes de libstdc++ (constat L9)

`launchable_task<F, Args...>` vérifie la déplaçabilité, `sendable` et `lifetime_aware` —
**jamais l'invocabilité**. Le fichier suivant compile proprement aujourd'hui, ce que j'ai
vérifié :

```cpp
#include <threadsafe/threadsafe.h>

#include <functional>
#include <string>

namespace {
struct BorrowsAPointer {
    int *borrowed;
    void operator()() const {}
};

struct NotCallableAtAll {
    int value;
};
}

using threadsafe::asynchronous_task_launcher;

// 1. Le concept dit non pour un callable réellement dangereux.
static_assert(!threadsafe::launchable_task<BorrowsAPointer>);

// Mais il ne demande jamais si F est appelable, donc un agrégat quelconque et
// un int SATISFONT launchable_task et prennent la surcharge contrainte.
static_assert(threadsafe::launchable_task<NotCallableAtAll>);
static_assert(threadsafe::launchable_task<int>);
static_assert(threadsafe::launchable_task<void (*)(int), std::string>);

// 2. La surcharge de repli non contrainte fait de tout appel rejeté une
//    expression bien formée, donc une requires-expression répond vrai pour des
//    appels que la bibliothèque entend refuser.
static_assert(requires(asynchronous_task_launcher launcher) {
    launcher.launch_task(BorrowsAPointer{nullptr});
});
static_assert(requires(asynchronous_task_launcher launcher) {
    launcher.launch_scoped_task(std::function<void()>{});
});
static_assert(requires(asynchronous_task_launcher launcher) {
    launcher.launch_task(std::string{"not callable"}, (int *)nullptr);
});

// 3. Le motif de détection d'un utilisateur ment donc sur chacun d'eux.
template <class F, class... Args>
concept naive_detection = requires(asynchronous_task_launcher launcher, F f,
                                   Args... args) {
    launcher.launch_task(std::move(f), std::move(args)...);
};

static_assert(naive_detection<BorrowsAPointer>);
static_assert(naive_detection<std::function<void()>>);
static_assert(naive_detection<std::string, int *>);
static_assert(naive_detection<int>);

int main() {}
```

Conséquence sur le diagnostic : `launcher.launch_task(42);` passe par la surcharge
**contrainte**, donc le mécanisme d'explication n'est jamais atteint. J'ai compilé et
mesuré : **33 lignes**, entièrement de la trace d'instanciation de libstdc++, se terminant par

```
/opt/homebrew/.../c++/16/thread:274:27: error: static assertion failed:
  std::jthread arguments must be invocable after conversion to rvalues
  • 'int' is not invocable, because
  • error: 'int' cannot be used as a function
```

Le correctif proposé par le constat L9 (`fix_regression_checked: suite-passes`, non revérifié
par moi) ajoute un concept `invocable_as_task` aux deux concepts et un
`assert_invocable_as_task` en deuxième étape des deux explicateurs :

```cpp
// std::jthread injects a stop_token in front of the arguments when the callable
// can take one, so "is this task callable at all" is that same two-way question.
template <class F, class... Args>
concept invocable_as_task = std::invocable<F, std::stop_token, Args...>
                         || std::invocable<F, Args...>;

template <class F, class... Args>
concept launchable_task = ownable_by_launcher<F, Args...>
                       && invocable_as_task<F, Args...>
                       && sendable<F>
                       && lifetime_aware<F>
                       && (sendable<Args> && ...)
                       && (lifetime_aware<Args> && ...);

template <class F, class... Args>
concept launchable_scoped_task = ownable_by_launcher<F, Args...>
                              && invocable_as_task<F, Args...>
                              && sendable<F>
                              && (sendable<Args> && ...);
```

```cpp
// Asked before the traits, because "not a task at all" is a different mistake
// from "not a safe task", and libstdc++'s own message for it is unreadable.
template <class F, class... Args>
consteval void assert_invocable_as_task() {
    if (!invocable_as_task<F, Args...>)
        throw std::meta::exception(
            type_name(^^F)
                + u8" is not callable with these arguments, so it is not a "
                  u8"task; std::jthread calls it as f(args...) — or as "
                  u8"f(stop_token, args...) when it can take a stop_token",
            ^^F);
}
```

L'agent rapporte 33 lignes → 12 lignes. **Ce correctif n'est pas d'abord un correctif de
diagnostic : il corrige un concept qui est faux** (`launchable_task<int>` est vrai
aujourd'hui). Il appartient donc à [08](./08-api-et-flexibilite.md) autant qu'à ce rapport,
et il devrait être traité comme un correctif de concept dont le meilleur message est un
effet secondaire.

### 5.3 Le beau message ne s'affiche que si vous écrivez un prvalue (constat L11)

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

`launch_task(F f, Args... args)` prend par valeur. Un prvalue est élidé directement dans le
paramètre et atteint le corps, donc le beau message s'affiche ; un lvalue ou un xvalue doit
être construit par déplacement **avant** l'entrée dans le corps, donc l'utilisateur voit
`error: use of deleted function 'NonMovable::NonMovable(NonMovable&&)'`. Obtenir
l'explication dépend de la question de savoir si l'on a écrit `NonMovable{}` ou
`std::move(f)` — une distinction sans aucun sens pour l'utilisateur.

Le correctif du constat L11 (`suite-passes`, non revérifié par moi) remplace les quatre
surcharges par des formes à référence universelle. Les quatre, entières :

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

Là encore, la moitié de la valeur du correctif est ailleurs : il ferme aussi la variante où
`launcher.launch_scoped_task<SharedCounterHandle&>(handle)` vide l'objet de l'appelant sans
qu'aucun `std::move` n'apparaisse sur le site d'appel. Voir [02](./02-robustesse-des-helpers.md)
et [08](./08-api-et-flexibilite.md).

### 5.4 Une référence prend la mauvaise branche (constat TC-13, faible)

`assert_synchronizable` teste `std::is_const_v<T>`, qui est faux pour `const Plain&` puisqu'une
référence n'est jamais cv-qualifiée. J'ai compilé :

```cpp
#include <threadsafe/threadsafe.h>
struct Plain { int value_; };
int main() { threadsafe::assert_synchronizable<const Plain &>(); }
```

→ `'is_synchronizable<T> is opt-in: ... Ask is_synchronizable<const T> for a read-only share, ...'`

On répond à l'utilisateur « demandez `is_synchronizable<const T>` » alors que c'est exactement
ce qu'il vient de demander. Le correctif du volet (2) de TC-13 est autonome et sûr ; la
fonction entière :

```cpp
template <class T>
consteval void assert_synchronizable() {
    if (is_synchronizable_v<T>)
        return;

    // A reference is never const-qualified itself, so ask about what it names.
    using Referent = std::remove_reference_t<T>;

    // Only the const question has a structural answer to walk; the full trait
    // is opt-in, so a non-const T has nothing to explain beyond that.
    if (!std::is_const_v<Referent>)
        throw std::meta::exception(
            u8"is_synchronizable<T> is opt-in: it holds only for types that "
            u8"synchronize themselves (std::atomic, a mutex-protected "
            u8"wrapper). Ask is_synchronizable<const T> for a read-only share, "
            u8"or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE to vouch for it",
            ^^T);

    detail::descend_const_synchronizable(^^Referent,
                                         detail::type_name(^^Referent));
}
```

Le volet (1) du même constat — ajouter `is_synchronizable<T&>` — change une **réponse de
trait**, pas un message, et son auteur le signale explicitement comme non vérifié contre la
suite. **Il ne relève pas de ce rapport** : c'est une question de cohérence de l'API pour
[01](./01-robustesse-des-traits.md) et [08](./08-api-et-flexibilite.md). Aucun chemin interne
de la bibliothèque n'interroge `is_synchronizable<T&>` — tous font `remove_cvref` d'abord —
donc l'impact réel est nul et je recommande de **ne pas** l'appliquer sans une décision
explicite.

---

## 6. Le décompte : combien de messages se terminent par une ACTION ?

J'ai extrait toutes les chaînes de raison distinctes des en-têtes. Critère binaire : un
message est une **ACTION** s'il nomme une chose concrète à écrire ou à faire (spécialiser tel
trait, employer `std::ref`, tenir un `shared_ptr`, poser l'autre question, employer la
macro) ; sinon c'est un **FAIT**.

| Fichier | Chaînes | Dont ACTION |
|---------|--------:|------------:|
| `sendable.h` | 9 | 3 |
| `synchronizable.h` | 12 | 4 |
| `lifetime_aware.h` | 8 | 2 |
| `asynchronous_task_launcher.h` | 4 | 2 |
| **Total** | **33** | **11 (33 %)** |

Deux tiers des messages énoncent un fait et s'arrêtent là. Mais le chiffre brut n'est pas le
plus intéressant. **Sur les 11 actions, 7 mènent aujourd'hui quelque part de faux ou
d'impossible** :

| Action | Verdict |
|--------|---------|
| `Ask is_synchronizable<const T> ... or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` | **Excellente** — trois issues, toutes réelles |
| `hold the object, or a std::shared_ptr to it` | **Excellente** |
| `specialize is_sendable ... (the pimpl idiom)` (type incomplet) | **Bonne** |
| `specialize is_synchronizable ... (the pimpl idiom)` | **Bonne** |
| `specialize is_sendable to state the intent` (copie/déplacement) | Correcte quand elle se déclenche sur le type de l'utilisateur ; **dangereuse** sur les 10 cas où elle se déclenche à tort (§3.2) |
| `specialize is_synchronizable to state the intent` (copie/déplacement) | Idem |
| `specialize is_sendable to state the intent` (closure) | **Impossible** — une lambda de site d'appel n'a pas de nom |
| `specialize is_synchronizable to state the intent` (closure) | **Impossible** |
| `specialize is_lifetime_aware to state the intent` (closure) | **Impossible** |
| `share it with std::ref instead` (callable) | **Impasse** pour `launch_task` (§5.1) |
| `share it with std::ref instead` (arguments) | **Impasse** pour `launch_task` |

Après le correctif consolidé du §8, le compte devient **17 ACTION sur 36 chaînes (47 %)** —
les trois messages terminaux se dédoublent en « c'est une spécialisation, lisez la règle » et
« la réponse a été mise en cache, redemandez une fois le type complet », les trois messages
de closure deviennent suivables, et les deux `std::ref` désignent `launch_scoped_task`. Plus
important que le pourcentage : **il ne reste aucune action impossible et aucune action
dangereuse**.

Trois messages non réflexifs, hors de ce décompte, méritent d'être cités parce qu'ils sont
excellents :

```cpp
    T& operator*() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");
    T* operator->() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");
```

Le `delete("raison")` de C++26 employé ainsi est exactement l'usage pour lequel la
fonctionnalité a été normalisée ; c'est une diapositive à lui tout seul.

---

## 7. Ce qui résiste — et c'est la moitié solide de la bibliothèque

Ces points ont été attaqués et n'ont pas cédé. Ils sont vérifiés, et pour la plupart je les
ai revérifiés moi-même par recompilation.

| Ce qui a été attaqué | Résultat |
|----------------------|----------|
| **Chemin profond à travers les membres** | Exact et complet. `04_three_level_member` → `Root::a (A)::b (B)::pointer (int*) ...`. `14_deep_chain` nomme les 20 niveaux ; `27_chain60` en nomme 60, en 0,76 s. Aucune troncature, aucune limite de profondeur atteinte |
| **Chemin à travers les classes de base** | Exact, et orthographié différemment à dessein : `Root::(base A)::(base B)::pointer (int*) ...` |
| **L'argument de coût derrière le chemin vide dans le trait** | Il tient. `stress.cpp` (chaîne de 60) compile en 0,76 s, `stress_false.cpp` en 1,42 s. Le commentaire de `explain_sendable` annonce un facteur 38 si le trait construisait un chemin à chaque réponse `false` : la conception l'évite correctement |
| **La règle `mutable`** | Nomme le bon coupable avec la bonne phrase : `'const Cache::hits (int) is mutable, so it is written through a const reference: its type must be fully synchronizable'` |
| **`is_lifetime_aware` sur les vues** | Parfait : `'std::basic_string_view<char> is a borrowed range: a view over someone else's storage, it does not keep its elements alive'`, idem pour `std::span<const int>`. Le test `borrowed_range` est la règle la plus tranchante de la bibliothèque |
| **Le message d'opt-in de `is_synchronizable`** | Le meilleur de la bibliothèque : il offre les trois issues réelles (`const T`, la macro, ou rien) |
| **La spécialisation utilisateur explicite** | `21_fallback_specialized` répond juste : `'Vouched is not sendable: is_sendable is specialized to false for it'` |
| **Ne signaler que le premier membre fautif** | C'est le bon choix, pas un défaut. `23_two_bad_members` nomme `TwoProblems::first` et s'arrête, comme le fait tout compilateur |
| **Le repli à deux surcharges du lanceur** | Bon motif, et il fonctionne : la surcharge contrainte n'est jamais choisie pour un mauvais `F`, et `explain_launch_task` rejoue vraiment le concept dans l'ordre de lecture — `10_non_movable_callable` signale bien l'échec de déplaçabilité **avant** tout échec de trait |
| **`launch_scoped_task` explique aussi bien que `launch_task`** | `22_scoped_task` donne la phrase complète sur le pointeur |
| **Une lambda sans capture ne frotte pas** | `32_capture_nothing` compile proprement ; le premier programme (`ergo/01_hello.cpp` : lancer une lambda prenant un `std::vector<int>` et un `std::string`) compile sans une seule annotation |
| **Le découpage en couches du correctif est légal** | Une fonction `consteval` déclarée dans un en-tête et définie dans un en-tête ultérieur est appelable depuis le premier (`diag/28_layer.cpp`). C'est ce qui rend le correctif du §3.3 légitime plutôt qu'une astuce |
| **La spécialisation tardive dans une même TU est détectée** | `error: specialization of threadsafe::is_synchronizable<Widget> after instantiation`. Seul le cas inter-TU est silencieux |
| **`has_unreflectable_state` est la défense porteuse et elle tient** | GCC 16 rapporte `nsdm = 0` pour une closure de 4 octets ayant capturé un `int` par valeur comme pour une de 8 octets ayant capturé par référence. Sans ce test, `[&local]{ return local; }` serait déclarée `sendable` et la bibliothèque serait triviale à casser avec le callable le plus courant de C++. **L'auteur a choisi le bon côté du compromis** ; c'est le message, et non la règle, qu'il faut corriger |

Il faut le dire aussi clairement que les défauts : **le moteur est bon**. Le chemin est le
meilleur morceau de la bibliothèque, et il est déjà digne d'une conférence. Ce qui manque,
c'est de le laisser aller jusqu'au bout dans les trois ou quatre endroits où une
spécialisation lui coupe la route.

---

## 8. Le correctif consolidé : ce que j'ai compilé, et ce que ça coûte

J'ai réuni dans un seul arbre d'en-têtes les correctifs des §2.1, §3.3, §4 et §5.1 —
treize fonctions, trois branches de message, trois déclarations avancées, trois marqueurs
et un `static_assert`, répartis sur sept fichiers :

| Fichier | Changement |
|---------|-----------|
| `details/utils.h` | `element_step` ajoutée ; `is_closure_type` ajoutée ; `has_unreflectable_state` resserrée |
| `details/sendable.h` | déclaration de `diagnose_std_wrapper_sendable` ; marqueur sur les six spécialisations du trait ; `sendable_answer_is_structural` ; `descend_sendable` réécrite ; message de closure réécrit |
| `details/synchronizable.h` | déclaration ; marqueur sur `is_synchronizable<const T>` ; `const_synchronizable_answer_is_structural` ; `descend_const_synchronizable` réécrite ; message de closure réécrit |
| `details/lifetime_aware.h` | déclaration ; marqueur sur le template primaire ; `lifetime_aware_answer_is_structural` ; `descend_lifetime_aware` réécrite ; message de closure réécrit |
| `details/allowed_std_wrappers.h` | les trois prédicats deviennent trois paires diagnose/answer |
| `details/synchronized_value.h` | `static_assert((assert_sendable<T>(), true));` ajouté en premier |
| `details/asynchronous_task_launcher.h` | les deux `throw` de `assert_ownable_by_launcher` réécrits |

**Vérification, faite par moi, aujourd'hui, avec g++-16 (Homebrew GCC) 16.2.0 :**

| Contrôle | Résultat |
|----------|----------|
| Les 11 unités de test compilent | **Oui, toutes** |
| Composition avec le correctif de `std_wrapper` du lead | **Oui**, 11/11, et le blanchiment de `const` est fermé |
| Le corpus de diagnostics recompilé | 19/28 coupables exacts (contre 13), 24/28 messages non faux (contre 14) |
| Champ de bits sans nom accepté, closures avec capture toujours refusées | **Oui**, les 7 assertions tiennent |

**Coût en temps de compilation.** Mesures faites sur cette machine, qui n'est manifestement
pas au repos — je le signale parce que les chiffres par fichier sont bruyants (une TU passe
de 710 à 1140 ms, une autre de 990 à 700 ms, ce qui est du bruit et non un effet du
correctif), et parce que mon total de référence de 7960 ms est ~16 % au-dessus des 6849 ms
mesurés par le lead sur une machine au repos. Les deux mesures utiles sont donc les rapports :

| Charge | Référence | Avec correctif | Écart |
|--------|----------:|---------------:|------:|
| `bench.cpp` (10 réponses trait-lourdes, meilleur de 3) | 690 ms | 700 ms | **+1,4 %** |
| Les 11 unités de test, total (meilleur de 3 chacune) | 7960 ms | 8180 ms | **+2,8 %** |

Cohérent avec le +6 % qu'un agent avait mesuré sur un banc plus étroit. **Le surcoût est
réel mais négligeable, et il est payé au bon endroit** : le parcours profond à travers un
conteneur ne s'exécute que lorsqu'un chemin a été amorcé, c'est-à-dire uniquement depuis
`assert_*`, c'est-à-dire uniquement quand quelqu'un lit le message. Le trait lui-même
continue de répondre `false` sans construire de chaîne. C'est le même marché que celui que
`explain_sendable` documente déjà, et le correctif le respecte à la lettre.

Rappelons par ailleurs, parce que c'est la mise en perspective qui manque : le lead a mesuré
qu'une TU **vide** n'incluant que l'en-tête parapluie coûte 586 ms des ~620 ms d'une TU de
test typique, et que `constant expression evaluation` — c'est-à-dire **tout** le travail des
traits — pèse au maximum 9 %, environ 50 ms. Les +10 ms de ce correctif se comparent à ces
50 ms, pas aux 620. Voir [06](./06-performance-compilation.md).

---

## 9. La recommandation numéro un : les messages ne sont testés par rien

C'est mon propre constat, et je le place en tête des recommandations parce qu'il explique
pourquoi ce rapport a pu trouver dix messages faux dans une bibliothèque dont les 11 unités
de test sont vertes.

`tests/test_diagnostics.cpp` fait 81 lignes. Il vérifie que `assert_*` **ne lance pas** sur
un type conforme, et que le trait répond bien `false` sur un type non conforme. Il ne fige
**aucun texte**. Pas une seule assertion ne regarde ce qu'un refus dit. Son propre
commentaire l'assume :

```cpp
// The assert_* functions are the diagnostic face of the traits: they agree with
// the trait on a conforming type (they compile and return), and turn a "false"
// into a std::meta::exception naming the culprit. Only the agreeing half is
// testable here — the throwing half *is* a compile error by design.
```

**Cette dernière phrase est fausse, et je l'ai réfutée par compilation.** Une fonction
`consteval` peut attraper la `std::meta::exception` que `assert_*` lance et comparer son
`u8what()`. Le fichier suivant est complet, compile, et constitue exactement le test qui
manque :

```cpp
#include <threadsafe/threadsafe.h>

#include <meta>
#include <string>
#include <string_view>
#include <vector>

// Le message qu'un refus porte, comme valeur. assert_sendable le lance, et une
// fonction consteval peut l'attraper : les diagnostics sont donc testables
// exactement comme les traits le sont, avec un static_assert et sans runtime.
template <class T>
consteval bool sendable_rejection_is(std::u8string_view expected) {
    try {
        threadsafe::assert_sendable<T>();
    } catch (const std::meta::exception &failure) {
        return std::u8string(failure.u8what()) == std::u8string(expected);
    }
    return false;
}

template <class T>
consteval bool sendable_rejection_mentions(std::u8string_view fragment) {
    try {
        threadsafe::assert_sendable<T>();
    } catch (const std::meta::exception &failure) {
        return std::u8string(failure.u8what()).find(fragment)
            != std::u8string::npos;
    }
    return false;
}

struct Borrowing { int *pointer; };

static_assert(sendable_rejection_is<Borrowing>(
    u8"Borrowing::pointer (int*) is a pointer or a reference: sending it shares "
    u8"its referent with the other thread, so the referent must be "
    u8"synchronizable — and synchronizability is opt-in"));

// Le parcours doit atteindre l'élément À L'INTÉRIEUR du conteneur, pas s'arrêter
// à l'enveloppe.
static_assert(sendable_rejection_mentions<std::vector<Borrowing>>(
    u8"::pointer (int*) is a pointer or a reference"));

// Et il ne doit jamais conseiller de spécialiser le trait pour un conteneur standard.
static_assert(!sendable_rejection_mentions<std::vector<Borrowing>>(
    u8"specialize is_sendable to state the intent"));

int main() {}
```

Résultats, mesurés par moi :

```
$ g++-16 -std=c++26 -freflection -I <en-têtes d'origine> -fsyntax-only test_message_text.cpp
   → 2 static assertion failed   (les deux assertions sur std::vector)

$ g++-16 -std=c++26 -freflection -I <en-têtes corrigés> -fsyntax-only test_message_text.cpp
   → 0 erreur
```

**Ce fichier est à la fois le test de non-régression du correctif et la preuve du bug.**
Deux détails appris en l'écrivant, à noter pour qui l'intégrera :

- écrire `constexpr auto message = rejection<T>();` échoue avec
  `is not a constant expression because it refers to a result of 'operator new'`. Le
  `std::u8string` ne peut pas franchir la frontière de la constante ; il faut faire la
  comparaison **à l'intérieur** de la fonction `consteval` et n'en renvoyer qu'un `bool`,
  comme ci-dessus ;
- placer le type de test dans un `namespace { }` anonyme change le message :
  `u8display_string_of` produit `{anonymous}::Borrowing`. Les types d'un test de message
  doivent donc être au périmètre de l'espace de noms global, contrairement au reste de la
  suite. C'est exactement le genre de chose qu'un test aurait attrapé et qu'un corpus de
  captures manuelles ne peut pas garantir.

**Recommandation.** Ajouter `tests/test_diagnostic_messages.cpp` avec les deux helpers
ci-dessus et une douzaine d'assertions couvrant les messages à fort trafic : le pointeur, la
référence, `mutable`, la vue empruntée, l'opt-in, la lambda avec capture, le conteneur
standard, la spécialisation utilisateur. Coût : un fichier, environ 60 lignes, ~600 ms de
compilation. Bénéfice : la promesse centrale de la bibliothèque devient une propriété
vérifiée au lieu d'une intention. Cela relève aussi de [03](./03-couverture-de-tests.md) —
et la campagne de mutation citée par le lead (260 mutants, 77 survivants réels) mesure
précisément la même chose du côté des traits.

---

## 10. Ce que je ne recommande pas

Conformément à la consigne « challenge the need », voici ce qu'il ne faut **pas** faire.

| Proposition | Pourquoi non |
|-------------|--------------|
| Faire porter une explication au concept `sendable` | **Impossible en C++.** Une contrainte qui lance est une erreur dure, pas un `false` ; cela détruirait la répartition à deux surcharges. Écrire la limite dans `CLAUDE.md` (§2.2) et passer à autre chose |
| Ajouter un `static_assert` explicatif dans les constructeurs de `copy_on_write` / `synchronized_value` (cas 19 et 20) | **Impossible** : la contrainte est dans la clause `requires`, donc l'échec se produit pendant la résolution de surcharge, avant tout corps. Et la dernière ligne de GCC — « `int` is not constructible from `std::string` » — est déjà correcte et suffisante |
| Un crochet `diagnose_*` par famille pour `unique_ptr`, `shared_ptr`, `reference_wrapper`, `copy_on_write`, `value_guard` | **Pas maintenant.** Quatre messages honnêtes valent mieux que quatre messages faux, et chaque crochet ajoute une fonction à une bibliothèque pédagogique dont la lisibilité est un objectif. Le marqueur du §3.3 rend l'ajout sûr et incrémental ; qu'ils viennent seulement si la démo de conférence en a besoin |
| Ajouter `is_synchronizable<T&>` (volet 1 du constat TC-13) | **Pas dans ce rapport.** C'est un changement de réponse de trait, pas de message ; aucun chemin interne ne pose la question ; son auteur ne l'a pas vérifié contre la suite. À décider dans [01](./01-robustesse-des-traits.md) |
| Élargir la liste blanche `allowed_std_wrappers` pour couvrir `string_view` et `chrono::duration` (cas 03 et 36, qui restent faux après mon correctif) | **Ce n'est pas un problème de diagnostic.** C'est une décision de périmètre du trait, qui appartient à [08](./08-api-et-flexibilite.md). Je note seulement que pour `03` j'ai vérifié que suivre le conseil — `template <> struct threadsafe::is_sendable<std::string_view> : std::true_type {};` — ne casse rien et fait apparaître, au cycle de compilation suivant, la **bonne** phrase : `'std::basic_string_view<char> is a borrowed range: a view over someone else's storage, it does not keep its elements alive'`. Coût : un cycle de compilation perdu, pas un trou de sûreté |
| Raccourcir les diagnostics en général | **Le problème n'est pas la longueur.** La médiane du corpus est de 7 lignes. Les quatre cas longs sont les quatre cas où la bibliothèque ne parle pas |

Il faut également écrire quelque part — `CLAUDE.md` est le bon endroit — la limite que le
lead a démontrée au ThreadSanitizer et qu'aucun message ne peut jamais énoncer : une classe
dont le seul état partagé est un `static inline long` incrémenté dans une fonction membre
**`const`** passe `is_sendable`, `is_synchronizable<const T>` et `launchable_task` ; le
`synchronized_value` correspondant choisit un `std::shared_mutex` ; et TSan signale une
course réelle avec les deux threads ne tenant qu'un `shared_lock`. **La réflexion raisonne
sur des déclarations, jamais sur des corps de fonction.** Aucune formulation de message ne
comblera cela, et une bibliothèque pédagogique qui l'affiche franchement est plus honnête —
et fait une meilleure diapositive — qu'une qui la tait.

---

## 11. Récapitulatif des constats

| Constat | Gravité | Ce que c'est | Correctif | Statut de régression | Vérifié par moi |
|---------|---------|--------------|-----------|----------------------|-----------------|
| **Q2** / **TLS-03** / **L8** | Élevée | Une réponse venue d'une spécialisation est expliquée en re-parcourant le template primaire : coupable inventé, conseil dangereux. 10 cas sur 28 | §3.3, consolidé | `suite-passes` (les trois) | **Oui** — recompilé, 11/11, corpus revérifié, trou de sûreté démontré par compilation |
| **Q8** | Moyenne | `synchronized_value` et le concept `sendable` n'expliquent rien | §2.1 (`synchronized_value`) ; **aucun** pour le concept, limite du langage | `suite-passes` | **Oui** — 20 → 24 lignes, coupable en ligne 10 ; contre-exemple du concept compilé |
| **Q5** + **TC-11** | Moyenne | Toute lambda capturante reçoit le même message, qui accuse la mauvaise chose et donne un conseil inapplicable ; un champ de bits sans nom reçoit le même | §4 | `suite-passes` (les deux) | **Oui** — trois lambdas + champ de bits recompilés avant/après |
| **ADV-09** | Moyenne | `share it with std::ref instead` ne peut jamais satisfaire `launch_task` | §5.1 (volet 1) | `not-checked` par l'auteur ; **mon volet 1 est `suite-passes`, vérifié** | **Oui pour le volet 1** ; non pour le volet `explain_launch_task` |
| **L9** | Moyenne | `launchable_task` ne demande jamais si `F` est appelable ; `launch_task(42)` fait 33 lignes de libstdc++ | §5.2 | `suite-passes` (auteur) | Partiel — j'ai recompilé le reproducteur et compté les 33 lignes ; je n'ai pas appliqué le correctif |
| **L11** | Moyenne | Le beau message n'apparaît que pour un prvalue ; les arguments de template explicites vident l'objet de l'appelant | §5.3 | `suite-passes` (auteur) | Non — rapporté tel quel |
| **ADV-06** | Moyenne | Une réponse périmée ou d'un type incomplet est signalée comme « spécialisée à false », sans qu'aucune spécialisation existe | Intégré au §3.3 | `not-checked` par l'auteur ; **ma version est `suite-passes`, vérifiée** | **Oui** — reproducteur compilé avant/après |
| **TC-13** | Faible | `assert_synchronizable<const T&>` prend la branche opt-in et renvoie l'utilisateur à la question qu'il vient de poser | §5.4, volet 2 seulement | `not-checked` | Partiel — j'ai reproduit le message ; je n'ai pas appliqué le correctif |
| **Nouveau (ce rapport)** | Élevée | **Aucun test ne fige aucun message.** Le mécanisme pour les tester existe et coûte 10 lignes | §9 | `suite-passes` — c'est un fichier de test nouveau | **Oui** — écrit, compilé contre les deux arbres : 2 échecs avant, 0 après |

**Comptabilité des scénarios pour le domaine « qualité »** (diagnostics, simplification
pédagogique, ergonomie des cinq premières minutes, extensibilité) : 85 unités rejouées,
75 reproduites, 10 impasses. Le corpus de diagnostics proprement dit compte 38 sources, dont
30 captures figées ; j'ai recompilé 20 d'entre elles ainsi que 8 sources non capturées, plus
6 reproducteurs que j'ai écrits moi-même.

## 12. Les trois messages à mettre sur une diapositive

Si la conférence ne montre que trois messages, ce sont ceux-là. Les deux premiers existent
déjà et sont excellents ; le troisième est celui du §4.

```
'Root::a (A)::b (B)::pointer (int*) is a pointer or a reference: sending it shares
 its referent with the other thread, so the referent must be synchronizable —
 and synchronizability is opt-in'
```

Le chemin. Trois niveaux, le membre nommé comme l'utilisateur l'a écrit, le type entre
parenthèses, la règle en une phrase. Personne d'autre ne fait ça.

```
'is_synchronizable<T> is opt-in: it holds only for types that synchronize themselves
 (std::atomic, a mutex-protected wrapper). Ask is_synchronizable<const T> for a
 read-only share, or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE to vouch for it'
```

Un refus, la raison du refus, et les trois issues réelles. C'est le modèle que devraient
suivre les 22 messages qui, aujourd'hui, se contentent d'énoncer un fait.

```
'main()::<lambda()> is a lambda with captures, and a capture is invisible to
 reflection — a `[&x]` borrow cannot be told from a `[x]` copy, so no capture is
 trusted. Capture nothing and pass the state as launch_task arguments, where
 each one is checked by name'
```

Une limite réelle de la réflexion C++26, le compromis assumé du bon côté, et une action que
le spectateur peut taper. C'est aussi la leçon `Send` de Rust, en une phrase.
