# 05 — Simplicité

> Verdict : le code est **remarquablement lisible pour ce qu'il fait**, et la seule
> chose qui manque vraiment à un lecteur de conférence n'est pas une abstraction —
> c'est **une page qui dit ce que la réflexion ne peut pas voir**. La duplication du
> squelette de diagnostic, qui saute aux yeux, est le bon choix et doit rester. En
> revanche un défaut réel se cache dans les `#include` : un en-tête n'est **pas
> autonome**, et cela casse les onze TU dès qu'on touche à l'ordre du parapluie.
> Correctif d'une ligne, vérifié.

Ce rapport juge le code comme **support pédagogique**, puisque c'est l'objectif que
`CLAUDE.md` s'assigne : « The code is made to be educational for an international
conference. » Les deux autres règles du fichier — « Always use explicit name for
variables », « Avoid useless comments » — servent de grille.

Toutes les mesures ci-dessous ont été prises sur le commit `64f9c06`, GCC 16.2.0.

---

## 1. La forme générale : 1 321 lignes, et c'est peu

| en-tête | lignes | commentaires | % |
|---|---:|---:|---:|
| `synchronizable_base.h` | 32 | — | — |
| `vocabulary.h` | 42 | — | — |
| `copy_on_write.h` | 60 | 0 | **0 %** |
| `smart_pointers.h` | 66 | — | — |
| `synchronized_value.h` | 113 | 5 | 4 % |
| `asynchronous_task_launcher.h` | 121 | 13 | 10 % |
| `allowed_std_wrappers.h` | 132 | 28 | 21 % |
| `utils.h` | 155 | 40 | **25 %** |
| `sendable.h` | 182 | 24 | 13 % |
| `lifetime_aware.h` | 201 | 24 | 11 % |
| `synchronizable.h` | 217 | 33 | 15 % |
| **total** | **1 321** | | |

Mille trois cents lignes pour un modèle `Send`/`Sync` complet, quatre traits, trois
helpers et un système de diagnostic : c'est **court**, et c'est le premier argument
de la présentation. Un auditoire peut lire `sendable.h` en entier sur trois
diapositives.

La densité de commentaires est en revanche **exactement inversée par rapport au
besoin**. `utils.h` — le fichier de plomberie — est commenté à 25 %, tandis que
`copy_on_write.h`, qui contient la seule ligne de la bibliothèque dont la correction
dépend du modèle mémoire C++, n'a **aucun commentaire**. J'y reviens en [section 4](#4-les-commentaires).

---

## 2. La duplication du squelette : mesurée, puis défendue

C'est ce qu'on voit en premier. `sendable.h`, `synchronizable.h` et
`lifetime_aware.h` portent chacun la **même** structure de quatre fonctions, plus un
bloc de déclarations anticipées :

```cpp
namespace detail {
consteval void diagnose_default_is_X(std::meta::info type, std::u8string path = {});
consteval bool default_is_X(std::meta::info type);
[[noreturn]] consteval void descend_X(std::meta::info inner, const std::u8string &path);
}
```

- `diagnose_default_is_X` — une fonction `consteval void` qui **lève** pour dire non ;
- `default_is_X` — un `try`/`catch` qui transforme la levée en `bool` ;
- `descend_X` — continue la marche à l'intérieur d'un sous-objet ;
- `explain_X` — rejette, ou continue plus profond si un chemin est en cours.

### 2.1 Combien exactement

Mesure : commentaires et littéraux de message retirés, nom du trait unifié, espaces
normalisés, puis appariement de lignes (`difflib.SequenceMatcher`).

| paire | lignes identiques | part du plus petit |
|---|---:|---:|
| `sendable.h` ↔ `lifetime_aware.h` | **91** | **73 %** |
| `sendable.h` ↔ `synchronizable.h` | 75 | 60 % |
| `lifetime_aware.h` ↔ `synchronizable.h` | 70 | 50 % |

Trois fichiers dont deux sont à 73 % identiques : la tentation de factoriser est
légitime, et c'est la première chose qu'un relecteur propose.

### 2.2 Pourquoi il ne faut pas le faire

**Parce que ce qui diffère est exactement ce qu'il faut enseigner.**

La factorisation naturelle est un moteur générique paramétré par une politique — un
`walk_structurally<Policy>(info)` où `Policy` fournit « que faire d'un pointeur »,
« que faire d'un membre », « que faire d'une base ». Écrivons ce que devient alors la
différence entre les trois traits. Aujourd'hui elle se lit en clair, ligne à ligne,
dans la boucle des membres. Voici les trois versions, côte à côte, telles qu'elles
sont dans le code :

```cpp
// sendable.h — un membre est sendable, point.
for (info member : nonstatic_data_members_of(type, context)) {
    const auto member_type = remove_cv(type_of(member));
    if (!is_sendable_type(member_type))
        explain_sendable(member, u8"is not sendable", member_type, path);
}
```

```cpp
// lifetime_aware.h — même forme, autre question.
for (info member : nonstatic_data_members_of(type, context)) {
    const auto member_type = remove_cv(type_of(member));
    if (!is_lifetime_aware_type(member_type))
        explain_lifetime_aware(member, u8"is not lifetime aware",
                               member_type, path);
}
```

```cpp
// synchronizable.h — trois bras, et c'est TOUT le sujet du trait.
for (info member : nonstatic_data_members_of(type, context)) {
    const auto member_type = type_of(member);
    if (is_mutable_member(member)) {
        // mutable defeats const: this member is writable through a const&, so it
        // needs the full (write-safe) trait, not the const one.
        if (!is_synchronizable_type(remove_cv(member_type)))
            reject_at(member, u8"is mutable, ...", path);
    } else if (is_reference_type(member_type)) {
        // a reference member's constness is unrelated to the referent's; the
        // referent may be shared and mutated through another alias.
        if (!is_synchronizable_type(remove_cvref(member_type)))
            reject_at(member, u8"is a reference: the const stops there ...", path);
    } else if (!is_synchronizable_type(add_const(member_type))) {
        // ordinary value member: const propagates normally.
        explain_const_synchronizable(member, u8"is not readable ...",
                                     remove_cv(member_type), path);
    }
}
```

Ces trois blocs **sont la leçon**. « Un membre `mutable` s'écrit à travers un
`const&`, donc il lui faut le trait complet » et « la constance d'un membre référence
n'a rien à voir avec celle du référent » sont les deux idées les plus difficiles de
toute la bibliothèque, et elles sont ici visibles à deux lignes d'écart de leur
équivalent trivial dans `sendable.h`. Dans un moteur générique, elles deviendraient
trois surcharges d'une politique, dans un quatrième fichier, à lire en tenant le
moteur en tête.

**Le rapport de mutation confirme cet arbitrage par un autre chemin.** La boucle des
bases du trait `const` peut être **supprimée en entier** sans qu'aucun des onze TU
ne s'en aperçoive ([03](./03-couverture-de-tests.md)) — un mutant que j'ai vérifié
personnellement. Si les trois marches partageaient un moteur, ce mutant unique
casserait les trois traits d'un coup : la duplication **localise** les régressions
autant qu'elle les multiplie. C'est un argument dans les deux sens, et il faut le
dire ainsi.

**Ce qui mérite d'être factorisé l'est déjà** : `utils.h` porte `reject`,
`reject_at`, `path_step`, `describe`, `trait_value`, `has_unreflectable_state`,
`may_hijack_copy_move`. Les 73 % « identiques » sont en réalité surtout la boucle
`for` et les gardes structurelles — des lignes courtes et évidentes — pas les
décisions.

**Recommandation : ne rien changer.** C'est la réponse à la consigne « challenge the
need » : la duplication est ici un choix pédagogique correct, et je n'ai pas trouvé
de factorisation qui rende `synchronizable.h` plus facile à expliquer sur scène. Une
phrase en tête des trois fichiers suffirait à couper court à la question du
relecteur :

```cpp
// Les trois traits partagent cette forme à quatre fonctions. Elle est répétée, et
// non factorisée, parce que ce qui les distingue tient dans la boucle des membres
// tout en bas — et que c'est là, et seulement là, que le lecteur doit regarder.
```

---

## 3. L'idiome « lever pour dire non »

```cpp
inline consteval bool default_is_sendable(std::meta::info type) {
    try {
        diagnose_default_is_sendable(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}
```

Une fonction `consteval void` qui **lève** pour signifier « faux », rattrapée par une
enveloppe qui en fait un `bool`. C'est surprenant à la première lecture, et c'est
malgré tout le bon design — pour une raison qui se démontre :

**Une seule marche sert deux usages.** Le trait veut un `bool` ; `assert_sendable`
veut une phrase qui nomme le coupable. Avec un `bool`, il faudrait deux parcours et
les garder d'accord. Ici il n'y en a qu'un, et c'est le paramètre `path` qui décide
lequel des deux usages parle. Le commentaire de `explain_sendable` chiffre l'enjeu :

```cpp
// Only assert_sendable seeds a path, because it is the only caller that reads
// the message. The trait leaves it empty and pays nothing: walking every
// subobject a second time to word a message nobody reads would make each
// "false" answer quadratic — measured at 38x on a 60-level chain.
```

**L'alternative évidente est plus lourde.** Rendre `std::optional<std::u8string>` ou
`struct { bool ok; std::u8string why; }` oblige chaque site d'appel à propager
l'échec à la main :

```cpp
// L'alternative, écrite en entier pour un seul bras de la marche :
consteval std::optional<std::u8string>
default_is_sendable_reason(std::meta::info type) {
    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = remove_cv(type_of(member));
        if (auto reason = default_is_sendable_reason(member_type))
            return u8"member `" + member_name(member) + u8"`: " + *reason;
    }
    return std::nullopt;
}
```

Chaque `if` devient trois lignes au lieu d'une, et la construction du message se
fait **à chaque niveau** — ce qui rétablit exactement le coût quadratique que le
design actuel évite. L'exception, ici, est le mécanisme qui permet de ne payer la
mise en forme que sur le chemin qui la lit.

**Recommandation : garder, et l'assumer sur scène.** C'est un bon moment de
présentation — « une exception à la compilation, rattrapée à la compilation » — pas
une bizarrerie à cacher. Il manque juste, en tête de `sendable.h`, la phrase qui
désamorce la surprise :

```cpp
// Lever signifie « non ». Une seule marche sert donc deux usages : le trait, qui
// rattrape et répond `false`, et assert_sendable, qui laisse filer et fait de la
// raison le message d'erreur.
```

---

## 4. Les commentaires

`CLAUDE.md` demande d'éviter les commentaires inutiles. **La bibliothèque est
conforme** : je n'ai pas trouvé un seul commentaire qui paraphrase son code. Le
problème est ailleurs — la répartition.

### 4.1 Ceux qui portent, et qui doivent rester tels quels

Trois commentaires expliquent un « pourquoi » qu'aucune relecture du code ne
donnerait, et ce sont les meilleurs du fichier :

- `may_hijack_copy_move` (`utils.h`, 19 lignes) explique qu'un `template <class U>
  T(U&&)` gagne la résolution de surcharge contre `T(const T&)` sur une lvalue non
  `const`, et que la réflexion ne permet pas de distinguer cette forme d'un
  constructeur à deux itérateurs. Sans ce paragraphe, la garde paraît absurdement
  large ; avec, elle est évidemment juste. **À mettre sur une diapositive.**
- l'en-tête de `allowed_std_wrappers.h` (« An allow-list, not a deduction: nothing in
  reflection tells `std::vector<T>` apart from a type that hides sharing behind the
  same arguments ») énonce honnêtement que la liste est un **acte de confiance**.
- la note de `explain_sendable` sur le coût quadratique, citée plus haut, avec sa
  mesure.

### 4.2 Ce qui manque, et qui manque gravement

**`copy_on_write.h` n'a aucun commentaire — 0 sur 60 lignes.** Or il contient ceci :

```cpp
    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }
```

Une barrière `acquire` isolée, dans la branche `else`, sans un mot. C'est la ligne la
plus subtile de toute la bibliothèque : elle s'apparie avec le `__ATOMIC_ACQ_REL` du
`_M_release` de `shared_ptr`, et [02](./02-robustesse-des-helpers.md) démontre
qu'elle est **portante**. Un lecteur qui l'ignore la supprimera ; le rapport de
mutation montre d'ailleurs que **rien dans la suite ne l'en empêcherait**. Le
commentaire manquant, à écrire mot pour mot :

```cpp
    // use_count() == 1 est une lecture RELAXED (libstdc++ : _M_get_use_count,
    // « No memory barrier is used here »). Elle dit qu'aucun autre handle
    // n'existe, mais ne synchronise pas avec les écritures du thread qui a
    // laissé tomber l'avant-dernier — c'est son ~shared_ptr qui a fait le
    // release. Cette barrière est l'acquire correspondant : sans elle, le
    // writer peut lire un T que personne ne partage plus mais dont il ne voit
    // pas encore les dernières écritures. [atomics.fences]/4.
    T& as_mutable()
```

Deuxième manque, du même ordre : **`copy_on_write` n'a aucun canal de publication**,
et rien ne le dit. Un lecteur suppose naturellement qu'il peut partager une poignée
entre threads ; c'est faux, `is_synchronizable_v<copy_on_write<T>>` est `false`, et
la seule voie est `synchronized_value<copy_on_write<T>>`. Une phrase en tête de
classe l'éviterait.

### 4.3 Le commentaire qui devrait être une documentation

`asynchronous_task_launcher.h` porte une **précondition non vérifiable** en
commentaire majuscule :

```cpp
    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it does
    // not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
```

C'est honnête, et c'est au mauvais endroit : c'est le **contrat public** de
`launch_scoped_task`, la seule porte de la bibliothèque qui soit aussi peu sûre
qu'un `std::thread` nu. Cela appartient à une documentation utilisateur, pas à un
commentaire d'implémentation que personne ne lit avant d'appeler la fonction.

---

## 5. Le défaut réel : un en-tête qui n'est pas autonome

C'est la seule chose de ce rapport qui soit un bug plutôt qu'un avis.

`asynchronous_task_launcher.h` ouvre sa classe sur :

```cpp
class asynchronous_task_launcher {
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");
```

La réponse pour `std::stop_token` vient de spécialisations qui vivent dans
`vocabulary.h`. **Or `asynchronous_task_launcher.h` n'inclut pas `vocabulary.h`.**
Il ne fonctionne que parce que `threadsafe.h` a inclus `vocabulary.h` avant lui.

Test d'autonomie, chaque en-tête compilé seul :

```
  OK   allowed_std_wrappers.h
  FAIL asynchronous_task_launcher.h  <-- non autonome
  OK   copy_on_write.h
  OK   lifetime_aware.h
  OK   sendable.h
  OK   smart_pointers.h
  OK   synchronizable_base.h
  OK   synchronizable.h
  OK   synchronized_value.h
  OK   utils.h
  OK   vocabulary.h
```

Dix sur onze sont propres. Le onzième donne ceci — un `static_assert` qui accuse
`std::jthread` alors que le vrai problème est un `#include` absent :

```
asynchronous_task_launcher.h:82:19: error: static assertion failed: std::jthread
injects a stop_token that the Args constraints never see; it must satisfy them on
its own
   82 |     static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
      |                   ^~~~~~~~~~~~~~~~~~~~~~~~~
  • constraints not satisfied
```

Conséquence mesurée : **inverser l'ordre des `#include` de `threadsafe.h` casse les
onze TU de la suite.** Le parapluie a donc un ordre significatif, non documenté, et
qu'aucun test ne protège.

### Le correctif, vérifié

Trois lignes de commentaire et un `#include`, dans
`include/threadsafe/details/asynchronous_task_launcher.h` :

```cpp
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
// The static_assert below asks about std::stop_token, whose answer is a
// specialization living in vocabulary.h. Without this include the header is
// not self-contained and the assertion fires on include order alone.
#include <threadsafe/details/vocabulary.h>
```

Vérifié :

```
>>> l'en-tête est maintenant autonome
>>> ordre du parapluie INVERSÉ, après correctif : 0 / 11 TU en échec
>>> ordre normal, après correctif :               0 / 11 TU en échec
```

L'ordre des `#include` du parapluie devient **sans importance**, ce qui est la
propriété qu'on veut. C'est aussi une petite leçon transposable : dans une
bibliothèque dont les réponses sont des spécialisations, « inclure ce dont on parle »
n'est pas une politesse, c'est une condition de correction — la même idée que le
piège de spécialisation différée de [08](./08-api-et-flexibilite.md).

---

## 6. Les noms

### 6.1 Les variables

`CLAUDE.md` : « Always use explicit name for variables. » La règle est tenue partout,
avec des noms comme `member_type`, `unqualified`, `wrapped`, `context`,
`allowed_std_wrappers` — sauf à trois endroits, dont deux sont défendables :

| endroit | nom | verdict |
|---|---|---|
| `asynchronous_task_launcher.h:89, 107` | `launch_task(F f, Args... args)` | **à corriger** : c'est l'API publique, celle qu'un utilisateur lit en premier |
| `copy_on_write.h` | `ptr_` | acceptable — un seul membre, dont le rôle est évident ; `block_` serait toutefois plus juste, puisque c'est le bloc partagé qui compte |
| partout | `context` pour `access_context::unchecked()` | correct |

Le seul qui compte vraiment :

```cpp
    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    void launch_task(F callable, Args... arguments) {
        threads_.emplace_back(std::move(callable), std::move(arguments)...);
    }
```

### 6.2 Les noms des traits — la vraie question

C'est le point de nommage qui mérite qu'on s'y arrête, parce qu'il touche à la
pédagogie et non au style.

**`is_lifetime_aware` est un nom faible.** « Aware » ne décrit pas une propriété :
un type ne « prend pas conscience » d'une durée de vie. La propriété est
*possède, ou maintient en vie, ce qu'il atteint*. `is_owning` serait plus direct,
et se relie à la notion que tout le monde a déjà. Cela dit, `is_owning` suggère une
possession exclusive, que le trait n'exige pas (`shared_ptr` répond oui). Un nom
juste serait `keeps_alive` ou `is_self_contained`. **Ce n'est pas assez décisif pour
justifier un renommage global** — mais si un renommage a lieu un jour, c'est celui-là.

**`is_synchronizable<T>` contre `is_synchronizable<const T>` est un vrai piège.** Les
deux orthographes se ressemblent et posent deux questions **différentes** :

| écriture | question réelle | par défaut |
|---|---|---|
| `is_synchronizable<T>` | *T se synchronise-t-il lui-même ?* (`std::atomic`, un wrapper à mutex) | `false`, **opt-in** |
| `is_synchronizable<const T>` | *plusieurs threads peuvent-ils LIRE un `const T` ?* | calculé **structurellement** |

L'une est une déclaration d'intention que l'utilisateur signe ; l'autre est une
déduction que le compilateur fait. Elles n'ont ni le même défaut, ni la même
fiabilité, ni la même portée — et elles ne diffèrent que par un `const`. Le message
de `assert_synchronizable` est d'ailleurs obligé de rattraper la confusion à la main :

```cpp
        throw std::meta::exception(
            u8"is_synchronizable<T> is opt-in: it holds only for types that "
            u8"synchronize themselves (std::atomic, a mutex-protected "
            u8"wrapper). Ask is_synchronizable<const T> for a read-only share, "
            u8"or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE to vouch for it",
            ^^T);
```

Quand un message d'erreur doit expliquer qu'il existe une *autre orthographe du même
nom* qui répond à une *autre question*, le nom a raté quelque chose.

**Recommandation, et c'est le seul renommage que je défends :** garder
`is_synchronizable<T>` et donner à la question `const` un nom qui lui soit propre.

```cpp
// Dans synchronizable.h, en plus (et non à la place) de la spécialisation :
// La question const a son propre nom, parce que ce n'est pas la même question :
// celle-ci est structurelle et calculée, l'autre est un engagement que
// l'utilisateur signe.
template <class T>
constexpr bool is_concurrently_readable_v = is_synchronizable_v<const T>;

template <class T>
concept concurrently_readable = is_concurrently_readable_v<T>;
```

Coût : deux lignes, aucune rupture — `is_synchronizable<const T>` continue de
fonctionner, et reste le point de spécialisation. Bénéfice : la diapositive peut
écrire `concurrently_readable<Config>` et l'auditoire sait immédiatement de quoi on
parle. C'est aussi le vocabulaire que Rust n'a pas besoin d'avoir, parce que chez lui
`&T` *interdit* la mutation ; dire pourquoi le C++ a besoin des deux noms est
précisément le passage intéressant.

---

## 7. `utils.h`, le fourre-tout

155 lignes qui mélangent trois métiers :

| rôle | fonctions |
|---|---|
| mise en forme des messages | `type_name`, `member_name`, `describe`, `path_step`, `reject`, `reject_at` |
| prédicats de réflexion | `trait_value`, `has_unreflectable_state`, `is_copy_move_destroy_member` |
| **politique de sûreté** | `compute_dynamic_type_is_known`, `may_hijack_copy_move`, `has_only_default_copy_move_destroy` |

La troisième catégorie n'est pas de l'outillage : ce sont des **décisions de
conception**, celles qui expliquent la plupart des refus que rencontre un utilisateur
([08](./08-api-et-flexibilite.md) mesure leur portée). Les enterrer dans un fichier
nommé `utils` les rend introuvables.

Découpage proposé, à coût nul puisque `utils.h` n'est inclus que par les trois
en-têtes de traits :

```
details/diagnostic_text.h   type_name, member_name, describe, path_step,
                            reject, reject_at
details/reflection.h        trait_value, has_unreflectable_state,
                            is_copy_move_destroy_member
details/safety_policy.h     compute_dynamic_type_is_known, may_hijack_copy_move,
                            has_only_default_copy_move_destroy
```

`safety_policy.h` fait alors une quarantaine de lignes, se lit d'un bloc, et devient
la diapositive « voici les trois jugements que la bibliothèque porte, et pourquoi ».
**C'est le seul découpage que je recommande** ; le reste de l'arborescence est bon.

---

## 8. Ce qui est réussi, et qu'il faut montrer

À dire aussi nettement que les critiques :

- **Le modèle tient en quatre questions**, et chacune s'écrit en une phrase. La
  correspondance avec `Send`/`Sync` donne un point d'entrée immédiat à quiconque a
  vu Rust.
- **La forme `is_xxx` / `is_xxx_v` / `concept xxx`** est celle de `std::is_same`. Rien
  à apprendre.
- **Les messages d'erreur nomment le coupable et le chemin pour y arriver** —
  `Error::ptr (IntPtr)::ptr (int *) is not sendable`. C'est l'argument de vente, et
  [04](./04-diagnostics.md) montre qu'il tient, à quelques exceptions près.
- **Le mécanisme réflexif est contraint, pas décoratif.** La lecture des traits passe
  par `substitute(^^is_sendable_v, {type})` et non par un `[:type:]`, parce qu'un
  paramètre de fonction n'est jamais une expression constante. C'est un très bon
  moment de conférence : la contrainte est réelle et la solution est élégante.
- **Les refus sont conservateurs par construction.** `is_synchronizable<T>` est
  `false` par défaut, et il faut signer pour l'inverser. Sur une bibliothèque de
  sûreté, c'est le bon défaut, et il est visible en une ligne de
  `synchronizable_base.h`.
- **`copy_on_write.h` fait 60 lignes** pour un type dont la sémantique est correcte
  jusqu'au modèle mémoire. C'est un exploit de concision qu'il faut assumer — après
  y avoir mis les commentaires de la [section 4.2](#42-ce-qui-manque-et-qui-manque-gravement).

---

## 9. Ce que je ferais, dans l'ordre

| | action | coût | pourquoi |
|---|---|---|---|
| 1 | **`#include <threadsafe/details/vocabulary.h>`** dans `asynchronous_task_launcher.h` ([§5](#5-le-défaut-réel--un-en-tête-qui-nest-pas-autonome)) | 1 ligne | seul vrai bug du rapport ; rend l'ordre du parapluie sans importance, vérifié 0/11 dans les deux sens |
| 2 | **Commenter la barrière `acquire`** de `as_mutable` ([§4.2](#42-ce-qui-manque-et-qui-manque-gravement)) | 8 lignes | la ligne la plus subtile de la bibliothèque, aujourd'hui nue, et qu'aucun test ne protège |
| 3 | **Écrire la limite de la réflexion** dans `CLAUDE.md` | 1 paragraphe | voir [01](./01-robustesse-des-traits.md) : c'est le meilleur passage de la conférence, et il n'est écrit nulle part |
| 4 | **Ajouter `concurrently_readable`** ([§6.2](#62-les-noms-des-traits--la-vraie-question)) | 2 lignes | donne un nom à la question `const`, qu'un message d'erreur doit aujourd'hui désambiguïser à la main |
| 5 | **Renommer `F f, Args... args`** ([§6.1](#61-les-variables)) | 2 lignes | c'est l'API publique, et `CLAUDE.md` le demande |
| 6 | **Découper `utils.h`** ([§7](#7-utilsh-le-fourre-tout)) | déplacements | sort la politique de sûreté du fourre-tout et en fait une diapositive |

## Ce qu'il ne faut **pas** faire

- **Factoriser le squelette de diagnostic.** 73 % de lignes identiques, et c'est
  quand même le bon choix : ce qui diffère est ce qu'il faut enseigner, et la
  duplication localise les régressions ([§2.2](#22-pourquoi-il-ne-faut-pas-le-faire)).
- **Remplacer les exceptions `consteval` par un `optional<u8string>`.** Cela
  rétablirait le coût quadratique que le design évite, mesuré à ×38 sur une chaîne de
  60 niveaux ([§3](#3-lidiome--lever-pour-dire-non-)).
- **Renommer `is_lifetime_aware`.** Le nom est faible mais aucune alternative n'est
  assez nettement meilleure pour payer le renommage ([§6.2](#62-les-noms-des-traits--la-vraie-question)).
- **Élaguer les commentaires denses de `utils.h`.** Ils expliquent des « pourquoi »
  non déductibles du code ; c'est `copy_on_write.h` qui est sous-commenté, pas
  `utils.h` qui l'est trop.

---

## Voir aussi

- [01 — Robustesse des traits](./01-robustesse-des-traits.md) — la limite que la [§9](#9-ce-que-je-ferais-dans-lordre) demande d'écrire
- [02 — Robustesse des helpers](./02-robustesse-des-helpers.md) — la démonstration que la barrière `acquire` est portante
- [03 — Couverture de tests](./03-couverture-de-tests.md) — pourquoi rien ne protège cette barrière
- [04 — Diagnostics](./04-diagnostics.md) — la qualité des messages, mesurée
- [08 — API et flexibilité](./08-api-et-flexibilite.md) — la portée des gardes de `utils.h`
- [09 — Méthodologie](./09-methodologie.md)
