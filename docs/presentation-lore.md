# ThreadSafe — le lore de la présentation

Narration pour Meeting C++. Ce document ne décrit pas *ce que fait* la bibliothèque
(`CLAUDE.md` et `docs/audit-synthese.md` s'en chargent) : il décrit **l'histoire qu'on
raconte avec**, dans l'ordre où on la raconte, avec ce qu'on veut que la salle ressente
à chaque étape.

## La thèse

> La thread safety a toujours été une propriété structurelle de vos types.
> C++26 est la première version du langage capable de la lire.

Tout le reste — les traits, le walk, les helpers — n'est que la démonstration de cette
phrase. Rust n'est pas le sujet : c'est la preuve, déjà administrée ailleurs, que la
question a une réponse mécanique.

**Titres candidats (slides en anglais)**
- *It Compiles — Thread Safety as a Property of Your Types*
- *Send and Sync, Compiled — Auto Traits for C++26*
- *What the Compiler Already Knew*

## L'antagoniste

Ce n'est ni le data race, ni `std::thread`, ni le programmeur pressé.

**L'antagoniste est une phrase : « ça compile ».**

Elle revient à chaque acte, de plus en plus ironique. Elle est vraie à chaque fois.
C'est ce qui la rend dangereuse : le compilateur ne ment pas, il ne sait simplement pas
ce qu'on lui demande de vérifier.

Son complice s'appelle `const`. La salle croit que `const` veut dire « personne n'écrit ».
`const` veut dire « *je* n'écris pas à travers *cette* poignée ». La différence tient un
acte entier (Acte V).

---

## Acte I — La scène de crime

Trois lignes au tableau. Elles compilent toutes les trois. Les trois sont de l'UB ou le
deviennent au premier refactor :

```cpp
std::thread t{[&data] { data.push_back(42); }};   // capture par référence
auto shared = std::make_shared<Widget>();         // partagé, Widget non synchronisé
int Widget::count() const { return *counter_; }   // const, mais counter_ est un int*
```

Demander à la salle : « qui a écrit ça cette année ? ». Toutes les mains.

**Le point** : aucun de ces trois bugs n'est un bug de *code*. Ce sont trois bugs de
*type*. L'information manquante — « ce `Widget` peut-il traverser une frontière de
thread ? » — n'est écrite nulle part, alors qu'elle est entièrement déterminée par la
structure de `Widget`.

Sentiment visé : **inconfort de reconnaissance**. Pas de la peur, de la familiarité.

## Acte II — Pourquoi C++ ne pouvait pas

Le standard définit le data race, puis dit : UB. Il ne donne aucun moyen de *poser la
question* à un type. Et les contournements qu'on connaît tous échouent tous de la même
façon :

- une convention de nommage : ne se propage pas aux membres ;
- une macro, un tag, une base `ThreadSafe` : il faut l'écrire sur chaque type, y compris
  ceux qu'on ne possède pas ;
- un sanitizer : il faut que le bug se produise, sur la machine de test, ce jour-là.

Le point commun : **tous demandent au développeur de redire ce que la structure du type
dit déjà.** Le compilateur voit les membres. Il ne pouvait juste pas nous les montrer.

Sentiment visé : **c'était structurel, pas culturel.** Ce n'est la faute de personne.

## Acte III — L'emprunt (et ce qu'on emprunte vraiment)

Rust arrive ici, et repart vite. Ce qu'on prend n'est pas la syntaxe, ni le borrow
checker : c'est une idée de deux mots — **auto trait**.

`Send` et `Sync` ne sont pas déclarés. Ils sont *déduits* : un type est `Send` si tous ses
membres le sont. Rien à annoter, rien à maintenir, et ça marche sur les types des autres.

Et un auto trait, lu par un ingénieur C++, c'est une phrase très simple :

> une fonction récursive sur les membres d'un type.

C'est exactement ce que `std::meta` sait faire depuis C++26. **L'idée était disponible
depuis dix ans ; c'est l'outil qui manquait.**

Sentiment visé : **le déclic.** C'est ici que la salle comprend que la suite est faisable.

## Acte IV — Trois questions

On ne pose pas « est-ce thread-safe ? » : la question n'a pas de réponse. On en pose trois,
dans le vocabulaire de la bibliothèque :

| Question | Trait | Rust |
|---|---|---|
| Puis-je **envoyer** ce `T` d'un thread à un autre ? | `is_sendable<T>` | `Send` |
| Puis-je l'**utiliser depuis plusieurs threads à la fois** ? | `is_synchronizable<T>` | `Sync` |
| Ce `T` **possède-t-il** ce qu'il désigne ? | `is_lifetime_aware<T>` | *(le borrow checker)* |

Le troisième est le seul qui n'ait pas d'équivalent en face, et c'est le plus intéressant
à présenter : **Rust n'en a pas besoin, parce que son borrow checker le fait gratuitement
et en continu.** Nous n'en avons pas. Alors on remplace un vérificateur de durées de vie
par une question posée une fois, à l'entrée du thread : *est-ce que ce que je lance
possède ce dont il a besoin ?*

C'est le moment d'honnêteté de la conférence : **on ne reproduit pas Rust, on reproduit
deux traits et demi, et la demie est explicitement moins forte.** Dire cette phrase tôt
achète toute la crédibilité du reste.

Et le corollaire qui fait toujours mouche :

```cpp
is_sendable<T&> == is_synchronizable<T>
```

Envoyer une référence à un autre thread *est* un partage. Une ligne, et la moitié des
bugs de l'Acte I tombe.

## Acte V — `const` est un mensonge

Le pic émotionnel. Un seul type au tableau :

```cpp
struct Widget { int* counter_; };
const Widget w;   // ça compile
```

`w` est `const`. La salle est prête à le partager entre threads. `*w.counter_` est
modifiable par n'importe qui.

D'où la règle, écrite en gros, qui traverse toute la bibliothèque :

> **Le `const` derrière une indirection n'est jamais cru.**

C'est une phrase que l'audience peut remporter chez elle même si elle n'écrit jamais de
réflexion. C'est la meilleure ligne du talk.

Sentiment visé : **la trahison.** Le mot-clé en qui tout le monde a confiance est celui
qui ment.

## Acte VI — La machine, en un écran

`diagnose_is_sendable` tient sur une slide. C'est tout le moteur. Le ressort dramatique
est qu'il n'y a **rien à cacher** : pas de métaprogrammation cryptique, une suite de `if`
qui se lit à voix haute.

Trois idées à faire passer, et seulement trois :

1. **`_v` est la mémoire.** Le compilateur n'instancie une variable template qu'une fois
   par `T` : le walk sur une hiérarchie ne tourne qu'une fois par unité de traduction,
   quel que soit le nombre de fois qu'on pose la question. (L'audit le vérifie : un DAG de
   profondeur 60, soit 2⁶⁰ chemins naïfs, compile au temps de base.)
2. **La récursion passe par `_v`, jamais par la fonction.** Conséquence : une
   spécialisation écrite dans le `.cpp` de l'utilisateur, bien après la bibliothèque, est
   quand même atteinte par le walk.
3. **Le walk est conservateur : tout ce qu'il ne peut pas prouver est un non.** État non
   réflectable, vues empruntantes, copie ou destructeur écrits à la main, constructeur
   template qui pourrait détourner la copie — refusés avant même de regarder les membres.

De quoi découle la maxime de conception de la bibliothèque :

> **Un « non » n'a jamais besoin d'être affirmé. Seule la confiance doit l'être.**

Deux questions, enfin, ne reçoivent pas de réponse mais un **refus** : `void` et les types
incomplets. Ce n'est pas `false`, c'est un `static_assert`. « Complète le type — ou
vouche pour lui — avant de poser la question. »

## Acte VII — Le mot `unsafe`

Le walk ne peut pas tout prouver. `std::vector`, `std::atomic`, `std::shared_ptr` : leur
thread safety est un fait d'ingénierie, pas une propriété de leurs membres. Il faut
quelqu'un pour l'affirmer.

D'où l'unique point de personnalisation, et sa forme :

```cpp
template <class T>
struct threadsafe::is_unsafe_synchronizable<threadsafe::synchronized_value<T>>
    : std::bool_constant<threadsafe::is_sendable_v<T>> {};
```

Trois choses à souligner :

- **Les traits sûrs sont fermés.** On ne spécialise pas `is_sendable`. On ne peut vouloir
  que *donner* sa confiance, jamais forcer un refus : `false` et « rien affirmé » sont la
  même chose, et retombent sur le walk. Il n'y a pas de troisième état.
- **Le mot `unsafe` apparaît exactement là où une connaissance remplace une preuve.**
  C'est la reddition de comptes : `grep unsafe` donne la liste complète de ce qu'il faut
  croire sur parole.
- Et le clou : la ligne ci-dessus **est** `impl<T: Send> Sync for Mutex<T>`. La même
  phrase, dans l'autre langage. Montrer les deux côte à côte, sans commentaire.

## Acte VIII — La récompense

Les traits ne servent à rien tant qu'ils ne refusent pas du code réel. Trois helpers, un
argument chacun :

- **`asynchronous_task_launcher`** — la frontière. C'est le seul endroit où l'on paie :
  `launch_task` exige `sendable` et `lifetime_aware`. La lambda de l'Acte I ne compile
  plus, et le message dit pourquoi.
- **`synchronized_value<T>`** — le verrou choisi par le type. `shared_mutex` si
  `const T` est synchronizable, `mutex` sinon. **Le type system choisit le verrou.** Le
  `static_assert(sendable<T>)` est dans le constructeur, pas dans le corps de la classe :
  demander un trait sur `X<T>` *complète* `X<T>`, donc un invariant d'usage écrit dans le
  corps rendrait le type inquestionnable. C'est une leçon de design qui dépasse le sujet.
- **`copy_on_write<T>`** — partagé en lecture, copié à la première écriture.

## Acte IX — Le procès

L'acte qui distingue ce talk d'une démo. La bibliothèque a été auditée de façon
adversariale : unions anonymes, bitfields, `[[no_unique_address]]`, héritage en diamant,
types polymorphes, `mutable`, shallow-const à tous les étages, closures, vues empruntantes,
deleters malveillants, `atomic<shared_ptr<T>>`. 141 sondes compilées, chaque constat soumis
à un vérificateur chargé de le **réfuter**.

Résultat : **aucun type non thread-safe n'est accepté par le walk.**

Et la seule faille trouvée est la meilleure diapositive du talk :
`const std::unique_ptr<T, D>` était vouché **pour n'importe quel deleter** — or un deleter
no-op, c'est précisément l'idiome du pointeur observateur qui ne possède rien.

> La machine avait raison partout. C'est l'humain, dans la couche où il affirme au lieu de
> prouver, qui s'est trompé.

C'est la justification rétrospective de tout l'Acte VII : on isole le mot `unsafe` parce
que c'est là que les bugs vivent.

## Acte X — Ce que ça n'attrape pas

On finit par les limites, jamais par la démo. Une audience de Meeting C++ pardonne une
limite annoncée et ne pardonne pas une limite découverte.

- **Pas de borrow checker.** La `T&` rendue par `as_mutable()` n'est exclusive qu'à
  l'instant de l'appel : copier le handle juste après re-partage le bloc. Chaque étape est
  légitime isolément. C'est exactement ce que `Arc::make_mut` ferme côté Rust, et ce qu'on
  ne peut pas fermer ici.
- **Les deadlocks ne sont pas détectés** — parité exacte avec Rust, à dire explicitement.
- **Les race conditions logiques restent possibles** :
  `if (sv.lock_shared()->empty()) sv.lock()->push_back(x);` compile.
- **Des faux négatifs assumés** : une `struct` d'atomics, un type possédant récursif, un
  constructeur forwarding. Le walk conservateur les refuse par prudence, pas par verdict.

Et la conclusion qui referme l'antagoniste :

> Les faux négatifs coûtent un `static_assert`. Les faux positifs coûtent un data race en
> production. On sait lequel des deux on veut.

## La dernière slide

Revenir aux trois lignes de l'Acte I. Les trois ne compilent plus. Les trois messages
d'erreur expliquent pourquoi.

> Le compilateur savait. Il n'avait simplement jamais eu le droit de le dire.

---

## Rythme et fils rouges

**Répliques récurrentes** — « Ça compile. » (Actes I, V, IX) · « Tout ce que je ne peux pas
prouver est un non. » · « Le `const` derrière une indirection n'est jamais cru. »

**Découpage indicatif (60 min)** — I-II : 8 min · III-IV : 10 min · V : 6 min ·
VI : 12 min · VII : 8 min · VIII : 8 min · IX : 5 min · X + fin : 5 min.

**Formats courts** — en 30 min : I, IV, V, VI, X. En lightning : l'Acte V seul, il se
suffit.

**À ne pas faire** — ouvrir sur la réflexion (c'est l'outil, pas le sujet) ; opposer C++ et
Rust (on emprunte une idée, on ne fait pas un match) ; présenter les helpers avant les
traits (ils sont la conséquence) ; cacher l'Acte X pour finir sur un applaudissement.
