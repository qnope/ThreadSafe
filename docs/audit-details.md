# Audit ThreadSafe — Détails

_Audit multi-agents du 2026-09-06. Chaque finding ci-dessous a été vérifié de façon adversariale (compilation g++-16 quand applicable). Les findings réfutés lors de la vérification ne figurent pas ici._


## Trait `is_synchronizable`


### 1. 🔴 Critique — Unsoundness: un pointeur de fonction NON-const est déclaré synchronizable

**Fichier** : `include/threadsafe/details/synchronizable_base.h`


Dans diagnose_is_synchronizable, le test `if (is_function_type(remove_pointer(type))) return true;` (lignes 47-48) est placé AVANT le garde `if (!is_const(type)) return false;`. Il matche donc `void(*)()` mutable, pas seulement `void(*const)()`. Vérifié par compilation avec g++-16 : `static_assert(is_synchronizable_v<void(*)()>)` et `static_assert(is_sendable_v<void(*&)()>)` passent tous les deux. Conséquence concrète : la bibliothèque autorise à envoyer à un autre thread une référence (ou un `void(**)()`) vers un pointeur de fonction mutable ; un thread peut alors réassigner le pointeur pendant qu'un autre le lit — data race. Le commentaire du test existant (« a function pointee is code, and code is immutable ») ne s'applique qu'au pointé, pas au pointeur lui-même, qui est un scalaire mutable comme n'importe quel autre. Notez que la branche scalaire (ligne 60) refuse correctement `int` non-const et `int U::*` non-const (vérifié) — seuls les pointeurs de fonction bénéficient à tort de ce raccourci.


**Correction proposée** :

```cpp
Ne court-circuiter que les vrais types fonction, et laisser les pointeurs de fonction passer par le garde const :

  if (is_function_type(type))
    return true;

  ... // arrays, garde !is_const

  if (is_pointer_type(type) || is_lvalue_reference_type(type)) {
    const auto pointee = remove_cv(remove_pointer(type));
    if (is_function_type(pointee))
      return true; // const fn-pointer: le code est immuable
    return pointee_answer(pointee, is_synchronizable_type);
  }

Ajouter les régressions : static_assert(!is_synchronizable_v<void(*)()>); static_assert(is_synchronizable_v<void(*const)()>); static_assert(!is_sendable_v<void(*&)()>);
```


> _Vérification_ : Confirmed in include/threadsafe/details/synchronizable_base.h: the function-pointer shortcut (lines 47-48) precedes the `!is_const(type)` guard (line 53), so mutable `void(*)()` is deemed synchronizable. Reproduced with g++-16: static_assert(is_synchronizable_v<void(*)()>) and static_assert(is_sendable_v<void(*&)()>) both compile, while non-const int is correctly rejected. This lets a thread reassign a shared mutable function pointer while another reads it — a data race; the immutability argument applies to the pointee code, not the pointer object.


### 2. 🟡 Mineur — Faux négatif / asymétrie : une référence top-level n'est jamais synchronizable

**Fichier** : `include/threadsafe/details/synchronizable_base.h`


`is_synchronizable_v<std::atomic<int>&>` vaut false (vérifié) car `is_const(T&)` est false et le walk retourne false ligne 53-54 avant d'atteindre la branche lvalue-reference de la ligne 56 (qui n'est donc atteignable que pour les pointeurs, jamais pour les références — code mort partiel). Pourtant le même référent est accepté comme MEMBRE : `is_synchronizable_v<const RefsAtomic>` (membre `std::atomic<int>&`) est true via la branche membre-référence (lignes 76-78), et `is_sendable_v<std::atomic<int>&>` est true aussi. L'asymétrie est peut-être voulue (sendable traite les références lui-même), mais alors `is_lvalue_reference_type` dans la condition ligne 56 est inatteignable et mérite d'être retiré ou documenté ; sinon, traiter les références avant le garde const comme le fait sendable.


**Correction proposée** :

```cpp
Soit retirer `|| is_lvalue_reference_type(type)` (branche inatteignable), soit remonter le cas référence avant `if (!is_const(type))` :

  if (is_lvalue_reference_type(type))
    return pointee_answer(remove_cv(remove_reference(type)), is_synchronizable_type);
```


> _Vérification_ : Confirmé par compilation (g++-16, -freflection). Dans /Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/synchronizable_base.h : `diagnose_is_synchronizable` retourne false ligne 53-54 (`if (!is_const(type)) return false;`) pour toute référence, car une référence n'est jamais const-qualifiée au top-level — même `const std::atomic<int>&` donne false (vérifié). La branche `is_lvalue_reference_type(type)` ligne 56 est donc du code mort : elle n'est atteignable que pour les pointeurs const (`std::atomic<int>* const` est bien true, vérifié). L'asymétrie est réelle : `is_synchronizable_v<std::atomic<int>&>` == false, alors que le même référent en MEMBRE (`struct RefsAtomic { std::atomic<int>& r; };` avec `is_synchronizable_v<const RefsAtomic>`) == true via la branche membre-référence lignes 76-78, et `is_sendable_v<std::atomic<int>&>` == true car sendable.h:50-51 traite la référence avant tout garde const (`return is_synchronizable_type(remove_reference(...))`). Faux négatif conservateur (pas un trou de sûreté), mais le test de référence ligne 56 est inatteignable et l'asymétrie top-level vs membre est incohérente avec le modèle Rust (&T Sync ssi T Sync).


### 3. ℹ️ Info — Couverture de tests : cas absents de test_synchronizable.cpp

**Fichier** : `tests/test_synchronizable.cpp`


Les cas suivants se comportent correctement (vérifiés par compilation) mais ne sont couverts par aucun static_assert du fichier : (1) unions — `const union {int; int*;}` refusée, `const union {int; double;}` acceptée ; (2) bitfields — `const B{int a:3;}` accepté, un bitfield `mutable` refusé ; (3) enums — `const E` accepté via la branche scalaire ; (4) pointeurs-à-membre — `int U::* const` accepté, non-const refusé ; (5) types polymorphes — `const V{virtual...; int;}` accepté (le vptr est immuable après construction, c'est sain, mais aucun test ne fige ce comportement, alors que test_polymorphic.cpp ne teste que sendable/pointées) ; (6) classes avec bases (la boucle bases_of lignes 66-68 n'a aucun test dédié ici) ; (7) le pointeur de fonction non-const top-level, qui aurait révélé la faille critique ci-dessus ; (8) références rvalue top-level. Ajouter ces static_assert protégerait le walk contre les régressions, en particulier la boucle sur les bases et le traitement des unions.


**Correction proposée** :

```cpp
Ajouter au fichier des static_assert pour : unions (sûre/non sûre), bitfield mutable, enum const, pointeur-à-membre const/non-const, type polymorphe const, un Derived const avec base contenant un membre mutable non-atomique (doit être refusé), et les régressions fonction-pointeur du finding critique.
```


## Trait `is_sendable`


### 4. 🔴 Critique — const derriere une indirection est fait confiance par is_sendable (const int* / const int& sendables)

**Fichier** : `include/threadsafe/details/sendable.h`


La branche lvalue-reference de diagnose_is_sendable ne retire pas le cv du referent: `if (is_lvalue_reference_type(type)) return is_synchronizable_type(remove_reference(remove_pointer(type)));` et la branche pointeur y renvoie via `add_lvalue_reference(remove_pointer(type))`. Pour `const int*` cela demande is_synchronizable<const int>, qui est vrai (scalaire const). Verifie au compilateur: `is_sendable_v<const int*>` et `is_sendable_v<const int&>` valent true. Or un autre alias `int*` sur le meme objet peut le muter pendant que le thread receveur lit — c'est exactement ce que CLAUDE.md interdit (« Const behind an indirection is never trusted ») et ce que la couche smart-pointer fait correctement: pointee_is_synchronizable dans smart_pointers.h fait remove_cv, d'ou `!is_sendable_v<std::shared_ptr<const int>>` (teste) mais `is_sendable_v<const int*>` == true. Incoherence doctrinale et trou de soundness.


**Correction proposée** :

```cpp
Dans diagnose_is_sendable, aligner sur la regle des smart pointers: `return is_synchronizable_type(add_const(remove_cv(remove_reference(remove_pointer(type)))));` — c'est-a-dire demander la synchronizabilite pleine du type non-cv (comme pointee_is_synchronizable), pas celle du type const. Concretement: `return is_synchronizable_type(remove_cv(remove_reference(remove_pointer(type))));` puis ajouter les tests `static_assert(!is_sendable_v<const int*>)` et `static_assert(!is_sendable_v<const int&>)`.
```


> _Vérification_ : Confirmed at the compiler (g++-16, -freflection): all four asserts pass — is_sendable_v<const int*> == true, is_sendable_v<const int&> == true, while is_sendable_v<std::shared_ptr<const int>> == false and is_sendable_v<int*> == false. Mechanism verified in include/threadsafe/details/sendable.h lines 50-54: the lvalue-reference branch calls is_synchronizable_type(remove_reference(remove_pointer(type))) without remove_cv, so const int& asks is_synchronizable<const int>; in synchronizable_base.h that hits the is_const -> is_scalar_type path (lines 53, 60-61) and returns true. By contrast, the synchronizable pointer branch (synchronizable_base.h line 57) and smart_pointers.h both do remove_cv on the pointee before asking, which is why shared_ptr<const int> is correctly rejected. So a raw const int*/const int& is trusted where the same const-pointee behind a smart pointer is not — a genuine incoherence with CLAUDE.md's "Const behind an indirection is never trusted", and a soundness hole: another thread holding a plain int* alias can mutate while the receiving thread reads through the sent const alias.


### 5. 🔴 Critique — Les rvalue references sont traitees comme des valeurs: is_sendable_v<int&&> == true

**Fichier** : `include/threadsafe/details/sendable.h`


Apres la branche lvalue-reference, `type = remove_reference(type);` fait tomber T&& dans le walk par valeur. Verifie: `is_sendable_v<int&&>`, `is_sendable_v<StatefulCallable&&>` et un struct membre `struct RvRef { int&& r; }` sont tous sendables. Mais en C++ `std::move` ne transfere rien: `int x; envoyer(std::move(x));` — l'emetteur garde x et peut le muter pendant que le receveur ecrit a travers la reference. Une T&& partage son referent exactement comme une T& (le message du test SyncType&& le dit lui-meme: « an rvalue reference shares the referent too », mais ce cas ne passe que grace au claim synchronizable). Le walk est cense etre conservateur; ici il fait confiance a une exclusivite (&mut a la Rust) qu'aucun borrow checker ne verifie. La derniere ligne de test_sendable.cpp, `static_assert(is_sendable_v<int&&>);` (sans message), cimente le trou; le cas membre `int&&` n'est teste nulle part.


**Correction proposée** :

```cpp
Traiter T&& comme T&: dans diagnose_is_sendable remplacer la branche lvalue par `if (is_reference_type(type)) return is_synchronizable_type(remove_cv(remove_reference(type)));` (le check is_dynamic_type_known existant reste devant). Inverser le test en `static_assert(!is_sendable_v<int&&>, ...)` et ajouter un test sur un membre rvalue-ref.
```


> _Vérification_ : Confirmed in sendable.h: only is_lvalue_reference_type gets the synchronizable check (lines 50-51); line 56 `type = remove_reference(type)` drops T&& into the by-value walk, so int&& → scalar → true. test_sendable.cpp line 131 asserts !is_sendable_v<int&> while line 267 asserts is_sendable_v<int&&> — same referent, opposite answers. A C++ rvalue reference aliases its referent like an lvalue reference (std::move transfers nothing), so this trusts an exclusivity nothing verifies, violating the walk's conservative rule; member int&& case is likewise accepted and untested.


### 6. 🟠 Majeur — is_synchronizable_v<T> implique is_sendable_v<T> — divergence avec Rust (Sync n'implique pas Send)

**Fichier** : `include/threadsafe/details/sendable.h`


`if (is_scalar_type(type) || is_synchronizable_type(type)) return true;` fait de tout type synchronizable un type sendable, et test_sendable.cpp l'assume (« is_synchronizable_v<T> implies is_sendable_v<T> »). En Rust, Sync n'implique pas Send: un type peut etre sur en lecture partagee mais avoir un destructeur ou un etat lie a son thread d'origine (MutexGuard, types a TLS). Consequence pratique: un utilisateur qui ecrit seulement `is_unsafe_synchronizable<X> : true_type` accorde silencieusement is_sendable<X> aussi — le receveur detruira X sur son thread. Meme logique dans smart_pointers.h ou `is_unsafe_sendable<std::shared_ptr<T>>` ne demande que pointee_is_synchronizable (Rust exige T: Send + Sync pour Arc<T>: Send, car le dernier handle detruit T sur un autre thread). C'est coherent en interne uniquement parce que Sync=>Send est cable; si c'est un choix assume, il doit etre documente comme faisant partie du contrat unsafe (« vouch Sync = vouch Send »); sinon, exiger `is_sendable && is_synchronizable` du pointee pour shared_ptr/weak_ptr et retirer l'implication.


**Correction proposée** :

```cpp
Au minimum documenter dans CLAUDE.md/synchronizable_base.h que vouch synchronizable implique sendable (destruction incluse). Sinon: retirer `|| is_synchronizable_type(type)` du walk par valeur et changer shared_ptr/weak_ptr en `pointee sendable && pointee synchronizable`.
```


> _Vérification_ : Confirmed: sendable.h:61 hard-wires synchronizable => sendable, so is_unsafe_synchronizable<X> vouching silently grants is_sendable<X>; smart_pointers.h:69-75 makes shared_ptr/weak_ptr Send on pointee Sync alone (Rust Arc requires Send+Sync since the last handle destroys T on another thread). A Sync-but-not-Send type (thread-affine destructor, TLS) is expressible only via the unsafe layer, and that layer's contract nowhere documents 'vouch Sync = vouch Send' — only test messages ('Sync implies Send in this library') acknowledge it. Deliberate choice, but undocumented in the contract and diverging from the Rust model the library claims to follow.


### 7. 🟡 Mineur — shared_ptr sendable ignore le deleter et l'allocateur efface par type

**Fichier** : `include/threadsafe/details/smart_pointers.h`


`is_unsafe_sendable<std::shared_ptr<T>> : bool_constant<detail::pointee_is_synchronizable<T>()>` ne peut pas voir le deleter/allocateur efface au moment de la construction (`std::shared_ptr<SyncType>(p, ThreadAffineDeleter{})`): ce deleter voyage dans le control block et s'execute sur le thread qui lache la derniere reference. Contrairement a unique_ptr (dont D est verifie: `is_sendable_v<D>`), ce trou est invisible au systeme de types — inevitable a la compilation, mais il merite d'etre documente comme hypothese du vouch (et test_smart_pointers.cpp ne mentionne pas cette limite).


**Correction proposée** :

```cpp
Ajouter un commentaire sur la specialisation shared_ptr/weak_ptr et une note dans CLAUDE.md: le vouch suppose un deleter/allocateur sendable (le defaut l'est).
```


> _Vérification_ : Confirmed. smart_pointers.h:69-71 vouches is_unsafe_sendable<std::shared_ptr<T>> from pointee_is_synchronizable<T>() alone; the type-erased deleter/allocator (e.g. shared_ptr<SyncType>(p, ThreadAffineDeleter{})) is invisible in the type and runs on the last-releasing thread. unique_ptr does check its deleter (is_sendable_v<D>, lines 64-67 and 81-84), and test_smart_pointers.cpp asserts that rule for unique_ptr but never mentions the erased shared_ptr deleter limit. Inherent to shared_ptr's design (not fixable at compile time), but it is a real undocumented assumption of the vouch and a documentation/test gap, exactly as the finding states.


### 8. ℹ️ Info — is_sendable<T&> != is_synchronizable<T> quand T est polymorphe non-final (doc vs code)

**Fichier** : `include/threadsafe/details/sendable.h`


CLAUDE.md documente `is_sendable<T&> = is_sendable<T*> = is_synchronizable<T>`, mais le walk insere `if (is_reference_type(type) && !is_dynamic_type_known(remove_reference(type))) return false;` avant. Une reference a un type polymorphe non-final vouched synchronizable donne donc is_sendable<T&> == false alors que is_synchronizable<T> == true. Plus strict, donc sain, mais l'egalite documentee est fausse; par ailleurs le `remove_pointer` dans la branche lvalue (`remove_reference(remove_pointer(type))`) est un no-op sur un type reference — code mort qui brouille la lecture.


**Correction proposée** :

```cpp
Preciser dans CLAUDE.md « ... et le type dynamique du referent doit etre connu (non polymorphe ou final) », et supprimer le remove_pointer inutile.
```


## Trait `is_lifetime_aware`


### 9. 🟠 Majeur — weak_ptr vouché lifetime_aware alors qu'il ne garde pas son référent en vie

**Fichier** : `include/threadsafe/details/smart_pointers.h`


is_smart_pointer inclut std::weak_ptr, donc `template <smart_pointer T> struct is_unsafe_lifetime_aware<T>` (lignes 51-53) répond vrai pour weak_ptr<T>. Le contrat de la trait est « owns its data or keeps its referent alive » ; un weak_ptr ne garde que le control block en vie, l'objet peut mourir. C'est sûr en pratique uniquement parce que l'accès passe par lock() qui peut échouer — mais rien dans la trait n'encode cette nuance. Le test (test_lifetime_aware.cpp:56, « keeps its control block alive ») documente le choix, mais il contredit la doc de la trait dans CLAUDE.md. À trancher : soit documenter explicitement que weak_ptr compte comme lifetime_aware car son observation est checked, soit le retirer de la claim.


**Correction proposée** :

```cpp
Soit exclure weak_ptr de la claim lifetime_aware, soit documenter dans lifetime_aware.h que l'observation via lock() rend le weak_ptr acceptable.
```


> _Vérification_ : Confirmed: smart_pointers.h lines 36 and 51-53 do vouch std::weak_ptr as is_unsafe_lifetime_aware, while the trait contract ("owns its data or keeps its referent alive") is literally violated — weak_ptr keeps only the control block alive. It is safe in practice (access requires lock()) and deliberately asserted in test_lifetime_aware.cpp:56, so this is not a soundness bug but a genuine contract/documentation inconsistency: the checked-observation rationale exists only in a test message, not in the trait's documentation. Resolution is exactly as the finding says — either document that checked observation counts as lifetime awareness, or drop the weak_ptr claim.


### 10. 🟡 Mineur — Incohérence référence de fonction vs pointeur de fonction

**Fichier** : `include/threadsafe/details/lifetime_aware.h`


Ligne 50 : `if (is_function_type(remove_pointer(type))) return true;` accepte `void(*)()` et `void()`, mais `void(&)()` tombe sur `is_reference_type` (ligne 53) et répond faux. Le code d'une fonction vit pour toujours dans les deux cas. Vérifié : static_assert(!is_lifetime_aware_v<void(&)()>) passe. Faux négatif inoffensif mais incohérent pour une bibliothèque pédagogique.


**Correction proposée** :

```cpp
if (is_function_type(remove_reference(remove_pointer(type))))
  return true;  // avant le test is_reference_type
```


> _Vérification_ : Confirmé par lecture et compilation. Dans include/threadsafe/details/lifetime_aware.h, la ligne 50 `if (is_function_type(remove_pointer(type))) return true;` accepte `void()` (type fonction nu) et `void(*)()` (remove_pointer expose le type fonction), mais `void(&)()` est un type référence : remove_pointer le laisse inchangé, is_function_type est faux, et la ligne 53 `if (is_reference_type(type) || is_pointer_type(type)) return false;` répond faux. Test compilé avec g++-16 -std=c++26 -freflection : static_assert(is_lifetime_aware_v<void()>), static_assert(is_lifetime_aware_v<void(*)()>) et static_assert(!is_lifetime_aware_v<void(&)()>) passent tous. Le code d'une fonction vit pour toute la durée du programme dans les trois cas, donc traiter la référence de fonction différemment du pointeur est bien une incohérence — un faux négatif conservateur (inoffensif pour la sûreté) mais incohérent, ce qui compte pour une bibliothèque pédagogique. Fix possible : tester is_function_type(remove_pointer(remove_reference(type))) avant la branche référence.


### 11. 🟡 Mineur — Les lambdas à init-capture sont invisibles à la réflexion : tout rejeté, y compris les captures par valeur

**Fichier** : `include/threadsafe/details/utils.h`


Vérifié : pour `auto val_lambda = [x = 1](){};`, GCC 16 n'expose pas les membres du closure — has_unreflectable_state(^^L) est vrai, donc is_walkable_type est faux et is_lifetime_aware_v<decltype(val_lambda)> est faux. Conséquences : (1) faux négatif — une lambda qui possède ses captures par valeur ne peut pas être passée au launcher qui exige lifetime_aware (asynchronous_task_launcher.h:23) ; (2) le rejet des lambdas capturant par référence fonctionne par accident (état non réflexif) et non par le walk sur le membre référence — si GCC expose un jour les captures, le comportement change silencieusement. Ajouter des tests fige le contrat.


> _Vérification_ : Confirmé par compilation avec g++-16 contre la bibliothèque réelle. Pour `auto val_lambda = [x = 1](){};` : (1) std::meta::nonstatic_data_members_of(^^L, unchecked) est vide — GCC 16 n'expose pas les captures du closure ; (2) threadsafe::detail::has_unreflectable_state(^^L) (utils.h:32-37 : non-empty, non-polymorphe, sans bases ni membres réflexifs) est donc vrai, is_walkable_type faux ; (3) is_lifetime_aware_v<L> et is_sendable_v<L> sont tous deux faux. Comme is_task_participant_v exige is_lifetime_aware_v (asynchronous_task_launcher.h:23), une lambda à capture par valeur — pourtant propriétaire de son état — est rejetée par le launcher : faux négatif réel. Le second point tient aussi : le rejet des captures par référence passe par has_unreflectable_state, pas par le walk sur un membre référence ; si GCC exposait un jour les captures, le comportement changerait silencieusement (dans un sens qui reste conservateur pour les valeurs, mais le contrat n'est figé par aucun test — grep sur tests/ ne montre pas de test de lambda à init-capture).


### 12. ℹ️ Info — Aucun type empruntant accepté à tort — le walk est robuste

**Fichier** : `include/threadsafe/details/lifetime_aware.h`


Vérifié par compilation (g++-16) : string_view, span<int> et span<int,4>, subrange, ref_view, itérateur de vector, tuple<int&>, optional<string_view>, pair<int,int*>, reference_wrapper, std::function, lambda capturant par référence, unique_ptr<int, deleter-lambda capturant par référence>, shared_ptr<Poly> (polymorphe non-final) sont tous refusés. Les couches de défense (borrowed_range à la ligne 59, rejet des pointeurs/références lignes 53-54, walk transitif via all_bases_and_members, pointee_answer + is_dynamic_type_known pour les smart pointers, all_wrapped_types pour les std_wrappers) se recouvrent correctement.


### 13. ℹ️ Info — pointee_is_lifetime_aware applique is_dynamic_type_known au deleter stocké par valeur

**Fichier** : `include/threadsafe/details/smart_pointers.h`


Lignes 15-21 : pour unique_ptr<T,D>, wrapped_types_of retourne {T, D} et chaque argument passe par pointee_answer, qui exige is_dynamic_type_known. Le deleter D est stocké par valeur dans unique_ptr : son type dynamique est statique, la question polymorphe ne s'applique qu'au pointé. unique_ptr<int, DeleterPolymorpheNonFinal> est donc rejeté à tort. Faux négatif conservateur, sans danger.


### 14. ℹ️ Info — Le constructeur aliasing de shared_ptr est un trou indétectable à la compilation

**Fichier** : `include/threadsafe/details/smart_pointers.h`


`std::shared_ptr<int>(std::shared_ptr<X>{}, &local_int)` produit un shared_ptr<int> qui n'entretient pas local_int : la claim `is_unsafe_lifetime_aware<smart_pointer T>` répond vrai alors que l'instance emprunte. C'est une propriété de valeur, invisible au niveau des types — aucune trait compile-time ne peut le voir. Vaut une mention dans la doc/talk : la trait décrit le contrat du type, pas chaque instance.


## Helpers (`synchronized_value`, `copy_on_write`, launcher)


### 15. 🟠 Majeur — synchronized_value::lock() utilisable sur un temporaire — garde pendante

**Fichier** : `include/threadsafe/details/synchronized_value.h`


`[[nodiscard]] guard lock() { return guard{mutex_, value_}; }` et `lock_shared() const` ne sont pas ref-qualifiés. `value_guard` supprime bien `operator*() &&` sur une garde temporaire, mais rien n'empêche `auto g = threadsafe::synchronized_value<int>{1}.lock();` : le temporaire est détruit à la fin de l'expression, `g` conserve un `std::unique_lock` sur un mutex détruit et un `T*` pendouillant — UB à la première utilisation et au unlock du destructeur. La même précaution appliquée à la garde (delete sur rvalue) manque au wrapper lui-même.


**Correction proposée** :

```cpp
[[nodiscard]] guard lock() & { return guard{mutex_, value_}; }
[[nodiscard]] const_guard lock_shared() const & { return const_guard{mutex_, value_}; }
// éventuellement: guard lock() && = delete("..."); const_guard lock_shared() const && = delete("...");
```


> _Vérification_ : Confirmed: in include/threadsafe/details/synchronized_value.h lines 75-78, `lock()` and `lock_shared()` are not ref-qualified, so `auto g = synchronized_value<int>{1}.lock();` compiles (guaranteed copy elision bypasses the deleted guard copy ctor). The temporary wrapper dies at the semicolon while the guard keeps a std::unique_lock on the destroyed mutex_ and a dangling T* — UB on use and on the destructor's unlock. value_guard deletes its own rvalue operator*/-> but the wrapper lacks the matching `&`-qualification; fix is `guard lock() &` / deleting `&&` overloads.


### 16. 🟠 Majeur — copy_on_write : état moved-from déréférençable (use-after-move → nullptr)

**Fichier** : `include/threadsafe/details/copy_on_write.h`


Le move constructor/assignment implicitement générés déplacent le `std::shared_ptr<T> ptr_`, le laissant nul. Ensuite `const T& operator*() const noexcept { return *ptr_; }` et `operator->` déréférencent nullptr (UB, et `noexcept` masque tout), et `as_mutable()` sur un objet moved-from voit `use_count() == 0 != 1` et exécute `std::make_shared<T>(*ptr_)` — déréférencement nul également. Aucun test ne couvre le move. Pour un type dont tout l'intérêt est un invariant « ptr_ jamais nul », le move par défaut casse l'invariant silencieusement.


**Correction proposée** :

```cpp
Soit supprimer les moves (le copy est bon marché : un incrément atomique) :
    copy_on_write(copy_on_write&&) = delete;
    copy_on_write& operator=(copy_on_write&&) = delete;
soit les définir comme des copies (ne pas vider la source). Ajouter les static_assert correspondants dans test_copy_on_write.cpp.
```


> _Vérification_ : Confirmed by reading include/threadsafe/details/copy_on_write.h: the class has no user-declared copy/move/destructor, so implicit move operations null out ptr_. operator* (line 26, noexcept) and operator-> then dereference nullptr, and as_mutable() on a moved-from object takes the use_count()!=1 branch (use_count()==0) and executes std::make_shared<T>(*ptr_), also a null deref. The only constructor always make_shared's, so the never-null invariant exists but is broken silently by default moves; no test exercises move.


### 17. 🟠 Majeur — copy_on_write::as_mutable : la T& retournée survit à la garantie d'unicité

**Fichier** : `include/threadsafe/details/copy_on_write.h`


`T& as_mutable()` détache si `use_count() != 1` puis retourne `*ptr_`. Mais la référence retournée n'est liée à rien : sur le même thread, `T& r = c.as_mutable(); auto c2 = c; launcher.launch_task(f, c2); r.mutate();` — après la copie, le bloc est de nouveau partagé et l'écriture via `r` court-circuite le détachement : data race avec le lecteur de `c2`, jamais vue par le check compile-time (cow<T> reste vouché sendable). Rust résout cela avec le borrow checker (`Arc::make_mut`) ; ici au minimum le pattern mérite d'être fermé par un guard à la synchronized_value (un objet qui garde la référence et dont la durée de vie borne l'écriture), ou documenté comme contrat.


**Correction proposée** :

```cpp
Retourner un write-guard non copiable/non movable qui expose T& par operator*/operator-> const& (et delete sur rvalue), au lieu d'une T& nue ; ou a minima documenter que la référence est invalidée par toute copie ultérieure du copy_on_write.
```


> _Vérification_ : Confirmed by reading include/threadsafe/details/copy_on_write.h (lines 29-45). `as_mutable()` returns `*ptr_` as a bare `T&` with no guard object; the implicitly-defaulted copy constructor copies the shared_ptr, so after `T& r = c.as_mutable(); auto c2 = c;` the control block is shared again while `r` still aliases the shared object. Writing through `r` on one thread while another thread reads `*c2` is a data race, and `is_unsafe_sendable<copy_on_write<T>>` (line 44-45) vouches the type sendable (`is_sendable_v<T> && is_synchronizable_v<const T>`), so e.g. copy_on_write<int>/<std::string> passes the launcher's compile-time check — nothing in the trait system can see the escaped reference. The scenario in the finding compiles and races exactly as described. Mitigation options (a lifetime-bounding guard object like synchronized_value's, or an explicit documented contract) are apt; note however that even a guard only narrows, not closes, the hole absent a borrow checker, since C++ cannot prevent copying `c` while the guard lives — but the finding itself is real.


### 18. ℹ️ Info — Deadlock runtime possible : lock() réentrant ou ordre de verrouillage

**Fichier** : `include/threadsafe/details/synchronized_value.h`


`std::mutex`/`std::shared_mutex` ne sont pas récursifs : `auto g1 = sv.lock(); auto g2 = sv.lock();` sur le même thread est un deadlock (UB pour std::mutex), et deux synchronized_value verrouillées dans des ordres opposés par deux threads aussi. C'est indétectable au compile-time avec ce design (attendu), mais aucune API n'offre le verrouillage conjoint de plusieurs valeurs.


**Correction proposée** :

```cpp
Optionnel : une fonction amie `lock(synchronized_value&...)` basée sur std::scoped_lock pour le multi-verrouillage sans ordre; sinon documenter la limite (pertinent pour une conférence).
```


### 19. ℹ️ Info — Couverture : le chemin fallback static_assert du launcher n'est vérifié que pour 2 messages

**Fichier** : `tests/build_errors`


Les surcharges non contraintes de launch_task/launch_scoped_task (celles qui portent les static_assert de diagnostic) sont exercées par les build_errors 01-15, mais rien ne vérifie le cas « callable OK, argument non lifetime-aware via launch_scoped_task accepté » à l'envers : launch_scoped_task accepte volontairement des reference_wrapper (test présent) — en revanche aucun build_error n'exerce le message de la surcharge fallback de launch_scoped_task avec un argument non sendable seul (04 teste le callable). Mineur : les asserts positifs/négatifs compile-time dans test_asynchronous_task_launcher.cpp couvrent bien les concepts eux-mêmes. À noter aussi : le static_assert `task_participant<std::stop_token>` dans le corps de la classe est une bonne garde et vérifié à toute instanciation.


### 20. ℹ️ Info — copy_on_write::as_mutable : la fence acquire est correcte mais mérite un commentaire-test

**Fichier** : `include/threadsafe/details/copy_on_write.h`


Le motif `if (ptr_.use_count() != 1) detach; else std::atomic_thread_fence(std::memory_order_acquire);` est le bon (synchronise avec le décrément release du dernier autre détenteur, comme le destructeur de shared_ptr), mais `use_count()` est spécifié sans garantie de synchronisation par le standard — le code repose sur le fait que l'implémentation lit le compteur atomiquement (vrai en pratique sur libstdc++). Pour un code éducatif, ce point subtil gagnerait un commentaire ou une assert de doc; aucun bug concret sur la toolchain visée (GCC 16).


## Simplicité / valeur éducative


### 21. 🟠 Majeur — Dépendance d'include cachée : smart_pointers.h utilise wrapped_types_of sans inclure allowed_std_wrappers.h

**Fichier** : `include/threadsafe/details/smart_pointers.h`


`pointee_is_lifetime_aware` appelle `wrapped_types_of(^^T)` défini dans allowed_std_wrappers.h, mais smart_pointers.h n'inclut que lifetime_aware.h, sendable.h et synchronizable.h. Ça compile uniquement parce que threadsafe.h inclut allowed_std_wrappers.h en premier — un header non auto-suffisant, fragile et pédagogiquement trompeur. De plus, sémantiquement, `wrapped_types_of` sur `std::unique_ptr<T, D>` traite le deleter D comme un « pointee » et lui pose `pointee_answer(..., is_lifetime_aware_type)` — le nom ment sur ce qu'il fait.


**Correction proposée** :

```cpp
Soit inclure allowed_std_wrappers.h, soit (mieux) écrire des spécialisations explicites par pointeur : `is_unsafe_lifetime_aware<std::unique_ptr<T,D>> : bool_constant<pointee_answer(^^T, is_lifetime_aware_type) && is_lifetime_aware_v<D>>` etc., qui n'ont plus besoin de wrapped_types_of et disent exactement ce qu'elles vérifient.
```


> _Vérification_ : Confirmed by standalone compile: a TU including only smart_pointers.h fails with "a declaration of 'wrapped_types_of' must be available" (plus missing std::ranges::all_of — no <ranges>/<algorithm> include). wrapped_types_of is defined only in allowed_std_wrappers.h and no transitive include of smart_pointers.h reaches it; it compiles only via threadsafe.h's include order. The semantic note also holds: wrapped_types_of on unique_ptr<T> yields both T and std::default_delete<T>, so the deleter is queried through pointee_answer as if it were a pointee.


### 22. 🟠 Majeur — copy_on_write : atomic_thread_fence acquire non expliqué et à la valeur douteuse

**Fichier** : `include/threadsafe/details/copy_on_write.h`


Dans `as_mutable()`, la branche `else std::atomic_thread_fence(std::memory_order_acquire);` quand `use_count() == 1` est le point le plus subtil de tout le fichier (synchronisation avec la destruction de la dernière autre copie, à la manière du compteur de shared_ptr) et n'est ni expliqué ni testable à la compilation. Pour du code éducatif, une barrière mémoire muette est pire qu'absente : soit elle est nécessaire et mérite l'explication, soit elle relève du culte du cargo. Par ailleurs `ptr_.use_count()` est documenté comme approximatif en présence de weak_ptr/threads ; l'argument de sûreté (aucun weak_ptr n'est jamais émis, et use_count==1 implique propriété exclusive locale) devrait être énoncé.


**Correction proposée** :

```cpp
Ajouter la justification en une phrase au-dessus de la fence (synchronise-avec la décrémentation release du dernier autre propriétaire), ou la retirer si l'analyse montre qu'elle est inutile.
```


> _Vérification_ : Verified in include/threadsafe/details/copy_on_write.h lines 29-37: as_mutable() has `else std::atomic_thread_fence(std::memory_order_acquire);` with no comment in the file (or nearby docs) explaining it synchronizes with the release operation of the last other copy's destruction, nor stating the argument why use_count()==1 is reliable here (no weak_ptr ever emitted, ptr_ never shared outside the class). The fence is likely correct and needed (same pattern as shared_ptr's control block), so this is a documentation/pedagogy finding, not a correctness bug — but for explicitly educational code the critique is accurate: the subtlest line of the file is mute.


### 23. 🟡 Mineur — Code mort : is_smart_pointer_type et is_smart_pointer_v ne sont utilisés nulle part

**Fichier** : `include/threadsafe/details/smart_pointers.h`


`is_smart_pointer_type(std::meta::info)` (et le `is_smart_pointer_v` qu'il instancie via `trait_value`) n'est référencé nulle part dans include/ ni tests/. Seul le concept `smart_pointer` sert (dans `template <smart_pointer T> struct is_unsafe_lifetime_aware`). Pour une conférence, chaque symbole doit gagner sa place.


**Correction proposée** :

```cpp
Supprimer `is_smart_pointer_type` et `is_smart_pointer_v`; garder `is_smart_pointer` + le concept `smart_pointer`.
```


> _Vérification_ : Confirmed. In include/threadsafe/details/smart_pointers.h, `is_smart_pointer_type(std::meta::info)` (line 47) and `is_smart_pointer_v` (line 42) have zero call sites: a full-repo grep finds them only at their definitions. `is_smart_pointer_v` is referenced solely by `is_smart_pointer_type` itself (via `trait_value(^^is_smart_pointer_v, ...)`), which is itself unreferenced — so both are dead together. The only live consumer of the machinery is the `smart_pointer` concept (which reads `is_smart_pointer<T>::value` directly, not `_v`), used by `template <smart_pointer T> struct is_unsafe_lifetime_aware<T>` in the same file. Both symbols can be deleted with no effect on include/ or tests/.


### 24. 🟡 Mineur — Quadruple duplication de pointee_is_synchronizable pour shared_ptr / weak_ptr / reference_wrapper

**Fichier** : `include/threadsafe/details/smart_pointers.h`


Huit spécialisations identiques : `is_unsafe_sendable<std::shared_ptr<T>>`, `<std::weak_ptr<T>>`, `<std::reference_wrapper<T>>` et leurs pendants `is_unsafe_synchronizable<const ...>` héritent toutes de `bool_constant<detail::pointee_is_synchronizable<T>()>`. C'est le même énoncé répété six fois.


**Correction proposée** :

```cpp
Un alias intermédiaire lisible, p.ex. `template <class T> using shares_its_pointee = std::bool_constant<detail::pointee_is_synchronizable<T>()>;` puis six lignes `struct is_unsafe_sendable<std::shared_ptr<T>> : shares_its_pointee<T> {};` — la règle « partagé ⇒ le pointé doit être synchronizable » n'est énoncée qu'une fois.
```


> _Vérification_ : Confirmed: six specializations in include/threadsafe/details/smart_pointers.h (lines 69-96) — is_unsafe_sendable and is_unsafe_synchronizable<const ...> for shared_ptr, weak_ptr, reference_wrapper — all inherit from the identical expression bool_constant<detail::pointee_is_synchronizable<T>()>. The finding's count wording is sloppy (says eight/quadruple; unique_ptr's specializations differ and don't belong), but the core duplication claim (same statement repeated six times) is accurate. Style/duplication finding, not a bug.


### 25. 🟡 Mineur — lifetime_aware.h réimplémente trait_value à la main pour borrowed_range

**Fichier** : `include/threadsafe/details/lifetime_aware.h`


`extract<bool>(substitute(^^std::ranges::borrowed_range, {\n type}))` (avec un retour à la ligne cassé au milieu de l'initializer-list) duplique exactement `detail::trait_value` défini dans utils.h et utilisé partout ailleurs.


**Correction proposée** :

```cpp
`if (trait_value(^^std::ranges::borrowed_range, type)) return false;` — plus court, cohérent, et le formatage bizarre disparaît.
```


> _Vérification_ : lifetime_aware.h:59-60 contains `extract<bool>(substitute(^^std::ranges::borrowed_range, {type}))` with a broken line wrap inside the initializer list; utils.h:8-10 defines detail::trait_value as exactly this expression, and lifetime_aware.h itself uses trait_value elsewhere (lines 19, 38). trait_value(^^std::ranges::borrowed_range, type) is a drop-in replacement since substitute accepts a concept. The finding is accurate hand-duplication, though a minor style/DRY issue, not a correctness bug.


### 26. 🟡 Mineur — Appel mort remove_pointer dans la branche lvalue-reference de diagnose_is_sendable

**Fichier** : `include/threadsafe/details/sendable.h`


Dans `if (is_lvalue_reference_type(type)) return is_synchronizable_type(remove_reference(remove_pointer(type)));` le type est une lvalue reference, donc `remove_pointer` est un no-op garanti (la branche pointeur recourt via `add_lvalue_reference(remove_pointer(...))` avant d'arriver ici). Le lecteur cherche un sens qui n'existe pas.


**Correction proposée** :

```cpp
`return is_synchronizable_type(remove_reference(type));`
```


> _Vérification_ : Confirmed: at sendable.h:50-51, type is an lvalue reference, and remove_pointer on any reference type (including reference-to-pointer like int*&) is a no-op since references are not pointer types. The pointer branch at :53-54 already handles pointers via add_lvalue_reference(remove_pointer(...)) before recursing, so line 51's remove_pointer can never strip anything. Dead call; remove_reference(type) alone is equivalent. Cosmetic/readability finding only, no behavioral bug.


### 27. 🟡 Mineur — Test is_synchronizable_type probablement redondant dans is_unsafe_sendable des wrappers std

**Fichier** : `include/threadsafe/details/allowed_std_wrappers.h`


`is_unsafe_sendable<T> : bool_constant<is_synchronizable_type(^^T) || all_wrapped_types(^^T, is_sendable_type)>` : si la claim vaut false, le walk `diagnose_is_sendable` teste de toute façon `is_synchronizable_type(type)` avant d'échouer. Le `||` gauche ne change donc jamais la réponse finale, il ne fait qu'obscurcir la règle « un wrapper est sendable si tout ce qu'il enveloppe l'est ».


**Correction proposée** :

```cpp
`struct is_unsafe_sendable<T> : std::bool_constant<detail::all_wrapped_types(^^T, is_sendable_type)> {};` (vérifier par le build que test_containers.cpp passe toujours).
```


> _Vérification_ : Confirmed. The sendable claim's sole consumer is diagnose_is_sendable (sendable.h:44). For a std_wrapper (class type, never scalar/ref/ptr/array), when the claim is false the walk falls through to line 61 `if (is_scalar_type(type) || is_synchronizable_type(type)) return true;` — the same is_synchronizable_type question, asked before is_walkable_type rejection and before the member walk, through the same _v memo. Therefore `is_synchronizable_type(^^T)` in allowed_std_wrappers.h:85 never changes the final is_sendable_v answer (nor the poisoning order), and the claim could be just `all_wrapped_types(^^T, is_sendable_type)`. No test reads is_unsafe_sendable_v of a wrapper directly. Scope caveat: the similar disjunct in is_unsafe_synchronizable<const T> (line 90) is a separate question not covered by this finding.


### 28. 🟡 Mineur — Style incohérent entre headers : indentation, accolades de namespace, forme des traits

**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h`


Trois incohérences visibles côte à côte sur un slide : (1) asynchronous_task_launcher.h et copy_on_write.h sont indentés à 4 espaces avec `}` nu en fin de namespace, tous les autres headers à 2 espaces avec `} // namespace threadsafe` ; (2) `is_sendable` est un `std::bool_constant<...>` alors que `is_lifetime_aware` utilise `static constexpr bool value = ...` — deux styles pour le même patron ; (3) sendable.h et lifetime_aware.h exposent les concepts `sendable` / `lifetime_aware`, mais il n'existe aucun `concept synchronizable` alors que le trait est du même rang.


**Correction proposée** :

```cpp
Passer un clang-format unique, aligner `is_lifetime_aware` sur `bool_constant`, et ajouter `template <class T> concept synchronizable = is_synchronizable_v<T>;` (ou documenter pourquoi il est volontairement absent).
```


> _Vérification_ : Les trois incohérences sont confirmées. (1) Indentation/fermeture : asynchronous_task_launcher.h et copy_on_write.h sont à 4 espaces (17 et 10 lignes top-level à 4 espaces, 0 à 2) et se terminent par un `}` nu, tandis que sendable.h, lifetime_aware.h, synchronizable.h, etc. sont à 2 espaces et ferment avec `} // namespace threadsafe`. (2) Forme des traits : sendable.h:25 `struct is_sendable : std::bool_constant<detail::diagnose_is_sendable(^^T)> {};` vs lifetime_aware.h:27 `static constexpr bool value = detail::diagnose_is_lifetime_aware(^^T);` — deux styles pour le même patron. (3) Concepts : grep de `concept ` dans include/ montre `sendable` (sendable.h:32) et `lifetime_aware` (lifetime_aware.h:35) mais aucun `concept synchronizable`, alors que le trait is_synchronizable est du même rang.


### 29. 🟡 Mineur — Six noms pour deux concepts dans le launcher

**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h`


`detail::diagnose_scoped_task_participant<T>()`, `is_scoped_task_participant_v`, `scoped_task_participant`, plus le trio non-scoped : trois couches de noms pour `std::move_constructible<T> && is_sendable_v<T> [&& is_lifetime_aware_v<T>]`. Contrairement aux vrais traits, il n'y a ici ni mémoïsation par walk ni customisation — les fonctions detail et les `_v` n'apportent rien que le concept n'exprime.


**Correction proposée** :

```cpp
`template <class T> concept scoped_task_participant = std::move_constructible<T> && sendable<T>;` et `template <class T> concept task_participant = scoped_task_participant<T> && lifetime_aware<T>;` — 4 lignes au lieu de ~20, et les concepts de la lib (`sendable`, `lifetime_aware`) sont enfin montrés en usage.
```


> _Vérification_ : Confirmed: the diagnose_* consteval functions and the is_*_participant_v variables in asynchronous_task_launcher.h are used only to define the two concepts and nowhere else in the repo. Unlike the real traits, they carry no reflective walk to memoize (the underlying _v traits already memoize) and no is_unsafe_* customization point. The two concepts express the whole rule (move_constructible && is_sendable_v [&& is_lifetime_aware_v]) directly; the four extra names are pure layering with no benefit.


### 30. 🟡 Mineur — has_unreflectable_state : nom en négatif de sa logique, conditions surprenantes

**Fichier** : `include/threadsafe/details/utils.h`


`has_unreflectable_state` retourne true pour « non vide, non polymorphe, zéro base, zéro membre » — c'est une heuristique « le sizeof vient d'un état que la réflexion ne voit pas (mutex pthread, etc.) », mais rien ne le dit, et la clause `!is_polymorphic_type(type)` (le vptr explique la taille) est un piège de lecture. C'est LA fonction que le public devra comprendre pour croire au caractère conservateur du walk.


**Correction proposée** :

```cpp
Renommer p.ex. `size_unaccounted_by_reflection` ou introduire des booléens nommés : `const bool nothing_visible = bases_of(...).empty() && nonstatic_data_members_of(...).empty(); const bool size_explained = is_empty_type(type) || is_polymorphic_type(type); return nothing_visible && !size_explained;`
```


> _Vérification_ : The factual description holds: has_unreflectable_state (utils.h:32-37) returns true exactly for non-empty, non-polymorphic, base-less, member-less types, with no comment explaining the "sizeof comes from state reflection cannot see" heuristic, and its only use is double-negated in is_walkable_type (line 101: return !has_unreflectable_state(type)). The !is_polymorphic_type clause is subtle but correct and load-bearing (final polymorphic empties get their size from the vptr; non-final polymorphic types are rejected elsewhere via is_dynamic_type_known). This is a genuine clarity/documentation finding for an explicitly educational library — but no behavioral defect exists, and the title's claim that the name is "en négatif de sa logique" is inaccurate (the name matches the return; only the call site is negated).


### 31. ℹ️ Info — La branche rvalue-reference est implicite et facile à rater

**Fichier** : `include/threadsafe/details/sendable.h`


Après les branches lvalue-ref et pointeur, `type = remove_reference(type);` ne peut concerner qu'une rvalue reference, qui tombe alors dans le walk valeur. La règle « T&& se send comme T » n'est jamais énoncée ; pour un public de conférence c'est le genre de ligne qui demande cinq minutes d'explication orale.


**Correction proposée** :

```cpp
Rendre la branche explicite : `if (is_rvalue_reference_type(type)) return is_sendable_type(remove_reference(type));` et supprimer la mutation de `type`.
```


### 32. ℹ️ Info — Cast bool(...) superflu dans shared_readable

**Fichier** : `include/threadsafe/details/synchronized_value.h`


`static constexpr bool shared_readable = bool(is_synchronizable_v<const T>);` — `is_synchronizable_v` est déjà un `constexpr bool`; le cast suggère à tort qu'il ne l'est pas.


**Correction proposée** :

```cpp
`static constexpr bool shared_readable = is_synchronizable_v<const T>;`
```


### 33. ℹ️ Info — context inutilisé jusqu'au milieu de diagnose_is_synchronizable

**Fichier** : `include/threadsafe/details/synchronizable_base.h`


`const auto context = std::meta::access_context::unchecked();` est déclaré en première ligne mais ne sert qu'après ~8 early-returns, contrairement aux autres walks qui délèguent à `all_bases_and_members`. Déplacer la déclaration juste avant les boucles rapprocherait la définition de l'usage ; ou mieux, extraire la boucle membres/mutable/reference dans un helper à côté de `all_bases_and_members` pour que les trois diagnose aient la même silhouette.


**Correction proposée** :

```cpp
Déplacer `const auto context = ...` juste avant `for (auto base : ...)`.
```


## Thread safety & performance runtime


### 34. 🟠 Majeur — copy_on_write::as_mutable rend une T& non gardée qui survit au partage ultérieur (aliasing race)

**Fichier** : `include/threadsafe/details/copy_on_write.h`


`T& as_mutable()` retourne une référence brute vers l'état partagé sans aucun garde. Le check `if (ptr_.use_count() != 1)` est un TOCTOU par rapport à toute copie faite APRÈS l'appel : (1) thread A appelle `as_mutable()` alors que le bloc est unique — pas de copie, il garde `T& ref`; (2) A copie le `copy_on_write` (copie implicite autorisée, la classe est vouched sendable via `is_unsafe_sendable<copy_on_write<T>>`) et l'envoie au thread B; (3) A écrit via `ref` pendant que B lit via `operator*` → data race sur le même `T`, entièrement à travers l'API publique et validée par les traits (rien ne trace la référence retournée). C'est l'écart entre le modèle « copie = snapshot » et la référence mutable persistante. Code : `T& as_mutable() { if (ptr_.use_count() != 1) ptr_ = std::make_shared<T>(*ptr_); ... return *ptr_; }`


**Correction proposée** :

```cpp
Ne pas exposer de T& persistante : soit un guard non copiable à la synchronized_value (avec operator*/-> supprimés sur temporaire), soit une API par callback `template<class F> decltype(auto) update(F f) { ...; return f(*ptr_); }` qui borne la durée de vie de l'accès mutable, soit documenter/asserter que la référence invalide toute copie ultérieure.
```


> _Vérification_ : Confirmé par lecture du code : as_mutable() (copy_on_write.h:29-37) retourne `T&` brut après un check use_count() ponctuel ; le constructeur de copie implicite (le ctor template exclut copy_on_write, donc la copie du shared_ptr partage le bloc) permet de copier APRÈS l'appel, et is_unsafe_sendable<copy_on_write<T>> (lignes 43-45) vouch l'envoi à un autre thread pour tout T sendable avec const T synchronizable (ex. int). A garde la T& d'un bloc alors unique, copie et envoie à B ; A écrit via la ref pendant que B lit via operator* sur le même T → data race à travers l'API publique, rien n'invalide ni ne trace la référence.


### 35. 🟡 Mineur — asynchronous_task_launcher : threads_ croît sans borne, aucun moissonnage des tâches terminées

**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h`


`launch_task` fait `threads_.emplace_back(...)` et rien ne retire jamais les jthread terminés avant la destruction du launcher. Un launcher longue durée accumule un descripteur de thread OS + une entrée vecteur par tâche, même finie depuis longtemps. De plus la destruction joint séquentiellement dans l'ordre d'insertion (correct mais peut être long). Pas de data race (le launcher n'est ni vouched synchronizable ni copiable, donc le walk empêche son partage), mais c'est un coût runtime réel pour un composant présenté comme un lanceur générique.


**Correction proposée** :

```cpp
Ajouter un balayage des threads joignables terminés (p.ex. via un compteur/flag par tâche, ou `std::erase_if` sur un état done) ou exposer un `wait_all()` qui joint et vide `threads_`.
```


> _Vérification_ : Confirmé par lecture de include/threadsafe/details/asynchronous_task_launcher.h. `launch_task` fait `threads_.emplace_back(std::move(f), std::move(args)...)` (ligne 57) et aucune méthode ne retire jamais les jthread terminés — la classe n'a que launch_task/launch_scoped_task et le membre `std::vector<std::jthread> threads_` (ligne 86); pas de reap, pas de join intermédiaire, pas de clear. Chaque tâche lancée conserve donc un descripteur de thread OS joinable + une entrée vecteur jusqu'à la destruction du launcher, qui joint séquentiellement via les destructeurs de jthread dans l'ordre du vecteur. Aucune data race (pas de vouch synchronizable, walk conservateur bloque le partage), donc le finding est correctement qualifié: coût runtime réel (croissance non bornée) pour un launcher longue durée, pas un bug de sûreté. Nuance atténuante: la bibliothèque est explicitement pédagogique (conférence) et les tests sont compile-time uniquement, donc c'est peut-être un choix de simplicité assumé — mais le comportement décrit est exact.


### 36. 🟡 Mineur — launch_scoped_task crée un thread pour le joindre immédiatement — exécution 100% synchrone au prix d'un spawn

**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h`


`launch_scoped_task` fait `std::jthread task{std::move(f), ...}; task.join();` : l'appelant bloque jusqu'à la fin, donc il n'y a aucune concurrence gagnée, mais on paie la création + destruction d'un thread OS (~dizaines de µs) à chaque appel. Si l'intention est « le join borne la durée de vie donc lifetime_aware n'est pas exigé », l'API n'exprime pas cela : elle ressemble à un lancement asynchrone. Au minimum le nom/doc devrait dire que c'est bloquant; idéalement retourner le jthread (non joint) dans un scope RAII pour permettre du vrai parallélisme borné (plusieurs tâches puis join).


**Correction proposée** :

```cpp
Retourner un objet scope (jthread joint au destructeur de l'appelant) ou accepter un pack de tâches lancées en parallèle puis toutes jointes, au lieu de spawn+join une par une.
```


> _Vérification_ : Verified in asynchronous_task_launcher.h lines 72-75: launch_scoped_task constructs a std::jthread then calls task.join() immediately, so execution is fully synchronous with an OS thread spawned per call. The relaxed constraint (scoped_task_participant drops is_lifetime_aware_v) shows the intent is 'join bounds lifetime', but the API name inside a class called asynchronous_task_launcher suggests async launch and returns nothing to allow real bounded parallelism. Finding is factually correct; only caveat is the library is educational, so the simplification may be deliberate.


### 37. 🟡 Mineur — launch_task prend F et Args par valeur : copie + move au lieu d'une seule decay-copy

**Fichier** : `include/threadsafe/details/asynchronous_task_launcher.h`


`void launch_task(F f, Args... args) { threads_.emplace_back(std::move(f), std::move(args)...); }` : pour un argument lvalue, on paie une copie dans le paramètre puis un move dans l'état du jthread, alors que jthread fait déjà sa propre decay-copy. Avec des arguments coûteux à déplacer (T sans move, gros tableaux) c'est une copie supplémentaire complète. Un forwarding reference `template<class F, class... Args> requires launchable_task<std::decay_t<F>, std::decay_t<Args>...> void launch_task(F&& f, Args&&... args) { threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...); }` élimine l'étape intermédiaire tout en gardant les contraintes sur les types decayed (ceux que jthread stocke réellement).


**Correction proposée** :

```cpp
Passer par forwarding references et contraindre sur std::decay_t, en transférant via std::forward directement dans emplace_back.
```


> _Vérification_ : Confirmed in asynchronous_task_launcher.h:54-75: launch_task/launch_scoped_task take F and Args by deduced value then std::move into jthread, which itself decay-copies — so lvalue arguments pay copy+move instead of one decay-copy. Forwarding references with constraints on std::decay_t would remove the intermediate object. Minor overstatement: types without move are impossible (task_participant requires std::move_constructible), so the extra step is an extra move, costly mainly for types whose move is element-wise (large arrays); the fallback static_assert overloads must be updated in tandem.


### 38. ℹ️ Info — synchronized_value : lock() puis lock_shared() (ou lock() réentrant) sur le même thread = deadlock/UB non signalé

**Fichier** : `include/threadsafe/details/synchronized_value.h`


Les mutex sont non récursifs (`std::mutex` / `std::shared_mutex`). `auto g = sv.lock(); auto g2 = sv.lock_shared();` sur le même thread est un deadlock (et pour shared_mutex, comportement indéfini). Rien dans l'API ne le prévient et il n'existe pas d'API multi-verrous (`std::scoped_lock`-style) pour verrouiller deux synchronized_value sans risque d'interblocage croisé entre threads. C'est le piège runtime le plus facile à provoquer avec cette classe; le reste de sa conception est solide (guard non copiable, operator*/-> supprimés sur temporaire, choix shared_mutex conditionné correctement par is_synchronizable_v<const T>, vouching cohérent avec Mutex<T>: Sync ⇔ T: Send).


**Correction proposée** :

```cpp
Ajouter un `apply(sv1, sv2, f)` basé sur std::scoped_lock pour le multi-verrouillage, et documenter la non-réentrance.
```


### 39. ℹ️ Info — copy_on_write : la barrière acquire est correcte mais le protocole use_count mérite un commentaire — et as_mutable copie toujours, jamais de move

**Fichier** : `include/threadsafe/details/copy_on_write.h`


Le motif `if (use_count() != 1) copie; else atomic_thread_fence(acquire);` est valide (load relâché du compteur observant le décrément release d'un autre thread + fence acquire ⇒ synchronizes-with), c'est le même protocole que les implémentations COW de libstdc++ — mais rien ne l'explique dans un code annoncé comme pédagogique. Côté performance : quand le bloc est partagé, `std::make_shared<T>(*ptr_)` copy-construit toujours; pour un usage « je vais tout remplacer », un `assign(T new_value)` éviterait la copie de l'ancien contenu (make_shared + move) au lieu de copier puis écraser.


**Correction proposée** :

```cpp
Ajouter un commentaire justifiant la fence, et éventuellement un `void assign(T v) { ptr_ = std::make_shared<T>(std::move(v)); }`.
```


## Performance compilation, API & flexibilité


### 40. 🟠 Majeur — Diagnostics: aucun chemin vers le membre fautif malgré des fonctions nommées diagnose_*

**Fichier** : `include/threadsafe/details/sendable.h`


`detail::diagnose_is_sendable(std::meta::info)` retourne un `bool` nu ; quand `static_assert(sendable<T>)` échoue dans le launcher ou `synchronized_value`, l'utilisateur ne sait pas QUEL membre/base a fait échouer le walk (ex. un `int*` enfoui à 3 niveaux). Pour une bibliothèque pédagogique de conférence, c'est le point d'ergonomie n°1. C++26 permet des messages `static_assert` générés : un walk parallèle qui construit le chemin (`"Outer::middle -> Borrowing::borrowed is a raw pointer"`) utilisable dans les messages serait un gros gain sans toucher au walk memoïsé (on ne le lance que dans la branche d'échec, donc coût nul sur le chemin succès).


**Correction proposée** :

```cpp
template <class T> consteval std::string why_not_sendable(); // walk non-memoïsé, appelé uniquement dans la branche fallback : static_assert(sendable<T>, detail::why_not_sendable<T>());
```


> _Vérification_ : Confirmed. `detail::diagnose_is_sendable(std::meta::info)` in include/threadsafe/details/sendable.h (lines 40-68) returns a bare bool with no record of which base/member failed, and every point of use asserts with a fixed string literal: synchronized_value.h:59 ("the mutex serializes access... T must be sendable") and asynchronous_task_launcher.h:62/65/79/81 ("the callable must be movable, sendable and lifetime-aware", etc.). No header builds a member path or uses C++26 user-generated static_assert messages; the only member-naming diagnostics anywhere are the void/incomplete rejections in utils.h (assert_queryable_type), which cover a different case. So a failure on e.g. a raw pointer buried 3 levels deep names only the outer T, never the offending member — the finding's claim holds. The proposed fix is also architecturally plausible: the walk is memoized via _v and the diagnostic pass would only run in the failure branch.


### 41. 🟠 Majeur — Pas de concept `synchronizable` alors que `sendable` et `lifetime_aware` existent

**Fichier** : `include/threadsafe/details/synchronizable_base.h`


`sendable.h` définit `concept sendable` et `lifetime_aware.h` définit `concept lifetime_aware`, mais `synchronizable_base.h` ne définit aucun `concept synchronizable`. Un utilisateur ne peut pas écrire `template <synchronizable T>` ni `requires synchronizable<const T>` de façon symétrique — incohérence d'API gratuite.


**Correction proposée** :

```cpp
template <class T> concept synchronizable = is_synchronizable_v<T>;
```


> _Vérification_ : Confirmed by grep over include/: `concept sendable` (sendable.h:32) and `concept lifetime_aware` (lifetime_aware.h:35) exist, and `is_synchronizable_v` is defined (synchronizable_base.h:32), but no `concept synchronizable` is declared anywhere in the headers. Users cannot write `template <synchronizable T>` symmetrically with the other two traits.


### 42. 🟠 Majeur — Vouch complet = 3 à 4 spécialisations à écrire à la main

**Fichier** : `include/threadsafe/details/vocabulary.h`


Pour vouch un type opaque sur les trois traits il faut écrire `is_unsafe_sendable<T>`, `is_unsafe_synchronizable<const T>` et `is_unsafe_lifetime_aware<T>` (voir vocabulary.h qui répète ce triplet pour allocator, stop_token, stop_source — 9 spécialisations pour 3 types). Aucun raccourci n'est offert à l'utilisateur alors que c'est LE point de customisation de la bibliothèque.


**Correction proposée** :

```cpp
Ajouter un agrégateur opt-in, ex. `template <class T> struct is_unsafe_thread_safe : std::false_type {};` consulté par les trois couches unsafe (ou une macro THREADSAFE_TRUST(type)) ; vocabulary.h devient 3 lignes.
```


> _Vérification_ : Confirmed: vocabulary.h has exactly 9 is_unsafe_* specializations for 3 types, and no shortcut (macro, helper base, or vouch-all mechanism) exists anywhere in include/ — the library itself repeats the same triplet in allowed_std_wrappers.h and smart_pointers.h. Minor nuance: is_unsafe_synchronizable<const T> defaults to the non-const specialization (synchronizable_base.h:14), so a user can write the non-const form, but 3 hand-written specializations per trait per type are still required. The verbosity may be intentional (explicit 'unsafe' at each trust assertion), but the ergonomic gap described is real.


### 43. 🟠 Majeur — is_default_type/is_walkable_type recalculés par trait : members_of itéré jusqu'à 3 fois par type

**Fichier** : `include/threadsafe/details/utils.h`


`is_walkable_type` (qui appelle `is_default_type`, itérant TOUS les `members_of` y compris les fonctions, plus `has_unreflectable_state`) est une fonction consteval libre appelée depuis `diagnose_is_sendable`, `diagnose_is_synchronizable` et `diagnose_is_lifetime_aware`. Contrairement aux traits, elle n'est pas memoïsée via un variable template : demander les 3 traits sur une hiérarchie profonde re-parcourt les listes de membres 3 fois par type. C'est la partie la plus coûteuse du walk (members_of inclut les templates de fonctions).


**Correction proposée** :

```cpp
template <class T> constexpr bool is_walkable_v = is_walkable_type(^^T); // et dans les walks : trait_value(^^is_walkable_v, type) pour profiter de la memoïsation d'instanciation.
```


> _Vérification_ : is_walkable_type (utils.h:91) is a non-memoized consteval free function called from all three trait walks (sendable.h:64, synchronizable_base.h:63, lifetime_aware.h:66). It invokes is_default_type, which iterates the full members_of list (including function templates), plus has_unreflectable_state. Only the trait _v variable templates are memoized, per trait; querying the 3 traits on the same type re-iterates member lists up to 3 times per type. Compile-time perf finding, factually accurate.


### 44. 🟡 Mineur — Chaque is_sendable_v<T> instancie aussi is_synchronizable_v<T> même quand c'est inutile

**Fichier** : `include/threadsafe/details/sendable.h`


Dans `diagnose_is_sendable` : `if (is_scalar_type(type) || is_synchronizable_type(type)) return true;` — pour chaque type classe non-const interrogé en sendable, on instancie `is_synchronizable_v<T>` (qui répond false quasi immédiatement via `!is_const`, mais paie l'instanciation du variable template + assert_queryable + la recherche de spécialisation partielle unsafe). Ce test ne sert que les rares types vouchés synchronizable non-const (atomic, synchronized_value). Le déplacer APRÈS l'échec du walk membre (ou tester d'abord `is_unsafe_synchronizable_type`) éviterait une instanciation par type sur le chemin commun.


**Correction proposée** :

```cpp
if (is_scalar_type(type)) return true; if (is_unsafe_synchronizable_type(type)) return true; // le walk complet synchronizable n'apporte rien à un type non-const non vouché
```


> _Vérification_ : Confirmed: sendable.h:61 evaluates is_synchronizable_type(type) for every non-const class type before the member walk, instantiating is_synchronizable_v<T> (assert_queryable + unsafe partial-spec lookup) even though non-const types can only be synchronizable via an unsafe voucher (diagnose_is_synchronizable returns false at !is_const for all others). The check only serves vouched types like std::atomic. However, severity is minimal (one memoized instantiation per type per TU, no correctness issue), and the finding's alternative fix — testing is_unsafe_synchronizable_type first — would break bare function types, which are sendable only via the function-type branch inside diagnose_is_synchronizable; only the "move after walk failure" variant preserves semantics, and it merely shifts the cost to walk-failing types.


### 45. 🟡 Mineur — Les spécialisations utilisateur is_unsafe_sendable<const T> sont silencieusement mortes

**Fichier** : `include/threadsafe/details/sendable.h`


`diagnose_is_sendable` (et `diagnose_is_lifetime_aware`) strippe cv AVANT d'interroger la couche unsafe : `if (const auto unqualified = remove_cv(type); unqualified != type) return is_sendable_type(unqualified);`. Une spécialisation `is_unsafe_sendable<const MyT>` n'est donc jamais consultée — piège silencieux, d'autant que la convention synchronizable, elle, EXIGE la forme `const T`. La règle « sendable/lifetime : sans const ; synchronizable : avec const » n'est vérifiée nulle part.


**Correction proposée** :

```cpp
Ajouter dans le primary : template <class T> struct is_unsafe_sendable<const T> { static_assert(false, "vouch the unqualified T: the walk strips cv before asking"); }; (idem lifetime_aware).
```


> _Vérification_ : Confirmed in sendable.h:41-44 and lifetime_aware.h:44-47: remove_cv recursion happens before the is_unsafe_* query, so a user specialization on const T is never instantiated and silently ignored. The asymmetric convention is real (library writes synchronizable vouches on const T, sendable/lifetime on T) and only synchronizable has a const-forwarding partial spec (synchronizable_base.h:14); nothing diagnoses a misplaced const-form sendable/lifetime spec.


### 46. ℹ️ Info — borrowed_range instancié avant les sorties bon marché dans le walk lifetime_aware

**Fichier** : `include/threadsafe/details/lifetime_aware.h`


`diagnose_is_lifetime_aware` évalue `substitute(^^std::ranges::borrowed_range, {type})` (résolution d'overload begin/end complète) avant `is_scalar_type(type)` et avant `is_walkable_type`. Inverser (scalaire d'abord, borrowed_range juste avant le walk membre) évite une instanciation de concept pour tous les scalaires et types non-walkables rencontrés en récursion.


**Correction proposée** :

```cpp
if (is_scalar_type(type)) return true; if (!is_walkable_type(type)) return false; if (extract<bool>(substitute(^^std::ranges::borrowed_range, {type}))) return false;
```


### 47. ℹ️ Info — Wrappers std utiles absents de allowed_std_wrappers : expected, span exclu sans message, bitset, chrono

**Fichier** : `include/threadsafe/details/allowed_std_wrappers.h`


La liste couvre conteneurs + pair/tuple/optional/variant/array mais pas `std::expected` (valeur pure, même statut qu'optional/variant), ni `std::flat_map`/`std::flat_set`, ni `std::bitset`/`std::chrono::duration`/`time_point` (ceux-ci passent peut-être par le walk mais dépendent des détails d'implémentation libstdc++ — is_default_type peut les rejeter sur un membre non-defaulted). Un utilisateur avec `std::expected<Data, Error>` doit vouch à la main alors que la preuve est identique à variant. Par ailleurs `std::string_view`/`std::span` échouent (borrowed_range) mais seulement pour lifetime_aware — pour sendable ils échouent via is_walkable sans indice sur la raison.


**Correction proposée** :

```cpp
Ajouter ^^std::expected (et flat_map/flat_set si dispo GCC 16) à allowed_std_wrappers ; vouch explicitement bitset/duration/time_point dans vocabulary.h plutôt que dépendre du walk sur l'implémentation.
```


### 48. ℹ️ Info — Incohérence de style : is_lifetime_aware n'utilise pas bool_constant

**Fichier** : `include/threadsafe/details/lifetime_aware.h`


`is_sendable` et `is_synchronizable` héritent de `std::bool_constant<...>` mais `is_lifetime_aware` déclare `static constexpr bool value = ...;` à la main. Aucun impact fonctionnel, mais le type perd l'héritage true_type/false_type (tag dispatch, conversions) que les deux autres offrent — surprise pour du code générique.


**Correction proposée** :

```cpp
template <class T> struct is_lifetime_aware : std::bool_constant<detail::diagnose_is_lifetime_aware(^^T)> {};
```
