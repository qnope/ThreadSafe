# Audit ThreadSafe — détail des constats

Ce document contient, pour chaque constat, le code incriminé et la correction.
Il est le compagnon de [`audit-synthese.md`](audit-synthese.md), qui donne la
lecture d'ensemble et l'ordre des travaux.

**Méthode.** Chaque constat a été produit par un auditeur qui devait le prouver en
compilant une sonde, puis soumis à un second agent chargé de le *réfuter* :
recompiler la sonde, vérifier que le code cité existe bien à cette ligne, appliquer
la correction sur une copie des en-têtes et recompiler les 12 fichiers de tests.
66 constats ont survécu à cette contre-épreuve ; 16 ont été réfutés et
sont listés dans la synthèse.

Compilateur : `g++-16 (Homebrew GCC 16.2.0)`, `-std=c++26 -freflection`, sur Apple
Silicon. Sonde type :

```bash
g++-16 -std=c++26 -freflection -fsyntax-only -I include sonde.cpp
```

**Sévérités.** *CRITIQUE* : trou de sûreté — un trait répond OUI sur un type
dangereux, ou une course de données réelle. *MAJEUR* : faux négatif bloquant, piège
d'API silencieux, exigence de `Task.md` non satisfaite, ou gain de performance
mesuré et significatif. *MINEUR* : friction. *DÉTAIL* : cosmétique, ou constat
informatif à conserver tel quel.

---

## Index

| Id | Sévérité | Emplacement | Constat |
|---|---|---|---|
| [SEND-01](#send-01--une-reference-rvalue-t-est-traitee-comme-une-valeur-t--un-membre-int-r-est-declare-sendable) | CRITIQUE | `sendable.h:53` | Une reference rvalue T&& est traitee comme une valeur T |
| [SEND-02](#send-02--les-traits-surs-ne-sont-pas-clos--is_sendable--is_synchronizable--is_lifetime_aware-sont-specialisables-et-la-contrefacon-traverse-le-walk) | MAJEUR | `sendable.h:25` | Les traits surs ne sont pas clos |
| [SEND-03](#send-03--linvariant-documente-is_sendablet--is_synchronizablet-est-faux-pour-t--const-u) | MINEUR | `sendable.h:56` | L'invariant documente `is_sendable<T*> == is_synchronizable<T>` est faux pour `T = const… |
| [SYNC-01](#sync-01--stdatomic_flag-nest-pas-vouché-alors-que-stdatomic-lest--lidiome--mutable-flag--est-rejeté) | MINEUR | `synchronizable.h:11` | std::atomic_flag n'est pas vouché alors que std::atomic l'est |
| [SYNC-02](#sync-02--is_unsafe_synchronizablet-sans-const-accorde-silencieusement-le-partage-mutable-concurrent) | MINEUR | `synchronizable_base.h:14` | is_unsafe_synchronizable<T> sans const accorde silencieusement le partage MUTABLE concur… |
| [SYNC-03](#sync-03--aucun-concept-synchronizable-alors-que-sendable-et-lifetime_aware-existent) | MINEUR | `synchronizable_base.h:33` | Aucun concept `synchronizable`, alors que `sendable` et `lifetime_aware` existent |
| [SYNC-04](#sync-04--un-membre-de-donnee-statique-mute-par-une-methode-const-ne-fait-pas-echouer-is_synchronizableconst-t-sans-test-ni-note) | MINEUR | `synchronizable_base.h:71` | Un membre de donnee STATIQUE mute par une methode const ne fait pas echouer is_synchroni… |
| [SYNC-05](#sync-05--volatile-fait-perdre-les-vouches-écrites-à-la-main-atomic-shared_ptr-mais-pas-celles-des-wrappers-std) | DÉTAIL | `synchronizable_base.h:13` | volatile fait perdre les vouches écrites à la main (atomic, shared_ptr) mais pas celles … |
| [LIFE-01](#life-01--is_smart_pointer-est-une-porte-derobee-non-marquee-unsafe-qui-accorde-is_lifetime_aware-a-nimporte-quel-emprunt) | CRITIQUE | `smart_pointers.h:51` | is_smart_pointer est une porte derobee non marquee "unsafe" qui accorde is_lifetime_awar… |
| [LIFE-02](#life-02--un-type-recursif-possedant-node-contenant-unique_ptrnode-ou-vectornode-fait-echouer-tout-trait-avec-une-cascade-derreurs-illisible) | MAJEUR | `lifetime_aware.h:32` | Un type recursif possedant (Node contenant unique_ptr<Node> ou vector<Node>) fait echoue… |
| [LIFE-03](#life-03--pointee_is_lifetime_aware-applique-is_dynamic_type_known-au-deleter-de-unique_ptr-qui-est-pourtant-stocke-par-valeur) | MINEUR | `smart_pointers.h:15` | pointee_is_lifetime_aware applique is_dynamic_type_known au deleter de unique_ptr, qui e… |
| [LIFE-04](#life-04--un-type-non-template-inscrit-dans-is_smart_pointer-declenche-une-stdmetaexception-non-rattrapee-au-lieu-dun-diagnostic) | MINEUR | `smart_pointers.h:16` | Un type non-template inscrit dans is_smart_pointer declenche une std::meta::exception no… |
| [LIFE-05](#life-05--asymetrie-fonction-pointeur--fonction-reference-void-est-lifetime_aware-void-ne-lest-pas) | MINEUR | `lifetime_aware.h:50` | Asymetrie fonction-pointeur / fonction-reference: void(*)() est lifetime_aware, void(&)(… |
| [LIFE-06](#life-06--stdweak_ptr-est-declare-lifetime_aware-alors-quil-ne-maintient-pas-son-referent-en-vie) | DÉTAIL | `smart_pointers.h:51` | std::weak_ptr est declare lifetime_aware alors qu'il ne maintient pas son referent en vie |
| [WALK-01](#walk-01--un-pointeur-vers-type-incomplet-pimpl-sort-une-erreur-de-precondition-gcc-au-lieu-du-static_assert-de-la-bibliotheque) | MAJEUR | `utils.h:24` | Un pointeur vers type incomplet (PImpl) sort une erreur de precondition GCC au lieu du s… |
| [WALK-02](#walk-02--faux-negatifs-en-masse-sur-la-bibliotheque-standard--la-liste-allowed_std_wrappers-est-fermee-et-un-utilisateur-ne-peut-pas-letendre) | MINEUR | `allowed_std_wrappers.h:29` | Faux negatifs en masse sur la bibliotheque standard |
| [WALK-03](#walk-03--interroger-void-produit-deux-static_assert-au-lieu-d-un) | DÉTAIL | `utils.h:16` | Interroger void produit DEUX static_assert au lieu d un |
| [COW-01](#cow-01--operator--est-declare-noexcept-alors-quil-renvoie-une-copie-de-t--stdterminate-prouve) | MAJEUR | `copy_on_write.h:27` | operator*() && est declare noexcept alors qu'il renvoie une COPIE de T |
| [COW-02](#cow-02--as_mutable--na-aucune-contrainte--il-se-declare-disponible-pour-un-t-move-only-puis-explose-dans-le-corps) | MAJEUR | `copy_on_write.h:40` | as_mutable() && n'a aucune contrainte |
| [COW-03](#cow-03--copy_on_writet-na-aucune-règle-is_unsafe_synchronizable--le-type--plusieurs-lecteurs--de-la-bibliothèque-nest-jamais-partageable-en-lecture) | MINEUR | `copy_on_write.h:46` | copy_on_write<T> n'a aucune règle is_unsafe_synchronizable |
| [COW-04](#cow-04--as_mutable-construit-un-happens-before-sur-use_count-que-la-norme-declare-explicitement-approximatif) | MINEUR | `copy_on_write.h:33` | as_mutable() construit un happens-before sur use_count(), que la norme declare explicite… |
| [COW-05](#cow-05--le-constructeur-variadique-ne-permet-pas-la-construction-par-liste--cowstdvectorint-v1-2-3-ne-compile-pas) | MINEUR | `copy_on_write.h:21` | Le constructeur variadique ne permet pas la construction par liste |
| [SV-01](#sv-01--impossible-de-verrouiller-deux-synchronized_value-ensemble-le-deadlock-classique-est-atteignable-et-non-detecte) | MAJEUR | `synchronized_value.h:75` | Impossible de verrouiller deux synchronized_value ensemble: le deadlock classique est at… |
| [SV-02](#sv-02--le-constructeur-variadique-court-circuite-le-constructeur-de-copie-et-de-déplacement-supprimés) | MINEUR | `synchronized_value.h:56` | Le constructeur variadique court-circuite le constructeur de copie et de déplacement sup… |
| [SV-03](#sv-03--stdconstructible_from-ment-sur-synchronized_valuet-quand-t-nest-pas-sendable) | MINEUR | `synchronized_value.h:59` | std::constructible_from ment sur synchronized_value<T> quand T n'est pas sendable |
| [SV-04](#sv-04--lidiome-dune-ligne-svlock-est-interdit-meme-en-lecture-pure-alors-que-la-vraie-fuite-de-reference-passe-sans-un-mot) | MINEUR | `synchronized_value.h:23` | L'idiome d'une ligne *sv.lock() est interdit meme en lecture pure, alors que la vraie fu… |
| [TASK-01](#task-01--launchable_task-nexige-pas-linvocabilité--lerreur-la-plus-fréquente-sort-de-thread-pas-du-launcher) | MINEUR | `asynchronous_task_launcher.h:41` | launchable_task n'exige pas l'invocabilité |
| [TASK-02](#task-02--aucune-lambda-capturante-nest-acceptée-et-le-message-derreur-accuse-le-mauvais-coupable) | MINEUR | `asynchronous_task_launcher.h:77` | Aucune lambda capturante n'est acceptée, et le message d'erreur accuse le mauvais coupable |
| [TASK-03](#task-03--launch_scoped_task-ne-lance-rien-il-execute-les-taches-en-serie) | MINEUR | `asynchronous_task_launcher.h:72` | launch_scoped_task ne lance rien: il execute les taches en serie |
| [TASK-04](#task-04--les-messages-derreur-ne-nomment-jamais-le-coupable-les-fonctions-diagnose_-ne-diagnostiquent-rien) | MINEUR | `asynchronous_task_launcher.h:62` | Les messages d'erreur ne nomment jamais le coupable: les fonctions diagnose_* ne diagnos… |
| [TASK-05](#task-05--aucun-moyen-de-recuperer-une-valeur-de-retour-ni-dattendre-les-taches-sans-detruire-le-launcher) | MINEUR | `asynchronous_task_launcher.h:56` | Aucun moyen de recuperer une valeur de retour ni d'attendre les taches sans detruire le … |
| [DOC-01](#doc-01--claudemd98--lidentité-is_sendablet--is_synchronizablet-est-fausse-dès-que-t-est-cv-qualifié) | MINEUR | `CLAUDE.md:98` | CLAUDE.md:98 |
| [DOC-02](#doc-02--une-specialisation-is_unsafe_-en-false_type-revoque-la-confiance-accordee-par-la-bibliotheque-contrairement-a-la-doc--et-un-test-laffirme-comme-voulu) | MINEUR | `CLAUDE.md:58` | Une specialisation is_unsafe_* en false_type REVOQUE la confiance accordee par la biblio… |
| [SIMP-01](#simp-01--une-référence-nest-traitée-comme-telle-que-dans-la-boucle-des-membres--stdpairstdatomicint-int-est-refusé-là-où-un-membre-stdatomicint-est-accepté) | MINEUR | `synchronizable_base.h:77` | Une référence n'est traitée comme telle que dans la boucle des membres |
| [SIMP-02](#simp-02--le-filet-borrowed_range-nattrape-rien-que-le-walk-nattrape-deja-et-rejette-a-tort-des-vues-possedantes) | MINEUR | `lifetime_aware.h:59` | Le filet borrowed_range n'attrape rien que le walk n'attrape deja, et rejette a tort des… |
| [SIMP-03](#simp-03--trois-couches-de-nommage-detaildiagnose_--_v--concept-pour-une-conjonction-de-deux-termes) | MINEUR | `asynchronous_task_launcher.h:16` | Trois couches de nommage (detail::diagnose_* + _v + concept) pour une conjonction de deu… |
| [SIMP-04](#simp-04--lordre-des-include-de-threadsafeh-est-porteur-de-sens-et-non-commente--trier-la-liste-casse-la-compilation) | MINEUR | `include/threadsafe/threadsafe.h:3` | L'ordre des #include de threadsafe.h est porteur de sens et non commente |
| [SIMP-05](#simp-05--code-mort--is_smart_pointer_v-et-is_smart_pointer_type-ne-sont-utilises-nulle-part) | MINEUR | `smart_pointers.h:41` | Code mort |
| [SIMP-06](#simp-06--lordre-des-branches-de-walk_is_sendable-est-un-invariant-de-soundness-silencieux--deplacer-une-ligne-rend-int-sendable) | MINEUR | `sendable.h:55` | L'ordre des branches de walk_is_sendable est un invariant de soundness silencieux |
| [SIMP-07](#simp-07--is_default_type--default-de-quoi--le-nom-ne-dit-rien-de-ce-qui-est-verifie) | MINEUR | `utils.h:76` | is_default_type |
| [SIMP-08](#simp-08--synchronized_value--la-chaine-de-conditional_t-cache-les-deux-seules-idees-du-type-verrou-ecrivain--verrou-lecteur) | MINEUR | `synchronized_value.h:45` | synchronized_value |
| [SIMP-09](#simp-09--synchronizableh-ne-contient-pas-is_synchronizable--le-decoupage-en-12-fichiers-a-un-nom-qui-trompe) | MINEUR | `synchronizable.h:1` | synchronizable.h ne contient pas is_synchronizable |
| [SIMP-10](#simp-10--asynchronous_task_launcher--deux-fonctions-detail-dune-ligne-imitent-le-protocole-des-traits-sans-en-avoir-la-raison) | MINEUR | `asynchronous_task_launcher.h:14` | asynchronous_task_launcher |
| [SIMP-11](#simp-11--operator---supprime--type-de-retour-t-au-lieu-dun-pointeur-et-noexcept-sur-une-fonction-supprimee) | DÉTAIL | `copy_on_write.h:28` | operator->() && supprime |
| [SIMP-12](#simp-12--indentation-à-4-espaces--seul-fichier-du-projet-à-ne-pas-suivre-le-style-à-2-espaces) | DÉTAIL | `asynchronous_task_launcher.h:17` | Indentation à 4 espaces |
| [SIMP-13](#simp-13--remove_cv-sur-les-membres-dans-all_bases_and_members-est-du-code-mort) | DÉTAIL | `utils.h:48` | remove_cv sur les membres dans all_bases_and_members est du code mort |
| [SIMP-14](#simp-14--diagnose_is_synchronizable-reimplemente-all_bases_and_members--le-walk-existe-en-deux-exemplaires) | DÉTAIL | `synchronizable_base.h:67` | diagnose_is_synchronizable reimplemente all_bases_and_members |
| [SIMP-15](#simp-15--le-prefixe-diagnose_-ment--ces-fonctions-ne-diagnostiquent-rien-elles-marchent-dans-le-type) | DÉTAIL | `sendable.h:40` | Le prefixe diagnose_ ment |
| [SIMP-16](#simp-16--pointee_answerpointee-question--un-nom-substantif-pour-un-predicat-et-un-parametre-appele-question) | DÉTAIL | `utils.h:27` | pointee_answer(pointee, question) |
| [PC-01](#pc-01--le-build-des-tests-paie-deux-passes-par-tu-scan-de-modules-et-aucun-pch---64--disponibles-pour-2-lignes-de-cmake) | MAJEUR | `tests/CMakeLists.txt:15` | Le build des tests paie deux passes par TU (scan de modules) et aucun PCH |
| [PC-02](#pc-02--threadsafeh-impose-mutex-shared_mutex-et-thread-a-qui-ne-veut-que-les-traits--36--par-tu) | MAJEUR | `include/threadsafe/threadsafe.h:3` | threadsafe.h impose <mutex>, <shared_mutex> et <thread> a qui ne veut que les traits |
| [PC-03](#pc-03--le-detour-trait_value-coute-78-us-par-arete-du-graphe-meme-quand-la-reponse-est-deja-memoisee) | MINEUR | `utils.h:8` | Le detour trait_value coute ~78 us par arete du graphe, meme quand la reponse est deja m… |
| [PC-04](#pc-04--wrapped_types_of-alloue-un-stdvector-consteval-par-arete-de-conteneur-std) | MINEUR | `allowed_std_wrappers.h:59` | wrapped_types_of alloue un std::vector consteval par arete de conteneur std |
| [PC-05](#pc-05--au-dela-d-environ-50-niveaux-d-imbrication-a-travers-un-wrapper-std-la-profondeur-constexpr-de-512-explose-en-156-erreurs) | DÉTAIL | `utils.h:9` | Au-dela d environ 50 niveaux d imbrication a travers un wrapper std, la profondeur const… |
| [PC-06](#pc-06--mesure-negative--la-memoisation-par-_v-fonctionne-y-compris-a-travers-les-aretes-du-graphe) | DÉTAIL | `sendable.h:28` | Mesure negative |
| [PC-07](#pc-07--mesure-negative--assert_queryable_type-et-stdremove_all_extents_t-ne-coutent-rien) | DÉTAIL | `utils.h:12` | Mesure negative |
| [PR-01](#pr-01--synchronized_value-choisit-stdshared_mutex-par-defaut--13x-a-163x-plus-lent-et-208-octets-pour-un-int) | MAJEUR | `synchronized_value.h:47` | synchronized_value choisit std::shared_mutex par defaut |
| [PR-02](#pr-02--asynchronous_task_launcher-ne-purge-jamais-threads_--161-ko-de-rss-retenus-par-tache-terminee) | MAJEUR | `asynchronous_task_launcher.h:86` | asynchronous_task_launcher ne purge jamais threads_ |
| [PR-03](#pr-03--launch_task-prend-f-et-args-par-valeur--cout-mesure-nul-le-perfect-forwarding-napporterait-rien) | DÉTAIL | `asynchronous_task_launcher.h:56` | launch_task prend F et Args par valeur |
| [PR-04](#pr-04--verification-demandee--les-traits-ne-laissent-rien-dans-lassembleur-et-value_guard-est-entierement-elide) | DÉTAIL | `synchronized_value.h:75` | Verification demandee |
| [TEST-01](#test-01--ordre-dinclusion--gcc-protege-a-linterieur-dune-tu-mais-deux-tu-divergentes-produisent-une-violation-dodr-silencieuse) | CRITIQUE | `tests/test_deferred_specialization.cpp:35` | Ordre d'inclusion |
| [TEST-02](#test-02--asynchronous_task_launcher-accepte-un-type-unsafe--lexigence-taskmd-nest-ni-respectee-ni-testable) | MAJEUR | `asynchronous_task_launcher.h:60` | asynchronous_task_launcher ACCEPTE un type unsafe |
| [TEST-03](#test-03--zero-test-dexecution--le-detach-de-copy_on_write-le-verrouillage-des-guards-et-le-lancement-des-threads-ne-sont-jamais-executes) | MAJEUR | `tests/CMakeLists.txt:1` | Zero test d'execution |
| [TEST-04](#test-04--les-surcharges--supprimées--le-mécanisme-central-du-guard--ne-sont-couvertes-par-aucun-static_assert) | MINEUR | `tests/test_synchronized_value.cpp:96` | Les surcharges `&&` supprimées |
| [TEST-05](#test-05--le-fichier-de-tests-nappelle-jamais-le-launcher--les-deux-corps-et-les-surcharges-de-diagnostic-ne-sont-jamais-instanciés) | MINEUR | `tests/test_asynchronous_task_launcher.cpp:24` | Le fichier de tests n'appelle jamais le launcher |
| [TEST-06](#test-06--is_lifetime_aware-est-le-trait-le-moins-couvert--ni-enums-ni-unions-ni-classes-vides-ni-lambdas-ni-types-fonction-ni-le-garde-is_default_type) | MINEUR | `tests/test_lifetime_aware.cpp:1` | is_lifetime_aware est le trait le moins couvert |

---

## `is_sendable<T>`

### SEND-01 — Une reference rvalue T&& est traitee comme une valeur T : un membre `int&& r` est declare sendable

**CRITIQUE** · robustesse · `include/threadsafe/details/sendable.h:53`

La branche ligne 50 ne capture que les lvalue references. Pour une rvalue reference, la ligne 53 fait `type = remove_reference(type)` et la suite de la fonction traite le referent comme une VALEUR : scalaire -> oui, tableau -> element, classe -> walk des membres. Or `T&&` est une reference : elle designe un objet qui vit ailleurs (ex. `std::move(local)`), exactement comme `T&`. Consequence : un aggregat qui alias un objet via `&&` passe le trait, alors que le meme aggregat ecrit avec `&` est refuse (test_sendable.cpp:232, `HoldsRef`). Le trait dit OUI sur un type qui partage de la memoire mutable. Le trou est atteignable depuis l'API publique : `launchable_scoped_task` (qui n'exige que move_constructible + sendable) accepte l'aggregat, et `synchronized_value<T>` dont le constructeur fait `static_assert(sendable<T>)` l'accepte aussi.

> Défaut trouvé indépendamment par 3 auditeurs : *La regle « const derriere une indirection n'est jamais fait confiance » est contournee par le chemin `const T&&`*, *Le chemin `T&&` court-circuite la couche is_unsafe_sendable et remove_cv : `std::vector<int>&&` repond NON alors que `std::vector<int>` repond OUI*.

**Code actuel**

```cpp
if (is_reference_type(type) && !is_dynamic_type_known(remove_reference(type)))
  return false;

if (is_lvalue_reference_type(type))
  return is_synchronizable_type(remove_cv(remove_reference(type)));

type = remove_reference(type);

if (is_pointer_type(type))
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le correctif propose est correct et je l'ai compile tel quel. Dans include/threadsafe/details/sendable.h, remplacer les lignes 50-53 :

```cpp
if (is_lvalue_reference_type(type))
  return is_synchronizable_type(remove_cv(remove_reference(type)));

type = remove_reference(type);
```
par :

```cpp
if (is_reference_type(type))
  return is_synchronizable_type(remove_cv(remove_reference(type)));
```
La suite de diagnose_is_sendable ne voit alors plus jamais de reference, ce qui est a la fois plus court et plus sur.

Mises a jour de tests (exactement 4, verifiees par compilation, aucune autre) :
- tests/test_sendable.cpp:157 : `static_assert(is_sendable_v<const int &&>, "is_sendable rvalue ref");` devient `static_assert(!is_sendable_v<const int &&>, "is_sendable — const derriere une rvalue reference n'est pas plus fiable que derriere une lvalue reference");` — a fusionner avec la ligne 154 qui dit deja exactement cela pour `const int&`.
- tests/test_sendable.cpp:164 : `is_sendable_v<SyncType *&&>` devient `!is_sendable_v<SyncType *&&>`, message a reecrire en "une rvalue reference a un pointeur alias la variable pointeur elle-meme, que l'autre thread peut ecrire — meme reponse que SyncType*&". J'ai verifie par compilation que `is_sendable_v<SyncType*&>` est deja faux dans le code actuel : le message existant, qui affirme la parite avec `SyncType*`, est faux pour la forme reference.
- tests/test_sendable.cpp:278 : `static_assert(is_sendable_v<int &&>);` devient `static_assert(!is_sendable_v<int &&>, "is_sendable — une rvalue reference designe un objet qui vit ailleurs, exactement comme une lvalue reference");`.
- tests/test_polymorphic.cpp:74 : `is_sendable_v<PolyFinal&&>` devient `!is_sendable_v<PolyFinal&&>`; le commentaire de bloc ligne 70 ("Rvalue references: unlike passing by value, binding does not slice") doit etre reecrit, car sous le correctif la question du slicing ne se pose plus : une rvalue reference est traitee comme une reference, donc c'est is_synchronizable du referent qui decide, et PolyFinal non-const echoue avant meme la question du type dynamique.

A NE PAS toucher : tests/test_sendable.cpp:135-136, `static_assert(is_sendable_v<SyncType &&>, "is_sendable — an rvalue reference shares the referent too")`. Cette assertion enonce deja la regle visee et reste vraie sous le correctif (verifie).

A ajouter dans CLAUDE.md, section is_sendable, pour que la regle soit ecrite et non plus seulement implicite :
```cpp
- `is_sendable<T&>` = `is_sendable<T&&>` = `is_sendable<T*>` = `is_synchronizable<T>`
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p2.cpp` :

```text
struct HoldsLvalueRef { int& r; };
struct HoldsRvalueRef { int&& r; };
struct HoldsRvalueRefToArray { int (&&a)[4]; };
static_assert(!is_sendable_v<HoldsLvalueRef>);
static_assert(is_sendable_v<int&&>);
static_assert(is_sendable_v<HoldsRvalueRef>);
static_assert(is_sendable_v<int (&&)[4]>);
static_assert(is_sendable_v<HoldsRvalueRefToArray>);
```
g++-16 -std=c++26 -freflection -fsyntax-only -I include p2.cpp
=> compile sans erreur : TOUTES ces assertions sont vraies, donc l'aggregat qui alias un int via && est bien declare sendable.

Sonde `p3.cpp` (atteignabilite depuis l'API publique) :
```text
struct AliasLvalue { int& r; };  struct AliasRvalue { int&& r; };
static_assert(!launchable_scoped_task<decltype([](AliasLvalue){}), AliasLvalue>);
static_assert(launchable_scoped_task<decltype([](AliasRvalue){}), AliasRvalue>);
void build() { synchronized_value<AliasRvalue> sv{AliasRvalue{42}}; }
```
=> compile sans erreur.

Sonde `p15.cpp` : static_assert(is_sendable_v<std::array<int,4>&&>) et static_assert(!is_sendable_v<std::array<int,4>&>) compilent ensemble.

Verification du correctif (copie patchee du header dans .../audit_sendable/inc) : p4.cpp
```text
static_assert(!is_sendable_v<int&&>); static_assert(!is_sendable_v<AliasRvalue-like>);
static_assert(!is_sendable_v<int (&&)[4]>); static_assert(is_sendable_v<SyncType&&>);
static_assert(is_sendable_v<std::vector<int>>);
```
=> compile : le correctif ferme les trous sans toucher aux types valeur.

Contre-vérification indépendante :

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include p1.cpp && echo "P1 OK"
P1 OK

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include p2.cpp && echo "P2 OK"
P2 OK

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include p4.cpp && echo "OK"
OK

# --- correctif applique dans .../verify_rvalue_ref_hole/include, recompilation des 12 tests ---
$ for f in tests/test_*.cpp; do if g++-16 -std=c++26 -freflection -fsyntax-only -I ./include "$f" 2>/dev/null; then echo "OK: $(basename $f)"; else echo "CASSE: $(basename $f)"; fi; done
OK: test_asynchronous_task_launcher.cpp
OK: test_containers.cpp
OK: test_copy_on_write.cpp
OK: test_deferred_specialization.cpp
OK: test_diagnostics.cpp
OK: test_lifetime_aware.cpp
[…]

</details>

### SEND-02 — Les traits surs ne sont pas clos : is_sendable / is_synchronizable / is_lifetime_aware sont specialisables et la contrefacon traverse le walk

**MAJEUR** · flexibilité · `include/threadsafe/details/sendable.h:25`

CLAUDE.md affirme que les traits surs sont CLOS et n'ont aucune specialisation utilisateur. Rien dans le code ne le garantit : ce sont trois templates de classe ordinaires, donc explicitement specialisables, DANS LES DEUX SENS. Un 'template <> struct threadsafe::is_sendable<Raw> : std::true_type {};' force un OUI sur un type que le walk refuse, et ': std::false_type' force un NON sur un type prouve sur - ce que la couche is_unsafe_* est justement censee rendre impossible. Pire : la contrefacon n'est pas locale. La recursion passant par is_sendable_v (qui lit is_sendable<T>::value), le OUI forge se propage a tout type qui contient Raw comme membre, et le launcher accepte alors un callable qui emprunte une variable locale. Les memes trois lignes existent dans synchronizable_base.h:28 et lifetime_aware.h:27. Pour une conference, l'invariant central de la presentation tombe en une ligne posee par quelqu'un du public.

> Défaut trouvé indépendamment par 3 auditeurs : *is_sendable_v est aussi specialisable, et c'est lui que lisent la recursion reflective et le launcher*, *Les traits "surs" ne sont pas clos : is_sendable<T> et is_sendable_v<T> sont directement specialisables*.

**Code actuel**

```cpp
template <class T>
struct is_sendable : std::bool_constant<detail::diagnose_is_sendable(^^T)> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le correctif propose est bon mais s'arrete a mi-chemin : il verrouille `is_sendable<T>` (que personne ne lit) et laisse `is_sendable_v<T>` (que tout lit) specialisable — sonde P4, exploit intact. Il faut les deux, et il faut que la reponse faisant autorite vive dans `detail`. Variante compilee, 12/12 tests OK, P1/P3/P4 tous rejetes, semantique inchangee (P0) :

Dans sendable.h (idem pour les deux autres traits) :

```cpp
namespace detail {
consteval bool diagnose_is_sendable(std::meta::info type);

template <class T>
struct sendable_answer : std::bool_constant<diagnose_is_sendable(^^T)> {};

template <class T>
constexpr bool sendable_answer_v =
    assert_queryable_type<T>() && sendable_answer<T>::value;
} // namespace detail

template <class T> using is_sendable = detail::sendable_answer<T>;          // non specialisable
template <class T> constexpr bool is_sendable_v = detail::sendable_answer_v<T>;  // facade

template <class T> concept sendable = detail::sendable_answer_v<T>;         // lit la reponse, pas la facade

inline consteval bool is_sendable_type(std::meta::info type) {
  return detail::trait_value(^^detail::sendable_answer_v, type);
}
```
Puis router TOUS les consommateurs internes vers le nom `detail::*_answer_v`, sinon la facade reste exploitable :
```cpp
- asynchronous_task_launcher.h:18  is_sendable_v<T>            -> detail::sendable_answer_v<T>
- asynchronous_task_launcher.h:23  is_lifetime_aware_v<T>      -> detail::lifetime_aware_answer_v<T>
- synchronizable.h:13              is_sendable_v<T>            -> detail::sendable_answer_v<T>
- copy_on_write.h:48,52            is_sendable_v / is_synchronizable_v / is_lifetime_aware_v -> detail::*_answer_v
- smart_pointers.h:67,84           idem
- synchronized_value.h:45,90,94    idem
```
Apres ce routage, une specialisation de la facade `is_sendable_v` ne ment plus qu'a l'utilisateur qui l'a ecrite : elle ne traverse plus le walk, ni les concepts, ni le launcher.

Ce qui reste (et qu'il faut assumer plutot que pretendre le contraire sur le slide) : rien n'empeche d'ecrire `template <> struct threadsafe::detail::sendable_answer<X> : std::true_type {};`. La garantie obtenue n'est donc pas "impossible" mais "il faut plonger la main dans detail::" — ce qui est exactement la formulation honnete pour la conference, et c'est deja tout ce que valait la phrase "la seule porte est is_unsafe_*".

Alternative a considerer serieusement, vu la severite reelle : ne rien changer au code et corriger CLAUDE.md, qui affirme une cloture que le C++ ne donne pas. Les sondes P6/P7 montrent que le pouvoir d'accorder un OUI dangereux est deja offert, par design et par la porte documentee ; le seul pouvoir nouveau qu'apporte la forgerie est celui de forcer un NON, c'est-a-dire d'etre plus conservateur. Le code corrige ci-dessus coute ~6 lignes par trait plus un routage mecanique — il vaut le coup uniquement parce que la phrase est le message pedagogique central. Si l'auteur prefere garder les en-tetes tels quels, alors la phrase de CLAUDE.md doit devenir "les traits surs ne sont pas destines a etre specialises" au lieu de "sont CLOS".

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

SONDE p1_break_closure.cpp, compilee SANS erreur avec les en-tetes actuels (donc la cloture est fausse) :

```text
struct Raw { int* borrowed; };
struct SafeBox { int value; };
template <> struct threadsafe::is_sendable<Raw> : std::true_type {};
template <> struct threadsafe::is_sendable<SafeBox> : std::false_type {};
static_assert(threadsafe::is_sendable_v<Raw>);
static_assert(!threadsafe::is_sendable_v<SafeBox>);
struct HolderOfRaw { Raw member; };
static_assert(threadsafe::is_sendable_v<HolderOfRaw>);   // le OUI forge traverse le walk
struct HolderOfSafe { SafeBox member; };
static_assert(!threadsafe::is_sendable_v<HolderOfSafe>); // le NON forge aussi
```
Sortie : 'P1 OK: closure broken' (0 erreur).

Meme resultat pour les deux autres traits (p2_break_all3.cpp) : is_synchronizable<const Raw> et is_lifetime_aware<Raw> forces a true, 0 erreur.

VERIFICATION DU CORRECTIF : j'ai copie include/ dans un scratchpad, applique l'alias ci-dessus, puis :
- p1_break_closure.cpp echoue desormais : "error: invalid class name in declaration of 'using threadsafe::is_sendable = struct threadsafe::detail::sendable_answer<Raw>'"
- les 12 fichiers de tests/*.cpp compilent tous sans erreur avec les en-tetes patches.

Contre-vérification indépendante :

$ g++-16 -std=c++26 -freflection -fsyntax-only -I <repo>/include p1_break_closure.cpp
P1 OK: closure broken                 (0 erreur — la cloture est bien fausse)

$ ... p0_baseline.cpp
P0 OK: baseline confirms walk answers no on Raw   (0 erreur — la forgerie inverse donc reellement)

$ ... p2_break_all3.cpp        -> P2 OK: all3 broken
$ ... p2b_baseline.cpp         -> P2B OK: baseline no

$ ... p3_launcher.cpp
P3 EXPLOIT OK: launcher accepte le callable qui emprunte une locale
$ ... p3b_baseline.cpp         -> P3B OK: sans la forgerie, les deux traits disent non

$ ... p5_control.cpp
<repo>/include/threadsafe/details/asynchronous_task_launcher.h:62:23: error:
```text
static assertion failed: the callable must be movable, sendable and lifetime-aware
```
(controle: sans forgerie le launcher refuse)

=== VERIFICATION DU CORRECTIF PROPOSE (copie patchee, alias) ===
[…]

</details>

### SEND-03 — L'invariant documente `is_sendable<T*> == is_synchronizable<T>` est faux pour `T = const U`

**MINEUR** · API · `include/threadsafe/details/sendable.h:56`

CLAUDE.md affirme `is_sendable<T&> = is_sendable<T*> = is_synchronizable<T>`. Le detour `add_lvalue_reference(remove_pointer(type))` amene a la branche lvalue (ligne 51) qui applique `remove_cv` sur le referent — volontairement, pour implementer « const derriere une indirection n'est jamais fait confiance ». L'egalite annoncee est donc fausse des que T porte un const : `is_synchronizable_v<const int>` vaut true tandis que `is_sendable_v<const int*>` vaut false. L'egalite reelle est `is_sendable<T*> == is_synchronizable<remove_cv_t<T>>`. Ce n'est pas un trou de soundness (le comportement est le bon, conservateur), mais l'enonce publie dans la documentation d'un support de conference est incorrect et contredit la regle voisine enoncee trois lignes plus bas dans le meme fichier.

**Code actuel**

```cpp
if (is_pointer_type(type))
  return is_sendable_type(add_lvalue_reference(remove_pointer(type)));
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Toujours aucune correction de code : le walk est correct, seule la documentation ment. Mais la correction proposee par l'auditeur (`is_synchronizable<std::remove_cv_t<T>>`) reste fausse pour un type polymorphe non-`final` vouche (probe2.cpp ci-dessus). Il manque la condition de type dynamique connu, imposee par `sendable.h:47`.

Remplacer `CLAUDE.md:98` :

```
- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<T>`
```

par :

```
- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<std::remove_cv_t<T>>`,
  et seulement si le type dynamique de `T` est connu (non polymorphe, ou `final`).
```

Les deux clauses rendent visibles, dans la formule elle-meme, les deux gardes que
le code applique et que l'enonce actuel passe sous silence :

- `remove_cv_t` materialise « const derriere une indirection n'est jamais fait
  confiance » (sendable.h:51, `remove_cv(remove_reference(type))`) ;
- « type dynamique connu » materialise le rejet de sendable.h:47
  (`is_reference_type(type) && !is_dynamic_type_known(remove_reference(type))`),
  qui coupe AVANT la branche lvalue et rend donc la premiere clause seule
  insuffisante.

Variante en prose, sans doute meilleure pour une audience de conference que la
chaine d'egalites — elle dit la meme chose et se lit dans le sens du walk :

```
- Une indirection vers `T` (`T&` ou `T*`) est sendable quand `T`, **const retire**,
  est synchronizable — et que le type dynamique de `T` est connu.
```

Predicat valide par compilation sur 25 types (probe3.cpp), incluant cv, tableaux,
fonctions, pointeurs multi-niveaux, polymorphes vouches et non vouches, `final`,
`std::vector`, `std::atomic`, `std::unique_ptr`.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p10.cpp` (extrait) :

```text
static_assert(is_synchronizable_v<const int>);
static_assert(!is_sendable_v<const int*>);
```
g++-16 -std=c++26 -freflection -fsyntax-only -I include p10.cpp
=> compile sans erreur : les deux cotes de l'egalite annoncee different.

(La meme sonde confirme par ailleurs que les axes sains le sont bien : unions, unions anonymes, bitfields, [[no_unique_address]], bases virtuelles, heritage multiple, lambdas capturantes/generiques/mutables, operator() template, conversion template, volatile, pointeurs de membre, pointeurs multi-niveaux, tableaux multidimensionnels et de bornes inconnues — sondes p5.cpp, p6.cpp, p10.cpp, toutes compilees sans erreur.)

</details>

## `is_synchronizable<T>` et `is_synchronizable<const T>`

### SYNC-01 — std::atomic_flag n'est pas vouché alors que std::atomic l'est : l'idiome « mutable flag » est rejeté

**MINEUR** · flexibilité · `include/threadsafe/details/synchronizable.h:11`

Seul `std::atomic<T>` est vouché. `std::atomic_flag` — la primitive lock-free canonique — ne l'est pas. Le walk structurel lui répond non en forme non const (`!is_const -> false`), donc : `is_synchronizable_v<std::atomic_flag>` est faux, `is_sendable_v<std::atomic_flag&>` est faux, et surtout un membre `mutable std::atomic_flag` rend la classe englobante non synchronizable, alors que le même idiome avec `mutable std::atomic<int>` passe (c'est exactement le `SafeCounter` de tests/test_synchronizable.cpp:17). L'incohérence est d'autant plus visible que `const std::atomic_flag` répond vrai, lui, mais par accident : le walk tombe sur le `unsigned char` de `__atomic_flag_base`, pas sur une connaissance de l'atomicité.

**Code actuel**

```cpp
template <class T>
struct is_unsafe_synchronizable<std::atomic<T>>
    : std::bool_constant<is_sendable_v<T>> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le correctif proposé est correct et suffisant tel quel — je l'ai compilé et passé les 12 tests. Je l'ajuste seulement sur deux points de finition propres à ce repo (tests = static_assert, donc un vouch non testé n'existe pas).

1) include/threadsafe/details/synchronizable.h — après la spécialisation de std::atomic :

```cpp
template <class T>
struct is_unsafe_synchronizable<std::atomic<T>>
    : std::bool_constant<is_sendable_v<T>> {};

template <>
struct is_unsafe_synchronizable<std::atomic_flag> : std::true_type {};
```

std::atomic_flag n'a pas de paramètre de type : aucune condition à propager, d'où le std::true_type inconditionnel (et non un bool_constant). La spécialisation `is_unsafe_synchronizable<const T>` de synchronizable_base.h:14 couvre automatiquement `const std::atomic_flag`. `<atomic>` est déjà inclus ligne 3 : rien d'autre à ajouter.

2) tests/test_synchronizable.cpp — ancrer le vouch, en miroir exact du SafeCounter existant (ligne 17) :

```cpp
struct SafeFlag {
    mutable std::atomic_flag raised;
};
```
puis, à côté des assertions sur SafeCounter :
```cpp
static_assert(threadsafe::is_synchronizable_v<std::atomic_flag>);
static_assert(threadsafe::is_synchronizable_v<const SafeFlag>);
```
(vérifié : compile.)

3) CLAUDE.md — la section « is_unsafe_<trait> » énumère les types vouchés (« std::vector, std::unique_ptr, std::atomic, synchronized_value, copy_on_write »). Écrire « std::atomic et std::atomic_flag » pour que la liste documentée reste le reflet exact du code — c'est précisément le genre d'écart qui se voit en conférence.

Ne PAS ajouter de vouch is_unsafe_sendable ni is_unsafe_lifetime_aware : le walk répond déjà vrai sur les deux pour std::atomic_flag (vérifié dans p1.cpp et final.cpp), un vouch supplémentaire serait de la confiance accordée sans nécessité.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sondes .../audit_sync/p5.cpp et p7.cpp : `!is_synchronizable_v<std::atomic_flag>`, `!is_sendable_v<std::atomic_flag&>`, `is_synchronizable_v<const std::atomic_flag>`, `is_synchronizable_v<const MutableAtomic>` (mutable std::atomic<int>) mais `!is_synchronizable_v<const MutableFlag>` (mutable std::atomic_flag) — tout compile sans erreur.

Correctif validé : .../audit_sync/fix1.cpp avec la spécialisation ci-dessus compile `is_synchronizable_v<std::atomic_flag>`, `is_synchronizable_v<const MutableFlag>` et `is_sendable_v<std::atomic_flag&>`.

</details>

### SYNC-02 — is_unsafe_synchronizable<T> sans const accorde silencieusement le partage MUTABLE concurrent

**MINEUR** · API · `include/threadsafe/details/synchronizable_base.h:14`

Pour voucher un type maison il faut ecrire trois specialisations, et celle de synchronizable exige la forme const - CLAUDE.md le mentionne ('la bonne forme const') mais rien ne la fait respecter. Or les deux formes ne sont pas equivalentes en force, elles sont ordonnees : grace a 'is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T>', la forme SANS const (celle qu'un utilisateur ecrit naturellement, par symetrie avec is_unsafe_sendable<T> et is_unsafe_lifetime_aware<T>) accorde les DEUX reponses. Un utilisateur qui voulait dire 'mon handle est sur a LIRE depuis plusieurs threads' obtient 'mon handle est sur a MUTER depuis plusieurs threads' : is_synchronizable_v<T> devient vrai, donc is_sendable_v<T&> et is_sendable_v<T*> aussi (sendable.h:51/56), et synchronized_value<T> passe au shared_mutex. L'erreur est d'autant plus facile a commettre que la forme correcte est la seule des trois a porter un const, et qu'aucun diagnostic ne distingue les deux.

**Code actuel**

```cpp
template <class T> struct is_unsafe_synchronizable : std::false_type {};

template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Correction documentaire uniquement — aucun changement de header (l'heritage
`is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T>` est correct et
doit rester: `std::atomic<T>` (synchronizable.h:12), `synchronized_value<T>`
(synchronized_value.h:89) et `std::default_delete<T>` (smart_pointers.h:62) sont
genuinement synchronizables en mutation).

Ajouter dans CLAUDE.md, a la fin de la section "### `is_unsafe_<trait>` — the one
customization point, opt-in only", le bloc suivant (j'ai retire l'affirmation
FAUSSE du finding sur le shared_mutex, refutee par la sonde p2, et ajoute le
precedent interne qui rend la regle verifiable) :

    #### Voucher un type maison : la forme `const` n'est pas un detail

    Les trois claims sont independants — envoyer / partager / posseder — et un
    type peut n'en meriter qu'un ou deux. Le cas courant d'un handle RAII est
    « sendable et proprietaire, mais partageable en LECTURE seulement » :

    ```cpp
    template <> struct threadsafe::is_unsafe_sendable<FileHandle> : std::true_type {};
    template <> struct threadsafe::is_unsafe_synchronizable<const FileHandle> : std::true_type {};
    template <> struct threadsafe::is_unsafe_lifetime_aware<FileHandle> : std::true_type {};
    ```

    Les deux formes de la claim `synchronizable` ne sont pas symetriques, elles
    sont **ordonnees** : `is_unsafe_synchronizable<const T>` accorde la lecture
    concurrente ; `is_unsafe_synchronizable<T>` (sans const) accorde en plus la
    **mutation** concurrente — l'heritage `is_unsafe_synchronizable<const T> :
    is_unsafe_synchronizable<T>` fait descendre la seconde sur la premiere. Omettre
    le `const` rend donc aussi `T&` et `T*` sendable, c'est-a-dire autorise le
    launcher a faire traverser une reference mutable. Ne l'ecrivez sans `const`
    que si toute mutation de `T` est deja serialisee en interne.

    La bibliotheque applique cette regle sur elle-meme : forme `const` pour
    `std::allocator`, `std::stop_token`, `unique_ptr`, `shared_ptr`, `weak_ptr`,
    `reference_wrapper` et les conteneurs ; forme sans `const` seulement pour
    `std::atomic`, `synchronized_value` et `std::default_delete`.

CHALLENGE MAINTENU (je suis d'accord avec le finding sur ce point) : ne PAS ajouter
de macro `THREADSAFE_VOUCH(T)` ni d'`unsafe_everything<T>`. Le cout mesure est de
3 lignes (sonde p3), et ces 3 lignes sont exactement le contenu pedagogique de
l'expose. Une macro qui accorde les trois d'un coup encourage le sur-vouch et
masque precisement le seul detail qui merite d'etre vu : le `const`.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
SONDE p18_constform.cpp, compilee SANS erreur avec les en-tetes actuels (donc tous ses static_assert sont vrais) :

  struct ReadOnlyHandle { void* raw; };
  struct MutableHandle  { void* raw; };
  template <> struct threadsafe::is_unsafe_synchronizable<const ReadOnlyHandle> : std::true_type {};
  template <> struct threadsafe::is_unsafe_synchronizable<MutableHandle> : std::true_type {};

  static_assert(threadsafe::is_synchronizable_v<const ReadOnlyHandle>);
  static_assert(!threadsafe::is_synchronizable_v<ReadOnlyHandle>);
  static_assert(!threadsafe::is_sendable_v<ReadOnlyHandle&>);  // forme const : OK

  static_assert(threadsafe::is_synchronizable_v<const MutableHandle>);
  static_assert(threadsafe::is_synchronizable_v<MutableHandle>);
  static_assert(threadsafe::is_sendable_v<MutableHandle&>);    // forme sans const : mutation concurrente accordee
  static_assert(threadsafe::is_sendable_v<MutableHandle*>);

Sonde `p8_vouch_cost.cpp` : le vouch complet d'un FileHandle RAII enveloppant un FILE* coute exactement 3 lignes, compilees sans erreur.
```

</details>

### SYNC-03 — Aucun concept `synchronizable`, alors que `sendable` et `lifetime_aware` existent

**MINEUR** · API · `include/threadsafe/details/synchronizable_base.h:33`

Les trois traits sont presentes comme symetriques dans CLAUDE.md, mais seuls deux exposent un concept. L'utilisateur qui ecrit `template <threadsafe::synchronizable T>` obtient une erreur de nom. Il doit deviner l'asymetrie et retomber sur `requires threadsafe::is_synchronizable_v<T>`. Sur une slide qui aligne les trois traits, l'absence saute aux yeux.

**Code actuel**

```cpp
template <class T>
constexpr bool is_synchronizable_v =
    detail::assert_queryable_type<T>() && is_synchronizable<T>::value;

inline consteval bool is_synchronizable_type(std::meta::info type) {
```

**Correction proposée**

template <class T>
constexpr bool is_synchronizable_v =
```cpp
detail::assert_queryable_type<T>() && is_synchronizable<T>::value;
```
template <class T>
concept synchronizable = is_synchronizable_v<T>;

inline consteval bool is_synchronizable_type(std::meta::info type) {

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

h_surface.cpp:
```text
template <threadsafe::sendable T> struct A {};        // OK
template <threadsafe::lifetime_aware T> struct B {};  // OK
template <threadsafe::synchronizable T> struct C {};  // ERREUR
```
Sortie g++-16:
```text
h_surface.cpp:7:11: error: 'threadsafe::synchronizable' has not been declared; did you mean 'threadsafe::is_synchronizable'?
```

</details>

### SYNC-04 — Un membre de donnee STATIQUE mute par une methode const ne fait pas echouer is_synchronizable<const T>, sans test ni note

**MINEUR** · robustesse · `include/threadsafe/details/synchronizable_base.h:71`

Le walk const n'inspecte que nonstatic_data_members_of. Une classe dont une methode const incremente un `static int` non atomique est declaree synchronizable<const T> : deux lecteurs concurrents courent une data race que le trait a pourtant affirme absente. CLAUDE.md revendique "le walk est CONSERVATEUR : tout ce qu'il ne peut pas prouver est un non" — ici il prouve un oui qu'il ne peut pas prouver. La limite est en partie irreductible (une methode const peut ecrire n'importe quel global), mais le cas du membre statique est, lui, visible par reflection, et surtout il n'existe ni test ni ligne de documentation qui le mentionne — pour du materiel de conference, c'est la question que le public posera.

**Code actuel**

```cpp
for (auto member : nonstatic_data_members_of(type, context)) {
  const auto member_type = type_of(member);

  if (is_mutable_member(member)) {
    if (!is_synchronizable_type(member_type))
      return false;
  } else if (is_reference_type(member_type)) {
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Retenir UNIQUEMENT l'option (a) — l'option (b) est mesuree comme nuisible (voir probe_b2 : elle rejette `inline static std::atomic<int>` et un compteur statique prive inutilise, deux types parfaitement surs) et ne doit pas etre appliquee.

1. Ajouter dans tests/test_synchronizable.cpp (et/ou test_sendable.cpp) le cas qui pinne l'etat de l'art — verifie compilant contre les headers actuels :

  namespace {
  struct StaticCounter {
    int payload;
    static int call_count;
    void touch() const { ++call_count; }
  };
  int StaticCounter::call_count = 0;
  }

  static_assert(is_synchronizable_v<const StaticCounter>,
                "is_synchronizable - un membre de donnee statique n'est pas de "
                "l'etat par objet : le trait parle de l'objet, pas de ce que ses "
                "methodes const touchent ailleurs");
  static_assert(is_sendable_v<StaticCounter>,
                "is_sendable - meme limite : l'etat partage hors de l'objet "
                "(statique ou global) est hors de portee du walk");

2. Ajouter une ligne dans CLAUDE.md, sous "### `is_synchronizable<const T>`", qui qualifie la revendication de conservatisme :

  Le walk repond sur l'**etat de l'objet**, pas sur ce que ses methodes touchent
  ailleurs : un membre de donnee statique, comme n'importe quel global, n'est pas
  de l'etat par objet et n'est pas inspecte. `mutable` est verifie parce que c'est
  de l'etat par objet mute par une methode const ; un `static` ne l'est pas.

Cette formulation est preferable a un simple "limite connue" : elle donne le critere (etat par objet) qui explique d'un coup pourquoi `mutable` est verifie et pourquoi `static` ne l'est pas — c'est exactement la question que posera le public.
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p05_adversarial.cpp` contre les headers actuels :
```text
p05_adversarial.cpp:37:15: error: static assertion failed: PROBE-STATIC: a const method mutating a static races between readers
```
La sonde attendait !is_synchronizable_v<const StaticCounter> ; le trait repond donc TRUE. Toutes les autres sondes du meme fichier (union anonyme, champs de bits, losange virtuel, base privee, membre volatile) passent : le walk est correct partout ailleurs.

</details>

### SYNC-05 — volatile fait perdre les vouches écrites à la main (atomic, shared_ptr) mais pas celles des wrappers std

**DÉTAIL** · robustesse · `include/threadsafe/details/synchronizable_base.h:13`

La spécialisation de transfert `is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T>` déduit `T = volatile X` pour `const volatile X` ; elle ne retombe donc jamais sur une spécialisation écrite pour `X` ou pour `const X`. Le vouch de `std::atomic<T>` et celui de `const std::shared_ptr<T>` sont perdus sous volatile, alors que la règle des wrappers std survit (son contrainte `std_wrapper` est évaluée par réflexion, qui ignore le cv). Le résultat est un faux négatif — donc sûr — mais l'incohérence entre deux familles de vouches est contraire à l'invariant annoncé « un vouch ne peut qu'accorder ».

**Code actuel**

```cpp
template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee est incorrecte (verifie par compilation : elle repare atomic mais pas shared_ptr). Correctif qui marche, dans include/threadsafe/details/synchronizable_base.h, a ajouter apres la ligne 14 :

```cpp
template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};

template <class T>
struct is_unsafe_synchronizable<const volatile T>
    : is_unsafe_synchronizable<const T> {};
```

`<const volatile T>` est plus specialise que `<const T>` et que la version contrainte `<const T> requires std_wrapper<T>`, donc aucune ambiguite. En delegant a `<const T>` (et non a `T` nu), on atteint les vouches ecrits sous forme `<const std::shared_ptr<T>>` comme ceux ecrits sous forme `<std::atomic<T>>` (ces derniers via le transfert `<const T>` deja present).

Verifie : les deux vouches survivent a volatile, le vouch vector continue de marcher, aucun faux positif introduit, et les 12 tests existants compilent.

Alternative recommandee vu la vocation educative : ne rien changer au code et ecrire noir sur blanc dans CLAUDE.md que volatile sur un type classe ne porte pas les vouches, plutot que de laisser le comportement dependre de la forme (`<X>` vs `<const X>`) dans laquelle chaque vouch a ete redige.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p2b.cpp` : `is_unsafe_synchronizable_v<const std::atomic<int>>` vrai mais `is_unsafe_synchronizable_v<const volatile std::atomic<int>>` FAUX (static assertion failed: E?), idem pour `const volatile std::shared_ptr<std::atomic<int>>` (G?), tandis que `is_synchronizable_v<const volatile std::vector<int>>` reste vrai (p2.cpp, assertion « B: volatile kills the vouch » en échec). Le correctif proposé n'a PAS été compilé.

</details>

## `is_lifetime_aware<T>`

### LIFE-01 — is_smart_pointer est une porte derobee non marquee "unsafe" qui accorde is_lifetime_aware a n'importe quel emprunt

**CRITIQUE** · API · `include/threadsafe/details/smart_pointers.h:51`

CLAUDE.md affirme que `is_unsafe_<trait>` est "the one customization point" et que les traits surs sont clos. C'est faux: `is_smart_pointer` (ligne 30) est un trait ouvert, specialisable par l'utilisateur, sans le mot `unsafe` dans son nom, et la specialisation groupee `template <smart_pointer T> struct is_unsafe_lifetime_aware<T>` en fait un second point de personnalisation qui ACCORDE la confiance lifetime_aware. Le verdict ne regarde que les arguments de template du type: un type qui n'appartient rien du tout devient lifetime_aware des qu'il s'inscrit dans is_smart_pointer. Consequence concrete et prouvee: un emprunt brut traverse `launch_task` et le thread detache ecrit dans un objet mort.

> Défaut trouvé indépendamment par 3 auditeurs : *is_smart_pointer est un second point de personnalisation public, non documente, qui accorde la confiance sans le mot "unsafe"*, *is_smart_pointer est un point d'extension public non documente, et il rend l'instanciation de is_unsafe_lifetime_aware ambigue pour tout type deja std_wrapper*.

**Code actuel**

```cpp
template <smart_pointer T>
struct is_unsafe_lifetime_aware<T>
    : std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Identique a la correction proposee, a une regression pres qu'elle introduisait : garder `detail::pointee_answer` sur le deleter `D` au lieu de `is_lifetime_aware_v<D>`, sinon le garde `is_dynamic_type_known` disparait pour un deleter polymorphe non-final (prouve par deleter_guard.cpp ci-dessus).

Dans include/threadsafe/details/smart_pointers.h :

(a) supprimer `detail::pointee_is_lifetime_aware` (lignes 15-21), qui n'a plus d'appelant ;
(b) remplacer la specialisation groupee (lignes 51-53) par les trois vouchs explicites :

template <class T, class D>
struct is_unsafe_lifetime_aware<std::unique_ptr<T, D>>
```cpp
: std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type) &&
                     detail::pointee_answer(^^D, is_lifetime_aware_type)> {};
```
template <class T>
struct is_unsafe_lifetime_aware<std::shared_ptr<T>>
```cpp
: std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type)> {};
```
template <class T>
struct is_unsafe_lifetime_aware<std::weak_ptr<T>>
```cpp
: std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type)> {};
```
Verifie : fix_check.cpp EXIT=0 et les 12 tests EXIT=0.

Deux nettoyages qui decoulent du (b) et que l'auditeur n'a pas releves :

- `is_smart_pointer_type` (lignes 47-49) et le concept `smart_pointer` (44-45) n'ont alors plus AUCUN appelant dans la bibliotheque (le grep le montre : ils n'en avaient deja pas d'autre). Soit les supprimer avec le reste, soit assumer `is_smart_pointer` comme un predicat purement descriptif — mais il ne doit alors plus rien accorder, ce qui est exactement l'effet du (b).
- CLAUDE.md : une fois le (b) applique, les phrases « `is_unsafe_<trait>` — the one customization point » et « The word `unsafe` appears wherever knowledge is asserted instead of proved » redeviennent vraies. Sans le (b), ce sont les deux phrases a corriger, et c'est le pire des deux choix pour du code de conference.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Sonde `s3.cpp`, compilee SANS ERREUR (EXIT=0) avec `g++-16 -std=c++26 -freflection -fsyntax-only -I include`:

  #include <threadsafe/threadsafe.h>
  #include <atomic>
  template <class T> struct BorrowPtr { T *borrowed; T &operator*() const { return *borrowed; } };
  template <class T> struct threadsafe::is_smart_pointer<BorrowPtr<T>> : std::true_type {};
  static_assert(threadsafe::is_lifetime_aware_v<BorrowPtr<std::atomic<int>>>);
  static_assert(threadsafe::is_sendable_v<BorrowPtr<std::atomic<int>>>);
  static_assert(threadsafe::is_task_participant_v<BorrowPtr<std::atomic<int>>>);
  void dangle() {
    threadsafe::asynchronous_task_launcher launcher;
    { std::atomic<int> local{0};
      launcher.launch_task([](BorrowPtr<std::atomic<int>> p) { ++*p; },
                           BorrowPtr<std::atomic<int>>{&local}); }
  }

Sortie: aucune, EXIT=0. Le launcher accepte l'emprunt.

Verification du correctif: headers copies dans scratchpad/audit_lifetime/inc2 avec le patch ci-dessus. fix_check.cpp compile EXIT=0 avec `!is_lifetime_aware_v<BorrowPtr<std::atomic<int>>>` et `!is_task_participant_v<...>`, et les 12 fichiers de tests/*.cpp compilent tous OK contre inc2.
```

Contre-vérification indépendante :

$ g++-16 -std=c++26 -freflection -fsyntax-only -I include <sonde>.cpp
s3 EXIT=0                  (sonde de l'auditeur reproduite a l'identique, aucune sortie)
baseline EXIT=0            (!is_lifetime_aware_v && !is_task_participant_v sans la registration)
truthful EXIT=0            (is_lifetime_aware_v && is_task_participant_v AVEC registration VRAIE)
truthful_baseline EXIT=0   (!is_lifetime_aware_v && !is_task_participant_v sans registration)

Divergence du fix propose (deleter polymorphe non-final) :
--- contre upstream ---                          EXIT=0   (refuse, garde is_dynamic_type_known actif)
--- contre mon patch (pointee_answer sur D) ---  EXIT=0   (refuse, garde preserve)
--- fix tel que propose (is_lifetime_aware_v<D>) ---
EXIT=1
deleter_guard.cpp:9:15: error: static assertion failed

[…]

</details>

### LIFE-02 — Un type recursif possedant (Node contenant unique_ptr<Node> ou vector<Node>) fait echouer tout trait avec une cascade d'erreurs illisible

**MAJEUR** · robustesse · `include/threadsafe/details/lifetime_aware.h:32`

Le memo `_v` ne casse pas les cycles: interroger un trait sur un type qui se contient indirectement (via un conteneur ou un smart pointer possedant) reentre dans la variable template en cours d'instanciation. GCC n'en fait ni un "non" ni un diagnostic lisible: il emet `'value' is not a member of threadsafe::is_lifetime_aware<Node>` puis `non-constant condition for static assertion`. C'est la forme la plus banale du C++ (arbre, liste chainee possedante) et elle est inutilisable avec la bibliotheque. Le probleme est commun aux trois walks (is_sendable et is_synchronizable echouent identiquement), il n'y a pas de hang (pas de boucle infinie du compilateur), mais le message ne mentionne jamais la recursion.

> Défaut trouvé indépendamment par 2 auditeurs : *Un type recursif (Tree contenant std::vector<Tree>) rend les trois traits ininterrogeables*.

**Code actuel**

```cpp
template <class T>
constexpr bool is_lifetime_aware_v =
    detail::assert_queryable_type<T>() && is_lifetime_aware<T>::value;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Le diagnostic du finding est bon, sa correction ne l'est pas. Trois corrections a apporter.

(a) L'echappatoire a documenter est le vouch du WRAPPER, pas du noeud. Le vouch du noeud desactive tout le walk (faux positif prouve) ; le vouch du wrapper casse le cycle exactement la ou il se produit et laisse le walk inspecter les autres membres :

  // NE PAS FAIRE : desactive le walk entier de Tree
  // template <> struct threadsafe::is_unsafe_lifetime_aware<Tree> : std::true_type {};

  // FAIRE : ne vouche que l'arete qui boucle
  struct Tree;
  template <>
  struct threadsafe::is_unsafe_lifetime_aware<std::unique_ptr<Tree>>
      : std::true_type {};
  struct Tree { std::unique_ptr<Tree> left; int payload; };
  static_assert(threadsafe::is_lifetime_aware_v<Tree>);          // OK, compile

  // et le walk voit toujours le reste :
  struct Tree2;
  template <>
  struct threadsafe::is_unsafe_lifetime_aware<std::unique_ptr<Tree2>>
      : std::true_type {};
  struct Tree2 { std::unique_ptr<Tree2> left; int &borrowed; };
  static_assert(!threadsafe::is_lifetime_aware_v<Tree2>);        // OK, compile

Idem pour les conteneurs std et pour is_sendable :
  struct VecNode;
  template <>
  struct threadsafe::is_unsafe_sendable<std::vector<VecNode>> : std::true_type {};
  struct VecNode { std::vector<VecNode> kids; int *borrowed; };
  static_assert(!threadsafe::is_sendable_v<VecNode>);            // OK, compile

(b) Corriger les deux commentaires de test qui affirment le contraire de la realite. tests/test_smart_pointers.cpp:121-123 ("un auto-pointeur possedant ... termine la recursion") et tests/test_copy_on_write.cpp:103-105 ("les types auto-referents repondent, ils ne recursent pas a l'infini") ne sont vrais que pour les combinaisons ou le vouch change de question (unique_ptr -> is_synchronizable_v<T> non-const, shared_ptr -> is_synchronizable_v<T>). Ils sont faux pour is_lifetime_aware_v<Tree>, is_sendable_v<Tree> et les trois traits sur vector<VecNode>. Reformuler en nommant la condition reelle : la recursion ne termine que lorsque le vouch repose une question DIFFERENTE de celle en cours.

(c) Ajouter dans CLAUDE.md, a cote de "la specialisation doit etre ecrite avant la premiere question", un paragraphe nommant la limite : un type qui se contient a travers un wrapper qui forwarde le MEME trait (vector<Node>, unique_ptr<Node> pour sendable/lifetime_aware, shared_ptr<Node> pour lifetime_aware) reentre dans la variable template en cours d'instanciation ; GCC n'emet ni oui, ni non, mais une cascade "'value' is not a member of". Donner l'echappatoire (a) comme recette, et ajouter un tests/test_recursive_types.cpp reprenant les quatre static_assert ci-dessus.

Il n'existe effectivement pas de correction locale dans le code : couper le cycle demanderait de faire circuler un ensemble de types "en cours de visite" a travers le walk, or le point d'entree est justement la variable template `_v` nullaire (c'est elle qui porte le memo ET le point de personnalisation utilisateur), qui ne peut pas etre parametree par cet ensemble. Sur ce point precis le finding a raison.
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Quatre sondes, chacune compilee avec un timeout de 60s (aucune n'a atteint le timeout):

```text
r1.cpp: struct Node { std::vector<Node> children; };  is_lifetime_aware_v<Node>
r3.cpp: struct Node { std::unique_ptr<Node> next; };  is_lifetime_aware_v<Node>
r4.cpp: struct Node { std::shared_ptr<Node> next; };  is_lifetime_aware_v<Node>
r5.cpp: struct Node { std::vector<Node> children; };  is_sendable_v<Node>
```
Toutes EXIT=1 avec, par exemple pour r3:
```text
utils.h:9:23: error: the value of 'threadsafe::is_lifetime_aware_v<Node>' is not usable in a constant expression
lifetime_aware.h:16:74: error: 'value' is not a member of 'threadsafe::is_unsafe_lifetime_aware<std::unique_ptr<Node> >'
lifetime_aware.h:32:65: error: 'value' is not a member of 'threadsafe::is_lifetime_aware<Node>'
r3.cpp:4:27: error: non-constant condition for static assertion
```
r2.cpp (struct Node { Node *next; }) compile EXIT=0: le pointeur brut est rejete ligne 53 avant toute recursion.
r6.cpp (le vouch anticipe ci-dessus) compile EXIT=0.

Contre-vérification indépendante :

=== reproduction des sondes annoncees ===
r1  EXIT=1
r2  EXIT=0
r3  EXIT=1
r4  EXIT=1
r5  EXIT=1
r6  EXIT=0

erreurs de r3.log (164 lignes au total pour un source de 4 lignes) :
```text
:80  utils.h:9:23: error: the value of 'threadsafe::is_lifetime_aware_v<Node>' is not usable in a constant expression
:127 lifetime_aware.h:16:74: error: 'value' is not a member of 'threadsafe::is_unsafe_lifetime_aware<std::unique_ptr<Node> >'
:152 lifetime_aware.h:32:65: error: 'value' is not a member of 'threadsafe::is_lifetime_aware<std::unique_ptr<Node> >'
:159 lifetime_aware.h:32:65: error: 'value' is not a member of 'threadsafe::is_lifetime_aware<Node>'
:162 r3.cpp:4:27: error: non-constant condition for static assertion
```
erreurs de r1.log (vector) : memes 5 erreurs, avec std::vector<Node> a la place de std::unique_ptr<Node>.
[…]

</details>

### LIFE-03 — pointee_is_lifetime_aware applique is_dynamic_type_known au deleter de unique_ptr, qui est pourtant stocke par valeur

**MINEUR** · robustesse · `include/threadsafe/details/smart_pointers.h:15`

`wrapped_types_of(^^T)` remonte TOUS les arguments de template qui sont des types. Pour `std::unique_ptr<T, D>` cela inclut le deleter D, auquel `pointee_answer` applique `is_dynamic_type_known` (utils.h:29). Or ce garde-fou existe pour les types atteints DERRIERE une indirection (on ne connait pas le type dynamique d'un pointee). Le deleter, lui, est un sous-objet par valeur de l'unique_ptr: son type dynamique est exactement son type statique, il n'y a rien a garder. Resultat: un deleter polymorphe non-final rend l'unique_ptr non lifetime_aware alors qu'il possede parfaitement son pointee et son deleter. Faux negatif, sans danger pour la surete, mais incoherent avec `is_unsafe_sendable<std::unique_ptr<T, D>>` (ligne 64) qui, lui, demande correctement `is_sendable_v<D>` sans is_dynamic_type_known.

**Code actuel**

```cpp
template <class T> consteval bool pointee_is_lifetime_aware() {
  const auto template_arguments = wrapped_types_of(^^T);

  return std::ranges::all_of(template_arguments, [](const auto argument) {
    return pointee_answer(argument, is_lifetime_aware_type);
  });
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le correctif propose par l'auditeur est bon sur le fond mais incomplet : en supprimant `pointee_is_lifetime_aware` sans remplacer le vouch generique `template <smart_pointer T> struct is_unsafe_lifetime_aware<T>` (smart_pointers.h:51-53), `shared_ptr` et `weak_ptr` perdent leur vouch. Voici la version complete, que j'ai appliquee et compilee (fix_check.cpp EXIT=0, 12/12 tests OK).

Dans include/threadsafe/details/smart_pointers.h :

1) Supprimer entierement le helper (lignes 15-21) :

```cpp
template <class T> consteval bool pointee_is_lifetime_aware() {
  const auto template_arguments = wrapped_types_of(^^T);

  return std::ranges::all_of(template_arguments, [](const auto argument) {
    return pointee_answer(argument, is_lifetime_aware_type);
  });
}
```

2) Remplacer le vouch generique (lignes 51-53) :

```cpp
template <smart_pointer T>
struct is_unsafe_lifetime_aware<T>
    : std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};
```

par trois vouchs explicites, en miroir exact de la forme deja utilisee par `is_unsafe_sendable` (ligne 64) : `pointee_answer` uniquement sur le pointee, trait normal sur le deleter stocke par valeur.

```cpp
template <class T>
struct is_unsafe_lifetime_aware<std::shared_ptr<T>>
    : std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type)> {};

template <class T>
struct is_unsafe_lifetime_aware<std::weak_ptr<T>>
    : std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type)> {};

template <class T, class D>
struct is_unsafe_lifetime_aware<std::unique_ptr<T, D>>
    : std::bool_constant<detail::pointee_answer(^^T, is_lifetime_aware_type) &&
                         is_lifetime_aware_v<D>> {};
```

Notes de verification :
- `is_lifetime_aware_v<D>` traite correctement `const D` et `D&` : `diagnose_is_lifetime_aware` (lifetime_aware.h:44-45) retire les cv en tete, et un `D&` repond FAUX (ligne 53), donc `unique_ptr<T, D&>` reste conservativement non lifetime_aware — aucune regression.
- Le garde-fou est conserve la ou il a un sens : `!is_lifetime_aware_v<std::unique_ptr<Base>>` et `!is_lifetime_aware_v<std::shared_ptr<Base>>` (Base polymorphe non-final) restent FAUX.
- Effet de bord a decider : apres ce changement, le concept `smart_pointer` et `is_smart_pointer_type` (smart_pointers.h:45-49) n'ont plus aucun utilisateur dans le repo. `is_smart_pointer_type` etait deja mort avant le correctif. Si `is_smart_pointer` n'est pas considere comme de l'API publique a but pedagogique, ces trois declarations peuvent etre supprimees dans la foulee ; sinon les laisser ne casse rien.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `s5.cpp`, compilee EXIT=0 (donc toutes les assertions ci-dessous sont vraies telles qu'ecrites) contre le header d'origine:

```text
struct PolymorphicDeleter { virtual ~PolymorphicDeleter() = default; void operator()(int *p) const { delete p; } };
static_assert(threadsafe::is_lifetime_aware_v<PolymorphicDeleter>);                        // le deleter seul: VRAI
static_assert(!threadsafe::is_lifetime_aware_v<std::unique_ptr<int, PolymorphicDeleter>>); // l'unique_ptr: FAUX
struct FinalDeleter final { void operator()(int *p) const { delete p; } };
static_assert(threadsafe::is_lifetime_aware_v<std::unique_ptr<int, FinalDeleter>>);        // final: VRAI
```
La seule variable entre les deux unique_ptr est `final`, donc la cause est bien `is_dynamic_type_known(D)`.
Apres correctif (inc2), fix_check.cpp compile EXIT=0 avec `is_lifetime_aware_v<std::unique_ptr<int, PolymorphicDeleter>>` VRAI, `!is_lifetime_aware_v<std::unique_ptr<int, BorrowPtr<int>>>` toujours FAUX, et les 12 tests OK.

</details>

### LIFE-04 — Un type non-template inscrit dans is_smart_pointer declenche une std::meta::exception non rattrapee au lieu d'un diagnostic

**MINEUR** · robustesse · `include/threadsafe/details/smart_pointers.h:16`

`wrapped_types_of` appelle `template_arguments_of(dealias(type))` sans le garde `has_template_arguments` que `is_allowed_std_wrapper` (allowed_std_wrappers.h:52) prend soin de poser. Si un utilisateur inscrit un type non-template dans `is_smart_pointer`, l'appel leve une exception de reflection au milieu d'un consteval, et le compilateur affiche `uncaught exception of type 'std::meta::exception'` suivi de la meme cascade de `'value' is not a member of ...` que le cas recursif. L'utilisateur n'a aucun moyen de relier ce message a sa specialisation.

> Défaut trouvé indépendamment par 2 auditeurs : *is_smart_pointer specialise pour un type non-template fait remonter une std::meta::exception brute depuis les entrailles de la bibliotheque*.

**Code actuel**

```cpp
const auto template_arguments = wrapped_types_of(^^T);
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Dans `include/threadsafe/details/smart_pointers.h`, remplacer

```cpp
template <class T> consteval bool pointee_is_lifetime_aware() {
  const auto template_arguments = wrapped_types_of(^^T);
```
par

```cpp
template <class T> consteval bool pointee_is_lifetime_aware() {
  static_assert(has_template_arguments(dealias(^^T)),
                "is_smart_pointer may only be specialized for a class template: "
                "the pointee is read from its template arguments");
  if (!has_template_arguments(dealias(^^T)))
    return false;

  const auto template_arguments = wrapped_types_of(^^T);
```
Le `static_assert` seul (correction proposee par l'auditeur) ne suffit pas: il ajoute une ligne lisible mais laisse l'exception de reflection et toute la cascade (5 erreurs au lieu de 4, out2.txt). Le `return false;` anticipe empeche l'evaluation de `wrapped_types_of` et ramene le diagnostic a une unique erreur (out3.txt). Verifie: les 12 `tests/test_*.cpp` compilent, et `shared_ptr` / `unique_ptr` / `weak_ptr` / un smart pointer utilisateur template gardent exactement leurs reponses.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `s1.cpp`, contre le header d'origine:

```text
struct DanglingHandle { int *borrowed; };
template <> struct threadsafe::is_smart_pointer<DanglingHandle> : std::true_type {};
static_assert(!threadsafe::is_lifetime_aware_v<DanglingHandle>);
```
Sortie (EXIT=1):
```text
smart_pointers.h:52:8: error: uncaught exception of type 'std::meta::exception'; 'what()': 'reflection does not have template arguments'
lifetime_aware.h:16:74: error: 'value' is not a member of 'threadsafe::is_unsafe_lifetime_aware<DanglingHandle>'
lifetime_aware.h:32:65: error: 'value' is not a member of 'threadsafe::is_lifetime_aware<DanglingHandle>'
s1.cpp:5:15: error: non-constant condition for static assertion
```
Apres correctif (inc2), la meme sonde compile EXIT=0 et `!is_lifetime_aware_v<DanglingHandle>` est vrai.

</details>

### LIFE-05 — Asymetrie fonction-pointeur / fonction-reference: void(*)() est lifetime_aware, void(&)() ne l'est pas

**MINEUR** · API · `include/threadsafe/details/lifetime_aware.h:50`

La ligne 50 ne dit oui qu'aux pointeurs de fonction (`remove_pointer`), puis la ligne 53 rejette toutes les references. Une reference de fonction designe pourtant une fonction a duree de stockage statique: elle ne peut pas pendre, exactement comme le pointeur. is_synchronizable le sait deja (synchronizable_base.h:47 dit oui a `is_function_type(type)`), donc une reference de fonction est SENDABLE mais pas LIFETIME_AWARE. Consequence concrete: une struct qui tient une reference de fonction est acceptee par is_sendable et refusee par launch_task, alors que la meme struct avec un pointeur de fonction passe. Incoherence visible dans une presentation.

**Code actuel**

```cpp
if (is_function_type(remove_pointer(type)))
  return true;

if (is_reference_type(type) || is_pointer_type(type))
  return false;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Ne PAS appliquer `is_function_type(remove_pointer(remove_reference(type)))` : cela rend lifetime_aware une reference vers un OBJET pointeur de fonction (`void(*&)()`), qui peut pendre — verifie par compilation.

Appliquer plutot, dans include/threadsafe/details/lifetime_aware.h:50 :

```cpp
if (is_function_type(remove_pointer(type)) ||
    is_function_type(remove_reference(type)))
  return true;
```
Les deux retraits sont poses en alternative, jamais composes : on accepte la fonction, le pointeur de fonction et la reference de fonction, et on ne traverse jamais une indirection vers un objet. La ligne 53 (`is_reference_type || is_pointer_type -> false`) reste inchangee et continue de rejeter `void(*&)()`, `void(**)()` et toutes les autres references.

Verifie : probe2.cpp EXIT=0 et les 12 tests/test_*.cpp compilent contre la copie patchee.

En complement, ajouter a tests/test_lifetime_aware.cpp les cas que la suite actuelle ne couvre pas (c'est leur absence qui laisse passer la correction fautive) :

```cpp
using FnPtr = void (*)();
static_assert(threadsafe::is_lifetime_aware_v<void (&)()>);
static_assert(!threadsafe::is_lifetime_aware_v<FnPtr &>);
static_assert(!threadsafe::is_lifetime_aware_v<void (**)()>);
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Sonde `s4.cpp`, compilee EXIT=0 contre le header d'origine (donc tout ce qui suit est vrai tel qu'ecrit):

  void some_function();
  struct HoldsFunctionReference { void (&fn)(); };
  struct HoldsFunctionPointer   { void (*fn)(); };
  static_assert(threadsafe::is_sendable_v<HoldsFunctionReference>);        // sendable
  static_assert(!threadsafe::is_lifetime_aware_v<HoldsFunctionReference>); // mais pas lifetime_aware
  static_assert(!threadsafe::is_task_participant_v<HoldsFunctionReference>);// donc refusee par launch_task
  static_assert(threadsafe::is_lifetime_aware_v<HoldsFunctionPointer>);
  static_assert(threadsafe::is_task_participant_v<HoldsFunctionPointer>);  // acceptee

p1.cpp confirme les cas nus: PROBE 01_fnptr (void(*)()) VRAI, PROBE 03_fnref (void(&)()) FAUX,
PROBE 02_fnptrptr (void(**)()) FAUX (correct: le pointeur pointe est un objet qui peut mourir).
```

</details>

### LIFE-06 — std::weak_ptr est declare lifetime_aware alors qu'il ne maintient pas son referent en vie

**DÉTAIL** · API · `include/threadsafe/details/smart_pointers.h:51`

La definition revendiquee dans CLAUDE.md est "True if a T owns its data or keeps its referent alive". weak_ptr ne fait ni l'un ni l'autre: il maintient le bloc de controle, pas l'objet. Le verdict n'est pas un trou de surete (l'acces passe obligatoirement par lock(), qui rend un shared_ptr vide si l'objet est mort, donc rien ne pend jamais), mais c'est un contre-exemple a la regle enoncee, et le commentaire du test l'admet a demi-mot ("weak_ptr keeps its control block alive"). Pour une bibliotheque dont la regle EST le contenu pedagogique, la definition doit couvrir le cas au lieu d'etre contredite par lui.

**Code actuel**

```cpp
template <smart_pointer T>
struct is_unsafe_lifetime_aware<T>
    : std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Aucun changement de code (le verdict `true` est correct pour `weak_ptr` comme pour les pointeurs de fonction). La correction est documentaire, mais doit couvrir les DEUX contre-exemples, pas seulement `weak_ptr` comme le proposait l'auteur.

1) CLAUDE.md:104-106 — remplacer :

```cpp
### `is_lifetime_aware<T>`

True if a `T` owns its data or keeps its referent alive. Ownership is **transitive**
```
par :

```cpp
### `is_lifetime_aware<T>`

True if a `T` cannot hand out a reference that outlives its referent. Three
shapes satisfy it: `T` owns its data (a scalar, a `vector`), it keeps the
referent alive (`shared_ptr`, `unique_ptr`), or it checks before handing
anything out (`std::weak_ptr`, whose only access is `lock()`, and function
pointers, whose referent is static). A borrow — `T*`, `T&`, `span`,
`reference_wrapper` — never qualifies. The answer is **transitive**: the
question reaches the pointee and every member.
```
2) tests/test_lifetime_aware.cpp:56 — aligner le message, qui aujourd'hui esquive
le mot « referent » au lieu d'expliquer pourquoi la réponse est oui :

```cpp
static_assert(is_lifetime_aware_v<std::weak_ptr<int>>,
              "is_lifetime_aware — weak_ptr cannot hand out a dangling reference: "
              "lock() checks the control block and yields nothing if the object died");
```
Patch appliqué sur une copie dans mon scratchpad, `tests/test_*.cpp` tous recompilés : aucun test cassé.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `s6.cpp`, compilee EXIT=0:

```text
static_assert(threadsafe::is_lifetime_aware_v<std::weak_ptr<int>>);
static_assert(threadsafe::is_sendable_v<std::weak_ptr<std::atomic<int>>>);
static_assert(threadsafe::is_task_participant_v<std::weak_ptr<std::atomic<int>>>);
```
Donc un weak_ptr traverse bien launch_task. p1.cpp PROBE 17_weakptr est egalement VRAI.

</details>

## Les fondations du walk réflectif

### WALK-01 — Un pointeur vers type incomplet (PImpl) sort une erreur de precondition GCC au lieu du static_assert de la bibliotheque

**MAJEUR** · robustesse · `include/threadsafe/details/utils.h:24`

`assert_queryable_type<T>` teste `is_complete_type(^^std::remove_all_extents_t<T>)`, mais la recursion sur un pointeur passe par une REFERENCE : `diagnose_is_sendable` fait `is_sendable_type(add_lvalue_reference(remove_pointer(type)))`. Or `remove_all_extents_t<Impl&>` vaut `Impl&`, et une reference est toujours un type complet : le static_assert ne se declenche donc pas. Le walk atteint ensuite `is_dynamic_type_known`, qui appelle `is_polymorphic_type` sur un type de classe incomplet et viole sa precondition. CLAUDE.md promet qu'un pointee incomplet "empoisonne la question" ; c'est vrai, mais avec un message interne du compilateur, pas celui de la bibliotheque. La forme qui declenche ca est le PImpl, l'idiome C++ le plus courant : `struct Widget { struct Impl; Impl* impl_; };`.

> Défaut trouvé indépendamment par 3 auditeurs : *Un pointeur ou une reference vers un type incomplet produit une erreur interne opaque au lieu du static_assert documente*, *Une reference/un pointeur vers un type incomplet echappe a assert_queryable_type et sort une erreur interne du compilateur*.

**Code actuel**

```cpp
inline consteval bool is_dynamic_type_known(std::meta::info type) {
  type = remove_cv(remove_all_extents(type));
  return !is_polymorphic_type(type) || is_final(type);
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee est juste et je l'ai validee : les DEUX moities sont necessaires (j'ai prouve par compilation que chacune seule echoue, la garde seule produisant meme un faux negatif silencieux). J'y apporte un seul raffinement : etendre la garde aux unions, sinon une union incomplete sort le message de la bibliotheque en double.

Dans `include/threadsafe/details/utils.h`, remplacer les lignes 12-25 :

```cpp
template <class T> consteval bool assert_queryable_type() {
  static_assert(!std::is_void_v<T>,
                "void is not a value: there is nothing to send, share or "
                "keep alive");
  static_assert(is_complete_type(
                    ^^std::remove_all_extents_t<std::remove_reference_t<T>>),
                "an incomplete type has unknown members: complete it before "
                "asking the traits");
  return true;
}

inline consteval bool is_dynamic_type_known(std::meta::info type) {
  type = remove_cv(remove_all_extents(type));
  if ((is_class_type(type) || is_union_type(type)) && !is_complete_type(type))
    return false;
  return !is_polymorphic_type(type) || is_final(type);
}
```

Role de chaque moitie, mesure a la compilation :
- `remove_reference_t` dans `assert_queryable_type` : c'est lui qui produit le message de la bibliotheque. `is_sendable_v<Impl&>` ne doit plus passer la verification de completude sous pretexte qu'une reference est toujours complete.
- la garde dans `is_dynamic_type_known` : elle est indispensable parce que GCC instancie l'operande droit du `&&` (`is_sendable<T>::value`) meme apres l'echec du static_assert de l'operande gauche. Sans elle, `diagnose_is_sendable` s'execute quand meme et `sendable.h:47` viole la precondition de `is_polymorphic_type`. Elle sert donc a SUPPRIMER l'erreur interne du compilateur, pas a repondre — c'est `assert_queryable_type` qui repond.

Verifie : 1 seule erreur (message de la bibliotheque) sur `struct Widget2 { struct Impl; Impl* impl_; };`, sur `struct W3 { I3& r; };` et sur `union U; struct WU { U* p; };` ; les 12 tests du depot compilent ; les reponses du walk sur les types complets (pointeurs polymorphes/final/plain, `unique_ptr`, vouch `is_unsafe_sendable` sur pointee incomplet) sont inchangees.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

SONDE q_rawpimpl.cpp :
```text
#include <threadsafe/threadsafe.h>
struct Widget2 { struct Impl; Impl* impl_; };
constexpr bool a = threadsafe::is_sendable_v<Widget2>;
```
SORTIE AVANT (headers du depot) :
```text
utils.h:24:30: error: type trait 'std::meta::is_polymorphic_type' preconditions not satisfied
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<Widget2::Impl&>'
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<Widget2::Impl*>'
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<Widget2>'
```
SORTIE APRES (copie patchee dans scratchpad/audit_walk/inc) : une seule erreur
```text
inc/threadsafe/details/utils.h:16:35: error: static assertion failed: an incomplete type has unknown members: complete it before asking the traits
```
NON-REGRESSION : les 12 fichiers de tests du depot compilent avec le header patche (test_asynchronous_task_launcher, test_containers, test_copy_on_write, test_deferred_specialization, test_diagnostics, test_lifetime_aware, test_polymorphic, test_sendable, test_smart_pointers, test_soundness_regressions, test_synchronizable, test_synchronized_value : tous OK).

Contre-vérification indépendante :

=== q_rawpimpl.cpp, headers DU DEPOT (reproduction exacte du finding) ===
utils.h:24:30: error: type trait 'std::meta::is_polymorphic_type' preconditions not satisfied
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<Widget2::Impl&>'
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<Widget2::Impl*>'
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<Widget2>'
-> identique au "AVANT" annonce, mot pour mot.

=== qq3.cpp (membre reference vers incomplet), headers DU DEPOT ===
utils.h:24:30: error: type trait 'std::meta::is_polymorphic_type' preconditions not satisfied
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<I3&>'
sendable.h:29:59: error: 'value' is not a member of 'threadsafe::is_sendable<W3>'
-> seconde forme du bug, non citee par le finding.

[…]

</details>

### WALK-02 — Faux negatifs en masse sur la bibliotheque standard : la liste allowed_std_wrappers est fermee et un utilisateur ne peut pas l'etendre

**MINEUR** · flexibilité · `include/threadsafe/details/allowed_std_wrappers.h:29`

allowed_std_wrappers est un tableau ferme de 18 templates. Tout type standard possedant absent du tableau repond NON aux trois traits, non pas parce que le walk aurait trouve quelque chose de suspect, mais parce qu'il ne peut pas le traverser (les implementations libstdc++ ont des constructeurs templates ou une base d'implementation, donc detail::is_default_type echoue et is_walkable_type rend false). Tableau mesure (send / sync<const T> / life), en-tetes actuels :

  std::vector<int>      [liste]   1 1 1   <- reference
  std::flat_map<int,int>          0 0 0   faux negatif (base _Flat_map_impl non 'default')
  std::flat_set<int>              0 0 0   faux negatif
  std::expected<int,int>          0 0 0   faux negatif
  std::bitset<8>                  0 0 0   faux negatif
  std::valarray<int>              0 0 0   faux negatif
  std::complex<double>            0 0 0   faux negatif
  std::filesystem::path           0 0 0   faux negatif
  std::stringstream               0 0 0   faux negatif
  std::stack/queue/priority_queue 0 0 0   faux negatif (adaptateurs, tres courants)
  std::inplace_vector<int,4>      0 0 0   faux negatif
  std::span<int>                  0 0 0   CORRECT (borrowed : life=0 voulu)
  std::string_view                0 0 0   CORRECT (borrowed)
  std::initializer_list<int>      0 0 0   CORRECT (borrowed)
  std::any                        0 0 0   CORRECT (efface : indecidable, conservateur)
  std::function / move_only_function 0 0 0 CORRECT (efface)

Pour un public de conference, 'je mets un std::expected dans mon synchronized_value et la biblio dit non' est la premiere question qui tombera, et la reponse actuelle ('completez la liste dans les sources') est la mauvaise reponse pour une biblio header-only.

**Code actuel**

```cpp
inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,
    ^^std::deque,
    ^^std::list,
    ^^std::forward_list,
    ^^std::basic_string,
    ...
    ^^std::array,
};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
DEUX CORRECTIONS, la seconde est indispensable et manque au finding.

(a) allowed_std_wrappers.h — completer le tableau (comme propose), ligne 47, plus les #include <bitset> <complex> <expected> <flat_map> <flat_set> <queue> <stack> <valarray> :

    ^^std::array,
    ^^std::expected,
    ^^std::flat_map,
    ^^std::flat_set,
    ^^std::bitset,
    ^^std::complex,
    ^^std::valarray,
    ^^std::stack,
    ^^std::queue,
    ^^std::priority_queue,
};

(b) OBLIGATOIRE avec (a) — allowed_std_wrappers.h:64-67, sinon std::expected<void, E> devient une erreur dure dans utils.h. Un argument de template `void` ne porte aucun etat : il n'y a rien a envoyer, partager ou garder en vie, donc rien a interroger.

  AVANT :
    for (auto argument : template_arguments_of(dealias(type)))
      if (is_type(argument))
        wrapped.push_back(wrapper_is_const ? add_const(remove_cv(argument))
                                           : remove_cv(argument));

  APRES :
    for (auto argument : template_arguments_of(dealias(type)))
      if (is_type(argument) && !is_void_type(remove_cv(argument)))
        wrapped.push_back(wrapper_is_const ? add_const(remove_cv(argument))
                                           : remove_cv(argument));

VERIFIE : les 12 fichiers tests/*.cpp compilent, et les 13 assertions de p4_final.cpp passent, dont
  static_assert(is_sendable_v<std::expected<void, std::string>>);
  static_assert(!is_sendable_v<std::expected<void, int*>>);
  static_assert(!is_lifetime_aware_v<std::expected<void, std::string_view>>);
  static_assert(is_sendable_v<synchronized_value<std::expected<void, std::string>>>);

(c) Doc : plutot que « la liste n'est pas une frontiere infranchissable », montrer les 3 lignes qui suffisent, puisqu'elles compilent deja sur les headers actuels et sont le vrai contre-argument au titre du finding :

  template <class T, class E>
  struct threadsafe::is_unsafe_sendable<std::expected<T, E>>
      : std::bool_constant<threadsafe::is_sendable_v<T> && threadsafe::is_sendable_v<E>> {};
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
TABLEAU MESURE par p9_stdtable.cpp / p20_adaptors.cpp (programmes compiles ET executes, sortie citee ci-dessus).
CAUSE ETABLIE par p10_why.cpp et p12.cpp : std::flat_map est walkable mais sa base unique std::_Flat_map_impl<...> rend walk=0 def=0 ; expected/bitset/valarray/complex/path/stringstream ont directement is_default_type=0.

VERIFICATION DU CORRECTIF 1 (tableau etendu dans une copie scratchpad des en-tetes), p22_list_fix.cpp compile sans erreur :
  static_assert(threadsafe::is_sendable_v<std::expected<int,std::string>>);
  static_assert(threadsafe::is_synchronizable_v<const std::expected<int,std::string>>);
  static_assert(threadsafe::is_lifetime_aware_v<std::expected<int,std::string>>);
  static_assert(threadsafe::is_sendable_v<std::flat_map<int,std::string>>);
  static_assert(threadsafe::is_sendable_v<std::bitset<8>>);
  static_assert(threadsafe::is_sendable_v<std::complex<double>>);
  static_assert(threadsafe::is_sendable_v<std::valarray<int>>);
  static_assert(threadsafe::is_sendable_v<std::stack<int>>);
  static_assert(threadsafe::is_sendable_v<std::priority_queue<int>>);
  static_assert(!threadsafe::is_sendable_v<std::expected<int*,int>>);   // reste conservateur
  static_assert(!threadsafe::is_sendable_v<std::flat_map<int,int*>>);   // reste conservateur
Les 12 fichiers de tests/*.cpp compilent toujours avec la liste etendue.

VERIFICATION DU CORRECTIF 2 (en-tetes NON modifies), p21_user_extends_list.cpp compile sans erreur : 9 lignes de is_unsafe_* cote utilisateur suffisent a couvrir std::expected, sans ambiguite avec la specialisation partielle contrainte sur std_wrapper.
```

</details>

### WALK-03 — Interroger void produit DEUX static_assert au lieu d un

**DÉTAIL** · robustesse · `include/threadsafe/details/utils.h:16`

`void` est aussi un type incomplet, donc les deux static_assert de `assert_queryable_type` echouent. L'utilisateur qui ecrit `is_sendable_v<void*>` recoit a la fois "void is not a value" et "an incomplete type has unknown members: complete it before asking the traits" — le second est faux et actionnable a tort (on ne "complete" pas void). Pour une bibliotheque dont le contrat explicite est que le diagnostic vit dans le message, c'est le message qui se contredit.

> Défaut trouvé indépendamment par 3 auditeurs : *Demander un trait sur `void` declenche deux static_assert, dont un faux*, *is_sendable_v<void> emet une seconde erreur absurde: "complete it before asking the traits"*.

**Code actuel**

```cpp
static_assert(!std::is_void_v<T>,
              "void is not a value: there is nothing to send, share or "
              "keep alive");
static_assert(is_complete_type(^^std::remove_all_extents_t<T>),
              "an incomplete type has unknown members: complete it before "
              "asking the traits");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Correction MINIMALE, sans le `remove_reference_t` hors-sujet (include/threadsafe/details/utils.h:12-20), verifiee: supprime le message parasite pour void et laisse strictement inchange tout le reste (Incomplete& / Incomplete* diagnostiquent exactement comme avant), 12/12 tests OK.

```cpp
template <class T> consteval bool assert_queryable_type() {
  static_assert(!std::is_void_v<T>,
                "void is not a value: there is nothing to send, share or "
                "keep alive");
  static_assert(std::is_void_v<T> ||
                    is_complete_type(^^std::remove_all_extents_t<T>),
                "an incomplete type has unknown members: complete it before "
                "asking the traits");
  return true;
}
```

Le `||` court-circuite en evaluation constante: pour `void`, le second operande n'est jamais evalue, donc seul le message "void is not a value" est emis.

NE PAS reprendre le `std::remove_reference_t` de la correction proposee dans ce finding: il est sans rapport avec void et modifie le diagnostic des types incomplets derriere reference/pointeur (il ajoute un static_assert a cote de l'erreur dure existante). Si cet ajout est souhaite, il doit faire l'objet d'un finding distinct — et la vraie question y est le `is_polymorphic_type` de utils.h:24 appele sur un type incomplet, qui produit une erreur dure au lieu d'un diagnostic de la bibliotheque; masquer cela derriere un static_assert supplementaire ne supprime pas l'erreur dure.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

SONDE q_void.cpp : `constexpr bool a = threadsafe::is_sendable_v<void>;`
AVANT : `grep -c "static assertion failed"` = 2
```text
utils.h:13:23: error: static assertion failed: void is not a value: there is nothing to send, share or keep alive
utils.h:16:33: error: static assertion failed: an incomplete type has unknown members: complete it before asking the traits
```
Idem pour `is_sendable_v<void*>` et `is_sendable_v<const void>` (4 messages dans ce dernier cas).

APRES (copie patchee) : un seul message
```text
inc/threadsafe/details/utils.h:13:23: error: static assertion failed: void is not a value: there is nothing to send, share or keep alive
```
Les 12 fichiers de tests du depot compilent toujours.

</details>

## `copy_on_write<T>`

### COW-01 — operator*() && est declare noexcept alors qu'il renvoie une COPIE de T : std::terminate prouve

**MAJEUR** · robustesse · `include/threadsafe/details/copy_on_write.h:27`

`T operator*() && noexcept` renvoie un `T` par valeur : il construit une copie de `*ptr_` sur place. Si le constructeur de copie de `T` lance (un `std::bad_alloc` de `std::string` suffit), le `noexcept` transforme l'exception en `std::terminate`. La surcharge existe pour une bonne raison (empecher `*std::move(c)` de renvoyer une reference pendante sur un temporaire), mais son `noexcept` est un mensonge pour tout `T` non trivialement copiable, c'est-a-dire pour tous les `T` interessants de la suite de tests (`std::string`, `std::vector<int>`, `std::map<...>`).

> Défaut trouvé indépendamment par 2 auditeurs : *copy_on_write::operator*() && renvoie une copie profonde silencieuse (+160 us sur un vector de 8 Mio) et ment sur noexcept*.

**Code actuel**

```cpp
T operator*() && noexcept { return *ptr_; }
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le correctif propose est correct tel quel : je l'ai applique, il compile, il fait ce qu'il annonce et il ne casse aucun des 13 tests. Je le reprends a l'identique, avec deux precisions verifiees.

Dans include/threadsafe/details/copy_on_write.h, remplacer la ligne 27 :

```cpp
T operator*() && noexcept { return *ptr_; }
```
par :

```cpp
T operator*() && noexcept(std::is_nothrow_copy_constructible_v<T>) {
  return *ptr_;
}
```
Precision 1 — aucun include a ajouter : <type_traits> est deja inclus ligne 6.

Precision 2 — ne pas toucher aux lignes 24, 25 et 28. Le grep exhaustif confirme que `const T &operator*() const &` et `const T *operator->() const &` renvoient une reference et un pointeur : leur `noexcept` inconditionnel est exact et doit rester. La ligne 27 est la seule anomalie du fichier.

Dans tests/test_copy_on_write.cpp, ajouter en fin de fichier (l'alias `cow`, <string> et <utility> y sont deja disponibles, verifie par compilation) :

```cpp
static_assert(!noexcept(*std::declval<cow<std::string>&&>()),
              "operator*() && renvoie une copie : il ne peut pas etre "
              "noexcept quand la copie de T peut lancer");
static_assert(noexcept(*std::declval<cow<int>&&>()),
              "mais la copie d'un T trivial ne lance pas : la garantie est "
              "conservee la ou elle est vraie");
```
Ces deux assertions sont discriminantes et non decoratives : le fichier de test ainsi augmente compile (exit 0) avec le correctif et echoue avec l'entete d'origine. La regression est donc verrouillee, ce qui est exactement la propriete que la bibliotheque revendique — la verification a la compilation.

Note de conception, a ne pas confondre avec le correctif : renvoyer un `T` par valeur depuis la surcharge `&&` reste le bon choix et ne doit pas etre modifie. On pourrait etre tente d'aller plus loin en deplacant depuis le bloc quand `ptr_.use_count() == 1`, mais ce serait un changement semantique risque — `use_count()` n'est pas une base fiable pour decider d'un vol en presence de concurrence, precisement le scenario que cette bibliotheque adresse. Le defaut est strictement le `noexcept` ; le corriger sans rien changer d'autre est la bonne portee.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

1) Preuve a la compilation que le noexcept ment (scratchpad/audit_cow/p2_noexcept.cpp, exit 0) :
```text
static_assert(!std::is_nothrow_copy_constructible_v<std::string>);
static_assert(noexcept(*std::declval<copy_on_write<std::string> &&>()));
static_assert(std::is_same_v<decltype(*std::declval<copy_on_write<std::string>&&>()), std::string>);
```
2) Preuve a l'execution (scratchpad/audit_cow/p2b_run.cpp) :
```text
struct Throwy { Throwy() = default; Throwy(const Throwy&) { throw 42; } };
threadsafe::copy_on_write<Throwy> c{};
try { auto v = *std::move(c); } catch (...) { puts("CATCH"); }
```
-> g++-16 -std=c++26 -freflection ... -o p2b && ./p2b donne :
```text
terminate called after throwing an instance of 'int'
exit=134
```
Le catch n'est jamais atteint.

3) Le correctif compile et laisse passer les tests du depot : header patche dans scratchpad/audit_cow/fix/, sonde p10.cpp (`!noexcept(...string...)` + `noexcept(...int...)`) exit 0 avec le fix, exit 1 avec l'entete d'origine ; tests/test_copy_on_write.cpp recompile avec le fix : exit 0.

Contre-vérification indépendante :

$ g++-16 -std=c++26 -freflection -fsyntax-only -I .../include p1.cpp; echo "exit=$?"
exit=0
```text
-> le noexcept EST present sur une fonction qui renvoie std::string par valeur.
```
$ g++-16 -std=c++26 -freflection -I .../include p2_run.cpp -o p2_run && ./p2_run; echo "run exit=$?"
terminate called after throwing an instance of 'int'
run exit=134
```text
-> "CATCH" n'est jamais affiche. std::terminate confirme.
```
$ grep -rn "noexcept" include/
include/threadsafe/details/copy_on_write.h:24:  const T &operator*() const & noexcept { return *ptr_; }
include/threadsafe/details/copy_on_write.h:25:  const T *operator->() const & noexcept { return ptr_.get(); }
include/threadsafe/details/copy_on_write.h:27:  T operator*() && noexcept { return *ptr_; }
include/threadsafe/details/copy_on_write.h:28:  T operator->() && noexcept = delete;
[…]

</details>

### COW-02 — as_mutable() && n'a aucune contrainte : il se declare disponible pour un T move-only puis explose dans le corps

**MAJEUR** · API · `include/threadsafe/details/copy_on_write.h:40`

La surcharge lvalue porte `requires std::copy_constructible<T>` et est donc SFINAE-friendly : c'est exactement ce que teste tests/test_copy_on_write.cpp:131 (`!can_detach<cow<std::unique_ptr<int>>>`, avec le rationale "un T qui ne peut pas etre copie donne un handle en lecture seule, PAS une erreur dure"). La surcharge rvalue ligne 40 n'a pas cette contrainte. Resultat : pour un `T` move-only, la detection repond OUI (la surcharge && est declaree viable) alors que l'appel produit une erreur dure a l'interieur du corps, exactement ce que le test pretend avoir exclu. Le `can_detach` du test ne prend qu'une lvalue, il ne couvre pas cette moitie de l'API.

**Code actuel**

```cpp
T as_mutable() && { return std::move(as_mutable()); }
```

**Correction proposée**

```cpp
T as_mutable() &&
    requires std::copy_constructible<T>
  {
    return std::move(as_mutable());
  }

et completer le test, qui aujourd'hui ne regarde que la lvalue :

  template <class C>
  constexpr bool can_detach_rvalue = requires(C c) { std::move(c).as_mutable(); };

  static_assert(!can_detach_rvalue<cow<std::unique_ptr<int>>>,
                "as_mutable — la rvalue doit disparaitre comme la lvalue, pas "
                "echouer dans son corps");
  static_assert(can_detach_rvalue<cow<int>>);
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
1) La detection repond OUI sur l'entete d'origine (scratchpad/audit_cow/p9.cpp, exit 0) :
  template <class C> constexpr bool detach_lvalue = requires(C c) { c.as_mutable(); };
  template <class C> constexpr bool detach_rvalue = requires(C c) { std::move(c).as_mutable(); };
  using MO = threadsafe::copy_on_write<std::unique_ptr<int>>;
  static_assert(!detach_lvalue<MO>);
  static_assert(detach_rvalue<MO>);   // <-- passe : la rvalue se declare disponible

2) L'appel est une erreur dure (scratchpad/audit_cow/p3b_moveonly_call.cpp) :
  copy_on_write<std::unique_ptr<int>> c{std::make_unique<int>(1)};
  auto out = std::move(c).as_mutable();
-> copy_on_write.h:40:50: error: passing 'threadsafe::copy_on_write<std::unique_ptr<int> >' as 'this' argument discards qualifiers
   note: in call to 'T threadsafe::copy_on_write<T>::as_mutable() && [with T = std::unique_ptr<int>]'
   (la surcharge & est eliminee par sa contrainte, ne reste que la && qui ne peut pas se lier a l'lvalue *this)

3) Le correctif marche (header patche dans scratchpad/audit_cow/fix/, sonde p9b.cpp) :
  static_assert(!detach_rvalue<cow<std::unique_ptr<int>>>);
  static_assert(detach_rvalue<cow<std::string>>);
-> exit 0 ; et tests/test_copy_on_write.cpp recompile avec le fix : exit 0.
```

Contre-vérification indépendante :

$ g++-16 -std=c++26 -freflection -fsyntax-only -I /Users/.../include p1.cpp
EXIT=0
```text
-> la detection repond bien OUI sur la rvalue pour un T move-only
```
$ g++-16 -std=c++26 -freflection -fsyntax-only -I /Users/.../include p2.cpp
In file included from .../threadsafe.h:11, from p2.cpp:1:
.../copy_on_write.h: In instantiation of 'T threadsafe::copy_on_write<T>::as_mutable() && [with T = std::unique_ptr<int>]':
p2.cpp:7:37:   required from here
```text
7 |   auto out = std::move(c).as_mutable();
  |              ~~~~~~~~~~~~~~~~~~~~~~~^~
```
.../copy_on_write.h:40:50: error: passing 'threadsafe::copy_on_write<std::unique_ptr<int> >' as 'this' argument discards qualifiers [-fpermissive]
```text
40 |   T as_mutable() && { return std::move(as_mutable()); }
   |                                        ~~~~~~~~~~^~
```
[…]

</details>

### COW-03 — copy_on_write<T> n'a aucune règle is_unsafe_synchronizable : le type « plusieurs lecteurs » de la bibliothèque n'est jamais partageable en lecture

**MINEUR** · flexibilité · `include/threadsafe/details/copy_on_write.h:46`

`copy_on_write<T>` n'expose en const que `operator*() const &` et `operator->() const &`, qui rendent un `const T&` ; `as_mutable()` est non const. La lecture concurrente est donc sûre exactement quand `is_synchronizable_v<const T>` — condition que le fichier calcule déjà pour la clause sendable (ligne 48). Mais aucune spécialisation `is_unsafe_synchronizable` n'est écrite, et le walk structurel ne peut pas la retrouver : `is_default_type(copy_on_write<T>)` est faux (constructeur template, `may_hijack_copy_move`), donc `is_walkable_type` échoue et `const copy_on_write<T>` répond toujours non. Résultat : on ne peut pas mettre un `copy_on_write` dans une struct partagée en lecture, ni dans un `synchronized_value` en mode shared_mutex — ce qui prive le type de sa raison d'être (beaucoup de lecteurs, une copie à l'écriture).

> Défaut trouvé indépendamment par 2 auditeurs : *is_synchronizable_v<const copy_on_write<T>> repond non, alors que const synchronized_value<T> repond oui : incoherence entre les deux helpers*.

**Code actuel**

```cpp
template <class T>
struct is_unsafe_sendable<copy_on_write<T>>
    : std::bool_constant<is_sendable_v<T> && is_synchronizable_v<const T>> {};

template <class T>
struct is_unsafe_lifetime_aware<copy_on_write<T>>
    : std::bool_constant<is_lifetime_aware_v<T>> {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposée est correcte telle quelle et je l'ai compilée : dans `include/threadsafe/details/copy_on_write.h`, entre la clause sendable (l. 46-48) et la clause lifetime_aware (l. 50-52), insérer

```cpp
template <class T>
struct is_unsafe_synchronizable<const copy_on_write<T>>
    : std::bool_constant<is_synchronizable_v<const T>> {};
```

(La forme non const reste volontairement refusée : `as_mutable()` réassigne `ptr_`.)

Deux ajustements par rapport au finding :

1. Ne pas présenter `!is_sendable_v<const copy_on_write<T>&>` comme un symptôme ni comme une chose que le patch répare : `diagnose_is_sendable` retire le const du référent (sendable.h:51), c'est la règle « const derrière une indirection n'est jamais fait confiance », et elle s'applique identiquement à `const std::vector<int>&`. Ce résultat reste false après correction, et c'est voulu.

2. Ajouter au test `tests/test_copy_on_write.cpp` (à côté de l'assertion existante l. 107 qui ne couvre que la forme non const) le couple qui fixe l'intention, sinon la clause pourra redisparaître sans qu'aucun test ne tombe :

```cpp
static_assert(is_synchronizable_v<const cow<std::string>>,
              "is_synchronizable — un handle const n'expose que operator* et "
              "operator-> qui rendent un const T& : as_mutable est non const");
static_assert(!is_synchronizable_v<const cow<Cache>>,
              "is_synchronizable — la lecture partagée vaut exactement ce que "
              "vaut const T");
static_assert(threadsafe::synchronized_value<cow<std::string>>::shared_readable,
              "synchronized_value — un payload copy-on-write retrouve le "
              "shared_mutex");
```

Ces trois assertions compilent contre la copie corrigée (vérifié).

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p9.cpp`, compilée sans erreur : `is_sendable_v<copy_on_write<std::string>>` vrai, `is_synchronizable_v<const std::string>` vrai, mais `!is_synchronizable_v<const copy_on_write<std::string>>`, `!is_synchronizable_v<const HoldsCow>` (struct contenant un copy_on_write) et `!is_sendable_v<const copy_on_write<std::string>&>`.

Correctif validé : .../audit_sync/fix1.cpp, qui écrit la spécialisation ci-dessus dans la TU, compile `is_synchronizable_v<const copy_on_write<std::string>>`, `is_synchronizable_v<const HoldsCow>` et `!is_synchronizable_v<const copy_on_write<int*>>` (toujours refusé quand const T n'est pas partageable).

</details>

### COW-04 — as_mutable() construit un happens-before sur use_count(), que la norme declare explicitement approximatif

**MINEUR** · thread safety · `include/threadsafe/details/copy_on_write.h:33`

Le coeur du composant decide de detacher ou d'ecrire en place a partir de `ptr_.use_count()`. L'analyse memoire complete est la suivante.

SCENARIO. Le walk interdit de partager UN MEME objet `copy_on_write` entre threads (prouve ci-dessous : ni `cow&`, ni `cow*`, ni `const cow&`, ni `const cow` comme membre ne passent). Seule une COPIE voyage : deux objets `cow` distincts, un par thread, partagent un bloc de controle. C'est exactement le cas que `as_mutable()` doit arbitrer.

CAS 1 - A lit 2, B lit 2 : les deux copient. Les deux lisent `*ptr_` en const simultanement -> couvert par `is_synchronizable_v<const T>` de la claim. OK.
CAS 2 - A detruit/reassigne son handle (2->1), puis B lit 1 : B prend la branche `else`, fence acquire, puis ECRIT dans le bloc. Les lectures de A doivent happen-before les ecritures de B. [atomics.fences]/3 : un fence acquire se synchronise avec une operation release X s'il existe une lecture atomique M (meme relaxed) sequencee AVANT le fence et qui lit la valeur ecrite par X. Ici M = le load dans `use_count()`, X = la decrementation du refcount. La valeur 1 ne peut venir que de cette decrementation (ordre de modification), et le load est bien sequence avant le fence. La paire tient DONC SI la decrementation est une operation release.
CAS 3 - B lit 1 pendant qu'un tiers incremente : impossible, si le compteur vaut 1 personne d'autre ne detient de handle pour le copier.

Il n'y a donc PAS de course reelle sur libstdc++ : le fence est correct et correctement apparie. MAIS l'appariement repose entierement sur deux details d'implementation, pas sur la norme :
- `_Sp_counted_base::_M_release()` decremente avec `__ATOMIC_ACQ_REL` (ext/atomicity.h:71) -> la moitie release existe.
- `_M_get_use_count()` est un load `__ATOMIC_RELAXED`, avec le commentaire libstdc++ : "No memory barrier is used here so there is no synchronization with other threads."

La norme, elle, refuse ce raisonnement : la note attachee a `use_count()` ([util.smartptr.shared.obs]) dit que le resultat doit etre traite comme approximatif et, mot pour mot, que `use_count() == 1` n'implique PAS que les acces faits via un `shared_ptr` precedemment detruit soient "in any sense completed". C'est la raison meme du retrait de `shared_ptr::unique()` en C++20. Le code fait precisement l'inference que la note interdit.

Consequence : le composant est correct sur libstdc++ (et libc++) mais n'a aucune garantie normative, et une implementation qui rendrait la decrementation relaxed le casserait silencieusement. Pour une lib educative c'est la ligne la plus subtile du depot, elle n'a ni commentaire, ni test, ni mention de l'hypothese.

**Code actuel**

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

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Garder l'idee (documenter la ligne) mais reecrire le commentaire et abandonner l'alternative "normative".

1) Le commentaire. Version compilee et validee contre les 12 tests (aucun "CASSE"), plus courte, et corrigeant deux defauts de la version proposee : elle repond d'abord a la vraie question du lecteur ("pourquoi une barriere alors que je suis seul ?"), et elle enonce le risque residuel correctement.

  T &as_mutable() &
    requires std::copy_constructible<T>
  {
    if (ptr_.use_count() != 1) {
      ptr_ = std::make_shared<T>(*ptr_);
      return *ptr_;
    }

    // Sole owner now, but another thread was one a moment ago and read the T.
    // Its handle's release decrement is the value this use_count() just read,
    // so the acquire fence orders its reads before our writes
    // ([atomics.fences]/3). [util.smartptr.shared.obs] calls use_count()
    // approximate and does not promise that pairing: it holds as long as
    // use_count() atomically reads the very counter the decrement modifies.
    std::atomic_thread_fence(std::memory_order_acquire);
    return *ptr_;
  }

Pourquoi la derniere phrase differe de celle proposee : le finding ecrit qu'"une implementation qui rendrait la decrementation relaxed le casserait silencieusement". C'est inexact et cela donnerait un commentaire trompeur. Une decrementation non-finale ordonnee release est imposee par le contrat propre de shared_ptr : si elle etait relaxed, la decrementation finale executant ~T courserait avec les acces anterieurs des autres proprietaires, et shared_ptr serait casse pour tout le monde, independamment de ce code. L'appariement survit d'ailleurs aux variantes (fence release + decrementation relaxed s'apparie via [atomics.fences]/2 ; acq_rel/seq_cst via /3). Le seul risque conforme reel est qu'une implementation ne lise pas ATOMIQUEMENT, dans use_count(), l'objet meme que la decrementation modifie — liste chainee de proprietaires, compteur stripe, ou lecture non atomique du compteur (cas MSVC), ce qui invalide l'exigence "Y is an atomic operation" de [atomics.fences]/3. C'est cette condition-la qu'il faut nommer.

2) Completer CLAUDE.md, qui est le support d'explication canonique du depot (les headers n'ont volontairement aucun commentaire explicatif : zero dans tout include/). Section `copy_on_write<T>`, remplacer :
   "A shared `T` read through `const` only; `as_mutable()` copies first whenever the block is shared."
par :
   "A shared `T` read through `const` only; `as_mutable()` copies first whenever
   the block is shared. When it is not, the in-place write still needs an acquire
   fence: a thread that dropped its handle a moment ago read the `T`, and its
   release decrement — the value `use_count()` just read — is what orders those
   reads before ours. This is the one place the library trusts an implementation
   property rather than a normative one: `use_count()` is specified as approximate."

3) Abandonner l'alternative `std::shared_ptr<std::pair<std::atomic<unsigned>, T>>` avec fetch_sub/fetch_add manuels. Elle double le comptage (shared_ptr compte deja), donc elle oblige `copy_on_write` a ecrire ses propres copie/deplacement/affectation/destructeur pour tenir le second compteur — or le walk rejette precisement les types non-default (`detail::is_default_type`), et la classe ne survivrait qu'en s'appuyant davantage sur l'echappatoire `is_unsafe_sendable`. Elargir l'echappatoire pour reparer un manque de documentation est un mauvais echange, et cela ferait du composant le plus didactique du depot le plus artisanal.

4) Facultatif, ajouter une note dans tests/test_copy_on_write.cpp a cote de l'assertion existante sur `!is_synchronizable_v<cow<int>>`, qui est deja la charniere du raisonnement (c'est elle qui garantit que seule une COPIE traverse un thread, donc que le seul scenario a arbitrer est celui que le fence traite). Le message actuel ne dit pas que le fence en depend.
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Sonde `p1_traits.cpp` -- compile a 0, prouve que seule une COPIE peut traverser un thread (donc que le scenario analyse est bien le seul possible) :
  static_assert(!is_synchronizable_v<C>); static_assert(!is_synchronizable_v<const C>);
  static_assert(!is_sendable_v<C&>); static_assert(!is_sendable_v<C*>); static_assert(!is_sendable_v<const C&>);
  static_assert(is_sendable_v<C>); static_assert(is_sendable_v<const C>);
  struct Holder { C c; }; static_assert(!is_synchronizable_v<const Holder>);
  static_assert(!is_sendable_v<std::shared_ptr<C>>);
-> g++-16 -std=c++26 -freflection -fsyntax-only : exit 0.

Preuve de l'appui sur l'implementation, dans les entetes GCC 16.2 installes :
  .../include/c++/16/bits/shared_ptr_base.h:231-235 : _M_get_use_count() { // No memory barrier is used here so there is no synchronization with other threads.  auto __count = __atomic_load_n(&_M_use_count, __ATOMIC_RELAXED); }
  .../include/c++/16/bits/shared_ptr_base.h:384,427,436 : _M_release() { if (__gnu_cxx::__exchange_and_add_dispatch(&_M_use_count, -1) == 1) ... }
  .../include/c++/16/ext/atomicity.h:71 : __exchange_and_add(...) { return __atomic_fetch_add(__mem, __val, __ATOMIC_ACQ_REL); }
TSan n'est pas disponible avec g++-16 sur cette machine (arm64-darwin, symboles __tsan_* non definis a l'edition de liens), donc pas de preuve dynamique -- l'analyse ci-dessus conclut de toute facon a l'absence de course reelle sur cette implementation.
```

</details>

### COW-05 — Le constructeur variadique ne permet pas la construction par liste : cow<std::vector<int>> v{1, 2, 3} ne compile pas

**MINEUR** · API · `include/threadsafe/details/copy_on_write.h:21`

Le constructeur transmet ses arguments a `std::make_shared<T>(...)`, qui fait une initialisation PARENTHESEE : `::new (pv) T(args...)`. Un `std::initializer_list` ne se deduit jamais d'un paquet variadique, donc toutes les formes que l'utilisateur ecrit spontanement pour un conteneur echouent. Pour un composant de conference dont l'exemple canonique est `cow<std::vector<int>>` / `cow<std::string>`, le premier geste de l'utilisateur ne compile pas, avec un message illisible (`no matching function for call to 'std::vector<int>::vector(int, int, int)'`, puis une erreur dans `iterator_traits<int>`). Le contournement existe mais personne ne le devine : il faut ecrire `std::initializer_list<int>{1, 2, 3}` explicitement. A noter que la garde anti-copie (lignes 19-20), elle, a ete verifiee sur tous les cas demandes et fait exactement ce qu'il faut -- aucun probleme de ce cote.

**Code actuel**

```cpp
template <class... Args>
  requires std::constructible_from<T, Args...> &&
           (sizeof...(Args) != 1 ||
            (!std::same_as<std::remove_cvref_t<Args>, copy_on_write> && ...))
explicit copy_on_write(Args &&...args)
    : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La surcharge proposee par l'auditeur repare vector/string mais PAS map (deduction de `U` impossible depuis des accolades imbriquees). Rendre le parametre non deductible en le derivant de `T::value_type` :

```cpp
  template <class U = T>
    requires std::constructible_from<T, std::initializer_list<typename U::value_type> &>
  explicit copy_on_write(std::initializer_list<typename U::value_type> values)
      : ptr_(std::make_shared<T>(values)) {}
```

A inserer juste apres le constructeur variadique (copy_on_write.h:22). `U = T` par defaut et non deduit ; `typename U::value_type` est une substitution failure en contexte immediat pour un `T` scalaire, donc la surcharge disparait proprement pour `cow<int>`.

Verifie en compilation (exit 0) : `cow<std::vector<int>> v{1,2,3}`, `cow<std::string> s{'a','b','c'}`, `cow<std::map<int,std::string>> m{{1,"a"},{2,"b"}}`, `cow<std::unordered_map<int,std::string>> u{{1,"a"}}`, `cow<std::vector<std::vector<int>>> nested{{1,2},{3,4}}`, et sans regression : `cow<int> n{5}`, `cow<std::vector<int>> copy_of_v{v}`, `cow<std::vector<int>> sized(std::size_t{4}, 7)`, `cow<std::string> lit{"bonjour"}`, `cow<std::unique_ptr<int>> p{...}`. Garde anti-copie intacte : `copy_constructible<cow<int>>`, `constructible_from<cow<int>, cow<int>&/const&/&&>`, `constructible_from<cow<cow<int>>, cow<cow<int>>&>`, wrapping volontaire `constructible_from<cow<cow<int>>, cow<int>&>`, `!convertible_to<const char*, cow<std::string>>`, `!constructible_from<cow<int>, std::initializer_list<int>>`. Les 12 tests du repo compilent.

Le test de non-regression a ajouter dans tests/test_copy_on_write.cpp :

```cpp
static_assert(std::constructible_from<cow<std::vector<int>>, std::initializer_list<int>>
                  && std::constructible_from<cow<std::map<int, std::string>>,
                                             std::initializer_list<std::pair<const int, std::string>>>,
              "la construction par liste doit rester la voie naturelle pour un conteneur");
static_assert(!std::constructible_from<cow<int>, std::initializer_list<int>>,
              "et ne doit pas exister quand le T n'est pas un conteneur");
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

scratchpad/audit_cow/p4_ctor.cpp -DPROBE_INIT_LIST=1 :
```text
copy_on_write<std::vector<int>> v{1, 2, 3};
```
-> p4_ctor.cpp:38:44: error: no matching function for call to 'threadsafe::copy_on_write<std::vector<int> >::copy_on_write(<brace-enclosed initializer list>)'
```text
• error: no matching function for call to 'std::vector<int>::vector(int, int, int)'
  • error: no type named 'iterator_category' in 'struct std::iterator_traits<int>'
```
scratchpad/audit_cow/p5.cpp (exit 0) prouve le contournement et l'absence de la voie naturelle :
```text
static_assert(std::constructible_from<copy_on_write<std::vector<int>>, std::initializer_list<int>>);
static_assert(!std::constructible_from<copy_on_write<std::vector<int>>, int, int, int>);
```
Garde anti-copie verifiee sans defaut (p4_ctor.cpp, exit 0) : constructible_from<CI, CI&>, <CI, const CI&>, <CI, CI&&>, copie de cow<cow<int>> depuis cow<cow<int>>&, emballage volontaire cow<cow<int>> depuis cow<int>& et const cow<int>&, et slicing depuis une classe derivee -- tous corrects ; `copy_on_write<std::string> s = "bonjour";` est bien refuse par le explicit.

</details>

## `synchronized_value<T>`

### SV-01 — Impossible de verrouiller deux synchronized_value ensemble: le deadlock classique est atteignable et non detecte

**MAJEUR** · API · `include/threadsafe/details/synchronized_value.h:75`

`mutex_` est prive, le constructeur de `value_guard` est prive et n'a pour ami que `synchronized_value<T>`. Il n'existe aucune fonction libre `lock(a, b)` ni equivalent de `std::scoped_lock`. L'exemple canonique de tout cours de thread-safety — le virement entre deux comptes — se bloque donc en interblocage, et la bibliotheque, dont l'argument de vente est "difficile d'avoir des race conditions", ne dit rien a la compilation ni a l'execution. Un spectateur qui pose la question "et si je dois verrouiller deux valeurs?" met l'orateur en difficulte: la reponse actuelle est "tu ne peux pas le faire correctement".

> Défaut trouvé indépendamment par 2 auditeurs : *Aucun verrouillage atomique de plusieurs synchronized_value : le deadlock AB/BA s'écrit sans diagnostic*.

**Code actuel**

```cpp
  [[nodiscard]] guard lock() & { return guard{mutex_, value_}; }
  [[nodiscard]] const_guard lock_shared() const & {
    return const_guard{mutex_, value_};
  }
...
private:
  mutable mutex mutex_;
  T value_;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Diagnostic confirme, mais NE PAS appliquer la correction proposee telle quelle (elle reintroduit une data race via `decltype(auto)` et un UB d'aliasing). Version verifiee, dans include/threadsafe/details/synchronized_value.h.

Ajouter aux includes du fichier: `#include <cassert>`, `#include <cstddef>`, `#include <functional>`.

Dans la section privee de `synchronized_value` (avant `mutable mutex mutex_;`) — la signature doit etre REPETEE A L'IDENTIQUE, trailing return type compris, sinon l'amitie ne prend pas:

```cpp
private:
  template <class F, class... Ts>
  friend auto with_all(F action, synchronized_value<Ts> &...values)
      -> std::invoke_result_t<F, Ts &...>;

  mutable mutex mutex_;
  T value_;
};
```

Puis, apres la classe:

```cpp
template <class F, class... Ts>
auto with_all(F action, synchronized_value<Ts> &...values)
    -> std::invoke_result_t<F, Ts &...> {
  static_assert(
      !std::is_reference_v<std::invoke_result_t<F, Ts &...>>,
      "the action must not return a reference to a guarded value: the locks "
      "are released when with_all returns, so the reference would be a "
      "race waiting to happen — return by value");

  if constexpr (sizeof...(values) > 1) {
    const void *const addresses[] = {
        static_cast<const void *>(std::addressof(values))...};
    for (std::size_t left = 0; left < sizeof...(values); ++left)
      for (std::size_t right = left + 1; right < sizeof...(values); ++right)
        assert(addresses[left] != addresses[right] &&
               "with_all: the same synchronized_value was passed twice; "
               "locking one mutex twice is undefined behaviour");
  }

  std::scoped_lock all{values.mutex_...};
  return std::invoke(action, values.value_...);
}
```

Usage (l'exemple canonique du virement, sans interblocage possible):

```cpp
void transfer(Account &from, Account &to, int amount) {
  threadsafe::with_all([&](int &f, int &t) { f -= amount; t += amount; },
                       from, to);
}
```

Trois raisons de preferer cette forme pour une conference: `std::scoped_lock` delegue a `std::lock`, dont l'algorithme back-off rend l'ordre des arguments indifferent (le point pedagogique); la lambda borne la duree de vie des references, donc aucune garde a manipuler; et les deux pieges restants sont fermes a la compilation (reference echappee) et par assert (aliasing).

Verifications effectuees: g3_fix (20000 virements croises) exit=0 "no deadlock"; g6_escape rejete par le static_assert; g7_alias attrape par l'assert; les 12 tests de tests/ compilent (FAIL=0); traits de synchronized_value inchanges.

Note de perimetre: `with_all` ne couvre que le verrouillage exclusif. Un pendant partage (`with_all_shared` sur des `const synchronized_value<Ts>&`, en `std::shared_lock` quand `shared_readable`) n'est pas necessaire pour lever l'interblocage et peut etre laisse de cote pour garder l'exemple lisible.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
g_traps.cpp compile SANS aucun diagnostic avec -Wall -Wextra, y compris:

  void transfer(Account &from, Account &to, int amount) {
    auto from_guard = from.lock();
    auto to_guard = to.lock();
    *from_guard -= amount;
    *to_guard += amount;
  }

g2_deadlock.cpp (deux taches, transfer(alice,bob) et transfer(bob,alice)) compile et s'execute; tue apres 4 secondes: exit=137 (SIGKILL). Interblocage confirme, jamais atteint "no deadlock".
```

Contre-vérification indépendante :

=== g_traps.cpp (headers d'origine, -Wall -Wextra) ===
COMPILE_OK_NO_DIAGNOSTIC

=== g2_deadlock.cpp (headers d'origine) ===
BUILD_OK
exit=137          <- SIGKILL apres 4 s, "no deadlock" JAMAIS imprime. Interblocage confirme.

=== g4_noworkaround.cpp : probe A, atteindre mutex_ (headers d'origine) ===
g4_noworkaround.cpp:5:26: error: 'threadsafe::synchronized_value<int>::mutex threadsafe::synchronized_value<int>::mutex_' is private within this context
g4_noworkaround.cpp:5:36: error: 'threadsafe::synchronized_value<int>::mutex threadsafe::synchronized_value<int>::mutex_' is private within this context

=== g5_nodefer.cpp : probe B, differer/reordonner/deplacer les gardes (headers d'origine) ===
[…]

</details>

### SV-02 — Le constructeur variadique court-circuite le constructeur de copie et de déplacement supprimés

**MINEUR** · API · `include/threadsafe/details/synchronized_value.h:56`

`synchronized_value(const synchronized_value &) = delete;` (ligne 65) ne lie qu'une `const&`. Le constructeur variadique, lui, déduit `Args = synchronized_value&` (lvalue non const) ou `Args = synchronized_value` (rvalue) et gagne la résolution de surcharge par correspondance exacte. Dès que `std::constructible_from<T, synchronized_value&>` est vrai — un T avec constructeur de conversion glouton, vouché sendable — `sync_value b{a};` et `sync_value b{std::move(a)};` compilent, alors que le `= delete` annonce l'inverse. Portée réelle vérifiée : `std::any` ne déclenche pas le trou (il exige copy_constructible, que le wrapper n'est pas), donc seul un type utilisateur à ctor forwarding + vouché `is_unsafe_sendable` l'atteint. C'est donc étroit, mais c'est un invariant annoncé par un `= delete` explicite qui n'est pas tenu, dans du code qui sert de support pédagogique.

**Code actuel**

```cpp
template <class... Args>
  requires std::constructible_from<T, Args...>
explicit synchronized_value(Args &&...args)
    : value_(std::forward<Args>(args)...) {
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction de l'auteur ferme le constructeur mais laisse `make()` (lignes 68-69)
avec la meme clause non gardee, ou l'appel part en erreur profonde dans
`std::make_shared`. Il faut factoriser la garde et l'appliquer aux deux points
d'entree. Dans `include/threadsafe/details/synchronized_value.h` :

```cpp
  // A single argument that is itself a synchronized_value belongs to the
  // copy/move constructors below, not to this forwarding constructor.
  template <class... Args>
  static constexpr bool constructs_value =
      std::constructible_from<T, Args...> &&
      (sizeof...(Args) != 1 ||
       !(std::same_as<std::remove_cvref_t<Args>, synchronized_value> || ...));

  template <class... Args>
    requires constructs_value<Args...>
  explicit synchronized_value(Args &&...args)
      : value_(std::forward<Args>(args)...) {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses "
                  "thread boundaries — one thread at a time — so T must "
                  "be sendable");
  }

  synchronized_value(const synchronized_value &) = delete;
  synchronized_value &operator=(const synchronized_value &) = delete;

  template <class... Args>
    requires constructs_value<Args...>
  [[nodiscard]] static std::shared_ptr<synchronized_value>
  make(Args &&...args) {
    return std::make_shared<synchronized_value>(std::forward<Args>(args)...);
  }
```

`<concepts>` et `<type_traits>` sont deja inclus (lignes 3 et 7), aucun en-tete
a ajouter. `synchronized_value` designe ici le injected-class-name, donc
`synchronized_value<U>` avec `U != T` reste acceptable — verifie par
`can_construct<sync_sink, threadsafe::synchronized_value<int>&>` qui passe
toujours. Le fold vide (`sizeof...(Args) == 0`) vaut `false`, donc le
constructeur par defaut reste viable.

Verifie : p8_full2.cpp rc=0 contre cet arbre (include2/), et les 12 fichiers de
tests/ compilent sans regression.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Trou prouvé — scratchpad/audit_syncval/p5b.cpp, rc=0 :
  struct Sink { Sink() = default; template <class... U> Sink(U &&...) {} };
  template <> struct threadsafe::is_unsafe_sendable<Sink> : std::true_type {};
  using sync_sink = threadsafe::synchronized_value<Sink>;
  static_assert(can_construct<sync_sink, sync_sink &>, "copy hijacked");   // PASSE
  static_assert(can_construct<sync_sink, sync_sink>,  "move hijacked");    // PASSE
  static_assert(!can_construct<sync_sink, const sync_sink &>);            // le = delete gagne ici
  sync_sink moved{std::move(a)};  // compile
Contrôle négatif : `!can_construct<synchronized_value<int>, synchronized_value<int>&>` passe, et scratchpad/audit_syncval/p13_any.cpp montre que `std::any` n'est pas concerné.
Correction vérifiée : en-tête patché dans scratchpad/audit_syncval/patched/ ; `!can_construct<proto_sink, proto_sink&>` et `!can_construct<proto_sink, proto_sink>` passent (scratchpad/audit_syncval/p9_fixes.cpp, rc=0), et les 12 fichiers de tests/ compilent tous OK contre l'arbre patché.
```

</details>

### SV-03 — std::constructible_from ment sur synchronized_value<T> quand T n'est pas sendable

**MINEUR** · API · `include/threadsafe/details/synchronized_value.h:59`

Le `static_assert(sendable<T>)` est dans le CORPS du constructeur : il se déclenche à l'instanciation de la définition, pas à la résolution de surcharge. Donc `std::constructible_from<synchronized_value<NonSendable>, NonSendable>` et `std::default_initializable<...>` répondent VRAI alors que toute construction réelle est une erreur dure. Du code générique SFINAE (sélection de surcharge, `std::optional`/conteneur qui teste avant d'agir) choisira donc une branche qui explose ensuite. Point positif à noter : j'ai cherché un chemin qui crée une instance SANS passer par ce constructeur et je n'en ai trouvé AUCUN — l'assert est efficace sur les 8 chemins testés. Et le type reste bien complétable sans instance (membre de classe, union, classe dérivée, std::vector<T> vide), conformément à CLAUDE.md.

> Défaut trouvé indépendamment par 2 auditeurs : *synchronized_value<T> se construit avec un T non sendable : meme trou, meme non-testabilite*.

**Code actuel**

```cpp
static_assert(sendable<T>,
              "the mutex serializes access, but the T still crosses "
              "thread boundaries — one thread at a time — so T must "
              "be sendable");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Remplacer synchronized_value.h:55-63 par l'idiome de surcharge supprimee deja utilise par `value_guard` dans le meme fichier (lignes 23-28) :

```cpp
  template <class... Args>
    requires std::constructible_from<T, Args...> && sendable<T>
  explicit synchronized_value(Args &&...args)
      : value_(std::forward<Args>(args)...) {}

  template <class... Args>
    requires std::constructible_from<T, Args...> && (!sendable<T>)
  explicit synchronized_value(Args &&...args) = delete (
      "the mutex serializes access, but the T still crosses "
      "thread boundaries — one thread at a time — so T must "
      "be sendable");
```

Ne PAS toucher a la requires-clause de `make()` (ligne 69) : la laisser telle quelle preserve le message pedagogique pour `synchronized_value<NonSendable>::make()`, alors qu'y ajouter `&& sendable<T>` le remplace par « no matching function ».

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Le mensonge du trait — scratchpad/audit_syncval/p11_constructible_lie.cpp, rc=0 :
```text
static_assert(!threadsafe::is_sendable_v<NonSendable>);
static_assert(std::constructible_from<bad, NonSendable>);
static_assert(std::default_initializable<bad>);
```
Absence de chemin de contournement — scratchpad/audit_syncval/p8_instance_attempts.cpp compilé 8 fois (-DATTEMPT=1..8) : `bad value;`, `bad::make()`, `std::make_shared<bad>()`, `bad array[2]`, `vector::emplace_back()`, `optional::emplace()`, classe dérivée à ctor hérités, `new bad()` — les 8 déclenchent « must be sendable ».
Complétabilité sans instance — scratchpad/audit_syncval/p2_nonsendable_paths.cpp et p7_no_instance.cpp, rc=0.
Échec de l'idiome à deux surcharges : scratchpad/audit_syncval/patched2 (contraintes dupliquées) => `std::constructible_from<synchronized_value<int>, int>` devient FAUX ; scratchpad/audit_syncval/patched3 (concept nommé `initializes_value`) => les 12 tests passent mais `constructible_from<bad, NonSendable>` reste VRAI.

</details>

### SV-04 — L'idiome d'une ligne *sv.lock() est interdit meme en lecture pure, alors que la vraie fuite de reference passe sans un mot

**MINEUR** · API · `include/threadsafe/details/synchronized_value.h:23`

Les surcharges rvalue de `operator*` et `operator->` sont supprimees, ce qui bannit `++*counter->lock();` et meme `std::println("{}", *counter->lock_shared());` — deux expressions parfaitement sures, la garde vivant jusqu'a la fin de la full-expression. C'est l'idiome standard (Rust: `*counter.lock().unwrap() += 1;`). Chaque acces coute donc deux lignes et une variable nommee. Et la protection est illusoire: des que l'utilisateur obeit et nomme sa garde, il peut extraire la reference et la faire survivre au verrou, ce qui compile en silence sous -Wall -Wextra. La bibliotheque interdit donc le cas sur et laisse passer le cas dangereux — un public technique le remarquera.

**Code actuel**

```cpp
T &operator*() && noexcept = delete (
    "a temporary guard is destroyed at the semicolon, so it cannot "
    "hand out a reference");
T *operator->() && noexcept = delete (
    "a temporary guard is destroyed at the semicolon, so it cannot "
    "hand out a reference");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La suppression des surcharges rvalue est a GARDER : ma sonde p4 confirme qu'elle intercepte bien `auto &dangling = *counter->lock();`, qui est un vrai dangling. Le manque est l'absence de forme une-ligne, et le helper doit refuser de rendre une reference — sinon il ne vaut pas mieux que la garde nommee.

A ajouter dans `synchronized_value` (include/threadsafe/details/synchronized_value.h, juste avant `lock() && = delete`) :

```cpp
template <class F>
  requires std::invocable<F &, T &>
std::invoke_result_t<F &, T &> with_lock(F action) & {
  static_assert(!std::is_reference_v<std::invoke_result_t<F &, T &>>,
                "the action must return a value: a reference would outlive "
                "the lock that guards it");
  auto held = lock();
  return action(*held);
}

template <class F>
  requires std::invocable<F &, const T &>
std::invoke_result_t<F &, const T &> with_lock_shared(F action) const & {
  static_assert(!std::is_reference_v<std::invoke_result_t<F &, const T &>>,
                "the action must return a value: a reference would outlive "
                "the lock that guards it");
  auto held = lock_shared();
  return action(*held);
}
```
Aucun include a ajouter : `<concepts>` et `<type_traits>` sont deja la (lignes 3 et 7).

Difference decisive avec la version proposee par l'auditeur : le type de retour est EPINGLE a `std::invoke_result_t` et un `static_assert` interdit qu'il soit une reference. La version a `decltype(auto)` laissait passer silencieusement `with_lock([](int &v) -> int & { return v; })` (verifie : compile, s'execute, fuit). Ici le meme lambda est rejete a la compilation avec le message ci-dessus.

Usage retrouve, en une ligne :
```cpp
counter->with_lock([](int &value) { ++value; });
auto snapshot = counter->with_lock_shared([](const int &value) { return value; });
```
Verifie : les deux usages compilent et s'executent, le lambda fuyant est rejete, et les 12 tests existants compilent sans modification.

Note pedagogique a ne pas survendre sur scene : ce helper ferme la fuite par REFERENCE, pas toute fuite (un lambda peut encore rendre un pointeur ou un reference_wrapper vers la valeur). C'est une garantie honnete et bon marche, pas une preuve — et c'est precisement la nuance que la correction initiale escamotait.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

a_counter.cpp refuse par g++-16:
```text
error: use of deleted function 'T& threadsafe::value_guard<T, Lock>::operator*() && [with T = int; Lock = std::unique_lock<std::shared_mutex>]': a temporary guard is destroyed at the semicolon, so it cannot hand out a reference
   14 |             ++*shared->lock();
```
a_counter2.cpp (lecture pure) refuse aussi:
```text
error: use of deleted function 'T& threadsafe::value_guard<T, Lock>::operator*() && [with T = const int; Lock = std::shared_lock<std::shared_mutex>]'
   23 |   std::println("{}", *counter->lock_shared());
```
g_traps.cpp compile et s'execute SANS diagnostic (-Wall -Wextra):
```text
int &escape(Account &account) { auto guard = account.lock(); return *guard; }
```
Sortie: "escaped reference reads 7 with no lock held"

</details>

## `asynchronous_task_launcher`

### TASK-01 — launchable_task n'exige pas l'invocabilité : l'erreur la plus fréquente sort de <thread>, pas du launcher

**MINEUR** · API · `include/threadsafe/details/asynchronous_task_launcher.h:41`

launchable_task / launchable_scoped_task ne vérifient que la participation des types, jamais que F est appelable avec Args.... Un oubli d'argument ou un mauvais type satisfait donc la contrainte, la surcharge contrainte est élue, et l'erreur remonte de std::jthread::_S_create, cinq cadres d'instanciation plus bas, avec une trace vector/alloc_traits/construct_at. Toute la machinerie de diagnostic (les deux surcharges de secours) est contournée précisément dans le cas d'erreur le plus courant. Au passage, cela rend les tests trompeurs : launchable_task<decltype([](SyncCounter&){}), std::reference_wrapper<SyncCounter>> ne teste en réalité que l'argument, le callable n'est jamais confronté à lui.

> Défaut trouvé indépendamment par 2 auditeurs : *launchable_task n'exige pas std::invocable : un couple accepte peut exploser dans <thread>*.

**Code actuel**

```cpp
template <class F, class... Args>
concept launchable_task = task_participant<F> && (task_participant<Args> && ...);

template <class F, class... Args>
concept launchable_scoped_task = scoped_task_participant<F>
                              && (scoped_task_participant<Args> && ...);
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee par le finding est correcte et complete telle quelle ; je l'ai compilee integralement (12/12 tests OK). Je la reproduis avec les emplacements exacts.

Dans include/threadsafe/details/asynchronous_task_launcher.h, remplacer les lignes 41-46 par :

```cpp
template <class F, class... Args>
concept invocable_as_task = std::invocable<F, Args...>
                         || std::invocable<F, std::stop_token, Args...>;

template <class F, class... Args>
concept launchable_task = task_participant<F>
                       && (task_participant<Args> && ...)
                       && invocable_as_task<F, Args...>;

template <class F, class... Args>
concept launchable_scoped_task = scoped_task_participant<F>
                              && (scoped_task_participant<Args> && ...)
                              && invocable_as_task<F, Args...>;
```
La seconde alternative reproduit l'injection du stop_token par std::jthread ; sans elle, `[](std::stop_token, int){}` est rejete a tort (verifie : 4 erreurs).

Puis, OBLIGATOIREMENT, ajouter la meme condition aux DEUX surcharges de secours, sinon celles-ci sont elues, leurs static_assert passent, et l'appel compile en ne faisant rien (verifie).

Dans le corps de la surcharge de secours `launch_task(F, Args...)` (apres le static_assert existant sur les Args, ligne 67) :

```cpp
static_assert(invocable_as_task<F, Args...>,
              "the callable must be invocable with those arguments");
```
Dans le corps de la surcharge de secours `launch_scoped_task(F, Args...)` (apres le static_assert existant sur les Args, ligne 82) :

```cpp
static_assert(invocable_as_task<F, Args...>,
              "the callable must be invocable with those arguments");
```
Deux remarques de mise en oeuvre, non bloquantes :
- `<concepts>` est deja inclus (ligne 3) et `<stop_token>` aussi (ligne 4) : aucun include a ajouter.
- `invocable_as_task` est place dans le namespace `threadsafe` public, par coherence avec `launchable_task` qui l'est deja ; le mettre dans `detail` marcherait aussi mais rendrait le message de contrainte moins lisible pour l'auditoire de la conference.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `e2.cpp` : launcher.launch_task([](int value){ (void)value; });  // argument oublié
Avec le header actuel : 49 lignes de diagnostic, l'erreur vient de /opt/homebrew/.../c++/16/thread:274 « std::jthread arguments must be invocable after conversion to rvalues », via 5 cadres « required from » (jthread::_S_create → jthread::jthread → construct_at → allocator_traits::construct → vector::emplace_back → launcher.h:57).
Avec le correctif (copie inc/) : « asynchronous_task_launcher.h:52: error: static assertion failed: the callable must be invocable with those arguments », et les 12 fichiers de tests/ compilent toujours (12/12 OK).

</details>

### TASK-02 — Aucune lambda capturante n'est acceptée, et le message d'erreur accuse le mauvais coupable

**MINEUR** · flexibilité · `include/threadsafe/details/asynchronous_task_launcher.h:77`

nonstatic_data_members_of renvoie 0 membre pour un type de fermeture, quelles que soient ses captures : has_unreflectable_state est donc vrai, is_walkable_type faux, et is_sendable / is_lifetime_aware répondent faux pour TOUTE lambda capturante. C'est conservateur donc sûr, mais cela vide le launcher de son ergonomie : on ne peut lui passer qu'une lambda sans capture ou un foncteur nommé. Pire, l'asymétrie est invisible : le foncteur écrit à la main avec un membre std::atomic<int>& est accepté, alors que la lambda [&counter] strictement équivalente est refusée avec le message « the callable must be movable and sendable » — qui laisse croire que c'est l'atomic qui pose problème. C'est le tout premier mur que rencontrera quelqu'un qui essaie la bibliothèque après la conférence (le walk le sait déjà, cf. tests/test_soundness_regressions.cpp:162, mais le launcher ne le dit pas au point d'usage).

> Défaut trouvé indépendamment par 2 auditeurs : *Toute lambda capturante est refusee, meme quand ce qu'elle capture est sendable*.

**Code actuel**

```cpp
template <typename F, typename... Args>
void launch_scoped_task(F, Args...) {
    static_assert(scoped_task_participant<F>,
                  "the callable must be movable and sendable");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Garder l'idee (nommer la vraie cause au point d'usage) mais proteger l'appel reflectif, sinon toute F non-classe declenche une `std::meta::exception` a la compilation.

Dans `include/threadsafe/details/asynchronous_task_launcher.h`, ajouter `#include <threadsafe/details/utils.h>`, puis dans `launch_scoped_task` (et a l'identique dans `launch_task`, avec le message generique correspondant):

```cpp
static_assert(scoped_task_participant<F>
                  || !std::is_class_v<F>
                  || !detail::has_unreflectable_state(^^F),
              "a lambda's captures are invisible to reflection: pass "
              "the state as arguments, or use a named function object "
              "whose members the traits can walk");
static_assert(scoped_task_participant<F>,
              "the callable must be movable and sendable");
```
La garde `!std::is_class_v<F>` est indispensable: sans elle, `has_unreflectable_state(^^F)` sur `F = Bad*` (ou tout pointeur/fonction non sendable) produit « non-constant condition for static assertion » + « uncaught exception of type 'std::meta::exception': not a complete class type ».

Vu la vocation educative, une alternative plus simple et sans nouvelle dependance serait de ne rien ajouter au launcher et d'enrichir le texte existant: "the callable must be movable and sendable (a capturing lambda never is: its captures are invisible to reflection — use a named function object, or pass the state as arguments)". Zero code reflectif, zero risque de regression, meme benefice pedagogique.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p2.cpp`, exécutée : pour cap_int, cap_refint, cap_refsync, cap_shared, cap_ptr, cap_uniq, cap_this → « send=false life=false members=0 empty=false default=true walkable=false ». Donc même [p = std::make_shared<SyncCounter>()] est refusé.
Sonde `p7.cpp`, compilée et exécutée : struct Job { std::atomic<int>& counter; void operator()() const { counter.fetch_add(1); } }; → static_assert(is_scoped_task_participant_v<Job>) PASSE et launcher.launch_scoped_task(Job{counter}) affiche counter=1, tandis que la sonde e1.cpp avec launch_scoped_task([&counter]{ counter.fetch_add(1); }) échoue sur « static assertion failed: the callable must be movable and sendable ».
Correctif vérifié (copie inc3/) : la lambda capturante déclenche bien le nouveau message, NotSafe (copie utilisateur) ne déclenche que le message générique.

</details>

### TASK-03 — launch_scoped_task ne lance rien: il execute les taches en serie

**MINEUR** · API · `include/threadsafe/details/asynchronous_task_launcher.h:72`

`launch_scoped_task` cree un jthread puis le join IMMEDIATEMENT dans la meme expression. Il n'y a donc aucun parallelisme: N appels s'executent l'un apres l'autre. Le nom dit "launch", la classe s'appelle `asynchronous_task_launcher`, et la semantique est un appel synchrone avec un aller-retour de thread en prime. C'est le seul chemin qui accepte des types non lifetime-aware (le "scoped" de la bibliotheque), donc un utilisateur qui veut emprunter de l'etat local croit obtenir de la concurrence bornee par la portee (facon std::thread scope / structured concurrency) et obtient du code sequentiel plus lent que sans threads. En conference, montrer `launch_scoped_task` sur 4 taches puis mesurer 830ms au lieu de 200ms est une demonstration qui se retourne contre l'orateur.

**Code actuel**

```cpp
template <typename F, typename... Args>
    requires launchable_scoped_task<F, Args...>
void launch_scoped_task(F f, Args... args) {
    std::jthread task{std::move(f), std::move(args)...};
    task.join();
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

NE PAS appliquer l'option (2) de l'auditeur: son `scoped_task_group` est un trou de
soundness demontre (probeB). Garder `launch_scoped_task` tel quel — son join immediat
est le mecanisme qui autorise l'abandon de `is_lifetime_aware`.

Ajouter a la place un lot qui lance tout puis joint tout AVANT de rendre la main.
L'invariant "aucune tache ne survit a l'appel" est conserve a l'identique, et on
obtient le vrai parallelisme. Dans include/threadsafe/details/asynchronous_task_launcher.h,
apres launch_scoped_task (ligne 75):

```cpp
// Runs every task concurrently and joins them all before returning, so the
// borrowing guarantee is unchanged: no task outlives this call.
template <typename... Fs>
    requires (launchable_scoped_task<Fs> && ...)
void launch_scoped_tasks(Fs... tasks) {
    std::vector<std::jthread> running;
    running.reserve(sizeof...(Fs));
    (running.emplace_back(std::move(tasks)), ...);
}
```
Mesure: 835ms -> 210ms sur les 4 taches de 200ms, counter=4 (donc bien joint avant
retour). Les 12 tests du repo compilent sans modification.

L'emprunt se fait via le motif deja beni par les tests (lignes 54-56), un functor
portant un reference_wrapper — PAS une lambda a capture par reference, qui est
correctement rejetee sur un local:

```cpp
struct Worker {
    std::reference_wrapper<SyncCounter> borrowed;
    void operator()() const { borrowed.get().counter.fetch_add(1); }
};
launcher.launch_scoped_tasks(work, work, work, work);
```
Test a ajouter dans tests/test_asynchronous_task_launcher.cpp:

```cpp
namespace { struct Borrower {
    std::reference_wrapper<SyncCounter> borrowed;
    void operator()() const {}
}; }
static_assert(launchable_scoped_task<Borrower>,
              "launch_scoped_tasks — a functor borrowing a synchronizable "
              "object runs in parallel, the launcher joins them all");
```
Sur le nommage, l'option (1) de l'auditeur reste pertinente et peu couteuse: la
doc de `launch_scoped_task` devrait dire explicitement "execute la tache sur un
autre thread et ATTEND son retour — c'est cette attente qui autorise l'emprunt".
Une ligne de commentaire au-dessus de la ligne 70 suffit et desamorce le piege de
conference sans changer une ligne de semantique.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

b2_scoped.cpp compile et s'execute:

```text
for (int worker = 0; worker < 4; ++worker)
  launcher.launch_scoped_task(
      [] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); });
```
Sortie reelle: "4 scoped tasks of 200ms each took 830ms"
(200ms x 4 = execution strictement sequentielle; du parallelisme donnerait ~210ms)

</details>

### TASK-04 — Les messages d'erreur ne nomment jamais le coupable: les fonctions diagnose_* ne diagnostiquent rien

**MINEUR** · API · `include/threadsafe/details/asynchronous_task_launcher.h:62`

Toutes les erreurs du launcher font 18 a 20 lignes et se terminent sur "the expression 'is_task_participant_v<T> [with T = FileHandle]' evaluated to 'false'". L'utilisateur apprend QUE c'est faux, jamais POURQUOI ni QUEL membre a echoue. Pour un `struct Outer { Middle middle; int ok; }` dont le `int*` est enfoui trois niveaux plus bas, il doit bissecter a la main avec des static_assert — et cette bissection MEME est ce qui casse le vouch ulterieur (voir finding sur l'ordre des specialisations). Le cas du destructeur utilisateur est le pire: `is_default_type` refuse silencieusement, l'utilisateur ne peut pas deviner que le probleme est son `~FileHandle()` et non un membre. Pour une bibliotheque educative dont les fonctions internes s'appellent deja `diagnose_is_sendable`, c'est le trou d'API le plus grave.

**Code actuel**

```cpp
template <typename F, typename... Args>
void launch_task(F, Args...) {
    static_assert(task_participant<F>,
                  "the callable must be movable, sendable and "
                  "lifetime-aware");
    static_assert((task_participant<Args> && ...),
                  "every argument must be movable, sendable and "
                  "lifetime-aware");
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Deux corrections a la proposition. (A) Le prototype doit consulter les familles vouch-ees conditionnellement AVANT `is_default_type`, sinon il dit « vouch-e ton std::vector<int*> » — un conseil qui percerait la soundness. (B) Il vaut mieux le cabler dans le launcher que d'en faire un opt-in, sinon l'utilisateur doit deja savoir que l'aide existe au moment ou il est bloque.

include/threadsafe/details/explain.h (nouveau, teste, 11/11 tests verts) :

  #pragma once
  #include <meta>
  #include <string>
  #include <threadsafe/details/allowed_std_wrappers.h>
  #include <threadsafe/details/sendable.h>
  #include <threadsafe/details/smart_pointers.h>
  #include <threadsafe/details/utils.h>

  namespace threadsafe {
  namespace detail {

  consteval std::string explain_not_sendable(std::meta::info type);
  consteval std::string explain_not_synchronizable(std::meta::info type);

  inline consteval std::string name_of(std::meta::info type) {
    return std::string{display_string_of(type)};
  }

  inline consteval std::string explain_not_synchronizable(std::meta::info type) {
    const std::string name = name_of(type);
    if (is_synchronizable_type(type)) return name + ": synchronizable";
    if (!is_const(type) && !is_array_type(type) && !is_function_type(type))
      return name + " is not const, and only a const (or internally "
                    "synchronised, e.g. std::atomic) value may be shared";
    return name + ": not synchronizable";
  }

  // LA correction : les familles vouch-ees conditionnellement d'abord.
  inline consteval std::string vouched_wrapper_failed(std::meta::info type,
                                                      const std::string &name) {
    if (is_smart_pointer_type(type)) {
      const auto pointee = remove_cv(template_arguments_of(dealias(type))[0]);
      if (template_of(dealias(type)) == ^^std::unique_ptr)
        return name + " owns its pointee, which must itself be sendable; " +
               explain_not_sendable(pointee);
      return name + " shares its pointee, which must be synchronizable "
                    "(usable by several threads at once); " +
             explain_not_synchronizable(pointee);
    }
    for (auto wrapped : wrapped_types_of(type))
      if (!is_sendable_type(wrapped))
        return name + " -> element " + explain_not_sendable(wrapped);
    return name + ": the vouch is_unsafe_sendable<" + name + "> evaluated to false";
  }

  inline consteval std::string explain_not_sendable(std::meta::info type) {
    const std::string name = name_of(type);
    if (is_sendable_type(type)) return name + ": sendable";
    if (const auto unqualified = remove_cv(type); unqualified != type)
      return name + " -> " + explain_not_sendable(unqualified);
    if (is_pointer_type(type) || is_lvalue_reference_type(type))
      return name + ": an indirection is sendable only if its target is "
                    "synchronizable; " +
             explain_not_synchronizable(
                 remove_cv(remove_pointer(remove_reference(type))));
    if (is_array_type(type))
      return name + " -> " + explain_not_sendable(remove_all_extents(type));
    if (!is_class_type(type) && !is_union_type(type))
      return name + ": neither a class nor a sendable scalar";
    if (is_smart_pointer_type(type) || is_allowed_std_wrapper(type))
      return vouched_wrapper_failed(type, name);   // <-- AVANT is_default_type
    if (!is_default_type(type))
      return name + ": has a user-written copy/move/destructor (or a "
                    "constructor template that could hijack them), so the "
                    "member walk cannot be trusted; vouch with "
                    "is_unsafe_sendable<" + name + ">";
    if (has_unreflectable_state(type))
      return name + ": not empty but exposes no reflectable member";
    const auto context = std::meta::access_context::unchecked();
    for (auto base : bases_of(type, context))
      if (!is_sendable_type(type_of(base)))
        return name + " -> base " + explain_not_sendable(type_of(base));
    for (auto member : nonstatic_data_members_of(type, context))
      if (!is_sendable_type(remove_cv(type_of(member))))
        return name + " -> member " + std::string{identifier_of(member)} +
               " of type " + explain_not_sendable(remove_cv(type_of(member)));
    return name + ": unexplained";
  }

  template <class T> void assert_task_participant() {
      static_assert(is_sendable_v<T>, std::string_view{why_not_sendable<T>()});
      static_assert(std::move_constructible<T>, "must be move-constructible");
      static_assert(is_lifetime_aware_v<T>, "must own its data (lifetime-aware)");
  }

  } // namespace detail

  template <class T> consteval auto why_not_sendable() {
    return define_static_string(detail::explain_not_sendable(^^T));
  }
  } // namespace threadsafe

Et asynchronous_task_launcher.h:60-68 devient :

    template <typename F, typename... Args>
    void launch_task(F, Args...) {
        detail::assert_task_participant<F>();
        (detail::assert_task_participant<Args>(), ...);
    }

Notes de rigueur (mesurees, pas supposees) :
- `define_static_string` est dans `std`, PAS `std::meta`, sous GCC 16 : le code du finding ne compile pas tel quel.
- `explain_not_sendable(^^T)` sur un `T` sendable rend « T: sendable » ; l'aide n'est appelable que sur le chemin d'echec (elle l'est ici, puisque le `static_assert` ne la declenche que quand il echoue).
- Un membre `void*` fait remonter d'abord les `static_assert` de `assert_queryable_type` (utils.h:13/16) : deux erreurs parasites avant le message utile. C'est inherent au fait que `explain_*` rejoue le walk via `is_sendable_type` ; acceptable, mais a mentionner si on en fait une slide.
- La memoisation par `_v` est intacte : `explain.h` rejoue le walk sans instrumenter `diagnose_is_sendable`, et le chemin normal ne paie rien puisque le message n'est evalue qu'en cas d'echec.
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

f_why.cpp compile et produit exactement:

```text
error: static assertion failed: Outer -> member middle of type Middle -> member leaf of type Leaf -> member borrowed of type int*: an indirection is sendable only if its target is synchronizable (shareable by several threads at once); int is not

error: static assertion failed: CustomDtor: has a user-written copy/move/destructor (or a constructor template that could hijack them), so the member walk cannot be trusted; vouch with is_unsafe_sendable<CustomDtor>
```
A comparer avec la sortie ACTUELLE de d_handle.cpp (20 lignes, g++-16), dont le seul contenu utile est:
```text
error: static assertion failed: every argument must be movable, sendable and lifetime-aware
  • the expression 'is_task_participant_v<T> [with T = FileHandle]' evaluated to 'false'
```

</details>

### TASK-05 — Aucun moyen de recuperer une valeur de retour ni d'attendre les taches sans detruire le launcher

**MINEUR** · flexibilité · `include/threadsafe/details/asynchronous_task_launcher.h:56`

`launch_task` retourne void et `asynchronous_task_launcher` n'a ni `join()` ni `wait()`. Pour recuperer un seul resultat, l'utilisateur doit (1) creer un `synchronized_value<std::vector<T>>` puits, (2) le faire passer par shared_ptr dans chaque tache, (3) ouvrir une portee artificielle `{ ... }` pour forcer le join avant de lire. Le programme "calcule ces 4 sommes partielles et rends-les moi" fait 26 lignes. Pour un talk, cela rend impossible la demo la plus convaincante ("regarde, le parallelisme sans data race, et je recupere mon resultat") sans d'abord expliquer le puits partage.

**Code actuel**

```cpp
    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    void launch_task(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }
...
private:
    std::vector<std::jthread> threads_;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee par l'auditeur est a rejeter telle quelle : elle oublie `#include <future>` et elle explose en erreur dure sur toute tache retournant `void`. Voici la version que j'ai reellement compilee, executee, et passee sur les 12 tests existants.

Dans `include/threadsafe/details/asynchronous_task_launcher.h`, ajouter aux includes (lignes 3-7) :

```cpp
#include <future>
#include <type_traits>
```

Ajouter, juste avant `class asynchronous_task_launcher` (ligne 48), le concept qui ferme le trou `void` — la disjonction court-circuite en normalisation de contrainte, donc `is_sendable_v<void>` n'est jamais instancie et le `static_assert` de `assert_queryable_type` ne se declenche pas :

```cpp
template <class T>
concept sendable_result = std::is_void_v<T> || sendable<T>;
```

Puis, dans la section publique, juste avant `private:` (ligne 85) :

```cpp
    void join() {
        for (std::jthread &task : threads_) {
            if (task.joinable()) {
                task.join();
            }
        }
        threads_.clear();
    }

    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
              && sendable_result<std::invoke_result_t<F, Args...>>
    [[nodiscard]] std::future<std::invoke_result_t<F, Args...>>
    launch_task_with_result(F f, Args... args) {
        using result_type = std::invoke_result_t<F, Args...>;
        std::packaged_task<result_type(Args...)> task{std::move(f)};
        std::future<result_type> result = task.get_future();
        threads_.emplace_back(std::move(task), std::move(args)...);
        return result;
    }
```

Trois ecarts deliberes par rapport a la proposition d'origine :
1. `join()` plutot que `wait()` : c'est un join de threads, pas une attente de future ; `wait()` prete a confusion avec `std::future::wait()` juste a cote.
2. `sendable_result` au lieu de `sendable` direct : sans lui, `launch_task_with_result([]{ ... })` (le cas le plus courant) est une erreur de compilation non recuperable, pas une surcharge ecartee.
3. Nommage explicite (`result_type`, `result`, `task`) conforme a la regle CLAUDE.md « toujours des noms de variables explicites ».

Complement recommande mais non inclus dans la verification minimale : ajouter la surcharge-diagnostic miroir de celle des lignes 60-68, sinon un type refuse donne un `no matching function for call to ...` brut au lieu du message pedagogique maison :

```cpp
    template <typename F, typename... Args>
    void launch_task_with_result(F, Args...) {
        static_assert(task_participant<F>,
                      "the callable must be movable, sendable and "
                      "lifetime-aware");
        static_assert((task_participant<Args> && ...),
                      "every argument must be movable, sendable and "
                      "lifetime-aware");
        static_assert(sendable_result<std::invoke_result_t<F, Args...>>,
                      "the result crosses a thread boundary too, so it must "
                      "be sendable");
    }
```

Fichiers de verification : verify_launcher_join_result/ (`include/` = correction proposee verbatim, `include2/` = correction corrigee). Aucun fichier du repo n'a ete modifie.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

p_join.cpp:
```text
launcher.join();
```
Sortie g++-16:
```text
error: 'class threadsafe::asynchronous_task_launcher' has no member named 'join'
```
j_result.cpp (le contournement complet) compile et s'execute ("4 partial results") mais fait 26 lignes pour 4 sommes partielles, dont une portee `{ }` artificielle et un `synchronized_value<std::vector<long>>` puits.

</details>

## En-tête public et documentation

### DOC-01 — CLAUDE.md:98 — l'identité is_sendable<T&> = is_synchronizable<T> est fausse dès que T est cv-qualifié

**MINEUR** · API · `CLAUDE.md:98`

La doc énonce `is_sendable<T&> = is_sendable<T*> = is_synchronizable<T>`. Le code, lui, enlève le cv du référent avant de poser la question (sendable.h:51 : `is_synchronizable_type(remove_cv(remove_reference(type)))`). La règle réelle est donc `is_sendable<T&> == is_synchronizable<std::remove_cv_t<T>>`. L'écart n'est pas anecdotique : `is_synchronizable_v<const std::string>` est vrai mais `is_sendable_v<const std::string&>` est faux, `is_sendable_v<const char*>` est faux, et plus généralement l'analogie Rust annoncée (« T: Sync ⟺ &T: Send ») ne tient pas — un type prouvé partageable en lecture ne peut pas être passé par référence const à un thread. Le code est intentionnel (le retrait du remove_cv casse trois static_assert explicitement motivés) : c'est la phrase de doc qui est à corriger, pas la ligne 51.

**Code actuel**

```cpp
- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<T>`
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposée énonce une règle qui reste falsifiable (sonde p2 : `is_synchronizable_v<PolyVouched>` vrai, `is_sendable_v<PolyVouched&>` faux, à cause de `sendable.h:47` que la formule ignore). Remplacer `CLAUDE.md:98` par :

```markdown
- `is_sendable<T&>` = `is_sendable<T*>` = `is_synchronizable<std::remove_cv_t<T>>`,
  **et seulement si le type dynamique de `T` est connu** (non polymorphe, ou `final`) :
  derrière une indirection, une base polymorphe non `final` peut cacher n'importe quel
  dérivé, dont on ne sait rien.

  Le `const` du référent est retiré avant de poser la question, parce qu'il ne décrit
  que *cette* vue : un autre alias non const peut écrire le même objet. Un `const T`
  prouvé partageable en lecture n'est donc pas pour autant envoyable par `const T&` —
  l'analogie Rust « `T: Sync` ⟺ `&T: Send` » ne tient que parce que Rust, lui,
  garantit l'absence d'alias mutable concurrent, ce que C++ ne fait pas. Pour envoyer,
  il faut posséder la donnée (`copy_on_write`, `synchronized_value`) ou voucher
  `is_unsafe_synchronizable<T>`.
```

Modification purement documentaire : aucun header touché, les 12 tests continuent de compiler (baseline vérifiée propre ci-dessus). Les trois branches de la règle sont couvertes par mes sondes : le retrait du cv par p1, le garde-fou dynamique par p2, et les cas positifs (`PolyFinalVouched&`, `const std::atomic<int>&`, `void(&)()`) par p3.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p1.cpp`, compilée sans erreur : `is_synchronizable_v<const std::string>` ET `!is_sendable_v<const std::string&>` ET `!is_sendable_v<const std::string*>` ET `!is_sendable_v<const char*>`.

Que le code soit l'intention et la doc l'erreur est prouvé par la copie patchée .../audit_sync/inc2 (ligne 51 sans `remove_cv`) : test_polymorphic.cpp:56, test_polymorphic.cpp:81 et test_sendable.cpp:154 échouent alors, avec les messages « final settles the dynamic type, but the const is only a view » et « const behind an indirection is never trusted ».

</details>

### DOC-02 — Une specialisation is_unsafe_* en false_type REVOQUE la confiance accordee par la bibliotheque, contrairement a la doc — et un test l'affirme comme voulu

**MINEUR** · API · `CLAUDE.md:58`

CLAUDE.md:58-60 revendique : "Une specialisation ne peut qu'ACCORDER la confiance ; elle ne peut pas forcer un non — `false` et 'aucune revendication' sont la meme chose. Il n'y a pas de troisieme etat." C'est faux des que la confiance vient elle-meme d'une specialisation partielle : une specialisation TOTALE ecrite par l'utilisateur l'emporte, et le `false_type` supprime le vouch de la bibliotheque. Un utilisateur peut ainsi rendre synchronized_value<int> non-synchronizable. Pire : tests/test_containers.cpp:54 fait exactement cela sur std::vector<OptedOut> et l'ASSERTE (ligne 162) comme comportement voulu ("a full specialization outranks the std-wrapper rule"). La documentation et les tests se contredisent donc sur l'invariant central du modele de confiance — sur du materiel pedagogique de conference. La direction est sure (plus conservateur), mais l'enonce enseigne est faux.

**Code actuel**

```cpp
CLAUDE.md:58 — "The unsafe primaries derive from `std::false_type`. A specialization can only
*grant* trust; it cannot force a no — `false` and "no claim" are the same
thing, and both fall through to the walk. There is no third state."

tests/test_containers.cpp:54 —
template <>
struct threadsafe::is_unsafe_sendable<std::vector<OptedOut>> : std::false_type {};
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Le comportement est correct et conservateur : c'est l'enonce de CLAUDE.md qu'il faut preciser, pas le code. Remplacer CLAUDE.md:58-62 par :

  "The unsafe primaries derive from `std::false_type`. On a type nobody has
   vouched for, `false` and \"no claim\" are the same thing: both fall through
   to the walk. The one exception is ordinary C++ specialization ranking — a
   user's FULL specialization outranks a partial one, including the library's
   own. Writing `false_type` on a type the library vouches for (`std::vector`,
   `std::unique_ptr`, `std::atomic`, `synchronized_value`, `copy_on_write`)
   therefore removes that vouch and hands the question back to the walk, which
   answers no. That is the only way to opt out of trust the library granted,
   and it can only ever make an answer more conservative. A conditional claim
   is still written as a `bool_constant`, where `false` means \"nothing
   vouched, let the walk decide\":"

Et epingler les deux moities dans tests/test_containers.cpp (verifie compilant : `Plain` n'existe pas dans ce fichier, utiliser `MutCache` qui y est deja declare). A placer juste apres la specialisation `is_unsafe_sendable<std::vector<OptedOut>>` (ligne 55) :

  template <>
  struct threadsafe::is_unsafe_synchronizable<threadsafe::synchronized_value<int>>
      : std::false_type {};

  template <>
  struct threadsafe::is_unsafe_sendable<MutCache> : std::false_type {};

puis, avec les autres static_assert :

  static_assert(!threadsafe::is_synchronizable_v<threadsafe::synchronized_value<int>>,
                "is_unsafe_* — a full specialization outranks the library's own "
                "partial one, so false_type removes the vouch");
  static_assert(threadsafe::is_sendable_v<MutCache>,
                "is_unsafe_* — on a walkable type nobody vouched for, false_type "
                "is a no-op: the walk still answers");

(Attention a l'ordre : ces specialisations doivent precede toute question posee sur `synchronized_value<int>` dans la meme TU, conformement a la regle deja documentee dans CLAUDE.md.)
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p13_optout.cpp` :
```text
template <> struct threadsafe::is_unsafe_sendable<Plain> : std::false_type {};
template <> struct threadsafe::is_unsafe_synchronizable<threadsafe::synchronized_value<int>> : std::false_type {};
static_assert(is_sendable_v<Plain>);                                  // PASSE
static_assert(is_synchronizable_v<threadsafe::synchronized_value<int>>); // ECHOUE
```
Sortie : p13_optout.cpp:19:15: error: static assertion failed: PROBE-OPTOUT-2: false must NOT revoke the library's own vouch.
Les deux etats coexistent donc bien : sur un type walkable false == aucune revendication, sur un type vouche par partial specialization false == non force.

</details>

## Simplicité du code éducatif

### SIMP-01 — Une référence n'est traitée comme telle que dans la boucle des membres : std::pair<std::atomic<int>&, int> est refusé là où un membre std::atomic<int>& est accepté

**MINEUR** · simplicité · `include/threadsafe/details/synchronizable_base.h:77`

La règle « une référence membre franchit une indirection : on interroge le référent via pointee_answer » est écrite uniquement dans la boucle des membres. Interrogé directement, un type référence tombe dans `if (!is_const(type)) return false;` (un type référence n'est jamais const-qualifié) et répond non sans raison. C'est le chemin qu'emprunte la règle const des wrappers std, qui passe les arguments template tels quels à `is_synchronizable_type` : `const std::pair<std::atomic<int>&, int>` est donc refusé alors que `const struct { std::atomic<int>& a; }` est accepté. Deux réponses différentes pour la même forme, dans un code qui se veut pédagogique.

**Code actuel**

```cpp
if (is_mutable_member(member)) {
  if (!is_synchronizable_type(member_type))
    return false;
} else if (is_reference_type(member_type)) {
  if (!pointee_answer(remove_cvref(member_type), is_synchronizable_type))
    return false;
} else if (!is_synchronizable_type(add_const(member_type))) {
  return false;
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le correctif propose est valide tel quel (compile, 12/12 tests verts, aucune regression de soundness). Je le reprends en y ajoutant deux precisions que l'auditeur a omises.

Dans include/threadsafe/details/synchronizable_base.h, dans detail::diagnose_is_synchronizable, inserer la regle juste apres le test is_unsafe (ligne 44-45) et avant is_function_type :

```cpp
if (is_unsafe_synchronizable_type(type))
  return true;

if (is_reference_type(type))
  return pointee_answer(remove_cvref(type), is_synchronizable_type);

if (is_function_type(type))
  return true;
```
puis supprimer la branche devenue redondante dans la boucle des membres (add_const sur un type reference est un no-op, la reference retombe donc sur la nouvelle regle de premier niveau) :

```cpp
for (auto member : nonstatic_data_members_of(type, context)) {
  const auto member_type = type_of(member);

  if (is_mutable_member(member)) {
    if (!is_synchronizable_type(member_type))
      return false;
  } else if (!is_synchronizable_type(add_const(member_type))) {
    return false;
  }
}
```
Deux points a assumer explicitement, idealement en commentaire ou dans CLAUDE.md puisque le code est pedagogique :

(a) Placee avant is_function_type, la regle fait passer is_synchronizable_v<void(&)()> de faux a vrai. C'est coherent avec la ligne is_function_type -> true deja presente (un type fonction est sans etat), et c'est dans le sens sur. Si l'on veut zero changement sur cette forme, il suffit de placer la nouvelle regle APRES is_function_type — les tests passent aussi dans cet ordre, seule la reponse sur void(&)() change.

(b) La regle interroge le referent SANS const (remove_cvref). Consequence, inchangee par le correctif mais desormais visible au premier niveau : une reference vers une classe utilisateur qui n'encapsule que des atomiques, par exemple struct FinalSync final { std::atomic<int> a; }, reste refusee, parce que le walk de classe exige is_const. Verifie identique avant/apres sur FinalSync&, sur const struct{FinalSync& r;} et sur const std::pair<FinalSync&,int>. Ce n'est donc pas une regression, mais c'est un conservatisme separe qu'il vaut mieux nommer que laisser decouvrir : remplacer remove_cvref par add_const(remove_reference(...)) le leverait, au prix de rompre la regle CLAUDE.md « const derriere une indirection n'est jamais fait confiance » — a ne PAS faire dans ce correctif.

Suggestion de tests a ajouter dans tests/test_synchronizable.cpp pour verrouiller la regle :

```cpp
static_assert(is_synchronizable_v<const std::pair<std::atomic<int> &, int>>);
static_assert(is_synchronizable_v<const std::tuple<std::atomic<int> &>>);
static_assert(!is_synchronizable_v<const std::pair<int &, int>>);
static_assert(!is_synchronizable_v<const std::pair<const int &, int>>);
static_assert(is_synchronizable_v<std::atomic<int> &>);
static_assert(!is_synchronizable_v<int &>);
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p7.cpp`, compilée sans erreur : `is_synchronizable_v<const RefMemberAtomic>` vrai, `!is_synchronizable_v<const std::pair<std::atomic<int>&, int>>`, `!is_synchronizable_v<const std::tuple<std::atomic<int>&>>`.

Correctif validé sur la copie patchée .../audit_sync/inc3 : les 12 fichiers de tests du dépôt recompilent avec 0 static_assert en échec, et .../audit_sync/fix3.cpp compile `is_synchronizable_v<const std::pair<std::atomic<int>&, int>>`, `is_synchronizable_v<const std::tuple<std::atomic<int>&>>`, `!is_synchronizable_v<const std::pair<int&, int>>`, `!is_synchronizable_v<const std::pair<const int&, int>>`.

</details>

### SIMP-02 — Le filet borrowed_range n'attrape rien que le walk n'attrape deja, et rejette a tort des vues possedantes

**MINEUR** · simplicité · `include/threadsafe/details/lifetime_aware.h:59`

Les trois cibles revendiquees du filet (std::span, std::string_view, std::ranges::subrange) sont deja rejetees par la suite du walk: elles stockent un pointeur brut (rejete ligne 53 lors de la visite des membres) ou ne sont pas des `default_type` (constructeurs templates -> `is_walkable_type` faux). Le filet n'est donc pas porteur. En revanche il produit des faux negatifs sur des vues qui possedent integralement leur etat: `std::ranges::iota_view` et `std::ranges::empty_view` sont declares `enable_borrowed_range` par la norme alors qu'ils ne referencent rien. Pour une bibliotheque a vocation pedagogique, c'est une ligne qui coute un include <ranges>, une substitution de concept par reflection, et qui n'enseigne qu'une regle fausse ("borrowed_range == n'appartient pas").

**Code actuel**

```cpp
if (trait_value(^^std::ranges::borrowed_range, type))
  return false;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Supprimer lignes 59-60 de include/threadsafe/details/lifetime_aware.h et l'include `<ranges>` ligne 6 (verifie : aucun autre header ne depend de cet include transitivement pour un cas qui marchait deja).

Deux ajustements par rapport a la proposition initiale :

a) Corriger la justification. Le walk ne rejette PAS span/string_view/subrange/ref_view sur un membre pointeur : `is_walkable_type` est deja faux pour eux (ils ont des constructeurs templates, donc ne sont pas des `default_type`). Le rejet a lieu ligne 65, avant toute visite de membre. Sonde : `static_assert(!threadsafe::detail::is_walkable_type(^^std::span<int>));` compile.

b) Ajouter le contre-exemple couvert aux tests, pour rendre explicite ce que la suppression change. Dans tests/test_lifetime_aware.cpp, remplacer le commentaire « borrowed ranges do not own their data » (lignes 27-32) par la regle reellement appliquee, et ajouter les deux cas qui la delimitent :

```cpp
static_assert(!is_lifetime_aware_v<std::span<int>>,
              "is_lifetime_aware - a view whose state is a raw pointer does not own its data");
static_assert(!is_lifetime_aware_v<std::string_view>,
              "is_lifetime_aware - a view whose state is a raw pointer does not own its data");
static_assert(!is_lifetime_aware_v<std::ranges::subrange<int*>>,
              "is_lifetime_aware - a view whose state is a raw pointer does not own its data");
static_assert(is_lifetime_aware_v<std::ranges::iota_view<int, int>>,
              "is_lifetime_aware - iota_view owns its two bounds, borrowed_range or not");
static_assert(is_lifetime_aware_v<std::ranges::empty_view<int>>,
              "is_lifetime_aware - empty_view has no state to borrow");
```
Limite connue et assumee (a mentionner si la question vient en conference) : un type utilisateur dont l'etat est purement scalaire mais qui reference un stockage externe par indice repond VRAI. C'est deja le cas aujourd'hui des qu'il n'a pas fait `enable_borrowed_range = true` ; la suppression ne fait qu'uniformiser cette reponse au lieu de la faire dependre d'un opt-in sans rapport.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Copie des headers dans scratchpad/audit_lifetime/inc, les deux lignes supprimees. p3.cpp compile EXIT=0 (aucune erreur):

  static_assert(!is_lifetime_aware_v<std::span<int>>);
  static_assert(!is_lifetime_aware_v<std::string_view>);
  static_assert(!is_lifetime_aware_v<std::ranges::subrange<int *>>);
  static_assert(!is_lifetime_aware_v<std::span<int, 4>>);
  static_assert(!is_lifetime_aware_v<std::ranges::ref_view<std::vector<int>>>);
  static_assert(is_lifetime_aware_v<std::ranges::iota_view<int, int>>);
  static_assert(is_lifetime_aware_v<std::ranges::empty_view<int>>);

Et les 12 fichiers de tests/*.cpp compilent tous OK contre ./inc (aucune regression, y compris test_lifetime_aware.cpp et test_soundness_regressions.cpp).

Avec le header d'origine, p1.cpp/p2.cpp prouvent les faux negatifs actuels:
  error: static assertion failed: PROBE 09_iota == FALSE   (decltype(std::views::iota(0, 10)))
  error: static assertion failed: PROBE 40_empty_view == FALSE
```

</details>

### SIMP-03 — Trois couches de nommage (detail::diagnose_* + _v + concept) pour une conjonction de deux termes

**MINEUR** · simplicité · `include/threadsafe/details/asynchronous_task_launcher.h:16`

Les lignes 16-39 dépensent six noms (deux consteval detail::diagnose_*, deux variables _v publiques, deux concepts) pour exprimer « move_constructible && sendable » et « ça && lifetime_aware ». L'argument de mémoïsation invoqué dans CLAUDE.md ne s'applique pas ici : is_sendable_v / is_lifetime_aware_v sont déjà mémoïsés, et les contraintes atomiques d'un concept sont mises en cache par le compilateur — la couche _v n'ajoute rien. Les fonctions s'appellent diagnose_* alors qu'elles ne diagnostiquent rien (aucun static_assert, aucun walk), contrairement à diagnose_is_sendable. Et is_task_participant_v / is_scoped_task_participant_v ne sont utilisés nulle part ailleurs dans le dépôt : ce sont deux noms publics morts. Pour du code de conférence, c'est trois indirections à lire avant de voir la règle.

**Code actuel**

```cpp
template <class T>
consteval bool diagnose_scoped_task_participant() {
    return std::move_constructible<T> && is_sendable_v<T>;
}

template <class T>
consteval bool diagnose_task_participant() {
    return diagnose_scoped_task_participant<T>() && is_lifetime_aware_v<T>;
}

}

template <class T>
constexpr bool is_scoped_task_participant_v
    = detail::diagnose_scoped_task_participant<T>();

template <class T>
concept scoped_task_participant = is_scoped_task_participant_v<T>;

template <class T>
constexpr bool is_task_participant_v = detail::diagnose_task_participant<T>();

template <class T>
concept task_participant = is_task_participant_v<T>;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Collapser les trois couches comme propose, MAIS en testant le trait AVANT `std::move_constructible`, sinon le court-circuit de la conjonction de concepts fait perdre le rejet de `void` / des types incomplets (invariant CLAUDE.md).

Remplacer les lignes 14-39 de include/threadsafe/details/asynchronous_task_launcher.h (tout le bloc `namespace detail { ... }` plus les quatre declarations publiques) par :

```cpp
template <class T>
concept scoped_task_participant = sendable<T> && std::move_constructible<T>;

template <class T>
concept task_participant = scoped_task_participant<T> && lifetime_aware<T>;
```

L'ordre `sendable<T> &&` d'abord est load-bearing : il force l'instanciation de `is_sendable_v<T>`, donc du `static_assert` de `detail::assert_queryable_type` (utils.h:13 et 16). Court-circuiter `std::move_constructible` dans l'autre sens est inoffensif puisque ce concept ne contient aucun `static_assert`. Et comme les trois traits partagent le meme garde `assert_queryable_type`, empoisonner via `sendable` suffit a couvrir `lifetime_aware`.

Verifie par compilation contre une copie du header :
- 12/12 tests de tests/ compilent ;
- `scoped_task_participant<void>` redevient un hard error ("void is not a value...") comme dans le pristine ;
- `struct Pinned { Pinned(Pinned&&) = delete; void* opaque_handle; };` redevient empoisonne comme dans le pristine ;
- la surcharge contrainte de `launch_task`/`launch_scoped_task` est toujours selectionnee sur le chemin positif ;
- le diagnostic s'ameliore : « the expression 'is_sendable_v<T> [with T = RawRef]' evaluated to 'false' » au lieu de l'opaque « is_task_participant_v<T> ... evaluated to 'false' ».

Header : 89 -> 68 lignes, deux noms publics au lieu de quatre, zero nom mort.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Vérifié par compilation : header remplacé dans une copie (inc/), les 12 fichiers de tests/ compilent tous (12/12 OK), y compris test_soundness_regressions.cpp, test_synchronized_value.cpp et test_copy_on_write.cpp qui consomment launchable_task. grep sur tout le dépôt : is_task_participant_v et is_scoped_task_participant_v n'apparaissent que dans asynchronous_task_launcher.h.

</details>

### SIMP-04 — L'ordre des #include de threadsafe.h est porteur de sens et non commente : trier la liste casse la compilation

**MINEUR** · simplicité · `include/threadsafe/threadsafe.h:3`

asynchronous_task_launcher.h contient, dans le corps de sa classe, un `static_assert(task_participant<std::stop_token>, ...)` (l.49) qui est EVALUE a l'inclusion. Or ce fichier n'inclut pas vocabulary.h, qui est le seul a vouch-er std::stop_token. La lib ne compile que parce que threadsafe.h liste vocabulary.h AVANT le lanceur — dans une liste par ailleurs non triee, alors que tous les autres fichiers ont leurs includes tries. Consequence concrete : un simple clang-format (ou tout IDE qui trie les includes) sur threadsafe.h rend la bibliotheque non compilable, avec un message trompeur qui accuse std::stop_token. Aucun commentaire n'avertit. Pour un public de conference c'est le pire type de fragilite : invisible, et declenchee par un outil standard.

**Code actuel**

```cpp
#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/details/synchronized_value.h>
#include <threadsafe/details/copy_on_write.h>
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee par l'auditeur compile et ne casse rien (verifie : 12 tests, fail=0). Je la garde, mais je recadre la justification et je reduis le patch minimal.

Le vrai defaut n'est pas "la liste n'est pas triee", c'est que `asynchronous_task_launcher.h` n'est pas self-contained. Une seule ligne le corrige, et elle suffit — le tri de `threadsafe.h` devient alors purement cosmetique au lieu d'etre une condition de compilation :

// include/threadsafe/details/asynchronous_task_launcher.h, apres la ligne 10
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/vocabulary.h>   // <-- ajout : vouche std::stop_token, exige par le static_assert l.49

Cet ajout est deja coherent avec le fichier : il inclut `<stop_token>` lui-meme (l.4) pour nommer le type, il lui manquait juste le header qui en repond. Aucun cycle : `vocabulary.h` n'inclut pas le lanceur.

Le tri de `threadsafe.h` (lignes 3-11) peut suivre, et devient sur une fois la ligne ci-dessus en place — c'est d'ailleurs le seul moyen de garantir que la dependance cachee ne revienne pas : tant que le header n'est pas autonome, un commentaire "ne pas trier" dans `threadsafe.h` ne protegerait pas l'utilisateur qui inclut le detail header directement (sonde A, qui casse sur le repo intact sans qu'aucun tri soit intervenu).

Je NE recommande PAS la variante "ajouter un commentaire d'avertissement dans threadsafe.h" : elle documente la fragilite au lieu de la supprimer, et laisse la sonde A cassee.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

AVANT : copie intacte dans .../scratchpad/audit_simplicity/sortinc, seul threadsafe.h passe a clang-format (qui trie). `g++-16 -std=c++26 -freflection -fsyntax-only -I .../sortinc tests/test_diagnostics.cpp` ->
```text
asynchronous_task_launcher.h:49:19: error: static assertion failed: std::jthread injects a stop_token that the Args constraints never see; it must satisfy them on its own
```
APRES : copie .../scratchpad/audit_simplicity/selfcontained = threadsafe.h trie PLUS l'include de vocabulary.h dans le lanceur -> les 12 tests compilent, `RESULT fail=0`.

</details>

### SIMP-05 — Code mort : is_smart_pointer_v et is_smart_pointer_type ne sont utilises nulle part

**MINEUR** · simplicité · `include/threadsafe/details/smart_pointers.h:41`

`is_smart_pointer_type` n'est reference par aucun fichier de la lib ni aucun test ; `is_smart_pointer_v` n'est reference que par `is_smart_pointer_type`. Le concept `smart_pointer` (l.45), lui, passe directement par `is_smart_pointer<T>::value` et ignore le `_v`. Ce sont 9 lignes qui imitent le protocole `_v` + `_type` des vrais traits alors qu'elles ne participent a rien : un spectateur va les lire comme un quatrieme trait et chercher ou la reflection les interroge.

**Code actuel**

```cpp
template <typename T>
constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;

template <class T>
concept smart_pointer = is_smart_pointer<T>::value;

inline consteval bool is_smart_pointer_type(std::meta::info info) {
  return detail::trait_value(^^is_smart_pointer_v, info);
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Correction proposee valide telle quelle (verifiee : 12/12 tests OK, comportement identique). Diff exact applique dans include/threadsafe/details/smart_pointers.h :

@@ -38,16 +38,9 @@
 template <typename T, typename D>
 struct is_smart_pointer<std::unique_ptr<T, D>> : std::true_type {};
 
-template <typename T>
-constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;
-
 template <class T>
 concept smart_pointer = is_smart_pointer<T>::value;
 
-inline consteval bool is_smart_pointer_type(std::meta::info info) {
-  return detail::trait_value(^^is_smart_pointer_v, info);
-}
-
 template <smart_pointer T>
 struct is_unsafe_lifetime_aware<T>
```cpp
: std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};
```
Variante minimale si l'auteur souhaite conserver le compagnon `_v` public de style std (c'est le `_type` seul qui mime le protocole de reflection et cree la confusion pedagogique) : garder les lignes 41-42 et ne supprimer que les lignes 47-49. Cette variante compile aussi.

Note : apres suppression, `smart_pointers.h` n'utilise plus `detail::trait_value` directement, mais l'include de utils.h reste necessaire (via sendable.h/synchronizable.h) pour `pointee_answer` et `is_lifetime_aware_type` — ne pas retirer d'include.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

grep exhaustif sur include/ et tests/ : `is_smart_pointer_type` -> 1 seule occurrence (sa propre definition, smart_pointers.h:47) ; `is_smart_pointer_v` -> 2 occurrences, sa definition l.42 et son usage l.48 a l'interieur du mort. Suppression appliquee dans .../scratchpad/audit_simplicity/dead : les 12 tests compilent, `RESULT fail=0`.

</details>

### SIMP-06 — L'ordre des branches de walk_is_sendable est un invariant de soundness silencieux : deplacer une ligne rend int* sendable

**MINEUR** · simplicité · `include/threadsafe/details/sendable.h:55`

La bibliotheque entiere ne contient AUCUN commentaire (grep '//' hors '// namespace' : zero resultat sur les 840 lignes). Or la branche `is_scalar_type(type)` de la l.61 englobe les pointeurs : elle n'est correcte que parce qu'elle est placee APRES la branche pointeur de la l.55. Rien ne le dit. Un contributeur — ou un spectateur qui recopie la slide en reordonnant pour "grouper les cas simples d'abord" — ouvre un trou de soundness sans que rien ne proteste a la lecture. C'est exactement l'endroit ou un commentaire n'est PAS inutile au sens de CLAUDE.md.

**Code actuel**

```cpp
if (is_pointer_type(type))
  return is_sendable_type(add_lvalue_reference(remove_pointer(type)));

if (is_array_type(type))
  return is_sendable_type(remove_all_extents(type));

if (is_scalar_type(type) || is_synchronizable_type(type))
  return true;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Rendre la branche independante de l'ordre plutot que la commenter (include/threadsafe/details/sendable.h:55-62). Verifie : experience B, place en PREMIERE position, 12/12 fichiers de test a 0 erreur.

```cpp
if ((is_scalar_type(type) && !is_pointer_type(type)) ||
    is_synchronizable_type(type))
  return true;

if (is_pointer_type(type))
  return is_sendable_type(add_lvalue_reference(remove_pointer(type)));

if (is_array_type(type))
  return is_sendable_type(remove_all_extents(type));
```
Le seul chevauchement reel etait scalaire/pointeur ; une fois les pointeurs exclus du test scalaire, aucune des trois branches ne depend de sa position et aucun commentaire n'est necessaire (conforme a "Avoid useless comments").

NE PAS appliquer le commentaire propose par le finding : il affirme "doit rester apres les deux precedentes", or l'experience A (bloc scalaire place entre pointeur et tableau) passe les 12 fichiers de test. L'ordre vis-a-vis de la branche tableau n'est pas porteur.

NE PAS toucher a walk_is_synchronizable au titre de la soundness : le reordonnancement de `if (!is_const(type)) return false;` ne produit que des faux negatifs (13 assertions positives cassees, aucune negative inversee).

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Copie .../scratchpad/audit_simplicity/order : la seule modification est de remonter le bloc `is_scalar_type || is_synchronizable_type` au-dessus du bloc pointeur. Compilation de tests/test_diagnostics.cpp :
```text
test_diagnostics.cpp:67: error: static assertion failed  ->  static_assert(!is_sendable_v<int *>);
test_diagnostics.cpp:47, 59, 65, 68, 69 : idem (Borrowing, BorrowingOuter, Borrowing[4], vector<Borrowing>)
```
Un simple deplacement de branche, sans avertissement dans le code, rend `int*` sendable.

</details>

### SIMP-07 — is_default_type : "default" de quoi ? Le nom ne dit rien de ce qui est verifie

**MINEUR** · simplicité · `include/threadsafe/details/utils.h:76`

La fonction verifie qu'aucune copie/deplacement/destructeur n'est ecrit a la main et qu'aucun template de constructeur ou d'operator= ne peut les detourner. Autrement dit : "les operations de copie/deplacement/destruction sont celles que le compilateur genere". Le nom `is_default_type` ne porte rien de cela — "default" evoque `is_default_constructible`, ou un type par defaut. C'est pourtant le pivot de tout le modele conservateur (CLAUDE.md : "a user-written copy, move or destructor [...] fail before the member walk even starts"), donc le nom que le public doit retenir.

**Code actuel**

```cpp
inline consteval bool is_default_type(std::meta::info type) {
  const auto context = std::meta::access_context::unchecked();

  for (auto member : std::meta::members_of(type, context)) {
    if (may_hijack_copy_move(member))
      return false;

    if (is_copy_move_destructor(member) && !is_defaulted(member) &&
        !is_deleted(member))
      return false;
  }

  return true;
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Ne pas retenir `has_implicit_copy_move_destroy` : ma sonde prouve que le predicat est vrai pour un type a constructeur de copie SUPPRIME (cas 1) et pour un type a destructeur SUPPRIME (cas 2), qui n'ont ni copie ni destruction "implicit". Le nom exact doit couvrir `defaulted` ET `deleted`, dont le point commun est l'absence de code utilisateur.

include/threadsafe/details/utils.h:76 (corps strictement inchange) :

```cpp
inline consteval bool has_no_user_copy_move_destroy(std::meta::info type) {
  const auto context = std::meta::access_context::unchecked();

  for (auto member : std::meta::members_of(type, context)) {
    if (may_hijack_copy_move(member))
      return false;

    if (is_copy_move_destructor(member) && !is_defaulted(member) &&
        !is_deleted(member))
      return false;
  }

  return true;
}
```

include/threadsafe/details/utils.h:98 :

```cpp
  if (!has_no_user_copy_move_destroy(type))
    return false;
```

CLAUDE.md:42 : remplacer `detail::is_default_type` par `detail::has_no_user_copy_move_destroy`.

Verifie : ligne 76 = 75 colonnes (sous le defaut LLVM de 80 ; le depot n'a pas de .clang-format), les 12 tests compilent (fail=0), et les 6 cas de la sonde restent coherents avec le nouveau nom.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Renommage applique dans .../scratchpad/audit_simplicity/clear : les 12 tests compilent, `RESULT fail=0`. Le nom tient sur une ligne (63 colonnes), clang-format ne le replie pas.

</details>

### SIMP-08 — synchronized_value : la chaine de conditional_t cache les deux seules idees du type (verrou ecrivain / verrou lecteur)

**MINEUR** · simplicité · `include/threadsafe/details/synchronized_value.h:45`

Le coeur pedagogique de synchronized_value tient en une phrase : "si const T est synchronizable, les lectures peuvent etre partagees, donc shared_mutex + shared_lock ; sinon mutex + unique_lock partout". Le code l'exprime en un conditional_t imbrique dans un argument de template sur trois lignes repliees par clang-format, ou le lecteur doit apparier `shared_readable` avec deux endroits eloignes et compter les chevrons. Detail supplementaire : `bool(is_synchronizable_v<const T>)` — `is_synchronizable_v` est deja `constexpr bool`, le cast est du bruit pur. Et `shared_readable` nomme une propriete de T, pas ce qu'on en deduit.

**Code actuel**

```cpp
static constexpr bool shared_readable = bool(is_synchronizable_v<const T>);

using mutex =
    std::conditional_t<shared_readable, std::shared_mutex, std::mutex>;
using guard = value_guard<T, std::unique_lock<mutex>>;
using const_guard =
    value_guard<const T,
                std::conditional_t<shared_readable, std::shared_lock<mutex>,
                                   std::unique_lock<mutex>>>;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee est acceptee VERBATIM — elle compile, passe les 12 tests, et produit des types strictement identiques. Aucun amendement necessaire.

```cpp
  static constexpr bool concurrent_reads_are_safe =
      is_synchronizable_v<const T>;

  using mutex = std::conditional_t<concurrent_reads_are_safe, std::shared_mutex,
                                   std::mutex>;

  using writer_lock = std::unique_lock<mutex>;
  using reader_lock = std::conditional_t<concurrent_reads_are_safe,
                                         std::shared_lock<mutex>, writer_lock>;

  using guard = value_guard<T, writer_lock>;
  using const_guard = value_guard<const T, reader_lock>;
```

Deux precisions que le finding n'apporte pas et qui lui sont favorables :

1. Le renommage `shared_readable` -> `concurrent_reads_are_safe` touche un membre PUBLIC, mais grep confirme qu'aucun test ni header ne le reference (seuls hits hors du fichier : des .i preprocesses dans build-gccDebug/). Zero appelant casse.

2. Garder `concurrent_reads_are_safe` public est le bon choix ici et il ne faut PAS le passer en private : il rend la branche choisie assertable depuis un test, ce qui a de la valeur dans un codebase dont le but affiche est pedagogique. Ma sonde s'en sert (`static_assert(shared_branch::concurrent_reads_are_safe)`).

Ne PAS ajouter de commentaire explicatif au-dessus du bloc : CLAUDE.md impose « Avoid useless comments », et c'est tout l'interet de la correction que les noms portent seuls l'explication.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Reecriture appliquee dans .../scratchpad/audit_simplicity/clear : les 12 tests compilent, `RESULT fail=0` (dont test_synchronized_value.cpp, 151 lignes, qui couvre les deux branches shared/exclusive).

</details>

### SIMP-09 — synchronizable.h ne contient pas is_synchronizable : le decoupage en 12 fichiers a un nom qui trompe

**MINEUR** · simplicité · `include/threadsafe/details/synchronizable.h:1`

Le trait `is_synchronizable` vit dans `synchronizable_base.h` (90 lignes). Le fichier `synchronizable.h` (15 lignes) ne contient qu'une seule specialisation, celle de std::atomic. Un spectateur a qui on dit "le trait Sync est dans synchronizable.h" ouvre le mauvais fichier ; et le suffixe `_base` ne designe rien (il n'y a aucune derivation, aucune couche "base"). Par ailleurs cette specialisation d'atomic est exactement de meme nature que celles de vocabulary.h (allocator, stop_token) : un vouch de type standard. Le fichier existe uniquement pour eviter un cycle d'include que la fusion resout.

**Code actuel**

```cpp
// details/synchronizable.h, dans son integralite
#pragma once

#include <atomic>
#include <type_traits>

#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable_base.h>

namespace threadsafe {

template <class T>
struct is_unsafe_synchronizable<std::atomic<T>>
    : std::bool_constant<is_sendable_v<T>> {};

} // namespace threadsafe
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Fusion correcte et complete (compilee, 12/12 tests OK) :

1. Supprimer `include/threadsafe/details/synchronizable.h`.
2. Renommer `include/threadsafe/details/synchronizable_base.h` -> `include/threadsafe/details/synchronizable.h`.
3. Remplacer les references `details/synchronizable_base.h` -> `details/synchronizable.h` (une seule occurrence reelle : `details/sendable.h:6`).
4. Deplacer le vouch d'atomic dans `details/vocabulary.h`, a cote des autres vouchs de types standard, en ajoutant `#include <atomic>` :

```cpp
// details/vocabulary.h
#include <atomic>
#include <memory>
#include <stop_token>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_unsafe_synchronizable<std::atomic<T>>
    : std::bool_constant<is_sendable_v<T>> {};

template <class T>
struct is_unsafe_sendable<std::allocator<T>> : std::true_type {};
// ... le reste inchange
```

5. ETAPE MANQUANTE dans le finding original — supprimer `CMakeLists.txt:16` :
```
    include/threadsafe/details/synchronizable.h
    include/threadsafe/details/synchronizable_base.h   <-- supprimer cette ligne
```
Sans ca, `cmake -B build` echoue sur une source inexistante.

Justification a corriger dans le texte du finding : il ne s'agit PAS d'un cycle d'include (il n'y en a aucun), mais d'un ordre de couches — le vouch d'atomic depend de `is_sendable_v`, qui depend de `is_synchronizable`, donc il doit vivre au-dessus de `sendable.h`. `vocabulary.h` est deja cette couche, d'ou la relocalisation. J'ai prouve la contrainte : mettre le vouch dans le fichier du trait produit `error: 'is_sendable_v' was not declared in this scope`.

Resultat : `details/` passe de 11 a 10 headers, et chaque trait est dans le fichier qui porte son nom (`utils.h`, `synchronizable.h`, `sendable.h`, `lifetime_aware.h`), ce qui rend annoncable l'ordre de lecture propose.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Fusion appliquee dans .../scratchpad/audit_simplicity/merge (atomic deplace dans vocabulary.h, synchronizable.h supprime, synchronizable_base.h renomme en synchronizable.h, includes mis a jour et tries) : `ls details/` donne 10 fichiers, et les 12 tests compilent, `RESULT fail=0`.

</details>

### SIMP-10 — asynchronous_task_launcher : deux fonctions detail:: d'une ligne imitent le protocole des traits sans en avoir la raison

**MINEUR** · simplicité · `include/threadsafe/details/asynchronous_task_launcher.h:14`

Les traits ont un `detail::diagnose_*` parce que le walk est recursif et a besoin d'une declaration anticipee. Ici, `diagnose_scoped_task_participant` et `diagnose_task_participant` ne sont que des conjonctions d'une ligne, sans recursion : le namespace detail, les deux fonctions consteval et les deux variables qui les appellent forment 23 lignes pour exprimer deux `&&`. Le lecteur, entraine par les trois fichiers precedents, va sauter dans detail:: en s'attendant a un walk et n'y trouver rien.

**Code actuel**

```cpp
namespace detail {

template <class T>
consteval bool diagnose_scoped_task_participant() {
    return std::move_constructible<T> && is_sendable_v<T>;
}

template <class T>
consteval bool diagnose_task_participant() {
    return diagnose_scoped_task_participant<T>() && is_lifetime_aware_v<T>;
}

}

template <class T>
constexpr bool is_scoped_task_participant_v
    = detail::diagnose_scoped_task_participant<T>();

template <class T>
concept scoped_task_participant = is_scoped_task_participant_v<T>;

template <class T>
constexpr bool is_task_participant_v = detail::diagnose_task_participant<T>();

template <class T>
concept task_participant = is_task_participant_v<T>;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee est correcte telle quelle et je la valide sans modification. Remplacer les lignes 14-39 de include/threadsafe/details/asynchronous_task_launcher.h par:

template <class T>
constexpr bool is_scoped_task_participant_v =
```cpp
std::move_constructible<T> && is_sendable_v<T>;
```
template <class T>
concept scoped_task_participant = is_scoped_task_participant_v<T>;

template <class T>
constexpr bool is_task_participant_v =
```cpp
is_scoped_task_participant_v<T> && is_lifetime_aware_v<T>;
```
template <class T>
concept task_participant = is_task_participant_v<T>;

Deux precisions que le finding omet:

1. Les includes restent inchanges. `#include <concepts>` (ligne 3) est toujours necessaire pour `std::move_constructible`; ne pas le retirer en croyant nettoyer.

2. Le gain n'est pas seulement cosmetique, et cela vaut la peine d'etre dit a l'auteur. Le code actuel fait appeler `diagnose_scoped_task_participant<T>()` par `diagnose_task_participant<T>()` DIRECTEMENT, c'est-a-dire en court-circuitant la variable `is_scoped_task_participant_v<T>`. C'est une entorse a l'invariant que CLAUDE.md revendique explicitement (« `_v` is the memo: the compiler instantiates a variable template once per `T` »): la conjonction scoped est recalculee au lieu d'etre lue dans le memo. La reecriture proposee passe par `is_scoped_task_participant_v<T>` et realigne donc le fichier sur la regle de memoisation du reste de la bibliotheque. C'est un argument supplementaire pour appliquer le patch, au-dela de la lisibilite.

Verification: 12/12 tests compilent, sonde de 27 static_assert identique avant/apres (y compris le cas discriminant `std::atomic<int>*` ou scoped=oui et task=non), rejet de void et des types incomplets byte-identique, diagnostic d'echec du launcher identique.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Reecriture appliquee dans .../scratchpad/audit_simplicity/launcher : les 12 tests compilent, `RESULT fail=0` (dont test_asynchronous_task_launcher.cpp, 70 lignes de static_assert sur les deux concepts).

</details>

### SIMP-11 — operator->() && supprime : type de retour T au lieu d'un pointeur, et noexcept sur une fonction supprimee

**DÉTAIL** · simplicité · `include/threadsafe/details/copy_on_write.h:28`

La suppression fait son travail (`std::move(c)->size()` est bien refuse), mais la declaration est incoherente avec la surcharge qu'elle jumelle ligne 25 : un `operator->` renvoie un pointeur, pas un `T` par valeur -- un `T` ne pourrait meme pas participer a la chaine de `operator->`. Le `noexcept` sur une fonction supprimee est du bruit pur. Sur une slide ou les deux lignes 25 et 28 sont cote a cote, l'asymetrie `const T *` / `T` se lit comme une faute de frappe.

> Défaut trouvé indépendamment par 2 auditeurs : *copy_on_write : operator-> supprime avec un type de retour T incoherent avec l'operator-> reel*.

**Code actuel**

```cpp
const T *operator->() const & noexcept { return ptr_.get(); }

T operator*() && noexcept { return *ptr_; }
T operator->() && noexcept = delete;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee est correcte et suffisante, je la reprends telle quelle :

include/threadsafe/details/copy_on_write.h ligne 28 :
- T operator->() && noexcept = delete;
+ const T *operator->() && = delete;

Elle retablit la symetrie visuelle avec la ligne 25 (`const T *operator->() const & noexcept`), donne a `operator->` un type de retour qui a un sens, et supprime le `noexcept` inutile sur une fonction supprimee. Verifie : la suppression reste effective et les 12 tests du depot compilent sans modification.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

scratchpad/audit_cow/p6.cpp -DPROBE_ARROW=1 : la suppression fonctionne deja avec l'entete d'origine
```text
-> p6.cpp:12:24: error: use of deleted function 'T threadsafe::copy_on_write<T>::operator->() && [with T = std::__cxx11::basic_string<char>]'
```
Header patche dans scratchpad/audit_cow/fix/ avec `const T *operator->() && = delete;` : sonde p10.cpp exit 0, et tests/test_copy_on_write.cpp recompile exit 0 -- le changement de type de retour ne casse rien (y compris le static_assert ligne 120-121 du depot, qui porte sur la surcharge const&).

</details>

### SIMP-12 — Indentation à 4 espaces : seul fichier du projet à ne pas suivre le style à 2 espaces

**DÉTAIL** · simplicité · `include/threadsafe/details/asynchronous_task_launcher.h:17`

Tous les autres headers sont formatés au style LLVM (indentation de premier niveau à 2 espaces) ; asynchronous_task_launcher.h est le seul à utiliser 4 espaces, et il n'y a pas de .clang-format à la racine pour arbitrer. Sur un slide projeté, la rupture de style saute aux yeux et fait perdre quelques secondes à l'auditoire.

> Défaut trouvé indépendamment par 3 auditeurs : *asynchronous_task_launcher.h est le seul fichier hors format : 85 lignes sur 89 divergent du style des 10 autres en-tetes*, *vocabulary.h : accolade de namespace non commentee, seul fichier avec le lanceur dans ce cas*.

**Code actuel**

```cpp
template <class T>
consteval bool diagnose_scoped_task_participant() {
    return std::move_constructible<T> && is_sendable_v<T>;
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le volet reformatage est bon, mais le `.clang-format` doit IMPERATIVEMENT neutraliser le tri des includes, sans quoi les 12 tests cassent.

1. Reformater le seul fichier deviant :
```cpp
clang-format -i --style=LLVM include/threadsafe/details/asynchronous_task_launcher.h

Ce qui donne reellement (et non le snippet a deux lignes propose, que LLVM joint) :

  template <class T> consteval bool diagnose_scoped_task_participant() {
    return std::move_constructible<T> && is_sendable_v<T>;
  }

  template <class T> consteval bool diagnose_task_participant() {
    return diagnose_scoped_task_participant<T>() && is_lifetime_aware_v<T>;
  }

  } // namespace detail
```
2. Si un `.clang-format` est ajoute a la racine, il DOIT contenir SortIncludes: Never :

```cpp
  BasedOnStyle: LLVM
  SortIncludes: Never

Un `BasedOnStyle: LLVM` nu reordonne threadsafe.h, place asynchronous_task_launcher.h avant vocabulary.h, et fait exploser le static_assert sur std::stop_token dans les 12 tests.
```
3. Bonus fortement recommande, independant du cosmetique : l'ordre des includes de threadsafe.h est une dependance reelle et silencieuse (vocabulary.h doit preceder asynchronous_task_launcher.h). Rien ne le signale aujourd'hui. Ajouter un commentaire dans threadsafe.h, par exemple au-dessus de la ligne vocabulary.h :

```cpp
// vocabulary.h vouches for std::stop_token; it must precede
// asynchronous_task_launcher.h, which asks the question in a
// class-scope static_assert. Do not sort these includes.
```
Verification : avec (1) + (2), les 12 tests recompilent OK ; le fichier passe a two=14 four=12, et le diff token a token contre l'original est vide hormis les commentaires de fermeture de namespace.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Comptage des lignes indentées de premier niveau par fichier (grep -cE '^  [^ ]' vs '^    [^ ]') :
```text
asynchronous_task_launcher.h  two=0  four=17   <= seul fichier à zéro ligne à 2 espaces
utils.h two=29 | synchronized_value.h two=30 | synchronizable_base.h two=15 | sendable.h two=12 | lifetime_aware.h two=11 | copy_on_write.h two=11 | allowed_std_wrappers.h two=8 | smart_pointers.h two=5
```
(ls -a à la racine : aucun .clang-format).

</details>

### SIMP-13 — remove_cv sur les membres dans all_bases_and_members est du code mort

**DÉTAIL** · simplicité · `include/threadsafe/details/utils.h:48`

`all_bases_and_members` n'a que deux appelants — `diagnose_is_sendable` (sendable.h:67) et `diagnose_is_lifetime_aware` (lifetime_aware.h:68) — et tous deux commencent par `if (const auto unqualified = remove_cv(type); unqualified != type) return ...` (sendable.h:41, lifetime_aware.h:44). Le cv du type de membre est donc deja retire par la question elle-meme. Le `remove_cv` ici ne change jamais une reponse, mais il cree une asymetrie visible avec la ligne des bases juste au-dessus (`question(type_of(base))`, sans remove_cv) qui se lit comme un oubli et invite le lecteur a chercher un cas ou une base serait cv-qualifiee — il n'y en a pas, une base ne peut pas etre cv-qualifiee.

**Code actuel**

```cpp
for (auto base : bases_of(type, context))
  if (!question(type_of(base)))
    return false;

for (auto member : nonstatic_data_members_of(type, context))
  if (!question(remove_cv(type_of(member))))
    return false;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee par l'auditeur est correcte telle quelle. Dans include/threadsafe/details/utils.h:48 :

```cpp
for (auto member : nonstatic_data_members_of(type, context))
  if (!question(type_of(member)))
    return false;
```
Verifie : 16 formes de membres cv-qualifies donnent des reponses identiques, et les 12 tests du depot compilent.

Precision a ajouter par rapport a l'enonce de l'auditeur : ne PAS ajouter symetriquement un remove_cv sur la boucle des bases. Le langage ignore les cv-qualifiers d'un base-specifier ([class.derived]/2), donc type_of(base) n'est jamais cv-qualifie — verifie par sonde, is_const(type_of(base)) == 0 meme pour `struct X : ConstY {}`. Ce serait du code mort garanti.

Alternative si l'auteur prefere garder le helper auto-suffisant plutot que couple a la precondition de ses appelants : renommer pour enoncer la precondition, par exemple `all_bases_and_members_unqualified`, ou documenter en une ligne que `question` est suppose absorber le cv. Le trait synchronizable, lui, ne passe deliberement pas par ce helper precisement parce qu'il lit `is_const` (synchronizable_base.h:53, 67-83) — c'est ce contraste qui rend la precondition non evidente a la lecture.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

SONDE q_constmem.cpp, types a membres const de toutes formes (const int, const std::string, const std::vector<int>, const int*, int* const, const int[3]), reponses imprimees pour les trois traits.

AVEC le remove_cv (headers du depot) :
A send=1 sync_const=1 life=1 | B 1 1 1 | C 1 1 1 | D 0 0 0 | E 0 0 0 | F 1 1 1
SANS le remove_cv (copie patchee) :
A send=1 sync_const=1 life=1 | B 1 1 1 | C 1 1 1 | D 0 0 0 | E 0 0 0 | F 1 1 1

Identiques. Les 12 fichiers de tests du depot compilent egalement sans le remove_cv.

</details>

### SIMP-14 — diagnose_is_synchronizable reimplemente all_bases_and_members : le walk existe en deux exemplaires

**DÉTAIL** · simplicité · `include/threadsafe/details/synchronizable_base.h:67`

CLAUDE.md annonce "Chaque trait est une consteval — la marche structurelle". En pratique il y a deux marches : `all_bases_and_members` (utils.h:39-52), utilisee par is_sendable et is_lifetime_aware, et une copie manuscrite dans `diagnose_is_synchronizable`, avec une politique cv differente (add_const sur les bases et les membres au lieu de rien/remove_cv) et deux cas supplementaires (is_mutable_member, membre reference). Consequence pratique : toute evolution du walk — un nouveau genre de membre, un nouveau garde — doit etre ecrite deux fois, et rien dans le code ne le signale. Pour un public de conference c'est le point ou le modele mental "une seule marche" se casse.

> Défaut trouvé indépendamment par 2 auditeurs : *Duplication bases+membres de walk_is_synchronizable : ne PAS unifier avec all_bases_and_members, mais nommer la regle du membre*.

**Code actuel**

```cpp
for (auto base : bases_of(type, context))
  if (!is_synchronizable_type(add_const(type_of(base))))
    return false;

for (auto member : nonstatic_data_members_of(type, context)) {
  const auto member_type = type_of(member);

  if (is_mutable_member(member)) {
    if (!is_synchronizable_type(member_type))
      return false;
  } else if (is_reference_type(member_type)) {
    if (!pointee_answer(remove_cvref(member_type), is_synchronizable_type))
      return false;
  } else if (!is_synchronizable_type(add_const(member_type)))
    return false;
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

NE PAS appliquer le refactoring propose : il ajoute 8 lignes nettes et rend la signature du helper asymetrique (un callback qui recoit un type, un autre qui recoit un membre).

Si l'on veut traiter le vrai point pedagogique — la politique const-read de is_synchronizable n'est nulle part nommee, elle est noyee dans un if/else-if a l'interieur d'une boucle — la bonne correction est d'extraire et de NOMMER la politique, en laissant les boucles tranquilles. Teste : compile, les 12 tests passent, et la sonde differentielle de 21 assertions donne le meme resultat que l'original.

Dans synchronizable_base.h, avant diagnose_is_synchronizable :

```cpp
// The const-read policy: what a `const` object exposes of one member.
// This is what makes the synchronizable walk differ from the sendable and
// lifetime-aware ones, which read every member unqualified.
inline consteval bool is_const_readable_member(std::meta::info member) {
  const auto member_type = type_of(member);

  if (is_mutable_member(member))
    return is_synchronizable_type(member_type);

  if (is_reference_type(member_type))
    return pointee_answer(remove_cvref(member_type), is_synchronizable_type);

  return is_synchronizable_type(add_const(member_type));
}
```

et la boucle membres devient :

```cpp
  for (auto member : nonstatic_data_members_of(type, context))
    if (!is_const_readable_member(member))
      return false;
```

Gain reel : le concept central du trait ("lecture const") porte enfin un nom et un commentaire qui dit explicitement en quoi cette marche differe des deux autres — c'est le point que le public de conference doit retenir. Le squelette de boucle reste duplique, et c'est tres bien : quatre lignes de range-for ne justifient pas un helper a deux callbacks.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

NON COMPILE — c'est une observation de structure, pas un comportement. Les deux boucles sont lisibles telles quelles a utils.h:43-51 et synchronizable_base.h:67-83 ; j'ai verifie par lecture que `all_bases_and_members` n'a que deux appelants (grep : sendable.h:67, lifetime_aware.h:68) et que is_synchronizable ne l'utilise pas. Aucune difference de reponse n'est en jeu.

</details>

### SIMP-15 — Le prefixe diagnose_ ment : ces fonctions ne diagnostiquent rien, elles marchent dans le type

**DÉTAIL** · simplicité · `include/threadsafe/details/sendable.h:40`

CLAUDE.md pose l'invariant a la lettre : "The answer is a plain bool; the explanation lives in the static_assert messages at the point of use [...] never inside the trait." Et pourtant les trois fonctions centrales s'appellent `diagnose_is_sendable`, `diagnose_is_synchronizable`, `diagnose_is_lifetime_aware`. Un spectateur qui lit `diagnose_` s'attend a un diagnostic (message, chemin d'echec, raison) et va chercher ou il est produit ; il n'existe pas. Le nom decrit exactement ce que le design promet de ne PAS faire. Ce que ces fonctions font s'appelle, et le CLAUDE.md le dit lui-meme deux paragraphes plus haut, "the structural walk". Meme chose en pire dans le lanceur (asynchronous_task_launcher.h:17 et 22) ou `diagnose_scoped_task_participant` n'est meme pas un walk mais une conjonction d'une ligne.

**Code actuel**

```cpp
namespace detail {
consteval bool diagnose_is_sendable(std::meta::info type);
}

template <class T>
struct is_sendable : std::bool_constant<detail::diagnose_is_sendable(^^T)> {};
...
inline consteval bool diagnose_is_sendable(std::meta::info type) {
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le renommage propose est bon mais oublie la doc. Version complete (verifiee : 12/12 tests compilent) :

1) Sur les en-tetes, renommage a frontieres de mot :
```cpp
find include -name '*.h' -print0 | xargs -0 perl -pi -e '
  s/\bdiagnose_is_sendable\b/walk_is_sendable/g;
  s/\bdiagnose_is_synchronizable\b/walk_is_synchronizable/g;
  s/\bdiagnose_is_lifetime_aware\b/walk_is_lifetime_aware/g;
  s/\bdiagnose_scoped_task_participant\b/check_scoped_task_participant/g;
  s/\bdiagnose_task_participant\b/check_task_participant/g;'

Cela touche 13 sites : sendable.h:21,25,40 ; synchronizable_base.h:24,29,41 ; lifetime_aware.h:23,28,43 ; asynchronous_task_launcher.h:17,22,23,30,36.
```
2) AJOUT manquant dans la correction proposee — CLAUDE.md:24-25 cite le nom et deviendrait faux. Remplacer :

```cpp
Each trait is one consteval function — `detail::diagnose_is_sendable(info)`,
the structural walk — asked through a `_v` constexpr variable:

par :

Each trait is one consteval function — `detail::walk_is_sendable(info)`,
the structural walk — asked through a `_v` constexpr variable:
```
3) Aucun test ne reference ces identifiants (grep confirme : les 12 tests n'utilisent que les `_v` publics), donc aucune retouche cote tests.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Renommage complet (regex \b, sur les 12 en-tetes) applique dans .../scratchpad/audit_simplicity/clear : les 12 tests compilent, `RESULT fail=0`.

</details>

### SIMP-16 — pointee_answer(pointee, question) : un nom-substantif pour un predicat, et un parametre appele "question"

**DÉTAIL** · simplicité · `include/threadsafe/details/utils.h:27`

Trois problemes empiles sur trois lignes, sur un helper appele depuis 6 endroits. (1) `pointee_answer` est un nom de substantif ("la reponse du pointe") alors que la fonction repond oui/non : au site d'appel on lit `if (!pointee_answer(...))` = "si pas la-reponse-du-pointe". (2) le parametre s'appelle `question` alors qu'il ne pose rien : c'est le trait qu'on evalue. (3) la fonction ne fait pas que deleguer, elle ajoute silencieusement `is_dynamic_type_known` — c'est-a-dire la vraie regle metier ("derriere une indirection, on n'accorde rien a un type polymorphe non final"), que ni son nom ni aucun commentaire n'annonce. La meme paire de noms se retrouve dans `all_bases_and_members` (utils.h:40) et `all_wrapped_types` (allowed_std_wrappers.h:73). Quant au choix du pointeur de fonction plutot qu'un template, il n'est explique nulle part, et il n'interdit d'ailleurs pas les lambdas (verifie ci-dessous) : le lecteur ne peut pas deviner que c'est un choix de temps de compilation.

**Code actuel**

```cpp
inline consteval bool pointee_answer(std::meta::info pointee,
                                     bool (*question)(std::meta::info)) {
  return question(pointee) && is_dynamic_type_known(pointee);
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Ne pas renommer. Le couple question/answer est la convention documentee de CLAUDE.md ; le seul manque reel est la regle metier tue. Dans include/threadsafe/details/utils.h:27, ajouter uniquement le commentaire :

// derriere une indirection, la reponse exige en plus que le type dynamique
// soit connu : un pointeur vers une base polymorphe non finale peut
// designer n'importe quel derive, dont on ne sait rien
inline consteval bool pointee_answer(std::meta::info pointee,
```cpp
                                   bool (*question)(std::meta::info)) {
return question(pointee) && is_dynamic_type_known(pointee);
```
}

Verifie : 12/12 tests compilent (ALTFIX fail=0).

Si l'auteur tient malgre tout a l'alias pour lisibilite, l'ecrire SANS la justification perf inventee (un template n'instancierait que 3 fois, une par trait) :
using trait_question = bool (*)(std::meta::info);
et l'utiliser dans pointee_answer, all_bases_and_members (utils.h:40) et all_wrapped_types (allowed_std_wrappers.h:73) en gardant le parametre nomme `question`.

En complement, documenter la regle dans CLAUDE.md a cote de « Const behind an indirection is never trusted », qui est aujourd'hui la seule regle d'indirection enoncee : « Derriere une indirection, un type polymorphe non final n'obtient rien : son type dynamique est inconnu. »

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Renommage applique dans .../scratchpad/audit_simplicity/clear : les 12 tests compilent, `RESULT fail=0`. Sur la question "lambda ou pas" : sonde .../scratchpad/audit_simplicity/probes/lambda.cpp = `static_assert(threadsafe::detail::all_bases_and_members(^^Plain, [](std::meta::info t) consteval { return threadsafe::is_sendable_type(t); }));` -> compile. Une lambda sans capture se convertit deja en bool(*)(std::meta::info) ; le pointeur de fonction n'est donc pas une contrainte pour l'appelant, seulement un choix non explique.

</details>

## Performance à la compilation

### PC-01 — Le build des tests paie deux passes par TU (scan de modules) et aucun PCH : -64 % disponibles pour 2 lignes de CMake

**MAJEUR** · perf. compilation · `tests/CMakeLists.txt:15`

Le chiffre le plus important de cet audit : dans une TU de test, 83 % du temps est du re-parsing d'en-tetes standard, pas du walk reflectif. Mesure (Apple M3 Pro, g++-16 16.2.0, min de 5 runs) : TU vide = 0.033 s ; `#include <threadsafe/threadsafe.h>` + `int main(){}` = 0.623 s ; le test le plus lourd (test_soundness_regressions.cpp, 207 lignes) = 0.754 s. Le walk complet de ce fichier ne coute donc que 0.131 s, soit 17 % de la TU. Ce cout d'en-tetes est paye 12 fois (une par test). Deux reglages CMake l'attaquent directement. (1) `target_compile_features(... cxx_std_26)` active par defaut le scan de dependances de modules C++20 : CMake genere une regle `CXX_COMPILER__threadsafe_tests_scanned_` avec un `PREPROCESSED_OUTPUT_FILE ... .ddi.i`, donc chaque test est preprocessé une fois en plus, pour rien (le projet n'a aucun module). (2) sans PCH, les ~490 en-tetes (verifie avec `-H` : 486 lignes) sont relus 12 fois. Le scan doit etre coupe AVANT que le PCH ne serve a quelque chose : la passe de scan n'utilise pas le .gch, donc le PCH seul ne gagne rien. Matrice mesuree (`cmake --build`, tous les .cpp touches, min de 3, rc=0 partout) :
  actuel (no PCH, scan ON)  : -j1 10.85 s   -j12 1.77 s
  PCH seul (scan ON)        : -j1 10.78 s   -j12 1.72 s   (aucun gain)
  scan OFF seul             : -j1  8.41 s   -j12 1.26 s   (-23 % / -29 %)
  scan OFF + PCH            : -j1  3.87 s   -j12 0.62 s   (-64 % / -65 %)
Sur une seule TU avec les flags exacts de CMake, le PCH fait passer test_sendable.cpp de 0.692 s a 0.302 s (-56 %). Cout : le .gch pese 129 Mo. Commandes : `cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-16 -G Ninja && cmake --build build -j12`, chronometre en Python (time.perf_counter).

**Code actuel**

```cpp
add_library(threadsafe_tests OBJECT
    test_synchronizable.cpp
    ...
    test_diagnostics.cpp
)
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Dans `tests/CMakeLists.txt`, après la ligne 15 (`target_link_libraries(...)`). L'ordre des deux réglages n'est pas décoratif : mesuré, le PCH seul ne gagne rien tant que la passe de scan est active.

```cmake
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)

# Le projet n'a aucun module C++20. Active par defaut avec cxx_std_26, la passe
# de scan preprocesse chaque test une seconde fois pour rien -- et elle n'utilise
# pas le .gch, donc elle annule le benefice du PCH tant qu'elle est la.
# A retirer le jour ou la bibliotheque sera livree en module.
set_target_properties(threadsafe_tests PROPERTIES CXX_SCAN_FOR_MODULES OFF)

# 80 % d'une TU de test est du re-parsing d'en-tetes standard (474 en-tetes,
# 0.63 s sur 0.79 s) ; le walk reflectif ne coute que 0.12 s. On ne paie les
# en-tetes qu'une fois au lieu de douze. Cout : un .gch de ~129 Mo dans build/.
target_precompile_headers(threadsafe_tests PRIVATE <threadsafe/threadsafe.h>)
```

Vérifié : `cmake --build` rc=0 sur les 12 tests, et un `static_assert` faux injecté fait bien échouer le build sous PCH — les tests compile-time continuent de s'exécuter.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Copies du repo dans le scratchpad (repo0 = tel quel, repo = +PCH, repo0_ns = +scan OFF, repo_ns = +scan OFF +PCH), 4 arbres CMake/Ninja distincts, tous les test_*.cpp touches avant chaque mesure, min de 3 runs, exit code 0 verifie pour les 4 configurations :
actuel (no PCH, scan ON)   -j1=10.85s  -j12=1.77s
PCH seul (scan ON)         -j1=10.78s  -j12=1.72s
scan OFF seul              -j1=8.41s   -j12=1.26s
scan OFF + PCH             -j1=3.87s   -j12=0.62s
Preuve que le PCH est bien consomme : `g++-16 ... -include cmake_pch.hxx -H -fsyntax-only test_sendable.cpp` sort 6 lignes (dont `! .../cmake_pch.hxx.gch`) contre 486 lignes sans PCH. Preuve de la double passe : `grep 'test_sendable.cpp.o: CXX_COMPILER' build.ninja` -> regle `CXX_COMPILER__threadsafe_tests_scanned_` + `PREPROCESSED_OUTPUT_FILE = ....ddi.i`.

Contre-vérification indépendante :

```text
(a) repo0: 12   repo_pch: 12   repo0_ns: 0   repo_ns: 0
    PREPROCESSED_OUTPUT_FILE = tests/CMakeFiles/threadsafe_tests.dir/test_synchronizable.cpp.o.ddi.i
    build tests/CMakeFiles/threadsafe_tests.dir/test_synchronizable.cpp.o: CXX_COMPILER__threadsafe_tests_scanned_ ...

(b) Run A (min de 3) :
      repo0      -j1= 10.62s  -j12= 2.11s
      repo_pch   -j1= 12.03s  -j12= 2.27s
      repo0_ns   -j1=  9.35s  -j12= 2.04s
      repo_ns    -j1=  4.87s  -j12= 0.95s
    Run B (min de 5) :
      repo0      -j1= 12.13s  -j12= 1.84s
      repo0_ns   -j1=  8.73s  -j12= 1.60s
      repo_ns    -j1=  4.09s  -j12= 0.68s
    → correction complète : -54 % a -66 % (-j1), -55 % a -63 % (-j12). PCH seul : aucun gain.
    Taille du .gch : 129M (ls -lh cmake_pch.hxx.gch)

(c) empty_TU 0.036   header_only 0.666   soundness 0.786   sendable_noPCH 0.667
    nombre d'en-tetes inclus : 474
[…]
```

</details>

### PC-02 — threadsafe.h impose <mutex>, <shared_mutex> et <thread> a qui ne veut que les traits : +36 % par TU

**MAJEUR** · perf. compilation · `include/threadsafe/threadsafe.h:3`

L'en-tete unique melange la couche de traits (is_sendable / is_synchronizable / is_lifetime_aware) et les facilites runtime (synchronized_value, copy_on_write, asynchronous_task_launcher). Ces dernieres tirent <mutex>, <shared_mutex>, <thread>, qui sont les en-tetes standard les plus chers de tout le graphe sur cette machine : <mutex> seul = 0.443 s, <thread> seul = 0.402 s, <shared_mutex> seul = 0.271 s (contre <meta> 0.191 s, <vector> 0.176 s, <variant> 0.056 s). Un utilisateur qui veut seulement interroger is_sendable_v<T> les paie quand meme. Mesure : `#include <threadsafe/threadsafe.h>` = 0.623 s ; un `threadsafe/traits.h` regroupant uniquement allowed_std_wrappers / synchronizable / sendable / smart_pointers / vocabulary / lifetime_aware = 0.459 s. Gain : 0.164 s par TU, soit -26 %.

A noter, contre l'intuition : les 16 en-tetes de conteneurs d'allowed_std_wrappers.h (<map>, <unordered_map>, <variant>, <tuple>, <deque>, <forward_list>, <set>, <unordered_set>...) ne sont PAS le probleme. Mesure : traits de base seuls (sendable+synchronizable+lifetime_aware, sans allowed_std_wrappers) = 0.390 s ; en ajoutant allowed_std_wrappers + smart_pointers + vocabulary = 0.459 s. Les conteneurs ne coutent que 0.069 s (+18 % des traits, 11 % de threadsafe.h) parce que <memory>, <ranges>, <functional> et <meta>, deja indispensables, ont deja tire l'essentiel. Un `threadsafe/std.h` separe pour les conteneurs n'en vaut pas la peine ; separer les facilites, oui.

**Code actuel**

```cpp
#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/details/synchronized_value.h>
#include <threadsafe/details/copy_on_write.h>
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le decoupage propose est repris tel quel, avec en plus le basculement des consommateurs traits-only — sans lui le gain reste theorique.

1) Nouveau include/threadsafe/traits.h :

```cpp
#pragma once

#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/synchronizable.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/smart_pointers.h>
#include <threadsafe/details/vocabulary.h>
#include <threadsafe/details/lifetime_aware.h>
```
2) include/threadsafe/threadsafe.h devient (sur-ensemble strict, aucune rupture pour l'existant) :

```cpp
#pragma once

#include <threadsafe/traits.h>
#include <threadsafe/details/asynchronous_task_launcher.h>
#include <threadsafe/details/synchronized_value.h>
#include <threadsafe/details/copy_on_write.h>
```
3) AJOUT indispensable — basculer sur `#include <threadsafe/traits.h>` les 7 TU de tests qui n'utilisent aucune facilite runtime :
```cpp
tests/test_containers.cpp, test_deferred_specialization.cpp, test_diagnostics.cpp,
test_lifetime_aware.cpp, test_polymorphic.cpp, test_smart_pointers.cpp, test_synchronizable.cpp
(laisser test_sendable.cpp, test_copy_on_write.cpp, test_synchronized_value.cpp,
test_asynchronous_task_launcher.cpp, test_soundness_regressions.cpp sur threadsafe.h).
Mesure : -1.467 s (-16.2 %) sur le build de tests complet, et ces 7 TU servent de test de
non-regression du decoupage (si une facilite fuit un jour dans la couche de traits, ils cassent).
```
4) Une ligne dans CLAUDE.md, section "Architecture: the traits" : `<threadsafe/traits.h>` pour
```cpp
interroger is_sendable_v / is_synchronizable_v / is_lifetime_aware_v sans payer <mutex>,
<shared_mutex> ni <thread> ; `<threadsafe/threadsafe.h>` pour tout le reste. Utile pour une
bibliotheque a vocation pedagogique : le decoupage physique rend visible le decoupage logique
que le document revendique deja.
```
Ne PAS faire, verifie par la mesure : un `threadsafe/std.h` separe pour les 16 conteneurs
d'allowed_std_wrappers.h. Ils ne pesent que ~0.07 s (<memory>, <ranges>, <functional>, <meta>,
deja indispensables, ont paye l'essentiel) — la complexite ne serait pas remboursee.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Copie de l'arbre include dans variant_split/ avec l'ajout de threadsafe/traits.h, puis `g++-16 -std=c++26 -freflection -fsyntax-only -I <dir> <probe>.cpp` chronometre en Python, min de 5 runs :
  TU vide                        : 0.033 s
  #include <threadsafe/threadsafe.h> : 0.623 s
  #include <threadsafe/traits.h>     : 0.459 s   (-26 %)
  traits de base seuls (sendable+synchronizable+lifetime_aware) : 0.390 s
Cout des en-tetes standard pris isolement (meme protocole, min de 3) : mutex 0.443, thread 0.402, shared_mutex 0.271, stop_token 0.249, ranges 0.246, memory 0.240, functional 0.210, meta 0.191, unordered_map 0.180, unordered_set 0.179, vector 0.176, map 0.174, deque 0.171, array 0.163, string 0.162, algorithm 0.116, set 0.095, list 0.090, forward_list 0.090, tuple 0.075, optional 0.075, atomic 0.074, variant 0.056, utility 0.048, type_traits 0.037, concepts 0.037, cstddef 0.034.
```

Contre-vérification indépendante :

```text
Par TU (min / mediane sur 9 runs) :
  empty                    min=0.034 med=0.035
  threadsafe.h (orig)      min=0.669 med=0.684
  threadsafe.h (split)     min=0.696 med=0.700
  traits.h (split)         min=0.496 med=0.504   -> -0.173 s, -26 %
  <mutex>                  min=0.481 med=0.489
  <thread>                 min=0.467 med=0.825
  <shared_mutex>           min=0.312 med=0.347
  <meta>                   min=0.223 med=0.228

Deux series de controle supplementaires :
  base traits  min=0.425 / 0.437
  traits.h     min=0.548 / 0.501
  threadsafe.h min=0.721 / 0.683
  -> conteneurs d'allowed_std_wrappers ~0.07 s ; facilites runtime ~0.19 s. Claim contre-intuitif confirme.

Localisation des en-tetes chers (grep) :
  details/synchronized_value.h:5,6 -> <mutex>, <shared_mutex>
  details/asynchronous_task_launcher.h:5 -> <thread>
  aucun de ces trois n'apparait ailleurs dans include/.

[…]
```

</details>

### PC-03 — Le detour trait_value coute ~78 us par arete du graphe, meme quand la reponse est deja memoisee

**MINEUR** · perf. compilation · `include/threadsafe/details/utils.h:8`

La memoisation par `_v` fonctionne (voir le finding informatif separe) : le CORPS du walk n'est execute qu'une fois par type. Mais le CHEMIN D'ACCES a la reponse memoisee, lui, n'est pas gratuit : chaque arete du graphe de types repasse par `extract<bool>(substitute(^^is_sendable_v, {type}))`, et ce round-trip coute le meme prix qu'il s'agisse d'une premiere visite ou d'un memo-hit.

Micro-mesure isolee (aucun en-tete ThreadSafe, juste <meta> + un `template<class T> constexpr bool trivial_v = std::is_class_v<T>;`) :
  - 2000 appels `is_class_type(^^Ti)` directs sur 2000 types distincts : +0.014 s -> 7 us/appel
  - 2000 appels `extract<bool>(substitute(^^trivial_v,{^^Ti}))` sur 2000 types distincts : +0.120 s -> 60 us/appel
  - 20000 appels du meme substitute sur LE MEME type T0 (donc 19999 memo-hits) : +0.991 s -> 50 us/appel
Le substitute coute donc ~8x une primitive de reflection, et la memoisation ne le rend pas moins cher.

Mesure dans le walk reel : `struct Big { int m0..m3999; }` + `static_assert(is_sendable_v<Big>)` = 0.971 s ; le meme fichier sans le static_assert = 0.650 s ; le fichier vide equivalent = 0.632 s. Le parsing des 4000 membres coute 0.018 s, le walk 0.311 s, soit 78 us pour une arete qui ne fait que redemander `is_sendable_v<int>`.

On peut court-circuiter sans rien changer a la semantique pour les types arithmetiques : `diagnose_is_sendable` repond deja `true` pour tout scalaire (sendable.h:61) et `diagnose_is_lifetime_aware` aussi (lifetime_aware.h:62). La couche `is_unsafe_<trait>` ne peut qu'ACCORDER la confiance, donc une specialisation utilisateur sur `int` ne pourrait de toute facon que redire `true` : le raccourci est exactement equivalent. Attention : ne PAS faire le meme raccourci dans `is_synchronizable_type` — la non-constance y repond `false` (synchronizable_base.h:53) et un `is_unsafe_synchronizable<int>` ecrit par un utilisateur doit pouvoir renverser ce non.

Tension a assumer : c'est un chemin rapide, donc du bruit dans un code a vocation pedagogique. Sur la suite de tests actuelle le gain est nul (les types y sont minuscules) ; il ne se voit que sur de vrais types larges.

**Code actuel**

```cpp
inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
  return extract<bool>(substitute(trait, {type}));
}

// include/threadsafe/details/sendable.h:34
inline consteval bool is_sendable_type(std::meta::info type) {
  return detail::trait_value(^^is_sendable_v, type);
}

// include/threadsafe/details/lifetime_aware.h:37
inline consteval bool is_lifetime_aware_type(std::meta::info type) {
  return detail::trait_value(^^is_lifetime_aware_v, type);
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee est correcte telle quelle : elle compile, preserve la semantique, et les 12 tests passent. Je la recopie sans modification.

// include/threadsafe/details/sendable.h:34
inline consteval bool is_sendable_type(std::meta::info type) {
```cpp
if (is_arithmetic_type(remove_cv(type)))
  return true;
return detail::trait_value(^^is_sendable_v, type);
```
}

// include/threadsafe/details/lifetime_aware.h:37
inline consteval bool is_lifetime_aware_type(std::meta::info type) {
```cpp
if (is_arithmetic_type(remove_cv(type)))
  return true;
return detail::trait_value(^^is_lifetime_aware_v, type);
```
}

Deux garde-fous a respecter si elle est appliquee :
1. NE PAS porter le raccourci dans `is_synchronizable_type` (synchronizable_base.h:35). La non-constance
```cpp
y repond false (ligne 53), donc un `is_unsafe_synchronizable<int>` ecrit par un utilisateur doit
pouvoir renverser ce non ; le raccourci l'en empecherait. Verifie : le raccourci n'est equivalent
que la ou le trait repond deja true inconditionnellement pour tout scalaire.
```
2. Documenter en une ligne pourquoi c'est sur, sinon le lecteur d'une bibliotheque pedagogique lit
```cpp
deux lignes qui repondent avant le walk sans savoir qu'elles ne peuvent rien changer :
// `is_unsafe_sendable` ne peut qu'ACCORDER la confiance et tout type arithmetique est deja
// sendable (diagnose_is_sendable, is_scalar_type) : ce raccourci est exactement equivalent.
```
RECOMMANDATION : ne pas appliquer aujourd'hui. Le gain mesure est reel (-12 a -20 % sur des TU a
types larges) mais vaut exactement zero sur ce depot (+1.5 % sur la suite, soit du bruit), et il se
paie en clarte dans un code dont CLAUDE.md dit qu'il est fait pour etre lu en conference. A garder en
reserve, avec les chiffres ci-dessus, pour le jour ou la bibliotheque serait utilisee sur une base de
code large. Si l'auteur l'applique quand meme, factoriser le predicat dans utils.h
(`inline consteval bool is_trivially_trusted(std::meta::info t) { return is_arithmetic_type(remove_cv(t)); }`)
plutot que dupliquer la condition dans deux en-tetes.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
Micro-benchmark autonome (scratchpad/audit_perfcomp/sub_*.cpp), min de 3 runs :
  direct  N=0 0.212 s | N=500 0.219 s | N=2000 0.226 s  -> 7 us/appel
  subst   N=0 0.209 s | N=500 0.244 s | N=2000 0.329 s  -> 60 us/appel
  subst sur le MEME type  N=0 0.212 s | N=2000 0.313 s | N=20000 1.203 s -> 50 us/memo-hit
Walk reel (`struct Big{int m0..m3999;}`), min de 4 runs :
  avec static_assert 0.971 s | sans static_assert 0.650 s | parse seul du fichier vide 0.632 s -> walk = 0.311 s / 4000 aretes = 78 us
Variante corrigee (copie de l'arbre include dans variant_arith/ puis variant_all/) :
  si_4000  : 0.971 -> 0.804 s (-17 %)
  distinct_1500 (1500 structs distinctes interrogees une fois) : 1.524 -> 1.445 s (-5 %)
  s_400 (struct a 400 membres de types distincts) : 0.940 -> 0.930 s
Les 12 tests du repo compilent sans erreur contre variant_all (`g++-16 -std=c++26 -freflection -fsyntax-only -I variant_all tests/test_*.cpp`, rc=0 pour les 12).
```

</details>

### PC-04 — wrapped_types_of alloue un std::vector consteval par arete de conteneur std

**MINEUR** · perf. compilation · `include/threadsafe/details/allowed_std_wrappers.h:59`

`all_wrapped_types` n'a besoin que de parcourir les arguments de template, mais passe par `wrapped_types_of` qui materialise un `std::vector<std::meta::info>` : une allocation consteval + un remplissage + une destruction a chaque noeud de conteneur std du graphe. Comme `all_wrapped_types` est appele depuis les trois specialisations `is_unsafe_<trait>` des std_wrapper (lignes 86, 91, 96), un `std::map<K,V>` paie cela plusieurs fois. Mesure : 600 `static_assert(is_sendable_v<std::vector<Di>>)` sur 600 types distincts = 1.990 s, le meme fichier avec `sizeof(...)>0` a la place = 1.177 s, donc 0.800 s de walk. En remplacant le vector par une boucle directe : 1.850 s, soit 0.140 s de moins (-7 % du fichier, -12 % du walk), gain reproduit 3 fois sur 3 (1.969/1.936, 2.035/1.923, 1.981/1.949). Gain modeste mais gratuit : la version sans allocation est aussi courte et aussi lisible que l'originale. `wrapped_types_of` reste utile pour `pointee_is_lifetime_aware` (smart_pointers.h:16) qui a vraiment besoin de la sequence.

**Code actuel**

```cpp
inline consteval bool all_wrapped_types(std::meta::info type,
                                        bool (*question)(std::meta::info)) {
  for (auto wrapped : wrapped_types_of(type))
    if (!question(wrapped))
      return false;

  return true;
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Remplacer `all_wrapped_types` (allowed_std_wrappers.h:72-79) par la boucle directe, en laissant `wrapped_types_of` en place pour `smart_pointers.h:16` :

inline consteval bool all_wrapped_types(std::meta::info type,
```cpp
                                      bool (*question)(std::meta::info)) {
const bool wrapper_is_const = is_const(type);

for (auto argument : template_arguments_of(dealias(type))) {
  if (!is_type(argument))
    continue;

  const auto wrapped = wrapper_is_const ? add_const(remove_cv(argument))
                                        : remove_cv(argument);
  if (!question(wrapped))
    return false;
}

return true;
```
}

Verifie : compile, les 12 tests du repo passent, semantiquement identique (meme ensemble d'appels a `question`, meme ordre, meme sortie anticipee). Gain mesure sur cette machine : -0.100 s CPU sur un fichier de 2.12 s (-4.7 % du fichier, -11 % du walk des traits), et jusqu'a -0.39 s sur un fichier a base de `std::map`.

Une seule nuance a porter au rapport : annoncer -0.09 a -0.10 s et -4.7 % du fichier, pas -0.140 s et -7 % — la magnitude d'origine est surevaluee d'environ 40 %, meme si la fraction-du-walk annoncee (-12 %) est juste.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Copie de l'arbre include dans variant_noalloc/ avec ce seul changement. `g++-16 -std=c++26 -freflection -fsyntax-only -I <dir> w_vec.cpp` (600 `static_assert(is_sendable_v<std::vector<Di>>)`), min de 4 runs, 3 series independantes :
```text
base 1.969 / noalloc 1.936
base 2.035 / noalloc 1.923
base 1.981 / noalloc 1.949
```
Reference sans trait (`sizeof(std::vector<Di>)>0`) : 1.177 s.
Les 12 tests du repo compilent sans erreur contre variant_noalloc (rc=0 pour les 12).

</details>

### PC-05 — Au-dela d environ 50 niveaux d imbrication a travers un wrapper std, la profondeur constexpr de 512 explose en 156 erreurs

**DÉTAIL** · perf. compilation · `include/threadsafe/details/utils.h:9`

Chaque niveau d'imbrication coute une dizaine de cadres constexpr : `trait_value` -> substitute/extract -> `is_unsafe_sendable<std::vector<...>>` -> `all_wrapped_types` -> `std::ranges::contains`/`all_of`. A 60 niveaux la limite GCC par defaut (`-fconstexpr-depth=512`) est atteinte dans l'implementation de <ranges>, et l'utilisateur recoit 156 erreurs dont les deux premieres pointent dans des en-tetes libstdc++, aucune dans ThreadSafe. Le memo `_v` ne sauve pas : c'est la profondeur de la pile d'evaluation de la premiere descente, pas le nombre d'instanciations. Seuil mesure entre 50 (OK) et 60 (echec) pour la forme `struct L{i} { std::vector<L{i-1}> v; L{i-1} d; };` ; l'imbrication sans wrapper std (`struct L{i} { L{i-1} d; };`) tient sans probleme a 60. Impact reel faible — une profondeur de 50 est rare — mais le mode d'echec est illisible.

**Code actuel**

```cpp
inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
  return extract<bool>(substitute(trait, {type}));
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee par l'auditeur est la BONNE modification, mais son benefice annonce (repousser le seuil) est nul ; son vrai benefice est de ramener l'erreur dans ThreadSafe. A appliquer pour cette raison-la seulement.

Dans include/threadsafe/details/allowed_std_wrappers.h:50-54, remplacer :

```cpp
inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
  type = dealias(type);
  return has_template_arguments(type) &&
         std::ranges::contains(allowed_std_wrappers, template_of(type));
}
```
par :

```cpp
inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
  type = dealias(type);
  if (!has_template_arguments(type))
    return false;

  const std::meta::info wrapper = template_of(type);
  for (std::meta::info allowed : allowed_std_wrappers)
    if (allowed == wrapper)
      return true;

  return false;
}
```
Mesure reelle de cet effet (verifie, pas suppose) :
- seuil inchange : passe a N=50, echoue a N=55, avant comme apres ;
- erreur de profondeur relocalisee de bits/invoke.h + bits/ranges_base.h vers include/threadsafe/details/utils.h:9, et 1 erreur de profondeur au lieu de 2 ;
- les 8 tests/test_*.cpp compilent sans regression.

Le `#include <algorithm>` de la ligne 3 reste necessaire par ailleurs (d'autres en-tetes du walk s'en servent) ; ne pas le retirer sans verifier.

Le VRAI remede a la profondeur, lui, n'est pas dans le code : documenter `-fconstexpr-depth=` comme reponse au message. Verifie : `-fconstexpr-depth=2048` fait compiler le cas N=60 non patche avec exit=0.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

SONDE q_d.cpp generee (N=60) :
```text
struct L0 { int x; };
struct L1 { std::vector<L0> v; L0 d; };  ... struct L60 { std::vector<L59> v; L59 d; };
static_assert(threadsafe::is_sendable_v<L60>);
```
SORTIE :
```text
/opt/homebrew/Cellar/gcc/16.2.0/include/c++/16/bits/invoke.h:98:73: error: 'constexpr' evaluation depth exceeds maximum of 512 (use '-fconstexpr-depth=' to increase the maximum)
/opt/homebrew/.../bits/ranges_base.h:134:29: error: 'constexpr' evaluation depth exceeds maximum of 512 ...
(grep -c ' error: ' = 156)
```
BALAYAGE du seuil, meme forme : N=8,9,10,11,15,20,30,35,40,45,50 -> 0 erreur ; N=60 -> 2 erreurs de profondeur.
Forme sans wrapper std (struct L{i} { L{i-1} d; }) a N=60 -> 0 erreur de profondeur.

</details>

### PC-06 — Mesure negative : la memoisation par _v fonctionne, y compris a travers les aretes du graphe

**DÉTAIL** · perf. compilation · `include/threadsafe/details/sendable.h:28`

Le soupcon classique — « diagnose_* est consteval, donc substitute() reexecute le walk a chaque arete » — est FAUX ici, mesures a l'appui. Le compilateur instancie bien `is_sendable_v<T>` une seule fois par T et par TU, et toute arete ulterieure vers T recupere la valeur sans rejouer le corps.

(1) Requete repetee au sommet : `static_assert(is_sendable_v<Big>)` avec Big a 200 membres de types distincts, repete 1, 10, 100 et 1000 fois -> 0.727 / 0.742 / 0.741 / 0.736 s. Mille requetes coutent exactement une requete.
(2) Sous-arbre partage (le vrai test de la memoisation inter-aretes) : Heavy a 200 membres de types distincts, reference par W wrappers eux-memes membres d'un Top.
   Heavy lourd + 1 wrapper    : 0.780 s
   Heavy lourd + 200 wrappers : 0.903 s
   Heavy trivial + 200 wrappers : 0.761 s
   Delta du a Heavy vu depuis 200 wrappers = 0.142 s, c'est-a-dire le cout d'UN SEUL walk de Heavy (0.780 - 0.590 = 0.190 s), pas de 200.
(3) Croissance mesuree partout lineaire : struct a N membres distincts N=0/25/50/100/200/400 -> 0.590/0.607/0.661/0.672/0.750/0.899 (~0.77 ms par type distinct) ; chaine d'heritage D=0/10/30/60 -> 0.635/0.651/0.667/0.705 (~1.2 ms/niveau) ; std::vector imbrique D=0/5/10/20/40 -> 0.637/0.653/0.689/0.672/0.695 (~1.4 us... lineaire et quasi plat).
(4) Le seul cas d'apparence superlineaire, `std::tuple<T0..T199>` (0.622/0.647/0.677/0.764/0.979 pour N=0/25/50/100/200), vient de libstdc++ et non du walk : le meme fichier avec `sizeof(Tup)>0` au lieu du trait coute deja 0.840 s sur les 0.979 s. Le walk n'apporte que 0.139 s.
Aucune correction a proposer : ce point du cahier des charges est un faux probleme. Ce qui reste cher, c'est le prix unitaire de l'arete memoisee, traite dans le finding sur trait_value.

**Code actuel**

```cpp
template <class T>
constexpr bool is_sendable_v =
    detail::assert_queryable_type<T>() && is_sendable<T>::value;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Aucune correction. Ne rien changer a `is_sendable_v` ni au passage par `detail::trait_value`.

Ce finding doit rester au rapport comme constat informatif : il ferme une piste d'optimisation plausible mais fausse. Le raisonnement « `diagnose_is_sendable` est consteval, donc `substitute()` rejoue le walk a chaque arete » est refute par la mesure — la recursion passe par l'instanciation de la variable template `is_sendable_v<T>`, que GCC memoise une fois par T et par TU, y compris quand T est atteint par des centaines d'aretes distinctes. Toute reecriture visant a « ajouter un cache » au walk serait un cout de lisibilite pour un gain nul.

Deux ameliorations de la *redaction* du finding, sans effet sur sa conclusion :
1. Remplacer l'arithmetique du point (2) par le factoriel 2x2 (largeur de Heavy x nombre de wrappers) mesure ici. L'auditeur compare un delta mesure dans un fichier (sh_200_200 - sh_1_200) a un cout de walk estime dans un autre (sh_200_1 - only_include), ce qui melange cout du walk et cout de declaration des types. Le 2x2 montre directement que le surcout de la largeur du sous-arbre partage est independant du nombre d'aretes (+0.155 s a 1 arete, +0.138 s a 200 aretes pour h=200 ; +0.298 vs +0.398 pour h=400), et permet l'argument massue : sans memoisation, h=400/w=200 couterait ~60 s au lieu des 1.245 s mesurees.
2. Donner les temps en delta sur la baseline `only_include` plutot qu'en absolu : les absolus varient de ~10-13 % d'une machine/charge a l'autre (0.610 s ici contre 0.590 s annonces pour la meme baseline), alors que les deltas sont stables et sont ce que le finding affirme reellement.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Tous les fichiers generes dans scratchpad/audit_perfcomp (rep_*.cpp, sh_*.cpp, s_*.cpp, inh_*.cpp, nest_*.cpp, tup_*.cpp, tupna_*.cpp), compiles avec `g++-16 -std=c++26 -freflection -fsyntax-only -I include <f>.cpp`, min de 3 runs :
repeat x1/x10/x100/x1000 (width 200) : 0.727 / 0.742 / 0.741 / 0.736
sh_200_1 0.780 | sh_200_50 0.773 | sh_200_200 0.903 | sh_1_200 0.761
struct N=0/25/50/100/200/400 : 0.590 / 0.607 / 0.661 / 0.672 / 0.750 / 0.899
inherit D=0/10/30/60 : 0.635 / 0.651 / 0.667 / 0.705
nest D=0/5/10/20/40 : 0.637 / 0.653 / 0.689 / 0.672 / 0.695
tuple N=0/25/50/100/200 avec trait : 0.622 / 0.647 / 0.677 / 0.764 / 0.979
tuple N=0/50/100/200 sans trait (sizeof) : 0.623 / 0.645 / 0.680 / 0.840
-ftime-report sur distinct_1500.cpp (1500 types distincts) : phase parsing 84 %, constant expression evaluation 34 %, template instantiation 19 %, constraint satisfaction 17 %, overload resolution 17 %. Sur only_include.cpp : constant expression evaluation 2 %. `-ftime-trace` n'existe pas dans g++-16 (`unrecognized command-line option`).

</details>

### PC-07 — Mesure negative : assert_queryable_type et std::remove_all_extents_t ne coutent rien

**DÉTAIL** · perf. compilation · `include/threadsafe/details/utils.h:12`

`assert_queryable_type<T>()` est bien evalue dans chaque `_v`, donc a chaque noeud du walk, et il instancie deux templates standard par noeud (`std::is_void_v<T>` et l'alias `std::remote_all_extents_t<T>`). L'hypothese etait que ces instanciations standard pesent. Mesure : non. Une variante remplacant les deux par les primitives de reflection (`is_void_type(^^T)` et `remove_all_extents(^^T)`, deja utilisees en utils.h:23) est neutre ou legerement PLUS LENTE : sur distinct_1500.cpp (1500 types distincts, donc 1500 executions de assert_queryable_type) base 1.567 s contre variante 1.613 s ; sur si_4000.cpp base 0.974 s contre variante 0.973 s ; sur s_400.cpp base 0.950 s contre variante 0.920 s (dans le bruit). GCC memoise ces alias standard aussi bien que ses propres primitives. Aucune raison de sacrifier la lisibilite des messages de static_assert pour cela.

Dans le meme registre, une autre hypothese testee et refutee : les specialisations partielles contraintes (`template <detail::std_wrapper T> struct is_unsafe_sendable<T>`) obligent le compilateur a evaluer `is_allowed_std_wrapper(^^T)` pour chaque T atteint par le walk, et -ftime-report attribue 17 % du temps a la satisfaction de contraintes sur distinct_1500.cpp. Une variante supprimant purement et simplement les trois specialisations std_wrapper donne 1.561 s contre 1.552 s : ce temps de contraintes ne vient pas de la ; `has_template_arguments(type)` court-circuite deja le `std::ranges::contains` pour tout type non-template.

**Code actuel**

```cpp
template <class T> consteval bool assert_queryable_type() {
  static_assert(!std::is_void_v<T>,
                "void is not a value: there is nothing to send, share or "
                "keep alive");
  static_assert(is_complete_type(^^std::remove_all_extents_t<T>),
                "an incomplete type has unknown members: complete it before "
                "asking the traits");
  return true;
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Aucune correction. Ne pas toucher `assert_queryable_type` : garder `!std::is_void_v<T>` et `is_complete_type(^^std::remove_all_extents_t<T>)`, et garder les trois specialisations `template <detail::std_wrapper T> struct is_unsafe_<trait>` de allowed_std_wrappers.h.

Ce finding est une mesure negative a conserver telle quelle dans le rapport : elle documente deux pistes d'optimisation testees et refutees, et vaccine contre une reecriture future qui ne gagnerait rien.

Deux rectificatifs a porter dans le texte du finding avant publication :
1. Le champ `problem` ecrit « std::remote_all_extents_t » ; lire `std::remove_all_extents_t`.
2. Remplacer « neutre ou legerement PLUS LENTE » par simplement « neutre » : la direction n'est pas reproductible (mes runs interleaves donnent la variante marginalement plus rapide, -1.9 % a -3.6 % sur le min ; une serie non interleavee donne l'inverse). Seule l'absence de gain est etablie.
3. Ajouter la reserve de methode sur variant_noconcept : retirer les trois specialisations rend `std::vector`/`std::map`/`std::string` non-sendable, donc tout benchmark contenant un wrapper std ne compile plus et son chronometrage ne veut rien dire. La mesure n'est valable que sur un benchmark sans wrapper std (j'ai refait avec plain_1500.cpp : 1500 structs de scalaires purs). Sur cette base, la conclusion tient : `-ftime-report` donne constraint satisfaction 0.53 s des deux cotes.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

```text
variant_aqt/ = copie de l'arbre include avec `!is_void_type(^^T)` et `is_complete_type(remove_all_extents(^^T))`. `g++-16 -std=c++26 -freflection -fsyntax-only -I <dir> <f>.cpp`, min de 3 a 4 runs :
  distinct_1500 : base 1.567 s / variante 1.613 s
  si_4000       : base 0.974 s / variante 0.973 s
  s_400         : base 0.950 s / variante 0.920 s
variant_noconcept/ = copie avec les trois `template <detail::std_wrapper T> struct is_unsafe_<trait>` supprimees :
  distinct_1500 : base 1.552 s / variante 1.561 s
  -ftime-report sur la variante : constraint satisfaction toujours 17 %, constant expression evaluation 34 %, TOTAL 1.60 s — identiques a la base.
```

</details>

## Performance à l'exécution

### PR-01 — synchronized_value choisit std::shared_mutex par defaut : 1,3x a 163x plus lent et 208 octets pour un int

**MAJEUR** · perf. exécution · `include/threadsafe/details/synchronized_value.h:47`

`shared_readable` est vrai des que `is_synchronizable_v<const T>` l'est, c'est-a-dire pour presque TOUS les types courants (int, std::vector<int>, std::string...). Le type bascule alors automatiquement sur std::shared_mutex. Sur cette plateforme (macOS/arm64, libstdc++ GCC 16, std::shared_mutex = pthread_rwlock_t) c'est le pire choix possible dans le regime d'utilisation normal d'un synchronized_value : section critique courte.

Deux consequences mesurees :

1) TAILLE. sizeof(std::shared_mutex) = 200 octets contre 64 pour std::mutex. Donc sizeof(synchronized_value<int>) = 208 octets, offsetof(value_) = 200 : le mutex represente 98,1 % de l'objet, soit 52x la taille de la donnee protegee. synchronized_value<std::vector<int>> = 224 octets pour 24 octets de payload.

2) VITESSE. Mesure end-to-end sur le VRAI type de la bibliotheque contre un jumeau identique force sur std::mutex : le shared_mutex n'est JAMAIS plus rapide, meme a 100 % de lectures, et s'effondre des qu'il y a un ecrivain (jusqu'a 163x). Le point de bascule mesure : std::shared_mutex ne devient gagnant que lorsque la section critique de LECTURE depasse ~500 ns de travail utile — un regime ou l'utilisateur ne met justement pas un int.

Le choix est de plus non contournable : la couche is_unsafe_* ne permet pas de le forcer, et il n'y a aucun parametre de politique.

NOTE (faux positif ecarte) : j'ai teste le false sharing entre le mutex et value_. Il n'est PAS reproductible : value_ est a l'offset 200, donc sur la ligne de cache 1 (128-255) alors que les mots chauds du rwlock sont au debut, ligne 0. Comparaison layout {shared_mutex; long} contre {shared_mutex; alignas(128) long}, 4 executions a 8 threads : +11,7 %, -22,4 %, +15,6 %, +91,4 % — bruit pur, signe non stable. Il n'y a pas de finding false sharing ici.

**Code actuel**

```cpp
static constexpr bool shared_readable = bool(is_synchronizable_v<const T>);

using mutex =
    std::conditional_t<shared_readable, std::shared_mutex, std::mutex>;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Meme direction que l'auditeur — defaut `std::mutex`, partage en opt-in — mais deux corrections : l'assertion de politique passe dans le constructeur (CLAUDE.md interdit les invariants d'usage dans le corps de la classe, car interroger un trait complete le type), et `default_sharing_policy` est supprime (point de personnalisation superflu hors discipline `is_unsafe_*`).

`include/threadsafe/details/synchronized_value.h` :

```cpp
// ligne 16 : remplace `template <class T> class synchronized_value;`
enum class sharing_policy { exclusive, shared };

template <class T, sharing_policy Policy = sharing_policy::exclusive>
class synchronized_value;

// ligne 34, dans value_guard
  template <class, sharing_policy> friend class synchronized_value;

// lignes 43-45
template <class T, sharing_policy Policy> class synchronized_value {
public:
  static constexpr bool shared_readable =
      Policy == sharing_policy::shared && bool(is_synchronizable_v<const T>);

// lignes 47-53 inchangees

// dans le constructeur, avant le static_assert(sendable<T>) existant
    static_assert(Policy == sharing_policy::exclusive || shared_readable,
                  "sharing_policy::shared was asked for, but a const T is not "
                  "synchronizable — readers could not share it safely");

// lignes 88-94
template <class T, sharing_policy Policy>
struct is_unsafe_synchronizable<synchronized_value<T, Policy>>
    : std::bool_constant<is_sendable_v<T>> {};

template <class T, sharing_policy Policy>
struct is_unsafe_lifetime_aware<synchronized_value<T, Policy>>
    : std::bool_constant<is_lifetime_aware_v<T>> {};
```

`tests/test_synchronized_value.cpp` lignes 101-104, a mettre a jour (c'est le seul test casse) :

```cpp
static_assert(std::same_as<sync_int::const_guard,
                           threadsafe::value_guard<
                               const int, std::unique_lock<std::mutex>>>,
              "exclusion is the default: sharing readers is opt-in, because "
              "a shared_mutex only pays off for long reads");

using shared_int =
    threadsafe::synchronized_value<int, threadsafe::sharing_policy::shared>;
static_assert(std::same_as<shared_int::const_guard,
                           threadsafe::value_guard<
                               const int, std::shared_lock<std::shared_mutex>>>,
              "lock_shared — readers of a const-synchronizable T really share");
```

Verification complete effectuee (voir probe_output) : 11/12 tests inchanges passent, le 12e passe apres la mise a jour ci-dessus, `sizeof(synchronized_value<int>)` tombe de 208 a 72 octets et `sv<vector<int>>` de 224 a 88, l'opt-in `synchronized_value<int, sharing_policy::shared>` retrouve `shared_lock<shared_mutex>`, la politique `shared` sur un T dont le `const` n'est pas synchronizable est refusee a la construction, et les traits restent interrogeables sur toutes les instanciations.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

COMPILE ET EXECUTE.

(a) Tailles — /private/tmp/.../scratchpad/audit_perf_runtime/sizes.cpp + offsets.cpp, g++-16 -std=c++26 -freflection -O2 :
```
mutex=64 shared_mutex=200 unique_lock<shared_mutex>=16 shared_lock=16
synchronized_value<int>: sizeof=208 align=8 shared_readable=1
synchronized_value<vector<int>>: sizeof=224 shared_readable=1 (vector=24)
synchronized_value<Mutating>: sizeof=72 shared_readable=0
sizeof=208 offset(value_)=200  -> mutex occupies 98.1% of the object
cache line (128B) of mutex start = 0, of value_ = 1
```
(hw.cachelinesize = 128 sur M3 Pro)

(b) Benchmark end-to-end sur le vrai type — bench_sv_end2end.cpp, -O2, Apple M3 Pro 12 coeurs, ns par operation :
```
  1 thr, write_every=  0 : library(shared_mutex)=      6.5  std::mutex=     4.7  ->   1.36x plus lent
  1 thr, write_every=  1 : library(shared_mutex)=      6.3  std::mutex=     4.7  ->   1.34x
  2 thr, write_every=  0 : library(shared_mutex)=     10.8  std::mutex=     7.3  ->   1.48x
  2 thr, write_every=100 : library(shared_mutex)=     44.2  std::mutex=     8.2  ->   5.42x
  2 thr, write_every= 10 : library(shared_mutex)=    357.8  std::mutex=     8.2  ->  43.46x
  2 thr, write_every=  1 : library(shared_mutex)=   1303.4  std::mutex=     8.0  -> 163.43x
  4 thr, write_every=  0 : library(shared_mutex)=    110.9  std::mutex=    15.1  ->   7.35x
  4 thr, write_every=  1 : library(shared_mutex)=   1426.9  std::mutex=    13.6  -> 104.65x
  8 thr, write_every=  0 : library(shared_mutex)=    272.1  std::mutex=    17.9  ->  15.19x
  8 thr, write_every= 10 : library(shared_mutex)=   1276.1  std::mutex=    16.7  ->  76.34x
```
Aucune ligne ou le shared_mutex gagne.

(c) Ou le shared_mutex gagne-t-il vraiment ? bench_mutex2.cpp, 8 threads, 100 % lectures, longueur de section critique variable :
```
  read_work=     0  mutex=     17.9 ns  shared_mutex=    204.5 ns  gain shared =  0.09x
  read_work=   100  mutex=     89.9 ns  shared_mutex=    337.2 ns  gain shared =  0.27x
  read_work=  1000  mutex=    753.4 ns  shared_mutex=    260.3 ns  gain shared =  2.89x
  read_work= 10000  mutex=   4356.1 ns  shared_mutex=    419.5 ns  gain shared = 10.38x
```
Bascule entre read_work=100 (~90 ns) et read_work=1000 (~750 ns).

(d) Le patch compile : `g++-16 -std=c++26 -freflection -O2 -I fix fixtest.cpp` ->
```
sizeof default(exclusive)=72  sizeof opt-in shared=224
```
[…]

Contre-vérification indépendante :

=== (a) sizes.cpp — tailles : chiffres de l'auditeur reproduits a l'identique ===
mutex=64 shared_mutex=200
sv<int>: sizeof=208 align=8 shared_readable=1
sv<vector<int>>: sizeof=224 shared_readable=1 (vector=24)
sv<string>: sizeof=232 shared_readable=1
is_sync<const Mutating> = 1
(hw.cachelinesize = 128, hw.ncpu = 12, Apple arm64)

=== (b) bench.cpp — end-to-end, ns/op : aucune ligne ou le shared_mutex gagne ===
 1 thr, write_every=  0 : library=      6.6  std::mutex=     4.8  ->    1.37x
 1 thr, write_every=  1 : library=      6.5  std::mutex=     4.8  ->    1.36x
 2 thr, write_every=  0 : library=     11.3  std::mutex=     8.0  ->    1.41x
 2 thr, write_every=100 : library=     25.0  std::mutex=     8.0  ->    3.13x
 2 thr, write_every= 10 : library=    190.7  std::mutex=     7.3  ->   26.21x
 2 thr, write_every=  1 : library=   1216.4  std::mutex=     6.8  ->  177.57x
[…]

</details>

### PR-02 — asynchronous_task_launcher ne purge jamais threads_ : 16,1 Ko de RSS retenus par tache TERMINEE

**MAJEUR** · perf. exécution · `include/threadsafe/details/asynchronous_task_launcher.h:86`

`threads_` grandit a chaque `launch_task` et n'est jamais purge : le seul point de join est le destructeur du launcher. Un std::jthread termine mais non joint conserve toute la structure de thread de l'OS (pile incluse) jusqu'au join. Le cout n'est donc pas la croissance du vector, c'est la RETENTION memoire des threads deja finis.

Mesure : 16,1 Ko de RSS retenus par tache terminee, parfaitement lineaire, jamais rendus avant la destruction du launcher. 20 000 taches courtes -> 322 Mo de RSS pour des threads qui ne font plus rien depuis longtemps. Un launcher a duree de vie applicative (le cas d'usage naturel d'un objet qui s'appelle « launcher ») fuit indefiniment.

A noter que le point souleve par ailleurs — le vector qui grandit sans reserve — est NEGLIGEABLE : voir le finding separe. Le probleme n'est pas le realloc, c'est le join absent.

> Défaut trouvé indépendamment par 2 auditeurs : *Aucun moyen d'attendre, de recycler ou d'arrêter les tâches : 4000 threads OS simultanés, vector jamais vidé*.

**Code actuel**

```cpp
template <typename F, typename... Args>
    requires launchable_task<F, Args...>
void launch_task(F f, Args... args) {
    threads_.emplace_back(std::move(f), std::move(args)...);
}

// ...

std::vector<std::jthread> threads_;
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La variante (2) du finding NE DOIT PAS etre appliquee telle quelle : elle casse l'injection du stop_token (verifie par compilation). Voici la variante que j'ai compilee et executee, qui preserve l'injection.

Dans include/threadsafe/details/asynchronous_task_launcher.h, ajouter `#include <atomic>` et `#include <memory>`, puis :

```cpp
    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    void launch_task(F f, Args... args) {
        std::erase_if(running_tasks_, [](running_task const &task) {
            return task.finished->test(std::memory_order_acquire);
        });
        auto finished = std::make_shared<std::atomic_flag>();
        running_tasks_.push_back({finished, {}});
        running_tasks_.back().thread = std::jthread(
            [finished, f = std::move(f)]<class... Injected>(
                Injected &&...injected) mutable
                requires std::invocable<F &, Injected...>
            {
                f(std::forward<Injected>(injected)...);
                finished->test_and_set(std::memory_order_release);
            },
            std::move(args)...);
    }

  private:
    struct running_task {
        std::shared_ptr<std::atomic_flag> finished;
        std::jthread thread;
    };
    std::vector<running_task> running_tasks_;
```

Le point cle est la clause `requires std::invocable<F &, Injected...>` sur le lambda enveloppant. Sans elle, deux echecs symetriques :
- si l'enveloppe fixe `Args...` (variante (2) du finding), `std::jthread` ne voit plus de surcharge acceptant un stop_token et cesse de l'injecter : tout callable prenant un stop_token ne compile plus ;
- si l'enveloppe est un `auto&&...` non contraint, `std::is_invocable_v` reussit pour TOUTE signature, donc `std::jthread` injecte un stop_token meme a un callable qui n'en veut pas, et l'erreur se deplace sans disparaitre.
La clause `requires` fait refleter par l'enveloppe l'invocabilite exacte de `f`, ce qui restaure precisement la logique de detection de `std::jthread` et respecte le static_assert de la ligne 49.

Reserve de cout que je signale explicitement : `std::erase_if` rescanne tout le vecteur a chaque lancement, donc O(n^2) cumule si beaucoup de taches longues coexistent (n = taches en cours, pas taches lancees). Sans consequence pour des taches courtes.

Si cette complexite est jugee excessive pour une bibliotheque pedagogique, retenir la variante (1) du finding — `void join_all() { threads_.clear(); }` — qui compile trivialement et ne casse aucun test, en documentant que le launcher retient environ 16 Ko de RSS par tache terminee jusqu'au join.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

COMPILE ET EXECUTE. bench_retain.cpp, -O2, RSS lu via task_info(MACH_TASK_BASIC_INFO), taches vides `[]{}` donc toutes terminees depuis longtemps :
```
  4000 tasks launched (all long finished): rss = +64320 KB, 16.1 KB/task
  8000 tasks launched (all long finished): rss = +128624 KB, 16.1 KB/task
 12000 tasks launched (all long finished): rss = +192912 KB, 16.1 KB/task
 16000 tasks launched (all long finished): rss = +257120 KB, 16.1 KB/task
 20000 tasks launched (all long finished): rss = +321568 KB, 16.1 KB/task
```
bench_threads_vec.cpp confirme que tout est rendu d'un coup a la destruction :
```
rss before = 2192 KB
4000 launch_task : 20.338 us/call ; rss after launching = 66480 KB
rss after launcher destruction = 2480 KB
```

Le correctif (2), compile et execute — prune_fix.cpp :
```
  4000 tasks : rss = +64 KB
  8000 tasks : rss = +80 KB
 12000 tasks : rss = +80 KB
 16000 tasks : rss = +80 KB
 20000 tasks : rss = +128 KB
```
321 568 Ko -> 128 Ko, soit un facteur 2500, et la consommation devient bornee au lieu d'etre lineaire.

Contre-vérification indépendante :

=== Sonde 1 — bench_retain.cpp, HEAD non modifie (reproduction) ===
rss before = 2192 KB
```text
4000 tasks launched: rss = +64288 KB, 16.1 KB/task
8000 tasks launched: rss = +128576 KB, 16.1 KB/task
```
 12000 tasks launched: rss = +192848 KB, 16.1 KB/task
 16000 tasks launched: rss = +257040 KB, 16.1 KB/task
 20000 tasks launched: rss = +321552 KB, 16.1 KB/task
(annonce : +321568 KB / 16.1 KB par tache — ecart de 16 Ko sur 322 Mo)

=== Sonde 2 — bench_control.cpp, LE test decisif ===
A: vector of 20000 EMPTY jthreads: +336 KB
B: 20000 tasks, joined each time: +64 KB
C: 20000 unjoined, before destruction: +321184 KB
C: after launcher destruction:        +1152 KB
--> le vector seul pese 336 Ko (0,017 Ko/tache), les memes threads JOINTS pesent
```text
64 Ko : 99,9 % du cout est bien la retention des threads non joints, et tout
```
[…]

</details>

### PR-03 — launch_task prend F et Args par valeur : cout mesure NUL, le perfect forwarding n'apporterait rien

**DÉTAIL** · perf. exécution · `include/threadsafe/details/asynchronous_task_launcher.h:56`

Le soupcon naturel est que `void launch_task(F f, Args... args)` ajoute une copie par appel quand l'appelant passe une lvalue. J'ai mesure : c'est FAUX pour le nombre de copies.

std::jthread decay-copie deja ses arguments dans son propre stockage. Passer une lvalue a un jthread brut coute exactement le meme nombre d'allocations (3) que le chemin par valeur de la bibliotheque (3). Le perfect forwarding economiserait donc zero copie : il economiserait un MOVE, qui est gratuit pour tout type correctement movable.

Le cout reel existe bien, mais il est intrinseque au franchissement de frontiere de thread, pas a la signature : passer un std::vector<long long> de 8 Mio par lvalue coute une allocation de 8 Mio et ~150 us de plus que `std::move`. C'est vrai avec ou sans forwarding.

De plus, le passage par valeur fait ici un second travail, de SURETE : il force `Args` a se deduire vers le type valeur decay-e, exactement celui que jthread stockera. Avec `Args&&`, une lvalue ferait deduire `Args = T&`, et les traits seraient alors interroges sur `is_sendable_v<T&>` — qui vaut `is_synchronizable_v<T>`, une question DIFFERENTE de celle qui correspond a ce que jthread copie reellement. Le passage par valeur aligne la question posee aux traits sur l'objet reellement transfere.

VERDICT : ne pas toucher. Le passage par valeur est gratuit en copies et correct en surete.

**Code actuel**

```cpp
template <typename F, typename... Args>
    requires launchable_task<F, Args...>
void launch_task(F f, Args... args) {
    threads_.emplace_back(std::move(f), std::move(args)...);
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Aucun changement de code. Le finding a raison sur le fond, mais sa justification peut etre remplacee par une bien plus forte, que j'ai prouvee a l'execution.

Raison reelle de ne pas toucher a la ligne 56 : les DEUX surcharges de `launch_task` (la contrainte ligne 56 et celle de diagnostic ligne 61) doivent passer leurs parametres DE LA MEME FACON. Elles sont aujourd'hui toutes deux par valeur, donc elles deduisent `Args` identiquement : quand la contrainte echoue, la surcharge de diagnostic pose exactement la meme question aux traits et son `static_assert` se declenche.

Si l'on convertit la seule surcharge contrainte en `Args&&...`, une lvalue y deduit `Args = T&` (donc `is_lifetime_aware_v<T&> == false` -> contrainte ecartee) tandis que la surcharge de diagnostic, restee par valeur, rededuit `Args = T` (donc ses `static_assert` PASSENT) et execute son corps VIDE. Mesure : la tache n'est jamais lancee, sans le moindre diagnostic de compilation.

Deux ameliorations concretes, toutes deux reellement compilees :

(1) Ancrer l'invariant dans le code, pour qu'un futur refactor ne puisse pas le briser en silence. A ajouter dans `asynchronous_task_launcher` :

```cpp
static_assert(
    std::is_same_v<
        decltype(&asynchronous_task_launcher::launch_task<void (*)(int), int>),
        void (asynchronous_task_launcher::*)(void (*)(int), int)>,
    "les deux surcharges de launch_task doivent prendre leurs parametres "
    "par valeur : c'est ce qui garantit que la surcharge de diagnostic "
    "deduit Args comme la surcharge contrainte. Avec Args&&, une lvalue "
    "deduirait Args = T&, la contrainte serait ecartee, et la surcharge "
    "de diagnostic accepterait silencieusement l'appel sans rien lancer.");
```
(2) Combler le trou de couverture que cette verification a revele : la suite de tests complete passe contre une version qui n'execute AUCUNE tache, parce que les tests n'exercent que les concepts (`launchable_task<...>`) et jamais un appel reel. Comme les tests du projet sont compile-time uniquement, le minimum verifiable a la compilation est d'instancier reellement les appels, ce qui force au moins la selection de surcharge a etre exercee :

```cpp
// tests/test_asynchronous_task_launcher.cpp
inline void instantiate_real_calls() {
    threadsafe::asynchronous_task_launcher launcher;
    auto body = [](std::vector<int>) {};
    std::vector<int> payload{1, 2, 3};
    launcher.launch_task(body, payload);            // lvalue
    launcher.launch_task(body, std::move(payload)); // rvalue
}

Cela ne suffit pas a detecter l'abandon silencieux (il compile). Le seul
controle qui l'attrape est le static_assert (1) sur la signature : c'est
lui qui est la vraie protection, et c'est pourquoi je le recommande.
```
Enfin, corriger une formulation trop large du finding : « un MOVE, gratuit pour tout type correctement movable » est faux pour les agregats trivialement copiables volumineux — mesure a ~120 us supplementaires sur un `std::array<long long, 128K>`. L'enonce exact est : le passage par valeur coute zero ALLOCATION de plus qu'un forwarding parfait, et un move de plus, gratuit pour tout type dont le move ne copie pas (donc pour tous les conteneurs, mais pas pour un gros agregat).

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

COMPILE ET EXECUTE, comptage d'allocations par surcharge de operator new global.

(a) bench_launcher_copies.cpp, argument std::vector<long long> de 8 Mio, via la bibliotheque :
```
lvalue arg : allocs=3 bytes=8388664  time=532.0 us
rvalue arg : allocs=3 bytes=8388664  time=381.0 us
std::move  : allocs=2 bytes=56
```
(b) fwd_check.cpp, meme charge mais avec un std::jthread BRUT, ce qui simule exactement ce que donnerait le perfect forwarding :
```
jthread + lvalue (perfect fwd equiv) : allocs=3
jthread + rvalue                     : allocs=2
```
Identique : 3 allocations dans les deux cas. Le passage par valeur n'ajoute aucune allocation par rapport a un forwarding parfait.

</details>

### PR-04 — Verification demandee : les traits ne laissent RIEN dans l'assembleur, et value_guard est entierement elide

**DÉTAIL** · perf. exécution · `include/threadsafe/details/synchronized_value.h:75`

Resultat NEGATIF mais demande explicitement, donc rapporte : les deux points suivants sont verifies et ne constituent pas des defauts.

1) Les traits sont a cout runtime strictement nul. Une fonction qui interroge is_sendable_v, is_synchronizable_v<const T> et is_lifetime_aware_v se compile en deux instructions. Aucune trace des variables template, aucun initialiseur statique, aucune section de donnees.

2) value_guard fait 24 octets (16 de Lock + 8 de T*), mais il n'est JAMAIS materialise. `lock()` renvoie un prvalue `guard{mutex_, value_}` qui initialise directement l'objet de retour : l'elision garantie de C++17 s'applique, ce qui est d'ailleurs une condition de compilation puisque value_guard est non copiable et non movable (le fait que ca compile est deja la preuve). A -O2 le guard disparait entierement : il ne reste qu'un appel de lock, l'acces, et un appel d'unlock.

Le seul commentaire exploitable : c'est `_pthread_rwlock_wrlock` que l'on voit dans cet assembleur, pas `_pthread_mutex_lock` — conséquence du finding sur le choix par defaut du shared_mutex.

**Code actuel**

```cpp
[[nodiscard]] guard lock() & { return guard{mutex_, value_}; }
[[nodiscard]] const_guard lock_shared() const & {
  return const_guard{mutex_, value_};
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Aucune correction de code : il n'y a pas de defaut. Seule la redaction du finding merite d'etre resserree, car son argument technique est bancal meme si sa conclusion est juste. Remplacer « value_guard fait 24 octets [...] mais il n'est JAMAIS materialise [...] l'elision garantie de C++17 s'applique, ce qui est d'ailleurs une condition de compilation » par la formulation qui separe les deux mecanismes :

« value_guard fait 24 octets (16 de unique_lock<shared_mutex> + 8 de T*). Deux proprietes distinctes le rendent gratuit en pratique :
 - il n'est jamais COPIE ni DEPLACE : `lock()` renvoie le prvalue `guard{mutex_, value_}` qui initialise directement l'objet de retour, puis celui-ci initialise directement la variable du site d'appel. C'est l'elision obligatoire de C++17, garantie par la norme, et c'est une condition de compilation puisque value_guard est non copiable et non deplacable — le fait que ca compile en est la preuve.
 - ses 24 octets disparaissent a -O2 : la, c'est de la scalarisation ordinaire, une optimisation et non une garantie. A -O0 le guard occupe reellement la pile. »

Et remplacer « corps complet de use(...) » par « corps chaud de use(...), prologue/epilogue et branche d'exception L5 omis », puisque le bloc cite a ete filtre.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

COMPILE ET EXECUTE.

(a) trait_zero_cost.cpp, g++-16 -std=c++26 -freflection -O2 -S, corps complet de la fonction apres filtrage des directives :
```
__Z5probei:
	add	w0, w0, 1
	ret
```
La branche `if constexpr` sur les trois traits a totalement disparu.

(b) guard_asm.cpp, meme options, corps complet de `use(synchronized_value<int>&)` :
```
	mov	x19, x0
	bl	_pthread_rwlock_wrlock
	cmp	w0, 11
	beq	L5
	ldr	w0, [x19, 200]
	add	w0, w0, 1
	str	w0, [x19, 200]
	mov	x0, x19
	bl	_pthread_rwlock_unlock
	ret
```
Aucun objet guard construit, aucun stockage du T*, l'offset 200 est adresse directement. sizeof(guard) = sizeof(const_guard) = 24 (mesure dans sizes.cpp) mais jamais paye en pratique.

</details>

## Couverture de tests

### TEST-01 — Ordre d'inclusion : GCC protege a l'interieur d'une TU, mais deux TU divergentes produisent une violation d'ODR silencieuse

**CRITIQUE** · flexibilité · `tests/test_deferred_specialization.cpp:35`

Bonne nouvelle d'abord : le piege annonce par CLAUDE.md ('la specialisation doit etre ecrite avant la premiere question sur ce T') ne passe PAS silencieusement dans une meme TU. GCC diagnostique en dur 'specialization of threadsafe::is_unsafe_sendable<Late> after instantiation'. L'utilisateur est donc protege intra-TU, et aucun compteur consteval ni static_assert de coherence n'est necessaire - il faut juste le DIRE, car CLAUDE.md laisse croire a un changement silencieux.

La vraie faille est inter-TU, et elle est totalement muette : si la TU A pose is_sendable_v<Late> sans jamais inclure l'en-tete qui vouche pour Late, et la TU B le fait apres l'avoir inclus, les deux TU definissent le MEME symbole externe threadsafe::is_sendable_v<Late> avec deux initialiseurs differents. C'est une violation d'ODR (IFNDR) : aucun diagnostic a la compilation, aucun a l'edition de liens, et c'est l'ORDRE DES .o SUR LA LIGNE DE LIEN qui decide de la valeur survivante. Chaque TU a pourtant fait confiance a sa propre reponse constant-foldee au moment de ses static_assert.

Quant au test : test_deferred_specialization.cpp ne protege pas l'utilisateur, il documente uniquement le chemin heureux (la specialisation est ecrite avant toute question sur Opaque/Holder). Il ne pose jamais la question avant le vouch, donc il n'exerce ni le cas diagnostique intra-TU, ni la divergence inter-TU.

**Code actuel**

```cpp
template <>
struct threadsafe::is_unsafe_synchronizable<Opaque> : std::true_type {};

static_assert(!is_sendable_v<int*>);
static_assert(is_sendable_v<Holder>,
              "a specialization declared in the user's TU must reach the "
              "recursion over members");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le finding a raison sur le diagnostic mais se trompe sur deux points : il sous-estime la consequence (ce n'est pas « quelle valeur survit au lien », c'est une divergence de DISPOSITION MEMOIRE de synchronized_value<T> qui corrompt le mutex), et il declare a tort qu'aucun verrou technique n'est possible.

ACTION 1 — Corriger CLAUDE.md (indispensable, gratuit, ne casse rien).
Remplacer :
```cpp
« Because the claim is read by instantiating `is_unsafe_<trait><T>`, the
  specialization must be written before the first question about that `T`. »
```
par :
```cpp
« Because the claim is read by instantiating `is_unsafe_<trait><T>`, the
  specialization must be written before the first question about that `T`,
  **in every translation unit that asks**. Inside one TU, getting the order
  wrong is a hard error (GCC: "specialization of
  'threadsafe::is_unsafe_sendable<T>' after instantiation") — you are
  protected. Across TUs, nothing diagnoses it: two TUs that disagree define
  the same `is_sendable_v<T>` with different initialisers, which is an ODR
  violation (IFNDR). Because `synchronized_value<T>` derives its `mutex`
  type from `is_synchronizable_v<const T>`, the disagreement changes
  `sizeof(synchronized_value<T>)` and corrupts the lock at run time.
  Therefore: put every `is_unsafe_*` vouch in ONE header (e.g.
  `my_project/threadsafe_vouches.h`) included by every TU that asks a trait —
  never in a `.cpp`. A vouch for a type declared in an anonymous namespace is
  TU-local and immune by construction; that is why the tests use one. »
```
ACTION 2 — Documenter la protection intra-TU dans le test (volet 2 du finding, correct).
Dans tests/test_deferred_specialization.cpp, apres le bloc ligne 34-40, ajouter :
```cpp
// Getting the order wrong is NOT silent inside a TU. Asking before vouching:
//     static_assert(!is_sendable_v<Opaque>);            // question first
//     template <> struct threadsafe::is_unsafe_synchronizable<Opaque> ...
// GCC refuses:
//     error: specialization of 'threadsafe::is_unsafe_synchronizable<Opaque>'
//            after instantiation
// Across translation units there is no such protection — see CLAUDE.md.
```
(Commentaire pur : n'affecte aucune compilation, baseline 12/12 conservee.)

ACTION 3 — Optionnelle, a trancher par l'auteur : supprimer la charge utile dangereuse.
La corruption memoire vient uniquement du fait que la reponse du trait pilote la
disposition (synchronized_value.h:45-53). En figeant le type stocke :
```cpp
using mutex = std::shared_mutex;
using guard = value_guard<T, std::unique_lock<mutex>>;
```
(en gardant const_guard conditionnel : shared_lock si shared_readable, sinon
unique_lock), `sizeof(synchronized_value<T>)` devient independant des vouches.
Verifie : la sonde x_* n'abort plus (« -> 42 », EXIT=0). Le pire residuel devient
un simple desaccord d'exclusion entre TU, sans UB ni corruption.
COUT HONNETE : cela casse test_synchronized_value.cpp:110-118, qui asserte
explicitement `std::same_as<sync_memo::mutex, std::mutex>` (« no shared_mutex:
there is no read that may be shared »). Le choix du mutex est une optimisation
deliberee, testee et pedagogiquement interessante. Je ne recommande donc pas
d'appliquer l'action 3 telle quelle : c'est un arbitrage conception
(robustesse ODR contre demonstration de l'optimisation) que l'auteur doit
trancher. Si elle est retenue, il faut reecrire ces trois static_assert.

Les actions 1 et 2 seules suffisent a lever le caractere muet du piege et sont
sans risque.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

SONDE INTRA-TU p6_order.cpp (en-tetes actuels) : question posee avant le vouch, puis vouch. GCC refuse :
```text
"p6_order.cpp:7:32: error: specialization of 'threadsafe::is_unsafe_sendable<Late>' after instantiation"
```
Idem p7_order_member.cpp quand seule la question sur le CONTENEUR (Holder) precede le vouch sur le membre (Late).

SONDE INTER-TU (compilee ET EXECUTEE) :
```text
odr_common.h : struct Late { int* borrowed; };
odr_a.cpp    : static_assert(!threadsafe::is_sendable_v<Late>);  const bool* answer_seen_by_a(){ return &threadsafe::is_sendable_v<Late>; }
odr_b.cpp    : template <> struct threadsafe::is_unsafe_sendable<Late> : std::true_type {};
               static_assert(threadsafe::is_sendable_v<Late>);   const bool* answer_seen_by_b(){ return &threadsafe::is_sendable_v<Late>; }
```
Les DEUX static_assert passent. Puis :
```text
g++-16 ... odr_a.cpp odr_b.cpp odr_main.cpp  ->  "A sees 0 at 0x1025bc698 / B sees 0 at 0x1025bc698"
g++-16 ... odr_b.cpp odr_a.cpp odr_main.cpp  ->  "A sees 1 at 0x10214c698 / B sees 1 at 0x10214c698"
```
Meme adresse, valeur decidee par l'ordre de lien, zero avertissement du compilateur comme de l'editeur de liens.

Contre-vérification indépendante :

=== p6_order.cpp (intra-TU) : GCC diagnostique, l'utilisateur est protege ===
p6_order.cpp:7:32: error: specialization of 'threadsafe::is_unsafe_sendable<Late>' after instantiation
```text
7 | template <> struct threadsafe::is_unsafe_sendable<Late> : std::true_type {};
  |                                ^~~~~~~~~~~~~~~~~~~~~~~~
```
p6_order.cpp:9:27: error: static assertion failed
```text
9 | static_assert(threadsafe::is_sendable_v<Late>);
```
=== odr_a/odr_b (inter-TU, variable) : zero avertissement, valeur decidee par l'ordre de lien ===
--- order A B ---
A sees 0 at 0x100fb8698 / B sees 0 at 0x100fb8698
--- order B A ---
A sees 1 at 0x100438698 / B sees 1 at 0x100438698

=== sv_a/sv_b (inter-TU, LAYOUT) : compilation des deux TU SANS AUCUN avertissement ===
=== compile a ===
=== compile b ===
=== link+run ===
A: sizeof=72 shared=0 | B: sizeof=208 shared=1
[…]

</details>

### TEST-02 — asynchronous_task_launcher ACCEPTE un type unsafe : l'exigence Task.md n'est ni respectee ni testable

**MAJEUR** · tests · `include/threadsafe/details/asynchronous_task_launcher.h:60`

Task.md exige "asynchronous_task<T> ne doit pas accepter de type unsafe". La surcharge de diagnostic non contrainte rend l'expression `l.launch_task(f, args...)` PARFAITEMENT BIEN FORMEE pour un F et des Args unsafe : les static_assert ne se declenchent qu'a l'instanciation du corps, pas a la formation de l'expression. Consequence concrete : (1) aucun static_assert ne peut verifier l'exigence — le pattern `static_assert(!requires(launcher l, UnsafeF f){ l.launch_task(f); })` ECHOUE contre les headers actuels ; (2) tout code generique qui teste `requires { l.launch_task(f); }` ou `std::is_invocable_v` obtient une reponse FAUSSE et croit que le lancement est sur. Il n'existe aucun test dans tests/ qui verifie la non-acceptation ; les 14 assertions de test_asynchronous_task_launcher.cpp testent uniquement le concept `launchable_task`, jamais l'appel. Meme probleme exact pour launch_scoped_task (ligne 77).

> Défaut trouvé indépendamment par 3 auditeurs : *La surcharge de diagnostic non contrainte fait passer un appel non sûr pour une expression VALIDE (SFINAE)*, *La surcharge de diagnostic du launcher rend launch_task toujours detectable: requires{} donne un faux positif*.

**Code actuel**

```cpp
template <typename F, typename... Args>
void launch_task(F, Args...) {
    static_assert(task_participant<F>,
                  "the callable must be movable, sendable and "
                  "lifetime-aware");
    static_assert((task_participant<Args> && ...),
                  "every argument must be movable, sendable and "
                  "lifetime-aware");
}
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Correctif d'origine retenu tel quel, verifie compile + non-regression.

1) include/threadsafe/details/asynchronous_task_launcher.h — remplacer les deux surcharges de diagnostic non contraintes (lignes 60-68 et 77-83) par des declarations supprimees contraintes (C++26 `= delete("raison")`). L'expression devient mal formee, donc DETECTABLE et TESTABLE, et le message reste aussi lisible qu'avant :

    template <typename F, typename... Args>
        requires (!launchable_task<F, Args...>)
    void launch_task(F, Args...) = delete(
        "launch_task: the callable and every argument must be movable, "
        "sendable and lifetime-aware");

    template <typename F, typename... Args>
        requires (!launchable_scoped_task<F, Args...>)
    void launch_scoped_task(F, Args...) = delete(
        "launch_scoped_task: the callable and every argument must be movable "
        "and sendable");

Les deux surcharges contraintes (lignes 54-58 et 70-75) ne changent pas. Le `requires (!...)` rend les deux surcharges mutuellement exclusives, ce qui supprime au passage la dependance a l'ordonnancement partiel par contraintes.

2) tests/test_asynchronous_task_launcher.cpp — ajouter le test qui manquait, qui verifie enfin l'exigence Task.md a l'APPEL et non seulement sur le concept.

ATTENTION, verifie experimentalement sous GCC 16 : le `requires` doit etre dans un contexte DEPENDANT (variable template), sinon GCC emet une erreur dure « use of deleted function » au lieu de repondre false. Le bloc ci-dessous compile sans erreur avec le correctif, et produit 4 `static assertion failed` sans lui :

    namespace {
    struct NonSendable {
        NonSendable(NonSendable const &) {}
        void operator()(int *) const {}
    };

    template <class F, class... Args>
    constexpr bool accepts_launch_task
        = requires(threadsafe::asynchronous_task_launcher launcher, F callable,
                   Args... arguments) {
              launcher.launch_task(callable, arguments...);
          };

    template <class F, class... Args>
    constexpr bool accepts_launch_scoped_task
        = requires(threadsafe::asynchronous_task_launcher launcher, F callable,
                   Args... arguments) {
              launcher.launch_scoped_task(callable, arguments...);
          };
    }

    static_assert(accepts_launch_task<decltype([] {})>,
                  "launch_task — a safe call is accepted");
    static_assert(accepts_launch_task<decltype([](int, std::string) {}), int,
                                      std::string>,
                  "launch_task — safe args are accepted");
    static_assert(!accepts_launch_task<NonSendable, int *>,
                  "launch_task — an unsafe callable and an unsafe arg are refused");
    static_assert(!accepts_launch_task<decltype([](int *) {}), int *>,
                  "launch_task — a raw pointer arg is refused");
    static_assert(accepts_launch_scoped_task<decltype([] {})>,
                  "launch_scoped_task — a safe call is accepted");
    static_assert(!accepts_launch_scoped_task<NonSendable, int *>,
                  "launch_scoped_task — an unsafe call is refused");
    static_assert(!accepts_launch_task<std::function<void()>>,
                  "launch_task — std::function owns unsynchronized state");

Non-regression confirmee : les 12 fichiers tests/test_*.cpp compilent avec le correctif, et les appels surs restent acceptes.
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p21_tmpl.cpp` contre les headers actuels :
```text
p21_tmpl.cpp:19:15: error: static assertion failed: PROBE-T2: the unsafe call must be rejected
```
(l'expression unsafe est donc valide). Avec le correctif (copie fix3/ puis fix5/) la meme sonde compile SANS erreur, et l'appel reel donne :
```text
error: use of deleted function 'void threadsafe::asynchronous_task_launcher::launch_task(F, Args ...) [with F = UnsafeCallable; Args = {int*}]': launch_task: the callable and every argument must be movable, sendable and lifetime-aware
```
Non-regression verifiee : les 12 fichiers de tests/*.cpp compilent tous avec le correctif.

Contre-vérification indépendante :

[1] probe.cpp contre les HEADERS D'ORIGINE :
probe.cpp:26:15: error: static assertion failed: PROBE-T2: the unsafe call must be rejected
probe.cpp:29:15: error: static assertion failed: PROBE-T3: the unsafe scoped call must be rejected
(PROBE-T0 et PROBE-T1 passent : le concept dit non, mais l'expression d'appel est bien formee.)

[3] real_call.cpp contre les HEADERS D'ORIGINE (appel reel) :
./include/threadsafe/details/asynchronous_task_launcher.h:62:23: error: static assertion failed: the callable must be movable, sendable and lifetime-aware
=> le garde-fou tient a l'appel reel ; ce n'est donc pas un trou de soundness.

[2] probe.cpp contre les HEADERS CORRIGES :
(aucune sortie — compile proprement)

[3bis] real_call.cpp contre les HEADERS CORRIGES :
[…]

</details>

### TEST-03 — Zero test d'execution : le detach de copy_on_write, le verrouillage des guards et le lancement des threads ne sont jamais executes

**MAJEUR** · tests · `tests/CMakeLists.txt:1`

Task.md exige "Test individuellement les helpers : copy_on_write<T>, synchronized_value<T>, asynchronous_task<T>". La cible est une OBJECT library sans executable ni CTest, donc la suite ne teste que des types. Le comportement RUNTIME des trois helpers n'est verifie par rien : la branche `ptr_.use_count() != 1` de copy_on_write::as_mutable (copy_on_write.h:33) — c'est-a-dire LA semantique du type — n'est jamais executee, pas plus que le fence de la branche unique, l'exclusion effective de value_guard, ou la creation/jointure des jthread du launcher. Un bug qui ferait detacher systematiquement (ou jamais) passerait toute la suite. C'est la lacune de couverture la plus grande par rapport a Task.md.

**Code actuel**

```cpp
add_library(threadsafe_tests OBJECT
    test_synchronizable.cpp
    ...
    test_diagnostics.cpp
)
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

```cpp
Ajouter un executable a cote de l'OBJECT library. Version verifiee : build CMake complet + ctest 1/1 Passed, et tuee par les deux mutations de `as_mutable()`.

CMakeLists.txt racine — `enable_testing()` doit venir AVANT `add_subdirectory(tests)`, sinon `add_test` du sous-repertoire n'est pas enregistre :

    if(THREADSAFE_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()

tests/CMakeLists.txt — `Threads::Threads` est indispensable (sans lui le link casse sous GCC/Linux, ou std::jthread exige -pthread ; l'oubli passe inapercu sur macOS) :

    find_package(Threads REQUIRED)
    add_executable(threadsafe_runtime_tests test_runtime_helpers.cpp)
    target_link_libraries(threadsafe_runtime_tests
        PRIVATE ThreadSafe::threadsafe Threads::Threads)
    add_test(NAME threadsafe_runtime COMMAND threadsafe_runtime_tests)

tests/test_runtime_helpers.cpp — le compteur passe a 100 000 iterations par thread : a 1 000, mesure faite, un mutex totalement absent donne quand meme 8000 a chaque execution, l'assertion ne prouve rien. A 100 000 elle detecte la perte de mises a jour (cout mesure 1,5 s).

    #include <threadsafe/threadsafe.h>

    #include <cassert>
    #include <memory>
    #include <string>

    namespace {

    constexpr int thread_count = 8;
    constexpr int increments_per_thread = 100'000;

    void check_copy_on_write_detaches_only_when_shared() {
      threadsafe::copy_on_write<std::string> original{"hello"};
      auto shared_copy = original;
      assert(&*original == &*shared_copy);

      original.as_mutable() += " world";
      assert(*shared_copy == "hello");
      assert(*original == "hello world");
      assert(&*original != &*shared_copy);

      const std::string *sole_owner_address = &*original;
      original.as_mutable() += "!";
      assert(&*original == sole_owner_address);
    }

    void check_synchronized_value_serializes_writes() {
      auto counter = threadsafe::synchronized_value<int>::make(0);
      {
        threadsafe::asynchronous_task_launcher launcher;
        for (int thread_index = 0; thread_index < thread_count; ++thread_index)
          launcher.launch_task(
              [](std::shared_ptr<threadsafe::synchronized_value<int>> shared) {
                for (int step = 0; step < increments_per_thread; ++step) {
                  auto guard = shared->lock();
                  ++*guard;
                }
              },
              counter);
      }
      auto guard = counter->lock_shared();
      assert(*guard == thread_count * increments_per_thread);
    }

    } // namespace

    int main() {
      check_copy_on_write_detaches_only_when_shared();
      check_synchronized_value_serializes_writes();
      return 0;
    }

Deux remarques pour l'auteur. Le fichier doit etre compile sans NDEBUG (donc pas en Release seul) sinon les `assert` disparaissent et le test devient un no-op qui passe toujours. Et la detection d'une vraie data race reste le role de `-fsanitize=thread` sur cet executable, pas des assertions : le compteur ne couvre que la perte de mises a jour.
```

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Fichier ecrit en .../scratchpad/audit_tests/missing_test_runtime.cpp, compile et EXECUTE contre les headers actuels :
```text
g++-16 -std=c++26 -freflection -I include missing_test_runtime.cpp -o rt2 && ./rt2  =>  RT_OK
```
Tous les asserts passent : le comportement est correct, mais rien dans le depot ne le verifie. Note pedagogique au passage : la premiere version de cette sonde utilisait `++*shared->lock()` et a ete correctement rejetee par l'operator*() && supprime ("a temporary guard is destroyed at the semicolon") — ce garde non plus n'est teste nulle part (voir finding separe).

Contre-vérification indépendante :

(a) ./rt -> exit 0, RT_OK. Preuve d'origine reproduite telle quelle.

(b) mutation `if (true)` (detache toujours) :
```text
  suite compile-time : aucune ligne "CASSE" sur les 12 fichiers -> LA SUITE PASSE
  ./rt_mut1 -> Assertion failed: (&*original == before), line 17 — exit=134
mutation `if (false)` (ne detache jamais) :
  suite compile-time : aucune ligne "CASSE" sur les 12 fichiers -> LA SUITE PASSE
  ./rt_mut2 -> Assertion failed: (*shared_copy == "hello"), line 12 — exit=134
Header restaure, suite de reference : SUITE_DONE, 0 erreur.
```
(c) 8 threads x 1 000 increments SANS verrou :
```text
  counter=8000 / 8000 / 8000 / 8000 / 8000 (attendu 8000) -> l'assertion du finding ne detecterait PAS un mutex absent.
8 threads x 200 000 increments SANS verrou :
  counter=219279 / 218941 / 224919 (attendu 1600000) -> a ce volume l'assertion mord.
```
[…]

</details>

### TEST-04 — Les surcharges `&&` supprimées — le mécanisme central du guard — ne sont couvertes par aucun static_assert

**MINEUR** · tests · `tests/test_synchronized_value.cpp:96`

Le fichier teste beaucoup : les traits, les types de mutex/guard, `can_lock<const sync_int&>`, la non-copiabilité. Mais il ne teste JAMAIS le seul mécanisme qui empêche `*sv.lock()` — les quatre `= delete` sur `operator*() &&`, `operator->() &&`, `lock() &&`, `lock_shared() &&`. Si quelqu'un supprime ces quatre lignes, la suite de tests reste verte. Et la voie naturelle (`static_assert(!requires(sync_int &s) { *s.lock(); })`) ne marche pas : sur GCC 16, utiliser une fonction supprimée dans une requires-expression est une ERREUR DURE, pas une contrainte non satisfaite — reproduit hors bibliothèque, donc c'est un comportement du compilateur, pas un défaut d'ici. Il faut donc une autre technique, et la bibliothèque en a déjà une sous la main : la réflexion.

> Défaut trouvé indépendamment par 2 auditeurs : *Les accesseurs rvalue supprimes — le garde le plus pedagogique des helpers — ne sont testes nulle part*.

**Code actuel**

```cpp
static_assert(can_lock<sync_int&> && can_lock_shared<sync_int&>);
static_assert(!can_lock<const sync_int&>,
              "lock — a const synchronized_value grants readers only");
static_assert(can_lock_shared<const sync_int&>);
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

La correction proposee rate le groupe `lock()` / `lock_shared()`. Version corrigee, verifiee : passe contre le depot intact, echoue des qu'on retire N'IMPORTE lequel des deux groupes.

Dans tests/test_synchronized_value.cpp, ajouter `#include <meta>` puis, dans le namespace anonyme existant (apres `can_lock_shared`) :

```cpp
consteval bool every_rvalue_overload_is_deleted(std::meta::info type) {
    bool found = false;
    for (auto member :
         members_of(type, std::meta::access_context::unchecked())) {
        if (!is_function(member) || !is_rvalue_reference_qualified(member))
            continue;
        found = true;
        if (!is_deleted(member))
            return false;
    }
    return found;
}
```

et en fin de fichier :

```cpp
static_assert(every_rvalue_overload_is_deleted(^^sync_int::guard),
              "a temporary guard is destroyed at the semicolon, so it must "
              "not hand out a reference");
static_assert(every_rvalue_overload_is_deleted(^^sync_int::const_guard));
static_assert(every_rvalue_overload_is_deleted(^^sync_memo::guard));
static_assert(every_rvalue_overload_is_deleted(^^sync_memo::const_guard));
static_assert(every_rvalue_overload_is_deleted(^^sync_int),
              "lock — a temporary wrapper must not hand out a guard that "
              "outlives it");
static_assert(every_rvalue_overload_is_deleted(^^sync_memo));
```

Differences avec la proposition d'origine : (a) suppression du filtre `is_operator_function`, qui excluait `lock() &&` et `lock_shared() &&` ; (b) ajout de `^^sync_int` et `^^sync_memo` comme cibles, le wrapper lui-meme n'etant jamais inspecte par la version proposee. Le drapeau `found` conserve sa fonction : un type dont on aurait retire toutes les surcharges `&&` renvoie false au lieu de passer par vacuite.

Fichiers de verification : verify_guard_rvalue_delete/corrected_fix.cpp et .../tests/test_synchronized_value.cpp (copie patchee).

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

La fonction réflexive ci-dessus compile et passe contre l'en-tête du dépôt tel quel : scratchpad/audit_syncval/p9_fixes.cpp, rc=0, avec `static_assert(every_rvalue_accessor_is_deleted(^^threadsafe::synchronized_value<int>::guard))`.
Que `requires` ne soit PAS utilisable ici est prouvé hors bibliothèque — scratchpad/audit_syncval/minimal_delete.cpp, rc=1 :
```text
struct X { int &operator*() && = delete("no"); int &operator*() const & { ... } };
static_assert(!requires { *std::declval<X>(); });  // => error: use of deleted function
```
(même résultat avec un `= delete` nu, donc ce n'est pas lié au message de suppression).

</details>

### TEST-05 — Le fichier de tests n'appelle jamais le launcher : les deux corps et les surcharges de diagnostic ne sont jamais instanciés

**MINEUR** · tests · `tests/test_asynchronous_task_launcher.cpp:24`

Les 17 assertions du fichier portent uniquement sur les concepts launchable_task / launchable_scoped_task ; aucune n'instancie asynchronous_task_launcher ni n'appelle launch_task / launch_scoped_task. Conséquences : (a) rien ne vérifie qu'un appel accepté par le concept compile réellement — c'est précisément le trou du finding sur l'invocabilité, qui passe inaperçu ; (b) les surcharges de diagnostic (lignes 60-68 et 77-83 du header), leurs messages et leur non-ambiguïté avec la surcharge contrainte ne sont jamais exercés ; (c) le static_assert de classe ligne 49 n'est même pas déclenché, puisque la classe n'est jamais instanciée dans les tests.

**Code actuel**

```cpp
static_assert(launchable_task<decltype([] {})>,
              "launch_task — a captureless lambda with no args is accepted");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Le patch du finding est correct tel quel — mais retirer la justification (c), fausse. Version verifiee (compilation reelle `-c -Wall -Wextra`, suite complete OK), a ajouter en fin de tests/test_asynchronous_task_launcher.cpp :

```cpp
// Jamais executee : la compiler EST le test. Les concepts ci-dessus prouvent ce
// qui est accepte ; ceci prouve qu'un appel accepte construit reellement un thread.
void the_accepted_calls_compile() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](int value, std::string text) { (void)value; (void)text; },
                         7, std::string{"ok"});
    launcher.launch_task([](std::unique_ptr<int> owned) { (void)owned; },
                         std::make_unique<int>(3));   // callable et argument move-only
    launcher.launch_task([](std::stop_token token) { (void)token; });
    SyncCounter counter;
    launcher.launch_scoped_task([](SyncCounter& shared) { shared.counter.fetch_add(1); },
                                std::ref(counter));
}
```

PIEGE a signaler : la cible de tests est une OBJECT library CMake, donc une vraie compilation et non `-fsyntax-only`. Ne PAS placer cette fonction dans le namespace anonyme du fichier (le reflexe vu le style local) : `-Wall` emet alors `-Wunused-function`. Soit on la laisse a linkage externe comme ci-dessus (verifie sans warning), soit on ecrit `[[maybe_unused]] void the_accepted_calls_compile()` dans le namespace anonyme (aussi verifie sans warning).

L'appel `std::unique_ptr<int>` est la ligne qui porte la valeur : c'est la seule qui attrape la perte d'un `std::move` dans `emplace_back`. La garder.

NE PAS ecrire dans la justification que cela « declenche le static_assert de classe ligne 49 » : celui-ci est deja evalue a l'inclusion du header, la classe n'etant pas un template.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Sonde `p3.cpp` (compilée et exécutée) : ces quatre appels compilent et s'exécutent (« task 7 ok / counter=1 / stop true 5 / uniq 3 »), et prouvent au passage qu'il n'y a AUCUNE ambiguïté entre la surcharge contrainte et la surcharge de diagnostic lorsque les contraintes sont satisfaites. Rien de tout cela n'est couvert aujourd'hui : grep sur tests/ montre que is_task_participant_v, is_scoped_task_participant_v, launch_task et launch_scoped_task n'apparaissent dans aucun .cpp de tests/.

</details>

### TEST-06 — is_lifetime_aware est le trait le moins couvert : ni enums, ni unions, ni classes vides, ni lambdas, ni types fonction, ni le garde is_default_type

**MINEUR** · tests · `tests/test_lifetime_aware.cpp:1`

Task.md exige de tester individuellement chacun des traits. La matrice de formes est couverte de facon tres inegale : test_sendable.cpp (53 assertions) et test_synchronizable.cpp (31) traversent scalaires, pointeurs, references, tableaux, unions, enums, classes vides, lambdas, fonctions, cv ; test_lifetime_aware.cpp (24 assertions) ne teste que scalaires, conteneurs, vues, references, pointeurs, tableaux et smart pointers. Rien sur les enums, les unions, les classes vides, les lambdas, les types fonction, volatile, les champs de bits, les bases virtuelles, ni — le plus important — sur le garde detail::is_default_type, qui cause un faux negatif SURPRENANT et non documente : is_lifetime_aware_v<std::function<void()>> vaut false alors que std::function possede sa cible. Une regression qui rendrait le trait trop permissif sur ces formes ne serait vue par rien. (Le reproche general "les tests n'assertent pas les negatifs" n'est en revanche PAS fonde : 191 negations sur ~412 assertions, soit ~46 %.)

**Code actuel**

```cpp
static_assert(is_lifetime_aware_v<int>,
              "is_lifetime_aware — a value owns its data");
static_assert(is_lifetime_aware_v<std::string>,
              "is_lifetime_aware — a container owns its data");
static_assert(is_lifetime_aware_v<std::vector<int>>,
              "is_lifetime_aware — a container owns its data");
static_assert(is_lifetime_aware_v<Own>,
              "is_lifetime_aware — a user struct owns its data");
```

**Correction** (recompilée et vérifiée contre les 12 fichiers de tests)

Etendre tests/test_lifetime_aware.cpp plutot que creer un fichier matrice qui duplique test_sendable.cpp/test_synchronizable.cpp. Bloc verifie compile (EXTENDED_OK, -Wall -Wextra) ; ajouter <functional> et <variant> aux includes du fichier :

namespace {
enum class Color { red };
union IntOrFloat { int i; float f; };
union HidesBorrow { int i; int *p; };
struct Empty {};
struct AnonymousUnionBorrow { union { int value; int *borrowed; }; };
struct OwnsButUserCopy { std::string owned; OwnsButUserCopy(const OwnsButUserCopy &); };
}

static_assert(is_lifetime_aware_v<Color>,
```cpp
"is_lifetime_aware — an enum owns its value");
```
static_assert(is_lifetime_aware_v<IntOrFloat>,
```cpp
"is_lifetime_aware — a union of scalars is walked like a struct");
```
static_assert(!is_lifetime_aware_v<HidesBorrow>,
```cpp
"is_lifetime_aware — a union alternative that borrows is seen");
```
static_assert(!is_lifetime_aware_v<AnonymousUnionBorrow>,
```cpp
"is_lifetime_aware — an anonymous union is walked into");
```
static_assert(is_lifetime_aware_v<Empty>,
```cpp
"is_lifetime_aware — an empty class has nothing to borrow");
```
static_assert(is_lifetime_aware_v<void()> && is_lifetime_aware_v<void (*)()>,
```cpp
"is_lifetime_aware — code has static storage duration");
```
static_assert(is_lifetime_aware_v<decltype([] {})>,
```cpp
"is_lifetime_aware — a captureless lambda borrows nothing");
```
static_assert(!is_lifetime_aware_v<decltype([x = 0] {})>,
```cpp
"is_lifetime_aware — a closure reflects no members, so its "
"captures cannot be proved owned");
```
static_assert(!is_lifetime_aware_v<OwnsButUserCopy>,
```cpp
"is_lifetime_aware — the structural guard refuses a user-written "
"copy, which may share instead of own");
```
static_assert(!is_lifetime_aware_v<std::function<void()>>,
```cpp
"is_lifetime_aware — same guard, same answer for std::function, "
"although it owns its target");
```
static_assert(!is_lifetime_aware_v<std::variant<int, std::string_view>>,
```cpp
"is_lifetime_aware — an alternative that borrows makes the "
"variant a borrow");
```
static_assert(is_lifetime_aware_v<volatile int>,
```cpp
"is_lifetime_aware — volatile is stripped like const");
```
Les seules formes du fichier propose qui ne sont couvertes par aucun trait aujourd'hui et qui meritent un fichier a part sont structurelles (champs de bits, bases virtuelles, base privee heritee) : si un tests/test_trait_shapes.cpp est cree pour elles, il DOIT etre ajoute a la liste explicite de tests/CMakeLists.txt (add_library(threadsafe_tests OBJECT ...)) — ce point manque au fix d'origine, sans quoi le fichier n'est jamais compile et le test est mort-ne.

Ne PAS presenter le false de std::function comme un bug a corriger : CLAUDE.md documente deja la regle (« non-default types ... fail before the member walk even starts ») et test_sendable.cpp:275 la teste pour is_sendable ; l'ajout ne fait que verrouiller le meme choix assume pour is_lifetime_aware.

<details>
<summary>Preuve — sonde compilée et sortie du compilateur</summary>

Fichier .../scratchpad/audit_tests/missing_test_trait_shapes.cpp : compile SANS erreur contre les headers actuels (SHAPES_OK) — ce sont donc des tests prets a ajouter. La surprise a documenter a ete isolee par p04_lifetime_gaps.cpp :
```text
p04_lifetime_gaps.cpp:20:15: error: static assertion failed: PROBE-A: std::function owns its target
p04_lifetime_gaps.cpp:22:15: error: static assertion failed: PROBE-B: a user-provided copy has nothing to do with ownership
```
(les deux types repondent donc false).

</details>
