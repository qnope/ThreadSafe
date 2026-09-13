# Audit ThreadSafe — détails

Rapport détaillé de l'audit décrit dans [audit-synthese.md](audit-synthese.md).
Chaque constat porte le code fautif, la preuve, et la correction lorsqu'elle existe.

**Méthode.** Douze dimensions auditées en parallèle, chaque constat ensuite confié à un
vérificateur indépendant chargé de le réfuter par recompilation. 141 sondes compilées avec
`g++-16 -std=c++26 -freflection -I include`, plusieurs exécutées. Les constats dont la
correction proposée s'est révélée fausse ou incomplète sous la contre-attaque portent la
mention **⚠ correction ajustée**. Aucun fichier du dépôt n'a été modifié : les correctifs ont
été validés sur des copies des en-têtes.

**Notation des sévérités.** *Critique* : un type unsafe est accepté. *Majeur* : bug réel, API
piégeuse, ou faux négatif bloquant un usage courant. *Mineur* : ergonomie, hygiène, perf,
faux négatif secondaire. *Info* : limite assumée, constat positif, suggestion.

---

## Critique

### C1 — `const std::unique_ptr<T, D>` est vouché pour n'importe quel deleter

`include/threadsafe/details/smart_pointers.h:81-84`

```cpp
template <class T, class D>
struct is_unsafe_synchronizable<const std::unique_ptr<T, D>>
    : std::bool_constant<detail::pointee_answer(^^T, is_synchronizable_type) &&
                         is_synchronizable_v<const D>> {};
```

La justification implicite de ce vouch est la propriété exclusive : « `unique_ptr` possède son
pointé, donc aucun alias non-const n'existe ». Le deleter n'est contraint que par
`is_synchronizable_v<const D>`, qu'un deleter vide satisfait trivialement. Or un deleter no-op
est exactement l'idiome du **pointeur observateur non propriétaire** — le type du deleter
annonce lui-même la non-propriété, et cette annonce est ignorée.

```cpp
struct NoOpDeleter { void operator()(const int *) const noexcept {} };
using ObserverPtr = std::unique_ptr<const int, NoOpDeleter>;

int hot = 0;
ObserverPtr observer{&hot};   // legal, pas de release(), pas de double delete

static_assert(threadsafe::is_synchronizable_v<const ObserverPtr>);          // passe
static_assert(threadsafe::synchronized_value<ObserverPtr>::shared_readable); // passe
```

`shared_readable` valant `true`, `synchronized_value` choisit un `shared_mutex` et `lock_shared()`
distribue des lectures concurrentes de `**guard`, non ordonnées avec le `++hot` que le code
d'origine reste libre d'exécuter via son alias. Data race bénie de bout en bout, dans du code
idiomatique et sans UB préalable.

La bibliothèque applique pourtant la bonne logique ailleurs : `const std::shared_ptr<const int>`
est **rejeté** (son vouch passe par `pointee_is_synchronizable`, qui fait `remove_cv`) pour
exactement ce risque d'aliasing, et un membre `const int * const` est rejeté de même.
Le cas `default_delete` reste défendable : y attacher un pointeur aliasé impose un `release()`
ou un double-delete UB, donc la convention de propriété tient.

**Correction** (validée : les quatre assertions attendues passent, aucun test du dépôt ne casse) :

```cpp
template <class T, class D>
struct is_unsafe_synchronizable<const std::unique_ptr<T, D>>
    : std::bool_constant<
          (std::is_same_v<D, std::default_delete<T>> ||
           is_synchronizable_v<std::remove_cv_t<T>>) &&
          detail::pointee_answer(^^T, is_synchronizable_type) &&
          is_synchronizable_v<const D>> {};
```

La confiance dans le `const` du pointé exige la propriété réelle (`default_delete`) ; avec un
deleter custom, seul un pointé **pleinement** synchronizable (`std::atomic<int>`) survit à
l'aliasing. Un deleter réellement propriétaire (`fclose`, `free`) devient conservativement
refusé — l'utilisateur peut le re-voucher lui-même, ce qui est la discipline de la bibliothèque.

---

## Majeur

### M1 — `copy_on_write::operator*() &&` : `noexcept` mensonger et copie inconditionnelle

`include/threadsafe/details/copy_on_write.h:27`

```cpp
T operator*() && noexcept { return *ptr_; }
```

Ce constat a été trouvé indépendamment par quatre auditeurs (robustesse, simplicité,
performance runtime, API).

`ptr_` est un `std::shared_ptr<T>`, donc `*ptr_` est une lvalue : `return *ptr_;` invoque
`T(const T&)` sans élision possible. Deux problèmes sur la même ligne.

*Le `noexcept` est faux.* Pour tout `T` dont la copie alloue, un `bad_alloc` traverse la
frontière `noexcept` et appelle `std::terminate`. Démontré à l'exécution avec un `T` dont la
copie lance : `terminate called after throwing an instance of 'std::runtime_error'`, exit 134,
l'exception n'atteint jamais le `catch`. Le mensonge est aussi visible dans le système de types :
`noexcept(*std::declval<cow<std::string>&&>())` vaut `true` alors que
`std::is_nothrow_copy_constructible_v<std::string>` vaut `false` — toute métaprogrammation en
aval (garantie forte de `vector`) est trompée. L'incohérence est interne à la classe :
`as_mutable() &&`, qui fait la même copie, est honnête.

*La copie est inconditionnelle.* Quand `use_count() == 1`, le `T` pourrait être déplacé hors du
bloc. Mesuré (`-O2`, type instrumenté) :

```
unique  *std::move(cow):              copies=1 moves=0   (optimal : 0 copies, 1 move)
unique  std::move(cow).as_mutable():  copies=0 moves=1
shared  *std::move(cow):              copies=1 moves=0   (correct)
```

Les deux chemins de drain rvalue de la même classe divergent. Le test `use_count() != 1` est
déjà présent ligne 33 : le correctif n'invente aucune analyse nouvelle.

**Correction** (mesures après patch : `unique → copies=0 moves=1`, `shared → copies=1 moves=0`,
`noexcept` faux, exception propagée, les 12 tests compilent) :

```cpp
T operator*() &&
  requires std::copy_constructible<T>
{
  if (ptr_.use_count() != 1)
    return *ptr_;
  std::atomic_thread_fence(std::memory_order_acquire);
  return std::move(*ptr_);
}
```

La clause `requires` répare au passage la requires-expression menteuse décrite en m10.
Une fois ce correctif appliqué, `as_mutable() &&` peut se réécrire `return *std::move(*this);`
(vérifié équivalent, et meilleur d'un move sur le chemin partagé) — voir m11.

### M2 — La `T&` de `as_mutable()` n'est exclusive qu'à l'instant de l'appel

`include/threadsafe/details/copy_on_write.h:30-38`

```cpp
T &as_mutable() &
  requires std::copy_constructible<T>
{
  if (ptr_.use_count() != 1)
    ptr_ = std::make_shared<T>(*ptr_);
  else
    std::atomic_thread_fence(std::memory_order_acquire);
  return *ptr_;
}
```

La promesse du type est « `as_mutable()` copie d'abord dès que le bloc est partagé ». Elle n'est
vraie qu'au moment de l'appel : rien n'invalide la référence rendue quand le bloc redevient
partagé ensuite.

```cpp
int &escaped = cow_value.as_mutable();        // use_count == 1, pas de détach
launcher.launch_task([](copy_on_write<int> shared_copy) {
  int observed = *shared_copy;                // lecture du MEME bloc
}, cow_value);                                // la copie re-partage le bloc
escaped = 42;                                 // ecriture non synchronisee
```

Compile sans le moindre diagnostic. Chaque réponse de trait est individuellement correcte :
`copy_on_write<int>` est légitimement vouché sendable et lifetime-aware, et la surcharge
contrainte de `launch_task` est bien celle qui est sélectionnée. Contrairement à l'échappement
de guard (M3), qui demande de sauver un pointeur hors d'une portée — geste visiblement suspect —,
ici chaque étape paraît légitime.

**Pas de correction possible** : aucun trait C++ ne voit une référence locale. C'est précisément
ce que le borrow checker ferme dans `Arc::make_mut`, qui emprunte l'`Arc` mutablement et interdit
donc de le cloner tant que le `&mut T` vit. Excellent point de conférence.

Mitigations, par ordre d'utilité :

1. **Documenter dans l'en-tête** que la `T&` n'est exclusive que jusqu'à la prochaine copie du
   handle. Aujourd'hui ni le header ni les tests ne le mentionnent, et la formulation du
   `CLAUDE.md` est littéralement fausse dès qu'une référence survit.
2. Offrir une API en fermeture qui borne visuellement la vie de la référence — à présenter comme
   ergonomie, **jamais** comme garantie (la fermeture peut toujours faire échapper la référence
   ou copier le handle) :

```cpp
template <class Function>
  requires std::copy_constructible<T>
decltype(auto) write(Function &&function) & {
  return std::forward<Function>(function)(as_mutable());
}
```

### M3 — Une référence fuit hors de la section critique via un guard nommé

`include/threadsafe/details/synchronized_value.h:23-31, 75-81`

Les surcharges `&&` supprimées ne bloquent que le guard **temporaire**. Quatre échappatoires
compilent silencieusement :

```cpp
int &leak_by_return(sync_int &sv) {
  auto guard = sv.lock();
  return *guard;                    // la reference survit au unlock
}

int *leak_by_pointer(sync_int &sv) {
  int *escaped = nullptr;
  { auto guard = sv.lock(); escaped = &*guard; }   // mutex relache ici
  return escaped;                   // chemin d'acces non garde
}

int &leak_by_value_param(sync_int &sv) {
  return [](sync_int::guard g) -> int & { return *g; }(sv.lock());
}                                   // elision garantie : ni copie ni move invoques

struct GuardHolder { sync_int::guard guard; };
GuardHolder holder{sv.lock()};      // meme mecanisme : le guard se loge dans un agregat
```

Les cas (C) et (D) sont les plus intéressants : l'élision de copie garantie (C++17) initialise
directement depuis la prvalue de `lock()`, **sans jamais invoquer** le constructeur de copie
supprimé ni le move implicitement supprimé. Le commentaire de `tests/test_synchronized_value.cpp:81-83`
(« a movable guard could be lodged in an aggregate and travel ») est donc inexact sur son premier
point : le logement compile, seul le *transport* est empêché.

Le canal inter-thread reste fermé, ce qui évite la qualification critique — vérifié :
`!is_sendable_v<int*>`, `!is_sendable_v<guard>`, `!is_sendable_v<GuardHolder>`,
`!std::movable<GuardHolder>`. Mais la course de données (pointeur fuité utilisé pendant qu'un
autre thread tient le verrou) compile sans un mot, ce qui écorne la promesse
« safety checked entirely at compile time ».

**Pas de correction complète possible** — limite partagée par `std::unique_lock`,
`folly::Synchronized` et le `std::synchronized_value` proposé ; Rust la ferme par le borrow
checker. Mitigations :

1. documenter que `&*guard` et le retour de `*guard` sont hors contrat ;
2. corriger le commentaire du test 81-83 ;
3. fournir une API en fermeture (voir m14), en sachant que son `static_assert` de filtrage est
   une heuristique : une struct enveloppant un `T*` retournée par valeur passe au travers
   (démontré).

### M4 — `launch_scoped_task` est entièrement synchrone

`include/threadsafe/details/asynchronous_task_launcher.h:70-75`

```cpp
template <typename F, typename... Args>
    requires launchable_scoped_task<F, Args...>
void launch_scoped_task(F f, Args... args) {
    std::jthread task{std::move(f), std::move(args)...};
    task.join();
}
```

Mesuré : deux tâches scoped dormant 300 ms chacune prennent **604 ms** (« parallel would be ~300 »).
L'appelant bloque, deux tâches scoped ne peuvent jamais s'exécuter en parallèle — dans une classe
nommée `asynchronous_task_launcher`, via une méthode nommée `launch_`.

Ce `join()` immédiat n'est pas un détail : **c'est la preuve de sûreté** de tout l'assouplissement
des contraintes. C'est lui qui autorise `std::reference_wrapper` vers un objet de la pile
(`scoped_task_participant` n'exige pas `lifetime_aware`), et il tient sur deux lignes sans un
commentaire. Un refactoring qui différerait le join rendrait instantanément unsound tous les
emplois testés avec `std::ref`.

Conséquence : le cas réellement utile — plusieurs tâches empruntantes concurrentes, jointes en fin
de portée — est **inexprimable**. Vérifié : `static_assert(!launchable_task<Work, std::reference_wrapper<Shared>>)`
compile, donc le seul chemin acceptant un emprunt est le chemin bloquant.

**⚠ Correction ajustée.** Le `task_scope` proposé initialement (join dans le destructeur)
**introduit une faille que le code actuel n'a pas** : l'ordre de destruction C++ étant l'inverse
de l'ordre de déclaration, tout objet déclaré *après* le `task_scope` est détruit *avant* que
`~task_scope` ne joigne les threads. Démontré par compilation :

```cpp
task_scope scope;
Shared borrowed{std::string(64, 'a')};   // detruit AVANT ~task_scope
scope.launch(work, std::ref(borrowed));  // use-after-free
```

Ce qu'il faut faire à la place, par ordre de coût :

```cpp
    std::jthread task{std::move(f), std::move(args)...};
    task.join(); // the immediate join is the safety proof: borrowed
                 // (non lifetime-aware) args outlive the task because
                 // the caller's frame is still alive right here
```

et renommer (`run_scoped_task`) ou documenter explicitement que « scoped » signifie
« l'appelant attend ».

### M5 — Aucune synchronisabilité pleine n'est prouvée structurellement

`include/threadsafe/details/synchronizable_base.h:53-54`

```cpp
  if (!is_const(type))
    return false;
```

Hors claims unsafe, types fonction et tableaux, **un type classe non-const n'est jamais prouvé
synchronizable**, quels que soient ses membres :

```cpp
struct Counters { std::atomic<int> hits; std::atomic<int> misses; };

static_assert(is_synchronizable_v<std::atomic<int>>);      // vrai
static_assert(is_synchronizable_v<std::atomic<int>[4]>);   // vrai (branche tableau)
static_assert(!is_synchronizable_v<Counters>);             // faux !
static_assert(!is_sendable_v<Counters&>);                  // cascade
static_assert(!is_sendable_v<Counters*>);
```

Le bloc de compteurs atomiques partagé par référence est le cas d'usage canonique d'une
bibliothèque de thread-safety (Rust dérive `Sync` structurellement pour un struct d'`AtomicI32`),
et le tableau C brut passe là où le struct qui l'enveloppe échoue. Cascade vérifiée : un membre
`mutable SafeCounter c;` est rejeté — la branche `mutable` exige la synchronisabilité **pleine**
du type du membre, structurellement impossible pour une classe — alors que le même membre
non-`mutable` passe via `add_const`.

**⚠ Correction ajustée.** La dérivation structurelle proposée est saine sur le principe, mais
telle quelle elle rend les types **classe vides** vacuously synchronizables, y compris polymorphes,
et casse quatre assertions existantes (`test_synchronizable.cpp:64`, `test_polymorphic.cpp:57, 75, 82`).
Le garde correctif a été trouvé et validé — avec lui, les douze fichiers de tests compilent et tous
les gains annoncés tiennent :

```cpp
  if (!is_const(type)) {
    if (is_volatile(type) || is_union_type(type) || !is_walkable_type(type))
      return false;

    // sans ce garde, tout type classe vide devient synchronizable
    if (bases_of(type, context).empty() &&
        nonstatic_data_members_of(type, context).empty())
      return false;

    for (auto base : bases_of(type, context))
      if (!is_synchronizable_type(type_of(base)))
        return false;

    for (auto member : nonstatic_data_members_of(type, context)) {
      const auto member_type = type_of(member);

      if (is_reference_type(member_type)) {
        if (!pointee_answer(remove_cvref(member_type), is_synchronizable_type))
          return false;
      } else if (!is_synchronizable_type(remove_cv(member_type))) {
        return false;
      }
    }

    return true;
  }
```

Nuance relevée par le vérificateur : la ligne 53 joue aussi un rôle de **terminaison de récursion**.
La dérivation structurelle pleine reste donc exposée aux types récursifs (voir M7), et les unions
sont volontairement exclues (membres superposés).

Ce constat est à arbitrer : c'est un faux négatif, pas un défaut de sûreté, et le refus actuel est
cohérent avec « seule la confiance est affirmée ». Un struct d'atomiques peut aussi porter des
invariants inter-membres qu'une dérivation automatique ignorerait.

### M6 — Pointeur vers type incomplet : erreur brute au lieu du `static_assert` documenté

`include/threadsafe/details/sendable.h:47-49`

```cpp
  if (is_reference_type(type))
    return is_dynamic_type_known(remove_cvref(type)) &&
           is_synchronizable_type(remove_cvref(type));
```

Le contrat documenté est qu'un type incomplet empoisonne la question via le message soigné de
`detail::assert_queryable_type`. Mais pour `is_sendable_v<Fwd*>` ou `is_sendable_v<Fwd&>`,
`is_dynamic_type_known` appelle `std::meta::is_polymorphic_type` sur le type incomplet **avant**
que la récursion n'atteigne le `_v` qui porte le `static_assert` :

```
utils.h:24:30: error: type trait 'std::meta::is_polymorphic_type' preconditions not satisfied
```

Une erreur brute de la bibliothèque de réflexion, sur un motif courant (forward declaration +
pointeur, pimpl à la main), dans une bibliothèque destinée à être projetée en conférence.
L'incohérence est interne : `pointee_answer` (`utils.h:27-30`) utilise déjà le bon ordre, et
`unique_ptr<Impl>` incomplet donne bien le message amical.

**Correction** (après patch : le message documenté apparaît pour `Fwd*`, les 12 tests compilent,
sémantique inchangée pour les types complets) :

```cpp
  if (is_reference_type(type))
    return is_synchronizable_type(remove_cvref(type)) &&
           is_dynamic_type_known(remove_cvref(type));
```

### M7 — Un type possédant récursif ne peut pas être interrogé

`include/threadsafe/details/lifetime_aware.h:68`, via `utils.h:9`

```cpp
struct Node {
  int value;
  std::unique_ptr<Node> next;
};
static_assert(threadsafe::is_lifetime_aware_v<Node>);
```

```
utils.h:9:23: error: the value of 'threadsafe::is_lifetime_aware_v<Node>'
              is not usable in a constant expression
```

L'évaluation instancie la claim smart-pointer de `unique_ptr<Node>`, qui redemande
`is_lifetime_aware_v<Node>` pendant l'initialisation de cette même variable template. Même
erreur pour `Tree { std::vector<Tree> }` et pour la récursion mutuelle `Expr`/`BinaryOp`.
`is_sendable_v<Node>` échoue identiquement.

Le diagnostic ne mentionne ni le cycle ni le remède, en contraste net avec les messages de
`assert_queryable_type`. Ces types (liste chaînée, arbre, AST) sont extrêmement réalistes comme
arguments de `launch_task`, et Rust les résout nativement — les auto-traits y sont coinductifs,
un cycle vaut oui.

**Contournement vérifié** (compile, exit 0), à documenter en attendant mieux :

```cpp
template <>
struct threadsafe::is_unsafe_lifetime_aware<Node> : std::true_type {};

static_assert(threadsafe::is_lifetime_aware_v<Node>);                   // OK
static_assert(threadsafe::is_lifetime_aware_v<std::unique_ptr<Node>>);  // OK
```

La couche unsafe est consultée avant le walk, ce qui casse le cycle. Le vouch doit être écrit
avant la première question sur `Node`.

Un vrai correctif demanderait une détection de cycle (ensemble de types en cours de visite
propagé à travers le walk), ce qui casserait la récursion via `_v` — donc la mémoïsation **et**
le point d'entrée des spécialisations utilisateur. C'est une refonte, pas un patch. À défaut,
ajouter un test montrant le vouch d'un type récursif.

### M8 — Un constructeur forwarding rend le type silencieusement non-sendable

`include/threadsafe/details/utils.h:66-74`

```cpp
inline consteval bool may_hijack_copy_move(std::meta::info function) {
  if (is_constructor_template(function))
    return true;
  ...
}
```

Le rejet est un choix conservateur délibéré (un constructeur template peut capturer la copie),
mais il touche une forme extrêmement répandue. Isolé par compilation, à membre identique :

```cpp
struct NoCtor    { std::string name; };                                    // sendable
struct PlainCtor { explicit PlainCtor(std::string); std::string name; };   // sendable
struct FwdCtor   { template <class U> explicit FwdCtor(U&&); std::string name; }; // REFUSE
```

La « templateness » du constructeur est la cause unique et isolée ; les trois traits tombent
ensemble, et le shape variadique `emplace` est également rejeté. Or rien dans le diagnostic ne
le dit : le message s'arrête à « every argument must be movable, sendable and lifetime-aware »
puis « the expression `is_task_participant_v<T>` evaluated to `false` ». L'utilisateur voit un
refus sur un type visiblement trivial et doit bissecter ses membres — qui ne sont pas en cause.

**⚠ Correction ajustée.** L'affinement proposé de `may_hijack_copy_move` (ne rejeter que les
constructeurs template pouvant lier le type lui-même) est **non implémentable** : la seule
primitive plausible, `std::meta::can_substitute`, discrimine correctement sur des constructeurs
*déclarés sans corps*, mais provoque une **erreur dure** dès que le constructeur a un corps réel —
elle instancie la mem-initializer-list, qui n'est pas dans le contexte immédiat, donc pas de SFINAE.

La voie réaliste est donc le diagnostic : documenter le remède au point d'usage et fournir un

```cpp
explain<T>()  // "type is not walkable: it declares a constructor template —
              //  vouch for it with threadsafe::is_unsafe_sendable if you know it is safe"
```

qui distingue « non-walkable (copie/move/dtor écrits, ou constructeur template) » de
« un membre a échoué ». Voir aussi m17.

---

## Mineur

### m1 — Incohérence tableau C / `std::array` sur les atomiques

`synchronizable_base.h:50-51` vs `allowed_std_wrappers.h:88-92`

La branche tableau du walk précède le test `is_const`, donc `std::atomic<int>[2]` est pleinement
synchronizable. Mais le vouch `std_wrapper` ne couvre que la forme `const T`, donc
`std::array<std::atomic<int>, 2>` non-const meurt sur `!is_const`.

```cpp
static_assert(is_synchronizable_v<std::atomic<int>[2]>);                    // vrai
static_assert(!is_synchronizable_v<std::array<std::atomic<int>, 2>>);       // faux
static_assert(is_sendable_v<std::atomic<int>(&)[2]>);                       // vrai
static_assert(!is_sendable_v<std::array<std::atomic<int>, 2>&>);            // faux
```

Même disposition mémoire, même sémantique d'accès — et la recommandation C++ moderne est
précisément `std::array`. **Correction** (validée, pas d'ambiguïté avec la spécialisation `const T`
du `std_wrapper`, `std::array<int,2>` reste rejeté) :

```cpp
template <class T, std::size_t N>
struct is_unsafe_synchronizable<std::array<T, N>>
    : std::bool_constant<is_synchronizable_v<T>> {};
```

### m2 — `const copy_on_write<T>` n'est jamais reconnu lisible concurremment

`copy_on_write.h:46-52` — aucune spécialisation `is_unsafe_synchronizable` n'existe, et le walk
est bloqué avant de commencer (le constructeur variadique déclenche `may_hijack_copy_move`).

À travers `const`, la classe n'expose que `operator*`/`operator->` rendant `const T&`/`const T*` ;
`ptr_` n'est jamais réassigné. C'est aussi sûr que `const std::shared_ptr<T>`, que la bibliothèque
vouche — et même plus, puisque `cow` ne rend jamais de `T&` en const et ne peut pas pointer un
type dynamique différent.

Conséquence silencieuse : `synchronized_value<Config>` avec `Config { copy_on_write<...> }` voit
`shared_readable` dégradé à `false` — `std::mutex` exclusif au lieu de `shared_mutex`, les
lecteurs purs sérialisés sans aucun signal.

**Correction** (validée sur copie patchée, les 12 tests passent) :

```cpp
template <class T>
struct is_unsafe_synchronizable<const copy_on_write<T>>
    : std::bool_constant<is_synchronizable_v<const T>> {};
```

### m3 — `is_synchronizable_v<T&>` répond `false` silencieusement

`synchronizable_base.h:53`. Une référence n'a pas de cv de tête, donc aucune branche ne la traite
et le walk tombe sur `!is_const → false` :

```cpp
static_assert(!is_synchronizable_v<std::atomic<int>&>);  // faux silencieux
static_assert(is_sendable_v<std::atomic<int>&>);         // vrai
```

Asymétrie piégeuse pour un lecteur venant de Rust (`&T: Sync ⇔ T: Sync`). Les membres de type
référence sont, eux, correctement traités par la branche dédiée. Deux directions, toutes deux
cohérentes : répondre via le référent
(`if (is_reference_type(type)) return pointee_answer(remove_cvref(type), is_synchronizable_type);`
avant la ligne 53 — ne change aucun verdict existant), ou rejeter explicitement les références
dans `assert_queryable_type` comme pour `void`.

### m4 — `std::expected` absent de `allowed_std_wrappers`

`allowed_std_wrappers.h:29-48`. Trouvé indépendamment par deux auditeurs.

`optional` et `variant` sont vouchés, `std::expected` ne l'est pas — alors qu'il possède ses
données exactement comme `optional` et qu'il est le type de retour vocabulaire par excellence.
Les trois traits répondent `false`, donc un `expected` possédant ne peut pas être passé à
`launch_task`. Même situation pour `std::flat_map`/`flat_set` (C++23) et `std::inplace_vector`
(C++26), vérifiée sur GCC 16.

**Correction** (validée : `expected<int, std::string>` devient sendable et lifetime-aware,
`expected<std::string_view, int>`, `expected<int, const char*>` et `expected<int*, int>` restent
correctement rejetés) :

```cpp
#include <expected>
// ...
    ^^std::optional,
    ^^std::expected,
    ^^std::variant,
```

### m5 — `= default` hors classe : la valeur du trait dépend de l'unité de traduction

`utils.h:83`

```cpp
    if (is_copy_move_destructor(member) && !is_defaulted(member) &&
        !is_deleted(member))
      return false;
```

Pour `struct S { S(const S&); };` avec `S::S(const S&) = default;` défini hors classe,
`is_defaulted` répond `true` **quand la définition est visible** au point d'interrogation.
Dans une TU qui ne voit que la déclaration — le motif normal « déclaré dans l'en-tête, défini
`= default` dans le .cpp », utilisé pour ancrer les vtables ou stabiliser l'ABI — le type est
rejeté. Même type, deux TU, deux valeurs pour la même variable template constexpr : territoire
ODR/IFNDR.

La direction reste toujours conservatrice, donc pas de faille. **Correction précise trouvée par
le vérificateur** : GCC 16 fournit `std::meta::is_user_provided`, qui ne dépend que de la première
déclaration et vaut donc `true` pour la copie défaultée hors classe, `false` pour le `= default`
en classe — exactement la distinction du standard ([dcl.fct.def.default]).

```cpp
    if (is_copy_move_destructor(member) && is_user_provided(member))
      return false;
```

### m6 — Les closures à captures possédantes ne sont jamais lifetime-aware

`utils.h:32-37` (`has_unreflectable_state`) via `lifetime_aware.h:65`

```cpp
auto make_owning_closure() {
  return [owned = std::string("hello")] { return owned.size(); };
}
// is_lifetime_aware_v<decltype(...)> == false, launch_task refuse
```

La réflexion GCC n'expose pas les captures : un closure non vide a zéro membre réfléchi, donc
`has_unreflectable_state` le déclare non-walkable. Vérifié : la struct équivalente écrite à la
main est acceptée par les deux traits — la cause est P2996/GCC, pas la logique du walk.

Le rejet est du bon côté de la prudence (une capture par référence est tout aussi invisible),
mais il touche le cas d'usage numéro un du launcher, et le contournement — spécialiser
`is_unsafe_*` pour un type qu'on ne peut nommer que par `decltype` — est peu praticable.
À défaut de fix, le message du launcher devrait suggérer l'alternative : passer l'état en
arguments (sendable + lifetime-aware) avec un lambda sans capture.

### m7 — Le détach copie via une référence non-const

`copy_on_write.h:34`

```cpp
ptr_ = std::make_shared<T>(*ptr_);
```

`ptr_` est un `shared_ptr<T>` non-const dans une méthode non-const, donc `*ptr_` est un `T&`
parfaitement forwardé. Si `T` possède à la fois `T(T&)` et `T(const T&)` — idiome rare mais légal —
la résolution choisit `T(T&)`, qui a le droit d'**écrire** dans la source, c'est-à-dire dans le
bloc encore partagé, concurremment aux lectures const des autres threads. Démontré à l'exécution
avec le header réel : `as_mutable detach selected ctor: 1`.

Le contrat du vouching est alors trahi : l'utilisateur a promis que `const T` est lisible
concurremment, et la bibliothèque effectue une lecture non-const pendant le détach.
Mineur (double vouch utilisateur requis + type exotique), mais le durcissement est gratuit :

```cpp
ptr_ = std::make_shared<T>(std::as_const(*ptr_));
```

`<utility>` est déjà inclus, et `std::copy_constructible<T>` exige déjà
`constructible_from<T, const T&>` : aucun type actuellement admis n'est exclu.

### m8 — `lock_shared()` sur une rvalue const compile

`synchronized_value.h:80-81`

```cpp
[[nodiscard]] guard lock() && = delete;
[[nodiscard]] const_guard lock_shared() && = delete;
```

Ces surcharges supprimées ne sont pas const-qualifiées : une rvalue **const** ne peut pas s'y lier
et retombe sur `lock_shared() const &`, qui l'accepte.

```cpp
const sync_int make_const() { return sync_int{7}; }
auto guard = make_const().lock_shared();  // compile ; temporaire mort, guard pendouillant
```

Le destructeur du `shared_lock` fera `unlock()` sur un mutex détruit. Le garde-fou est incohérent :
la même expression sur une rvalue non-const est rejetée. **Correction d'un token** (validée :
`const&&` capture aussi les rvalues non-const, aucune régression sur les lvalues) :

```cpp
[[nodiscard]] const_guard lock_shared() const && = delete;
```

### m9 — Ni protection ni aide contre les deadlocks

`synchronized_value.h:75-81`. Deux scénarios classiques compilent sans un mot :

```cpp
auto outer = sv.lock();
auto inner = sv.lock();   // meme mutex, meme thread : UB (precondition de std::mutex::lock)
```

```cpp
auto ga = a.lock();
auto gb = b.lock();       // ordre croise possible depuis un autre thread
```

Pour `std::mutex` ce n'est même pas un deadlock garanti mais de l'UB
([thread.mutex.requirements.mutex.general]) ; pour `std::shared_mutex`, un deadlock garanti.
Aucune détection compile-time n'est possible — il faudrait un suivi de possession par thread.

Point structurel : l'utilisateur **ne peut pas** écrire lui-même un helper multi-verrou à la
`std::scoped_lock`, car le constructeur de `value_guard` est privé et le seul ami est
`synchronized_value` (vérifié : « `value_guard(...)` is private »). Un `lock_all` devrait donc
être fourni par la bibliothèque. A minima : une phrase notant que les traits éliminent les data
races mais pas les deadlocks — point de parité exacte avec Rust, précieux en conférence.

### m10 — Les surcharges rvalue de `copy_on_write` ne sont pas contraintes

`copy_on_write.h:27, 40`. `as_mutable() &` est proprement contrainte par
`std::copy_constructible<T>`, les deux surcharges rvalue ne le sont pas : une requires-expression
les déclare disponibles pour un `T` non copiable, et l'appel réel échoue en erreur dure **dans le
corps**.

```cpp
// vrai pour cow<std::unique_ptr<int>>, alors que la sonde lvalue est correctement fausse
requires (C c) { std::move(c).as_mutable(); }
requires (C c) { *std::move(c); }
```

Pour `as_mutable() &&`, le diagnostic est particulièrement trompeur : GCC résout l'appel interne
vers la surcharge `&&` elle-même (la `&` étant éliminée par sa contrainte) et affiche
« passing ... as 'this' argument discards qualifiers » — une récursion apparente qui n'explique
rien. Cela contredit la philosophie SFINAE-friendly affichée par la surcharge lvalue, dont le
test du dépôt vérifie qu'elle donne « a read-only handle, not a hard error ».

**Correction** : ajouter `requires std::copy_constructible<T>` aux deux surcharges rvalue
(intégré aux correctifs M1 et m11).

### m11 — `as_mutable() &&` sur un bloc partagé : allocation entièrement gaspillée

`copy_on_write.h:40`

```cpp
T as_mutable() && { return std::move(as_mutable()); }
```

Dans une fonction `&&`-qualifiée, `*this` est une lvalue : l'appel non qualifié résout vers la
surcharge `&`, qui sur un bloc partagé fait `ptr_ = std::make_shared<T>(*ptr_)` — une allocation
(objet + bloc de contrôle) et une copie — puis la surcharge rvalue **move ce `T` hors du bloc
fraîchement alloué**, que le handle moribond détruit aussitôt. L'allocation n'a servi que de halte
intermédiaire, sur ce qui est le chemin nominal d'un COW. Mesuré :

```
shared  std::move(cow).as_mutable(): copies=1 moves=1 heap allocs=1   (optimal : 1 copie, 0, 0)
```

**Correction** (validée : chemin partagé optimal, chemin unique inchangé, fence conservée,
12 tests OK ; répare en prime m10) :

```cpp
T as_mutable() &&
  requires std::copy_constructible<T>
{
  if (ptr_.use_count() != 1)
    return *ptr_;
  std::atomic_thread_fence(std::memory_order_acquire);
  return std::move(*ptr_);
}
```

Une fois M1 appliqué, la forme `return *std::move(*this);` est équivalente et encore meilleure
d'un move sur le chemin partagé.

### m12 — L'invocabilité n'est pas contrainte : l'erreur jaillit de `<thread>`

`asynchronous_task_launcher.h:41-46`

```cpp
template <class F, class... Args>
concept launchable_task = task_participant<F> && (task_participant<Args> && ...);
```

`grep -rn "invocable" include/ tests/` ne renvoie rien : la bibliothèque ne pose jamais la
question. L'erreur la plus banale d'un débutant traverse donc toute la couche de messages soignés :

```
thread:274:27: error: static assertion failed:
  std::jthread arguments must be invocable after conversion to rvalues
```

suivi de 49 lignes de notes libstdc++, aucune émise par la bibliothèque — alors que les erreurs
de traits sont, elles, formulées en 18 lignes lisibles. Incohérence de qualité de diagnostic au
sein de la même API.

**Correction** (le `static_assert` dans le fallback n'est **pas** optionnel : sans lui, l'appel
sélectionnerait le fallback dont tous les asserts passent et compilerait silencieusement **sans
lancer aucune tâche**, ce qui serait pire que l'état actuel) :

```cpp
template <class F, class... Args>
concept task_invocable = std::invocable<F, Args...>
                      || std::invocable<F, std::stop_token, Args...>;

template <class F, class... Args>
concept launchable_task = task_participant<F> && (task_participant<Args> && ...)
                       && task_invocable<F, Args...>;
// idem pour launchable_scoped_task

// et dans chaque fallback :
static_assert(task_invocable<F, Args...>,
              "the callable cannot be invoked with these arguments "
              "(a leading std::stop_token is allowed)");
```

La disjonction est nécessaire : `jthread` accepte aussi `F(stop_token, args...)`.

### m13 — Le fallback rend tout appel `launch_task` bien formé aux yeux de SFINAE

`asynchronous_task_launcher.h:60-68`

```cpp
template <typename F, typename... Args>
void launch_task(F, Args...) {
    static_assert(task_participant<F>, "...");
    static_assert((task_participant<Args> && ...), "...");
}
```

Les `static_assert` ne se déclenchent qu'à l'instanciation du corps, donc
`requires { launcher.launch_task(f_unsafe); }` vaut **vrai** pour un appel qui échoue en dur.
Tout code générique testant la validité d'un lancement est trompé et déclenchera une erreur dure
au lieu de brancher.

C'est un compromis délibéré (messages sélectifs contre honnêteté SFINAE) qui mérite d'être
assumé — excellent point de discussion. À noter : l'alternative C++26 `= delete("raison")` a été
testée et **ne résout pas** le problème sur GCC 16 (l'usage d'une fonction supprimée dans une
requires-expression y produit une erreur dure au lieu d'évaluer à `false`). La seule autre voie
serait de supprimer le fallback, au prix des messages pédagogiques. Même motif ligne 77-83.

### m14 — Le one-liner sûr est interdit, et le message du `delete` est inexact

`synchronized_value.h:23-28`

```cpp
T &operator*() && noexcept = delete (
    "a temporary guard is destroyed at the semicolon, so it cannot "
    "hand out a reference");
```

`queue.lock()->push_back(42);` est **sûr** — le guard temporaire vit jusqu'au point-virgule, donc
le verrou est tenu pendant le `push_back` — et pourtant rejeté. Le message est factuellement
inexact pour cet usage : la référence est valide jusqu'au point-virgule ; le vrai danger visé est
`auto &r = *queue.lock();`.

Conséquence quotidienne : chaque accès coûte deux lignes et une variable nommée, même pour
`*counter.lock() += 1`. Et le guard nommé est précisément le vecteur des fuites de M3 : **l'API
pousse vers la forme la plus risquée**.

**⚠ Correction ajustée.** L'`apply`/`with_lock` en fermeture compile, mais `decltype(auto)` laisse
sortir une référence — il faut le filtre, en sachant qu'il reste une heuristique (une struct
enveloppant un `T*` retournée par valeur passe au travers) :

```cpp
template <class F>
  requires std::invocable<F &, T &>
decltype(auto) with_locked(F f) & {
  auto locked_guard = lock();
  using result = std::invoke_result_t<F &, T &>;
  static_assert(!std::is_reference_v<result> && !std::is_pointer_v<result>,
                "the result must not carry a path to the value out of the "
                "critical section");
  return f(*locked_guard);
}
// + variante const via lock_shared()
```

Et reformuler le `delete` :
`"binding a reference to a temporary guard outlives the lock — name the guard, or use with_locked()"`.

C'est aussi la direction de `std::synchronized_value` (P0290, `apply()`).

### m15 — `threads_` croît sans borne, aucune API d'attente

`asynchronous_task_launcher.h:56-58, 86`. `threads_` n'est modifié que par `emplace_back` ; aucun
`join`, `clear` ni `erase` n'existe. Une tâche terminée reste un thread joignable dont le handle OS
n'est libéré qu'à la destruction du launcher, et l'utilisateur n'a aucun moyen d'attendre sans
détruire l'objet. La fin de vie est par ailleurs correcte (`~vector<jthread>` fait `request_stop`
puis `join`, pas de detach).

**⚠ Correction ajustée.** `threads_.clear()` compile et libère bien, mais `~jthread` fait
`request_stop()` **avant** `join()` : une tâche coopérative prenant un `stop_token` — cas
explicitement anticipé par le `static_assert` lignes 49-51 — serait **annulée**, pas attendue.
Sous le nom `wait_for_all_tasks`, c'est trompeur. Joindre d'abord :

```cpp
void wait_for_all_tasks() {
    for (auto &task : threads_)
        if (task.joinable())
            task.join();
    threads_.clear();
}
```

### m16 — Deux en-têtes ne sont pas auto-suffisants

Tous deux ne compilent que grâce à l'ordre d'inclusion de `threadsafe.h` — exactement le genre de
fragilité qu'on ne veut pas montrer en conférence.

*`smart_pointers.h:16`* appelle `wrapped_types_of` (défini dans `allowed_std_wrappers.h`) et
`std::ranges::all_of` sans inclure ni l'un ni l'autre :

```
smart_pointers.h:16:35: error: there are no arguments to 'wrapped_types_of' that depend on
                        a template parameter, so a declaration must be available
```

```cpp
#include <algorithm>                                       // pour std::ranges::all_of
#include <threadsafe/details/allowed_std_wrappers.h>       // pour wrapped_types_of
```

*`asynchronous_task_launcher.h:49`* contient `static_assert(task_participant<std::stop_token>, ...)`
— excellente idée pédagogique — mais les vouchers qui rendent `stop_token` sendable vivent dans
`vocabulary.h`, non inclus. Inclure ce header seul fait donc échouer son propre `static_assert`,
avec un message qui **accuse à tort `stop_token`** :

```cpp
#include <threadsafe/details/vocabulary.h>
```

### m17 — Aucun diagnostic ne dit quel trait a échoué ni quelle capture est fautive

`asynchronous_task_launcher.h:60-68`. Avec une capture réaliste de quatre variables dont une seule
est fautive (`counters` = `shared_ptr<int>`), la sortie complète tient en 18 lignes dont aucune ne
mentionne `counters`, ni même « sendable » plutôt que « lifetime-aware » : le message agrège les
trois exigences et la chaîne de notes s'arrête à « `is_task_participant_v<T>` evaluated to `false` ».
Le walk étant consteval, GCC n'affiche aucune trace de la récursion. Les fonctions s'appellent
`diagnose_is_sendable` mais ne produisent aucun diagnostic. Même problème pour le constructeur de
`synchronized_value`.

**⚠ Correction ajustée.** Scinder le `static_assert` par axe nomme bien le trait fautif, mais
**perd** la distinction callable/argument que l'original possédait (démontré). Il faut croiser les
deux dimensions, soit six assertions, soit une forme paramétrée :

```cpp
static_assert(std::move_constructible<F>, "the callable must be movable");
static_assert(sendable<F>,                "the callable must be sendable");
static_assert(lifetime_aware<F>,          "the callable must own its data");
static_assert((std::move_constructible<Args> && ...), "every argument must be movable");
static_assert((sendable<Args> && ...),                "every argument must be sendable");
static_assert((lifetime_aware<Args> && ...),          "every argument must own its data");
```

À terme, un `threadsafe::explain_not_sendable<T>()` consteval rejouant le walk en accumulant le
chemin fautif serait le vrai remède (voir M8).

### m18 — La copie supprimée de `synchronized_value` ne guide pas vers `make()`

`synchronized_value.h:65-66`. Le premier réflexe — passer le `synchronized_value` à une tâche —
échoue sur « use of deleted function », sans aucun indice vers le chemin prévu. La bibliothèque
utilise pourtant déjà `= delete("raison")` avec soin dans `value_guard`, **dans le même fichier** :

```cpp
synchronized_value(const synchronized_value &) = delete (
    "a synchronized_value is pinned to its mutex: share it across "
    "threads through synchronized_value::make()");
synchronized_value &operator=(const synchronized_value &) = delete (
    "a synchronized_value is pinned to its mutex: share it across "
    "threads through synchronized_value::make()");
```

D'autant plus utile que `make()` est la seule façon de partager l'objet (il n'est ni copiable ni
movable) et que rien dans l'API ne le signale.

### m19 — `concept synchronizable` manquant

`synchronizable_base.h:33`. `sendable.h:32` et `lifetime_aware.h:35` définissent leur concept ;
le troisième trait du triptyque n'en a pas — vérifié par grep sur tout le dépôt. `synchronized_value`
doit donc écrire `is_synchronizable_v<const T>` là où le reste du code parle en concepts, et GCC
suggère lui-même « did you mean 'is_synchronizable'? ». Au-delà de la cohérence, les concepts
donnent de meilleurs diagnostics de subsomption. Une ligne :

```cpp
template <class T>
concept synchronizable = is_synchronizable_v<T>;
```

### m20 — `pointee_is_lifetime_aware` applique la logique « pointee » au deleter

`smart_pointers.h:15`. `wrapped_types_of` retourne **tous** les arguments template : pour
`unique_ptr<T, D>`, `pointee_answer` est appliqué au deleter, qui subit alors le test
`is_dynamic_type_known` — sans justification pour un deleter stocké par valeur. L'asymétrie est
interne au fichier : le voucher sendable (lignes 64-67) distingue correctement pointee et deleter.

```cpp
template <class T, class D>
struct is_unsafe_lifetime_aware<std::unique_ptr<T, D>>
    : std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type) &&
                         is_lifetime_aware_v<D>> {};
```

Validé : répare le faux négatif (deleter polymorphe non-final accepté) en préservant tous les
comportements existants. Réserver `pointee_is_lifetime_aware` à `shared_ptr`/`weak_ptr`.

### m21 — Deux styles d'itération pour la même idée

`smart_pointers.h:15` réimplémente avec `std::ranges::all_of` + lambda ce que
`all_wrapped_types` (`allowed_std_wrappers.h:72-79`) fait déjà par boucle `for`, dans le fichier
voisin. Une lambda sans capture se convertissant en pointeur de fonction, la réutilisation est
directe :

```cpp
template <class T> consteval bool pointee_is_lifetime_aware() {
  return all_wrapped_types(^^T, [](std::meta::info argument) {
    return pointee_answer(argument, is_lifetime_aware_type);
  });
}
```

Trois lignes et l'usage isolé de `ranges::all_of` disparaissent (ce qui rend d'ailleurs
`<algorithm>` inutile — voir m16).

### m22 — Noms de fichiers trompeurs

`synchronizable.h` fait 15 lignes et ne contient que le voucher `std::atomic` ; le trait — le
walk const, la règle `mutable`, `diagnose_is_synchronizable` — vit dans `synchronizable_base.h`,
un suffixe « _base » qui suggère un détail d'implémentation alors que **c'est le fichier le plus
important de la bibliothèque**. Sept fichiers incluent l'un ou l'autre selon leur besoin, et le
lecteur doit deviner la convention.

Le split n'existe que pour casser le cycle `sendable` ↔ `synchronizable` (le voucher atomic a
besoin de `is_sendable_v`). **Correction validée** : renommer `synchronizable_base.h` →
`synchronizable.h` et déplacer le voucher atomic dans `vocabulary.h`, qui inclut déjà `sendable.h`
et héberge déjà les vouchers `std::allocator`/`std::stop_token`. La carte des fichiers devient
honnête : un fichier par trait, plus des fichiers de vouchers.

### m23 — Style incohérent

Trois écarts dans un code destiné à être projeté : `asynchronous_task_launcher.h` est indenté à
4 espaces (17 lignes) quand les onze autres en-têtes sont à 2 ; `template <typename` apparaît
9 fois (launcher, `smart_pointers.h`) contre `template <class` partout ailleurs, mélangés dans le
même fichier ; le launcher et `vocabulary.h` ferment le namespace par un `}` nu là où les autres
écrivent `} // namespace threadsafe`.

Sur des slides côte à côte, cela fait « plusieurs auteurs » au lieu de « un design ».
**⚠ Ajustement** : le dépôt ne contient aucun `.clang-format`, donc le style LLVM est une
inférence. Ajouter un `.clang-format` (`BasedOnStyle: LLVM` suffit ; `FixNamespaceComments` est
actif par défaut et corrigera les fermetures de namespace) fige la convention.

### m24 — Le coût d'inclusion domine (97 %), pas d'en-tête « traits seuls »

`include/threadsafe/threadsafe.h`. Mesuré (best-of-8, entrelacé) : TU vide 0,07 s ;
`#include <threadsafe/threadsafe.h>` seul 0,58 s ; les 12 tests 0,58–0,66 s. L'évaluation des
traits de **toute** la suite existante coûte au plus ~0,08 s par TU : le walk consteval est
négligeable devant les includes.

Décomposition : cœur des traits 0,36 s → +wrappers 0,40 → +smart_pointers/vocabulary 0,41 →
+`synchronized_value`/`copy_on_write` 0,58 (c'est `<mutex>` à 0,43 s et `<shared_mutex>` à 0,27 s).
Un utilisateur qui ne veut que les concepts dans ses propres en-têtes — le cas le plus fréquent
dans un gros projet — paie 0,17 s/TU pour de la machinerie mutex/thread qu'il n'utilise pas.

**Correction** (mesurée : 0,713 s → 0,502 s sur un TU utilisateur, soit −30 % ; les 12 tests
restent verts) :

```cpp
// include/threadsafe/traits.h — traits et vouchers, sans mutex/thread
#pragma once
#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
// threadsafe.h reste l'umbrella complet et inclut traits.h
```

⚠ La variante « cœur pur à 0,36 s » (sans les vouchers std) a été réfutée : elle change les
réponses des traits sur les types standard.

### m25 — Pas de precompiled header pour la suite de tests

`tests/CMakeLists.txt`. Les 12 TU re-parsent chacune les mêmes 0,58 s d'en-têtes. Comme les tests
sont compile-time only — « building the test target *is* running the tests » —, c'est directement
le temps du cycle rouge/vert.

**⚠ Correction conditionnelle.** `target_precompile_headers(threadsafe_tests PRIVATE <threadsafe/threadsafe.h>)`
fonctionne et divise par ~2,2 la recompilation d'un TU isolé, mais **dégrade de ~30 % les builds
parallèles** (`-j12` : 1,41 s → 2,04 s), la génération du PCH se sérialisant devant 12 TU
indépendantes. À réserver au build séquentiel, ou à documenter comme conseil aux utilisateurs
plutôt qu'à activer inconditionnellement. Le vrai levier reste `cmake --build build -j`.

### m26 — Hygiène d'includes

`lifetime_aware.h:3-4` inclut `<functional>` (0,21 s) et `<memory>` (0,25 s) sans utiliser aucun
symbole de ces en-têtes — à supprimer. Symétriquement, `smart_pointers.h` utilise
`std::ranges::all_of` sans inclure `<algorithm>` : retirer `<algorithm>` d'`allowed_std_wrappers.h`
casse toute la suite (« `all_of` is not a member of `std::ranges` »). Aucun coût mesurable
aujourd'hui — les en-têtes arrivent par d'autres chemins — mais tout découpage futur (m24)
trébuchera dessus. Voir m16 pour le correctif complet de `smart_pointers.h`.

---

## Info

### Constats positifs vérifiés

- **Mémoïsation intacte, scaling linéaire.** Un DAG en losange de profondeur 60 (deux membres du
  type précédent par niveau, soit 2^60 chemins si la mémoïsation était cassée) compile au temps de
  base. 1600 `static_assert` sur le même type : temps de base. Linéarité confirmée en nombre de
  types (100/400/1600 → 0,65/0,85/1,76 s), en profondeur et en largeur. Aucun blowup.
- **Robustesse de `is_sendable`.** Double indirection (`int**`, `Sync**`, `int*&`, `int* const*`)
  toutes rejetées ; membres piégés (`mutable int*`, union nommée ou anonyme contenant un pointeur,
  `int(&)[4]`, `int*[]`) rejetés ; capture par valeur d'un objet vide toujours rejetée ; formes de
  détournement de copie, dont un `operator=` à paramètre objet explicite (« deducing this »),
  correctement attrapées.
- **Le launcher tient son exigence.** Sur ~25 cas adversariaux, aucun type unsafe accepté par
  `launch_task` ni `launch_scoped_task` — y compris le launcher passé à sa propre tâche.
- **Le vouch de `synchronized_value` est justifié.** `is_unsafe_synchronizable<synchronized_value<T>>`
  conditionné à `is_sendable_v<T>` reproduit exactement `impl<T: Send> Sync for Mutex<T>`, et
  coïncide avec le `static_assert(sendable<T>)` du constructeur : un `synchronized_value<T>`
  constructible est toujours vouché, et réciproquement.
- **La logique `use_count` + fence de `copy_on_write` est correcte.** L'idiome dont la fragilité a
  fait retirer `shared_ptr::unique()` est ici prouvablement sûr, mais grâce à trois faits externes
  à la fonction : le TOCTOU deux-threads-même-objet est exclu à la compilation
  (`!is_synchronizable_v<cow<T>>`), lire un `1` périmé est impossible, et la fence acquire
  s'apparie au décrément release du dernier autre propriétaire ([atomics.fences]/4).
  ⚠ La preuve dépend de garanties **libstdc++** que le standard ne donne pas pour `use_count()` —
  à consigner, puisque la chaîne GCC 16 est imposée par le projet.
- **Le point de customisation fonctionne pour un tiers.** Claim inconditionnel, claim conditionnel
  `bool_constant` sur un template, propagation automatique vers `const T`, effet immédiat sur les
  traits dérivés : tout vérifié. Le piège « spécialisation avant première question » est **attrapé
  par GCC dans la même TU** (« specialization of `is_unsafe_synchronizable<Late>` after
  instantiation ») ; seul le cas inter-TU reste une violation ODR non diagnosticable, inhérente au
  mécanisme C++.
- **Le rejet des membres `void*` est conforme** : erreur dure avec une chaîne d'instanciation
  lisible menant du `static_assert` utilisateur jusqu'au membre fautif. Comportement voulu.

### Limites assumées, sans remède

- **Pointeur déguisé et emprunt statique.** Un pointeur stocké dans un `std::uintptr_t` (scalaire,
  donc accepté) et un emprunt rangé dans un membre statique (le walk ne visite que l'état par
  objet) passent tous deux. Rust a exactement les mêmes angles morts. À dire sur scène : l'analyse
  prouve l'absence d'emprunt **déclaré dans le système de types**, pas l'absence d'emprunt.
- **Union avec membre `mutable` superposé.** `union U { mutable std::atomic<int> a; int b; };`
  est accepté en `const U`. La branche `mutable` raisonne « le membre gère sa concurrence », ce qui
  est fragile sous superposition — mais exploiter la faille exige de lire un membre inactif, déjà
  UB en mono-thread. Durcissement gratuit si souhaité :
  `if (is_mutable_member(member)) { if (is_union_type(type) || !is_synchronizable_type(member_type)) return false; }`.
- **`is_unsafe_sendable<const T>` est silencieusement ignoré.** `diagnose_is_sendable` retire les
  cv-qualifieurs avant de consulter la couche unsafe. Cohérent avec le modèle (le claim se met sur
  `T` nu), mais asymétrique avec `is_unsafe_synchronizable`, dont les claims `const` sont un idiome
  central. Un utilisateur qui calque l'idiome obtient un claim inerte, sans avertissement.
- **`shared_ptr<void>`** déclenche le poisoning voulu, mais avec un message inapplicable
  (« complete it before asking » — on ne complète pas `void`) sur un idiome réaliste. Contournement
  vérifié : `template <> struct threadsafe::is_unsafe_lifetime_aware<std::shared_ptr<void>> : std::true_type {};`.
- **`is_sendable_v<void>` déclenche deux `static_assert`** : le message parfaitement adapté est
  suivi d'un second hors sujet, l'évaluation consteval ne s'interrompant pas. Correction validée :
  `static_assert(std::is_void_v<T> || is_complete_type(^^std::remove_all_extents_t<T>), "...")`.
- **Surcoût constant de ~0,7 ms par type et par trait.** Un walk réflexif minimal équivalent coûte
  0,09 ms ; routé par `extract<bool>(substitute(...))` — l'indirection qui fournit mémoïsation et
  accès aux spécialisations utilisateur — 0,25 ms. Le reste (~0,45 ms) vient de l'empilement
  d'instanciations par requête (`is_X<T>` + `is_X_v<T>` + `assert_queryable_type<T>` +
  `is_unsafe_X<T>`). Court-circuiter la sous-requête `synchronizable` de `is_sendable` changerait
  la sémantique : à ne pas faire.
- **Un move supplémentaire par argument dans le launcher.** Les paramètres sink par valeur donnent
  1 copie + 2 moves contre 1 copie + 1 move pour un `std::jthread` direct. Quasi gratuit pour les
  types que le launcher accepte, et le passage par valeur a une vertu pédagogique.
- **Pas de canal de résultat.** La valeur de retour de la tâche est ignorée et une exception
  échappée appelle `std::terminate` (sémantique `std::thread`). Neutre côté sûreté — vérifié : un
  type de retour non-sendable est accepté, et c'est correct, la valeur étant construite, utilisée
  et détruite entièrement sur le thread travailleur.
- **Pas de combinateur « une opération sous un seul verrou ».** La suppression des `operator*`/`->`
  sur guard temporaire bloque élégamment le pire one-liner, mais la version en deux instructions —
  `if (sv.lock_shared()->empty()) sv.lock()->push_back(x);` — compile avec son check-then-act.
  Ce n'est pas de l'UB (chaque accès est verrouillé), mais c'est la race logique que l'API rend la
  plus facile à écrire. Un `with_lock` (m14) rendrait le chemin correct aussi court que le chemin
  piégé.
- **Ergonomie conservatrice à anticiper en démonstration live.** `launch_task(f, "hello")` échoue
  (`const char*` est un pointeur, jamais lifetime-aware) ; `std::string_view` échoue aussi
  (borrowed) ; seul `std::string` passe. Et toute lambda à capture, même `[x = 42]`, est refusée.
- **Préfixe `diagnose_` dilué.** Dans les traits, il désigne le walk réflexif — vocabulaire central
  de l'architecture. Dans le launcher, `diagnose_task_participant` est une simple conjonction de
  concepts, sans réflexion ni diagnostic. Simplification proposée :

```cpp
template <class T>
constexpr bool is_scoped_task_participant_v =
    std::move_constructible<T> && is_sendable_v<T>;

template <class T>
constexpr bool is_task_participant_v =
    is_scoped_task_participant_v<T> && is_lifetime_aware_v<T>;
```

  (supprime le namespace `detail` local et les deux fonctions `diagnose_`).
- **Cast `bool()` redondant** — `synchronized_value.h:45` : `is_synchronizable_v<const T>` est déjà
  un `constexpr bool`. Le cast fait douter le lecteur d'une conversion subtile qui n'existe pas.
- **Deux noms d'utilitaires cachent leur condition la plus importante.** `pointee_answer(pointee, question)`
  répond en réalité « `question(pointee)` **et** le type dynamique du pointee est connu » — or
  cette seconde condition est la subtilité de sûreté clé du modèle (un pointeur vers une base
  polymorphe non-final peut désigner un dérivé aux membres inconnus), exactement ce qu'on voudra
  expliquer sur scène. Et `is_copy_move_destructor` se lit « destructeur de copie-déplacement »
  alors qu'il teste « copy/move ctor, copy/move assign **ou** destructeur ».
