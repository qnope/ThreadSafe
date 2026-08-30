# Audit — Simplification du code et réduction du nombre de lignes (détails)
Date : 2026-08-30. Chaque proposition ci-dessous a été vérifiée adversarialement (existence du code cité, équivalence de comportement, respect de l'architecture décrite dans CLAUDE.md, compilation des tests `static_assert`).

## 1. Trois boucles identiques sur wrapped_types_of factorisables en une
**Fichier** : `include/threadsafe/details/allowed_std_wrappers.h` (lignes 59-114) — **Sévérité** : haute — **Lignes gagnées** : ~18

std_wrapper_is_sendable, std_wrapper_is_const_synchronizable et std_wrapper_is_lifetime_aware contiennent la même boucle (for + prepend_path(path_step_of_type)), seule la question posée par élément change. Un helper prenant un pointeur de fonction consteval TraitAnswer(*)(std::meta::info) suffit — et permet de supprimer les trois fonctions nommées, appelées une seule fois chacune, en inlinant dans les spécialisations.

**Code actuel :**

```cpp
inline consteval TraitAnswer std_wrapper_is_sendable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return {};
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (const auto answer = is_sendable_type(wrapped); !answer)
            return answer.prepend_path(path_step_of_type(wrapped));
    return {};
}
// + std_wrapper_is_const_synchronizable (même boucle avec add_const)
// + std_wrapper_is_lifetime_aware (même boucle, sans le court-circuit)
```

**Correction proposée :**

```cpp
inline consteval TraitAnswer
all_wrapped_types(std::meta::info type,
                  TraitAnswer (*question)(std::meta::info)) {
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (const auto answer = question(wrapped); !answer)
            return answer.prepend_path(path_step_of_type(wrapped));
    return {};
}

// puis, dans les spécialisations :
template <detail::std_wrapper T>
struct is_unsafe_sendable<T> {
    static consteval TraitAnswer diagnose() {
        if (detail::is_synchronizable_type(^^T)) return {};
        return detail::all_wrapped_types(^^T, detail::is_sendable_type);
    }
};

template <detail::std_wrapper T>
struct is_unsafe_synchronizable<const T> {
    static consteval TraitAnswer diagnose() {
        if (detail::is_synchronizable_type(^^T)) return {};
        return detail::all_wrapped_types(^^T, [](std::meta::info wrapped) consteval {
            return detail::is_synchronizable_type(std::meta::add_const(wrapped));
        });
    }
};

template <detail::std_wrapper T>
struct is_unsafe_lifetime_aware<T> {
    static consteval TraitAnswer diagnose() {
        return detail::all_wrapped_types(^^T, detail::is_lifetime_aware_type);
    }
};

(Note: le lambda sans capture se convertit en pointeur de fonction; vérifier que is_sendable_type/is_lifetime_aware_type ont exactement la signature TraitAnswer(std::meta::info), sinon les envelopper pareillement dans un lambda.)
```

**Vérification :** Vérifié sur le code réel et par compilation complète des tests (static_assert). Le code cité existe tel quel dans include/threadsafe/details/allowed_std_wrappers.h ; les trois fonctions sont bien la même boucle (seul is_lifetime_aware n'a pas le court-circuit, préservé par la proposition). Les signatures sont exactement `consteval TraitAnswer(std::meta::info)` (sendable.h:40, lifetime_aware.h:45, synchronizable_base.h:50), donc les pointeurs de fonction et le lambda consteval sans capture fonctionnent. Aucune règle d'architecture violée : les prepend_path sont conservés, les spécialisations unsafe restent le point de personnalisation, aucun memo _v contourné. Gain réel : -18 lignes nettes (12 insertions, 30 suppressions). UNE erreur dans le code proposé tel quel : is_sendable_type, is_synchronizable_type et is_lifetime_aware_type vivent dans le namespace `threadsafe`, PAS dans `threadsafe::detail` — les qualifier `detail::` ne compile pas. En retirant ce préfixe erroné (garder `detail::all_wrapped_types` et `detail::wrapped_types_of`), tout compile et tous les tests passent. La proposition est donc valide sur le fond, à condition de corriger cette qualification de namespace.

## 2. Helper detail::diagnose_is_synchronizable utilisé une seule fois — à replier dans is_synchronizable::diagnose()
**Fichier** : `include/threadsafe/details/synchronizable_base.h` (lignes 23-44) — **Sévérité** : haute — **Lignes gagnées** : ~8

Le trampoline via std::meta::info n'apporte rien : contrairement à diagnose_is_sendable (récursif, appelé par info), diagnose_is_synchronizable n'a qu'un seul appelant, is_synchronizable<T>::diagnose(). Le détour par is_unsafe_synchronizable_type(^^T) peut se lire directement via le memo is_unsafe_synchronizable_v<T>, ce qui est même plus conforme à l'architecture (lire le memo _v). Supprime la fonction, son namespace detail d'ouverture/fermeture, et l'indirection.

**Code actuel :**

```cpp
namespace detail {

consteval TraitAnswer diagnose_is_const_synchronizable(std::meta::info type);

inline consteval TraitAnswer diagnose_is_synchronizable(std::meta::info type) {
    if (const auto vouched = is_unsafe_synchronizable_type(type);
        vouched.answered)
        return vouched;

    return "carries no synchronization of its own: is_unsafe_synchronizable<T> "
           "is opt-in, so specialize it to vouch for a type that "
           "synchronizes itself";
}

}

template <class T>
struct is_synchronizable {
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_is_synchronizable(^^T);
    }
};
```

**Correction proposée :**

```cpp
namespace detail {
consteval TraitAnswer diagnose_is_const_synchronizable(std::meta::info type);
}

template <class T>
struct is_synchronizable {
    static consteval TraitAnswer diagnose() {
        if (const auto vouched = is_unsafe_synchronizable_v<T>; vouched.answered)
            return vouched;

        return "carries no synchronization of its own: "
               "is_unsafe_synchronizable<T> is opt-in, so specialize it to "
               "vouch for a type that synchronizes itself";
    }
};
```

**Vérification :** Vérifié: le code cité existe tel quel dans include/threadsafe/details/synchronizable_base.h (lignes 23-44). diagnose_is_synchronizable n'a bien qu'un seul appelant (grep sur include/ et tests/: uniquement is_synchronizable<T>::diagnose()), contrairement au cas récursif de sendable. L'équivalence est exacte: is_unsafe_synchronizable_type(^^T) = trait_value(^^is_unsafe_synchronizable_v, ^^T) = extract de substitute(is_unsafe_synchronizable_v, {^^T}), soit précisément is_unsafe_synchronizable_v<T>; le _v gère l'état unanswered via detail::unsafe_answer, donc vouched.answered se comporte identiquement. Le stampage with_trait et les chemins sont inchangés (aucun prepend nécessaire ici). La lecture directe du memo _v est conforme à la règle d'architecture de CLAUDE.md. J'ai appliqué la correction proposée et rebâti le projet (GCC 16, tests = static_assert): compilation complète sans erreur, tous les static_assert passent. La forme _type(^^T) reste disponible et utilisée ailleurs (diagnose_is_const_synchronizable), rien d'autre ne casse. Gain réel: suppression d'un trampoline inutile et de son ouverture/fermeture de namespace detail. Édit de vérification annulé ensuite (fichier restauré).

## 3. get_mutex_type/get_const_guard_type remplaçables par std::conditional_t
**Fichier** : `include/threadsafe/details/synchronized_value.h` (lignes 49-68) — **Sévérité** : moyenne — **Lignes gagnées** : ~14

Deux fonctions consteval + splices testent deux fois la même condition is_synchronizable_v<const T> pour choisir des types. std::conditional_t fait la même chose sans réflexion, en trois using, et supprime la duplication de la condition. Aucun changement de comportement: mêmes types résultants.

**Code actuel :**

```cpp
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
```

**Correction proposée :**

```cpp
static constexpr bool shared_readable = static_cast<bool>(is_synchronizable_v<const T>);

using mutex = std::conditional_t<shared_readable, std::shared_mutex, std::mutex>;
using guard = value_guard<T, std::unique_lock<mutex>>;
using const_guard = value_guard<const T,
    std::conditional_t<shared_readable, std::shared_lock<mutex>, std::unique_lock<mutex>>>;

(Si la réflexion ici est volontairement pédagogique pour la conférence, garder au moins la fusion des deux getters: la condition n'a besoin d'être écrite qu'une fois puisque le choix du lock découle du choix du mutex.)
```

**Vérification :** Le code cité existe tel quel (synchronized_value.h:49-68). std::conditional_t produit exactement les mêmes types; les tests (test_synchronized_value.cpp:110-124) vérifient les types résultants via std::same_as et passeraient à l'identique. TraitAnswer a un explicit operator bool constexpr (utils.h:25), donc static_cast<bool> est valide. Aucune règle d'architecture (memo _v, unsafe, prepend_path) n'est concernée; get_mutex_type/get_const_guard_type ne sont utilisés nulle part ailleurs. Le gain est réel (supprime deux fonctions consteval publiques, deux splices et la condition dupliquée). Seule réserve, déjà anticipée par la proposition: la réflexion peut être volontairement pédagogique pour la conférence — choix stylistique de l'auteur, pas une réfutation.

## 4. Alias can_launch_task / can_launch_scoped_task = renommage pur d'un concept
**Fichier** : `tests/test_copy_on_write.cpp, tests/test_synchronized_value.cpp, tests/test_soundness_regressions.cpp, tests/test_asynchronous_task_launcher.cpp` (lignes cow: 53-54 / sync: 23-24 / soundness: 74-75 / launcher: 16-20) — **Sévérité** : moyenne — **Lignes gagnées** : ~12

Quatre fichiers définissent `constexpr bool can_launch_task = threadsafe::launchable_task<F, Args...>;` — un simple renommage sans valeur ajoutée du concept public. Utiliser le concept directement dans les static_assert supprime 3 lignes par fichier (et 3 de plus pour l'alias can_launch_scoped_task de test_asynchronous_task_launcher). Après un `using threadsafe::launchable_task;` en tête, les assertions restent aussi lisibles.

**Code actuel :**

```cpp
template <class F, class... Args>
constexpr bool can_launch_task = threadsafe::launchable_task<F, Args...>;
...
static_assert(can_launch_task<decltype([] {})>, ...);
```

**Correction proposée :**

```cpp
using threadsafe::launchable_task;
...
static_assert(launchable_task<decltype([] {})>, ...);
```

**Vérification :** Renommage pur confirmé: can_launch_task = threadsafe::launchable_task<F, Args...> existe tel quel dans les 4 fichiers, et can_launch_scoped_task dans test_asynchronous_task_launcher.cpp:20 est un pur alias de launchable_scoped_task. Utiliser les concepts directement (avec using threadsafe::launchable_task) donne exactement les mêmes valeurs booléennes dans les static_assert — aucun changement de comportement, aucune règle d'architecture violée (code de test seulement). Réserve importante à respecter lors de l'application: can_launch_scoped_task dans test_synchronized_value.cpp:28 n'est PAS un alias du concept (c'est une requires-expression sur l.launch_scoped_task) et doit rester intact — la proposition ne le cite pas, elle est donc cohérente.

## 5. Trois diagnose() de unique_ptr structurés à l'identique
**Fichier** : `include/threadsafe/details/smart_pointers.h` (lignes 18-47, 80-95) — **Sévérité** : moyenne — **Lignes gagnées** : ~10

is_unsafe_sendable, is_unsafe_lifetime_aware et is_unsafe_synchronizable<const...> de std::unique_ptr<T,D> ont le même squelette: pointee -> pointee_step, deleter -> "deleter", sinon dynamic_type_is_known<pointee>. Un helper prenant les deux TraitAnswer déjà lus depuis les memos _v respecte la règle du memo (les _v restent lus au site d'appel) et replie chaque corps en une ligne. Les deux _v sont déjà instanciés inconditionnellement aujourd'hui, donc aucun changement d'instanciation.

**Code actuel :**

```cpp
template <class T, class D>
struct is_unsafe_sendable<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;
        if (const auto pointee_answer = is_sendable_v<pointee>; !pointee_answer)
            return pointee_answer.prepend_path(detail::pointee_step);
        if (const auto deleter_answer = is_sendable_v<D>; !deleter_answer)
            return deleter_answer.prepend_path("deleter");
        return detail::dynamic_type_is_known<pointee>;
    }
};
// + même squelette pour lifetime_aware et const-synchronizable
```

**Correction proposée :**

```cpp
namespace detail {
template <class Pointee>
inline consteval TraitAnswer unique_ptr_answer(TraitAnswer pointee_answer,
                                               TraitAnswer deleter_answer) {
    if (!pointee_answer) return pointee_answer.prepend_path(pointee_step);
    if (!deleter_answer) return deleter_answer.prepend_path("deleter");
    return dynamic_type_is_known<Pointee>;
}
}

template <class T, class D>
struct is_unsafe_sendable<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;
        return detail::unique_ptr_answer<pointee>(is_sendable_v<pointee>,
                                                  is_sendable_v<D>);
    }
};

template <class T, class D>
struct is_unsafe_lifetime_aware<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;
        return detail::unique_ptr_answer<pointee>(is_lifetime_aware_v<pointee>,
                                                  is_lifetime_aware_v<D>);
    }
};

template <class T, class D>
struct is_unsafe_synchronizable<const std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;
        return detail::unique_ptr_answer<pointee>(is_synchronizable_v<pointee>,
                                                  is_synchronizable_v<const D>);
    }
};
```

**Vérification :** Vérifié contre le code réel et par compilation. (1) Le code cité existe tel quel dans include/threadsafe/details/smart_pointers.h (lignes 18-47 et 80-95), à des retours à la ligne près. (2) Comportement identique: le helper préserve l'ordre pointee->deleter, les mêmes étapes de chemin (pointee_step puis "deleter"), et le repli sur detail::dynamic_type_is_known<Pointee>. TraitAnswer est un type littéral trivialement copiable, le passage par valeur ne change rien; un état unanswered est falsy avec error_message==nullptr, donc prepend_path est un no-op dans les deux versions. (3) L'affirmation sur l'instanciation est exacte: les deux _v sont nommés dans le corps de diagnose(), donc déjà instanciés inconditionnellement aujourd'hui — la perte du court-circuit syntaxique ne change ni instanciation ni calcul (les _v sont des constexpr évalués à l'instanciation). (4) Règles d'architecture respectées: les _v restent lus au site d'appel (memo), les spécialisations is_unsafe_* gardent leur diagnose() avec type de retour explicite, aucune spécialisation des traits sûrs, prepend_path inchangé. (5) J'ai appliqué le refactor exact proposé et compilé tout le projet avec g++-16: build complet réussi, donc tous les static_assert des 11 fichiers de tests (dont test_smart_pointers.cpp et test_diagnostics.cpp qui vérifient les textes de chemins) passent. Modification testée puis annulée; l'arbre de travail est revenu à son état initial.

## 6. assert_task_participant / assert_scoped_task_participant : deux helpers jumeaux fusionnables
**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h` (lignes 75-84) — **Sévérité** : moyenne — **Lignes gagnées** : ~5

Deux fonctions de 4 lignes qui ne diffèrent que par la variable _v lue. Un seul helper paramétré par une référence constexpr au TraitAnswer suffit ; les sites d'appel des surcharges de repli passent le _v explicitement. Le memo _v reste la seule source de vérité.

**Code actuel :**

```cpp
template <class T>
consteval void assert_task_participant() {
    static_assert(task_participant<T>, explain(is_task_participant_v<T>, ^^T));
}

template <class T>
consteval void assert_scoped_task_participant() {
    static_assert(scoped_task_participant<T>,
                  explain(is_scoped_task_participant_v<T>, ^^T));
}
// ... sites d'appel :
    detail::assert_task_participant<F>();
    (detail::assert_task_participant<Args>(), ...);
// et
    detail::assert_scoped_task_participant<F>();
    (detail::assert_scoped_task_participant<Args>(), ...);
```

**Correction proposée :**

```cpp
template <class T, const TraitAnswer& answer>
consteval void assert_participant() {
    static_assert(bool(answer), explain(answer, ^^T));
}
// sites d'appel :
    detail::assert_participant<F, is_task_participant_v<F>>();
    (detail::assert_participant<Args, is_task_participant_v<Args>>(), ...);
// et
    detail::assert_participant<F, is_scoped_task_participant_v<F>>();
    (detail::assert_participant<Args, is_scoped_task_participant_v<Args>>(), ...);
```

**Vérification :** Vérifié empiriquement : le code cité existe tel quel dans include/threadsafe/details/asynchronous_task_launcher.h (helpers lignes ~76-86, sites d'appel dans les surcharges de repli). J'ai appliqué la correction proposée, reconfiguré et rebuildé avec g++-16 : les 11 cibles de tests static_assert compilent (100% Built). Les tests d'erreurs (tests/build_errors 02, 03, 05) échouent toujours avec exactement les mêmes messages diagnostiques (« Counter* is not lifetime-aware because it borrows its pointee… », « Pinned is not able to take part in a task… », lambda capturante). Le NTTP `const TraitAnswer&` lié à la variable template constexpr `_v` est légal et accepté par GCC 16 ; le memo _v reste l'unique source de vérité (le helper ne fait que la lire), aucune règle de CLAUDE.md n'est violée (pas d'héritage entre traits, pas de recalcul, chemins intacts). Gain réel : deux helpers jumeaux fusionnés en un, comportement et diagnostics identiques. Seul bémol mineur : les sites d'appel répètent le type (T et _v<T>), mais cela reste local aux deux surcharges de repli. Fichier restauré après vérification.

## 7. shared_ptr et weak_ptr : diagnose() dupliqué mot pour mot
**Fichier** : `include/threadsafe/details/lifetime_aware.h` (lignes 99-121) — **Sévérité** : moyenne — **Lignes gagnées** : ~4

Les deux spécialisations is_unsafe_lifetime_aware<std::shared_ptr<T>> et <std::weak_ptr<T>> ont un corps identique (alias pointee + même diagnose). Factorisable en un helper detail:: sans toucher au modèle memo/_v : le helper lit toujours is_lifetime_aware_v (le memo) et rend le même TraitAnswer.

**Code actuel :**

```cpp
template <class T>
struct is_unsafe_lifetime_aware<std::shared_ptr<T>> {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;

    static consteval TraitAnswer diagnose() {
        if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
            return answer.prepend_path(detail::pointee_step);

        return detail::dynamic_type_is_known<pointee>;
    }
};

template <class T>
struct is_unsafe_lifetime_aware<std::weak_ptr<T>> {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;

    static consteval TraitAnswer diagnose() {
        if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
            return answer.prepend_path(detail::pointee_step);

        return detail::dynamic_type_is_known<pointee>;
    }
};
```

**Correction proposée :**

```cpp
namespace detail {
template <class T>
consteval TraitAnswer diagnose_shared_pointee() {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;
    if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
        return answer.prepend_path(pointee_step);
    return dynamic_type_is_known<pointee>;
}
}

template <class T>
struct is_unsafe_lifetime_aware<std::shared_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_shared_pointee<T>();
    }
};

template <class T>
struct is_unsafe_lifetime_aware<std::weak_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_shared_pointee<T>();
    }
};
```

**Vérification :** Le code cité existe mot pour mot (lifetime_aware.h:98-119) et les deux corps sont identiques. Le helper propose exactement le même calcul: lecture du memo is_lifetime_aware_v, prepend_path(pointee_step), fallback dynamic_type_is_known — tous existants dans utils.h. L'alias membre `pointee` supprimé n'est référencé nulle part ailleurs. Les tests (test_smart_pointers.cpp, test_lifetime_aware.cpp) n'interrogent que les _v, résultats inchangés. Aucune règle d'architecture violée: memo respecté, type de retour TraitAnswer explicite, pas d'héritage, helpers detail:: déjà le style maison. Gain modeste (lignes quasi neutres) mais réel: point unique de vérité, et le traitement identique de weak_ptr et shared_ptr est voulu par la bibliothèque (test « the weak_ptr form erases the referent just the same »).

## 8. prepend_path : construction du vecteur en 4 étapes au lieu de 2
**Fichier** : `include/threadsafe/details/utils.h` — **Sévérité** : moyenne — **Lignes gagnées** : ~3

Lignes 37-41 : reserve + push_back + insert avec arithmétique de pointeurs, alors que l'initialisation par liste plus append_range (C++23, disponible en C++26/GCC 16) et l'accesseur paths() déjà présent font la même chose. La reserve est superflue pour un vecteur consteval jetable.

**Code actuel :**

```cpp
std::vector<const char *> extended_path;
extended_path.reserve(path_step_count + 1);
extended_path.push_back(std::define_static_string(step));
extended_path.insert(extended_path.end(), path_steps,
                     path_steps + path_step_count);
```

**Correction proposée :**

```cpp
std::vector<const char *> extended_path{std::define_static_string(step)};
extended_path.append_range(paths());
```

**Vérification :** Code cité exact (utils.h lignes 37-41). La correction compile avec g++-16 en contexte consteval (append_range disponible), conserve l'ordre des étapes (nouvelle étape en tête puis l'ancien chemin via paths()), et la suite de tests compile intégralement avec le changement appliqué (les static_assert passent). Aucune règle d'architecture n'est touchée; la reserve est effectivement superflue pour un vecteur consteval jetable. Fichier reverté après vérification.

## 9. Corps identiques de is_unsafe_sendable<T&> et <T&&> (et quasi <T*>) — déléguer via le memo
**Fichier** : `include/threadsafe/details/sendable.h` (lignes 44-66) — **Sévérité** : moyenne — **Lignes gagnées** : ~3

Les spécialisations T& et T&& ont un corps strictement identique, et T* ne diffère que par le step. Conformément à l'architecture (une spécialisation qui délègue lit le memo _v), T&& peut déléguer à T& en une ligne.

**Code actuel :**

```cpp
template <class T>
struct is_unsafe_sendable<T&&> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>.prepend_path(
            detail::referent_step);
    }
};
```

**Correction proposée :**

```cpp
template <class T>
struct is_unsafe_sendable<T&&> {
    static consteval TraitAnswer diagnose() { return is_unsafe_sendable_v<T&>; }
};
```

**Vérification :** Le code cité existe tel quel (include/threadsafe/details/sendable.h:52-58, spécialisation T&& identique à T& lignes 44-50). La délégation `return is_unsafe_sendable_v<T&>;` est comportementalement équivalente: is_unsafe_sendable_v<T&> passe par detail::unsafe_answer (utils.h:112), qui appelle diagnose() de la spécialisation T& — exactement la même expression (is_synchronizable_v<remove_cv_t<T>>.prepend_path(referent_step)), avec answered=true, même chemin et même trait_name (stamped « synchronizable » en profondeur). Aucun risque de collision de déduction: un lvalue-ref type matche T&, jamais T&&. Conforme à CLAUDE.md (une spécialisation qui délègue lit le memo _v; ici le memo unsafe, pas d'héritage, pas de recalcul). Vérifié par compilation avec g++-16 -std=c++26 -freflection sur une copie patchée: test_sendable.cpp (dont static_assert(is_sendable_v<SyncType&&>) ligne 148), test_diagnostics.cpp, test_soundness_regressions.cpp et test_asynchronous_task_launcher.cpp compilent tous. Gain réel: supprime la duplication du corps. Seule réserve mineure (non bloquante): la version courte est légèrement moins « pédagogique » car le step referent_step n'apparaît plus explicitement dans la spécialisation T&&.

## 10. Concept function_type utilisé une seule fois — remplacer par une clause requires inline
**Fichier** : `include/threadsafe/details/synchronizable.h` (lignes 11-17) — **Sévérité** : moyenne — **Lignes gagnées** : ~2

Le concept function_type n'existe que pour contraindre la spécialisation qui suit et n'est réutilisé nulle part dans le dépôt (grep confirmé). Une clause requires sur la spécialisation partielle fait la même chose sans introduire un nom public dans le namespace threadsafe.

**Code actuel :**

```cpp
template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
struct is_unsafe_synchronizable<F> {
    static consteval TraitAnswer diagnose() { return {}; }
};
```

**Correction proposée :**

```cpp
template <class F>
    requires std::is_function_v<F>
struct is_unsafe_synchronizable<F> {
    static consteval TraitAnswer diagnose() { return {}; }
};
```

**Vérification :** Le code cité existe tel quel (include/threadsafe/details/synchronizable.h:11-17). grep confirme que function_type n'est utilisé qu'à ce seul endroit dans tout le dépôt. La clause requires est sémantiquement équivalente au concept pour contraindre la spécialisation partielle (même constrainte std::is_function_v, même normalisation atomique ici puisqu'un seul site l'utilise). Vérification empirique: les 11 fichiers de tests compilent tous sans erreur avec g++-16 -std=c++26 -freflection contre une copie patchée des headers. Aucune règle de CLAUDE.md n'est violée (aucun memo, chemin ou spécialisation unsafe affecté; le mécanisme unsafe reste identique). Le gain est réel: suppression d'un nom public inutile du namespace threadsafe.

## 11. Requires-expression maison au lieu du concept launchable_scoped_task existant
**Fichier** : `tests/test_synchronized_value.cpp` (lignes 26-30) — **Sévérité** : moyenne — **Lignes gagnées** : ~2

Le fichier réécrit à la main un requires-expression pour tester launch_scoped_task alors que la bibliothèque expose déjà le concept threadsafe::launchable_scoped_task (utilisé tel quel dans test_asynchronous_task_launcher.cpp, ligne 18-20). C'est une duplication de la contrainte de la bibliothèque, qui pourrait diverger silencieusement de la vraie surcharge.

**Code actuel :**

```cpp
template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_scoped_task(f, args...);
    };
```

**Correction proposée :**

```cpp
template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    threadsafe::launchable_scoped_task<F, Args...>;
```

**Vérification :** Le code cité existe tel quel (tests/test_synchronized_value.cpp:27-31) et le concept threadsafe::launchable_scoped_task existe (include/threadsafe/details/asynchronous_task_launcher.h:58) et est déjà utilisé à l'identique dans test_asynchronous_task_launcher.cpp:20-21. Mieux: le requires-expression maison est en fait plus faible que le concept, car asynchronous_task_launcher possède une surcharge de repli non contrainte de launch_scoped_task (h:113+, dont les static_assert ne se déclenchent qu'à l'instanciation du corps), si bien que l'expression d'appel est valide même pour des types refusés — le prédicat maison peut donc être trivialement vrai. Le seul usage dans ce fichier est une assertion positive (ligne 83, lambda (sync_int&) + sync_int&), vraie sous les deux formes (le même motif passe le concept dans test_asynchronous_task_launcher.cpp:48). La correction ne casse aucun static_assert, aligne le test sur la vraie contrainte de la surcharge, supprime une duplication susceptible de diverger, et ne viole aucune règle d'architecture (aucun trait/memo/_v/unsafe concerné).

## 12. Tronc structurel dupliqué entre diagnose_is_sendable et diagnose_is_const_synchronizable
**Fichier** : `include/threadsafe/details/sendable.h` (lignes 100-127 (et synchronizable_base.h 113-136)) — **Sévérité** : basse — **Lignes gagnées** : ~17

La séquence void / non-classe / incomplet / is_default_type / has_unreflectable_state / boucle sur bases est dupliquée entre les deux walks (~25 lignes chacun), à trois différences près : le nom du trait dans les messages, le trait récursif appliqué aux bases/membres, et le add_const côté synchronizable. Factorisable en un helper detail::structural_precheck(type, trait_name) retournant TraitAnswer pour la partie void/non-classe/incomplet/default/unreflectable (les messages se paramétrant par le nom du trait, p.ex. via concaténation define_static_string), les boucles bases/membres restant propres à chaque walk. Gain estimé ~15-20 lignes. Réserve : les messages actuels sont finement rédigés et le code se veut pédagogique (CLAUDE.md) ; la factorisation rend les messages moins littéraux. À arbitrer par l'auteur — je la signale comme la plus grosse duplication mais ne la recommande que si la concision prime sur la lisibilité pédagogique.

**Code actuel :**

```cpp
if (is_void_type(type))
    return "holds no value to send";

if (!is_class_type(type) && !is_union_type(type))
    return "is not a scalar, class or union type — is_sendable<T> supports no others";

if (!is_complete_type(type))
    return "is incomplete — is_sendable<T> needs a complete type; ...";

if (const auto answer = is_default_type(type); !answer)
    return answer;

if (has_unreflectable_state(type))
    return "holds state reflection cannot see ...";

(quasi-identique dans diagnose_is_const_synchronizable avec 'read', 'is_synchronizable<const T>' et 'is_unsafe_synchronizable')
```

**Vérification :** Vérifié dans include/threadsafe/details/sendable.h (l. ~99-118) et synchronizable_base.h (l. ~112-131) : la séquence void / non-classe-union / incomplet / is_default_type / has_unreflectable_state est bien dupliquée quasi mot pour mot, avec exactement les trois différences annoncées (nom du trait dans les messages — "send"/"read", "is_sendable<T>"/"is_synchronizable<const T>", "is_unsafe_sendable"/"is_unsafe_synchronizable" — trait récursif sur bases/membres, add_const côté synchronizable). Les boucles bases/membres divergent réellement (mutable, références, add_const) et sont correctement laissées hors du helper. Aucun test ne compare le texte de ces messages : test_diagnostics.cpp ne vérifie l'égalité textuelle que sur des raisons écrites par le test lui-même ("refused by this test", etc.), et les static_assert ne portent que sur la présence d'une raison et les chemins. Un helper detail::structural_precheck retournant TraitAnswer, avec messages paramétrés par define_static_string, ne change ni l'ordre des vérifications ni les valeurs retournées, et ne viole aucune règle d'architecture (memo _v, couche unsafe, prepend_path intacts). Réserve légitime et déjà énoncée par la proposition : la concaténation consteval des messages rend le code moins littéral, en tension avec l'objectif pédagogique de CLAUDE.md, et le gain net (~15-20 lignes moins la machinerie du helper) est modeste — l'arbitrage revient à l'auteur, mais la proposition telle qu'énoncée est factuellement exacte et techniquement réalisable sans changement de comportement.

## 13. Boilerplate SyncType + spécialisation is_unsafe_synchronizable dupliqué 4 fois (et NonSendable 3 fois)
**Fichier** : `tests/test_sendable.cpp, tests/test_synchronizable.cpp, tests/test_smart_pointers.cpp, tests/test_soundness_regressions.cpp, tests/test_asynchronous_task_launcher.cpp` (lignes sendable: 11, 123-128 / synchronizable: 6, 55-60 / smart_pointers: 10, 26-31 / soundness: 23, 77-82 / launcher: 7-11, 24-29) — **Sévérité** : basse — **Lignes gagnées** : ~16

Quatre fichiers déclarent le même `struct SyncType {};` suivi de la même spécialisation vide de is_unsafe_synchronizable (6 lignes chacune), et trois fichiers dupliquent `struct NonSendable { NonSendable(NonSendable const&) {} };`. Un en-tête de test partagé tests/test_support.h (namespace test_support, avec la spécialisation faite une fois) supprime environ 28 lignes de boilerplate contre ~12 lignes d'en-tête. Trade-off : chaque fichier de test cesse d'être autonome — à peser vu la vocation pédagogique du dépôt (l'architecture CLAUDE.md n'est pas violée : la spécialisation reste écrite avant la première question).

**Code actuel :**

```cpp
namespace { struct SyncType {}; }
...
template <>
struct threadsafe::is_unsafe_synchronizable<SyncType> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};
```

**Correction proposée :**

```cpp
// tests/test_support.h
#pragma once
#include <threadsafe/threadsafe.h>
namespace test_support {
struct SyncType {};
struct NonSendable { NonSendable(NonSendable const&) {} };
}
template <>
struct threadsafe::is_unsafe_synchronizable<test_support::SyncType> {
    static consteval threadsafe::TraitAnswer diagnose() { return {}; }
};

// dans chaque test : #include "test_support.h"
using test_support::SyncType;
```

**Vérification :** Vérifié dans le code: le boilerplate SyncType + spécialisation vide de is_unsafe_synchronizable est identique dans les 4 fichiers cités (test_sendable.cpp:11/130, test_synchronizable.cpp:7/55, test_smart_pointers.cpp:10/27, test_soundness_regressions.cpp:24/78), et NonSendable est dupliqué à l'identique dans 3 fichiers (test_asynchronous_task_launcher.cpp:12, test_copy_on_write.cpp:14, test_synchronized_value.cpp:12 — deux ne sont pas dans la liste de fichiers du titre, mais le compte est exact). La correction ne change aucun comportement: la spécialisation d'un template de classe dans un en-tête est ODR-conforme, le constructeur de copie de NonSendable est inline en classe, et les traits lisent les membres par réflexion — le namespace n'affecte aucun static_assert. La règle CLAUDE.md est respectée (spécialisation écrite avant la première question, traits sûrs toujours fermés, aucun impact memo/prepend_path). Gain réel (~28 lignes contre ~12), trade-off pédagogique honnêtement signalé et laissé au jugement de l'auteur.

## 14. Helpers is_copy_move_destroy_member / may_hijack_copy_move utilisés une seule fois
**Fichier** : `include/threadsafe/details/utils.h` — **Sévérité** : basse — **Lignes gagnées** : ~6

Lignes 151-163 : ces deux fonctions ne sont appelées que depuis is_default_type (même fichier, lignes 169 et 174) et nulle part ailleurs dans le dépôt (vérifié par grep). Elles peuvent être repliées dans is_default_type. Contre-argument pédagogique : leurs noms documentent l'intention ; si on les garde, les conserver telles quelles est défendable. Version repliée proposée ci-dessous.

**Code actuel :**

```cpp
inline consteval bool is_copy_move_destroy_member(std::meta::info member) {
    return std::meta::is_copy_constructor(member)
        || std::meta::is_move_constructor(member)
        || std::meta::is_copy_assignment(member)
        || std::meta::is_move_assignment(member)
        || std::meta::is_destructor(member);
}

inline consteval bool may_hijack_copy_move(std::meta::info member) {
    return std::meta::is_constructor_template(member)
        || (std::meta::is_operator_function_template(member)
            && std::meta::operator_of(member) == std::meta::op_equals);
}
```

**Correction proposée :**

```cpp
inline consteval TraitAnswer is_default_type(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();

    for (std::meta::info member : std::meta::members_of(type, context)) {
        const bool may_hijack_copy_move =
            std::meta::is_constructor_template(member)
            || (std::meta::is_operator_function_template(member)
                && std::meta::operator_of(member) == std::meta::op_equals);
        if (may_hijack_copy_move)
            return "has a constructor or assignment template that may be "
                   "selected as a copy or a move; write the special "
                   "members out, or specialize the trait";

        const bool is_copy_move_destroy =
            std::meta::is_copy_constructor(member)
            || std::meta::is_move_constructor(member)
            || std::meta::is_copy_assignment(member)
            || std::meta::is_move_assignment(member)
            || std::meta::is_destructor(member);
        if (is_copy_move_destroy
            && !std::meta::is_defaulted(member)
            && !std::meta::is_deleted(member))
            return "has a user-written copy, move or destructor that can "
                   "share state the members do not show; specialize the "
                   "trait to state the intent";
    }

    return {};
}
```

**Vérification :** Le code cité existe exactement (include/threadsafe/details/utils.h:151-163) et grep confirme que is_copy_move_destroy_member et may_hijack_copy_move ne sont appelés que depuis is_default_type (lignes 169, 174). La version repliée est sémantiquement identique : même ordre d'évaluation, mêmes conditions (le `continue` devient `is_copy_move_destroy && !defaulted && !deleted`), mêmes messages d'erreur — aucun changement de comportement ni de static_assert. Aucune règle d'architecture de CLAUDE.md n'est touchée (helpers internes, pas de trait/memo/path). Les noms restent documentés via les variables locales explicites, conformément à la règle de nommage. Seule réserve, déjà admise par la proposition : le gain est purement de concision dans un code pédagogique, donc modeste mais réel et sans risque.

## 15. Quatre spécialisations tableau (T[N], T[], const T[N], const T[]) réductibles par délégation
**Fichier** : `include/threadsafe/details/synchronizable_base.h` (lignes 61-87) — **Sévérité** : basse — **Lignes gagnées** : ~4

Les paires const/non-const ne diffèrent que par le const passé à is_synchronizable_v ; les versions const peuvent déléguer aux versions non bornées correspondantes via le memo, en une ligne chacune, au lieu de répéter le prepend_path(element_step). Note : les quatre spécialisations restent nécessaires (const T[N] est ambigu entre <const T> et <T[N]>), seule la duplication des corps est réductible.

**Code actuel :**

```cpp
template <class T, std::size_t N>
struct is_unsafe_synchronizable<const T[N]> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const T>.prepend_path(detail::element_step);
    }
};

template <class T>
struct is_unsafe_synchronizable<const T[]> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const T>.prepend_path(detail::element_step);
    }
};
```

**Correction proposée :**

```cpp
template <class T, std::size_t N>
struct is_unsafe_synchronizable<const T[N]> {
    static consteval TraitAnswer diagnose() {
        return is_unsafe_synchronizable_v<const T[]>;
    }
};

template <class T>
struct is_unsafe_synchronizable<const T[]> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const T>.prepend_path(detail::element_step);
    }
};

(idem pour T[N] déléguant à T[]; gain net faible, surtout de la déduplication)
```

**Vérification :** Code cité conforme au fichier. La délégation const T[N] → is_unsafe_synchronizable_v<const T[]> (et T[N] → T[]) produit une TraitAnswer strictement identique: même reason, même trait stamp, même unique step "[]" (pas de double prepend), même answered. La résolution de spécialisation partielle sélectionne bien <const T[]>. Les static_assert des tests (test_diagnostics.cpp:35,109; test_synchronizable.cpp:146) restent satisfaits. Conforme à CLAUDE.md: délégation via memo _v, type de retour explicite, quatre spécialisations conservées. Gain réel mais faible: duplication de prepend_path(element_step) réduite de 4 à 2 occurrences.

## 16. Includes morts : <span> et <atomic>
**Fichier** : `tests/test_soundness_regressions.cpp` (lignes 3-19) — **Sévérité** : basse — **Lignes gagnées** : ~2

std::span n'apparaît nulle part dans ce fichier (les vues testées sont std::string_view), et std::atomic n'y est jamais utilisé non plus (vérifié par grep). Deux includes morts.

**Code actuel :**

```cpp
#include <array>
#include <atomic>
...
#include <span>
```

**Correction proposée :**

```cpp
supprimer les lignes '#include <atomic>' et '#include <span>'
```

**Vérification :** Vérifié: dans tests/test_soundness_regressions.cpp, 'atomic' et 'span' n'apparaissent qu'aux lignes 4 et 12 (les includes eux-mêmes, grep confirmé). Compilation testée sans ces deux includes: le target threadsafe_tests (tests = static_assert à la compilation) build sans erreur, donc aucun static_assert cassé, aucun changement de comportement, aucune règle d'architecture concernée. Includes réellement morts; suppression valide.

## 17. Include <type_traits> inutilisé
**Fichier** : `include/threadsafe/details/vocabulary.h` — **Sévérité** : basse — **Lignes gagnées** : ~1

vocabulary.h inclut <type_traits> (ligne 5) mais n'utilise aucun trait standard — le fichier ne contient que des spécialisations vides.

**Code actuel :**

```cpp
#include <memory>
#include <stop_token>
#include <type_traits>
```

**Correction proposée :**

```cpp
#include <memory>
#include <stop_token>
```

**Vérification :** Le code cité existe tel quel (lignes 3-5 de include/threadsafe/details/vocabulary.h). Le fichier ne contient que des spécialisations vides de is_unsafe_* pour std::allocator, std::stop_token et std::stop_source — aucun trait de <type_traits> n'y est utilisé (TraitAnswer vient des headers threadsafe inclus, std::allocator de <memory>, std::stop_token/std::stop_source de <stop_token>). Vérifié empiriquement: suppression de l'include puis rebuild complet avec GCC 16 — tous les tests static_assert compilent (100% Built target threadsafe_tests). Aucune règle d'architecture n'est touchée.

## 18. Alias `cow` à portée fichier : à conserver
**Fichier** : `tests/test_copy_on_write.cpp` (lignes 58-59) — **Sévérité** : basse — **Lignes gagnées** : ~0

L'alias `template <class T> using cow = threadsafe::copy_on_write<T>;` est utilisé plus de 20 fois et raccourcit réellement chaque assertion — ce n'est pas une abstraction inutile. Aucune action; noté pour montrer que la chasse aux alias a distingué les deux cas (contrairement à can_launch_task qui ne fait que renommer un concept déjà court).

**Code actuel :**

```cpp
template <class T>
using cow = threadsafe::copy_on_write<T>;
```

**Vérification :** Le code cité existe tel quel (tests/test_copy_on_write.cpp:59-60, dans un namespace anonyme) et `cow<` est utilisé 27 fois dans le fichier — la revendication « plus de 20 fois » est exacte. La proposition ne demande aucune action : elle conserve l'alias, donc aucun comportement ne change, aucun static_assert n'est cassé, aucune règle de CLAUDE.md n'est concernée. Le constat est correct : l'alias raccourcit réellement chaque assertion et n'est pas un simple renommage.

---

# Propositions examinées puis réfutées

Ces pistes ont été envisagées mais rejetées à la vérification — utiles à connaître pour ne pas les re-proposer.

### 9 spécialisations « vouch » identiques répétant le même diagnose()
**Fichier** : `include/threadsafe/details/vocabulary.h`

Le code cité existe tel quel et la macro serait fonctionnellement équivalente, mais la proposition contredit l'esprit explicite de l'architecture: CLAUDE.md exige que chaque claim unsafe soit « spelled out » (spécialisation explicite, type de retour écrit) et précise que le code est pédagogique pour une conférence — ce fichier est précisément la vitrine du point de personnalisation is_unsafe_<trait>, et une macro THREADSAFE_VOUCH cache la forme que le lecteur doit apprendre à écrire dans sa propre TU. Le gain est illusoire: ~30 lignes économisées dans un seul petit fichier, au prix de deux macros (define/undef) dans un projet qui n'en utilise aucune. La proposition elle-même reconnaît ce doute (« garder la forme longue peut être un choix délibéré »); en cas de doute, invalide.

### unanswered() : 4 lignes pour poser un booléen
**Fichier** : `include/threadsafe/details/utils.h`

Le code cité existe (include/threadsafe/details/utils.h, lignes 19-23) mais la « correction » ne supprime aucune instruction : elle tasse simplement trois instructions sur une ligne physique, et la proposition écarte elle-même l'alternative du constructeur taggé en concluant « gain purement cosmétique ». Le gain est donc illusoire (0 instruction économisée, lisibilité dégradée pour un code pédagogique), ce qui est un critère explicite d'invalidité.

### path_step_of_member : variable intermédiaire et conversions évitables
**Fichier** : `include/threadsafe/details/utils.h`

Le code cité existe tel quel (utils.h:96-104), mais la correction proposée n'apporte aucun gain — elle est même une régression. 1) Elle ne « tient pas en une expression » comme annoncé : elle garde exactement une variable intermédiaire (`name`, devenue string_view) et le même nombre de lignes. 2) Elle ajoute deux conversions explicites `std::string(...)` là où le code actuel n'en construit qu'une : le projet est en C++26, où `operator+(std::string, std::string_view)` existe (P2591), donc `name + " (" + path_step_of_type(...)` concatène déjà directement ; envelopper `path_step_of_type(...)` dans `std::string(...)` crée un temporaire supplémentaire inutile. 3) Le schéma de path_step_of_base invoqué comme modèle (`std::string("base (") + ...`) n'est pas reproduit par la correction, qui fait autre chose. Comportement identique, complexité identique ou pire : gain illusoire.

### T[N] et T[] : deux spécialisations au corps identique
**Fichier** : `include/threadsafe/details/lifetime_aware.h`

Le code cité existe tel quel et l'héritage compilerait (detail::unsafe_answer détecte diagnose() via un requires que le membre hérité satisfait). Mais la proposition viole la règle d'architecture de CLAUDE.md sur les spécialisations unsafe : « A specialization spells the claim out — static consteval TraitAnswer diagnose(), with the return type written » — une struct vide qui hérite n'énonce plus sa réclamation, or c'est précisément le point d'extension pédagogique du projet (le mot unsafe apparaît là où la connaissance est *affirmée*). De plus, le gain est illusoire : le même duo T[N]/T[] existe à l'identique dans sendable.h (l.69/77) et synchronizable_base.h (l.62/69 et const l.76/83) — le dépôt a délibérément choisi la répétition explicite comme motif, et déduire un seul fichier sur quatre paires casserait la cohérence pour économiser ~5 lignes. Le corps dupliqué est le motif, pas un accident.

### T& et T&& : même claim, même message, fusionnables par la même technique
**Fichier** : `include/threadsafe/details/lifetime_aware.h`

Le code cité existe et la fusion compilerait avec le même comportement (diagnose() hérité satisfait le requires de detail::unsafe_answer, même message, aucun walk recalculé). Mais elle viole deux règles d'architecture explicites de CLAUDE.md : « A specialization spells the claim out — static consteval TraitAnswer diagnose(), with the return type written » (une struct vide qui hérite n'énonce pas le claim ; utils.h:116-119 contient même un static_assert qui police la forme exacte d'un claim), et « never inherits from another trait » (aucune spécialisation du dépôt ne délègue par héritage — toutes écrivent `return <trait>_v<...>;`). Elle couple aussi le claim de reference_wrapper<T> à celui de T&, dépendance cachée absente du code actuel. Gain de 4 lignes contre la lisibilité pédagogique voulue : illusoire.

### diagnose_task_participant : indirection à un seul appelant, repliable
**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h`

Le code cité existe tel quel (lignes 28-34) et la réécriture en ternaire est comportementalement équivalente (même memoïsation via _v, mêmes messages, with_trait inchangé). Mais le gain est illusoire : on passe de 4 lignes à 2 sans supprimer ni l'indirection ni le helper — la proposition admet elle-même que la vraie simplification (lire is_scoped_task_participant_v) est impossible à cause du stamp de trait. Pire, elle remplace l'idiome canonique du dépôt `if (const auto answer = ...; !answer) return answer;` — utilisé partout, y compris dans l'exemple d'architecture de CLAUDE.md pour prepend_path et dans diagnose_scoped_task_participant juste au-dessus — par un ternaire `scoped_answer ? autre : scoped_answer` moins lisible et incohérent avec le style éducatif du code. Aucune complexité n'est retirée, seule l'uniformité est perdue.

### Six diagnose() identiques pour shared_ptr/weak_ptr/reference_wrapper factorisables
**Fichier** : `include/threadsafe/details/smart_pointers.h`

Le code cité existe et la factorisation est comportementalement équivalente, mais le gain est illusoire et elle viole l'esprit de l'architecture. Comptage: les 6 spécialisations actuelles font ~36 lignes; la version proposée garde 6 spécialisations (~24 lignes) plus 2 helpers et leur namespace (~12 lignes) — total quasi identique, avec une couche d'indirection en plus. Surtout, CLAUDE.md exige que chaque spécialisation unsafe « spells the claim out »: la couche is_unsafe_* est précisément l'endroit où chaque affirmation non prouvée doit être lisible sur place, et le code est explicitement pédagogique pour une conférence — la règle « shared_ptr est sendable ssi son pointé est synchronizable » doit se lire dans le corps même, pas derrière un helper shared_pointee_answer<T>(). Aucune autre spécialisation de la bibliothèque (unique_ptr, default_delete…) ne factorise ses corps ainsi; la proposition introduirait un pattern nouveau pour une compression nulle.

### Includes de conteneurs potentiellement superflus
**Fichier** : `include/threadsafe/details/allowed_std_wrappers.h`

Réfutée : quasi tous les includes sont directement utilisés dans allowed_std_wrappers.h lui-même — <algorithm> pour std::ranges::contains (l.42), <meta> pour l'API réflexion, <vector> pour wrapped_types_of (l.49), et chacun des autres conteneurs pour sa réflexion ^^std::X dans le tableau allowed_std_wrappers (l.29-37 : vector, deque, list, forward_list, basic_string, map, set, unordered_map, unordered_set, pair (<utility>), tuple, optional, variant, array). Les retirer reviendrait à dépendre d'includes transitifs de lifetime_aware.h/sendable.h — fragile et contraire à include-what-you-use, alors que la réflexion ^^ exige la déclaration du template. Le seul candidat réellement discutable est <type_traits> (aucun usage direct visible), soit un gain d'une ligne, illusoire. La proposition elle-même se qualifie de fragile et basse priorité.

### Includes morts : <functional> et <string>... vérification partielle — seul <functional> pour reference_wrapper est utilisé; rien de mort ici finalement, mais <vector> ne sert qu'à une assertion
**Fichier** : `tests/test_smart_pointers.cpp`

Le constat factuel est exact (tous les includes de tests/test_smart_pointers.cpp sont utilisés : <functional> pour std::reference_wrapper l.86-91 et 131-136, <string> pour shared_ptr<std::string> l.120-127, <vector> l.106, <atomic> l.98-105 et 124-136), mais la proposition ne contient aucune correction : c'est une non-action. En tant que simplification, le gain est nul/illusoire — rien à appliquer, donc invalide comme proposition de changement.
