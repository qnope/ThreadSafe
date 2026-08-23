# Audit de performance — ThreadSafe

Date : 2026-08-23 — Toolchain : g++-16 (Homebrew GCC 16.2.0), C++26, `-freflection`.
Machine : macOS (Darwin 25.6.0), Apple Silicon. Toutes les mesures ci-dessous ont été
réellement compilées et exécutées (fichiers de scénarios dans le scratchpad de session,
aucun fichier du projet modifié).

## Synthèse

- **Compilation** : le coût des vérifications compile-time est **négligeable**. Sur un TU
  minimal, `-ftime-report` attribue **0,01–0,02 s (2–3 %)** à « constant expression
  evaluation » (là où vivent les fonctions `consteval` de réflexion). Le vrai coût est le
  **parsing des en-têtes standard** tirés par l'umbrella header (~0,56 s/TU, dont ~0,33 s
  évitables si on n'a besoin que des traits).
- **Mémoïsation** : déjà effective — les traits sont des variables template, donc
  l'instanciation est mémoïsée par le compilateur. Vérifié par un stress test à chemins
  exponentiels (300 structs imbriquées, 2^299 chemins de sous-objets) : temps linéaire,
  1,02 s au total.
- **Runtime** : les prétentions zéro-coût sont **vérifiées aux benchmarks et à
  l'assembleur**. `synchronized_value` = mutex nu, lecture `const` via `copy_on_write` =
  deux `ldr` (identique à `shared_ptr`, aucun refcount touché), lanceur de tâches = jthread
  nu.

Aucun problème critique ni majeur. Un finding mineur (umbrella header) et deux
suggestions.

---

## 1. Compilation

### 1.1 Mesures de référence

Build complet du projet après clean (10 TU de tests, série, `-j1`) :

```
cmake --build build --target clean && time cmake --build build -j1
real 7,13 s   (≈ 0,71 s / TU)
```

TU minimal (`#include <threadsafe/threadsafe.h>` + `main` vide) vs baseline équivalente
n'incluant que les en-têtes standard utilisés par la bibliothèque
(`<memory> <mutex> <shared_mutex> <thread> <vector> <map> <unordered_map> <atomic> <meta>`) :

| TU | temps réel |
|---|---|
| `int main(){}` seul | 0,03–0,06 s |
| baseline std headers | 0,54 s |
| `threadsafe.h` complet | 0,56–0,69 s |
| `sendable.h` + `synchronizable.h` seulement | 0,22–0,24 s |

**Le surcoût propre à ThreadSafe au-dessus de ses includes standard est ~0,15 s/TU.**
L'essentiel du temps est le parsing de la bibliothèque standard.

### 1.2 Où va le temps (`-ftime-report`, TU minimal)

```
 preprocessing                      :   0.08 ( 14%)
 parser (global)                    :   0.06 ( 10%)
 parser struct body                 :   0.07 ( 12%)
 parser inl. meth. body             :   0.08 ( 14%)
 template instantiation             :   0.18 ( 32%)
 constant expression evaluation     :   0.01 (  2%)
 overload resolution                :   0.11 ( 19%)
 TOTAL                              :   0.58
```

Sur `tests/test_sendable.cpp` (le plus gros TU de traits) : « constant expression
evaluation » monte seulement à **0,02 s (3 %)**. La réflexion C++26
(`default_is_sendable`, `default_is_const_synchronizable`, `default_is_lifetime_aware`,
`substitute`/`extract` dans `trait_value`) **n'est pas le poste dominant** ; les 32 %
d'instanciation de templates sont dus aux templates de la bibliothèque standard, pas aux
traits. Aucun expansion statement (`template for`) n'est utilisé dans les headers — le
flag `-fexpansion-statements` n'est d'ailleurs pas nécessaire pour compiler la
bibliothèque.

### 1.3 Mémoïsation : déjà acquise, vérifiée

Les traits sont des **variables template** (`is_sendable<T>`, etc.). Le compilateur
mémoïse chaque instanciation, et la récursion passe par `detail::trait_value` →
`std::meta::substitute(^^is_sendable, {type})`, qui **réinstancie la variable template**
plutôt que de rappeler la fonction `consteval` — chaque type n'est donc évalué qu'une
fois par TU, quel que soit le nombre de chemins qui y mènent.

Vérification : stress test avec 300 structs où `S<i>` contient deux `S<i-1>`
(2^299 chemins de sous-objets), interrogées pour les trois traits :

```cpp
struct S0 { int x; double y; };
struct S1 { S0 a; S0 b; int c[4]; float* p; };
// ... jusqu'à S299
static_assert(!threadsafe::is_sendable<S299> || true);
static_assert(!threadsafe::is_synchronizable<const S299> || true);
static_assert(!threadsafe::is_lifetime_aware<S299> || true);
```

Résultat : **1,02 s au total** (dont 0,56 s d'includes), « constant expression
evaluation » = 0,32 s pour 900 requêtes de traits sur des types profonds. Temps
linéaire → la mémoïsation fonctionne. **Ajouter une couche de memoization manuelle
n'apporterait rien.**

### 1.4 Finding mineur — l'umbrella header double le coût d'inclusion

**Sévérité : mineur.**

`threadsafe.h` tire `containers.h` (qui inclut `<deque> <forward_list> <list> <map>
<set> <string> <unordered_map> <unordered_set> <vector>`), `synchronized_value.h`
(`<mutex> <shared_mutex>`), `asynchronous_task_launcher.h` (`<thread>`), etc. Un
utilisateur qui ne veut que les traits paie 0,56 s/TU au lieu de 0,23 s/TU (mesuré,
§1.1) — soit **~2,4× le coût d'inclusion**.

Code actuel complet (`include/threadsafe/threadsafe.h`) :

```cpp
#pragma once

#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/containers.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/details/synchronized_value.h>
#include <threadsafe/details/copy_on_write.h>
```

Solution proposée : garder l'umbrella tel quel et ajouter un point d'entrée
« traits seulement » (nouveau fichier `include/threadsafe/traits.h`), complet :

```cpp
#pragma once

// Traits only: is_sendable, is_synchronizable, is_lifetime_aware and their
// concepts, without the vocabulary types and their standard-library includes.
// Costs ~0.23 s per TU instead of ~0.56 s for <threadsafe/threadsafe.h>.

#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/lifetime_aware.h>
```

Amélioration mesurée : **0,56 s → 0,23 s par TU** (−59 %) pour les TU qui
n'utilisent que les traits. Attention à un piège de cohérence : un TU qui n'inclut que
`traits.h` ne verra pas les spécialisations de `containers.h`/`smart_pointers.h` — pour
un `std::vector`, `is_sendable` retomberait sur la règle structurelle par défaut, qui le
refuse à cause de ses constructeurs templates (`may_hijack_copy_move`), donc réponse
identique par un autre chemin ici, mais ce ne serait **pas** garanti pour tout type.
Documenter clairement que `traits.h` n'est correct que pour des types utilisateur, ou y
inclure aussi `containers.h`/`smart_pointers.h`/`vocabulary.h` (ce qui rend le gain nul).
Pour une bibliothèque de conférence de ~900 lignes, on peut aussi juger que 0,3 s/TU ne
justifie pas la complexité — d'où la sévérité mineure.

### 1.5 Suggestion — court-circuit scalaire dans `default_is_sendable` : gain non mesurable

**Sévérité : suggestion (à ne PAS appliquer — mesuré sans effet).**

L'idée : dans `default_is_sendable`, `is_synchronizable_type(type)` (un
`substitute` + `extract`, relativement coûteux) est évalué avant `is_scalar_type(type)`
(un simple prédicat). Pour un scalaire, l'ordre inverse économise une substitution.

Code actuel (`include/threadsafe/details/sendable.h`, extrait complet de la fonction) :

```cpp
inline consteval bool default_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_sendable_type(unqualified);

    if (is_synchronizable_type(type) || is_scalar_type(type))
        return true;

    if (is_void_type(type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        throw exception(
            u8"is_sendable<T> supports only scalar, class and union types",
            type);

    if (!is_complete_type(type))
        throw exception(
            u8"is_sendable<T> requires a complete type — specialize is_sendable "
            u8"for types holding a pointer to an incomplete type (the pimpl "
            u8"idiom)",
            type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            return false;

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_sendable_type(remove_cv(type_of(member))))
            return false;

    return true;
}
```

Variante testée (seule la ligne du court-circuit change) :

```cpp
    if (is_scalar_type(type) || is_synchronizable_type(type))
        return true;
```

Mesure sur un stress test dédié aux scalaires (400 enums distincts + 400 structs à
membres scalaires, 400 requêtes `is_sendable`), 3 runs chacun :

| version | temps réels |
|---|---|
| ordre actuel | 0,83 / 0,81 / 0,82 s |
| scalaire d'abord | 0,79 / 0,79 / 0,82 s |

Gain **0–3 %, dans le bruit de mesure** : la mémoïsation fait que la substitution
n'a lieu qu'une fois par type distinct, et `is_synchronizable<T>` (non-const) est le
template primaire `= false`, trivial à instancier. La variante est sémantiquement
équivalente (un scalaire est sendable par les deux chemins) et légèrement plus lisible
(test le moins cher d'abord), mais **ne changez pas le code pour la performance** — il
n'y a rien à gagner. Même conclusion pour toute memoization manuelle (§1.3) : les
mécanismes du langage la fournissent déjà.

---

## 2. Runtime

Benchmarks compilés avec `-O2 -std=c++26 -freflection`, exécutés plusieurs fois ;
chiffres du run stabilisé (le premier run d'un processus surestime les sections
initiales — un écart apparent de 20 % sur le lanceur de tâches s'est révélé être un
artefact d'ordre de mesure, éliminé en interleavant les scénarios).

### 2.1 `synchronized_value<T>` vs `shared_mutex` + `T` nu — **zéro-coût confirmé**

| scénario (10 M itérations) | ns/op |
|---|---|
| `synchronized_value::lock()` + écriture | 5,78–5,94 |
| `std::unique_lock{shared_mutex}` + écriture nue | 5,78–5,92 |
| `synchronized_value::lock_shared()` + lecture | 5,88 |
| `std::shared_lock{shared_mutex}` + lecture nue | 5,94 |

Écart : **nul** (< 2 %, dans le bruit). Le `value_guard` (un `unique_lock`/`shared_lock`
+ un `T*`) est entièrement inliné à `-O2` ; le choix `shared_lock` vs `unique_lock` du
`const_guard` est fait à la compilation via `std::conditional_t`, sans branche runtime.

### 2.2 `copy_on_write<T>` vs `shared_ptr` — **zéro-coût confirmé, y compris à l'assembleur**

| scénario | ns/op |
|---|---|
| lecture const `(*cow)[i]`, `vector<int>(64)` | 0,25 |
| lecture `(*shared_ptr<const vector>)[i]` | 0,25 |
| `as_mutable()` propriétaire unique (pas de copie) | 0,26 |
| `as_mutable()` bloc partagé (copie handle + copie 64 ints) | ~35 |

La voie chaude `as_mutable()` unique ne coûte qu'un chargement de `use_count()` + une
branche jamais prise — identique à la lecture. La copie à ~35 ns/op sur bloc partagé est
le coût attendu (allocation `make_shared` + copie de 64 ints), c'est le contrat de
l'abstraction, pas un coût caché.

Vérification assembleur (ARM64, `-O2 -S`) — la lecture const n'a **aucun coût caché**
(pas de refcount, pas de verrou, pas d'appel) :

```cpp
int read_cow(const threadsafe::copy_on_write<int>& cow) { return *cow; }
```

```asm
__Z8read_cowRKN10threadsafe13copy_on_writeIiEE:
	ldr	x0, [x0]      ; charge le pointeur du shared_ptr
	ldr	w0, [x0]      ; charge la valeur
	ret
```

Deux `ldr`, soit exactement ce que produit un `shared_ptr<const T>` ou tout handle
détenant un pointeur ; la seule instruction de plus qu'un `const int*` nu est
l'indirection inhérente au handle. `operator*`/`operator->` sont `noexcept` et le
compilateur constate lui-même l'absence d'aliasing.

### 2.3 Lanceur de tâches — **parité avec `std::jthread` nu**

Mesure interleavée (2 000 threads créés+joints par section, 2 runs) :

| scénario | ns/op |
|---|---|
| `launch_scoped_task([]{...})` | 12 169–13 049 |
| `std::jthread{...}; join();` nu | 12 682–18 916 |

Parité complète : le coût est celui de la création/jointure du thread par l'OS
(~12,7 µs), les contraintes `requires sendable<F> && ...` sont purement compile-time et
le passage par valeur de `F` n'ajoute qu'un move d'une lambda vide. (Premier chiffre à
18,9 µs = warmup du premier scénario du processus, quel qu'il soit.)

`launch_task` ajoute un `emplace_back` dans le `std::vector<std::jthread>` — coût
d'amortissement de vecteur standard, rien de spécifique à la bibliothèque.

---

## 3. Récapitulatif des findings

| # | Sévérité | Finding | Gain mesuré |
|---|---|---|---|
| 1 | mineur | L'umbrella `threadsafe.h` coûte 0,56 s/TU quand les traits seuls coûtent 0,23 s ; un en-tête `traits.h` optionnel réduirait de 59 % le coût d'inclusion des TU traits-only (avec un piège de cohérence à documenter, §1.4) | −0,33 s/TU |
| 2 | suggestion | Court-circuit `is_scalar_type` avant `is_synchronizable_type` dans `default_is_sendable` : équivalent, marginalement plus lisible, mais gain 0–3 % dans le bruit — ne pas appliquer pour la performance (§1.5) | ~0 |
| 3 | suggestion | Constat positif à garder tel quel : la mémoïsation des traits est déjà assurée par l'instanciation des variables template (stress test à 2^299 chemins en temps linéaire, §1.3) ; toute memoization manuelle serait du bruit | n/a |

**Runtime : aucun finding.** Les trois prétentions zéro-coût (`synchronized_value`,
lecture const de `copy_on_write`, lanceur de tâches) sont vérifiées par benchmarks -O2
et, pour `copy_on_write`, par inspection de l'assembleur. Les vérifications compile-time,
« attendues lentes », sont en réalité à 2–3 % du temps de compilation d'un TU — le poste
dominant reste le parsing de la bibliothèque standard.
