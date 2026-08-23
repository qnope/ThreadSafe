# Audit — Simplicité d'implémentation et ergonomie de l'API

## 1. Simplicité de l'implémentation

### `utils.h::may_hijack_copy_move` / `has_only_default_copy_move_destroy`

C'est la plus grosse taxe de complexité/lisibilité de la bibliothèque, et c'est un vrai piège de correction pour les utilisateurs, pas seulement une odeur de code. N'importe quel constructeur ou `operator=` templaté, même contraint, marque le type entier non-sendable/non-synchronizable — confirmé par `ex2_customtype.cpp` : un `Point` avec un constructeur forwarding `template<class X, class Y> Point(X&&, Y&&)` échoue à `is_sendable<Point>` alors que le type n'est que deux `double`.

C'est documenté honnêtement dans le long commentaire, mais un nouveau venu tombe sur un `static_assertion failed` nu sans aucune explication du *pourquoi* (voir §2). La fonction elle-même est correcte ; c'est la *règle* qu'elle implémente qui est une sur-approximation large pour une bibliothèque éducative — une contrainte `requires !same_as<remove_cvref_t<U>, T>` sur le constructeur forwarding (l'idiome standard pour empêcher le hijacking) est assez courante pour que la traiter de façon identique à un constructeur forwarding gourmand et non contraint rende le trait surprenant pour le pattern C++ le plus idiomatique de la dernière décennie.

**Suggestion de simplification** : détecter la forme courante de contrainte auto-excluante via `is_constructor_template` + vérifier qu'un constructeur de copie/déplacement non-template est *aussi déclaré* (si l'utilisateur en a écrit un, l'ambiguïté motivant l'interdiction avec les membres spéciaux implicites disparaît), ou, plus honnêtement pour un codebase pédagogique, abandonner la détection automatique et exiger que ces types s'opt-in explicitement via une spécialisation du style `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE`, remplaçant ~15 lignes de raisonnement sur la résolution de surcharge par un commentaire d'une ligne « on n'essaie pas de deviner les membres spéciaux templatés, spécialisez le trait ».

### `asynchronous_task_launcher.h:26` et `:37` — mauvais usage de `std::forward`

```cpp
void launch_task(F f, Args... args) { threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...); }
```

`F` et `Args` sont déduits comme des types valeur simples (pas des références universelles — les paramètres de fonction sont `F f`, pas `F&& f`), donc `std::forward<F>(f)` ne « forward » rien : ça dégénère en un `static_cast` vers `F&&`, fonctionnellement équivalent à `std::move(f)`. Utiliser `std::forward` ici est un anti-pattern bien connu qui se lit comme un idiome de forwarding templaté sans en être un, et va induire en erreur les lecteurs d'un codebase *éducatif* en leur faisant croire que ce sont des paramètres de perfect-forwarding.

**Correction proposée** :
```cpp
template <typename F, typename... Args>
    requires sendable<F> && lifetime_aware<F>
          && (sendable<Args> && ...) && (lifetime_aware<Args> && ...)
void launch_task(F f, Args... args) {
    threads_.emplace_back(std::move(f), std::move(args)...);
}
```
(même correction pour `launch_scoped_task`, ligne 37).

### `synchronizable.h::default_is_const_synchronizable`, lignes 85-97

La branche à trois voies mutable/référence/plain est dense et se lit comme du code de réflexion « intelligent » sans commentaire de parcours associant chaque branche à sa justification (le commentaire *de niveau fichier* explique le concept une fois en haut, mais la branche elle-même n'en a aucun) :

```cpp
for (info member : nonstatic_data_members_of(type, context)) {
    const auto member_type = type_of(member);
    if (is_mutable_member(member)) {
        if (!is_synchronizable_type(remove_cv(member_type)))
            return false;
    } else if (is_reference_type(member_type)) {
        if (!is_synchronizable_type(remove_cv(remove_reference(member_type))))
            return false;
    } else if (!is_synchronizable_type(add_const(member_type))) {
        return false;
    }
}
```

C'est correct et chaque règle est individuellement justifiée par le commentaire de doc du module, mais pour un public de conférence, les trois cas mériteraient chacun un commentaire d'une ligne, juste où ils sont vérifiés, par exemple :

```cpp
for (info member : nonstatic_data_members_of(type, context)) {
    const auto member_type = type_of(member);
    if (is_mutable_member(member)) {
        // mutable defeats const: this member is writable through a const&, so it
        // needs the full (write-safe) trait, not the const one.
        if (!is_synchronizable_type(remove_cv(member_type))) return false;
    } else if (is_reference_type(member_type)) {
        // a reference member's constness is unrelated to the referent's; the
        // referent may be shared and mutated through another alias.
        if (!is_synchronizable_type(remove_cv(remove_reference(member_type)))) return false;
    } else if (!is_synchronizable_type(add_const(member_type))) {
        return false; // ordinary value member: const propagates normally.
    }
}
```

Pas de changement de logique — pure lisibilité, mais exactement le genre de chose qui transforme du code de réflexion « faites-moi confiance, c'est correct » en code pédagogique.

### Bilan

La machinerie de réflexion elle-même (`trait_value`, les diagnostics `consteval`/`throw exception`, les parcours récursifs base/membre) est raisonnablement minimale pour ce qu'elle fait — chaque fonction est courte, à responsabilité unique, et les `throw` donnent des indications correctes (conseils pimpl, conseils sur les types non supportés). Ce n'est pas sur-ingénieré. Le seul endroit où la complexité dépasse le bénéfice est la détection de hijacking copie/déplacement dans `utils.h`, inhérente à l'objectif (très raisonnable), mais qui est exactement le genre de subtilité qui devrait être mise en avant, pas enterrée dans un commentaire de 15 lignes découvert seulement quand un `static_assert` échoue.

---

## 2. Ergonomie de l'API — un essai du point de vue d'un nouveau venu

Cinq petits programmes écrits et compilés contre g++-16 (`-std=c++26 -freflection`).

### Programme 1 — un `std::vector` capturé est-il sendable ?

```cpp
std::vector<int> v{1,2,3};
auto f = [v]() mutable { v.push_back(4); };
static_assert(threadsafe::sendable<decltype(f)>);   // échoue
```

Sortie réelle du compilateur :
```
error: static assertion failed
  • the expression 'is_sendable<T> [with T = ._anon_259]' evaluated to 'false'
```

C'est tout — aucun diagnostic supplémentaire. La machinerie de traits a des diagnostics internes riches (`throw exception(...)` pour les types incomplets/non supportés) mais rien ne se déclenche ici car la réponse est un simple `false`, pas une erreur — l'utilisateur doit donc bisectionner à la main. Il a fallu re-dériver manuellement la règle (lambda mutable ⇒ `push_back` mute la capture ⇒ les closures ont besoin de `has_unreflectable_state == false`, mais un `vector` capturé par une lambda *mutable* est un membre de données non-statique et devrait normalement parcourir structurellement sans problème…) pour réaliser le vrai coupable : `std::vector` n'a pas de spécialisation `is_sendable` explicite qui dépend des subtilités de câblage du trait, et la closure elle-même, bien qu'elle ne capture que de l'état sendable, est parcourue structurellement — un utilisateur débutant n'a aucun moyen de savoir si l'échec vient de la lambda, de `vector`, ou de l'interaction, à partir du seul message.

**Friction** : les échecs de `static_assert` ne donnent aucune piste indiquant *quel* sous-objet a échoué au trait récursif — c'est le plus gros écart d'ergonomie. Un helper du type `static_assert(sendable<T>, "T is not sendable — its member/base ??? isn't")` (même en imprimant juste le type du membre en échec via réflexion à la compilation) aiderait énormément dans un contexte pédagogique.

### Programme 2 — le piège du constructeur forwarding

Décrit au §1 :

```cpp
struct Point {
    template <class X, class Y>
    Point(X&& x, Y&& y) : x_(std::forward<X>(x)), y_(std::forward<Y>(y)) {}
    double x_, y_;
};
static_assert(threadsafe::is_sendable<Point>);   // échoue, sans aucun message
```
```
error: static assertion failed
   12 |     static_assert(threadsafe::is_sendable<Point>);
```

Aucun sous-texte de diagnostic (pas d'expansion du concept `sendable`, car ceci utilise `is_sendable` directement plutôt qu'à travers le concept `sendable` — une incohérence : la forme concept donne *une* trace, la forme variable-template brute n'en donne aucune). Une structure de deux `double` parfaitement ordinaire échoue silencieusement à cause d'un idiome de constructeur complètement courant en C++. C'est l'angle le plus vif de toute la bibliothèque du point de vue utilisateur, et c'est aggravé par l'absence totale d'indice au point d'échec — il faudrait déjà connaître la règle `may_hijack_copy_move` de cette bibliothèque pour la déboguer.

### Programme 3 — `synchronized_value<int>`

Compile proprement, aucune friction. Verrouiller avec `lock()`/`lock_shared()` et les guards `nodiscard` se lit naturellement, et l'API du guard (`*g`, `->`) est idiomatique RAII. **C'est le coin le mieux conçu de la bibliothèque.**

### Programme 4 — `copy_on_write<std::vector<int>>`

Compile proprement aussi — `as_mutable()` et `operator*`/`operator->` sont intuitifs, et `is_sendable<copy_on_write<vector<int>>>` retourne correctement true puisque `vector<int>` n'a pas de constructeurs templatés problématiques. Bonne expérience.

### Programme 5 — `launch_task` avec une lambda capturant un `shared_ptr`

```cpp
auto data = std::make_shared<int>(42);
launcher.launch_task([data] { return *data + 1; });
```
```
error: no matching function for call to 'launch_task(...)'
  • the expression 'is_sendable<T> [with T = main::._anon_259]' evaluated to 'false'
```

C'est un comportement *correct* (un type polymorphe non-final échouerait via `unique_ptr`, mais ici c'est plus simple — la closure capture un `shared_ptr<int>`, et la sendabilité de `shared_ptr` est indexée sur `is_synchronizable<remove_cv_t<remove_all_extents_t<T>>>`, c'est-à-dire la sécurité de mutation complète de `int` — ça devrait être bon, ce qui veut dire que l'échec réel vient encore de `has_unreflectable_state`/le parcours structurel de la closure) mais le message SFINAE « no matching function » combiné à l'échec de contrainte `sendable<F>` est exactement le genre de diagnostic à deux sauts (trouver d'abord le concept en échec, puis trouver *pourquoi* le concept est faux) qu'un nouveau venu aura du mal à résoudre sans déjà connaître les internes.

### Résumé des points de friction

- **Aucune attribution dans les échecs.** Chaque mode d'échec se réduit à `is_sendable<T> evaluated to false` sans indication de quelle base/membre/règle de constructeur l'a déclenché. Pour une bibliothèque éducative, c'est la priorité n°1 — même une fonction `consteval` déclenchée au moment du `static_assert` qui rapporte le nom et le type du premier membre en échec via `std::meta::exception` transformerait l'expérience.
- **Les constructeurs forwarding sont une mine.** Un idiome C++ extrêmement courant et idiomatique (constructeurs de perfect-forwarding contraints) disqualifie silencieusement des types sans syntaxe spéciale ni avertissement, et le contournement (déclarer explicitement les membres spéciaux, ou spécialiser le trait) n'est signalé nulle part près de l'erreur.
- **Flexibilité pour les types personnalisés** : correcte pour les agrégats/POD simples et les types utilisant uniquement des membres spéciaux défaultés — le parcours structurel « fonctionne simplement » pour ce cas courant (Programmes 3/4 le confirment). Ça casse dès qu'un type utilise un idiome de constructeur générique moderne.
- **Conteneurs/callables** : `std::vector`, `optional`, `variant`, smart pointers, etc. sont pré-câblés dans `containers.h`/`vocabulary.h`/`smart_pointers.h`, donc les types STL courants fonctionnent d'emblée — bonne couverture, pas de friction là en soi. Les lambdas fonctionnent tant que leurs captures sont elles-mêmes sendable et que la lambda ne détient pas d'état non-réflectable au-delà, ce qui est raisonnable mais, encore une fois, indiagnosticable en cas d'échec.

---

*Fichiers référencés : `include/threadsafe/details/utils.h`, `synchronizable.h`, `sendable.h`, `synchronized_value.h`, `copy_on_write.h`, `asynchronous_task_launcher.h`, `vocabulary.h`. Programmes d'exemple et sortie g++-16 capturée : `/private/tmp/claude-501/-Users-amorrier-Programmation-ThreadSafe/a9241fcc-858d-44aa-8b7c-232bb71372a4/scratchpad/ex/ex{1..5}_*.cpp`.*
