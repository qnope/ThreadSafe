# Audit de simplification — détails

Chaque proposition confirmée a été vérifiée adversarialement : relecture du fichier réel,
grep de tous les usages, compilation complète GCC 16 (`cmake --build build`) avec le patch
appliqué, puis restauration de l'arbre. Voir la
[synthèse](audit-simplification-synthese.md) pour le bilan.

---

## 1. `vocabulary.h` (l. 26-54) — fusionner les six spécialisations stop_token/stop_source — **−11 lignes**

`std::stop_token` et `std::stop_source` portent exactement les mêmes trois affirmations
(sendable, synchronizable-const, lifetime-aware), écrites six fois à l'identique.

**Code actuel** :

```cpp
template <>
struct is_unsafe_sendable<std::stop_token> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_sendable<std::stop_source> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_synchronizable<const std::stop_token> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_synchronizable<const std::stop_source> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_lifetime_aware<std::stop_token> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_lifetime_aware<std::stop_source> {
    static consteval TraitAnswer diagnose() { return {}; }
};
```

**Correction** (fichier complet, version ajustée par le vérificateur : concept dans
`detail`, `#include <concepts>` ajouté) :

```cpp
#pragma once

#include <concepts>
#include <memory>
#include <stop_token>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_unsafe_sendable<std::allocator<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T>
struct is_unsafe_synchronizable<const std::allocator<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T>
struct is_unsafe_lifetime_aware<std::allocator<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

namespace detail {

template <class T>
concept stop_handle =
    std::same_as<T, std::stop_token> || std::same_as<T, std::stop_source>;

}

template <detail::stop_handle T>
struct is_unsafe_sendable<T> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <detail::stop_handle T>
struct is_unsafe_synchronizable<const T> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <detail::stop_handle T>
struct is_unsafe_lifetime_aware<T> {
    static consteval TraitAnswer diagnose() { return {}; }
};

}
```

**Verdict** : gardé (impact moyen). Aucune ambiguïté de spécialisation : les deux types
n'ont pas d'arguments template, donc la règle `std_wrapper` ne les capte pas ; la variante
non-const de `is_unsafe_synchronizable` reste non revendiquée comme avant. Le motif
« concept + spécialisation partielle contrainte » est déjà le style maison
(`detail::std_wrapper`). Build GCC 16 vert, y compris les static_assert de
`test_soundness_regressions.cpp` sur stop_token. Pédagogiquement, le concept nommé dit
que les deux poignées d'arrêt forment une même famille, au lieu de le laisser deviner par
la duplication.

---

## 2. `smart_pointers.h` (l. 52-76) — les specs *sendable* délèguent à leur forme const — **−5 lignes**

Les trois specs sendable dupliquent mot pour mot le corps des specs
`is_unsafe_synchronizable<const ...>` du même fichier (l. 92-116).

**Code actuel** :

```cpp
template <class T>
struct is_unsafe_sendable<std::shared_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<
            std::remove_cv_t<std::remove_all_extents_t<T>>>
            .prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_sendable<std::weak_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<
            std::remove_cv_t<std::remove_all_extents_t<T>>>
            .prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_sendable<std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>.prepend_path(
            detail::referent_step);
    }
};
```

**Correction** :

```cpp
template <class T>
struct is_unsafe_sendable<std::shared_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const std::shared_ptr<T>>;
    }
};

template <class T>
struct is_unsafe_sendable<std::weak_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const std::weak_ptr<T>>;
    }
};

template <class T>
struct is_unsafe_sendable<std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const std::reference_wrapper<T>>;
    }
};
```

**Verdict** : gardé (impact moyen). Réponse bit-à-bit identique : l'ordre partiel retient
`<const std::shared_ptr<T>>` face aux specs `<const T>`, la raison, le chemin (`*` ou `&`)
et l'estampille de trait (posée par le `_v` le plus profond, `with_trait` no-op ensuite)
sont inchangés — `test_diagnostics.cpp:113` assert le chemin exact `"*"` et passe. Cas
limites vérifiés (`shared_ptr<void>`, `shared_ptr<const T>`, `shared_ptr<T[]>`).
Pédagogiquement, la ligne unique énonce la loi du modèle Send/Sync : envoyer une poignée
partagée, c'est la partager — `sendable(shared_ptr<T>) ≡ synchronizable(const shared_ptr<T>)`.

---

## 3. `sendable.h` (l. 65-79) — `T[N]` délègue à `T[]` — **−4 lignes**

Les deux spécialisations tableau ont un corps strictement identique.

**Code actuel** :

```cpp
template <class T, std::size_t N>
struct is_unsafe_sendable<T[N]> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<std::remove_cv_t<T>>.prepend_path(
            detail::element_step);
    }
};

template <class T>
struct is_unsafe_sendable<T[]> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<std::remove_cv_t<T>>.prepend_path(
            detail::element_step);
    }
};
```

**Correction** :

```cpp
template <class T, std::size_t N>
struct is_unsafe_sendable<T[N]> {
    static consteval TraitAnswer diagnose() { return is_unsafe_sendable_v<T[]>; }
};

template <class T>
struct is_unsafe_sendable<T[]> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<std::remove_cv_t<T>>.prepend_path(
            detail::element_step);
    }
};
```

**Verdict** : gardé (impact moyen). Le précédent existe dans le même fichier :
`is_unsafe_sendable<T&&>` délègue déjà à `is_unsafe_sendable_v<T&>` (l. 54).
`detail::unsafe_answer` retourne `diagnose()` tel quel, donc la réponse est bit-à-bit
identique (même raison, même unique pas `[]`, même estampille). Les cas cv-qualifiés
(`const T[N]` → `const T[]` → `remove_cv_t`) sont inchangés. Build vert, y compris
`path_is(is_sendable_v<Borrowing[4]>, "[]::borrowed (int*)::*")`.

**Attention** : la même proposition sur `lifetime_aware.h` a été rejetée par son
vérificateur (voir section Rejets) — trancher les deux ensemble pour rester cohérent.

---

## 4. `allowed_std_wrappers.h` (l. 49-67) — fusionner `wrapped_types_of` dans `all_wrapped_types` — **−4 lignes**

`wrapped_types_of` n'a qu'un seul appelant dans tout le repo, qui re-itère aussitôt sur le
vector construit.

**Code actuel** :

```cpp
inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
    std::vector<std::meta::info> wrapped;
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type)))
        if (std::meta::is_type(argument))
            wrapped.push_back(std::meta::remove_cv(argument));
    return wrapped;
}

inline consteval TraitAnswer
all_wrapped_types(std::meta::info type,
                  TraitAnswer (*question)(std::meta::info)) {
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (const auto answer = question(wrapped); !answer)
            return answer.prepend_path(path_step_of_type(wrapped));

    return {};
}
```

**Correction** :

```cpp
inline consteval TraitAnswer
all_wrapped_types(std::meta::info type,
                  TraitAnswer (*question)(std::meta::info)) {
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type))) {
        if (!std::meta::is_type(argument))
            continue;

        const std::meta::info wrapped = std::meta::remove_cv(argument);
        if (const auto answer = question(wrapped); !answer)
            return answer.prepend_path(path_step_of_type(wrapped));
    }

    return {};
}
```

**Verdict** : gardé (impact moyen). Sémantique identique (même ordre, même filtre, même
`remove_cv`, même `prepend_path`) ; supprime une fonction intermédiaire et l'allocation
consteval d'un `std::vector` à chaque diagnostic, et sort plus tôt sur le premier échec.
Une seule fonction raconte toute l'histoire : « pour chaque argument de type du wrapper,
pose la question ».

---

## 5. `asynchronous_task_launcher.h` (l. 63-73) — garde morte dans `detail::explain` — **−3 lignes**

**Code actuel** :

```cpp
inline consteval std::string_view explain(TraitAnswer answer,
                                          std::meta::info type) {
    if (answer)
        return {};

    const auto rooted = answer.prepend_path(path_step_of_type(type));

    return std::define_static_string(std::string(rooted.full_path())
                                     + " is not " + rooted.trait_name
                                     + " because it " + rooted.error_message);
}
```

**Correction** :

```cpp
inline consteval std::string_view explain(TraitAnswer answer,
                                          std::meta::info type) {
    const auto rooted = answer.prepend_path(path_step_of_type(type));

    return std::define_static_string(std::string(rooted.full_path())
                                     + " is not " + rooted.trait_name
                                     + " because it " + rooted.error_message);
}
```

**Verdict** : gardé (impact faible). Depuis P2741, le message utilisateur d'un
`static_assert` n'est évalué que si la condition est fausse — prouvé empiriquement : sans
la garde, une évaluation sur une réponse positive concaténerait des `string_view` nuls et
échouerait en consteval, or toute la suite compile. Le contrat devient plus honnête :
`explain` ne parle que d'une réponse en échec, et un mésusage futur échouerait à la
compilation au lieu de produire silencieusement une chaîne vide.

---

## 6. `lifetime_aware.h` (l. 56-61) — `T&&` délègue au mémo de `T&` — **0 ligne, −1 duplication**

La chaîne `"borrows its referent instead of keeping it alive"` est écrite trois fois dans
le fichier (`T&`, `T&&`, `reference_wrapper`).

**Code actuel** :

```cpp
template <class T>
struct is_unsafe_lifetime_aware<T&&> {
    static consteval TraitAnswer diagnose() {
        return "borrows its referent instead of keeping it alive";
    }
};
```

**Correction** :

```cpp
template <class T>
struct is_unsafe_lifetime_aware<T&&> {
    static consteval TraitAnswer diagnose() {
        return is_unsafe_lifetime_aware_v<T&>;
    }
};
```

**Verdict** : gardé (impact faible). Pattern identique à `is_unsafe_sendable<T&&>` dans
`sendable.h` (l. 52-55). Identité bit-à-bit prouvée par static_assert sous g++-16 : même
message, `path_step_count == 0`, estampille « lifetime-aware » inchangée (posée dans
`is_lifetime_aware_v`, pas dans le mémo unsafe). La spécialisation `T&` devient la source
unique du message pour les références.

---

## 7. `lifetime_aware.h` (l. 92-97) — `reference_wrapper<T>` délègue au mémo de `T&` — **0 ligne, −1 duplication**

**Code actuel** :

```cpp
template <class T>
struct is_unsafe_lifetime_aware<std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return "borrows its referent instead of keeping it alive";
    }
};
```

**Correction** :

```cpp
template <class T>
struct is_unsafe_lifetime_aware<std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return is_unsafe_lifetime_aware_v<T&>;
    }
};
```

**Verdict** : gardé (impact faible). Combiné avec la proposition 6, le message
`"borrows its referent..."` n'existe plus qu'à un seul endroit. Précédent identique :
`is_unsafe_sendable<std::reference_wrapper<T>>` délègue déjà. Pédagogiquement,
« `reference_wrapper<T>` répond comme `T&` » exprime la sémantique au lieu de dupliquer
la chaîne. Vérifié par compilation des 11 TU de tests.

---

## 8. `synchronizable_base.h` (l. 3-5) — `#include <type_traits>` inutilisé — **−1 ligne**

**Code actuel** :

```cpp
#include <cstddef>
#include <meta>
#include <type_traits>
```

**Correction** :

```cpp
#include <cstddef>
#include <meta>
```

**Verdict** : gardé (impact faible). Aucun symbole de `<type_traits>` dans le fichier
(toutes les interrogations passent par `std::meta`) ; `utils.h`, inclus juste après,
l'apporte pour ses propres `std::is_void_v`/`std::is_polymorphic_v`/`std::is_final_v`.
Rebuild complet vert.

---

## 9. `test_sendable.cpp` (l. 129-140) — corps `diagnose()` sur une ligne — **−4 lignes**

**Code actuel** :

```cpp
template <>
struct threadsafe::is_unsafe_synchronizable<SyncType> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};
template <>
struct threadsafe::is_unsafe_sendable<OptIn> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};
```

**Correction** :

```cpp
template <>
struct threadsafe::is_unsafe_synchronizable<SyncType> {
    static consteval threadsafe::TraitAnswer diagnose() { return {}; }
};
template <>
struct threadsafe::is_unsafe_sendable<OptIn> {
    static consteval threadsafe::TraitAnswer diagnose() { return {}; }
};
```

**Verdict** : gardé (impact faible), avec un **caveat important** : les 13 corps triviaux
des headers utilisent la forme une-ligne (celle de l'exemple canonique de CLAUDE.md), mais
tous les autres fichiers de tests utilisent la forme multi-ligne
(`test_smart_pointers.cpp:26`, `test_containers.cpp:50`,
`test_asynchronous_task_launcher.cpp:21`, `test_diagnostics.cpp:128`,
`test_deferred_specialization.cpp:34`, `test_copy_on_write.cpp:60`,
`test_synchronizable.cpp:54-66`). Pour ne pas créer d'incohérence intra-tests, appliquer
la compaction aux 13 sites d'un coup.

---

## 10. `test_diagnostics.cpp` (l. 74-79 et 151-153) — factoriser `reasons_match` sur `reason_is` — **−2 lignes**

Le motif `!answer && std::string_view(answer.error_message) == ...` est écrit deux fois,
dans deux blocs anonymes distincts du même fichier.

**Code actuel** :

```cpp
consteval bool reasons_match(threadsafe::TraitAnswer answer,
                             threadsafe::TraitAnswer root_cause) {
    return !answer && !root_cause
        && std::string_view(answer.error_message)
               == std::string_view(root_cause.error_message);
}

// plus loin, lignes 151-153 :
consteval bool reason_is(threadsafe::TraitAnswer answer, std::string_view text) {
    return !answer && std::string_view(answer.error_message) == text;
}
```

**Correction** : remonter `reason_is` à côté de `reasons_match` et exprimer ce dernier
avec :

```cpp
consteval bool reason_is(threadsafe::TraitAnswer answer, std::string_view text) {
    return !answer && std::string_view(answer.error_message) == text;
}

consteval bool reasons_match(threadsafe::TraitAnswer answer,
                             threadsafe::TraitAnswer root_cause) {
    return !root_cause && reason_is(answer, root_cause.error_message);
}
```

puis supprimer la définition de `reason_is` aux lignes 151-153 (le second bloc
`namespace {` rouvre le même namespace anonyme, les usages des lignes 159-163 et 184
restent valides).

**Verdict** : gardé (impact faible). Vérifié par `g++-16 -std=c++26 -freflection
-fsyntax-only` : tous les static_assert passent. Le court-circuit `!root_cause &&` protège
la construction du `string_view` exactement comme l'original. Une seule définition de la
comparaison : « les raisons correspondent » signifie « la raison de answer est exactement
le texte de la cause racine ».

---

## Propositions rejetées

### `lifetime_aware.h` (l. 76-90) — `T[N]` délègue à `T[]`

Même remède que la proposition 3, corps identiques au caractère près. **Rejeté** par son
vérificateur : la délégation unsafe→unsafe change la surface de personnalisation (une
spécialisation utilisateur de `is_unsafe_lifetime_aware<X[]>` répondrait silencieusement
pour tous les `X[N]`), le gain est d'une ligne, et les deux spécialisations côte à côte
énoncent chacune directement la règle — forme jugée la plus lisible pour un code de
conférence.

**Tension à trancher** : la proposition 3 (sendable) a été gardée par un autre
vérificateur au motif du précédent `T&&` → `T&` existant. L'argument sur la surface de
personnalisation s'applique aux deux. Recommandation : appliquer les deux ou aucune, pas
une seule.

### `asynchronous_task_launcher.h` (l. 100-105) — jthread temporaire

```cpp
std::jthread{std::move(f), std::move(args)...}.join();
```

Techniquement correct (le temporaire est joint avant destruction, donc ni `request_stop`
ni second join), mais **rejeté** : contredit la règle du projet « always use explicit name
for variables » et l'objectif pédagogique. Gain d'une ligne sans gain sémantique.

### `test_synchronizable.cpp` (l. 54-66) — compaction des corps `diagnose()`

Doublon partiel de la proposition 9, **rejeté** pour périmètre trop étroit : compacter un
seul fichier de tests briserait la cohérence avec les huit autres. À faire sur les 13
sites d'un coup (voir caveat de la proposition 9).

### `vocabulary.h` — fusion stop_token/stop_source (doublon)

Deux finders ont proposé la même fusion. L'un des deux vérificateurs l'a rejetée en
prétendant que le commit 0bcb2a8 l'avait déjà appliquée — **prémisse fausse**, vérifiée
sur le fichier réel après le workflow : les six spécialisations explicites existent
toujours aux lignes 26-54. La version confirmée (proposition 1), validée par compilation,
fait foi.
