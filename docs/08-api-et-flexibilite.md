# 08 — API et flexibilité

**Verdict.** Le cœur de la bibliothèque est solide — les dix formes de contrebande structurelle que
l'équipe a tentées ont toutes été refusées, la marche profonde nomme le membre fautif sur soixante
niveaux, et `has_unreflectable_state` bloque `[&local]{}` qui aurait rendu le modèle trivialement
cassable. Le problème n'est pas la justesse, c'est le **rayon d'action des deux gardes
conservateurs** : `may_hijack_copy_move` refuse tout constructeur *template*, et le garde
copie/déplacement/destructeur refuse toute fonction spéciale écrite à la main. À eux deux ils
refusent **huit types-vocabulaire standard ordinaires**, **dix types de bibliothèque réalistes sur
dix**, et — je l'ai mesuré — **dix-sept des dix-huit templates de la liste blanche elle-même** : si
`std::vector<T>` passe, c'est uniquement parce que `allowed_std_wrappers.h` le nomme à la main. Il
n'y a ni `README` ni répertoire `examples/`, et le premier programme que quiconque écrira
(`launch_task([data]{ ... })`) ne compile pas. Enfin, un défaut **critique** : parce que les traits
sont lus réflexivement au point d'usage, une spécialisation écrite dans une seule unité de
traduction donne deux réponses de sûreté différentes dans un même programme, sans le moindre
diagnostic — et pour `synchronized_value<Cache>` cela donne deux `sizeof` (72 et 208), un lien
silencieux, et selon le niveau d'optimisation soit un `abort`, soit une lecture de mémoire
arbitraire. Sur ces vingt-deux constats, douze correctifs sont vérifiés suite verte, deux sont des
arbitrages à trancher, six n'ont volontairement pas de correctif, et j'en ai recompilé et rejoué
douze moi-même.

Rapports frères : [00 synthèse](./00-synthese.md) · [01 robustesse des traits](./01-robustesse-des-traits.md) ·
[02 robustesse des helpers](./02-robustesse-des-helpers.md) · [03 couverture de tests](./03-couverture-de-tests.md) ·
[04 diagnostics](./04-diagnostics.md) · [05 simplicité](./05-simplicite.md) ·
[06 performance de compilation](./06-performance-compilation.md) · [07 performance d'exécution](./07-performance-execution.md) ·
[09 méthodologie](./09-methodologie.md)

## Table des constats

| id | sévérité | sujet | correctif |
|---|---|---|---|
| [Q1](#41-le-cas-de-base--une-spécialisation-tardive) | critical | deux UT, deux réponses de sûreté, aucun diagnostic | suite verte (garde optionnel) |
| [TC-4](#42-le-cas-vérifié-par-le-lead--deux-tailles-pour-une-classe) | high | le vouch change `sizeof(synchronized_value<T>)` | **suite rouge — arbitrage** |
| [TC-3](#43-la-même-divergence-sans-écrire-la-moindre-spécialisation) | high | `is_defaulted` bascule sur un `= default` hors-ligne | suite verte |
| [ADV-05](#43-la-même-divergence-sans-écrire-la-moindre-spécialisation) | high | un type incomplet fige `false` pour toute l'UT | aucun (limite inhérente) |
| [Q4](#43-la-même-divergence-sans-écrire-la-moindre-spécialisation) | high | l'ordre des `#include` est porteur de sens | suite verte |
| [Q3](#2-le-rayon-daction-des-gardes) | high | 8 types std + 10 types utilisateurs sur 10 refusés | suite verte |
| [TLS-05](#21-le-tableau-complet-de-ce-qui-est-refusé) | high | vocabulaire de valeurs std non *sendable* | suite verte |
| [TLS-06](#21-le-tableau-complet-de-ce-qui-est-refusé) | high | `thread`, `future`, `promise`, `filesystem::path` refusés | suite verte |
| [TLS-04](#21-le-tableau-complet-de-ce-qui-est-refusé) | high | aucune primitive de synchro sauf `std::atomic` | suite verte |
| [TC-7](#21-le-tableau-complet-de-ce-qui-est-refusé) | high | le type gardé par mutex ne passe pas la marche const | suite verte |
| [TC-8](#23-larbitrage--rendre-le-prédicat-exact) | medium | `may_hijack_copy_move` trop large | **suite rouge — arbitrage** |
| [ADV-08](#13-le-programme-que-lon-écrit-vraiment) | medium | toute lambda capturante refusée | aucun (aveuglement de GCC) |
| [Q6](#31-ouvrir-la-liste-blanche-de-18-entrées-q6) | medium | liste blanche non extensible | suite verte |
| [Q7](#32-le-mutex-figé-de-synchronized_value-q7) | medium | mutex figé dans `synchronized_value` | suite verte |
| [Q9](#33-les-limites-de-la-macro-q9) | medium | la macro inutilisable là où on l'écrit | aucun pour le mécanisme |
| [E1](#52-copy_on_write-na-aucun-canal-de-publication-e1) | medium | `copy_on_write` ne publie jamais | aucun (documentation) |
| [E3](#54-les-manques-que-je-ne-comblerais-pas) | medium | pas de tranche de conteneur sans copie | aucun (règle assumée) |
| [E4](#53-synchronized_value-na-pas-de-verrou-multiple-e4) | medium | pas de verrou multiple, `scoped_lock` piège | *proposé et vérifié ici* |
| [SV-07](#54-les-manques-que-je-ne-comblerais-pas) | low | `[[nodiscard]]` attrape 2 fuites sur 4 | suite verte |
| [SV-08](#54-les-manques-que-je-ne-comblerais-pas) | low | pas d'initialisation par accolades | suite verte |
| [Q10](#15-quatre-orthographes-pour-une-question-et-un-concept-manquant) | low | pas de concept `synchronizable` | *vérifié ici, suite verte* |
| [ADV-11](#21-le-tableau-complet-de-ce-qui-est-refusé) | low | `mutex`, `latch`, `thread::id` refusés | non vérifié (décision de politique) |

---

## 1. La première heure

### 1.1 Il n'y a rien à lire

```
$ git ls-files
.gitignore
CLAUDE.md
CMakeLists.txt
Task.md
include/threadsafe/details/allowed_std_wrappers.h
include/threadsafe/details/asynchronous_task_launcher.h
include/threadsafe/details/copy_on_write.h
include/threadsafe/details/lifetime_aware.h
include/threadsafe/details/sendable.h
include/threadsafe/details/smart_pointers.h
include/threadsafe/details/synchronizable.h
include/threadsafe/details/synchronizable_base.h
include/threadsafe/details/synchronized_value.h
include/threadsafe/details/utils.h
include/threadsafe/details/vocabulary.h
include/threadsafe/threadsafe.h
tests/CMakeLists.txt
tests/test_asynchronous_task_launcher.cpp
tests/test_containers.cpp
tests/test_copy_on_write.cpp
tests/test_deferred_specialization.cpp
tests/test_diagnostics.cpp
tests/test_lifetime_aware.cpp
tests/test_sendable.cpp
tests/test_smart_pointers.cpp
tests/test_soundness_regressions.cpp
tests/test_synchronizable.cpp
tests/test_synchronized_value.cpp
```

Vérifié : **pas de `README`, pas de répertoire `examples/`**. `CLAUDE.md` décrit l'architecture des
traits mais ne montre aucun programme. Le seul code exécutable du dépôt est le corpus de tests, qui
est fait de `static_assert` : on ne peut pas le lire comme un exemple d'usage, seulement comme une
liste de réponses attendues.

Pour une bibliothèque destinée à une conférence internationale, c'est le manque le plus rentable à
combler, et il ne coûte rien à corriger : *un* fichier de vingt lignes.

### 1.2 Le plus petit programme complet

Voici le programme minimal qui utilise réellement la bibliothèque. Compilé et exécuté :

```cpp
// hello.cpp
#include <threadsafe/threadsafe.h>

#include <print>
#include <string>

struct greet_task {
    void operator()(std::string who) const { std::println("hello {}", who); }
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(greet_task{}, std::string{"conference"});
}
```

```
$ g++-16 -std=c++26 -freflection -I<include> -O1 -pthread hello.cpp -o hello && ./hello
hello conference
exit=0
```

C'est le programme qui devrait ouvrir le `README`. Notez ce qu'il ne contient pas : aucune
annotation, aucune spécialisation, aucun `static_assert`. Quand les types coopèrent, l'expérience
est sans friction — c'est ce que l'équipe a également constaté sur `ergo/01_hello.cpp` (une lambda
sans capture prenant un `std::vector<int>` et un `std::string`).

### 1.3 Le programme que l'on écrit vraiment

Personne n'écrit un foncteur nommé pour son premier programme de threads. On écrit une lambda :

```cpp
// hello_lambda.cpp
#include <threadsafe/threadsafe.h>

#include <print>
#include <string>

int main() {
    std::string who = "conference";
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([who] { std::println("hello {}", who); });
}
```

```
$ g++-16 -std=c++26 -freflection -I<include> -fsyntax-only hello_lambda.cpp
.../asynchronous_task_launcher.h:99:5: error: uncaught exception of type 'std::meta::exception';
  'what()': 'main()::<lambda()> holds state reflection cannot see (a closure type with captures);
             specialize is_sendable to state the intent'
```

**Le refus est correct et il n'y a rien à corriger dans la bibliothèque** (constat ADV-08). GCC 16
rapporte `nonstatic_data_members_of` vide pour un type de fermeture de 4 octets qui a capturé un
`int` par valeur : les captures sont littéralement invisibles à la réflexion. Sans le garde,
`[&local]{ return local; }` serait déclaré *sendable* et le modèle entier serait faux. L'auteur a
choisi le bon côté de l'arbitrage.

Ce qui est corrigeable, c'est le **conseil**. « specialize is_sendable » est faux à deux titres :

1. **Pour une lambda locale, il n'y a rien à spécialiser.** Le type de fermeture est local à `main`,
   et une spécialisation explicite doit être écrite à portée d'espace de noms. Il n'existe *aucune*
   façon d'appliquer le conseil.
2. **Même quand on peut, `is_sendable` ne suffit pas.** J'ai vouché une lambda de portée d'espace de
   noms et j'ai obtenu un second refus :

```
'what()': '{anonymous}::<lambda()> holds state reflection cannot see (a closure type with captures);
           specialize is_lifetime_aware to state the intent'
```

La recette complète, compilée et exécutée :

```cpp
// lambda_vouch2.cpp
#include <threadsafe/threadsafe.h>

#include <print>
#include <string>
#include <type_traits>

namespace {
auto owning_body = [payload = std::string{"payload"}] {
    std::println("hello {}", payload);
};
}

template <>
struct threadsafe::is_sendable<decltype(owning_body)> : std::true_type {};
template <>
struct threadsafe::is_lifetime_aware<decltype(owning_body)> : std::true_type {};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(owning_body);
}
```

```
$ ./lv2
hello payload
exit=0
```

Deux spécialisations, une lambda hissée en portée d'espace de noms, et un `decltype`. À comparer au
foncteur nommé, qui passe **sans aucune annotation** :

```cpp
// functor.cpp
#include <threadsafe/threadsafe.h>

#include <print>
#include <string>

struct greet_body {
    std::string payload;
    void operator()() const { std::println("hello {}", payload); }
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(greet_body{"payload"});
}
```

```
$ ./fu
hello payload
exit=0
```

**Recommandation (challenge du besoin).** Ne rien ajouter au code. Écrire *une phrase* dans le
`README` et *une phrase* dans le message de diagnostic : « la réflexion ne voit pas les captures
d'une lambda ; écrivez le callable comme une struct nommée à membres explicites ». Le foncteur nommé
est de toute façon le meilleur véhicule pédagogique : il rend les captures visibles à l'auditoire
autant qu'au compilateur, et c'est exactement le propos de la conférence. La correction du message
proposée par l'équipe (constat ADV-08) n'a **pas** été passée en régression ; elle ne change aucun
comportement, seulement du texte.

### 1.4 Le chemin de secours quand un type ordinaire est refusé

Voici un type légitime tel qu'on en trouve dans n'importe quelle base de code : écrit avant C++11,
règle de trois écrite à la main, puis un constructeur de déplacement ajouté. Il **possède** tout ce
qu'il contient ; rien n'est partagé.

```cpp
// legacy.h
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace acme {

class ReportBuffer {
public:
    ReportBuffer() = default;
    ReportBuffer(const ReportBuffer& other)
        : title_(other.title_), samples_(other.samples_) {}
    ReportBuffer(ReportBuffer&& other) noexcept
        : title_(std::move(other.title_)), samples_(std::move(other.samples_)) {}
    ReportBuffer& operator=(const ReportBuffer& other) {
        title_ = other.title_;
        samples_ = other.samples_;
        return *this;
    }
    ~ReportBuffer() { samples_.clear(); }

    void append(double sample) { samples_.push_back(sample); }
    std::size_t size() const { return samples_.size(); }

private:
    std::string title_;
    std::vector<double> samples_;
};

}
```

```cpp
// legacy0.cpp
#include <threadsafe/threadsafe.h>

#include "legacy.h"

struct publish_task {
    void operator()(acme::ReportBuffer report) const { (void)report.size(); }
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(publish_task{}, acme::ReportBuffer{});
}
```

Le refus, verbatim :

```
.../asynchronous_task_launcher.h:99:5: error: uncaught exception of type 'std::meta::exception';
  'what()': 'acme::ReportBuffer has a user-written copy, move or destructor — or a template that
             may be selected as one — which can share state the members do not show;
             specialize is_sendable to state the intent'
```

Et l'état exact des quatre réponses, mesuré :

```
is_sendable                 = false
is_synchronizable           = false
is_synchronizable<const>    = false
is_lifetime_aware           = true
```

Notez l'asymétrie : `is_lifetime_aware` répond **vrai** sur le même type, parce que ce trait
n'exécute pas le garde copie/déplacement. Deux traits de la même bibliothèque, deux conclusions
opposées sur `acme::ReportBuffer`. C'est le même désaccord que l'équipe a relevé sur `std::stack<int>`
(constat TLS-05), et c'est un signal en soi : le garde n'est pas une propriété du type, c'est une
propriété de *ce trait-là*.

#### Les trois voies de sortie, classées

**Route A — supprimer les fonctions spéciales écrites à la main. Recommandée.**

Chacune faisait exactement ce que le compilateur aurait généré. En les supprimant, le type garde son
comportement et la marche structurelle voit à travers lui. Compilé et exécuté :

```cpp
// legacyA.cpp
#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <string>
#include <vector>

namespace acme {

class ReportBuffer {
public:
    void append(double sample) { samples_.push_back(sample); }
    std::size_t size() const { return samples_.size(); }

private:
    std::string title_;
    std::vector<double> samples_;
};

}

struct publish_task {
    void operator()(acme::ReportBuffer report) const { (void)report.size(); }
};

static_assert(threadsafe::is_sendable_v<acme::ReportBuffer>);
static_assert(threadsafe::is_lifetime_aware_v<acme::ReportBuffer>);
static_assert(threadsafe::is_synchronizable_v<const acme::ReportBuffer>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(publish_task{}, acme::ReportBuffer{});
}
```

`exit=0`. Les trois `static_assert` tiennent. **C'est la seule route qui ne demande à personne de
faire confiance à personne** : la réponse reste déduite de la structure, elle reste vraie si le type
change, et elle est vraie dans toutes les unités de traduction. C'est aussi la seule qui améliore le
code d'origine (la règle de zéro).

**Route B — spécialiser le trait. Acceptable, avec un piège d'emplacement.**

Une seule spécialisation suffit ici, parce que `is_lifetime_aware` répond déjà vrai. Compilé et
exécuté :

```cpp
// legacyBmin.cpp
#include <threadsafe/threadsafe.h>

#include "legacy.h"

#include <type_traits>

template <>
struct threadsafe::is_sendable<acme::ReportBuffer> : std::true_type {};

struct publish_task {
    void operator()(acme::ReportBuffer report) const { (void)report.size(); }
};

static_assert(threadsafe::is_sendable_v<acme::ReportBuffer>);
static_assert(threadsafe::is_lifetime_aware_v<acme::ReportBuffer>);
static_assert(!threadsafe::is_synchronizable_v<acme::ReportBuffer>);
static_assert(!threadsafe::is_synchronizable_v<const acme::ReportBuffer>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(publish_task{}, acme::ReportBuffer{});
}
```

`exit=0`. La spécialisation doit être écrite **à portée globale, nom qualifié** — `[temp.expl.spec]`
exige qu'elle apparaisse dans un espace de noms englobant `threadsafe`. Et elle doit être écrite dans
un en-tête que *toutes* les unités de traduction voient : dans un seul `.cpp`, c'est le piège de la
[partie 4](#4-le-piège-odr). Si l'on veut en plus que la lecture concurrente soit permise (et donc
que `synchronized_value` choisisse un `std::shared_mutex`), il faut une deuxième spécialisation :

```cpp
template <>
struct threadsafe::is_synchronizable<const acme::ReportBuffer> : std::true_type {};
```

**Route C — la macro. Déconseillée pour ce cas.**

```cpp
// legacyC.cpp
#include <threadsafe/threadsafe.h>

#include "legacy.h"

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(acme::ReportBuffer);

struct publish_task {
    void operator()(acme::ReportBuffer report) const { (void)report.size(); }
};

static_assert(threadsafe::is_sendable_v<acme::ReportBuffer>);
static_assert(threadsafe::is_synchronizable_v<acme::ReportBuffer>);
static_assert(threadsafe::is_synchronizable_v<const acme::ReportBuffer>);
static_assert(threadsafe::is_sendable_v<acme::ReportBuffer&>);
static_assert(threadsafe::is_lifetime_aware_v<acme::ReportBuffer>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(publish_task{}, acme::ReportBuffer{});
}
```

`exit=0` — et c'est le problème. Les cinq `static_assert` passent. La macro répond à une question
*beaucoup plus forte* que celle qui a été posée : `sendable.h` court-circuite sur
`is_synchronizable_type(type)`, donc voucher « synchronizable » rend le type *sendable*, mais aussi
`const`-synchronizable, mais aussi partageable par référence (`is_sendable<T&>` = `is_synchronizable<T>`).
On voulait « je peux déplacer ce tampon vers un autre thread » ; on a déclaré « plusieurs threads
peuvent l'utiliser simultanément », ce qui est **faux** pour ce type. Il n'existe pas de
`THREADSAFE_UNSAFE_ASSERT_SENDABLE`, et c'est précisément pour cela que la macro est le mauvais
outil ici.

| rang | route | lignes | ce qu'on promet | vérifié dans les autres UT ? | recommandation |
|---|---|---|---|---|---|
| 1 | supprimer les fonctions spéciales | −8 | rien, tout reste déduit | oui, structurellement | **par défaut** |
| 2 | `template <> struct threadsafe::is_sendable<T> : std::true_type {};` | +2 | exactement ce qu'on voulait | **non** — à mettre dans un en-tête partagé | si A est impossible |
| 3 | `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(T)` | +1 | **bien plus** que ce qu'on voulait | non | jamais pour un besoin *sendable* |

### 1.5 Quatre orthographes pour une question, et un concept manquant

La bibliothèque offre quatre façons de poser la question *sendable*, quatre pour *lifetime*, et
**trois** pour *synchronizable*. Vérifié :

```cpp
// spellings.cpp
#include <threadsafe/threadsafe.h>
#include <vector>

static_assert(threadsafe::is_sendable<std::vector<int>>::value);
static_assert(threadsafe::is_sendable_v<std::vector<int>>);
static_assert(threadsafe::sendable<std::vector<int>>);
static_assert(threadsafe::is_sendable_type(^^std::vector<int>));

static_assert(threadsafe::is_lifetime_aware<std::vector<int>>::value);
static_assert(threadsafe::is_lifetime_aware_v<std::vector<int>>);
static_assert(threadsafe::lifetime_aware<std::vector<int>>);
static_assert(threadsafe::is_lifetime_aware_type(^^std::vector<int>));

static_assert(threadsafe::is_synchronizable<const std::vector<int>>::value);
static_assert(threadsafe::is_synchronizable_v<const std::vector<int>>);
static_assert(threadsafe::is_synchronizable_type(^^const std::vector<int>));
static_assert(threadsafe::synchronizable<const std::vector<int>>);
int main() {}
```

```
spellings.cpp:17:27: error: 'synchronizable' is not a member of 'threadsafe';
  did you mean 'is_synchronizable'?
```

Les onze premières assertions tiennent ; la douzième ne compile pas (constat Q10).

**Ce qui devrait être l'API publique.** Trois orthographes sur quatre s'adressent à trois publics
différents et méritent toutes de rester :

| orthographe | public | statut recommandé |
|---|---|---|
| `is_sendable<T>` (la classe) | celui qui **répond** — c'est le point de spécialisation | **public**, documenté comme *le* point d'extension |
| `is_sendable_v<T>` | celui qui **demande** dans du code ordinaire | **public**, l'orthographe par défaut |
| `sendable<T>` (le concept) | celui qui **contraint** une interface | **public**, la forme à montrer sur slide |
| `is_sendable_type(^^T)` | celui qui écrit du code du côté réflexion | **détail** — à déplacer dans `threadsafe::detail` |

`is_sendable_type` n'a de sens que pour du code qui manipule déjà des `std::meta::info` ; hors de la
bibliothèque, personne ne devrait en avoir besoin. Le laisser dans `threadsafe::` fait passer l'API
de trois à quatre noms pour une seule question, ce qui est exactement le genre d'asymétrie qu'un
auditoire remarque.

**L'ajout complet d'une ligne pour Q10** — je l'ai appliqué et vérifié moi-même :

```cpp
// ==== ÉDITION : include/threadsafe/details/synchronizable_base.h ====
// Immédiatement après is_synchronizable_v, dans namespace threadsafe :

template <class T>
concept synchronizable = is_synchronizable_v<T>;
```

Résultat de ma vérification (l'équipe avait marqué ce correctif `not-checked` ; je l'ai passé) :

```
Q10 OK                                  <- spellings.cpp compile entièrement
PASS test_asynchronous_task_launcher.cpp
PASS test_containers.cpp
PASS test_copy_on_write.cpp
PASS test_deferred_specialization.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
PASS test_sendable.cpp
PASS test_smart_pointers.cpp
PASS test_soundness_regressions.cpp
PASS test_synchronizable.cpp
PASS test_synchronized_value.cpp
```

**11 UT sur 11 vertes.** C'est trois lignes, ça débloque
`void consume(threadsafe::synchronizable auto&)`, et ça supprime la seule asymétrie de nommage de
l'API. À appliquer.

---

## 2. Le rayon d'action des gardes

C'est le vrai frein à l'adoption, et il se quantifie durement.

Deux gardes, tous deux dans `include/threadsafe/details/utils.h` :

```cpp
inline consteval bool may_hijack_copy_move(std::meta::info member) {
    return std::meta::is_constructor_template(member)
        || (std::meta::is_operator_function_template(member)
            && std::meta::operator_of(member) == std::meta::op_equals);
}

inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (may_hijack_copy_move(member))
            return false;

        if (!is_copy_move_destroy_member(member))
            continue;

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return false;
    }
    return true;
}
```

Le premier refuse **tout** constructeur template et **tout** `operator=` template, quels que soient
son arité et ses contraintes. Le second refuse **toute** fonction spéciale écrite à la main.

### 2.1 Le tableau complet de ce qui est refusé

Mesuré contre les en-têtes actuels, `g++-16 -std=c++26 -freflection` :

```
type                                     send   csync  life
std::string                              true   true   true
std::vector<int>                         true   true   true
std::map<int,string>                     true   true   true
std::array<int,4>                        true   true   true
std::pair<int,double>                    true   true   true
std::tuple<int,string>                   true   true   true
std::optional<int>                       true   true   true
std::variant<int,string>                 true   true   true
std::unique_ptr<int>                     true   false  true
--- refusés ---
std::chrono::milliseconds                false  false  true
std::chrono::steady_clock::time_point    false  false  true
std::complex<double>                     false  false  true
std::bitset<8>                           false  false  true
std::expected<int,string>                false  false  true
std::flat_map<int,string>                false  false  true
std::stack<int>                          false  false  true
std::queue<int>                          false  false  true
std::priority_queue<int>                 false  false  true
std::filesystem::path                    false  false  false
std::thread                              false  false  false
std::jthread                             false  false  false
std::future<int>                         false  false  false
std::promise<int>                        false  false  false
std::thread::id                          false  false  false
std::function<void()>                    false  false  false
std::move_only_function<void()>          false  false  false
```

Le tableau exhaustif des refus, par cause :

| famille | types | cause | constat | correctif |
|---|---|---|---|---|
| vocabulaire de valeurs | `chrono::duration`, `chrono::time_point`, `complex`, `bitset`, `expected` | constructeur template | Q3 / TLS-05 | liste blanche élargie, **suite verte** |
| adaptateurs de conteneurs | `stack`, `queue`, `priority_queue`, `flat_map`, `flat_set` | constructeur template | Q3 / TLS-05 | liste blanche élargie, **suite verte** |
| poignées de la norme | `thread`, `jthread`, `future`, `promise` | fonctions spéciales écrites | TLS-06 | spécialisations dans `vocabulary.h`, **suite verte** |
| pimpl | `filesystem::path` | pointeur vers type incomplet | TLS-06 | spécialisation, **suite verte** |
| primitives de synchro | `mutex` (×6), `condition_variable` (×2), `latch`, `barrier`, `counting_semaphore`, `atomic_flag`, `once_flag`, `atomic_ref` | `is_synchronizable` est opt-in et ne les nomme pas | TLS-04 / TC-7 / ADV-11 | spécialisations, **suite verte** (TLS-04, TC-7) / **non vérifié** (ADV-11) |
| identité | `thread::id` | libstdc++ le stocke comme `_opaque_pthread_t*` | ADV-11 | spécialisation, **non vérifié** |
| callables effacés | `function`, `move_only_function`, `bind`, lambdas capturantes | état invisible à la réflexion | ADV-08 | **aucun — refus correct** |

Le refus de `std::chrono::milliseconds` est celui qui coûtera le plus cher en pratique. Le
constructeur sur lequel le garde trébuche est

```cpp
template <class Rep2, class Period2>
constexpr duration(const duration<Rep2, Period2>&);
```

qui prend un `const&` et ne peut donc **pas** détourner une copie — le commentaire de `utils.h` le
dit lui-même — mais `parameters_of` ne s'applique pas à un template, donc il est bloqué quand même.
Conséquence : `launcher.launch_task(&poll_for, 100ms)` ne compile pas.

### 2.2 Dix types de bibliothèque réalistes sur dix

J'ai écrit les dix formes qu'une base de code réelle possède déjà — petit vecteur, typedef fort,
somme à deux branches, `Result<T,E>`, sosie d'`expected`, union taguée, quantité physique,
constructeur de chaînes, callable à stockage inline, pimpl par valeur. Le fichier complet :

```cpp
// ten_types.h
#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace corpus {

// 1. un vecteur à capacité fixe avec constructeur par paire d'itérateurs
template <class Element, std::size_t Capacity>
class small_vector {
public:
    small_vector() = default;
    template <class Iterator>
    small_vector(Iterator first, Iterator last) {
        for (; first != last; ++first) storage_[size_++] = *first;
    }
    std::size_t size() const { return size_; }

private:
    Element storage_[Capacity]{};
    std::size_t size_ = 0;
};

// 2. un typedef fort
template <class Underlying, class Tag>
struct strong {
    Underlying value;
    template <class Other>
        requires std::is_convertible_v<Other, Underlying>
    explicit constexpr strong(Other other) : value(static_cast<Underlying>(other)) {}
};
struct meters_tag {};

// 3. une somme à deux alternatives
template <class Left, class Right>
class either {
public:
    template <class Alternative>
    either(Alternative alternative) : is_left_(true) {
        ::new (storage_) Left(std::move(alternative));
    }
    ~either() {}

private:
    alignas(Left) alignas(Right)
        unsigned char storage_[sizeof(Left) > sizeof(Right) ? sizeof(Left)
                                                            : sizeof(Right)];
    bool is_left_;
};

// 4. un Result<T, E>
template <class Value, class Error>
class result {
public:
    result(Value value) : value_(std::move(value)), has_value_(true) {}
    template <class OtherError>
    result(OtherError error) : error_(std::move(error)), has_value_(false) {}

private:
    Value value_{};
    Error error_{};
    bool has_value_ = false;
};

// 5. un sosie de std::expected
template <class Value, class Error>
class expected_like {
public:
    expected_like() = default;
    template <class Other>
    expected_like(const expected_like<Other, Error>& other) : value_(other.value()) {}
    const Value& value() const { return value_; }

private:
    Value value_{};
    Error error_{};
};

// 6. une union taguée
template <class... Alternatives>
class tagged_union {
public:
    tagged_union() = default;
    template <class Alternative>
    explicit tagged_union(Alternative) : index_(1) {}

private:
    unsigned index_ = 0;
    alignas(Alternatives...) unsigned char storage_[
        std::max({sizeof(Alternatives)...})]{};
};

// 7. un type d'unités physiques
template <class Representation, int MetersExponent>
struct quantity {
    Representation value;
    template <class Number>
        requires std::is_arithmetic_v<Number>
    constexpr quantity(Number number)
        : value(static_cast<Representation>(number)) {}
};

// 8. un constructeur de chaînes à constructeur variadique
class string_builder {
public:
    string_builder() = default;
    template <class... Pieces>
    explicit string_builder(Pieces&&... pieces) {
        (buffer_.append(std::forward<Pieces>(pieces)), ...);
    }

private:
    std::string buffer_;
};

// 9. un callable à stockage inline
template <class Signature, std::size_t Capacity = 32>
class small_function;
template <class Result, class... Arguments, std::size_t Capacity>
class small_function<Result(Arguments...), Capacity> {
public:
    small_function() = default;
    template <class Callable>
    small_function(Callable callable) {
        ::new (storage_) Callable(std::move(callable));
    }

private:
    alignas(std::max_align_t) unsigned char storage_[Capacity]{};
};

// 10. un pimpl par valeur derrière un unique_ptr
class settings {
public:
    settings();
    settings(const settings& other);
    settings& operator=(const settings& other);
    ~settings();

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}
```

Résultat mesuré contre les en-têtes actuels :

```
1  small_vector<int,8>             send=false constsync=false lifetime=true
2  strong<double,meters>           send=false constsync=false lifetime=true
3  either<int,double>              send=false constsync=false lifetime=true
4  result<int,string>              send=false constsync=false lifetime=true
5  expected_like<int,string>       send=false constsync=false lifetime=true
6  tagged_union<int,string>        send=false constsync=false lifetime=true
7  quantity<double,1>              send=false constsync=false lifetime=true
8  string_builder                  send=false constsync=false lifetime=true
9  small_function<void()>          send=false constsync=false lifetime=true
10 settings (pimpl)                send=false constsync=false lifetime=false
```

**Dix sur dix refusés.** (Une nuance par rapport aux chiffres de l'équipe : sur ma version du pimpl,
`is_lifetime_aware` répond `false` et non `true` — l'écart vient de la façon dont l'`implementation`
incomplète est déclarée. Je rapporte ma mesure.)

### 2.2bis Dix-sept templates sur dix-huit de la liste blanche seraient refusés

Voici le chiffre qui devrait ouvrir la discussion. `is_sendable<T>` pour un
`detail::std_wrapper` court-circuite la marche structurelle ; on peut donc interroger la marche
directement et voir ce qu'elle *aurait* répondu :

```cpp
// allow18.cpp
#include <threadsafe/threadsafe.h>

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <optional>
#include <print>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#define ROW(NAME, ...)                                                        \
    std::println("{:<34} structural={:<6} trait={}", NAME,                     \
                 threadsafe::detail::default_is_sendable(^^__VA_ARGS__),       \
                 threadsafe::is_sendable_v<__VA_ARGS__>)

int main() {
    ROW("std::vector<int>", std::vector<int>);
    ROW("std::deque<int>", std::deque<int>);
    ROW("std::list<int>", std::list<int>);
    ROW("std::forward_list<int>", std::forward_list<int>);
    ROW("std::basic_string<char>", std::basic_string<char>);
    ROW("std::map<int,int>", std::map<int, int>);
    ROW("std::multimap<int,int>", std::multimap<int, int>);
    ROW("std::set<int>", std::set<int>);
    ROW("std::multiset<int>", std::multiset<int>);
    ROW("std::unordered_map<int,int>", std::unordered_map<int, int>);
    ROW("std::unordered_multimap<int,int>", std::unordered_multimap<int, int>);
    ROW("std::unordered_set<int>", std::unordered_set<int>);
    ROW("std::unordered_multiset<int>", std::unordered_multiset<int>);
    ROW("std::pair<int,int>", std::pair<int, int>);
    ROW("std::tuple<int,int>", std::tuple<int, int>);
    ROW("std::optional<int>", std::optional<int>);
    ROW("std::variant<int,double>", std::variant<int, double>);
    ROW("std::array<int,4>", std::array<int, 4>);
}
```

```
std::vector<int>                   structural=false  trait=true
std::deque<int>                    structural=false  trait=true
std::list<int>                     structural=false  trait=true
std::forward_list<int>             structural=false  trait=true
std::basic_string<char>            structural=false  trait=true
std::map<int,int>                  structural=false  trait=true
std::multimap<int,int>             structural=false  trait=true
std::set<int>                      structural=false  trait=true
std::multiset<int>                 structural=false  trait=true
std::unordered_map<int,int>        structural=false  trait=true
std::unordered_multimap<int,int>   structural=false  trait=true
std::unordered_set<int>            structural=false  trait=true
std::unordered_multiset<int>       structural=false  trait=true
std::pair<int,int>                 structural=false  trait=true
std::tuple<int,int>                structural=false  trait=true
std::optional<int>                 structural=false  trait=true
std::variant<int,double>           structural=false  trait=true
std::array<int,4>                  structural=true   trait=true
```

Il faut le dire tel quel, sur scène comme dans la documentation :

> **Sur les dix-huit templates de la liste blanche, dix-sept seraient refusés par le défaut
> structurel. Seul `std::array` passe tout seul. `std::string` et `std::vector` ne passent que
> parce que `allowed_std_wrappers.h` les nomme à la main.**

Ce n'est pas une critique de la liste blanche — la liste blanche est **la bonne réponse**, et le
commentaire qui l'accompagne dans `allowed_std_wrappers.h` explique correctement pourquoi elle est
une promesse et non une déduction. C'est une critique de la *proportion* : le défaut structurel,
présenté comme le mécanisme principal, refuse la quasi-totalité de la bibliothèque standard, et une
liste explicite de dix-huit entrées la rattrape. Un auditoire qui voit la liste blanche après avoir
vu la marche structurelle en tirera la bonne conclusion tout seul ; autant la lui donner d'emblée.

Le correctif de Q3/TLS-05/TLS-06/TLS-04/TC-7 consiste précisément à **allonger la liste**, et il est
vérifié suite verte par l'équipe. C'est cohérent avec le design, mais cela déplace le problème
plutôt que de le résoudre : la liste passe de 18 à ~30 entrées et les types *de l'utilisateur*
restent dehors (voir [partie 3](#3-extensibilité)).

### 2.3 L'arbitrage : rendre le prédicat exact

Le commentaire de `utils.h` affirme aujourd'hui :

> *Which shape it is cannot be told from here: parameters_of rejects a template, so an arity or a
> constraint that makes hijacking impossible — `T(It, It)`, or a `requires !same_as<remove_cvref_t<U>, T>`
> — is indistinguishable from a greedy forwarding constructor.*

**C'est faux**, et c'est un artefact pédagogique gênant : il énonce une limite de la réflexion C++26
qui n'existe pas. L'équipe l'a montré de deux façons (`substitute()` + `parameters_of` distingue les
quatre formes ; et les traits de trivialité du langage répondent directement à la question). J'ai
reconstruit et mesuré la seconde.

Le candidat, appliqué à `include/threadsafe/details/utils.h`, en remplacement complet de
`has_only_default_copy_move_destroy` :

```cpp
// Toute copie et tout déplacement de `T` sont générés par le compilateur si tous
// sont *triviaux* — une fonction fournie par l'utilisateur n'est jamais triviale,
// et un template sélectionné pour une copie est fourni par l'utilisateur. Les
// deux formes détournantes lient `T&` et `const T&&`, les deux formes d'argument
// que std::is_trivially_copyable_v ne couvre pas, elles sont donc écrites
// explicitement. Posée dans un contexte non évalué, la question n'instancie
// aucune définition — contrairement à can_substitute, qui instancie le CORPS du
// template et transforme un « false » en erreur dure.
template <class T>
constexpr bool copy_move_is_compiler_generated =
    std::is_trivially_copyable_v<T>
    && std::is_trivially_constructible_v<T, T &>
    && std::is_trivially_constructible_v<T, const T &&>
    && std::is_trivially_assignable_v<T &, T &>
    && std::is_trivially_assignable_v<T &, const T &&>;

inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();

    // Un type trivialement copiable n'a de code utilisateur dans aucune copie ni
    // aucun déplacement : aucun template n'a donc été sélectionné pour l'un
    // d'eux, quel que soit le nombre de templates qu'il déclare.
    const bool templates_cannot_have_won =
        trait_value(^^copy_move_is_compiler_generated, type);

    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (!templates_cannot_have_won && may_hijack_copy_move(member))
            return false;

        if (!is_copy_move_destroy_member(member))
            continue;

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return false;
    }
    return true;
}
```

**Ce que ça donne, mesuré par moi.** J'ai comparé le prédicat à la vérité-terrain du langage sur
neuf formes — `hijacks<T>` est `!std::is_trivially_constructible_v<T, T&>`, c'est-à-dire « copier
depuis une lvalue non-const exécute du code utilisateur » :

```cpp
// groundtruth.cpp
#include <threadsafe/threadsafe.h>
#include <concepts>
#include <print>
#include <type_traits>

struct Plain            { int x = 0; };
struct Greedy           { int x = 0; Greedy() = default; template <class U> Greedy(U&&) : x(1) {} };
struct GreedyLvalue     { int x = 0; GreedyLvalue() = default; template <class U> GreedyLvalue(U&) : x(1) {} };
struct VariadicForward  { int x = 0; VariadicForward() = default; template <class... A> VariadicForward(A&&...) : x(1) {} };
struct HarmlessConstRef { int x = 0; HarmlessConstRef() = default; template <class U> HarmlessConstRef(const U& o) : x(o.x) {} };
struct ByValueTemplate  { int x = 0; ByValueTemplate() = default; template <class U> ByValueTemplate(U o) : x(o.x) {} };
struct GuardedForward   { int x = 0; GuardedForward() = default;
                          template <class U> requires(!std::same_as<std::remove_cvref_t<U>, GuardedForward>)
                          GuardedForward(U&& o) : x(o.x) {} };
struct IterPair         { int x = 0; IterPair() = default; template <class It> IterPair(It, It) : x(1) {} };
struct VariadicByValue  { int x = 0; VariadicByValue() = default; template <class... A> VariadicByValue(A...) : x(1) {} };

template <class T>
constexpr bool hijacks = !std::is_trivially_constructible_v<T, T&>;

#define ROW(T) std::println("{:<18} hijacks={:<6} trait={}", #T, hijacks<T>, threadsafe::is_sendable_v<T>)
int main() {
    ROW(Plain); ROW(Greedy); ROW(GreedyLvalue); ROW(VariadicForward);
    ROW(HarmlessConstRef); ROW(ByValueTemplate); ROW(GuardedForward);
    ROW(IterPair); ROW(VariadicByValue);
}
```

```
--- en-têtes actuels ---            --- candidat ---
Plain            hijacks=false  trait=true      Plain            hijacks=false  trait=true
Greedy           hijacks=true   trait=false     Greedy           hijacks=true   trait=false
GreedyLvalue     hijacks=true   trait=false     GreedyLvalue     hijacks=true   trait=false
VariadicForward  hijacks=true   trait=false     VariadicForward  hijacks=true   trait=false
HarmlessConstRef hijacks=false  trait=false     HarmlessConstRef hijacks=false  trait=true
ByValueTemplate  hijacks=false  trait=false     ByValueTemplate  hijacks=false  trait=true
GuardedForward   hijacks=false  trait=false     GuardedForward   hijacks=false  trait=true
IterPair         hijacks=false  trait=false     IterPair         hijacks=false  trait=true
VariadicByValue  hijacks=false  trait=false     VariadicByValue  hijacks=false  trait=true
```

**Le candidat est exact sur les neuf formes** (`trait == !hijacks` partout). Les en-têtes actuels se
trompent sur cinq — toujours dans le sens du refus, jamais dans le sens de l'acceptation, ce qui
confirme que le garde actuel est *sûr* mais grossier. Aucune régression de sûreté : `Greedy`,
`GreedyLvalue` et `VariadicForward` restent refusés, parce que
`std::is_trivially_constructible_v<T, T&>` est exactement la question du détournement.

**Ce que le candidat rachète, mesuré :**

```
--- vocabulaire std sous le candidat ---
std::chrono::milliseconds                true   true   true    <- rachetés
std::chrono::steady_clock::time_point    true   true   true    <- rachetés
std::complex<double>                     true   true   true    <- rachetés
std::bitset<8>                           true   true   true    <- rachetés
std::expected<int,string>                false  false  true    (pas trivialement copiable)
std::flat_map<int,string>                false  false  true
std::stack<int>                          false  false  true
std::queue<int>                          false  false  true
std::priority_queue<int>                 false  false  true

--- les dix types réalistes sous le candidat ---
1  small_vector<int,8>             send=true  constsync=true  lifetime=true   <- racheté
2  strong<double,meters>           send=true  constsync=true  lifetime=true   <- racheté
3  either<int,double>              send=false constsync=false lifetime=true
4  result<int,string>              send=false constsync=false lifetime=true
5  expected_like<int,string>       send=false constsync=false lifetime=true
6  tagged_union<int,string>        send=true  constsync=true  lifetime=true   <- racheté
7  quantity<double,1>              send=true  constsync=true  lifetime=true   <- racheté
8  string_builder                  send=false constsync=false lifetime=true
9  small_function<void()>          send=true  constsync=true  lifetime=true   <- racheté
10 settings (pimpl)                false  false  false
```

**4 des 8 types std rachetés, 5 des 10 types utilisateurs rachetés.** Les autres restent refusés
parce qu'ils ne sont pas trivialement copiables du tout (ils contiennent un `std::string`) —
c'est-à-dire qu'ils gardent le même faux refus qu'aujourd'hui, sans régression.

**Ce qui régresse.** Une assertion, une seule, et elle encode explicitement la limitation :

```
PASS test_asynchronous_task_launcher.cpp
PASS test_containers.cpp
PASS test_copy_on_write.cpp
PASS test_deferred_specialization.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
FAIL test_sendable.cpp
  tests/test_sendable.cpp:202:15: error: static assertion failed:
    is_sendable — parameters_of rejects a template, so a shape that could never hijack
    is indistinguishable from one that does
PASS test_smart_pointers.cpp
PASS test_soundness_regressions.cpp
PASS test_synchronizable.cpp
PASS test_synchronized_value.cpp
```

La ligne en cause :

```cpp
static_assert(!is_sendable_v<ConstRefCtorTemplate> && !is_sendable_v<GuardedForwardingCtor>,
              "is_sendable — parameters_of rejects a template, so a shape that "
              "could never hijack is indistinguishable from one that does");
```

Sous le candidat, `ConstRefCtorTemplate` **et** `GuardedForwardingCtor` deviennent tous deux
*sendable*, ce qui est la **bonne** réponse (la vérité-terrain dit `hijacks=false` pour les deux).
*Correction à la note de l'équipe :* le constat TC-8 annonce que `GuardedForwardingCtor` reste
refusé « parce qu'il n'est pas trivialement copiable » ; ce n'est pas ce que j'observe — la version
du corpus de tests ne contient qu'un `int x` et devient donc *sendable*. La formulation de l'équipe
décrit une variante contenant un `std::string`, que j'ai reproduite séparément et qui reste bien
refusée.

**Arbitrage, pas correctif prêt à poser.** Statut : `suite-regresses`. Les trois positions
défendables :

| position | conséquence |
|---|---|
| appliquer le candidat, réécrire la ligne 202 | +4 types std, +5 types utilisateurs, prédicat exact, commentaire de `utils.h` devenu vrai ; **une assertion de test à réécrire, qui documentait la limitation** |
| ne rien changer, corriger le commentaire | zéro risque ; la bibliothèque continue de refuser `std::complex<double>` et le commentaire cesse de mentir |
| élargir la liste blanche (Q3/TLS-05) sans toucher au garde | les types std sont réparés, les types utilisateurs restent dehors ; **suite verte** |

**Ma recommandation** : élargir la liste blanche *maintenant* (suite verte, zéro risque, répare la
bibliothèque standard), corriger le commentaire mensonger de `utils.h` *maintenant*, et garder le
prédicat exact pour une deuxième passe — parce que son bénéfice réel (5 types utilisateurs sur 10)
est inférieur à celui du point de personnalisation de la [partie 3](#31-ouvrir-la-liste-blanche-de-18-entrées-q6),
qui les rachète tous les dix pour une ligne par template.

Coût de compilation, mesuré (meilleur de 3, machine **non oisive** — mon plancher est à 732 ms là où
le lead mesure 655 ms sur `test_soundness_regressions`, donc ces écarts sont dans le bruit) :

| arbre | UT | temps |
|---|---|---|
| actuel | `test_containers.cpp` | 718 ms |
| candidat TC-8 | `test_containers.cpp` | 749 ms (+4 %) |
| actuel | `test_soundness_regressions.cpp` | 732 ms |
| point de personnalisation Q6 | `test_soundness_regressions.cpp` | 748 ms (+2 %) |

---

## 3. Extensibilité

### 3.1 Ouvrir la liste blanche de 18 entrées (Q6)

Aujourd'hui, `allowed_std_wrappers` est un `inline constexpr std::meta::info[]` dans
`threadsafe::detail`, lu par `is_allowed_std_wrapper`, consommé par
`template <class T> concept std_wrapper = is_allowed_std_wrapper(^^T);`. **On ne peut ni ajouter à un
tableau depuis l'extérieur, ni spécialiser un concept.** La seule route pour un `my_vector<T, Alloc>`
est d'écrire trois spécialisations partielles à la main, qui répètent trois fois la même récursion et
qu'il faut maintenir en phase :

```cpp
namespace threadsafe {
template <class T, class Alloc>
struct is_sendable<my_vector<T, Alloc>>
    : std::bool_constant<is_sendable_v<T> && is_sendable_v<Alloc>> {};

template <class T, class Alloc>
struct is_synchronizable<const my_vector<T, Alloc>>
    : std::bool_constant<is_synchronizable_v<const T>
                         && is_synchronizable_v<const Alloc>> {};

template <class T, class Alloc>
struct is_lifetime_aware<my_vector<T, Alloc>>
    : std::bool_constant<is_lifetime_aware_v<T> && is_lifetime_aware_v<Alloc>> {};
}
```

Le point de personnalisation complet — **je l'ai appliqué et vérifié moi-même** :

```cpp
// ==== ÉDITION : include/threadsafe/details/allowed_std_wrappers.h ====
// (a) en tête de `namespace threadsafe`, AVANT `namespace detail {` :

// La même promesse que la liste ci-dessous fait à propos de std::vector, ouverte
// à un template de classe qui vous appartient : une spécialisation de ce
// template n'est rien d'autre que ses arguments de type, tenus par valeur, sans
// état ni partage propres. Dites-le une fois, là où le template est déclaré, et
// les trois traits répondent pour toutes ses spécialisations.
//
// C'est une promesse, pas une déduction, exactement comme la liste : la
// réflexion ne distingue pas un conteneur d'un type qui cache du partage
// derrière les mêmes arguments. Écrivez-la là où le template est déclaré, avant
// que le moindre trait ne soit lu pour lui, sinon les unités de traduction se
// contrediront (voir partie 4).
template <class T>
struct is_transparent_wrapper : std::false_type {};

template <class T>
constexpr bool is_transparent_wrapper_v = is_transparent_wrapper<T>::value;

// (b) remplacer is_allowed_std_wrapper par la paire :

inline consteval bool is_listed_std_wrapper(std::meta::info type) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type)
        && std::ranges::contains(allowed_std_wrappers,
                                 std::meta::template_of(type));
}

inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
    return is_listed_std_wrapper(type)
        || trait_value(^^is_transparent_wrapper_v, std::meta::remove_cv(type));
}
```

L'utilisateur écrit alors **une** ligne au lieu de dix-huit. Le programme de vérification complet :

```cpp
// q6.cpp
#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

template <class T, class Alloc = std::allocator<T>>
class my_vector {
public:
    my_vector() = default;
    template <class It> my_vector(It first, It last);
private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
    Alloc allocator_;
};

// UNE ligne, au lieu de trois spécialisations partielles.
template <class T, class Alloc>
struct threadsafe::is_transparent_wrapper<my_vector<T, Alloc>> : std::true_type {};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

static_assert(is_sendable_v<my_vector<std::string>>);
static_assert(!is_sendable_v<my_vector<int*>>);
static_assert(is_synchronizable_v<const my_vector<int>>);
static_assert(!is_synchronizable_v<const my_vector<int*>>);
static_assert(is_lifetime_aware_v<my_vector<std::string>>);
static_assert(!is_lifetime_aware_v<my_vector<int*>>);
static_assert(is_sendable_v<const my_vector<int>>, "cv forwarding still works");
static_assert(is_sendable_v<std::vector<my_vector<int>>>, "nests inside std");
static_assert(!is_synchronizable_v<my_vector<int>>, "opt-in stays opt-in");
int main() {}
```

Mes résultats :

```
--- en-têtes actuels : 8 erreurs (aucune des 9 assertions ne peut tenir) ---
--- avec le point de personnalisation : compile proprement ---
PASS test_asynchronous_task_launcher.cpp
PASS test_containers.cpp
PASS test_copy_on_write.cpp
PASS test_deferred_specialization.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
PASS test_sendable.cpp
PASS test_smart_pointers.cpp
PASS test_soundness_regressions.cpp
PASS test_synchronizable.cpp
PASS test_synchronized_value.cpp
```

**Compile, 11 UT sur 11 vertes, coût de compilation +2 % (dans le bruit).** La propagation
fonctionne dans les deux sens (`my_vector<int*>` reste refusé), le *cv forwarding* tient,
l'imbrication dans `std::vector` tient, et l'opt-in reste opt-in. C'est, à mon avis, **le correctif
d'ergonomie le plus rentable du rapport** : il rachète les dix types réalistes de la partie 2 pour
une ligne chacun, alors que le candidat TC-8 n'en rachète que cinq et casse un test.

Et il rend la liste blanche honnête : elle cesse d'être un privilège réservé à `std::` et devient un
mécanisme nommé, ce qui est aussi une meilleure histoire pour une conférence — « voici la promesse,
la bibliothèque standard l'utilise dix-huit fois, vous pouvez l'utiliser aussi ».

### 3.2 Le mutex figé de `synchronized_value` (Q7)

```cpp
// sv_mutex.cpp — ce que l'on ne peut pas écrire
threadsafe::synchronized_value<std::vector<int>, spin_lock> values{};
```
```
sv_mutex.cpp:16:63: error: wrong number of template arguments (2, should be 1)
```

Le mutex est choisi par un `consteval get_mutex_type()` privé qui renvoie `^^std::shared_mutex` ou
`^^std::mutex`. Pas de `recursive_mutex` derrière une API réentrante existante, pas de `timed_mutex`,
pas de spinlock maison. La réponse actuelle est « copiez la classe ».

Le fichier de remplacement complet, **appliqué, compilé, exécuté et passé en régression par moi** :

```cpp
// ==== REMPLACEMENT COMPLET : include/threadsafe/details/synchronized_value.h ====
#pragma once

#include <concepts>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

namespace detail {

// Un mutex qui peut aussi être tenu par plusieurs lecteurs à la fois.
// std::shared_lock a besoin exactement de ces trois opérations : les demander,
// c'est demander si un shared_lock sur ce mutex est bien formé.
template <class Mutex>
concept shared_lockable = requires(Mutex& mutex) {
    mutex.lock_shared();
    mutex.unlock_shared();
    { mutex.try_lock_shared() } -> std::same_as<bool>;
};

// Les lecteurs ne paient un mutex partagé que s'il y a quelque chose à
// partager : un T qui n'est pas lisible depuis plusieurs threads à la fois doit
// être verrouillé exclusivement même en lecture, et la comptabilité
// lecteurs/rédacteurs n'est alors que du coût. Ceci n'est que le *défaut* ; le
// second paramètre de template le remplace.
template <class T>
using default_mutex_for =
    std::conditional_t<is_synchronizable_v<const T>, std::shared_mutex,
                       std::mutex>;

}

template <class T, class Mutex = detail::default_mutex_for<T>>
class synchronized_value;

template <class T, class Lock>
class value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    T& operator*() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");
    T* operator->() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");

    T& operator*() const& noexcept { return *value_; }
    T* operator->() const& noexcept { return value_; }

private:
    template <class, class>
    friend class synchronized_value;

    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

template <class T, class Mutex>
class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

public:
    using mutex = Mutex;

    // Un verrou partagé n'est pris que si les deux moitiés l'autorisent : le
    // mutex peut être tenu par plusieurs lecteurs, et le T est lisible par
    // plusieurs lecteurs.
    static consteval auto get_const_guard_type() {
        if constexpr (detail::shared_lockable<Mutex>
                      && is_synchronizable_v<const T>) {
            return ^^value_guard<const T, std::shared_lock<Mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<Mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<Mutex>>;
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
    mutable Mutex mutex_;
    T value_;
};

template <class T, class Mutex>
struct is_synchronizable<synchronized_value<T, Mutex>> : is_sendable<T> {};

template <class T, class Mutex>
struct is_lifetime_aware<synchronized_value<T, Mutex>>
    : is_lifetime_aware<T> {};

template <class T, class Lock>
struct is_sendable<value_guard<T, Lock>> : std::false_type {};
template <class T, class Lock>
struct is_lifetime_aware<value_guard<T, Lock>> : std::false_type {};

}
```

Le programme de vérification, compilé et exécuté :

```cpp
// q7.cpp
#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

struct Mut { mutable int cached_ = 0; };

using sv_vector = threadsafe::synchronized_value<std::vector<int>>;
using sv_mut = threadsafe::synchronized_value<Mut>;
using sv_recursive =
    threadsafe::synchronized_value<std::vector<int>, std::recursive_mutex>;
using sv_timed =
    threadsafe::synchronized_value<std::vector<int>, std::timed_mutex>;

static_assert(std::is_same_v<sv_vector::mutex, std::shared_mutex>);
static_assert(std::is_same_v<sv_mut::mutex, std::mutex>);
static_assert(std::is_same_v<sv_vector::const_guard,
                             threadsafe::value_guard<const std::vector<int>,
                                 std::shared_lock<std::shared_mutex>>>);
static_assert(std::is_same_v<sv_recursive::const_guard,
                             threadsafe::value_guard<const std::vector<int>,
                                 std::unique_lock<std::recursive_mutex>>>);
static_assert(std::is_same_v<sv_timed::const_guard,
                             threadsafe::value_guard<const std::vector<int>,
                                 std::unique_lock<std::timed_mutex>>>);
static_assert(threadsafe::is_synchronizable_v<sv_recursive>);

int main() {
    sv_recursive values{};
    auto touch = [&values] {
        for (int step = 0; step < 1000; ++step) {
            auto guard = values.lock();
            guard->push_back(step);
        }
    };
    { std::jthread a{touch}, b{touch}; }
    const auto reader = values.lock_shared();
    std::printf("recursive_mutex-backed size = %zu\n", reader->size());

    sv_vector plain{};
    { auto guard = plain.lock(); guard->push_back(1); }
    const auto plain_reader = plain.lock_shared();
    std::printf("default size = %zu\n", plain_reader->size());
}
```

```
recursive_mutex-backed size = 2000
default size = 1
exit=0

PASS test_asynchronous_task_launcher.cpp   PASS test_smart_pointers.cpp
PASS test_containers.cpp                   PASS test_soundness_regressions.cpp
PASS test_copy_on_write.cpp                PASS test_synchronizable.cpp
PASS test_deferred_specialization.cpp      PASS test_synchronized_value.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
PASS test_sendable.cpp
```

Les défauts sont **identiques au comportement actuel** (`shared_mutex` pour `vector<int>`, `mutex`
pour un `T` à membre `mutable`), aucun code existant ne change, et `synchronized_value<T, M>` devient
adoptable dans du code qui possède déjà son verrou. **11 UT sur 11 vertes.**

Deux conséquences supplémentaires qui n'étaient pas dans le constat et que j'ai mesurées :

1. Le concept `shared_lockable` **rend la décision cohérente**. Aujourd'hui `get_const_guard_type`
   demande si `const T` est lisible, puis *suppose* que le mutex est un `std::shared_mutex`. Avec le
   paramètre, la décision interroge les deux moitiés.
2. **Elle transforme le piège ODR de la partie 4 en erreur d'édition de liens.** Voir
   [4.4](#44-latténuation-que-je-recommande).

Le rapport [07](./07-performance-execution.md) donne la mesure d'exécution qui compte ici : le
`std::shared_mutex` sélectionné automatiquement est jusqu'à **119× plus lent** qu'un `std::mutex`
sous section critique courte, et ne devient gagnant qu'au-delà d'environ 500 ns passées sous le
verrou. Le second paramètre de template est donc aussi le levier de performance manquant : la
sélection reste *sûre*, mais l'utilisateur peut enfin la corriger quand elle est *lente*.

### 3.3 Les limites de la macro (Q9)

La macro s'écrit :

```cpp
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)  \
    template <>                                       \
    struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}
```

Je l'ai testée sur les trois formes que l'énoncé demande.

**(a) Une virgule dans les arguments de template — ÇA MARCHE.**

```cpp
// macro1.cpp
#include <threadsafe/threadsafe.h>
#include <map>
#include <string>

template <class Key, class Value> struct Table { Key* keys; Value* values; };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Table<int, std::string>);

static_assert(threadsafe::is_synchronizable_v<Table<int, std::string>>);
int main() {}
```
```
-> compile proprement
```

Le `__VA_ARGS__` variadique absorbe la virgule. C'est un détail bien fait, et il mérite d'être noté :
la forme naïve `#define M(TYPE)` aurait échoué ici.

**(b) À l'intérieur d'un espace de noms — ÇA ÉCHOUE.**

```cpp
// macro2.cpp
#include <threadsafe/threadsafe.h>
#include <string>

namespace acme {
struct Widget { std::string name; ~Widget() {} };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Widget);
}
int main() {}
```
```
.../synchronizable_base.h:32:24: error: declaration of
  'struct threadsafe::is_synchronizable<acme::Widget>' in namespace 'acme'
  which does not enclose 'threadsafe'
   32 |     struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}
      |                        ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
macro2.cpp:6:1: note: in expansion of macro 'THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE'
```

**Ceci n'est pas réparable** — `[temp.expl.spec]` exige qu'une spécialisation explicite apparaisse
dans un espace de noms englobant le template spécialisé, et aucun corps de macro ne peut fermer puis
rouvrir l'espace de noms de l'appelant. C'est une limite du langage, pas un défaut de la
bibliothèque. Ce qui *est* réparable, c'est que l'erreur pointe vers l'en-tête de la bibliothèque au
lieu de la ligne de l'utilisateur, et que ni le nom de la macro ni sa documentation ne mentionnent la
contrainte. Le renommage proposé par l'équipe (non compilé, car il ne change aucun comportement) :

```cpp
// ==== ÉDITION : include/threadsafe/details/synchronizable_base.h ====
// À écrire À PORTÉE GLOBALE, avec le nom du type pleinement qualifié :
//
//   THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE_AT_GLOBAL_SCOPE(acme::Widget);
//
// La macro se développe en une spécialisation explicite de
// threadsafe::is_synchronizable, et [temp.expl.spec] exige qu'une telle
// spécialisation apparaisse dans un espace de noms englobant `threadsafe` — donc
// à l'intérieur de `namespace acme { ... }` elle ne compile pas, et le
// diagnostic pointe vers cet en-tête et non vers votre ligne.
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE_AT_GLOBAL_SCOPE(...)  \
    template <>                                                       \
    struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}

// Ancien nom conservé comme alias, pour ne rien casser :
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)  \
    THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE_AT_GLOBAL_SCOPE(__VA_ARGS__)
```

**(c) Pour une spécialisation partielle de template de classe — ÇA ÉCHOUE.**

```cpp
// macro3.cpp
#include <threadsafe/threadsafe.h>

template <class Element> struct Ring { Element* buffer; };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Ring<Element>);
int main() {}
```
```
macro3.cpp:5:46: error: 'Element' was not declared in this scope
    5 | THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Ring<Element>);
      |                                              ^~~~~~~
```

La macro code en dur `template <>` : elle ne peut produire qu'une spécialisation **explicite**. La
macro corrigée, que j'ai écrite et vérifiée :

```cpp
// macro4.cpp — la macro corrigée, compilée proprement
#define THREADSAFE_DETAIL_UNPARENTHESIZE(...) __VA_ARGS__

// Voucher pour toutes les spécialisations d'un template de classe d'un coup. Le
// premier argument est la liste des paramètres de template, parenthésée pour que
// ses virgules survivent au développement de la macro ; le reste est le motif de
// spécialisation.
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE_TEMPLATE(PARAMETERS, ...)     \
    template <THREADSAFE_DETAIL_UNPARENTHESIZE PARAMETERS>                     \
    struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}

template <class Element> struct Ring { Element* buffer; };
template <class Key, class Value> struct Table { Key* keys; Value* values; };

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE_TEMPLATE((class Element), Ring<Element>);
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE_TEMPLATE((class Key, class Value),
                                                 Table<Key, Value>);

static_assert(threadsafe::is_synchronizable_v<Ring<int>>);
static_assert(threadsafe::is_synchronizable_v<Ring<std::string>>);
static_assert(threadsafe::is_synchronizable_v<Table<int, std::string>>);
static_assert(threadsafe::is_sendable_v<Ring<int>>);
static_assert(threadsafe::is_sendable_v<Ring<int>&>);
int main() {}
```
```
TEMPLATE macro OK
```

**Mais je recommande de ne PAS l'ajouter.** Voici ce que l'utilisateur écrit sans macro du tout, que
j'ai également compilé :

```cpp
// macro5.cpp
#include <threadsafe/threadsafe.h>
#include <string>
#include <type_traits>

template <class Element> struct Ring { Element* buffer; };

template <class Element>
struct threadsafe::is_synchronizable<Ring<Element>> : std::true_type {};

static_assert(threadsafe::is_synchronizable_v<Ring<int>>);
static_assert(threadsafe::is_sendable_v<Ring<std::string>>);
int main() {}
```
```
hand-written OK
```

Deux lignes, lisibles, sans `THREADSAFE_DETAIL_UNPARENTHESIZE`, sans argument parenthésé, sans
convention à expliquer à un auditoire. **Défi du besoin : la macro `_TEMPLATE` ne mérite pas sa
place.** Pour une bibliothèque pédagogique, la spécialisation partielle écrite à la main *est* la
leçon ; une macro qui la cache est un pas dans la mauvaise direction. Je garderais uniquement le
renommage `_AT_GLOBAL_SCOPE`, dont le seul rôle est d'inscrire la contrainte dans le nom.

---

## 4. Le piège ODR

C'est le danger d'API le plus sérieux de la bibliothèque, et il est vérifié par le lead. La cause est
structurelle et se dit en une phrase :

> **Les traits sont lus réflexivement au point d'usage. Une spécialisation écrite après un premier
> usage, ou dans une partie seulement des unités de traduction, change la réponse — sans aucun
> diagnostic.**

### 4.1 Le cas de base : une spécialisation tardive

Le programme complet à deux UT (constat Q1), que j'ai reconstruit, compilé et exécuté :

```cpp
// ==== FICHIER : widget.h ====
#pragma once

struct Widget {
    int* borrowed;
};
```
```cpp
// ==== FICHIER : shared.h ====
#pragma once
#include <threadsafe/threadsafe.h>
#include "widget.h"

// Un template de fonction inline instancié dans les deux UT. Son CORPS lit le
// trait : son sens dépend donc des spécialisations que l'UT avait vues.
template <class T>
inline bool trait_says_sendable() {
    return threadsafe::is_sendable_v<T>;
}

struct Holder {
    Widget widget;
};
```
```cpp
// ==== FICHIER : a.cpp ====
#include "shared.h"
#include <print>

bool inline_answer_seen_from_b();
bool constant_baked_into_b();

bool constant_baked_into_a() { return threadsafe::is_sendable_v<Holder>; }
bool inline_answer_seen_from_a() { return trait_says_sendable<Holder>(); }

int main() {
    std::println("TU A compiled the constant as : {}", constant_baked_into_a());
    std::println("TU B compiled the constant as : {}", constant_baked_into_b());
    std::println("inline template, called from A: {}", inline_answer_seen_from_a());
    std::println("inline template, called from B: {}", inline_answer_seen_from_b());
}
```
```cpp
// ==== FICHIER : b.cpp ====
#include "shared.h"
#include <type_traits>

// L'UT B se porte garante de Widget APRÈS shared.h — l'air parfaitement légal,
// silencieusement divergent.
template <>
struct threadsafe::is_synchronizable<Widget> : std::true_type {};

bool constant_baked_into_b() { return threadsafe::is_sendable_v<Holder>; }
bool inline_answer_seen_from_b() { return trait_says_sendable<Holder>(); }
```

```
$ g++-16 -std=c++26 -freflection -I<include> -I. -O2 -pthread a.cpp b.cpp -o prog
BUILD CLEAN
$ ./prog
TU A compiled the constant as : false
TU B compiled the constant as : true
inline template, called from A: false
inline template, called from B: true

$ g++-16 ... b.cpp a.cpp -o prog2 && ./prog2
TU A compiled the constant as : false
TU B compiled the constant as : true
inline template, called from A: false
inline template, called from B: true
```

Les deux dernières lignes sont **la même spécialisation de template de fonction inline**,
`trait_says_sendable<Holder>()`, qui renvoie deux réponses différentes dans un seul programme. Zéro
avertissement, dans les deux ordres d'édition de liens.

À l'intérieur d'**une** UT, le compilateur attrape le problème (l'équipe l'a vérifié) :

```
within_tu.cpp:9:20: error: specialization of 'threadsafe::is_synchronizable<Widget>' after instantiation
```

Seul le cas inter-UT est silencieux.

### 4.2 Le cas vérifié par le lead : deux tailles pour une classe

C'est la version qui corrompt de la mémoire. `synchronized_value<T>::mutex` est choisi par
`if constexpr (is_synchronizable_v<const T>)`, et
`tests/test_deferred_specialization.cpp` présente le vouch comme quelque chose que l'utilisateur
écrit dans *son* UT. Le programme complet (constat TC-4), à compiler **deux fois** :

```cpp
// odr.cpp — compiler deux fois et lier les deux objets :
//   g++-16 -std=c++26 -freflection -I<include> -DTHREADSAFE_TU_WITH_VOUCH -O0 -c -o with_vouch.o odr.cpp
//   g++-16 -std=c++26 -freflection -I<include>                            -O0 -c -o without_vouch.o odr.cpp
//   g++-16 with_vouch.o without_vouch.o -o odrprog -pthread && ./odrprog

#include <threadsafe/threadsafe.h>

#include <cstdio>

struct Cache {
    mutable int hits;
};

#ifdef THREADSAFE_TU_WITH_VOUCH
// Écrit ici, dans cette unité de traduction uniquement — exactement le motif que
// tests/test_deferred_specialization.cpp présente comme supporté.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Cache);
#endif

using SharedCache = threadsafe::synchronized_value<Cache>;

void read_from_vouching_tu(SharedCache &shared_cache);
std::size_t size_seen_by_vouching_tu();

#ifdef THREADSAFE_TU_WITH_VOUCH

std::size_t size_seen_by_vouching_tu() { return sizeof(SharedCache); }

void read_from_vouching_tu(SharedCache &shared_cache) {
    auto guard = shared_cache.lock_shared();
    std::printf("  vouching TU read hits = %d\n", guard->hits);
}

#else

struct Sandwich {
    SharedCache shared_cache;
    unsigned long canary;
};

int main() {
    std::printf("sizeof(synchronized_value<Cache>) in the plain TU    = %zu\n",
                sizeof(SharedCache));
    std::printf("sizeof(synchronized_value<Cache>) in the vouching TU = %zu\n",
                size_seen_by_vouching_tu());

    Sandwich sandwich{SharedCache{Cache{7}}, 0xC0FFEEUL};
    std::printf("canary before = 0x%lX\n", sandwich.canary);
    read_from_vouching_tu(sandwich.shared_cache);
    std::printf("canary after  = 0x%lX\n", sandwich.canary);
    if (sandwich.canary != 0xC0FFEEUL)
        std::printf("CANARY CLOBBERED: the two TUs disagree on the layout\n");
    return 0;
}

#endif
```

Les deux objets compilent **sans un seul avertissement** ; l'édition de liens réussit. Sortie exacte,
que j'ai reproduite trois fois par niveau d'optimisation :

```
=== -O0 (le résultat du lead) ===
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/shared_mutex:246:
  void std::__shared_mutex_pthread::lock_shared(): Assertion '__ret == 0' failed.
sizeof(synchronized_value<Cache>) in the plain TU    = 72
sizeof(synchronized_value<Cache>) in the vouching TU = 208
canary before = 0xC0FFEE
exit=134
                                       (identique aux trois exécutions)

=== -O2 (le niveau auquel on livre) ===
sizeof(synchronized_value<Cache>) in the plain TU    = 72
sizeof(synchronized_value<Cache>) in the vouching TU = 208
canary before = 0xC0FFEE
  vouching TU read hits = 70320128
canary after  = 0xC0FFEE
exit=0
                                       (hits = 84164608, puis 40321024 aux exécutions suivantes)
```

**C'est pire que ce que dit le constat.** À `-O0` le programme avorte, ce qui est au moins bruyant. À
`-O1` et `-O2` — le niveau auquel n'importe qui livre — **il ne plante pas** : il lit de la mémoire
arbitraire (`hits` vaut 70320128, 84164608, 40321024 au lieu de 7), la canari survit, et le programme
sort avec le code 0. Une donnée fausse, silencieuse, non déterministe, dans un programme compilé sans
avertissement par une bibliothèque dont l'argument de vente est que le code non sûr ne compile pas.

### 4.3 La même divergence sans écrire la moindre spécialisation

Trois autres portes mènent au même endroit, et deux d'entre elles ne demandent **aucune** action de
l'utilisateur :

**TC-3 — `is_defaulted` bascule sur un `= default` hors-ligne.** `has_only_default_copy_move_destroy`
interroge `std::meta::is_defaulted`, qui vaut `false` pour `~Widget();` dans un en-tête et `true` une
fois que le `.cpp` a écrit `Widget::~Widget() = default;`. L'UT qui contient les définitions répond
`is_sendable_v<Widget> == true`, toutes les autres répondent `false`. C'est **exactement la forme du
pimpl**. Le correctif — remplacer `is_defaulted`/`is_deleted` par `std::meta::is_user_provided`, qui
ne bascule pas — est **vérifié suite verte** par l'équipe :

```cpp
// ==== ÉDITION : include/threadsafe/details/utils.h ====
inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (may_hijack_copy_move(member))
            return false;

        if (!is_copy_move_destroy_member(member))
            continue;

        // is_defaulted bascule dès qu'un `= default` hors-ligne est vu : une
        // unité de traduction qui ne voit que l'en-tête et celle qui contient
        // les définitions répondraient différemment pour le même type — deux
        // définitions de chaque template clé sur le trait. is_user_provided est
        // figé à la première déclaration : toutes les unités lisent la même
        // réponse.
        if (std::meta::is_user_provided(member))
            return false;
    }
    return true;
}
```

(`is_user_provided` subsume aussi l'ancienne échappatoire `is_deleted` : un `= delete` sur la
première déclaration n'est pas *user-provided*.) **À appliquer sans réserve.**

**ADV-05 — un type incomplet répond `false` et la réponse est figée pour toute l'UT.** Une UT qui
voit la définition du pimpl répond `true`, une autre `false`, les deux `static_assert` contradictoires
passent, et un `if constexpr` dans un en-tête produit deux corps pour le même symbole faible. L'équipe
a mesuré que le comportement observable **change avec l'ordre des `.o` sur la ligne d'édition de
liens** (à `-O0` ; à `-O2` GCC inline les deux copies et la divergence ne se manifeste pas).
**Aucun correctif, et c'est justifié** : la mise en cache est la façon dont C++ instancie les
templates de classe, et transformer le `false` en erreur dure casserait toutes les utilisations en
`if constexpr` et en concept, ainsi que la solution de contournement pimpl que la bibliothèque
recommande elle-même.

**Q4 — l'ordre des `#include` est porteur de sens.** La spécialisation partielle
`is_synchronizable<const T>` vit dans `synchronizable.h`, mais `smart_pointers.h` et `vocabulary.h`
lisent la question const sans l'inclure. Une UT qui n'inclut que
`<threadsafe/details/smart_pointers.h>` répond silencieusement
`is_synchronizable_v<const Plain> == false` là où la bibliothèque complète répond `true` — **même
question, réponses opposées, aucun diagnostic dans un sens comme dans l'autre**. Et
`asynchronous_task_launcher.h` ne compile pas seul :

```
asynchronous_task_launcher.h:82:19: error: static assertion failed:
  std::jthread injects a stop_token that the Args constraints never see;
  it must satisfy them on its own
```

Autrement dit, **alphabétiser les dix `#include` de `threadsafe.h` — un rangement parfaitement
naturel — casse la compilation**. Le correctif de l'équipe (rendre les onze en-têtes autonomes et
descendre la règle const dans `synchronizable_base.h`) est **vérifié suite verte**.

### 4.4 L'atténuation que je recommande

Trois leviers existent. Aucun ne ferme complètement le trou ; je le dis d'emblée.

**Levier 1 — le fil-piège `traits_settled<T>` (correctif Q1). Vérifié par moi.**

Lire un trait le *fige* : à partir de ce point la réponse est instanciée, et une spécialisation
écrite plus tard est une « specialization after instantiation », que le compilateur rejette. Il
suffit donc de forcer chaque UT à lire les quatre réponses au même endroit — l'en-tête qui déclare le
type.

```cpp
// ==== NOUVEAU FICHIER : include/threadsafe/details/traits_settled.h ====
#pragma once

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

// Lire un trait le fige : à partir de ce point la réponse est instanciée, et une
// spécialisation écrite plus tard est une spécialisation après instanciation,
// que le compilateur rejette. Lire les quatre ici, c'est tout le mécanisme.
//
// Écrivez `static_assert(threadsafe::traits_settled<Widget>);` dans l'en-tête
// qui déclare Widget. Toutes les unités de traduction voient cet en-tête, donc
// toutes figent les quatre mêmes réponses, et une unité qui se porte garante de
// Widget trop tard échoue à compiler au lieu de lier silencieusement une seconde
// réponse dans le programme.
//
// La somme garde chaque lecture évaluée : `||` court-circuiterait les suivantes
// et les laisserait non figées.
template <class T>
constexpr bool traits_settled = int{is_sendable_v<T>}
                                  + int{is_synchronizable_v<T>}
                                  + int{is_synchronizable_v<const T>}
                                  + int{is_lifetime_aware_v<T>}
                              >= 0;

}
```

```cpp
// ==== ÉDITION : include/threadsafe/threadsafe.h — fichier de remplacement complet ====
#pragma once

#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/details/synchronized_value.h>
#include <threadsafe/details/copy_on_write.h>
#include <threadsafe/details/traits_settled.h>
```

L'utilisateur écrit alors, dans l'en-tête qui déclare son type :

```cpp
// ==== FICHIER : widget2.h ====
#pragma once
#include <threadsafe/threadsafe.h>

struct Widget {
    int* borrowed;
};

// Le fil-piège : toute unité de traduction qui voit Widget fige ici ses quatre
// réponses, donc un vouch écrit plus tard est une spécialisation après
// instanciation et ne compile pas.
static_assert(threadsafe::traits_settled<Widget>);
```

Mes résultats :

```
=== a2.cpp (sans vouch) ===
  compile proprement
=== b2.cpp (vouch tardif) ===
b2.cpp:5:20: error: specialization of 'threadsafe::is_synchronizable<Widget>' after instantiation

=== le scénario TC-4, Cache déclaré dans cache.h avec le fil-piège ===
-- UT ordinaire --   compile proprement
-- UT avec vouch --  .../synchronizable_base.h:32:24: error: specialization of
                     'threadsafe::is_synchronizable<const Cache>' after instantiation

PASS test_asynchronous_task_launcher.cpp   PASS test_smart_pointers.cpp
PASS test_containers.cpp                   PASS test_soundness_regressions.cpp
PASS test_copy_on_write.cpp                PASS test_synchronizable.cpp
PASS test_deferred_specialization.cpp      PASS test_synchronized_value.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
PASS test_sendable.cpp
```

**Le fil-piège ferme les deux scénarios, Q1 et TC-4, et les 11 UT restent vertes.** Mais il est
**optionnel** : un utilisateur qui n'écrit pas le `static_assert` reste exposé, et la bibliothèque ne
peut pas le faire à sa place puisqu'elle ne voit jamais le type de l'utilisateur. Notez aussi que
dans le cas TC-4 le diagnostic pointe vers `synchronizable_base.h:32` — la macro, encore une fois,
masque le site d'appel.

**Levier 2 — rendre la divergence visible dans le nom décoré. C'est ma découverte de cet audit.**

Le correctif Q7 de la [partie 3.2](#32-le-mutex-figé-de-synchronized_value-q7) ajoute le mutex comme
second paramètre de template. Le défaut est résolu **au site d'usage**, donc dans l'UT sans vouch
`synchronized_value<Cache>` signifie `synchronized_value<Cache, std::mutex>` et dans l'UT avec vouch
`synchronized_value<Cache, std::shared_mutex>` — **deux types différents, deux noms décorés
différents**. J'ai relié les deux objets du programme TC-4 contre l'arbre Q7 :

```
$ g++-16 q7wv.o q7wov.o -o q7prog -pthread
Undefined symbols for architecture arm64:
  "read_from_vouching_tu(threadsafe::synchronized_value<Cache, std::mutex>&)", referenced from:
      _main in q7wov.o
ld: symbol(s) not found for architecture arm64
```

**La corruption mémoire silencieuse devient une erreur d'édition de liens.** C'est de très loin
l'atténuation la plus solide des trois, parce qu'elle ne demande rien à l'utilisateur : dès que le
type divergent traverse une frontière d'UT dans une signature — et il *doit* la traverser, sinon les
deux dispositions ne se rencontrent jamais — l'éditeur de liens refuse. Elle ne ferme pas Q1 pour
autant : la divergence de `is_sendable_v<Holder>` n'apparaît dans aucun nom décoré.

**Levier 3 — la disposition inconditionnelle (correctif TC-4). Suite ROUGE : à trancher.**

```cpp
// ==== ÉDITION : include/threadsafe/details/synchronized_value.h ====
// Remplacer le bloc get_mutex_type / get_const_guard_type (lignes 53-70) par :

    // Fixe, quoi que répondent les traits. Dériver le mutex — et donc la taille
    // de cette classe — de is_synchronizable_v<const T> rendait la disposition
    // dépendante du fait que l'unité de traduction courante avait vu ou non un
    // THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE.
    using mutex = std::shared_mutex;
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;

// et contraindre la lecture partagée, pour qu'une unité qui n'a pas vu le vouch
// échoue à compiler au lieu de prendre silencieusement un verrou auquel elle n'a
// pas droit :

    [[nodiscard]] const_guard lock_shared() const
        requires is_synchronizable_v<const T>
    {
        return const_guard{mutex_, value_};
    }
```

L'équipe l'a exécuté : le programme à deux UT affiche alors 208/208, lit correctement, et la canari
survit. **Mais il casse `tests/test_synchronized_value.cpp` lignes 120, 122, 125, 131 et 134**, qui
documentent précisément la fonctionnalité de sélection de mutex que cette version supprime
(« no shared_mutex: there is no read that may be shared »). C'est une décision de conception, pas un
patch. Et le [rapport 07](./07-performance-execution.md) donne l'argument contre : imposer
`std::shared_mutex` à tout le monde coûte jusqu'à 119× sous section critique courte.

**Ma recommandation, dans l'ordre :**

1. **Appliquer TC-3** (`is_user_provided`) — suite verte, ferme une porte que l'utilisateur ouvre
   sans le savoir.
2. **Appliquer Q4** (en-têtes autonomes) — suite verte, supprime la dépendance à l'ordre des
   `#include`.
3. **Appliquer Q7** (mutex paramétré) — suite verte, transforme TC-4 en erreur d'édition de liens.
4. **Ajouter `traits_settled` et le documenter** — suite verte, ferme Q1 pour qui l'écrit.
5. **Écrire la règle dans `CLAUDE.md` et dans le `README`**, en toutes lettres :

> Ces traits répondent à propos d'un type **tel qu'il est visible depuis l'unité de traduction
> courante**, et la réponse est figée à la première interrogation. Toute spécialisation
> (`is_sendable`, `is_synchronizable`, `is_lifetime_aware`, ou la macro
> `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE`) doit être écrite **dans un en-tête, immédiatement après
> la déclaration du type, et vue par toutes les unités de traduction**. Une spécialisation écrite
> dans un seul `.cpp` produit un programme dans lequel deux fichiers objets contiennent deux réponses
> de sûreté différentes ; C++ appelle cela *ill-formed, no diagnostic required*, et ni le compilateur
> ni l'éditeur de liens ne vous préviendront.

**Est-ce que l'un de ces leviers ferme complètement le trou ? Non.** Aucun n'est capable de le
fermer, et il faut le dire à l'auditoire plutôt que le cacher : c'est **le prix du choix de répondre
à des questions de sûreté avec des spécialisations de template**. Rust répond à la même question avec
`unsafe impl Send`, qui est un élément de la *crate* et donc unique par définition ; C++ n'a pas
d'équivalent, et la séparation en unités de traduction fait le reste. C'est, à mon avis, **la
meilleure diapositive de toute la présentation** : elle montre exactement où le modèle Rust ne se
transpose pas, et pourquoi.

---

## 5. Ce qui manque

### 5.1 Le lanceur : arrêter, joindre, savoir, rendre un résultat

`asynchronous_task_launcher` a deux méthodes publiques, `launch_task` et `launch_scoped_task`, et un
`std::vector<std::jthread>` privé. Il n'y a **aucun** moyen de demander un arrêt, de joindre avant la
destruction, de savoir quand une tâche est finie, ou de récupérer un résultat.

**La conséquence immédiate, que j'ai reproduite** : `launch_scoped_task` **interbloque** sur une tâche
coopérative, parce que le `std::jthread` qu'il crée est une variable locale — aucun appelant ne peut
atteindre son `stop_source`.

```cpp
// scoped_deadlock.cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stop_token>
#include <thread>

struct cooperative_task {
    void operator()(std::stop_token token) const {
        while (!token.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
};

int main() {
    std::jthread watchdog{[](std::stop_token token) {
        for (int second = 0; second < 3 && !token.stop_requested(); ++second)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!token.stop_requested()) {
            std::printf("DEADLOCK: launch_scoped_task never returned\n");
            std::fflush(stdout);
            std::_Exit(42);
        }
    }};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(cooperative_task{});
    std::printf("launch_scoped_task returned\n");
    watchdog.request_stop();
}
```
```
DEADLOCK: launch_scoped_task never returned
exit=42
```
« launch_scoped_task returned » n'est jamais affiché.

**L'ajout complet — deux méthodes, dix-huit lignes. Appliqué, compilé, exécuté et passé en régression
par moi :**

```cpp
// ==== ÉDITION : include/threadsafe/details/asynchronous_task_launcher.h ====
// Dans class asynchronous_task_launcher, juste avant `private:` :

    // Demander à toutes les tâches lancées jusqu'ici de s'arrêter. Une tâche qui
    // prend un std::stop_token en premier paramètre s'en voit remettre un par
    // std::jthread ; une tâche qui n'en prend pas ignore simplement la demande.
    void request_stop() noexcept {
        for (std::jthread& task : threads_)
            task.request_stop();
    }

    // Attendre toutes les tâches lancées jusqu'ici. Le destructeur le fait déjà,
    // un jthread après l'autre ; ceci fait de l'attente une instruction plutôt
    // qu'une portée, ce dont l'appelant a besoin pour observer des résultats.
    void join() {
        for (std::jthread& task : threads_)
            if (task.joinable())
                task.join();
        threads_.clear();
    }
```

Le programme de vérification, exécuté :

```cpp
// launcher_stop.cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stop_token>
#include <thread>

struct counting_task {
    std::shared_ptr<threadsafe::synchronized_value<long>> ticks;
    void operator()(std::stop_token token) const {
        while (!token.stop_requested()) {
            auto guard = ticks->lock();
            ++*guard;
        }
    }
};

int main() {
    auto ticks = threadsafe::synchronized_value<long>::make(0L);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(counting_task{ticks});
    launcher.launch_task(counting_task{ticks});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    launcher.request_stop();
    launcher.join();
    const auto reader = ticks->lock_shared();
    std::printf("stopped cleanly after %ld ticks\n", *reader);
}
```
```
stopped cleanly after 32852 ticks
exit=0

PASS test_asynchronous_task_launcher.cpp   PASS test_smart_pointers.cpp
PASS test_containers.cpp                   PASS test_soundness_regressions.cpp
PASS test_copy_on_write.cpp                PASS test_synchronizable.cpp
PASS test_deferred_specialization.cpp      PASS test_synchronized_value.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
PASS test_sendable.cpp
```

**Le canal de résultat : je recommande de ne PAS l'ajouter.** La norme en a déjà un, et il fonctionne
dès que la bibliothèque cesse de le refuser (constat TLS-06). J'ai compilé et exécuté :

```cpp
// result_channel.cpp
#include <threadsafe/threadsafe.h>

#include <future>
#include <print>
#include <type_traits>
#include <utility>

// Ce que la bibliothèque ne dit pas aujourd'hui (constat TLS-06) : les poignées
// de transfert de la norme possèdent leur état partagé.
template <class T>
struct threadsafe::is_sendable<std::promise<T>>
    : std::bool_constant<std::is_void_v<T> || is_sendable_v<std::remove_cv_t<T>>> {};
template <class T>
struct threadsafe::is_lifetime_aware<std::promise<T>>
    : std::bool_constant<std::is_void_v<T> || is_lifetime_aware_v<std::remove_cv_t<T>>> {};

struct compute_task {
    void operator()(std::promise<long> answer, long upper_bound) const {
        long total = 0;
        for (long value = 1; value <= upper_bound; ++value) total += value;
        answer.set_value(total);
    }
};

int main() {
    std::promise<long> answer;
    std::future<long> result = answer.get_future();

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(compute_task{}, std::move(answer), 1'000'000L);

    std::println("sum = {}", result.get());
}
```
```
sum = 500000500000
exit=0
```

Deux spécialisations — que le correctif TLS-06 propose de mettre dans `vocabulary.h`, **vérifié suite
verte** — et le canal de résultat de la norme fonctionne. Aucun `launch_task_with_result` n'est
nécessaire. C'est aussi une meilleure histoire pour une conférence : *la bibliothèque ne remplace pas
`<future>`, elle en garde la porte.*

**Un point à ne pas comber ici :** `threads_` est un `std::vector<std::jthread>` nu, donc lancer une
tâche depuis une tâche corrompt le vecteur — le lead a mesuré trois SIGSEGV (codes 139, 133, 139). Ce
n'est pas un manque d'API, c'est un défaut de robustesse ; il appartient au
[rapport 02](./02-robustesse-des-helpers.md).

### 5.2 `copy_on_write` n'a aucun canal de publication (E1)

`copy_on_write<T>` n'a ni `store()` ni `load()`, et `is_synchronizable<copy_on_write<T>>` est **faux**
— délibérément et correctement, puisque `as_mutable()` réassigne `ptr_` et que deux threads partageant
une poignée courraient sur le `shared_ptr` lui-même. Conséquence : rédacteur et lecteurs doivent
détenir des poignées **séparées**, et `as_mutable()` ne réassigne que celle du rédacteur.

La tentative naïve est refusée :

```
'what()': 'std::reference_wrapper<threadsafe::copy_on_write<Config> > has a user-written copy, move
           or destructor — or a template that may be selected as one — which can share state the
           members do not show; specialize is_sendable to state the intent'
```

(la raison affichée est fausse — la vraie est que `copy_on_write<Config>` n'est pas *synchronizable* ;
voir [04 diagnostics](./04-diagnostics.md).)

**Et la seule forme que les traits autorisent compile, tourne sans course, et ne publie jamais.**
J'ai écrit et exécuté le programme complet :

```cpp
// cow_never.cpp
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

struct Config {
    int version;
    std::string name;
};

struct reader_task {
    threadsafe::copy_on_write<Config> own_handle;
    std::shared_ptr<threadsafe::synchronized_value<int>> highest_seen;
    void operator()(std::stop_token token) const {
        int highest = 0;
        while (!token.stop_requested())
            if (own_handle->version > highest) highest = own_handle->version;
        auto guard = highest_seen->lock();
        if (highest > *guard) *guard = highest;
    }
};

int main() {
    threadsafe::copy_on_write<Config> writer_handle{Config{1, "v1"}};
    auto highest_seen = threadsafe::synchronized_value<int>::make(0);

    threadsafe::asynchronous_task_launcher launcher;
    for (int reader = 0; reader < 4; ++reader)
        launcher.launch_task(reader_task{writer_handle, highest_seen});

    for (int version = 2; version <= 200; ++version)
        writer_handle.as_mutable().version = version;

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    launcher.request_stop();
    launcher.join();

    const auto reader = highest_seen->lock_shared();
    std::printf("writer reached version %d; highest version any reader saw = %d\n",
                writer_handle->version, *reader);
}
```
```
writer reached version 200; highest version any reader saw = 1
writer reached version 200; highest version any reader saw = 1
```

(Utilise `request_stop()`/`join()` de la [section 5.1](#51-le-lanceur--arrêter-joindre-savoir-rendre-un-résultat).)
Le rédacteur atteint la version 200 ; les lecteurs n'en voient jamais aucune. Le programme est
race-free — l'équipe l'a confirmé sous ThreadSanitizer — et le plus rapide des cinq stratégies
mesurées (16 ns/lecture). Et il est **faux**.

**Le motif qui marche existe, et rien ne le désigne :** `synchronized_value<copy_on_write<T>>`.
J'ai écrit et exécuté le programme complet :

```cpp
// cow_publish.cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct Config {
    int version;
    std::string name;
    std::vector<int> weights;
};

using published_config =
    threadsafe::synchronized_value<threadsafe::copy_on_write<Config>>;

struct reader_task {
    std::shared_ptr<published_config> published;
    std::shared_ptr<threadsafe::synchronized_value<int>> highest_seen;
    void operator()(std::stop_token token) const {
        int highest = 0;
        while (!token.stop_requested()) {
            const threadsafe::copy_on_write<Config> snapshot = [this] {
                const auto guard = published->lock_shared();
                return *guard;
            }();
            if (snapshot->version > highest) highest = snapshot->version;
        }
        auto guard = highest_seen->lock();
        if (highest > *guard) *guard = highest;
    }
};

int main() {
    auto published = std::make_shared<published_config>(
        threadsafe::copy_on_write<Config>{Config{1, "v1", {1, 2, 3}}});
    auto highest_seen = threadsafe::synchronized_value<int>::make(0);

    threadsafe::asynchronous_task_launcher launcher;
    for (int reader = 0; reader < 4; ++reader)
        launcher.launch_task(reader_task{published, highest_seen});

    for (int version = 2; version <= 200; ++version) {
        auto guard = published->lock();
        guard->as_mutable().version = version;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    launcher.request_stop();
    launcher.join();

    const auto reader = highest_seen->lock_shared();
    std::printf("highest version any reader saw = %d\n", *reader);
}
```
```
highest version any reader saw = 200
exit=0
```

Les mesures de l'équipe sur ce banc (8 lecteurs × 400 000 lectures, `-O2`, **non revérifiées par le
lead**) :

| stratégie | ns/lecture | version max vue par un lecteur |
|---|---|---|
| `copy_on_write`, une poignée par thread | 16 | **1 — n'a jamais publié** |
| `synchronized_value<copy_on_write<Config>>` | 40 | 200 |
| `synchronized_value<Config>` (`shared_mutex`) | 320 | 200 |
| `std::shared_mutex` écrit à la main | 311 | 200 |
| `std::atomic<std::shared_ptr<const Config>>` | 38 | 200 |

Le motif imbriqué est à **5 % de la réponse canonique du manuel**. Il n'a besoin d'aucun code.

**Défi du besoin : ne rien ajouter à `copy_on_write`.** L'équipe a construit et mesuré les deux
correctifs candidats et les a rejetés, à raison : rendre `is_synchronizable<const copy_on_write<T>>`
vrai est sémantiquement correct mais mesure **344 ns/lecture contre 40**, une régression de 9× ; et
ajouter `store()`/`load()` dupliquerait le travail de `synchronized_value`. Ce qui manque n'est pas du
code, c'est **un motif nommé et une phrase dans `CLAUDE.md`** :

> `copy_on_write<T>` est une poignée d'**instantané par thread**, pas un canal de publication. Pour
> publier, mettez-la dans un `synchronized_value<copy_on_write<T>>` et lisez ainsi :
> ```cpp
> const copy_on_write<T> snapshot = [&] {
>     const auto guard = published->lock_shared();
>     return *guard;
> }();
> ```

La description actuelle dans `CLAUDE.md` — « A shared `T` read through `const` only » — dit
« shared » alors que le système de traits **interdit** de partager la poignée. C'est la phrase à
réécrire.

### 5.3 `synchronized_value` n'a pas de verrou multiple (E4)

Le virement entre deux comptes est le programme canonique. Aujourd'hui la seule chose qu'on puisse
écrire est verrouiller-l'un-puis-l'autre, et deux virements en sens inverse interbloquent. J'ai
exécuté le programme complet de l'équipe :

```
DEADLOCK: stuck after 1 of 400000 transfers
exit=2
DEADLOCK: stuck after 1 of 400000 transfers
exit=2
```

Un interblocage après **un seul** virement sur 400 000, deux fois de suite. Et le piège
`std::scoped_lock` est réel — j'ai vérifié les deux assertions :

```cpp
template <class Value>
constexpr bool scoped_lock_looks_ok = requires(Value& a, Value& b) { std::scoped_lock{a, b}; };
template <class Value>
constexpr bool has_try_lock = requires(Value& v) { v.try_lock(); };

static_assert(scoped_lock_looks_ok<account>, "std::scoped_lock{a, b} passes overload resolution");
static_assert(!has_try_lock<account>, "although the type is not Lockable at all");
```

Les deux tiennent. Le code passe la résolution de surcharge parce que `synchronized_value` a un membre
nommé `lock()`, puis produit trois erreurs au fond de libstdc++ :

```
/opt/homebrew/.../mutex:746:50: error: 'class threadsafe::synchronized_value<long long int>'
  has no member named 'unlock'; did you mean 'lock'?
/opt/homebrew/.../bits/unique_lock.h:159:34: error: 'std::unique_lock<threadsafe::synchronized_value<long long int> >::mutex_type'
  {aka 'class threadsafe::synchronized_value<long long int>'} has no member named 'try_lock'
/opt/homebrew/.../bits/unique_lock.h:203:24: error: ... has no member named 'unlock'; did you mean 'lock'?
```

L'équipe n'a pas proposé de correctif, en expliquant qu'un verrou multiple demanderait à `value_guard`
un chemin de construction en verrouillage différé, ce qui relâcherait des sémantiques délibérément
serrées. **Je propose une forme qui ne les relâche pas du tout** — et je l'ai appliquée, compilée,
exécutée trois fois et passée en régression :

```cpp
// ==== ÉDITION : include/threadsafe/details/synchronized_value.h ====
// (a) dans class synchronized_value, la section privée devient :

private:
    template <class Body, class... Values>
    friend decltype(auto) with_all(Body&& body,
                                   synchronized_value<Values>&... values);

    mutable mutex mutex_;
    T value_;
};

// (b) immédiatement après la classe, dans namespace threadsafe :

// Verrouiller plusieurs synchronized_value à la fois, dans l'ordre sans
// interblocage que choisit std::scoped_lock, et remettre leurs valeurs à `body`.
// Aucun garde ne s'échappe : chaque verrou est relâché quand `body` retourne, ce
// qui préserve la sémantique serrée de value_guard au lieu de la relâcher.
template <class Body, class... Values>
decltype(auto) with_all(Body&& body, synchronized_value<Values>&... values) {
    std::scoped_lock everything{values.mutex_...};
    return std::forward<Body>(body)(values.value_...);
}
```

Le programme de vérification complet, exécuté trois fois :

```cpp
// transfer.cpp
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

using account = threadsafe::synchronized_value<long long>;
constexpr int transfers = 200'000;
std::atomic<long long> progress{0};

struct transfer_task {
    std::shared_ptr<account> from;
    std::shared_ptr<account> to;
    void operator()() const {
        for (int step = 0; step < transfers; ++step) {
            threadsafe::with_all(
                [](long long& source, long long& destination) {
                    source -= 1;
                    destination += 1;
                },
                *from, *to);
            ++progress;
        }
    }
};

int main() {
    auto first = account::make(1'000'000LL);
    auto second = account::make(1'000'000LL);

    std::jthread watchdog{[](std::stop_token token) {
        long long previous = -1;
        int stalled_seconds = 0;
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const long long now = progress.load();
            stalled_seconds = (now == previous) ? stalled_seconds + 1 : 0;
            previous = now;
            if (stalled_seconds >= 3) {
                std::printf("DEADLOCK: stuck after %lld of %d transfers\n",
                            now, 2 * transfers);
                std::fflush(stdout);
                std::_Exit(2);
            }
        }
    }};

    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(transfer_task{first, second});
        launcher.launch_task(transfer_task{second, first});
    }
    watchdog.request_stop();

    const auto first_guard = first->lock_shared();
    const auto second_guard = second->lock_shared();
    std::printf("no deadlock: %lld + %lld = %lld\n", *first_guard, *second_guard,
                *first_guard + *second_guard);
}
```
```
no deadlock: 1000000 + 1000000 = 2000000    exit=0
no deadlock: 1000000 + 1000000 = 2000000    exit=0
no deadlock: 1000000 + 1000000 = 2000000    exit=0

PASS test_asynchronous_task_launcher.cpp   PASS test_smart_pointers.cpp
PASS test_containers.cpp                   PASS test_soundness_regressions.cpp
PASS test_copy_on_write.cpp                PASS test_synchronizable.cpp
PASS test_deferred_specialization.cpp      PASS test_synchronized_value.cpp
PASS test_diagnostics.cpp
PASS test_lifetime_aware.cpp
PASS test_sendable.cpp
```

**Six lignes, une amitié, aucun garde qui s'échappe, 11 UT sur 11 vertes, et le programme canonique de
l'interblocage n'interbloque plus.** La forme « passer un callable » est aussi la bonne pour une
conférence : elle rend visible que la section critique est une portée, et elle ne peut pas être
détournée en `auto g = ...;`. Notez qu'elle ne prend que des verrous **exclusifs** — c'est le bon
choix par défaut pour une transaction multi-objets, et `std::scoped_lock` ne sait de toute façon pas
mélanger exclusif et partagé.

### 5.4 Les manques que je ne comblerais pas

**E3 — pas de tranche de conteneur sans copie.** `std::span`, les pointeurs bruts et
`shared_ptr<const vector<T>>` sont tous refusés, donc la seule option sans copie est
`shared_ptr<synchronized_value<vector<T>>>` plus des bornes d'index portées à la main. **Aucun
correctif, et c'est justifié :** refuser `std::span` est *correct* sous les règles que la
bibliothèque énonce elle-même (`CLAUDE.md` : l'appropriation est transitive, le `const` derrière une
indirection n'est jamais cru), et une vue est par définition une vue sur le stockage d'autrui. Le coût
mesuré est de toute façon faible — 13 à 47 % contre un `std::span` nu (mesures de l'équipe, non
revérifiées par le lead) — donc c'est un constat d'ergonomie, pas de performance. Ce qui *est*
réparable, c'est la chasse au trésor en deux temps que produisent les diagnostics ; elle appartient au
[rapport 04](./04-diagnostics.md).

**SV-07 — `[[nodiscard]]` attrape deux façons sur quatre de laisser tomber un garde.** L'attribut est
sur `lock()`/`lock_shared()`, pas sur `value_guard` : `(void)sv.lock()`,
`static_cast<void>(sv.lock_shared())` et toute fonction utilisateur qui *retourne* un garde le
réduisent au silence. Et le `CMakeLists.txt` du projet n'ajoute ni `-Wall`, ni `-Wextra`, ni
`-Werror` : même les deux cas attrapés ne sont que des avertissements que la compilation ignore. Les
deux correctifs de l'équipe sont **vérifiés suite verte** et coûtent une ligne chacun :

```cpp
// ==== ÉDITION : include/threadsafe/details/synchronized_value.h ====
// [[nodiscard]] sur le TYPE, pas seulement sur lock() : un garde laissé tomber
// est un verrou pris et relâché au point-virgule, quelle que soit la façon dont
// on l'a obtenu — via lock(), ou via n'importe quelle fonction utilisateur qui
// en retourne un.
template <class T, class Lock>
class [[nodiscard]] value_guard {
```

```cmake
# ==== ÉDITION : CMakeLists.txt ====
target_compile_options(threadsafe INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Werror=unused-result>)
```

**À appliquer.** Les `(void)` explicites resteront hors de portée — c'est l'échappatoire que la norme
a conçue exprès, ce n'est pas un défaut de la bibliothèque. Mais le cas de l'accesseur, celui qu'une
vraie base de code rencontre, est fermé.

**SV-08 — pas d'initialisation par accolades, et `{3, 0}` ment.** `synchronized_value<std::vector<int>> v{{1,2,3}}`
ne compile pas (une liste entre accolades est un contexte non déduit pour `Args...`), et
`v{3, 0}` compile en signifiant `vector(3, 0)` — trois zéros. Le correctif de l'équipe est **vérifié
suite verte** :

```cpp
// ==== ÉDITION : include/threadsafe/details/synchronized_value.h ====
// Immédiatement avant `synchronized_value(const synchronized_value&) = delete;`,
// et ajouter <initializer_list> aux includes :

    // Une liste entre accolades est un contexte non déduit pour Args..., donc
    // sans ceci l'orthographe naturelle `synchronized_value<std::vector<int>> v{{1, 2, 3}}`
    // ne compile pas du tout, et `v{3, 0}` signifie silencieusement vector(3, 0).
    template <class Element>
        requires std::constructible_from<T, std::initializer_list<Element>&>
    explicit synchronized_value(std::initializer_list<Element> elements)
        : value_(elements) {}
```

```cpp
// ==== ÉDITION : include/threadsafe/details/copy_on_write.h ====
// Immédiatement avant `const T& operator*() const noexcept`,
// et ajouter <initializer_list> aux includes :

    template <class Element>
        requires std::constructible_from<T, std::initializer_list<Element>&>
    explicit copy_on_write(std::initializer_list<Element> elements)
        : ptr_(std::make_shared<T>(elements)) {}
```

C'est un **changement de comportement assumé** pour la seule orthographe qui compile aujourd'hui :
`sv{3, 0}` passe de trois zéros aux deux éléments `{3, 0}` — la réponse de `std::vector` aux mêmes
accolades. Rien dans `tests/` ne dépend de l'ancien sens. Pour une bibliothèque lue depuis une scène,
reproduire *le* piège d'initialisation de C++ à l'intérieur d'une bibliothèque de sûreté est un mauvais
signal ; à appliquer.

### 5.5 Verdict : ce qui mérite sa place dans une bibliothèque PÉDAGOGIQUE

| manque | ajout proposé | vérifié | bibliothèque pédagogique ? |
|---|---|---|---|
| pas d'arrêt, pas de jointure | `request_stop()` + `join()`, 18 lignes | oui, par moi, 11/11 | **OUI** — sans cela `launch_scoped_task` interbloque sur une tâche coopérative, et le motif `stop_token` est *l'*idiome C++20 à enseigner |
| pas de verrou multiple | `with_all(body, a, b, ...)`, 6 lignes | oui, par moi, 11/11 | **OUI** — le virement entre comptes est le programme canonique ; une bibliothèque de sûreté qui le fait interbloquer perd son argument |
| `[[nodiscard]]` percé | attribut sur le type + `-Werror=unused-result` | oui, équipe, 11/11 | **OUI** — deux lignes pour rendre exécutoire une propriété que l'en-tête qualifie lui-même de « load-bearing » |
| pas d'accolades | surcharge `initializer_list` ×2 | oui, équipe, 11/11 | **OUI** — c'est la première ligne que l'auditoire écrira |
| pas de concept `synchronizable` | `concept synchronizable`, 3 lignes | oui, par moi, 11/11 | **OUI** — supprime la seule asymétrie de nommage |
| liste blanche fermée | `is_transparent_wrapper`, ~15 lignes | oui, par moi, 11/11 | **OUI** — rachète les dix types réalistes et rend la promesse de la liste blanche enseignable |
| mutex figé | second paramètre de template | oui, par moi, 11/11 | **OUI** — et en prime transforme le piège ODR en erreur d'édition de liens |
| pas de canal de résultat | *rien* | — | **NON** — `<future>` en a déjà un ; il suffit de cesser de le refuser (TLS-06) |
| `copy_on_write` ne publie pas | *rien, une phrase de doc* | — | **NON** — les deux correctifs candidats ont été mesurés et rejetés ; le motif imbriqué est à 5 % de l'optimum |
| pas de tranche sans copie | *rien* | — | **NON** — refuser `std::span` est une règle assumée, pas un bug ; un `owned_slice<T>` serait un nouveau composant, pas une réparation |
| notification de fin de tâche | *rien* | — | **NON** — liste de souhaits de production ; `join()` suffit à un exemple, et une file de complétion est un autre exposé |
| macro `_TEMPLATE` | *rien* | (écrite et vérifiée, mais) | **NON** — la spécialisation partielle écrite à la main fait deux lignes et *est* la leçon |
| lambdas capturantes acceptées | *impossible* | — | **NON** — GCC 16 rapporte `nsdm=0` pour une fermeture de 4 octets ; le refus est la bonne réponse |
| macro utilisable dans un espace de noms | *impossible* | — | **NON** — `[temp.expl.spec]`, limite du langage |

---

## 6. Ce qui a résisté

Il faut le dire aussi fort que le reste : **le noyau est solide**. Les attaques suivantes n'ont rien
cassé.

- **La marche structurelle est infalsifiable par astuce de disposition.** Dix formes essayées, dix
  refusées, pour `is_sendable` **et** `is_synchronizable<const T>` : un membre `[[no_unique_address]]`
  tenant un emprunt ; une union anonyme avec une alternative `int*` ; un type struct imbriqué anonyme
  tenant un pointeur ; un diamant à base virtuelle tenant un pointeur ; une base privée tenant un
  pointeur ; une paire de champs de bits à côté d'un pointeur brut ; un tableau de structs emprunteurs ;
  un membre référence nu ; un tableau de pointeurs ; et un struct à trois niveaux dont le membre le
  plus profond est un `int*`. `access_context::unchecked()` atteint réellement les sous-objets privés
  et inaccessibles, et la normalisation `remove_extent`/`remove_cv` tient à travers les tableaux et la
  cv-qualification.
- **`has_unreflectable_state` est la défense porteuse et elle tient.** GCC 16 rapporte `nsdm=0` pour
  une fermeture de 4 octets ayant capturé un `int` par valeur, et `nsdm=0` pour une de 8 octets ayant
  capturé par référence. Sans ce garde, `[&local]{ return local; }` serait déclaré *sendable*.
- **`mutable` est attrapé exactement là où il faut.** `is_synchronizable<const MutableCache>` est faux
  pour `mutable int cache;`, avec l'explication correcte.
- **La marche de chemin profond est exacte et complète.** Vingt niveaux nommés sur
  `14_deep_chain`, soixante sur `27_chain60` en 0,76 s, sans troncature, sans points de suspension,
  sans limite de profondeur. Les bases sont orthographiées différemment des membres, exprès :
  `'Root::(base A)::(base B)::pointer (int*) is a pointer or a reference: ...'`.
- **`is_lifetime_aware` explique parfaitement les vues.** `'std::basic_string_view<char> is a borrowed
  range: a view over someone else's storage, it does not keep its elements alive'`. Le test
  `borrowed_range` est la règle la plus tranchante de la bibliothèque.
- **Spécialiser un trait pour son propre type marche dans toutes les formes attaquées, et compose.**
  Type simple, type dans un espace de noms imbriqué (`acme::io::detail::Channel`), type mémoïsant,
  template de classe via spécialisation partielle — tous compilent proprement. Et un `my_vector<T,Alloc>`
  spécialisé à la main continue de marcher à travers la cv-qualification
  (`is_sendable_v<const my_vector<int>>`) et à travers l'imbrication
  (`is_sendable_v<std::vector<my_vector<int>>>`). Je l'ai revérifié moi-même.
- **La spécialisation tardive DANS une unité de traduction est attrapée.**
  `error: specialization of threadsafe::is_synchronizable<Widget> after instantiation`. Seul le cas
  inter-UT est silencieux.
- **La cv-qualification ne laisse jamais fuir une mauvaise réponse.** `is_sendable_v<const Plain&>`,
  `<const Plain*>` et `<const std::string&>` sont tous correctement faux, et `is_synchronizable_v<const Plain>`
  est vrai pendant que `is_synchronizable_v<Plain>` est faux.
- **`std::array` est traité correctement dans tous les cas sondés** : taille zéro, tableaux de
  tableaux, tableaux de types vouchés, tableaux de type à membre `mutable` — 15 assertions sur 15.
- **Le mécanisme à deux surcharges du lanceur est le bon idiome et il marche.** La surcharge
  contrainte n'est jamais sélectionnée pour un mauvais `F`, la surcharge de repli instancie
  `explain_launch_task`, et l'ordre à l'intérieur rejoue fidèlement le concept — la non-mobilité est
  signalée **avant** tout échec de trait. Quatre tentatives pour faire dégénérer le repli en un simple
  `false` (requires-expression nue, concept écrit à la main, `std::is_invocable_v` sur une lambda
  enveloppante, appel non évalué) ont toutes échoué.
- **`std::function`, `std::move_only_function` et le résultat de `std::bind` sont refusés par les deux
  points d'entrée.** Aucun callable à effacement de type n'est passé, dans aucun scénario.
- **Ne signaler que le PREMIER membre fautif est le bon choix**, pas un défaut : c'est ce que fait
  tout compilateur.
- **Tous les négatifs tiennent encore sous la liste blanche élargie de Q3** : `span<int>`,
  `span<const int>`, `string_view` restent non-*sendable* et non-*lifetime-aware* ;
  `initializer_list<int>` reste non-*lifetime-aware* ; `expected<int*,string>`, `flat_map<int,int*>`,
  `stack<int*>`, `vector<int*>`, `chrono::duration<int*,ratio<1>>` restent non-*sendable* ;
  `complex<double>` reste non-synchronizable sans qualification pendant que `const complex<double>`
  est lisible.
- **La surcouche de la bibliothèque ne coûte essentiellement rien.** `synchronized_value<std::map>`
  égale un `std::shared_mutex` écrit à la main à 3 % près de 1 à 12 threads. Chaque défaut de
  performance trouvé est dans un choix de **politique**, jamais dans l'enveloppe.

---

## 7. Priorités

**À appliquer maintenant — tous suite verte, aucun risque :**

| ordre | quoi | constat | lignes | vérifié par |
|---|---|---|---|---|
| 1 | écrire un `README` avec le programme de la [section 1.2](#12-le-plus-petit-programme-complet) et la règle ODR de la [section 4.4](#44-latténuation-que-je-recommande) | — | ~40 | — |
| 2 | `is_user_provided` à la place de `is_defaulted`/`is_deleted` | TC-3 | 3 | équipe |
| 3 | rendre les onze en-têtes autonomes | Q4 | ~20 | équipe |
| 4 | `concept synchronizable` | Q10 | 3 | **moi**, 11/11 |
| 5 | `is_transparent_wrapper` — point de personnalisation | Q6 | ~15 | **moi**, 11/11 |
| 6 | `synchronized_value<T, Mutex>` | Q7 | remplacement de fichier | **moi**, 11/11 |
| 7 | liste blanche élargie + spécialisations `vocabulary.h` | Q3, TLS-04/05/06, TC-7 | ~80 | équipe |
| 8 | `[[nodiscard]]` sur `value_guard` + `-Werror=unused-result` | SV-07 | 2 | équipe |
| 9 | surcharges `initializer_list` | SV-08 | 12 | équipe |
| 10 | `request_stop()` + `join()` sur le lanceur | — | 18 | **moi**, 11/11 |
| 11 | `with_all(body, a, b, ...)` | E4 | 6 | **moi**, 11/11 |
| 12 | `traits_settled` + sa documentation | Q1 | ~25 | **moi**, 11/11 |

**À trancher, pas à appliquer :**

| quoi | constat | ce qui casse |
|---|---|---|
| prédicat de détournement exact (`copy_move_is_compiler_generated`) | TC-8 | `tests/test_sendable.cpp:202` — l'assertion qui documente la limitation |
| disposition inconditionnelle de `synchronized_value` | TC-4 | `tests/test_synchronized_value.cpp` lignes 120, 122, 125, 131, 134 — et jusqu'à 119× de coût d'exécution |
| voucher les primitives de synchro et `thread::id` | ADV-11 | non passé en régression ; décision de politique, à trancher puis à vérifier |

**À ne pas faire :** un canal de résultat sur le lanceur, un `store()`/`load()` sur
`copy_on_write`, un `owned_slice<T>`, une macro `_TEMPLATE`, et toute tentative d'accepter les
lambdas capturantes.

**À écrire, pas à coder :** trois phrases. Que `copy_on_write` est une poignée d'instantané par thread
et que la publication passe par `synchronized_value<copy_on_write<T>>`. Que la réflexion ne voit pas
les captures d'une lambda et qu'un foncteur nommé est la forme à écrire. Et que les traits répondent
à propos d'un type *tel qu'il est visible depuis l'unité de traduction courante* — la phrase de la
[section 4.4](#44-latténuation-que-je-recommande), qui est aussi la meilleure diapositive de la
conférence.
