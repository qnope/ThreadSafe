# Audit ThreadSafe — synthèse

Audit de la bibliothèque au commit `64f9c06`, contre les huit axes de `Task.md`.

Méthode : douze auditeurs indépendants, un par axe, chacun tenu de **compiler une
sonde** pour chaque affirmation — aucun constat n'est accepté sur un raisonnement
seul. Chaque constat est ensuite soumis à un vérificateur dont la mission est de le
**réfuter**, sous un angle imposé (exactitude / reproduction / intention de design),
avec consigne de conclure « faux » en cas de doute. Une seconde vague de trois
agents a balayé ce que personne n'avait couvert.

| | |
|---|---|
| Agents | 127 |
| Constats levés | 112 |
| **Confirmés après réfutation** | **61** (49 défauts distincts) |
| Réfutés | 51 |
| Gravité | 5 critiques · 20 majeurs · 23 moyens · 9 mineurs · 4 détails |

Le détail complet — code fautif, correction, reproduction compilée, compte rendu de
réfutation — est dans [`01-details.md`](./01-details.md). Les identifiants `Fxx`
ci-dessous y renvoient.

## Le verdict en une phrase

**Le modèle est juste, son implémentation a cinq trous**, et le défaut le plus
profond n'est pas dans une règle mais dans la rencontre entre deux choix par
ailleurs corrects : *un trait mémoïsé* et *un point d'extension à réponse tardive*.

Les 51 constats réfutés comptent autant que les 61 retenus : la marche structurelle,
la règle `is_sendable<T&> = is_synchronizable<T>`, le refus des closures à captures,
la garde sur les copy/move écrits à la main, la règle des wrappers standard — tout
cela a été attaqué et a tenu.

## Les cinq critiques

**F01 — La garde `dynamic_type_is_known` n'est posée que sur `unique_ptr`.**
`T*`, `T&`, `T&&`, `shared_ptr`, `weak_ptr`, `reference_wrapper` et les branches
pointeur / membre-référence de la marche `const` consomment une réponse
`is_synchronizable<T>` sans elle. Une spécialisation sur une base polymorphe non
`final` bénit donc toute la hiérarchie, y compris les dérivées qui ne se
synchronisent pas. Bout en bout : `launch_scoped_task` compile sans un mot un
programme qui court sur un `int` nu. C'est la seule incohérence *interne* de la
bibliothèque — la même garde est appliquée trois lignes plus loin, sur `unique_ptr`.

**F03 — Et la garde elle-même est trop faible.** Elle demande
`!is_polymorphic_v<T> || is_final_v<T>`, c'est-à-dire « ce type a-t-il des fonctions
virtuelles ». Mais ce qui autorise du stockage dérivé derrière une base, ce n'est
pas le polymorphisme, c'est un chemin de destruction correct : `shared_ptr` type-efface
toujours son deleter, donc `shared_ptr<Base> p = make_shared<Derived>()` est légal
pour une `Base` totalement non polymorphe. La suite de tests ne couvre que le cas
polymorphe. Atteint jusqu'au `make()` de la bibliothèque elle-même.

**F02 — Un type propriétaire récursif ne répond pas : il casse la compilation.**
`struct Node { std::vector<Node> kids; };` — la forme la plus banale d'une structure
*propriétaire*, exactement ce que le trait existe pour bénir — fait ré-entrer
`is_lifetime_aware_v<Node>` dans une spécialisation encore en cours
d'instanciation. GCC sort une erreur dure qu'aucun `if constexpr` ni SFINAE ne
rattrape. Les trois traits tombent pareil. C'est la première chose qu'un spectateur
tapera.

**F23 — `as_mutable()` rend un `T&` nu.** Le test d'exclusivité vaut pour l'instant
où il tourne ; la référence, elle, lui survit. Copier le handle ensuite — ce que
`is_sendable_v<copy_on_write<T>>` bénit explicitement — repartage le bloc dans
lequel l'appelant écrit encore. Reproduit en heap-use-after-free sous AddressSanitizer,
avec un programme qui **suit la discipline documentée par la bibliothèque**
(« one `copy_on_write` object belongs to one thread; share by copying it »). La
bibliothèque connaît le danger : `value_guard` supprime `operator*() &&` pour
exactement cette raison. `copy_on_write`, dont toute l'histoire de sûreté *est* le
détachement, n'a pas cette protection.

**F24 — `synchronized_value` choisit son *type de mutex* depuis un trait
extensible.** La taille de l'objet et l'offset de `value_` dépendent donc de
`is_synchronizable_v<const T>` au premier point d'instanciation. Une TU qui n'a pas
encore vu l'opt-in construit 72 octets autour d'un `std::mutex` ; une TU qui l'a vu
en construit 208 autour d'un `std::shared_mutex`. Même nom mangé, lien silencieux,
puis `lock()` sur un `shared_mutex` jamais construit — abort. C'est le seul endroit
de la bibliothèque où une réponse à liaison tardive atterrit dans un *layout*.

## Le thème de fond : mémoïsation × réponse tardive

F24 est le cas spectaculaire d'un problème plus large (**F04**). Les traits sont des
spécialisations de templates : la première instanciation **gèle** la réponse pour
toute la TU. Or deux choses répondent tard :

- la **complétude** — un type incomplet est rejeté par un `false`, pas par une
  erreur ; le compléter ensuite ne dégèle rien. L'idiome pimpl, celui que le
  message d'erreur de la bibliothèque *recommande*, donne des réponses opposées
  dans le `.cpp` qui complète l'impl et dans toutes les autres TU ;
- la **spécialisation utilisateur**, qui est le mécanisme d'extension officiel
  (`tests/test_deferred_specialization.cpp` en fait la démonstration).

Les deux produisent de l'IFNDR sans le moindre diagnostic. Pour une bibliothèque
pédagogique, c'est le pire piège possible : tout compile, tous les `static_assert`
passent, et la réponse de sûreté dépend de l'ordre des `#include`.

C'est, à mon sens, le sujet le plus intéressant de tout l'audit — et probablement
plus une **diapositive** qu'un correctif : « voici pourquoi Rust peut poser
`Send`/`Sync` sur un graphe global, et pourquoi C++ ne le peut pas ».

## Ce que le public verra : les diagnostics mentent

`descend_sendable`, `descend_lifetime_aware` et `descend_const_synchronizable`
re-lancent **toujours** la marche structurelle pour formuler la raison, y compris
quand la réponse `false` venait d'une spécialisation que la marche n'a jamais
consultée (F05, F06, F07, F09). La marche s'arrête alors sur la première chose
qu'elle n'aime pas — un template de constructeur — et la raison affichée est
inventée.

Concrètement, sur la faute la plus probable d'une démo live :

```
launcher.launch_task([](std::shared_ptr<int> p){}, std::make_shared<int>(1));
→ « std::shared_ptr<int> has a user-written copy, move or destructor …
   specialize is_sendable to state the intent »
```

La vraie raison est que `int` n'est pas synchronizable. Le conseil affiché, s'il est
suivi, **désactive la règle de sûreté centrale de la bibliothèque** pour tous les
`shared_ptr<int>`. Même chose pour `std::vector<int*>`, qui se voit reprocher les
membres de layout de libstdc++ ; et pour `unique_ptr<Base>`, à qui l'on conseille
« hold a `std::shared_ptr` to it » alors que `shared_ptr<Base>` est refusé pour la
même raison — le conseil boucle. Pour une closure à captures (F08), le conseil
« specialize `is_sendable` » est littéralement inapplicable : on ne peut pas écrire
une spécialisation explicite pour un type de closure déclaré en portée bloc.

Le travail sur les diagnostics est réel et visible dans le code ; il est aujourd'hui
correct sur la moitié des cas — celle où la réponse vient bien de la marche.

## Les faux négatifs, par ordre de visibilité sur scène

La bibliothèque refuse du code parfaitement correct dans des cas très ordinaires :

- **tout allocateur conforme** (F11) : `[allocator.requirements]` impose le
  constructeur template de rebind, que `may_hijack_copy_move` rejette. `std::allocator`
  n'échappe que parce que `vocabulary.h` le code en dur — donc tous les alias
  `std::pmr::` répondent `false`. Pire, `is_lifetime_aware` ne passe pas cette garde,
  et les trois traits se contredisent sur un même type ;
- **`std::chrono::duration`** (F55) : `launch_task(f, 5ms)` ne compile pas. La liste
  des wrappers standard est fermée à 18 templates ; tout le reste tombe dans la
  marche structurelle, qui rejette la plupart du vocabulaire standard
  (`bitset`, `complex`, `expected`, `queue`, `stack`, `valarray`, `flat_map`) ;
- **les primitives de synchronisation** (F14, F19) : `latch`, `barrier`,
  `counting_semaphore`, `atomic_flag`, `once_flag` ne sont jamais bénies, donc un
  membre `mutable std::mutex` coule la réponse `const` — soit précisément le type
  qu'on écrit pour *être* thread-safe ;
- **`copy_on_write` imbriqué** (F25, F27, F30) : il manque la règle
  `is_synchronizable<const copy_on_write<T>>`, et l'absence est contagieuse — un
  arbre de COW, l'usage canonique du motif, est inatteignable ;
- **`std::thread::id`** (F18), **`source_location`**, **`error_code`**,
  **`type_index`** (F16), les vues génératrices (`iota_view`) (F15).

## Thread safety du code d'exécution

Deux défauts réels, aucun dans les traits :

- **F32** — le launcher n'a pas de destructeur, donc `~vector<jthread>` arrête et
  joint les tâches **une par une** : la tâche `k` est jointe avant que la `k+1` ne
  soit seulement prévenue. Latence de fermeture = somme des latences, et surtout
  **interblocage** dès qu'une tâche attend qu'une autre ait vu son stop — la forme
  producteur/coordinateur standard, sans que l'utilisateur ait rien écrit de faux ;
- **F33** — `launch_scoped_task` joint avant que quoi que ce soit ait pu demander
  l'arrêt : le `stop_token` injecté par `jthread` n'est jamais déclenché, alors que
  `launch_task`, sur le même callable, fonctionne. Les deux points d'entrée sont en
  désaccord sur le contrat de l'argument que la classe certifie par `static_assert`.

`copy_on_write` (F34) et `synchronized_value` sont par ailleurs corrects du point de
vue du modèle mémoire ; la barrière `acquire` est la seule ligne non commentée d'un
code qui commente tout le reste.

## Performance

**Compilation** (F49) — chaque réponse `false` d'un trait **rend un message que le
trait jette aussitôt**. `path.empty()` signifie déjà exactement « c'est le trait qui
demande, il va jeter ». Mesuré, hors bibliothèque, à N=800 : `return false` 0,0125 ms,
exception à message littéral 0,56 ms (45×), exception à message rendu 1,04 ms (83×).
Dans la bibliothèque, sur 400 types : 0,65 ms/type pour un `true`, 2,33 ms/type pour
un `false`. C'est le seul gain de compilation d'un ordre de grandeur disponible, et
il ne coûte aucune lisibilité. Le PCH (F50) rendrait ~7,0 s sur 7,5 s en build série,
mais ne rapporte rien en parallèle.

**Exécution** (F51) — `synchronized_value<T>` choisit `std::shared_mutex` pour
*tout* `T` de type valeur (`int`, `string`, `vector`, `map`), sans paramètre ni
échappatoire. Or `is_synchronizable<const T>` est le trait *structurel* : il est vrai
pour à peu près tout ce qu'on met dans un `synchronized_value`. Mesuré : 35 % plus
lent non contendu, ~18× pour 8 lectures courtes concurrentes, ~105× pour 4–8
écritures, et 208 octets contre 72. Le défaut par défaut est le cas pessimal.
(F52) — le launcher retient chaque `jthread` terminé pour toute sa vie : 200 000
tâches = 91 s et 2,33 Go, contre 0,26 s et 8,9 Mo avec un pool minimal.

Les traits, eux, ne coûtent rien à l'exécution : vérifié.

## Simplicité — pour une bibliothèque de conférence

- **F39** — la machinerie de diagnostic (`descend_` / `explain_` / `default_is_`) est
  écrite **trois fois**, à l'identique au nom du trait près ; quatre blocs de
  commentaires sont dupliqués mot pour mot, et deux copies renvoient au commentaire
  de la troisième. Le public voit le même paragraphe trois fois et n'apprend rien la
  deuxième ;
- **F40** — les deux seuls splices `[: :]` calculant un type servent à choisir entre
  `shared_mutex`/`mutex` et `shared_lock`/`unique_lock`, ce que `std::conditional_t`
  fait en une ligne. C'est l'exemple le plus net de réflexion employée là où un
  template ordinaire se lit mieux : sur scène, cela enseigne que C++26 a besoin de la
  réflexion pour un `typedef` conditionnel, exactement le message inverse de celui
  qu'on veut faire passer. Les deux helpers sont en prime `public` ;
- **F41** — `asynchronous_task_launcher.h` est le seul en-tête non autonome : son
  `static_assert` de classe ne tient que parce que `threadsafe.h` inclut
  `vocabulary.h` avant lui.

## Ce que je corrigerais, dans cet ordre

1. **F02** — répondre au lieu de casser sur les types récursifs. C'est la forme la
   plus courante d'un type propriétaire, et l'échec est une erreur dure.
2. **F01 + F03** — une seule règle nommée, `is_synchronizable_through_indirection`,
   posée aux onze sites qui traversent une indirection, et une garde qui teste le
   chemin de destruction et non le polymorphisme. Le coût — un opt-in sur une base
   non `final` devient inutilisable à travers une indirection — est à énoncer dans
   le commentaire, pas à masquer : la bibliothèque le paie déjà pour `unique_ptr`.
3. **F23** et **F24** — le `T&` nu et le mutex choisi par un trait. Deux corruptions
   mémoire reproduites.
4. **F05–F09** — ne re-dériver une raison que lorsque la réponse vient réellement de
   la marche ; sinon dire « répondu par une spécialisation » et s'arrêter là. C'est
   ce que le public lira.
5. **F49** — ne rendre le message que si quelqu'un le lit. Un ordre de grandeur, sans
   contrepartie.
6. **F54** (`is_sendable_v<synchronized_value<T>>` tue la TU au lieu de répondre),
   **F33**, puis les faux négatifs par ordre de visibilité.

Et deux choses à **dire** plutôt qu'à corriger : la mémoïsation face aux réponses
tardives (F04/F24), et le fait qu'une réponse structurelle sur un type statique ne
prouve rien de l'objet réellement présent derrière une indirection (F01/F03). Ce
sont les deux endroits où le C++ résiste au modèle de Rust, et ils valent mieux
qu'un correctif silencieux.

---

*Un constat écarté est une propriété vérifiée : les 51 réfutations sont listées en
annexe de [`01-details.md`](./01-details.md).*
