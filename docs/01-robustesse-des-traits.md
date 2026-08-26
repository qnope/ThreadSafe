# 01 — Robustesse des traits

> Audit de `threadsafe::is_synchronizable<T>`, `is_synchronizable<const T>`,
> `is_lifetime_aware<T>` et `is_sendable<T>`.
> Toolchain : GCC 16.2.0 (Homebrew), `-std=c++26 -freflection`.
> Rapports frères : [00](./00-synthese.md) · [02](./02-robustesse-des-helpers.md) ·
> [03](./03-couverture-de-tests.md) · [04](./04-diagnostics.md) ·
> [05](./05-simplicite.md) · [06](./06-performance-compilation.md) ·
> [07](./07-performance-execution.md) · [08](./08-api-et-flexibilite.md) ·
> [09](./09-methodologie.md)

## Verdict

Le cœur structurel des quatre traits est solide et je n'ai pas réussi à le tromper : la marche
sur les membres résiste à dix formes de contrebande (union anonyme, `[[no_unique_address]]`,
bit-field, base virtuelle, structure imbriquée sans nom), le `mutable` est attrapé, le const
derrière une indirection n'est jamais cru, `has_unreflectable_state` empêche la lambda
`[&local]{}` de passer, et les 11 unités de test compilent sans une seule assertion qui
figerait un comportement buggé. Les défauts que je rapporte sont de trois natures très
différentes, et c'est cette séparation qui vaut mieux qu'une liste : (a) des **trous de forme**,
où la règle correcte existait et n'a pas été appliquée — les membres `static` invisibles
(ADV-01), le blanchiment cv sur les enveloppes standard (TC-1), la garde de type dynamique
posée sur `unique_ptr` seul (ADV-03) — tous corrigeables, et j'ai vérifié moi-même que les
cinq correctifs recommandés composent et gardent les 11 TU vertes ; (b) une **limite
inhérente**, où la réponse est structurellement juste et le code ne l'est pas — une méthode
`const` qui écrit via `const_cast`, un global, un singleton, un handle indexant un
`thread_local` — que **aucune** version de ces traits ne peut fermer, parce que P2996 reflète
des déclarations et jamais des corps de fonction ; (c) une **famille de faux rejets** dont
l'ampleur est mesurée ici à 16 types sur 27 du vocabulaire standard, qui n'est pas un problème
de sûreté mais qui décide si la bibliothèque est utilisable. La recommandation la plus rentable
du rapport n'est pas un correctif : c'est d'**écrire la limite (b) noir sur blanc**, parce
qu'elle est absente de `CLAUDE.md` et des onze en-têtes, et parce qu'elle est le meilleur
moment que cette conférence puisse offrir.

---

## Ce que j'ai revérifié moi-même

Les agents qui ont produit les 37 constats ont compilé et exécuté ce qu'ils rapportent, mais
seuls les constats critiques avaient été revérifiés par le lead. J'ai recompilé et réexécuté
moi-même, contre les en-têtes du dépôt, tout ce qui suit — et un résultat m'a fait corriger
trois constats à la fois.

| Vérification | Résultat |
|---|---|
| Base : 11 TU de `tests/` en `-fsyntax-only` | 11/11 propres |
| ADV-01 — rayon d'action du membre `static` sur les en-têtes intacts | 11 assertions tiennent |
| ADV-01 — correctif appliqué | ferme le trou, 11/11 TU, coût mesuré : 1 faux rejet |
| TC-1 — blanchiment cv | 8 assertions échouent sur les en-têtes intacts |
| TC-1 — correctif du lead (au niveau du concept) | ferme les 8, 11/11 TU |
| ADV-03 — asymétrie de la garde de type dynamique | 8 « HOLE » confirmés ouverts |
| ADV-03 — correctif appliqué | ferme les 8, garde les positifs légitimes, 11/11 TU |
| TC-5 — `shared_ptr` ignore l'opt-out | confirmé (1 assertion) |
| TC-3 — `is_defaulted` bascule sur un `= default` hors ligne | confirmé, correctif `is_user_provided` : 11/11 TU |
| TC-2 / TLS-02 / ADV-07 — types récursifs | **correctif complété et vérifié — voir §6** |
| ADV-08 — toute lambda capturante refusée | confirmé, y compris capture par valeur et par `unique_ptr` |
| Panorama de 27 types standard (4 questions chacun) | table §5, mesurée par moi |
| Les 5 correctifs recommandés, composés dans un seul arbre | tous les repros passent, 11/11 TU |
| Coût en compilation des 5 correctifs | dans le bruit (±20 ms sur ~700 ms) |

Une précision honnête sur les temps : ma machine est plus chargée que celle du lead (TU vide
avec l'en-tête parapluie : 640 ms chez moi contre 586 ms mesurés au repos par le lead). Seul
le **delta** entre arbre intact et arbre corrigé est significatif, et il est nul à la précision
de la mesure. Les chiffres absolus de compilation font l'objet du rapport
[06](./06-performance-compilation.md).

---

## 1. Les trois failles de sûreté critiques, et leur racine

### 1.1 ADV-01 — un membre `static` est invisible pour les trois marches structurelles

`is_sendable`, `is_synchronizable<const T>` et `is_lifetime_aware` parcourent tous
`std::meta::nonstatic_data_members_of`, dont le contrat est précisément de sauter les membres
`static`. Or un membre `static` n'est pas un état par objet : **tous** les objets du type
partagent le même, y compris la copie qui vient de traverser vers l'autre thread, et le `const`
sur l'instance ne le contraint en rien.

Le type problématique, complet et compilable :

```cpp
#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <thread>
#include <type_traits>
#include <vector>

class LookupTable {
public:
    int find(int key) const {
        ++probe_count_;              // écrit depuis une méthode CONST
        return key * 2;
    }
    static long probes() { return probe_count_; }

private:
    static inline long probe_count_ = 0;
};

static_assert(threadsafe::is_sendable_v<LookupTable>);
static_assert(threadsafe::is_synchronizable_v<const LookupTable>);
static_assert(threadsafe::is_lifetime_aware_v<LookupTable>);
static_assert(threadsafe::launchable_task<decltype([](LookupTable){}), LookupTable>);
static_assert(std::is_same_v<threadsafe::synchronized_value<LookupTable>::mutex,
                             std::shared_mutex>,
              "la bibliotheque a choisi un shared_mutex : les lecteurs sont concurrents");
static_assert(std::is_same_v<threadsafe::synchronized_value<LookupTable>::const_guard,
                             threadsafe::value_guard<const LookupTable,
                                 std::shared_lock<std::shared_mutex>>>);
static_assert(threadsafe::is_sendable_v<threadsafe::copy_on_write<LookupTable>>,
              "copy_on_write le benit aussi : N lecteurs, un seul bloc partage");

int main() {
    constexpr long per_thread = 200000;
    constexpr int thread_count = 4;

    threadsafe::synchronized_value<LookupTable> table;

    std::vector<std::jthread> workers;
    for (int i = 0; i < thread_count; ++i)
        workers.emplace_back([&table] {
            for (long n = 0; n < per_thread; ++n) {
                auto reader = table.lock_shared();   // shared_lock : concurrent
                (void)reader->find(static_cast<int>(n));
            }
        });
    workers.clear();

    std::printf("attendu %ld sondes, observe %ld\n",
                per_thread * thread_count, LookupTable::probes());
    return LookupTable::probes() == per_thread * thread_count ? 0 : 1;
}
```

**Observé.** Le fichier compile proprement contre les en-têtes du dépôt — j'ai revérifié les
onze assertions moi-même aujourd'hui, elles tiennent toutes. À l'exécution avec la vraie
bibliothèque (`g++-16 -O2 -pthread`), trois exécutions consécutives rapportées par l'agent :

```
attendu 800000 sondes, observe 799936   exit=1
attendu 800000 sondes, observe 799962   exit=1
attendu 800000 sondes, observe 799869   exit=1
```

Et le build ThreadSanitizer de l'extraction fidèle (fidélité vérifiée ligne à ligne contre
`synchronized_value.h`) :

```
WARNING: ThreadSanitizer: data race (pid=80699)
  Write of size 8 at 0x000104538000 by thread T2:
  Previous write of size 8 at 0x000104538000 by thread T1:
  Location is global 'LookupTable::probe_count_' at 0x000104538000
SUMMARY: ThreadSanitizer: data race
```

Deux threads écrivant les mêmes 8 octets alors que **les deux** ne tiennent qu'un
`shared_lock`. Le lead a revérifié ce fait personnellement.

**Le contraste qui en fait un trou et non une politique.** Le même partage, écrit autrement,
est correctement refusé — vérifié par moi dans le même fichier :

```cpp
struct MutableCache { mutable int cache_ = 0; mutable bool computed_ = false; };
struct PointerCache { long *shared_counter_; };

static_assert(!threadsafe::is_synchronizable_v<const MutableCache>);
static_assert(!threadsafe::is_sendable_v<PointerCache>);
static_assert(!threadsafe::is_synchronizable_v<const PointerCache>);
```

**Ce n'est pas une limite de la réflexion.** Un membre `static` *est* une déclaration.
`std::meta::members_of` + `is_variable` le voit, avec son nom, son type et sa durée de
stockage — l'agent a imprimé les quatre membres d'une classe témoin, y compris
`counter : long int [static-sd]` et `per_thread : int [thread-sd]`. La bibliothèque a manqué
ce cas parce qu'elle a tendu la main vers `nonstatic_data_members_of` sans se demander ce
qu'elle sautait. C'est exactement la règle que Rust énonce dans le langage lui-même : un
`static` doit être `Sync`.

**Correctif** (`fix_regression_checked: suite-passes`, **et revérifié par moi : 11/11 TU**).
Trois éditions.

*(1) `include/threadsafe/details/utils.h`* — ajouter `#include <vector>` au bloc d'includes,
puis insérer cette fonction complète immédiatement **avant**
`inline consteval bool is_copy_move_destroy_member(std::meta::info member) {` :

```cpp
// nonstatic_data_members_of saute deliberement les membres static, et un membre
// static n'est pas un etat par objet : tous les objets du type partagent le meme
// -- y compris la copie qui a traverse vers l'autre thread, et l'objet dont deux
// lecteurs tiennent une reference const. Ni envoyer une copie ni restreindre
// l'acces a const ne le contraint, donc le seul trait qui puisse le benir est le
// trait complet. C'est la regle que Rust enonce dans le langage lui-meme : un
// `static` doit etre Sync.
//
// type_of porte la cv-qualification du membre, donc un `static constexpr int` est
// interroge comme is_synchronizable<const int> et passe de lui-meme.
inline consteval std::vector<std::meta::info>
static_data_members_of(std::meta::info type) {
    std::vector<std::meta::info> statics;
    for (std::meta::info member :
         std::meta::members_of(type, std::meta::access_context::unchecked()))
        if (std::meta::is_variable(member))
            statics.push_back(member);
    return statics;
}
```

Toujours dans `utils.h`, pour que le diagnostic nomme le membre et non le type englobant,
insérer dans `describe` juste après la branche `is_nonstatic_data_member` et avant la branche
`is_base` :

```cpp
    if (std::meta::is_variable(subject))
        return u8"static member `" + member_name(subject) + u8"` of type "
             + type_name(std::meta::type_of(subject));
```

et dans `path_step`, à la même position :

```cpp
    if (std::meta::is_variable(subject))
        return u8"::" + member_name(subject) + u8" (static "
             + type_name(std::meta::type_of(subject)) + u8")";
```

*(2) `include/threadsafe/details/sendable.h`* — dans `diagnose_default_is_sendable`, insérer
entre la boucle `bases_of` et la boucle `nonstatic_data_members_of` :

```cpp
    for (info member : static_data_members_of(type))
        if (!is_synchronizable_type(type_of(member)))
            reject_at(member,
                      u8"is a static data member: every object of this type "
                      u8"shares the one object, so the copy that crosses to "
                      u8"the other thread shares it too — its type must be "
                      u8"synchronizable",
                      path);
```

*(3) `include/threadsafe/details/synchronizable.h`* — dans
`diagnose_default_is_const_synchronizable`, insérer entre la boucle `bases_of` et la boucle
`nonstatic_data_members_of` :

```cpp
    for (info member : static_data_members_of(type))
        if (!is_synchronizable_type(type_of(member)))
            reject_at(member,
                      u8"is a static data member: it is shared by every object "
                      u8"of this type, and const access to one object does not "
                      u8"restrain it — its type must be synchronizable",
                      path);
```

**Ce que j'ai mesuré après application**, avec ce fichier d'acceptation complet :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <map>
#include <string>

class LookupTable {
public:
    int find(int key) const { ++probe_count_; return key * 2; }
private:
    static inline long probe_count_ = 0;
};
struct Session      { static inline std::map<std::string,int> table_; int id_; };
struct ArenaHandle  { static inline thread_local int *arena_; std::size_t index_; };
struct BenignInt    { static constexpr int limit_ = 4; int v_; };
struct BenignArr    { static constexpr int table_[3] = {1,2,3}; int v_; };
struct BenignStr    { static inline const std::string label_ = "x"; int v_; };
struct BenignAtomic { static inline std::atomic<long> count_{0}; int v_; };
struct CharPtr      { static constexpr const char *label_ = "x"; int v_; };

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(!is_sendable_v<LookupTable>);
static_assert(!is_synchronizable_v<const LookupTable>);
static_assert(!is_sendable_v<Session>);
static_assert(!is_synchronizable_v<const Session>);
static_assert(!is_sendable_v<ArenaHandle>);
static_assert(is_sendable_v<BenignInt>    && is_synchronizable_v<const BenignInt>);
static_assert(is_sendable_v<BenignArr>    && is_synchronizable_v<const BenignArr>);
static_assert(is_sendable_v<BenignStr>    && is_synchronizable_v<const BenignStr>);
static_assert(is_sendable_v<BenignAtomic> && is_synchronizable_v<const BenignAtomic>);
static_assert(!is_sendable_v<CharPtr>,
              "COUT MESURE : static constexpr const char* est desormais refuse");

int main() {}
```

Sur les en-têtes intacts ce fichier produit 6 échecs ; sur l'arbre corrigé, **zéro**. Les 11 TU
de `tests/` restent vertes. Le coût est donc exactement **un** faux rejet :
`static constexpr const char* label = "x"`, parce que `is_synchronizable<const char>` est faux.
Ce n'est pas une politique nouvelle : la bibliothèque non corrigée refuse déjà un membre **non
statique** `const char*` pour la même raison. Le correctif ne fait qu'appliquer la position
existante « le const derrière une indirection n'est jamais cru » aux membres statiques aussi.

**Qualité du diagnostic après correctif** — je l'ai capturé moi-même :

```
'LookupTable::probe_count_ (static long int) is a static data member: every object
 of this type shares the one object, so the copy that crosses to the other thread
 shares it too — its type must be synchronizable'
```

et à travers un membre imbriqué :

```
'Outer::inner_ (Inner)::shared_ (static long int) is a static data member: every
 object of this type shares the one object, so the copy that crosses to the other
 thread shares it too — its type must be synchronizable'
```

C'est au niveau du reste des diagnostics de la bibliothèque, qui est haut (voir
[04](./04-diagnostics.md)).

---

### 1.2 ADV-02 — une méthode `const` qui écrit via `const_cast` est bénie, et corrompt le tas

Même famille, verdict opposé. La bibliothèque refuse correctement `mutable`. Le **même** cache
paresseux écrit avec `const_cast` au lieu de `mutable` est béni : `is_sendable_v` vrai,
`is_synchronizable_v<const T>` vrai, `synchronized_value` choisit `std::shared_mutex`, et
`copy_on_write` distribue le même bloc à tous les lecteurs.

Le type problématique, tel qu'il est soumis aux vrais en-têtes :

```cpp
#include <threadsafe/threadsafe.h>
#include <shared_mutex>
#include <type_traits>
#include <vector>

struct LazyTable {
    std::vector<int> rows;
    bool ready = false;
    const std::vector<int>& get() const {
        if (!ready) {
            auto& self = *const_cast<LazyTable*>(this);
            for (int i = 0; i < 2048; ++i) self.rows.push_back(i);   // realloue
            self.ready = true;
        }
        return rows;
    }
};

static_assert(threadsafe::is_sendable_v<LazyTable>);
static_assert(threadsafe::is_synchronizable_v<const LazyTable>);
static_assert(std::is_same_v<threadsafe::synchronized_value<LazyTable>::mutex,
                             std::shared_mutex>);
static_assert(std::is_same_v<threadsafe::synchronized_value<LazyTable>::const_guard,
                             threadsafe::value_guard<const LazyTable,
                                 std::shared_lock<std::shared_mutex>>>);

int main() {}
```

Ce fichier compile sans une erreur. Le programme d'exercice, complet (extraction plain-C++
fidèle du corps de `synchronized_value`, parce que la seule toolchain avec un runtime
sanitizer opérationnel ici ne compile pas la réflexion) :

```cpp
#include <cstdio>
#include <barrier>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

template <class T, class Lock>
class value_guard {
public:
    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}
    value_guard(const value_guard&) = delete;
    T* operator->() const& noexcept { return value_; }
private:
    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
public:
    using mutex = std::shared_mutex;                       // le choix de ThreadSafe
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;
    template <class... Args> explicit synchronized_value(Args&&... a)
        : value_(std::forward<Args>(a)...) {}
    [[nodiscard]] const_guard lock_shared() const { return const_guard{mutex_, value_}; }
private:
    mutable mutex mutex_;
    T value_;
};

struct LazyTable {
    std::vector<int> rows;
    bool ready = false;
    const std::vector<int>& get() const {
        if (!ready) {
            auto& self = *const_cast<LazyTable*>(this);
            for (int i = 0; i < 2048; ++i) self.rows.push_back(i);
            self.ready = true;
        }
        return rows;
    }
};

int main() {
    for (int round = 0; round < 400; ++round) {
        synchronized_value<LazyTable> table;
        std::barrier start{2};
        std::vector<std::thread> workers;
        for (int i = 0; i < 2; ++i)
            workers.emplace_back([&table, &start] {
                start.arrive_and_wait();
                auto reader = table.lock_shared();
                long sum = 0;
                for (int v : reader->get()) sum += v;   // itere pendant que l'autre realloue
                if (sum == -1) std::printf("jamais\n");
            });
        for (auto& w : workers) w.join();
    }
    std::printf("survecu a tous les tours\n");
}
```

**Observé** (`clang++ -std=c++20 -fsanitize=address -g -O1 -pthread`) :

```
==91084==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000001ff0
  freed by thread T2 here:
  previously allocated by thread T2 here:
SUMMARY: AddressSanitizer: heap-buffer-overflow vector.h:845 in
  std::vector<int>::__swap_out_circular_buffer(...)
```

et sur la variante `rows.assign(4096, 7)`, TSan rapporte trois courses distinctes.

**Correctif : AUCUN, et c'est le résultat négatif porteur du rapport.** L'écriture est une
*instruction dans un corps de fonction*. L'agent a énuméré toute la surface de requête
`consteval` de `<meta>` dans GCC 16 : les prédicats liés aux fonctions sont exactement
`is_function`, `is_function_type`, `is_function_template`, `is_function_parameter`,
`is_conversion_function(_template)`, `is_operator_function(_template)`,
`is_special_member_function`, `is_vararg_function`, `is_member_function_pointer_type`. La seule
chose qui touche à une définition est `source_location_of` — un fichier et une ligne, pas une
sémantique. Il n'y a pas de `body_of`, pas de réflexion d'instruction, pas de réflexion
d'expression, pas de requête « lit / écrit ». **P2996 reflète des déclarations ; il ne reflète
pas des définitions.** Voir §7.

La même racine produit SV-01 et COW-01 dans [02](./02-robustesse-des-helpers.md) : là-bas, le
mauvais « oui » du trait est converti par `synchronized_value` en un mutex plus faible, et par
`copy_on_write` en un bloc partagé. Le trait est la cause, les helpers sont l'amplificateur.

---

### 1.3 TC-1 — un cv-qualificatif de premier niveau blanchit un opt-out sur toute enveloppe standard

C'est le seul défaut du rapport qui transforme un **« non » délibéré de l'utilisateur en
« oui » silencieux**.

`template <detail::std_wrapper T> struct is_sendable<T>` est une spécialisation partielle
contrainte dont le concept accepte `const std::vector<X>` aussi volontiers que
`std::vector<X>`. Elle prend donc le pas à la fois sur la spécialisation explicite de
l'utilisateur et sur le renvoi cv du template primaire, et **recalcule** la réponse à partir
des arguments de template — en court-circuitant sur `is_synchronizable_type(type)`, qui pour une
écriture `const` est la question **lecture seule**, strictement plus faible.

Le fichier de reproduction, complet — je l'ai recompilé aujourd'hui :

```cpp
#include <threadsafe/threadsafe.h>

#include <optional>
#include <string>
#include <vector>

namespace {
// Un handle enregistre aupres du thread qui l'a cree : lisible partout,
// mais il ne doit jamais etre detruit sur un autre thread.
struct RenderHandle { int descriptor_; };
// Un element que l'utilisateur accepte, dans un conteneur qu'il n'accepte pas.
struct Row { int value_; };
}

// Les deux facons documentees de dire « ne pas envoyer ceci ».
template <>
struct threadsafe::is_sendable<RenderHandle> : std::false_type {};
template <>
struct threadsafe::is_lifetime_aware<RenderHandle> : std::false_type {};
template <>
struct threadsafe::is_sendable<std::vector<Row>> : std::false_type {};
template <>
struct threadsafe::is_lifetime_aware<std::vector<Row>> : std::false_type {};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// (a) l'element est opt-out
static_assert(!is_sendable_v<std::vector<RenderHandle>>);
static_assert(!is_sendable_v<const std::vector<RenderHandle>>);
static_assert(!is_sendable_v<const volatile std::vector<RenderHandle>>);
static_assert(!is_sendable_v<const std::optional<RenderHandle>>);
static_assert(!is_lifetime_aware_v<const std::vector<RenderHandle>>);

// (b) le conteneur lui-meme est opt-out
static_assert(!is_sendable_v<std::vector<Row>>);
static_assert(!is_sendable_v<const std::vector<Row>>);
static_assert(!is_sendable_v<volatile std::vector<Row>>);
static_assert(!is_sendable_v<const volatile std::vector<Row>>);
static_assert(!is_lifetime_aware_v<std::vector<Row>>);
static_assert(!is_lifetime_aware_v<const std::vector<Row>>);
static_assert(!is_lifetime_aware_v<volatile std::vector<Row>>);

// les reponses ordinaires ne doivent pas bouger
static_assert(is_sendable_v<std::vector<int>>);
static_assert(is_sendable_v<const std::vector<int>>);
static_assert(is_sendable_v<volatile std::vector<int>>);
static_assert(is_lifetime_aware_v<const std::vector<std::string>>);
static_assert(is_synchronizable_v<const std::vector<int>>);
static_assert(!is_synchronizable_v<const std::vector<int*>>);
```

**Observé, par moi, aujourd'hui** — 8 assertions échouent, exactement celles que le constat
annonce :

```
work/cv_launder.cpp:26:15: error: static assertion failed
work/cv_launder.cpp:27:15: error: static assertion failed
work/cv_launder.cpp:28:15: error: static assertion failed
work/cv_launder.cpp:32:15: error: static assertion failed
work/cv_launder.cpp:33:15: error: static assertion failed
work/cv_launder.cpp:34:15: error: static assertion failed
work/cv_launder.cpp:36:15: error: static assertion failed
work/cv_launder.cpp:37:15: error: static assertion failed
```

c'est-à-dire `is_sendable_v<const std::vector<RenderHandle>> == true` tandis que
`is_sendable_v<std::vector<RenderHandle>> == false`. Et l'exploitation passe par le vocabulaire
de la bibliothèque elle-même : `launchable_task` accepte
`std::shared_ptr<synchronized_value<const std::vector<RenderHandle>>>`, donc le dernier
propriétaire partagé — sur le thread récepteur — y exécute `~RenderHandle`.

**Correctif recommandé : celui du lead**, qui déplace la garde dans le concept plutôt que sur
chaque spécialisation. Dans `include/threadsafe/details/allowed_std_wrappers.h`, remplacer le
concept par sa version complète :

```cpp
// La famille comme concept : ce sur quoi les trois specialisations sont indexees.
//
// Une ecriture cv-qualifiee ne doit PAS atteindre ces regles : le concept
// l'accepterait, et la reponse serait alors recalculee a partir des arguments
// au lieu d'etre renvoyee vers la forme non qualifiee -- la ou vit la
// specialisation explicite de l'utilisateur. Le template primaire renvoie deja
// le cv vers la forme non qualifiee ; laissons-lui ce travail.
template <class T>
concept std_wrapper =
    std::same_as<T, std::remove_cv_t<T>> && is_allowed_std_wrapper(^^T);
```

**Vérifié par moi** : ce seul changement ferme les 8 assertions ci-dessus et les 11 TU restent
vertes. Le fichier compile tel quel — `std::same_as` arrive transitivement — mais ajouter
`#include <concepts>` au bloc d'includes du fichier est la bonne hygiène.

TC-1 proposait une variante équivalente qui ajoute `requires std::is_same_v<T, std::remove_cv_t<T>>`
sur chacune des deux spécialisations non-const (`is_sendable` et `is_lifetime_aware`), la règle
`is_synchronizable<const T>` étant déjà indexée sur `const T`. Elle passe aussi la suite. La
version du lead est préférable : une ligne, un seul endroit, et elle documente l'invariant à
l'endroit où le concept est défini.

---

### 1.4 La racine commune

ADV-01 et ADV-02 sont **le même symptôme** — un état partagé que la marche structurelle ne voit
pas — mais **deux causes opposées**, et les confondre serait passer à côté de la leçon.

| | ADV-01 (`static inline long`) | ADV-02 (`const_cast` dans une méthode `const`) |
|---|---|---|
| Où vit le partage | dans une **déclaration** | dans un **corps de fonction** |
| La réflexion peut-elle le voir ? | **oui** — `members_of` + `is_variable` | **non** — aucune requête n'atteint un corps |
| Statut | oubli, corrigé ici, 11/11 TU | limite inhérente, aucun correctif possible |
| Ce que fait Rust | le langage exige qu'un `static` soit `Sync` | rustc analyse les corps, et `const_cast` n'existe pas |

La formulation rigoureuse, et c'est elle qui devrait aller sur une diapositive : **les traits
sont sains vis-à-vis de la FORME d'un type et ne disent rien de son COMPORTEMENT.** Chaque
constat de ce rapport tombe dans l'une des deux catégories, plus une troisième que §3 isole —
la **visibilité**, qui n'est ni la forme ni le comportement mais le modèle d'unités de
traduction de C++.

TC-1 est d'une quatrième nature encore : ni forme, ni comportement, ni visibilité, mais un
**accident de résolution de spécialisation partielle**. C'est le seul défaut purement C++ du
lot, et c'est aussi le seul qui inverse une décision explicite de l'utilisateur.

---

## 2. Trait par trait

La tâche demandait de tester **individuellement** chacun des traits. Voici les quatre, avec
d'abord la règle, ensuite ce qui a résisté — les attaques qui n'ont **pas** cassé la
bibliothèque, qui sont longues et qui constituent la preuve que le noyau est bon — et enfin ses
défauts propres, par sévérité.

### 2.1 `is_synchronizable<T>` — « plusieurs threads à la fois » (≈ Rust `Sync`)

**La règle.** Opt-in, `false` par défaut (`synchronizable_base.h` :
`template <class T> struct is_synchronizable : std::false_type {};`). Trois seules réponses
positives sont écrites dans la bibliothèque : les types fonction (`is_synchronizable<F>` pour
`function_type F`), `std::atomic<T>` (qui hérite de `is_sendable<T>`), et ce que l'utilisateur
promet via `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE`. Plus les propagations tableau
`T[N]` / `T[]`.

**Ce qui a résisté.**

- **Le mécanisme de récursion réflexive fonctionne exactement comme documenté.** Une
  spécialisation écrite dans l'unité de traduction de l'utilisateur est atteinte par la marche,
  via `detail::trait_value` / `std::meta::substitute`
  (`tests/test_deferred_specialization.cpp`, plus un scénario indépendant montrant qu'un
  `is_sendable<std::vector<UserType>>` écrit après coup est bien lu).
- **La divergence silencieuse « vouch différé » n'existe pas.** L'hypothèse — interroger un
  trait avant un `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` puis après donne deux réponses sans
  diagnostic — est fausse : GCC 16 émet
  `specialization of threadsafe::is_synchronizable<Registry> after instantiation`. Seul le cas
  **inter-TU** est silencieux (§3).
- **Le refus délibéré de bénir l'écriture à travers un `stop_token` partagé tient.**
  `is_synchronizable_v<std::stop_source>` est faux tandis que
  `is_synchronizable_v<const std::stop_source>` est vrai, exactement comme `vocabulary.h` le
  documente et comme le commit `79f1e4f` l'a décidé. Deux scénarios du corpus qui « échouaient »
  visaient en fait cette décision : ce ne sont pas des défauts.
- **Spécialiser le trait pour son propre type marche dans toutes les formes attaquées** : un
  type simple, un type dans un espace de noms imbriqué (`acme::io::detail::Channel`), un type
  mémoïsant, et un *template de classe* via spécialisation partielle.
- **La garde de type dynamique, là où elle est appliquée, est correcte.**
  `detail::dynamic_type_is_known` traite `void`, les types incomplets, les types polymorphes non
  `final` et les types `final` exactement comme son commentaire l'annonce.

**Ses défauts.**

| Sévérité | Id | Défaut | Correctif |
|---|---|---|---|
| haute | TC-7 / TLS-04 / ADV-11 | aucun primitif de synchronisation sauf `std::atomic` n'est `Sync` : ni `mutex`, ni `shared_mutex`, ni `condition_variable`, ni `latch`, ni `semaphore`, ni `atomic_flag`, ni `once_flag`, ni `atomic_ref` | oui, §5 |
| moyenne | TC-6 | `Sync` implique silencieusement `Send` : un type « vouché » est envoyable sans que son destructeur soit jamais regardé | aucun — décision de conception, §5 |
| moyenne | TC-9 | `is_synchronizable_v<const synchronized_value<T>>` est une **erreur dure**, pas un `false` | oui, `suite-passes` |
| basse | TC-12 | une écriture `volatile` seule perd toute spécialisation : `volatile std::atomic<int>` n'est pas `Sync`, `const volatile std::atomic<int>` l'est | oui, `suite-passes` |
| basse | TC-13 | `is_synchronizable<T&>` n'a aucune spécialisation et vaut faux, alors que `is_sendable<T&>` interroge le référent ; et `assert_synchronizable<const T&>` affiche le message opt-in | (2) oui ; (1) `not-checked` |

Le correctif de TC-9, complet — dans `include/threadsafe/details/synchronized_value.h`, à côté
de la règle `is_synchronizable<synchronized_value<T>>` existante :

```cpp
// La meme reponse, redite pour l'ecriture const. Sans elle, la marche generique
// is_synchronizable<const T> appelle is_complete_type, ce qui instancie
// synchronized_value<T> et declenche son propre static_assert -- une erreur dure
// la ou le trait doit un simple false.
template <class T>
struct is_synchronizable<const synchronized_value<T>> : is_sendable<T> {};
```

Le correctif de TC-12, complet — dans `include/threadsafe/details/synchronizable.h`, en
remplacement des deux règles tableau const existantes :

```cpp
// volatile dit que le compilateur ne peut pas elider un acces ; il n'ajoute
// aucun ecrivain et ne retire aucune synchronisation. Sans ces regles, le
// template primaire repond faux pour un volatile std::atomic<int>, qui est
// exactement aussi synchronisable qu'un atomic ordinaire.
template <class T>
struct is_synchronizable<volatile T> : is_synchronizable<T> {};
template <class T>
struct is_synchronizable<const volatile T> : is_synchronizable<const T> {};

// Les formes tableau cv existent parce que <const T> et <volatile T> ci-dessus
// filtrent un tableau cv-qualifie et seraient sinon a egalite avec la regle
// <T[N]> de synchronizable_base.h.
template <class T, std::size_t N>
struct is_synchronizable<const T[N]> : is_synchronizable<const T> {};
template <class T>
struct is_synchronizable<const T[]> : is_synchronizable<const T> {};
template <class T, std::size_t N>
struct is_synchronizable<volatile T[N]> : is_synchronizable<volatile T> {};
template <class T>
struct is_synchronizable<volatile T[]> : is_synchronizable<volatile T> {};
template <class T, std::size_t N>
struct is_synchronizable<const volatile T[N]>
    : is_synchronizable<const volatile T> {};
template <class T>
struct is_synchronizable<const volatile T[]>
    : is_synchronizable<const volatile T> {};
```

---

### 2.2 `is_synchronizable<const T>` — « lecture sûre depuis plusieurs threads »

**La règle.** Structurelle. La synchronisabilité pleine l'implique ; sinon, la même garde
structurelle que `is_sendable` s'applique et chaque sous-objet atteignable à travers le `const`
doit être lisible en parallèle à son tour. Trois bras distincts dans la boucle des membres,
et c'est cette distinction qui fait la qualité de la règle :

1. un membre `mutable` est écrit à travers une `const&` : il faut le trait **complet** ;
2. un membre **référence** : la constance de la référence n'a rien à voir avec celle du
   référent, donc il faut le trait complet du référent ;
3. un membre valeur ordinaire : le `const` se propage normalement.

Plus la règle des pointeurs : « le const s'arrête au pointeur — le pointé peut être écrit par un
autre alias ».

**Ce qui a résisté.** C'est la marche la mieux attaquée du lot, et elle n'a pas cédé une fois.

- **Les trois bras de la boucle des membres sont tous corrects.** Un `mutable std::mutex`, un
  `mutable std::shared_mutex`, un `mutable std::once_flag`, un `mutable std::atomic_flag`, un
  `mutable int*`, un `mutable std::atomic<int>*` et un `mutable std::string` : tous refusés,
  avec la bonne phrase.
- **Le bras référence ne fait jamais confiance au const d'un membre référence.**
  `struct { int &referent_; }` est refusé ; `struct { void (&callback_)(); }` est accepté, parce
  qu'un type fonction est synchronisable et que du code est réellement partageable.
  `std::atomic<HasReferenceMember>` est refusé pour la même raison.
- **Le bras des membres ordinaires propage le const et s'arrête à chaque indirection.**
  Refusés : `void*`, `const void*`, un pointeur vers un type incomplet, `int (*)[4]`,
  `int *const`. Acceptés : `std::atomic<int> (*)[4]`, `int Host::*`, `void (Host::*)()`,
  `void (*)()`, `void (&)()`.
- **Unions, unions anonymes et bit-fields nommés sont parcourus correctement.** `SafeUnion` et
  `AnonymousUnion` acceptés ; `MixedUnion` et `AnonymousUnionBad` (qui cache un `int*`) refusés ;
  `MutableUnion` refusé ; `struct MixedBitfield { int flag_:3; int :5; int other_:2; }` accepté.
- **Bases virtuelles, bases répétées et polymorphisme tiennent.** Un diamant virtuel sur une
  `Root` propre est accepté ; le même diamant sur une `Root` qui détient un `int*` est refusé
  **par les deux bras virtuels** ; une base non virtuelle répétée est acceptée ; une classe
  polymorphe avec un membre `mutable` est refusée.
- **`[[no_unique_address]]`, bases vides et lambdas sans capture sont acceptés** (ils sont
  réellement vides), tandis que **toute** lambda qui détient une capture — par valeur, par
  pointeur, ou lambda générique — est refusée comme état non réflexif.
- **Les écritures cv et tableau s'accordent entre elles partout.** `const T[N]`, `const T[]`,
  `const T[2][3]`, `const volatile T[N]` et les tableaux const multidimensionnels donnent tous
  la même réponse que `const T`, et `const std::atomic<int>[4]` est accepté. Un contrôle sur 50
  écritures vérifie que `is_synchronizable_type(^^T)` et `is_synchronizable_v<T>` s'accordent.
- **`mutable` est attrapé exactement là où `const_cast` ne l'est pas** :
  `is_synchronizable<const MutableCache>` est faux pour
  `mutable int cache; mutable bool computed;`, avec l'explication correcte. La règle est juste ;
  c'est l'écriture alternative qui échappe (§1.2).
- **La règle « const derrière une indirection » est appliquée avec constance**, et pas seulement
  documentée : je n'ai trouvé aucune écriture de « pointeur const vers const » qui laisse passer
  un référent mutable.
- **La règle du membre `mutable` tient à travers la liste blanche** :
  `is_synchronizable<const T>` est faux pour `std::map<int, MutCache>`, `std::map<MutCache, int>`,
  `std::array<MutCache,3>` et `std::variant<int, MutCache>`.

**Ses défauts.**

| Sévérité | Id | Défaut | Correctif |
|---|---|---|---|
| critique | ADV-01 | membres `static` invisibles (§1.1) | oui, vérifié |
| critique | ADV-02 | `const_cast` dans une méthode `const` (§1.2) | **aucun possible** (§7) |
| critique | TC-1 | blanchiment cv sur les enveloppes standard (§1.3) | oui, vérifié |
| haute | ADV-03 | la garde de type dynamique manque sur `const shared_ptr`, `const weak_ptr`, `const reference_wrapper` (§4) | oui, vérifié |
| haute | TC-7 | `struct GuardedCounter { mutable std::mutex gate_; int value_; }` — le type gardé par mutex canonique — échoue, parce qu'un membre `mutable` exige le trait complet et que `std::mutex` n'est pas `Sync` (§5) | oui, `suite-passes` |
| haute | TC-2 | tout type récursif est une **erreur dure** (§6) | oui, complété et vérifié par moi |
| moyenne | TC-10 | `copy_on_write<T>` — le type « partage en lecture seule » de la bibliothèque — échoue à la question du partage en lecture seule | oui, `suite-passes` |
| moyenne | TC-11 | un `struct PaddingOnly { int : 32; };` est refusé, en accusant « a closure type with captures » | oui, `suite-passes` |
| basse | TC-14 | `int tail_[0]` : `is_sendable` refuse, `is_lifetime_aware` accepte — les deux marches structurelles se contredisent | aucun (cause hors bibliothèque) |
| basse | TC-15 | au-delà de ~120 niveaux d'imbrication, `-fconstexpr-depth` explose et la première erreur nomme `allocator.h` | aucun — rien n'est faux dans la bibliothèque |

Le correctif de TC-10, complet — dans `include/threadsafe/details/copy_on_write.h`, entre les
spécialisations `is_sendable` et `is_lifetime_aware` :

```cpp
// Un copy_on_write const ne distribue rien d'autre qu'un const T& : as_mutable
// n'est pas const, et le compteur de references que touche un lecteur est
// atomique. Le lire depuis plusieurs threads est exactement aussi sur que lire
// le T, ce qui est la raison d'etre du type -- et ce que la marche structurelle
// ne peut pas voir, parce que le constructeur variadique bloque le defaut.
template <class T>
struct is_synchronizable<const copy_on_write<T>> : is_synchronizable<const T> {};
```

Attention : le rapport [02](./02-robustesse-des-helpers.md) rapporte qu'un correctif *voisin* —
router `is_synchronizable<const copy_on_write<T>>` vers `is_synchronizable<const T>` afin que
`synchronized_value<copy_on_write<T>>` choisisse un `shared_mutex` — a été **mesuré 9× plus
lent** (344 ns contre 40 ns par lecture) et rejeté. Le trait doit être corrigé ; c'est le choix
de mutex qu'il déclenche en aval qui est le problème. Voir [07](./07-performance-execution.md).

Le correctif de TC-11, complet — dans `include/threadsafe/details/utils.h`, en remplacement de
`has_unreflectable_state` :

```cpp
// Un type de fermeture avec captures : la reflexion ne liste aucun membre, et
// pourtant l'objet n'est pas vide. L'identifiant est ce qui le distingue d'une
// classe nommee dont les seuls membres sont des bit-fields sans nom -- du
// remplissage, qui ne detient aucune valeur et ne cache rien.
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return !std::meta::has_identifier(type)
        && !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}
```

---

### 2.3 `is_lifetime_aware<T>` — « possède, ou maintient en vie, ce qu'il atteint »

**La règle.** Structurelle et transitive. Les références et les pointeurs bruts sont faux
d'office (sauf les pointeurs de fonction, carve-out explicite) ; `std::reference_wrapper<T>` est
faux inconditionnellement ; tout `std::ranges::borrowed_range` est refusé ; `shared_ptr<T>` et
`weak_ptr<T>` demandent `is_lifetime_aware_v<T>` **et** `dynamic_type_is_known<T>` ;
`unique_ptr<T,D>` demande la même chose et vérifie **aussi** le deleter `D`.

**Ce qui a résisté.** C'est le trait dont la règle centrale — `borrowed_range` — est la plus
tranchante de la bibliothèque, et elle a tenu partout.

- **Le classique « dangler de vue » est attrapé, alors même que la règle générale ne peut pas
  le voir.** `filter_view` sur `ref_view` n'est *pas* un `std::ranges::borrowed_range`, donc le
  test `borrowed_range` ne se déclenche jamais — et la marche structurelle l'attrape quand même,
  parce que `ref_view` détient un pointeur brut.
- **Les `borrowed_range` qui ne possèdent réellement rien sont tous correctement refusés** :
  `std::span<int>`, `std::string_view`, `std::ranges::subrange<int*>`,
  `std::ranges::ref_view<std::vector<int>>`.
- **`std::initializer_list<int>` est correctement refusé** — il emprunte son tableau de support.
- **`std::coroutine_handle<>` et `std::coroutine_handle<Promise>` sont correctement refusés** :
  un handle de coroutine est un handle non possédant vers une frame que quelqu'un d'autre
  détruit.
- **Tous les itérateurs de conteneur sont correctement refusés** :
  `std::vector<int>::iterator` et `::const_iterator`, `std::list<int>::iterator`,
  `std::map<int,int>::iterator`, `std::deque<int>::iterator`, `std::reverse_iterator`,
  `std::move_iterator`, `std::counted_iterator`, `std::back_insert_iterator`,
  `std::istream_iterator`.
- **`std::stop_callback` est correctement refusé par les trois traits** bien que `vocabulary.h`
  ne le mentionne jamais, et l'inquiétude implicite du corpus est doublement sans objet : il est
  refusé structurellement, **et** il n'est même pas `move_constructible`.
- **`std::optional<int&>` (C++26) est correctement refusé par les trois traits.** GCC 16 accepte
  le type, donc c'était un vrai test et pas un test vide.
- **Les unions ne peuvent pas faire passer un pointeur en fraude** : une union anonyme cachant
  un `int*`, une union nommée, et une union en forme d'`optional` avec constructeur et
  destructeur manuels sont toutes refusées, tandis qu'une union anonyme de membres purement
  possédants est acceptée.
- **Les types de fermeture sont traités correctement** : pour une lambda capturant un vrai
  local par référence, `nonstatic_data_members_of` retourne zéro membre,
  `has_unreflectable_state` se déclenche, et `is_lifetime_aware` répond faux. Le
  contre-exemple apparent d'un scénario du corpus était un faux positif de son propre fichier
  (la variable « capturée » était à portée d'espace de noms, donc rien n'était capturé).
- **`std::vector<bool>` répond correctement sur les trois traits** malgré sa représentation par
  proxy : la liste blanche lit `bool`, pas le `_Bit_type*` réellement stocké.
- **La revendication centrale de la liste blanche a survécu à une attaque directe.** Deux
  scénarios font passer de l'état par chaque emplacement de politique (allocateur, comparateur,
  hacheur, prédicat d'égalité), et la règle attrape tout, parce qu'elle lit **tous** les
  arguments de template qui sont des types.

**Ses défauts.**

| Sévérité | Id | Défaut | Correctif |
|---|---|---|---|
| critique | TLS-01 | le deleter type-effacé d'un `shared_ptr` n'est jamais vérifié → use-after-free ASan réel (§4) | **aucun possible** |
| haute | TLS-07 | un `shared_ptr` issu du constructeur d'aliasing est « lifetime aware » alors qu'il ne possède rien (§4) | **aucun possible** |
| haute | TLS-06 | `std::thread`, `std::jthread`, `std::future`, `std::promise`, `std::filesystem::path` ne sont pas `lifetime_aware` (§5) | oui, `suite-passes` |
| haute | TLS-03 | les rejets répondus par spécialisation sont expliqués en remarchant le template primaire — le message nomme un membre interne de libstdc++ et conseille une spécialisation qui serait **non sûre** | oui, `suite-passes` — détail dans [04](./04-diagnostics.md) |
| moyenne | TLS-09 | une référence de fonction n'est pas `lifetime_aware` alors qu'un pointeur vers la même fonction l'est | oui, `suite-passes` |
| moyenne | TLS-10 | la règle `borrowed_range` en bloc rejette `std::views::iota`, qui n'emprunte rien (§5) | oui, `suite-passes` |
| moyenne | ADV-07 / TLS-02 | tout type récursif possédant est une erreur dure (§6) | oui, complété et vérifié par moi |
| basse | TLS-11 | `filter_view` sur `owning_view` répond oui pour un `vector` et non pour une `list` — la réponse dépend d'un détail d'implémentation de libstdc++ (`_CachedPosition`) | aucun sans deviner |

Le correctif de TLS-09, complet. Dans `include/threadsafe/details/lifetime_aware.h`, insérer
immédiatement après la règle `is_lifetime_aware<F*>` existante :

```cpp
// Une reference de fonction designe la meme fonction qu'un pointeur de fonction,
// et une fonction n'est jamais detruite : rien n'est emprunte. Sans cette regle,
// la regle generique T& ci-dessus repondrait non pour `void (&)()` alors qu'elle
// repond oui pour `void (*)()`.
template <class F>
    requires std::is_function_v<F>
struct is_lifetime_aware<F&> : std::true_type {};
```

et dans `detail::diagnose_default_is_lifetime_aware`, remplacer

```cpp
    if (is_reference_type(type)
        || (is_pointer_type(type) && !is_function_type(remove_pointer(type))))
```

par

```cpp
    if ((is_reference_type(type) && !is_function_type(remove_reference(type)))
        || (is_pointer_type(type) && !is_function_type(remove_pointer(type))))
```

Dans `include/threadsafe/details/smart_pointers.h`, insérer immédiatement après
`is_sendable<std::reference_wrapper<T>>` :

```cpp
// lifetime_aware.h repond non pour tout std::reference_wrapper, parce qu'une
// reference emprunte. Une reference vers une *fonction* n'emprunte rien : une
// fonction a une duree de stockage statique et n'est jamais detruite, ce qui est
// deja la raison pour laquelle is_lifetime_aware<F*> est vrai pour un pointeur
// de fonction. Meme referent, meme reponse.
template <class F>
    requires std::is_function_v<F>
struct is_lifetime_aware<std::reference_wrapper<F>> : std::true_type {};
```

---

### 2.4 `is_sendable<T>` — « peut traverser vers un autre thread » (≈ Rust `Send`)

**La règle.** Structurelle, et c'est la seule règle pour les callables : « donner un callable à
un autre thread, *c'est* l'envoyer ». `is_sendable<T&>` = `is_sendable<T&&>` = `is_sendable<T*>`
= `is_synchronizable<T>`. La marche court-circuite sur
`is_synchronizable_type(type) || is_scalar_type(type)`, puis exige un type complet, puis
`has_only_default_copy_move_destroy`, puis `!has_unreflectable_state`, puis descend dans les
bases et les membres non statiques.

**Ce qui a résisté.** C'est la marche la plus attaquée, avec le plus grand nombre de tentatives
de contrebande, et aucune n'est passée.

- **Dix formes de contrebande de disposition mémoire ont toutes été correctement refusées**, et
  pour `is_sendable` **et** pour `is_synchronizable<const T>` : un membre `[[no_unique_address]]`
  détenant un emprunt ; une union anonyme avec une alternative `int*` ; une structure imbriquée
  sans nom ; une base virtuelle apportant l'emprunt ; une base répétée ; un bit-field voisinant
  un pointeur ; et les variantes profondes correspondantes.
- **`has_unreflectable_state` est la défense porteuse, et elle tient.** Vérification directe sur
  GCC 16 : une fermeture de taille 4 qui a capturé un `int` par valeur rapporte
  `nsdm = 0` ; une de taille 8 qui a capturé par référence rapporte `nsdm = 0` aussi. Sans le
  contrôle « non vide mais la réflexion ne voit aucun membre », `[&local]{}` serait bénie
  envoyable. C'est la seule chose entre la bibliothèque et cette bénédiction.
- **La marche termine à travers les pointeurs bruts.** `struct Node { int value; Node* next; }`
  répond faux proprement, sans explosion de récursion, parce que `is_sendable<T*>` court-circuite
  vers `is_synchronizable<T>` sans descendre. (La récursion *possédante* est une autre histoire :
  §6.)
- **L'opt-out se propage réellement là où il est lu** : un `is_sendable<T> : std::false_type` est
  respecté à travers les agrégats, à travers `std::vector<T>`, et à travers `std::unique_ptr<T>`.
  Seul `std::shared_ptr` l'ignore (TC-5, §4).
- **Le niveau d'imbrication 60 passe dans les deux polarités**, et à travers `assert_sendable`.
- **Les deux resserrements possibles de `may_hijack_copy_move` gardent tous les vrais
  détourneurs refusés.** `GreedyForward`, `GreedyLvalue` et `VariadicForwarding` — dont un
  scénario prouve indépendamment, via `!is_trivially_constructible_v<T, T&>`, que le template est
  réellement sélectionné — restent refusés sous les deux variantes.
- **`is_sendable<T&&>` dérivant de `is_synchronizable<T>` est défendable.** Une référence rvalue
  C++ n'est pas exclusive comme le `&mut` de Rust — l'expéditeur garde un nom pour l'objet —
  donc exiger `Sync` du référent est le choix conservateur correct. Aucun faux rejet
  atteignable par du code ordinaire n'a pu être construit. *À noter cependant, du rapport
  [03](./03-couverture-de-tests.md) : la règle `T&&` est **entièrement non testée**, 0 mutant
  sur 4 tué, y compris `is_sendable<T&&> -> std::true_type`.*
- **Du côté du lanceur** : `std::function<void()>`, `std::move_only_function<void()>` et le
  résultat de `std::bind` sont rejetés par `launch_task` **et** par `launch_scoped_task` ; une
  lambda capturant un pointeur brut **par valeur** est rejetée ; `std::span`, `std::string_view`,
  `const char*` et les pointeurs bruts sont tous rejetés par `launch_task` ; un foncteur sans
  état avec un constructeur de copie **écrit à la main** est rejeté.
- **La suite de tests de la bibliothèque est honnête.** 11/11 passent contre les en-têtes
  intacts, et elles passent encore contre chacun des correctifs indépendamment et combinés :
  **rien dans la suite ne figeait le comportement buggé**. C'est précisément pourquoi ADV-01,
  ADV-03 et TC-1 peuvent être adoptés sans toucher aux tests.

**Ses défauts.**

| Sévérité | Id | Défaut | Correctif |
|---|---|---|---|
| critique | ADV-01 | membres `static` invisibles (§1.1) | oui, vérifié |
| critique | ADV-02 | `const_cast` dans une méthode `const` (§1.2) | **aucun possible** (§7) |
| critique | TC-1 | blanchiment cv (§1.3) | oui, vérifié |
| haute | ADV-03 | la garde de type dynamique est posée sur `unique_ptr` seul (§4) | oui, vérifié |
| haute | ADV-04 | un handle affine au thread (un agrégat d'un `size_t` indexant un `thread_local`) passe `launch_task` et le programme **segfaute** | **aucun possible** (§7) |
| haute | ADV-05 | un type incomplet répond faux, la réponse est gelée pour toute la TU, et deux TU divergent en silence (§3) | aucun (§3) |
| haute | TC-3 | `is_defaulted` bascule sur un `= default` hors ligne : deux TU divergent (§3) | oui, vérifié par moi |
| haute | TC-5 | `std::shared_ptr<T>` ignore un opt-out `is_sendable<T>` explicite (§4) | oui, `suite-passes` |
| haute | TLS-05 / TLS-06 | le vocabulaire de valeurs standard n'est pas envoyable : `chrono`, `complex`, `bitset`, `expected`, `stack`, `queue`, `thread`, `future`, `path` (§5) | oui, `suite-passes` |
| haute | TC-2 | les types récursifs sont une erreur dure (§6) | oui, complété et vérifié par moi |
| moyenne | ADV-08 | **toute** lambda capturante est refusée, y compris une capture par valeur ou possédante — ce qui bloque l'idiome vitrine `launch_task([data]{ use(data); })` (§5) | **aucun possible** |
| moyenne | TC-8 | `may_hijack_copy_move` rejette tout template de constructeur ou d'`operator=`, quelle que soit son arité (§5) | `suite-regresses` — arbitrage |
| moyenne | TLS-08 | `std::span<T>` n'est pas envoyable même quand `T` est `Sync`, alors que `T*` l'est (§5) | oui, `suite-passes` |
| moyenne | ADV-06 | un `false` périmé ou dû à un type incomplet est rapporté comme « is_sendable is specialized to false for it » — la spécialisation n'existe pas | `not-checked` — voir [04](./04-diagnostics.md) |
| moyenne | ADV-09 | le conseil de `launch_task` est une impasse : il dit d'utiliser `std::ref`, et `std::ref` ne peut jamais satisfaire `launchable_task` | `not-checked` — voir [04](./04-diagnostics.md) |
| basse | TC-14 | tableau de longueur nulle : désaccord entre les deux marches structurelles | aucun |

Le correctif de TC-5, complet — dans `include/threadsafe/details/smart_pointers.h`, en
remplacement de la règle `is_sendable<std::shared_ptr<T>>` :

```cpp
// Partager le pointe demande la synchronisabilite, et le thread receveur peut
// laisser tomber la derniere reference -- il execute donc ~T la-bas. C'est un
// envoi, et c'est l'obligation que std::unique_ptr enonce deja.
template <class T>
struct is_sendable<std::shared_ptr<T>>
    : std::bool_constant<
          is_synchronizable_v<std::remove_cv_t<std::remove_all_extents_t<T>>>
          && is_sendable_v<std::remove_cv_t<std::remove_all_extents_t<T>>>> {};
```

Le conjoint supplémentaire est gratuit pour tout type `Sync` ordinaire : la marche `sendable`
court-circuite sur `is_synchronizable_type`, donc `is_sendable_v` est déjà vrai pour eux. Il ne
mord que lorsque l'utilisateur a écrit l'opt-out explicitement. **Confirmé par moi** : sur les
en-têtes intacts,
`static_assert(!is_sendable_v<std::shared_ptr<thread_affine_context>>)` échoue alors que la
même assertion sur `std::unique_ptr` passe.

---

## 3. La famille ODR : la réponse des traits n'est pas stable entre unités de traduction

C'est une propriété de sûreté **distincte** de toute règle individuelle, et elle mérite d'être
nommée à part : les quatre traits promettent une réponse *sur un type*, et ce qu'ils livrent
est une réponse *sur un type tel qu'il est visible depuis un point d'une unité de traduction*.
Trois mécanismes indépendants produisent la divergence.

| Id | Mécanisme | Symptôme | Statut du correctif |
|---|---|---|---|
| TC-3 | `std::meta::is_defaulted` bascule quand un `= default` **hors ligne** est vu | la TU qui contient les définitions répond `true`, toutes les autres `false` | **corrigé**, vérifié par moi, 11/11 TU |
| ADV-05 | un type **incomplet** répond `false`, et la réponse est mise en cache par l'instanciation | deux TU contradictoires ; le comportement dépend de l'ordre des `.o` sur la ligne de lien | aucun — inhérent |
| TC-4 | `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` écrit dans une TU et pas dans une autre | `sizeof(synchronized_value<Cache>)` vaut 72 d'un côté et 208 de l'autre ; le programme **lie sans diagnostic** et avorte | `suite-regresses` — arbitrage de conception |

### 3.1 TC-3 — l'idiome pimpl fait diverger `is_sendable`

`has_only_default_copy_move_destroy` interroge `std::meta::is_defaulted`, qui est faux pour
`~Widget();` dans un en-tête et vrai une fois que le `.cpp` a écrit
`Widget::~Widget() = default;`. C'est très exactement la forme de déclaration de l'idiome pimpl.

La forme intra-TU, complète, qui n'exige aucun script de build — **je l'ai recompilée
aujourd'hui** :

```cpp
#include <threadsafe/threadsafe.h>

namespace {
// la forme pimpl : declaree ici, defaultee hors ligne dans un .cpp
struct Widget {
    int value_;
    Widget();
    Widget(const Widget &);
    ~Widget();
};
}

constexpr bool asked_before = threadsafe::is_sendable_v<Widget>;

namespace {
Widget::Widget() = default;
Widget::Widget(const Widget &) = default;
Widget::~Widget() = default;
}

static_assert(asked_before == false);
static_assert(threadsafe::is_sendable_v<Widget> == false);
// la marche reflexive ne doit pas changer d'avis une fois les definitions vues
static_assert(threadsafe::detail::default_is_sendable(^^Widget) == false);
static_assert(threadsafe::detail::default_is_const_synchronizable(^^Widget) == false);

int main() {}
```

**Observé sur les en-têtes intacts** (par moi) :

```
work/odr_defaults.cpp:13:65: error: static assertion failed
work/odr_defaults.cpp:14:77: error: static assertion failed
```

— la variable en cache dit `false`, une requête réflexive fraîche dit `true`. En deux TU liées
en un seul programme, cela s'imprime ainsi (mesuré par l'agent) :

```
main.cpp   = 0
widget.cpp = 1
```

Le correctif, complet — dans `include/threadsafe/details/utils.h`, en remplacement du corps
entier de `has_only_default_copy_move_destroy` :

```cpp
inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (may_hijack_copy_move(member))
            return false;

        if (!is_copy_move_destroy_member(member))
            continue;

        // is_defaulted bascule quand un `= default` hors ligne est vu, donc une
        // unite de traduction header-only et celle qui detient les definitions
        // repondraient differemment pour le meme type -- deux definitions de
        // chaque template indexe sur le trait. is_user_provided est fixe des la
        // premiere declaration, donc toutes les unites lisent la meme reponse.
        if (std::meta::is_user_provided(member))
            return false;
    }
    return true;
}
```

`is_user_provided` absorbe aussi l'échappatoire `is_deleted` de l'ancienne version : un
`= delete` sur la première déclaration n'est pas *user-provided*. **Vérifié par moi** : le
fichier ci-dessus compile proprement sous ce correctif, et les 11 TU restent vertes.

### 3.2 ADV-05 — un type incomplet gèle un `false`, et l'ordre du lien décide

Les quatre fichiers, complets :

```cpp
// ---------- widget.h ----------
#pragma once
#include <threadsafe/threadsafe.h>
#include <memory>
#include <thread>

struct Implementation;                       // le pimpl, complete par certaines TU

struct Widget {
    std::unique_ptr<Implementation> impl;
};

// La barriere qu'un utilisateur ecrit : ne confier le widget a un worker que
// s'il a le droit de traverser.
inline void dispatch(Widget& widget, void (*body)(Widget&)) {
    if constexpr (threadsafe::is_sendable_v<Widget>) {
        std::jthread worker{body, std::ref(widget)};
    } else {
        body(widget);
    }
}
```

```cpp
// ---------- implementation.h ----------
#pragma once
struct Implementation { int value; };
```

```cpp
// ---------- tu_a.cpp ----------
#include "implementation.h"     // Implementation est COMPLETE avant la question
#include "widget.h"
#include <cstdio>

void run_b(Widget&);

static_assert(threadsafe::is_sendable_v<Widget>,
              "TU A voit une Implementation complete : Widget peut traverser");

void run_a() {
    Widget widget{std::make_unique<Implementation>(1)};
    std::printf("TU A compilee avec is_sendable_v<Widget> == %d\n",
                int(threadsafe::is_sendable_v<Widget>));
    dispatch(widget, [](Widget&) {});   // emet la copie de dispatch de TU A
    run_b(widget);
}
```

```cpp
// ---------- tu_b.cpp ----------
#include "widget.h"             // Implementation reste INCOMPLETE ici
#include <cstdio>
#include <thread>

static_assert(!threadsafe::is_sendable_v<Widget>,
              "TU B ne voit que la declaration anticipee : Widget ne doit pas traverser");

static std::thread::id owning_thread_id;

void run_b(Widget& widget) {
    owning_thread_id = std::this_thread::get_id();
    std::printf("TU B compilee avec is_sendable_v<Widget> == %d\n",
                int(threadsafe::is_sendable_v<Widget>));

    // Ecrit sur la foi du static_assert ci-dessus : dispatch est cense executer
    // le corps en ligne, sur ce thread meme.
    dispatch(widget, [](Widget&) {
        const bool inline_as_promised =
            std::this_thread::get_id() == owning_thread_id;
        std::printf("le corps de TU B s'est execute %s\n",
                    inline_as_promised
                        ? "en ligne, comme son static_assert le promettait"
                        : "SUR UN AUTRE THREAD -- le static_assert de TU B a ete jete");
    });
}
```

```cpp
// ---------- main.cpp ----------
void run_a();
int main() { run_a(); }
```

**Observé.** Les deux TU compilent proprement — chacune voit son `static_assert` tenir, et elles
se contredisent. Compilé en `-O0`, lié de deux façons :

```
$ link a.o b.o main.o && ./prog
TU A compilee avec is_sendable_v<Widget> == 1
TU B compilee avec is_sendable_v<Widget> == 0
le corps de TU B s'est execute SUR UN AUTRE THREAD -- le static_assert de TU B a ete jete

$ link b.o a.o main.o && ./prog
TU A compilee avec is_sendable_v<Widget> == 1
TU B compilee avec is_sendable_v<Widget> == 0
le corps de TU B s'est execute en ligne, comme son static_assert le promettait
```

Seul l'ordre de deux fichiers `.o` sur la ligne de lien a changé. Les deux objets définissent le
même symbole faible avec des corps différents :

```
a.o: 0000000000000ac8 (__TEXT,__text) weak external __Z8dispatchR6WidgetPFvS0_E
b.o: 0000000000000098 (__TEXT,__text) weak external __Z8dispatchR6WidgetPFvS0_E
```

Impasse honnête que l'agent a nommée : en `-O2`, GCC inline `dispatch` dans les deux appelants,
chacun garde sa réponse, et la divergence ne se manifeste plus. Le danger reste (c'est IFNDR
dans les deux cas), mais il n'est observable que quand la fonction est réellement émise hors
ligne.

La moitié intra-TU du même défaut, confirmée séparément :

```cpp
#include <threadsafe/threadsafe.h>

struct Later;
static_assert(!threadsafe::is_sendable_v<Later>);   // interroge alors qu'incomplet
struct Later { int a; int b; };                     // desormais complet
static_assert(!threadsafe::is_sendable_v<Later>);   // TOUJOURS faux

int main() {}
```

Un agrégat de deux `int` n'est pas envoyable, pour le reste de l'unité de traduction, parce que
quelqu'un a posé la question trop tôt.

**Correctif : aucun, et la raison est précise.** La mise en cache n'est pas un bug de la
bibliothèque, c'est la manière dont C++ instancie les templates de classe, et aucune
bibliothèque ne peut s'en extraire. La divergence inter-TU est de même inhérente au modèle de
compilation séparée : TU B ne *peut* pas voir la définition d'`Implementation`. Le seul levier
que la bibliothèque possède est le choix de répondre `false` pour un type incomplet plutôt que
d'émettre une erreur — et transformer cela en erreur n'est pas viable :
`default_is_sendable` attrape la `meta::exception` précisément pour que `is_sendable_v` puisse
servir dans un `if constexpr` et dans le concept `sendable`, et une erreur dure rendrait toute
utilisation SFINAE ou conceptuelle du trait mal formée — et casserait le contournement pimpl
que la bibliothèque recommande elle-même dans ses propres diagnostics.

**Ce qui reste à faire, et c'est de la documentation** : écrire que ces traits répondent sur un
type *tel qu'il est visible dans l'unité de traduction courante*, que la réponse est fixée à la
première requête, et que brancher sur un trait dans un en-tête est un risque ODR à moins que
toutes les TU voient les mêmes définitions. Le texte est en §7.

### 3.3 TC-4 — deux dispositions mémoire pour une classe, sans le moindre diagnostic

`synchronized_value<T>::mutex` est choisi par `if constexpr (is_synchronizable_v<const T>)`. La
macro de vouch est documentée — `tests/test_deferred_specialization.cpp` la présente ainsi —
comme quelque chose que l'utilisateur écrit dans **sa** TU. Un vouch présent dans une TU et
absent d'une autre donne à la même classe deux dispositions mémoire.

Le fichier, complet, à compiler **deux fois** puis à lier :

```cpp
// Compiler ce fichier DEUX FOIS et lier les deux objets :
//   g++-16 -std=c++26 -freflection -I<include> -DTHREADSAFE_TU_WITH_VOUCH \
//           -c -o with_vouch.o odr.cpp
//   g++-16 -std=c++26 -freflection -I<include> -c -o without_vouch.o odr.cpp
//   g++-16 with_vouch.o without_vouch.o -o odrprog -pthread && ./odrprog

#include <threadsafe/threadsafe.h>

#include <cstdio>

struct Cache {
    mutable int hits;
};

#ifdef THREADSAFE_TU_WITH_VOUCH
// Ecrit ici, dans cette unite de traduction seulement -- exactement le motif que
// tests/test_deferred_specialization.cpp presente comme supporte.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Cache);
#endif

using SharedCache = threadsafe::synchronized_value<Cache>;

void read_from_vouching_tu(SharedCache &shared_cache);
std::size_t size_seen_by_vouching_tu();

#ifdef THREADSAFE_TU_WITH_VOUCH

std::size_t size_seen_by_vouching_tu() { return sizeof(SharedCache); }

void read_from_vouching_tu(SharedCache &shared_cache) {
    auto guard = shared_cache.lock_shared();
    std::printf("  la TU qui vouche lit hits = %d\n", guard->hits);
}

#else

struct Sandwich {
    SharedCache shared_cache;
    unsigned long canary;
};

int main() {
    std::printf("sizeof(synchronized_value<Cache>) dans la TU simple   = %zu\n",
                sizeof(SharedCache));
    std::printf("sizeof(synchronized_value<Cache>) dans la TU qui vouche = %zu\n",
                size_seen_by_vouching_tu());

    Sandwich sandwich{SharedCache{Cache{7}}, 0xC0FFEEUL};
    std::printf("canari avant = 0x%lX\n", sandwich.canary);
    read_from_vouching_tu(sandwich.shared_cache);
    std::printf("canari apres = 0x%lX\n", sandwich.canary);
    if (sandwich.canary != 0xC0FFEEUL)
        std::printf("CANARI ECRASE : les deux TU ne s'accordent pas sur la disposition\n");
    return 0;
}

#endif
```

**Observé** (revérifié personnellement par le lead) — les deux objets compilent sans un seul
avertissement, le lien réussit :

```
sizeof(synchronized_value<Cache>) dans la TU simple     = 72
sizeof(synchronized_value<Cache>) dans la TU qui vouche = 208
canari avant = 0xC0FFEE
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/shared_mutex:246:
  void std::__shared_mutex_pthread::lock_shared(): Assertion '__ret == 0' failed.
[exit 134 / SIGABRT]
```

**Correctif : `suite-regresses` — c'est un arbitrage, pas un patch prêt à poser.** Rendre la
disposition inconditionnelle fonctionne :

```cpp
    // Fixe, quelle que soit la reponse des traits. Deriver le mutex -- et donc
    // la taille de cette classe -- de is_synchronizable_v<const T> faisait
    // dependre la disposition memoire du fait que l'unite de traduction courante
    // avait vu ou non un THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE.
    using mutex = std::shared_mutex;
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;
```

et contraindre la lecture partagée pour qu'une TU qui n'a pas vu le vouch échoue à compiler
plutôt que de prendre en silence un verrou auquel elle n'a pas droit :

```cpp
    [[nodiscard]] const_guard lock_shared() const
        requires is_synchronizable_v<const T>
    {
        return const_guard{mutex_, value_};
    }
```

Le programme à deux TU imprime alors 208/208, lit correctement, et le canari survit. **Mais**
cela casse les lignes 120, 122, 125, 131 et 134 de `tests/test_synchronized_value.cpp`, qui
documentent la fonctionnalité de sélection de mutex que ce changement supprime. Deux issues, et
c'est l'auteur qui tranche : soit la disposition cesse de dépendre du trait et ces cinq
assertions sont réécrites, soit **la macro de vouch est déclarée header-only** — c'est-à-dire
qu'elle doit être écrite dans un en-tête inclus par toutes les TU, et cela doit être écrit dans
`CLAUDE.md`. Corriger TC-3 ferme l'autre route vers le même danger, mais pas celle-ci.

Pour une conférence, je recommanderais la seconde : elle coûte une phrase de documentation au
lieu d'une fonctionnalité, et elle est exacte.

---

## 4. Les trous d'indirection : qui porte quelle garde

La bibliothèque connaît trois obligations distinctes qu'une indirection peut devoir honorer :

1. **la garde de type dynamique** — une réponse structurelle sur un pointé polymorphe non
   `final` ne prouve rien sur l'objet réellement là ; `detail::dynamic_type_is_known` la répond ;
2. **l'obligation de destruction** — le thread receveur peut détruire le pointé, donc il faut
   `Send` et pas seulement `Sync` ;
3. **la vérification du propriétaire réel** — le deleter, ou le bloc de contrôle.

Aucune de ces trois n'est appliquée uniformément. La table dit qui porte quoi, aujourd'hui,
sur les en-têtes intacts :

| Indirection | `is_sendable` demande | garde de type dynamique | honore un opt-out `is_sendable<T>` | vérifie le propriétaire réel |
|---|---|---|---|---|
| `T*` | `Sync(T)` | **non** — ADV-03 | s.o. | s.o. |
| `T&` / `T&&` | `Sync(T)` | **non** — ADV-03 | s.o. | s.o. |
| `std::reference_wrapper<T>` | `Sync(T)` | **non** — ADV-03 | s.o. | s.o. |
| `std::unique_ptr<T,D>` | `Send(T) && Send(D)` | **oui** | oui | **oui** (`D` est vérifié) |
| `std::shared_ptr<T>` | `Sync(T)` | **non** — ADV-03 | **non** — TC-5 | **non** — TLS-01 |
| `std::weak_ptr<T>` | `Sync(T)` | **non** — ADV-03 | **non** | **non** |
| `threadsafe::copy_on_write<T>` | `Send(T) && ConstSync(T)` | s.o. | oui | oui |

| Indirection | `is_lifetime_aware` répond | garde de type dynamique | vérifie le propriétaire réel |
|---|---|---|---|
| `T*` (non fonction) | `false` | s.o. | s.o. |
| `T&` (non fonction) | `false` | s.o. | s.o. |
| `F*` / `F` fonction | `true` | s.o. | s.o. |
| `F&` fonction | **`false`** — TLS-09, incohérent avec `F*` | s.o. | s.o. |
| `std::reference_wrapper<T>` | `false` (inconditionnel) | s.o. | s.o. |
| `std::unique_ptr<T,D>` | `LA(T) && LA(D) && dyn` | **oui** | **oui** |
| `std::shared_ptr<T>` | `LA(T) && dyn` | **oui** | **non** — TLS-01, TLS-07 |
| `std::weak_ptr<T>` | `LA(T) && dyn` | **oui** | **non** |

L'asymétrie n'est pas une politique : c'est le même raisonnement écrit une fois et pas la
deuxième. `utils.h` porte même le commentaire qui l'explique, juste au-dessus de
`compute_dynamic_type_is_known`.

### 4.1 ADV-03 — la garde de type dynamique n'est posée que sur `unique_ptr`

Le fichier de reproduction, complet — **j'ai vérifié moi-même que les huit assertions marquées
`HOLE` tiennent aujourd'hui** :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

// Une base que l'auteur a reellement rendue thread-safe : chaque membre est
// atomique. Repondre a la question de la bibliotheque de la maniere documentee
// -- en specialisant le trait -- est exactement ce que CLAUDE.md conseille.
struct Base {
    virtual ~Base() = default;
    virtual void bump() { count_.fetch_add(1, std::memory_order_relaxed); }
    std::atomic<int> count_{0};
};

template <>
struct threadsafe::is_synchronizable<Base> : std::true_type {};

// Une classe derivee ecrite plus tard, par quelqu'un d'autre, qui n'est PAS
// thread-safe.
struct Derived : Base {
    void bump() override { Base::bump(); ++unsynchronized_; }
    long unsynchronized_ = 0;
};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// unique_ptr : la garde se declenche -- correctement refuse.
static_assert(!is_sendable_v<std::unique_ptr<Base>>);
static_assert(!is_synchronizable_v<const std::unique_ptr<Base>>);

// Toutes les autres indirections vers la MEME Base : la garde est absente.
static_assert(is_sendable_v<std::shared_ptr<Base>>,      "shared_ptr HOLE");
static_assert(is_sendable_v<std::weak_ptr<Base>>,        "weak_ptr HOLE");
static_assert(is_sendable_v<Base*>,                      "raw pointer HOLE");
static_assert(is_sendable_v<Base&>,                      "reference HOLE");
static_assert(is_sendable_v<std::reference_wrapper<Base>>, "ref_wrapper HOLE");
static_assert(is_sendable_v<std::vector<Base*>>,         "vector<Base*> HOLE");
static_assert(is_synchronizable_v<const std::shared_ptr<Base>>, "const shared_ptr HOLE");
static_assert(is_synchronizable_v<const std::reference_wrapper<Base>>, "const ref_wrapper HOLE");

// Et par consequent synchronized_value donne un shared_lock aux lecteurs.
static_assert(std::is_same_v<
    threadsafe::synchronized_value<std::shared_ptr<Base>>::mutex,
    std::shared_mutex>, "shared_mutex choisi");

int main() {
    constexpr long per_thread = 200000;
    auto derived = std::make_shared<Derived>();

    threadsafe::synchronized_value<std::shared_ptr<Base>> shared{derived};

    {
        std::vector<std::jthread> workers;
        for (int i = 0; i < 4; ++i)
            workers.emplace_back([&shared] {
                for (long n = 0; n < per_thread; ++n) {
                    auto reader = shared.lock_shared();   // shared_lock
                    reader->get()->bump();
                }
            });
    }

    std::printf("membre atomique : %d (attendu %ld)\n",
                derived->count_.load(), per_thread * 4);
    std::printf("membre derive   : %ld (attendu %ld)\n",
                derived->unsynchronized_, per_thread * 4);
    return derived->unsynchronized_ == per_thread * 4 ? 0 : 1;
}
```

**Observé** avec la vraie bibliothèque (`g++-16 -O2 -pthread`), deux exécutions :

```
membre atomique : 800000 (attendu 800000)
membre derive   : 799996 (attendu 800000)   exit=1
membre atomique : 800000 (attendu 800000)
membre derive   : 799999 (attendu 800000)   exit=1
```

Le membre atomique que l'auteur a protégé est exact ; le membre dérivé dont personne n'a parlé à
la bibliothèque ne l'est pas. TSan sur l'extraction fidèle :

```
WARNING: ThreadSanitizer: data race (pid=83152)
  Write of size 8 at 0x00010d6008c8 by thread T1:
  Previous write of size 8 at 0x00010d6008c8 by thread T2:
SUMMARY: ThreadSanitizer: data race 05_tsan_shared_ptr.cpp:49 in Derived::bump()
```

La victime, ici, est **l'utilisateur consciencieux** : on n'atteint ce trou qu'en faisant la
chose responsable, c'est-à-dire en vouchant pour une base qu'on a réellement rendue thread-safe.

**Correctif** (`suite-passes`, **et vérifié par moi : ferme les 8 trous, les 11 TU restent
vertes, et les réponses positives légitimes ne bougent pas**). Deux éditions.

*(1) `include/threadsafe/details/sendable.h`* — remplacer les trois spécialisations
d'indirection par :

```cpp
// Chaque indirection pose les deux memes questions : le referent est-il
// synchronisable, et le type du referent est-il celui qui est reellement la. La
// seconde est ce a quoi detail::dynamic_type_is_known repond -- une reponse sur
// une base ne prouve rien sur un objet derive qui ajoute de l'etat non
// synchronise.
namespace detail {
template <class T>
constexpr bool referent_is_shareable =
    is_synchronizable_v<std::remove_cv_t<T>>
    && dynamic_type_is_known<std::remove_cv_t<T>>;
}

template <class T>
struct is_sendable<T&> : std::bool_constant<detail::referent_is_shareable<T>> {};
template <class T>
struct is_sendable<T&&> : std::bool_constant<detail::referent_is_shareable<T>> {};

template <class T>
struct is_sendable<T*> : std::bool_constant<detail::referent_is_shareable<T>> {};
```

*(2) `include/threadsafe/details/smart_pointers.h`* — remplacer les six spécialisations
concernées par :

```cpp
template <class T>
struct is_sendable<std::shared_ptr<T>>
    : std::bool_constant<
          detail::referent_is_shareable<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_sendable<std::weak_ptr<T>>
    : std::bool_constant<
          detail::referent_is_shareable<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_sendable<std::reference_wrapper<T>>
    : std::bool_constant<detail::referent_is_shareable<T>> {};

template <class T>
struct is_synchronizable<const std::shared_ptr<T>>
    : std::bool_constant<
          detail::referent_is_shareable<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::weak_ptr<T>>
    : std::bool_constant<
          detail::referent_is_shareable<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::reference_wrapper<T>>
    : std::bool_constant<detail::referent_is_shareable<T>> {};
```

Mon fichier d'acceptation, complet, qui vérifie que rien de légitime ne bouge :

```cpp
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

struct Base {
    virtual ~Base() = default;
    virtual void bump() { count_.fetch_add(1); }
    std::atomic<int> count_{0};
};
template <> struct threadsafe::is_synchronizable<Base> : std::true_type {};

struct FinalSync final { std::atomic<int> n_{0}; };
template <> struct threadsafe::is_synchronizable<FinalSync> : std::true_type {};

struct PlainSync { std::atomic<int> n_{0}; };
template <> struct threadsafe::is_synchronizable<PlainSync> : std::true_type {};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(!is_sendable_v<std::shared_ptr<Base>>);
static_assert(!is_sendable_v<std::weak_ptr<Base>>);
static_assert(!is_sendable_v<Base*>);
static_assert(!is_sendable_v<Base&>);
static_assert(!is_sendable_v<std::reference_wrapper<Base>>);
static_assert(!is_sendable_v<std::vector<Base*>>);
static_assert(!is_synchronizable_v<const std::shared_ptr<Base>>);
static_assert(!is_synchronizable_v<const std::reference_wrapper<Base>>);

// ce qui doit rester vrai
static_assert(is_sendable_v<std::shared_ptr<FinalSync>>);
static_assert(is_sendable_v<std::shared_ptr<PlainSync>>);
static_assert(is_sendable_v<std::atomic<int>*>);
static_assert(is_sendable_v<std::atomic<int>&>);

int main() {}
```

Ce fichier compile proprement sous le correctif. **Coût** : un type polymorphe non `final`
vouché ne peut plus être partagé à travers aucune indirection. L'utilisateur marque la classe
`final`, ou spécialise le trait pour le type dérivé concret. C'est exactement le prix que
`unique_ptr` fait déjà payer aujourd'hui.

### 4.2 TLS-01 et TLS-07 — ce que `shared_ptr` ne peut pas dire

Deux trous réels, avec des traces ASan réelles, et **aucun correctif possible**, pour la même
raison : le bloc de contrôle d'un `shared_ptr` est une propriété d'exécution.
`std::shared_ptr<Cell>` est **un seul type** qu'il ait été construit par `make_shared`, par
`shared_ptr(p)`, par `shared_ptr(p, PoolDeleter{pool})`, ou par le constructeur d'aliasing avec
un `use_count()` de zéro.

TLS-01, le deleter emprunteur, fichier complet :

```cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

struct Pool {
    std::atomic<int> released{0};
    void release(std::atomic<int> *cell) {
        released.fetch_add(1);   // <-- touche le Pool
        delete cell;
    }
};

struct PoolDeleter {
    Pool *pool;                  // emprunte
    void operator()(std::atomic<int> *cell) const { pool->release(cell); }
};

using Cell = std::atomic<int>;

// La bibliotheque verifie bien le deleter d'un unique_ptr :
static_assert(!threadsafe::is_lifetime_aware_v<std::unique_ptr<Cell, PoolDeleter>>,
              "unique_ptr : le deleter emprunteur est vu");
// mais le meme deleter dans un shared_ptr est invisible.
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<Cell>>,
              "shared_ptr : le deleter est type-efface, donc le trait dit oui");
static_assert(threadsafe::is_sendable_v<std::shared_ptr<Cell>>);
static_assert(threadsafe::launchable_task<
                  void (*)(std::shared_ptr<Cell>), std::shared_ptr<Cell>>,
              "et le lanceur l'accepte");

int main() {
    Pool *pool = new Pool();     // tient lieu d'un pool possede par cette portee
    {
        threadsafe::asynchronous_task_launcher launcher;
        std::shared_ptr<Cell> handle(new Cell(1), PoolDeleter{pool});

        launcher.launch_task(
            +[](std::shared_ptr<Cell> owned) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                owned->fetch_add(1);
                // `owned` meurt ici : derniere reference, PoolDeleter s'execute.
            },
            handle);

        handle.reset();          // ce thread lache sa reference

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        delete pool;             // le pool disparait pendant que la tache tourne
        std::puts("pool detruit, le worker tient encore le shared_ptr");
    }                            // le lanceur joint -> le deleter s'execute sur un Pool mort
    std::puts("fini");
}
```

**Observé** — construit directement contre les vrais en-têtes, avec réflexion **et** ASan, donc
il n'y a aucune fidélité d'extraction à vérifier (fait revérifié par le lead) :

```
==82373==ERROR: AddressSanitizer: heap-use-after-free on address 0x6020000000d0
WRITE of size 4 at 0x6020000000d0 thread T1
    #0 std::_Sp_counted_deleter<std::atomic<int>*, PoolDeleter, std::allocator<void>,
       (__gnu_cxx::_Lock_policy)2>::_M_dispose()
    #1 std::thread::_State_impl<...>::_M_run()
freed by thread T0 here:
    #1 main shared_ptr_deleter_uaf.cpp:56        // delete pool;
```

TLS-07, le constructeur d'aliasing, fichier complet :

```cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using Cell = std::atomic<int>;

static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<Cell>>,
              "le trait lit le type statique : un shared_ptr possede son pointe");
static_assert(threadsafe::launchable_task<void (*)(std::shared_ptr<Cell>),
                                          std::shared_ptr<Cell>>,
              "donc le lanceur le laisse traverser");

int main() {
    {
        threadsafe::asynchronous_task_launcher launcher;
        std::vector<Cell> cells(4);

        // [util.smartptr.shared.const] : le constructeur d'aliasing avec un
        // shared_ptr vide produit un shared_ptr sans aucune propriete.
        std::shared_ptr<Cell> borrowed(std::shared_ptr<Cell>{}, cells.data());
        std::fprintf(stderr, "use_count = %ld\n", (long) borrowed.use_count());

        launcher.launch_task(
            +[](std::shared_ptr<Cell> owned) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                owned->fetch_add(1);        // ecrit dans cells[0]
            },
            borrowed);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cells.clear();
        cells.shrink_to_fit();              // le stockage a disparu
        std::fputs("cells liberees, le worker tient encore le shared_ptr\n", stderr);
    }
    std::puts("fini");
}
```

**Observé** :

```
use_count = 0
cells liberees, le worker tient encore le shared_ptr
==82286==ERROR: AddressSanitizer: heap-use-after-free on address 0x6020000000d0
WRITE of size 4 at 0x6020000000d0 thread T1
freed by thread T0 here:
    #1 main shared_ptr_alias_uaf.cpp:42      // cells.shrink_to_fit()
```

L'agent a testé et **rejeté** les deux alternatives : répondre `false` inconditionnellement pour
`is_lifetime_aware<std::shared_ptr<T>>` casse trois assertions des tests et supprime la
fonctionnalité ; s'appuyer sur `dynamic_type_is_known<T>` est orthogonal (le `Cell` du repro est
un `std::atomic<int>`, assez `final` pour que cette garde passe). Un test d'exécution
`if (p.use_count() == 0) throw` dans `launch_task` transformerait une bibliothèque de
compilation en bibliothèque d'exécution, n'attraperait pas le cas de l'aliasing sur un
propriétaire condamné, et lirait `use_count` en course.

**Ce qui est honnête, et qui est un changement de commentaire uniquement** — remplacer le
commentaire au-dessus des règles `shared_ptr`/`weak_ptr` dans
`include/threadsafe/details/lifetime_aware.h` par :

```cpp
// Ownership is transitive: the control block keeps the T alive, but a T that
// only borrows still borrows -- and that answer is read off the static type, so
// the dynamic type must be known for it to hold of the object actually pointed to.
//
// PRECONDITION, which no static check can enforce: the control block must own
// the pointee. Reflection cannot see a shared_ptr's deleter -- it is erased at
// construction -- so a deleter that borrows (a pool, an arena, a free list) is
// invisible here, and so is the aliasing constructor, which can produce a
// shared_ptr that owns nothing at all. Build such handles with make_shared, or
// with a deleter that owns everything it touches, before letting one cross a
// thread boundary.
```

C'est la forme que `launch_scoped_task` emploie déjà pour sa propre précondition invérifiable.
Je le recommande, et je le rapporte comme un commentaire, pas comme un correctif : il ne change
pas une seule réponse.

---

## 5. Les faux rejets : ampleur mesurée, et ce qui relève du choix

Un faux rejet n'est pas un problème de sûreté. C'est un problème d'**adoption**, et à ce titre
c'est un problème de conférence : la bibliothèque qui refuse `launch_task(&poll_for, 100ms)`
sera contournée, et l'échappatoire qu'elle propose alors — spécialisez le trait vous-même —
entraîne exactement l'habitude qu'elle existe pour empêcher.

### 5.1 L'ampleur, mesurée

J'ai écrit et exécuté mon propre panorama contre les en-têtes intacts : 27 types standard,
quatre questions chacun. `Sync` = `is_synchronizable_v<T>` (non qualifié), `S` = `is_sendable_v<T>`,
`CS` = `is_synchronizable_v<const T>`, `LA` = `is_lifetime_aware_v<T>`.

| Type | `Sync` | `S` | `CS` | `LA` |
|---|:--:|:--:|:--:|:--:|
| `std::mutex` | 0 | 1 | 1 | 1 |
| `std::shared_mutex` | 0 | 1 | 1 | 1 |
| `std::recursive_mutex` | 0 | 1 | 1 | 1 |
| `std::condition_variable` | 0 | **0** | **0** | 1 |
| `std::latch` | 0 | 1 | 1 | 1 |
| `std::binary_semaphore` | 0 | 1 | 1 | 1 |
| `std::atomic_flag` | 0 | 1 | 1 | 1 |
| `std::once_flag` | 0 | 1 | 1 | 1 |
| `std::atomic<int>` | **1** | 1 | 1 | 1 |
| `std::chrono::milliseconds` | 0 | **0** | **0** | 1 |
| `std::chrono::steady_clock::time_point` | 0 | **0** | **0** | 1 |
| `std::complex<double>` | 0 | **0** | **0** | 1 |
| `std::bitset<64>` | 0 | **0** | **0** | 1 |
| `std::expected<int, std::string>` | 0 | **0** | **0** | 1 |
| `std::stack<int>` | 0 | **0** | **0** | 1 |
| `std::queue<std::string>` | 0 | **0** | **0** | 1 |
| `std::priority_queue<int>` | 0 | **0** | **0** | 1 |
| `std::thread` | 0 | **0** | **0** | **0** |
| `std::jthread` | 0 | **0** | **0** | **0** |
| `std::future<int>` | 0 | **0** | **0** | **0** |
| `std::promise<int>` | 0 | **0** | **0** | **0** |
| `std::filesystem::path` | 0 | **0** | **0** | **0** |
| `std::thread::id` | 0 | **0** | **0** | **0** |
| `std::span<int>` | 0 | **0** | **0** | 0 |
| `decltype(std::views::iota(0,10))` | 0 | 1 | 1 | **0** |
| `std::string` | 0 | 1 | 1 | 1 |
| `std::vector<int>` | 0 | 1 | 1 | 1 |

**16 des 27 types sont refusés par `is_sendable`.** Un seul type standard, `std::atomic<T>`, est
`Sync` — donc `is_sendable_v<std::mutex&>` et `is_sendable_v<std::latch&>` sont faux, ce que
j'ai vérifié explicitement en fin de panorama.

Deux lectures se lisent directement dans la table et méritent d'être dites :

- **La colonne `LA` et la colonne `S` se contredisent sur douze lignes.** `chrono::milliseconds`,
  `complex`, `bitset`, `expected`, `stack`, `queue`, `priority_queue`, et les primitifs :
  `is_lifetime_aware` dit « ce type possède ce qu'il atteint », `is_sendable` dit « il pourrait
  partager un état caché ». La cause est une seule asymétrie : **`is_lifetime_aware` n'exécute
  pas `has_only_default_copy_move_destroy`, et `is_sendable` l'exécute.** Cette garde appartient
  aux deux marches ou à aucune ; aujourd'hui elle produit une famille de types que la
  bibliothèque approuve et désapprouve simultanément.
- **`std::mutex` répond `S=1` et `CS=1` mais `Sync=0`**, et c'est le `Sync=0` qui compte : un
  `std::mutex&` ne peut pas traverser, un `std::mutex` par valeur ne le peut pas non plus parce
  qu'il n'est ni copiable ni déplaçable. La ligne `S=1` est vraie et sans effet.

### 5.2 Ce qui relève d'un choix délibéré, et ce qui est un accident

| Groupe | Ids | Nature | Correctif |
|---|---|---|---|
| Primitifs de synchronisation | TC-7, TLS-04, ADV-11 | **accident d'omission** — la politique opt-in est cohérente, mais le raisonnement a été appliqué à deux types de vocabulaire (`stop_token`, `stop_source`) et pas aux primitifs que la norme garantit déjà | oui, `suite-passes` |
| Vocabulaire de valeurs | TLS-05, TLS-06 | **accident** — dégât collatéral de `may_hijack_copy_move` et du pimpl incomplet | oui, `suite-passes` |
| `std::span<T>` | TLS-08 | **accident** — la bibliothèque répond déjà correctement pour `T*` | oui, `suite-passes` |
| `std::views::iota` | TLS-10 | **accident** — la règle `borrowed_range` confond « vue **dans** un stockage » et « vue **sans** stockage » | oui, `suite-passes` |
| Templates de constructeur | TC-8 | **délibéré et documenté** (`tests/test_sendable.cpp:202`) | `suite-regresses` — arbitrage |
| Lambdas capturantes | ADV-08 | **délibéré et nécessaire** — la garde est porteuse | **aucun** — limite de GCC 16 |
| `filter_view` sur `owning_view<list>` | TLS-11 | **accident**, mais irréparable sans deviner | aucun |
| Types récursifs | TC-2, TLS-02, ADV-07 | **accident** | oui — §6 |

### 5.3 Les primitifs de synchronisation

Trois agents ont trouvé indépendamment le même défaut et proposé trois versions du même
correctif ; TC-7 et TLS-04 ont été régression-testés (`suite-passes`), ADV-11 non
(`not-checked`, et son auteur le dit explicitement parce que c'est un changement de politique).
Le nœud est là :

```cpp
struct GuardedCounter {
    mutable std::mutex gate_;
    int value_;
};
static_assert(threadsafe::is_synchronizable_v<const GuardedCounter>);  // ECHOUE aujourd'hui
```

Le type gardé par mutex canonique — le premier exemple de tout cours sur les threads — échoue,
et le diagnostic est correct et sans issue :

```
'const Cache::mutex_ (std::mutex) is mutable, so it is written through a const
 reference: its type must be fully synchronizable'
```

`mutable` exige le trait complet ; `std::mutex` n'est pas `Sync` ; fin.

Le correctif, complet — à ajouter à `include/threadsafe/details/vocabulary.h`, juste avant
l'accolade fermante de `namespace threadsafe`, en étendant le bloc d'includes de ce fichier avec
`<atomic>`, `<barrier>`, `<condition_variable>`, `<cstddef>`, `<latch>`, `<mutex>`,
`<semaphore>`, `<shared_mutex>` et `#include <threadsafe/details/synchronizable.h>` :

```cpp
// Les primitifs de synchronisation de la norme sont Sync par construction :
// [thread.mutex.requirements.mutex] exige lock/unlock concurrents depuis
// plusieurs threads, [thread.condition] le dit des variables de condition,
// [thread.coord] de latch, barrier et counting_semaphore, et [atomics.flag] de
// atomic_flag. C'est exactement ce que demande le trait non qualifie. Rien de
// structurel ne peut le deduire : la bibliotheque doit les nommer.
template <>
struct is_synchronizable<std::atomic_flag> : std::true_type {};
template <>
struct is_synchronizable<std::once_flag> : std::true_type {};

template <>
struct is_synchronizable<std::mutex> : std::true_type {};
template <>
struct is_synchronizable<std::recursive_mutex> : std::true_type {};
template <>
struct is_synchronizable<std::timed_mutex> : std::true_type {};
template <>
struct is_synchronizable<std::recursive_timed_mutex> : std::true_type {};
template <>
struct is_synchronizable<std::shared_mutex> : std::true_type {};
template <>
struct is_synchronizable<std::shared_timed_mutex> : std::true_type {};

template <>
struct is_synchronizable<std::condition_variable> : std::true_type {};
template <>
struct is_synchronizable<std::condition_variable_any> : std::true_type {};

template <>
struct is_synchronizable<std::latch> : std::true_type {};
template <std::ptrdiff_t LeastMaxValue>
struct is_synchronizable<std::counting_semaphore<LeastMaxValue>>
    : std::true_type {};

// La fonction de completion s'execute sur le thread qui arrive en dernier, donc
// la barriere synchronise chaque thread avec elle : elle doit etre envoyable.
template <class CompletionFunction>
struct is_synchronizable<std::barrier<CompletionFunction>>
    : is_sendable<CompletionFunction> {};

// PRECONDITION, redite depuis [atomics.ref.generic] : tant qu'un
// std::atomic_ref refere a un objet, tout acces a cet objet doit passer par un
// atomic_ref. Sous cette precondition le referent se synchronise lui-meme,
// exactement comme la regle de std::atomic le suppose.
template <class T>
struct is_synchronizable<std::atomic_ref<T>> : is_sendable<T> {};
```

Ce que le correctif **ne** fait délibérément **pas** : il ne touche pas à `is_lifetime_aware`,
donc `launch_task` (la voie détachée) refuse toujours `std::ref(gate)` — correctement, et
désormais avec le bon message. Seul `launch_scoped_task`, qui joint avant de retourner,
l'accepte. C'est exactement la distinction que la bibliothèque veut enseigner.

Le fichier d'acceptation, complet :

```cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<std::atomic_flag>);
static_assert(is_synchronizable_v<std::mutex>);
static_assert(is_synchronizable_v<std::atomic_ref<int>>);
static_assert(!is_synchronizable_v<std::atomic_ref<int *>>);

static_assert(is_sendable_v<std::atomic_flag &>);
static_assert(is_sendable_v<std::reference_wrapper<std::atomic_flag>>);
static_assert(is_sendable_v<std::shared_ptr<std::atomic_flag>>);
static_assert(threadsafe::launchable_scoped_task<
              decltype([](std::atomic_flag &) {}),
              std::reference_wrapper<std::atomic_flag>>);

struct GuardedCounter {
    mutable std::mutex gate_;
    int value_;
};
static_assert(is_synchronizable_v<const GuardedCounter>);

struct LeakyCounter {
    mutable std::mutex gate_;
    mutable int cached_;
};
static_assert(!is_synchronizable_v<const LeakyCounter>);

int main() {}
```

`LeakyCounter` reste correctement refusé : c'est le contrôle qui prouve que le correctif
n'ouvre pas la vanne.

### 5.4 Le vocabulaire de valeurs

`std::chrono::milliseconds` est l'argument le plus courant qu'on passe à un thread, et il est
refusé. Le constructeur de `duration` sur lequel la garde trébuche est

```cpp
template<class Rep2, class Period2> constexpr duration(const duration<Rep2,Period2>&);
```

qui prend un `const&` et ne peut donc **pas** détourner une copie — le commentaire de `utils.h`
le dit lui-même — mais `parameters_of` ne peut pas être appelé sur un template, donc il est
bloqué quand même.

Correctif TLS-05 (`suite-passes`), trois éditions dans
`include/threadsafe/details/allowed_std_wrappers.h`.

*(a)* Bloc d'includes complet, remplacé :

```cpp
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <complex>
#include <deque>
#include <expected>
#include <forward_list>
#include <list>
#include <map>
#include <meta>
#include <optional>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
```

*(b)* Tableau `allowed_std_wrappers` complet, remplacé :

```cpp
inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,        ^^std::deque,             ^^std::list,
    ^^std::forward_list,  ^^std::basic_string,      ^^std::map,
    ^^std::multimap,      ^^std::set,               ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset,
    ^^std::pair,          ^^std::tuple,             ^^std::optional,
    ^^std::variant,       ^^std::array,
    // Vocabulaire de valeurs : une specialisation, ce sont ses arguments
    // detenus par valeur, et chacun de ces templates bloque le defaut
    // structurel avec un template de constructeur herite de la signature de la
    // norme, pas d'un partage qui lui serait propre.
    ^^std::chrono::duration, ^^std::chrono::time_point,
    ^^std::complex,       ^^std::bitset,            ^^std::expected,
    // Adaptateurs de conteneur : une specialisation detient le conteneur
    // adapte, et le conteneur est deja dans cette liste.
    ^^std::stack,         ^^std::queue,             ^^std::priority_queue,
};
```

*(c)* `wrapped_types_of` complet, remplacé, pour que `std::expected<void, E>` continue de
fonctionner :

```cpp
// Les arguments qui portent une valeur : les arguments de type. Un
// std::array<T, N> enveloppe des T, pas un N. Un argument void ne porte pas de
// valeur non plus -- c'est ce que std::expected<void, E> ecrit pour dire que le
// cas de succes ne detient rien.
inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
    std::vector<std::meta::info> wrapped;
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type)))
        if (std::meta::is_type(argument)
            && !std::meta::is_void_type(std::meta::dealias(argument)))
            wrapped.push_back(std::meta::remove_cv(argument));
    return wrapped;
}
```

La propagation continue de fonctionner, ce qui est tout l'intérêt de passer par la liste blanche
plutôt que de bénir les templates directement — vérifié par l'agent :
`is_sendable_v<std::stack<int>>` vrai, `is_sendable_v<std::stack<int*>>` faux,
`is_sendable_v<std::expected<int*, int>>` faux,
`is_synchronizable_v<const std::stack<int*>>` faux, `is_lifetime_aware_v<std::stack<int*>>` faux.

Correctif TLS-06 (`suite-passes`), à ajouter à `include/threadsafe/details/vocabulary.h` avec
`<filesystem>`, `<future>` et `<thread>` dans son bloc d'includes :

```cpp
// std::filesystem::path detient une chaine et un vecteur de composants. La
// marche structurelle ne peut pas le dire : la liste de composants est un
// pimpl, et un pointe incomplet est precisement ce que la marche refuse de
// juger.
template <>
struct is_sendable<std::filesystem::path> : std::true_type {};
template <>
struct is_synchronizable<const std::filesystem::path> : std::true_type {};
template <>
struct is_lifetime_aware<std::filesystem::path> : std::true_type {};

// Les handles move-only que la norme concoit pour traverser une frontiere de
// thread. Chacun detient son etat partage via un compteur de references ; aucun
// n'est lisible depuis deux threads a la fois, donc seules les questions non
// qualifiees sont repondues ici. [futures.unique.future]/2 enonce le transfert
// pour future ; [thread.thread.class] l'enonce pour thread.
template <>
struct is_sendable<std::thread> : std::true_type {};
template <>
struct is_lifetime_aware<std::thread> : std::true_type {};
template <>
struct is_sendable<std::jthread> : std::true_type {};
template <>
struct is_lifetime_aware<std::jthread> : std::true_type {};

// Le handle detient son etat partage ; la valeur qu'il livrera reste ce que T
// est, donc les deux questions suivent T. Un resultat void ne porte aucune
// valeur, donc aucun emprunt.
template <class T>
struct is_sendable<std::future<T>>
    : std::bool_constant<std::is_void_v<T>
                         || is_sendable_v<std::remove_cv_t<T>>> {};
template <class T>
struct is_lifetime_aware<std::future<T>>
    : std::bool_constant<std::is_void_v<T>
                         || is_lifetime_aware_v<std::remove_cv_t<T>>> {};
template <class T>
struct is_sendable<std::promise<T>>
    : std::bool_constant<std::is_void_v<T>
                         || is_sendable_v<std::remove_cv_t<T>>> {};
template <class T>
struct is_lifetime_aware<std::promise<T>>
    : std::bool_constant<std::is_void_v<T>
                         || is_lifetime_aware_v<std::remove_cv_t<T>>> {};
```

Délibérément laissés à `false`, et l'agent a vérifié que chacun le reste sous le correctif :
`std::shared_future<T>` — [futures.shared.future]/3 exige un `shared_future` **distinct** par
thread — et `std::packaged_task<Sig>`, qui stocke un callable type-effacé, donc rien de statique
à vérifier.

Correctif TLS-08 (`suite-passes`), à ajouter à `vocabulary.h` avec `<cstddef>`, `<span>` et
`#include <threadsafe/details/synchronizable.h>` :

```cpp
// Un std::span est un pointeur et une longueur, et la bibliotheque repond deja a
// cette paire : is_sendable<T*> vaut is_synchronizable<T>. Ecrit explicitement
// parce que les templates de constructeur de span bloquent le defaut structurel
// (may_hijack_copy_move ne peut pas voir que chacun d'eux prend une range, pas
// un span&).
//
// is_lifetime_aware est deliberement laisse a la marche structurelle : un span
// est un borrowed_range, et il le reste.
template <class T, std::size_t Extent>
struct is_sendable<std::span<T, Extent>>
    : is_synchronizable<std::remove_cv_t<T>> {};

template <class T, std::size_t Extent>
struct is_synchronizable<const std::span<T, Extent>>
    : is_synchronizable<std::remove_cv_t<T>> {};
```

Correctif TLS-10 (`suite-passes`), deux éditions dans
`include/threadsafe/details/lifetime_aware.h`. Insérer immédiatement **avant** le bloc de
commentaire qui commence par `// The trait itself, phrased so that "no" carries its reason.` :

```cpp
// std::ranges::borrowed_range promet que les iterateurs restent valides une fois
// l'objet range disparu. Deux formes distinctes obtiennent cette promesse : une
// vue *dans* un stockage qu'elle ne possede pas (std::span, std::string_view,
// std::ranges::subrange), et une vue sans aucun stockage, qui calcule chaque
// element (std::views::iota). Seule la premiere est un emprunt. Le
// dereferencement les distingue -- un stockage rend une reference dedans, un
// calcul rend une valeur.
inline consteval bool borrows_its_elements(std::meta::info type) {
    using namespace std::meta;

    if (!trait_value(^^std::ranges::borrowed_range, type))
        return false;

    return is_reference_type(
        dealias(substitute(^^std::ranges::range_reference_t, {type})));
}
```

puis, dans `detail::diagnose_default_is_lifetime_aware`, remplacer

```cpp
    if (trait_value(^^std::ranges::borrowed_range, type))
```

par

```cpp
    if (borrows_its_elements(type))
```

Le message actuel est activement faux — il affirme un stockage qui n'existe pas :

```
'std::ranges::iota_view<int, int> is a borrowed range: a view over someone
 else's storage, it does not keep its elements alive'
```

alors que `sizeof(decltype(std::views::iota(0,10))) == 2 * sizeof(int)`. Un cas que le correctif
n'atteint pas, nommé plutôt que masqué : `std::ranges::empty_view<int>` reste faux, parce que
`range_reference_t` y vaut `int&`. Il n'a aucun élément à faire pendre, donc c'est inoffensif.

### 5.5 Ce qui est délibéré, et que je recommande de ne pas corriger

**TC-8 — les templates de constructeur.** La bibliothèque documente la sur-rejection à
`tests/test_sendable.cpp:202` : « parameters_of rejects a template, so a shape that could never
hijack is indistinguishable from one that does ». La prémisse est vraie —
`parameters_of` sur un template de constructeur non substitué lève
`reflection does not represent a function or function type`. Deux resserrements ont été essayés :

- `can_substitute` + `parameters_of` sur un type sonde : corrige les six faux rejets et garde
  tous les détourneurs refusés, **mais `can_substitute` instancie le corps du template**.
  Simplement poser la question au trait devient alors une erreur dure pour tout type dont le
  corps du constructeur template est invalide quand l'argument est la classe elle-même —
  `std::complex<double>` est exactement un tel type. **Impasse**, et c'est un comportement de
  GCC 16.2 qui vaut d'être remonté en amont ;
- `is_trivially_*` à la place : pas d'erreur dure, corrige `Meters`, `Histogram`,
  `TemplatedAssign`, `MyVariant`, `FixedVector`, garde tous les détourneurs refusés, **mais ne
  sauve pas une forme inoffensive non trivialement copiable** (un `GuardedForward` qui détient
  un `std::string`), et **régresse** `tests/test_sendable.cpp:200-202`, qui encode la limitation
  actuelle.

Mon avis, sous la règle « challenger le besoin » : **ne pas corriger TC-8 dans le code, le
corriger dans la liste blanche.** Élargir `allowed_std_wrappers` (§5.4) fait disparaître le
symptôme là où il fait mal — le vocabulaire standard — pour un coût nul en sûreté, parce que la
liste blanche lit les arguments et propage. La sur-rejection résiduelle sur les types
utilisateur reste, et c'est une bonne diapositive : *« la réflexion de C++26 ne peut pas encore
regarder à l'intérieur d'un template de constructeur, donc nous refusons par défaut. »*

**ADV-08 — les lambdas capturantes.** Délibéré, **nécessaire**, et non corrigeable. Vérifié par
moi aujourd'hui sur les en-têtes intacts :

```cpp
#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>

using threadsafe::is_sendable_v;

void probe() {
    int local = 0;
    std::string owned = "x";
    std::unique_ptr<int> owner;

    auto by_ref   = [&local]{ return local; };
    auto by_val   = [local]{ return local; };
    auto owns     = [owned]{ return owned.size(); };
    auto owns_ptr = [p = std::move(owner)]{ return p.get(); };
    auto none     = []{ return 1; };

    static_assert(!is_sendable_v<decltype(by_ref)>);
    static_assert(!is_sendable_v<decltype(by_val)>,   "OBSERVE : capture par valeur refusee");
    static_assert(!is_sendable_v<decltype(owns)>,     "OBSERVE : capture possedante refusee");
    static_assert(!is_sendable_v<decltype(owns_ptr)>, "OBSERVE : capture unique_ptr refusee");
    static_assert(is_sendable_v<decltype(none)>);
}

int main() {}
```

Ce fichier compile proprement : **toutes** ces assertions tiennent. Et l'idiome vitrine :

```cpp
#include <threadsafe/threadsafe.h>
#include <string>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::string owned = "payload";
    launcher.launch_task([owned]{});
}
```

produit, mesuré par moi :

```
asynchronous_task_launcher.h:99:5: error: uncaught exception of type
'std::meta::exception'; 'what()': 'main()::<lambda()> holds state reflection
cannot see (a closure type with captures); specialize is_sendable to state the
intent'
```

La raison pour laquelle la bibliothèque n'a pas le choix, sondée directement sur GCC 16 :

```
by_val  empty=0 nsdm=0 members=1 size=4
by_ref  empty=0 nsdm=0 members=1 size=8
none    empty=1 nsdm=0 members=1 size=1
```

Une fermeture qui a capturé un `int` par valeur occupe 4 octets et rapporte **zéro** membre de
données non statique. Les captures sont réellement invisibles. Affaiblir
`has_unreflectable_state` admettrait immédiatement `[&local]{}` comme envoyable, ce qui est un
échange nettement pire.

Deux améliorations bornées, aucune régression-testée, donc à traiter comme des propositions :

```cpp
// A la toute fin de include/threadsafe/details/sendable.h, apres l'accolade
// fermante de namespace threadsafe -- l'echappatoire qui manque, symetrique de
// celle que is_synchronizable possede deja.
#define THREADSAFE_UNSAFE_ASSERT_SENDABLE(...)  \
    template <>                                 \
    struct threadsafe::is_sendable<__VA_ARGS__> : std::true_type {}
```

et faire nommer au diagnostic le remède qui marche réellement, puisque « spécialisez
`is_sendable` » est malaisé pour un type sans nom — dans `diagnose_default_is_sendable`,
remplacer le texte du rejet `has_unreflectable_state` par :

```cpp
    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures): the compiler does not expose a lambda's captures, "
               u8"so the walk cannot tell an owned capture from a borrowed "
               u8"one. Write the callable as a named struct with explicit "
               u8"members, or vouch for this one with "
               u8"THREADSAFE_UNSAFE_ASSERT_SENDABLE",
               path);
```

Le contournement qui fonctionne aujourd'hui, vérifié :

```cpp
struct Body { std::string owned; void operator()() const {} };
launcher.launch_task(Body{"payload"});   // accepte
```

Pour la conférence, c'est la diapositive la plus honnête du lot : **le modèle de la bibliothèque
est « un callable n'est qu'un type envoyable », et la réflexion d'aujourd'hui ne peut pas
regarder à l'intérieur du seul callable que tout le monde utilise.**

---

## 6. Les types récursifs : trois agents, trois verdicts, un correctif vérifié

C'est le seul endroit du rapport où j'ai contredit les données qu'on m'a remises, et où je l'ai
fait en compilant.

**Le défaut.** `struct Node { int value_; std::vector<Node> kids_; };` — le conteneur récursif
des manuels, légal depuis C++17 — ne répond pas `false` : il fait **échouer la compilation**.

```cpp
#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>

// La liste chainee possedante classique.
struct OwningNode { int value; std::unique_ptr<OwningNode> next; };
static_assert(threadsafe::is_sendable_v<OwningNode>);

// L'arbre possedant classique.
struct Tree { int value; std::vector<Tree> children; };
static_assert(threadsafe::is_sendable_v<Tree>);

// Par contraste : le noeud EMPRUNTEUR est traite correctement et termine.
struct Node { int value; Node* next; };
static_assert(!threadsafe::is_sendable_v<Node>);
```

**Observé** sur les en-têtes intacts :

```
utils.h:99:36: error: the value of 'threadsafe::is_sendable_v<Tree>' is not usable
  in a constant expression
sendable.h:24:48: error: 'value' is not a member of 'threadsafe::is_sendable<std::vector<Tree> >'
sendable.h:24:48: error: 'value' is not a member of 'threadsafe::is_sendable<Tree>'
H09c_recursive_vec.cpp:4:27: error: non-constant condition for static assertion
```

La marche termine à travers les **pointeurs** et meurt à travers la **propriété** — l'inverse de
ce qu'un utilisateur devinerait. `tests/test_smart_pointers.cpp:16` définit lui-même
`struct Tree { std::unique_ptr<Tree> left; };` et ne demande jamais `is_sendable_v<Tree>` ; la
suite ne l'attrape donc pas.

**Les trois verdicts qu'on m'a remis.** ADV-07 : « NONE que je sois prêt à livrer comme code
vérifié, et la raison est architecturale ». TLS-02 : « NONE within this design […] briser le
cycle demande un point fixe co-inductif, ce qui est une refonte, pas un patch ». TC-2 : un
correctif complet, `suite-passes`.

**Ce que j'ai fait.** J'ai appliqué le correctif de TC-2 et l'ai testé sur les deux formes.
Résultat : **il fonctionne pour la récursion à travers les conteneurs et pas pour la récursion à
travers les pointeurs intelligents.** La raison est nette : `is_sendable<std::unique_ptr<T,D>>`
lit `is_sendable_v<T>` **directement**, comme une expression C++ ordinaire, et contourne donc la
garde réflexive que TC-2 pose dans `trait_value`.

J'ai complété le correctif et revérifié le tout. Voici l'ensemble, complet.

*(1) `include/threadsafe/details/utils.h`* — insérer immédiatement après le `trait_value`
existant à deux arguments :

```cpp
// Un type peut s'atteindre lui-meme : `struct Node { std::vector<Node> kids; }`.
// La marche interroge alors le trait a propos d'une specialisation encore en
// cours d'instanciation, et lire son `value` est une erreur dure plutot qu'une
// reponse. La reflexion voit exactement cela -- la specialisation n'est pas
// encore un type complet -- et le cycle est ferme comme Rust ferme un auto
// trait : on suppose oui. L'obligation qu'ajouterait la recursion est celle que
// la question exterieure est deja en train de prouver, donc un vrai coupable
// ailleurs dans le type repond toujours non.
inline consteval bool trait_value(std::meta::info trait_class,
                                  std::meta::info trait_variable,
                                  std::meta::info type) {
    if (!std::meta::is_complete_type(std::meta::substitute(trait_class, {type})))
        return true;

    return trait_value(trait_variable, type);
}
```

*(2)* Router les trois faces `info` du trait à travers elle. Dans
`include/threadsafe/details/synchronizable_base.h` :

```cpp
inline consteval bool is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable, ^^is_synchronizable_v, type);
}
```

dans `include/threadsafe/details/sendable.h` :

```cpp
inline consteval bool is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable, ^^is_sendable_v, type);
}
```

dans `include/threadsafe/details/lifetime_aware.h` :

```cpp
inline consteval bool is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware, ^^is_lifetime_aware_v, type);
}
```

*(3) Ma complétion, sans laquelle `std::unique_ptr<Node>` explose encore.* Dans
`include/threadsafe/details/smart_pointers.h`, remplacer les deux règles `unique_ptr` par :

```cpp
// Ces deux regles doivent poser leur question a travers la face `info` du
// trait, pas via is_sendable_v / is_lifetime_aware_v : la variante `info` porte
// la garde de cycle, la lecture directe de la variable ne la porte pas, et
// `struct Node { std::unique_ptr<Node> next; }` est exactement le chemin qui
// passe par ici.
template <class T, class D>
struct is_sendable<std::unique_ptr<T, D>>
    : std::bool_constant<
          is_sendable_type(^^std::remove_all_extents_t<T>)
          && is_sendable_type(^^D)
          && detail::dynamic_type_is_known<std::remove_all_extents_t<T>>> {};

template <class T, class D>
struct is_lifetime_aware<std::unique_ptr<T, D>>
    : std::bool_constant<
          is_lifetime_aware_type(^^std::remove_all_extents_t<T>)
          && is_lifetime_aware_type(^^D)
          && detail::dynamic_type_is_known<std::remove_all_extents_t<T>>> {};
```

et dans `include/threadsafe/details/lifetime_aware.h`, remplacer les deux règles
`shared_ptr`/`weak_ptr` par (elles sont déclarées **avant** `is_lifetime_aware_type`, d'où
l'appel direct à `detail::trait_value`) :

```cpp
// Ownership is transitive: the control block keeps the T alive, but a T that
// only borrows still borrows -- and that answer is read off the static type, so
// the dynamic type must be known for it to hold of the object actually pointed to.
//
// La question passe par detail::trait_value a trois arguments et non par
// is_lifetime_aware_v, pour la garde de cycle : `struct Node {
// std::shared_ptr<Node> next; }` reentre sinon dans sa propre instanciation.
template <class T>
struct is_lifetime_aware<std::shared_ptr<T>>
    : std::bool_constant<
          detail::trait_value(
              ^^is_lifetime_aware, ^^is_lifetime_aware_v,
              ^^std::remove_cv_t<std::remove_all_extents_t<T>>)
          && detail::dynamic_type_is_known<
                 std::remove_cv_t<std::remove_all_extents_t<T>>>> {};
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>>
    : std::bool_constant<
          detail::trait_value(
              ^^is_lifetime_aware, ^^is_lifetime_aware_v,
              ^^std::remove_cv_t<std::remove_all_extents_t<T>>)
          && detail::dynamic_type_is_known<
                 std::remove_cv_t<std::remove_all_extents_t<T>>>> {};
```

**Ce que j'ai vérifié, moi, aujourd'hui.** Fichier d'acceptation complet :

```cpp
#include <threadsafe/threadsafe.h>
#include <list>
#include <map>
#include <memory>
#include <vector>

struct TreeNode { int value_; std::vector<TreeNode> children_; };
struct ListNode { int value_; std::list<ListNode> children_; };
struct MapNode  { std::map<int, MapNode> children_; };
struct Odd;
struct Even { std::vector<Odd> odds_; };
struct Odd  { std::vector<Even> evens_; };
struct BorrowTree  { int *borrowed_; std::vector<BorrowTree> children_; };
struct MutableTree { mutable int hits_; std::vector<MutableTree> children_; };
struct OwningNode  { int value; std::unique_ptr<OwningNode> next; };
struct SelfShared  { std::shared_ptr<SelfShared> next; };
struct UniqueTree  { int value; std::vector<std::unique_ptr<UniqueTree>> children; };
struct BorrowUniqueTree {
    int *borrowed_;
    std::vector<std::unique_ptr<BorrowUniqueTree>> children;
};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<TreeNode>);
static_assert(is_lifetime_aware_v<TreeNode>);
static_assert(is_synchronizable_v<const TreeNode>);
static_assert(is_synchronizable_v<const ListNode>);
static_assert(is_synchronizable_v<const MapNode>);
static_assert(is_synchronizable_v<const Even>);
static_assert(is_sendable_v<OwningNode>);
static_assert(is_lifetime_aware_v<OwningNode>);
static_assert(is_sendable_v<UniqueTree>);
static_assert(is_lifetime_aware_v<UniqueTree>);
static_assert(is_lifetime_aware_v<SelfShared>);

static_assert(!is_sendable_v<BorrowTree>);
static_assert(!is_synchronizable_v<const BorrowTree>);
static_assert(!is_synchronizable_v<const MutableTree>);
static_assert(!is_sendable_v<BorrowUniqueTree>);
static_assert(!is_lifetime_aware_v<BorrowUniqueTree>);

int main() {}
```

Ce fichier échoue avec 26 erreurs sur les en-têtes intacts et compile **proprement** sous le
correctif complet. Les 11 TU de `tests/` restent vertes.

**Le stress de sûreté, parce qu'un point fixe co-inductif peut blanchir un coupable.** J'ai écrit
et exécuté celui-ci, complet :

```cpp
#include <threadsafe/threadsafe.h>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// coupable APRES le membre recursif
struct AfterPtr     { std::vector<AfterPtr> kids_; int *borrowed_; };
// mutable APRES le membre recursif
struct AfterMutable { std::vector<AfterMutable> kids_; mutable int hits_; };
// membre reference APRES le membre recursif
struct AfterRef     { std::vector<AfterRef> kids_; int &referent_; };
// coupable a deux sauts, en travers d'un cycle mutuel
struct B2; struct A2 { std::vector<B2> bs_; };
struct B2 { std::vector<A2> as_; int *borrowed_; };
// coupable enfoui dans un std::optional<std::pair<int, int *>>
struct Buried { std::vector<Buried> kids_; std::optional<std::pair<int, int *>> hidden_; };
// coupable derriere une recursion par unique_ptr
struct UPtrBad { std::unique_ptr<UPtrBad> next_; int *borrowed_; };

static_assert(!is_sendable_v<AfterPtr>);
static_assert(!is_synchronizable_v<const AfterPtr>);
static_assert(!is_lifetime_aware_v<AfterPtr>);
static_assert(!is_synchronizable_v<const AfterMutable>);
static_assert(!is_synchronizable_v<const AfterRef>);
static_assert(!is_sendable_v<AfterRef>);
static_assert(!is_sendable_v<A2>);
static_assert(!is_synchronizable_v<const A2>);
static_assert(!is_lifetime_aware_v<A2>);
static_assert(!is_sendable_v<Buried>);
static_assert(!is_lifetime_aware_v<Buried>);
static_assert(!is_sendable_v<UPtrBad>);
static_assert(!is_lifetime_aware_v<UPtrBad>);

// poser la question dans l'autre ordre d'abord ne doit rien blanchir non plus
struct OrderTree { std::vector<OrderTree> kids_; int *borrowed_; };
static_assert(!is_sendable_v<std::vector<OrderTree>>);
static_assert(!is_sendable_v<OrderTree>);

int main() {}
```

**Les 15 assertions tiennent sous le correctif.** Le cycle est fermé sans qu'aucun coupable soit
blanchi, y compris quand il est placé après le membre récursif, à deux sauts en travers d'un
cycle mutuel, enfoui dans une enveloppe standard, ou derrière une récursion par `unique_ptr`.

**Ce que cela change au récit.** ADV-07 avait raison sur le fond du problème : « le trait doit se
lire lui-même pour honorer les spécialisations de l'utilisateur, et se lire lui-même est ce qui
fait exploser les types récursifs » est effectivement une leçon plus fine sur la conception de
traits réflexifs que n'importe lequel des trous de sûreté. Mais la conclusion « donc c'est une
refonte » est fausse : `std::meta::is_complete_type` **sur la spécialisation du trait
elle-même** répond exactement à la bonne question, en cinq lignes, sans abandonner la lecture
réflexive. C'est un excellent moment de conférence : *le langage sait déjà dire « cette
spécialisation est en cours de construction », et cela suffit à implémenter le point fixe
co-inductif de Rust.*

Reste une chose à faire, et c'est un test : ajouter `tests/test_recursive_types.cpp` avec le
fichier d'acceptation et le stress ci-dessus, pour que le comportement soit **choisi** et non
subi.

---

## 7. La limite inhérente, et pourquoi elle est le meilleur moment du talk

### 7.1 Le fait, vérifié

Une classe dont le seul état partagé est un `static inline long` incrémenté dans une méthode
**`const`** passe `is_sendable`, passe `is_synchronizable<const T>`, passe `launchable_task` ;
`synchronized_value` sélectionne donc `std::shared_mutex` ; et ThreadSanitizer rapporte une
course de données réelle avec **les deux threads ne tenant qu'un `shared_lock`**. Le lead a
vérifié ce fait personnellement.

Cette instance-là est corrigeable — c'est ADV-01, §1.1, et le correctif est vérifié. Mais fermer
l'instance **ne ferme pas la classe**, et c'est le cœur du sujet. Vérifié contre l'arbre corrigé
combiné (ADV-01 + ADV-03 appliqués), **les cinq formes suivantes passent encore** :

| Forme | Pourquoi elle passe |
|---|---|
| une méthode `const` qui écrit via `const_cast` | l'écriture est une **instruction** |
| une méthode `const` qui touche un global à portée d'espace de noms | le global n'est pas un membre |
| un singleton atteint depuis n'importe quelle méthode | idem |
| une enveloppe autour d'une bibliothèque C à état caché (`strtok`, `rand`, `localtime`) | idem |
| un entier qui indexe une arène `thread_local` (ADV-04 — **SIGSEGV**, exit 139) | le sens est dans le résolveur |
| une structure détenant un descripteur de fichier dont l'offset est partagé (`::lseek`) | idem |

### 7.2 Pourquoi aucune version de ces traits ne peut la fermer

L'agent a énuméré **toute** la surface de requête `consteval` de `<meta>` dans GCC 16.2. Les
prédicats liés aux fonctions sont exactement :

```
is_function            is_function_type        is_function_template
is_function_parameter  is_conversion_function  is_conversion_function_template
is_operator_function   is_operator_function_template
is_special_member_function                     is_vararg_function
is_member_function_pointer_type
```

La seule chose qui approche l'accès à une définition est `source_location_of`, qui rend un
fichier et une ligne — pas une sémantique. **Il n'y a pas de `body_of`, pas de réflexion
d'instruction, pas de réflexion d'expression, pas de requête « lit / écrit ».** P2996 reflète
des **déclarations** ; il ne reflète pas des **définitions**.

Il s'ensuit qu'aucune règle exprimable dans cette bibliothèque ne peut distinguer

```cpp
int value() const { return cache_; }
```

de

```cpp
int value() const { const_cast<Widget*>(this)->cache_ = 42; return cache_; }
```

Les déclarations sont identiques octet pour octet. Les membres sont identiques. Les bases sont
identiques. Les membres spéciaux sont identiques. **Le type EST structurellement sûr en
lecture. Le code ne l'est pas.** Cet écart n'est refermable par aucune quantité d'astuce à
l'intérieur de ces en-têtes.

### 7.3 Pourquoi Rust n'a pas ce trou, en une phrase

`Sync` est sain en Rust parce que **`&T` interdit la mutation sans `UnsafeCell`**. Le `const` de
C++ n'interdit rien : `const_cast` est légal, `mutable` est légal, un global est atteignable
depuis n'importe où, et une méthode `const` peut appeler `::lseek`. Trois différences précises,
et chaque trou de ce rapport est l'une des trois qui transparaît :

| Différence | Ce qu'elle bloque en Rust | Ce qui passe ici |
|---|---|---|
| `rustc` analyse les **corps** | toute écriture dans une méthode `&self` | ADV-02, ADV-04, la famille singleton/global/C |
| un `static` mutable exige `unsafe` | l'état partagé par déclaration | ADV-01 (corrigeable — et corrigé §1.1) |
| il n'y a pas de `const_cast` | l'échappatoire de la mutation | ADV-02 |

Et une quatrième, qui n'est pas une différence de langage mais de modèle de compilation : Rust
n'a pas d'unités de traduction indépendantes qui puissent répondre différemment à la même
question, donc rien de la famille §3 n'existe là-bas.

### 7.4 Ce que je recommande : la documenter, pas la corriger

Sous la règle « challenger le besoin », c'est la recommandation la moins chère et la plus
rentable du rapport, et je la place **au-dessus de tous les correctifs sauf ADV-01 et TC-1**.

L'état actuel, mesuré par un `grep` sur les onze en-têtes et `CLAUDE.md` :

```
$ grep -rniE 'static (data )?member|global|singleton|const_cast|function body|thread_local|cannot check|does not check|body' include/ CLAUDE.md \
    | grep -viE 'static_assert|static inline|static consteval|static constexpr'

include/threadsafe/details/asynchronous_task_launcher.h:103:    // not itself join. The traits cannot check this; the join bounds the
include/threadsafe/details/sendable.h:79:// subobject a second time to word a message nobody reads would make each
```

Deux lignes, dont une sans rapport. **La seule admission de limite dans toute la bibliothèque**
est une précondition de quatre lignes sur `launch_scoped_task`, portant sur une durée de vie et
cantonnée à cette fonction. Pendant ce temps, `CLAUDE.md` fait sept affirmations positives sur
ce que les traits signifient et **zéro** négative, sous un titre qui promet
« safety checked entirely at compile time ».

Un piège précis à ne pas manquer : livrer le correctif d'ADV-01 **en silence** rendra la
bibliothèque plus digne de confiance en apparence tout en laissant les cas `const_cast`, global,
singleton, `thread_local` et état C caché exactement aussi cassés — donc **plus difficiles** à
trouver, pas plus faciles. Le correctif et la documentation vont ensemble ou ne vont pas.

Voici le texte que je recommande d'ajouter à `CLAUDE.md`, immédiatement après le titre
`## Architecture: the traits` et avant la sous-section `is_synchronizable<T>` :

````markdown
### What the traits can and cannot see

Every trait here is **structural**: it reads declarations. It sees bases, data
members, cv-qualification, `mutable`, references and pointers, and — since the
static-member rule — static data members. `std::meta` exposes nothing else:
there is no query that reaches into a function body, so no rule in this library
can distinguish

```cpp
int value() const { return cache_; }
int value() const { const_cast<Widget*>(this)->cache_ = 42; return cache_; }
```

The two declarations are identical. The traits answer about the first and are
silently wrong about the second.

Concretely, a type is blessed although it is not thread-safe whenever the
sharing lives in code rather than in a declaration:

| shape | why it passes |
|---|---|
| a `const` method writing through `const_cast` | the write is a statement |
| a `const` method touching a namespace-scope global | the global is not a member |
| a singleton reached from any method | likewise |
| a wrapper over a C library with hidden state (`strtok`, `rand`, `localtime`) | likewise |
| an integer handle indexing a `thread_local` arena | the meaning is in the resolver |
| a struct holding a file descriptor whose offset is shared | likewise |

Use `mutable` rather than `const_cast` when a `const` method must write: the
library sees `mutable` and refuses. Prefer a static member over a namespace
global for the same reason. Where neither is possible, specialize the trait to
false and say why.

A second limit worth stating: the answer is about the type **as visible in the
current translation unit**, and it is fixed at the first query. Asking while a
type is incomplete — or from inside its own definition — caches `false` for the
rest of the TU, and two translation units that see different definitions will
disagree. Branching on a trait inside a header (`if constexpr
(is_sendable_v<T>)`) is therefore an ODR hazard unless every TU sees the same
definitions.

In short: this library moves the Rust discipline into C++'s type system, not
into C++'s semantics. It catches the mistakes that are visible in a type. Rust
catches more because rustc checks bodies, requires `unsafe` to touch a mutable
`static`, and has no `const_cast`.
````

et de refléter la forme courte en tête de `include/threadsafe/details/sendable.h` et
`include/threadsafe/details/synchronizable.h`, juste à l'intérieur de `namespace threadsafe {`,
pour que la limite soit visible de qui lit les en-têtes plutôt que la documentation :

```cpp
// STRUCTURAL: this trait reads declarations -- bases, members, cv-qualification
// -- and never a function body. A const method that writes through const_cast,
// touches a global, or reaches a singleton is invisible to it and will be
// blessed. See CLAUDE.md, "What the traits can and cannot see".
```

### 7.5 Pourquoi c'est le meilleur moment disponible pour une conférence

Une bibliothèque de conférence est jugée sur ce qu'elle **enseigne**, pas sur son score de
sûreté. Or ce trou-ci enseigne quelque chose qu'aucune démonstration réussie ne pourrait
enseigner :

1. **Il est court à montrer.** Une classe de six lignes, une méthode `const`, un `static inline
   long`. Un public de conférence la lit en quatre secondes et est certain qu'elle est sûre.
2. **Le verdict de la bibliothèque est spectaculairement faux.** Pas « je ne sais pas », mais un
   **oui** affirmatif, sur lequel le helper agit ensuite en choisissant le verrou **plus
   faible**. Le mauvais verdict rend la course *plus* probable, pas moins.
3. **La preuve est disponible en direct.** ThreadSanitizer, deux threads, une écriture de 8
   octets, les deux ne tenant qu'un `shared_lock`.
4. **La séparation est nette et enseignable.** Un membre `static` est une déclaration : c'est
   corrigeable, et c'est corrigé sur scène en trente lignes. Un `const_cast` dans un corps ne
   l'est pas, et l'on peut le prouver en énumérant `<meta>` : il n'y a pas de `body_of`.
5. **Elle situe C++26 par rapport à Rust sans caricature.** Rust n'est pas « meilleur » : Rust a
   fait un choix de langage — `&T` interdit la mutation sans `UnsafeCell` — que C++ n'a pas fait,
   et qui n'était pas disponible rétroactivement. `const` en C++ est une promesse de l'appelé,
   pas une garantie du langage. **Une trait system ne peut pas être plus forte que le système de
   types sur lequel elle repose.**

C'est la phrase que je mettrais sur la diapositive de clôture :

> **`is_synchronizable<const T>` est aussi solide que `const` — et `const`, en C++, n'interdit
> rien.**

Corriger n'est pas possible. Contourner par une API — par exemple, exiger de
`synchronized_value` un opt-in explicite avant de choisir `std::shared_mutex` — déplace la
décision vers un humain sans rendre le trait correct, et c'est un changement d'API dont le rayon
d'action sur les tests et sur le récit du talk appartient à l'auteur, pas à un auditeur. **La
documenter est le seul geste qui soit à la fois honnête, gratuit et pédagogiquement supérieur.**

---

## 8. Récapitulatif : quoi faire, dans quel ordre

### 8.1 Les cinq correctifs que j'ai vérifiés moi-même, et qui composent

J'ai construit un seul arbre d'en-têtes portant les cinq, et exécuté tous les repros plus la
suite complète.

| # | Défaut | Édition | Coût |
|---|---|---|---|
| 1 | ADV-01 — membres `static` invisibles | `utils.h` + `sendable.h` + `synchronizable.h` (§1.1) | 1 faux rejet : `static constexpr const char*` |
| 2 | TC-1 — blanchiment cv | 1 ligne dans `allowed_std_wrappers.h` (§1.3) | aucun |
| 3 | TC-2/TLS-02/ADV-07 — types récursifs | `utils.h` + les 3 faces `info` + `smart_pointers.h` + `lifetime_aware.h` (§6) | aucun |
| 4 | ADV-03 — garde de type dynamique | `sendable.h` + `smart_pointers.h` (§4.1) | un type polymorphe non `final` vouché doit être `final` ou spécialisé |
| 5 | TC-5 — `shared_ptr` ignore l'opt-out | `smart_pointers.h` (§2.4) | aucun |

Résultat de la composition, mesuré par moi :

```
work/statics.cpp                 OK
work/cv_launder.cpp              OK
work/recursive_full.cpp          OK
work/cycle_stress.cpp            OK
work/dyn_fixed.cpp               OK
work/sharedptr_optout.cpp        OK
== SUITE ==  (aucune regression)
```

Coût en temps de compilation, meilleur de 5 exécutions par mesure, machine chargée — donc les
chiffres absolus sont supérieurs à ceux de [06](./06-performance-compilation.md), seul le delta
compte :

| Unité | intacte | 5 correctifs | delta |
|---|---:|---:|---:|
| TU vide + en-tête parapluie | 640 ms | 660 ms | +20 ms |
| `test_sendable.cpp` | 730 ms | 740 ms | +10 ms |
| `test_soundness_regressions.cpp` | 750 ms | 730 ms | −20 ms |
| `test_smart_pointers.cpp` | 670 ms | 680 ms | +10 ms |

Le delta est **dans le bruit**, y compris négatif sur une unité. Les cinq correctifs sont
gratuits.

### 8.2 Les correctifs supplémentaires, régression-testés par leur agent (`suite-passes`)

Non revérifiés par moi, donc solides mais attribués : TC-3 (§3.1, **celui-là je l'ai revérifié**,
11/11), TC-7 / TLS-04 (§5.3), TLS-05 / TLS-06 / TLS-08 / TLS-10 (§5.4), TLS-09 (§2.3),
TC-9 / TC-10 / TC-11 / TC-12 (§2.1, §2.2), TLS-03 (voir [04](./04-diagnostics.md)).

### 8.3 Les arbitrages, qui ne sont pas des patchs

| Id | Question à trancher |
|---|---|
| TC-4 | soit `synchronized_value` cesse de dériver sa disposition mémoire du trait (5 assertions de test à réécrire), soit la macro de vouch est déclarée **header-only** dans `CLAUDE.md`. Je recommande la seconde. |
| TC-8 | la sur-rejection sur les templates de constructeur : ne pas la corriger dans le code, la contourner par l'élargissement de la liste blanche (§5.4). |
| TC-6 | `Sync` implique `Send` est câblé et quatre TU en dépendent. Si le modèle doit changer, la forme honnête est un `THREADSAFE_UNSAFE_ASSERT_SENDABLE` compagnon et **aucune** implication entre les deux. |
| ADV-11 | quels types standard la bibliothèque vouche : décision de politique, pas de sûreté. |

### 8.4 Ce qui n'a pas de correctif, et ce qu'il faut en faire

| Id | Nature | Geste recommandé |
|---|---|---|
| ADV-02 | limite inhérente — corps de fonction | **documenter** (§7.4) |
| ADV-04 | limite inhérente — handle affine au thread | **documenter** (§7.4) |
| ADV-05 | limite inhérente — modèle d'unités de traduction | **documenter** (§7.4) |
| TLS-01, TLS-07 | propriété d'exécution d'un `shared_ptr` | **commentaire de précondition** (§4.2) |
| TLS-11 | dépend d'un détail d'implémentation de libstdc++ | épingler le comportement actuel dans un test |
| TC-14, TC-15 | causes hors de la bibliothèque | une phrase dans le commentaire voisin |

### 8.5 Les trois tests qui manquent, et qui sont plus utiles qu'un correctif

Détail complet dans [03](./03-couverture-de-tests.md) ; les trois qui relèvent de ce rapport :

1. `tests/test_recursive_types.cpp` — le fichier d'acceptation et le stress de §6, pour que le
   comportement récursif soit choisi.
2. Un test sur `is_sendable<T&&>`. Le rapport de mutation le dit : **0 mutant tué sur 4**, et
   `is_sendable<T&&> -> std::true_type` survit aux onze TU. Toute la règle `T&&` est non testée.
3. Un test qui épingle la sélection de mutex de `synchronized_value` **et** le fait qu'un vouch
   la change — parce que c'est le mécanisme de TC-4, et qu'aujourd'hui rien ne le rend visible.
