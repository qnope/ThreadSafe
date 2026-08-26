# 09 — Méthodologie

Cet audit porte sur la bibliothèque au commit `64f9c06`. Aucune affirmation chiffrée
n'y est reprise d'un audit antérieur ni déduite d'une lecture : **tout a été
recompilé ou réexécuté**. Ce rapport dit comment, et surtout où la méthode s'arrête.

## Le principe : chercher, puis essayer de se réfuter

Trouver un défaut dans une bibliothèque de sûreté est facile et trompeur — la
plupart des « failles » sont des choix conservateurs délibérés, ou des types déjà
refusés plus loin sur le chemin réel de l'utilisateur. La méthode tient donc en
trois temps :

1. **Sonder.** Un agent par trait, par helper et par axe de qualité, chacun écrivant
   et compilant de vrais scénarios plutôt que de raisonner sur le code.
2. **Réfuter.** Chaque défaut annoncé est confié à un vérificateur indépendant dont
   la consigne est de le *démolir* : recompiler le reproducteur, vérifier que la
   conclusion suit, appliquer le correctif proposé sur une **copie** des en-têtes,
   et recompiler **les onze TU de la suite existante** pour mesurer les régressions.
3. **Ne garder que ce qui survit.**

L'audit précédent avait établi la valeur de l'étape 2 : sur six failles d'abord
annoncées « critiques », **une seule** avait survécu. Le présent audit ajoute une
quatrième étape, décrite plus bas : la relecture personnelle des critiques.

## L'interruption, et pourquoi elle est mentionnée ici

La première passe a été **tuée en cours de route par une limite de session**. Trois
des quatre workflows sont morts avec leurs agents à mi-course. Leurs conclusions
écrites ont été perdues ; **leurs artefacts ne l'ont pas** — 1 066 fichiers de
scénarios et de sorties de compilation étaient sur disque.

La reprise n'a donc pas consisté à tout refaire, mais à **récolter** : six agents ont
repris chaque corpus survivant, l'ont **rejoué contre les en-têtes réels**, et n'ont
rapporté que ce qu'ils ont observé eux-mêmes. Consigne explicite : traiter chaque
fichier du corpus comme une *affirmation non vérifiée* de l'agent précédent, y
compris son nom et ses commentaires.

Cela se voit dans les chiffres, et c'est une bonne nouvelle pour la crédibilité du
reste : sur **363 scénarios rejoués, 319 ont reproduit et 42 sont déclarés impasses** —
des expériences abandonnées, des fichiers qui n'assertaient rien, ou des conclusions
simplement fausses. Le détail de chaque impasse est consigné dans les notes des
rapports concernés. Un exemple : `08_const_cast_uaf_run.cpp` ne déclenche jamais son
propre critère de succès (« observed 0 corrupted elements », 2 000 tours × 3
exécutions) — la course qu'il vise est pourtant réelle, mais **ce fichier ne la
démontre pas**, et il aurait été cité comme preuve sans le rejeu.

## Ce que j'ai vérifié moi-même

Le récolteur n'a pas eu d'étape de réfutation indépendante — la limite de budget a
imposé ce choix. Pour compenser, **chaque défaut critique a été recompilé et
réexécuté personnellement**, sans passer par un agent :

| vérification | résultat |
|---|---|
| la suite de référence | 11/11 TU vertes, build propre 7,7 s, GCC 16.2.0 |
| blanchiment par `const` (`is_sendable`) | reproduit sur un test minimal de 10 lignes que j'ai écrit |
| le correctif de ce blanchiment | écrit, compilé, **11/11 TU toujours vertes** |
| divergence ODR entre deux TU | `sizeof` 72 contre 208, lien **sans diagnostic**, abort 134 |
| interblocage de `launch_scoped_task` | chien de garde, sortie 42 |
| course de réentrance du lanceur | sorties 139, 133, 139 sur trois exécutions |
| use-after-free du deleter de `shared_ptr` | ASan, `WRITE of size 4` |
| handle lié à un thread accepté | SIGSEGV, sortie 139 |
| la limite inhérente (membre statique) | TSan : course réelle, **les deux threads en `shared_lock`** |
| 4 mutants « survivants » critiques | re-tués : tous survivent bien aux 11 TU |

Les défauts non critiques restent attribués aux agents qui les ont produits, avec
leur reproducteur complet : le lecteur peut les rejouer, et le [corpus](./scenarios/)
est là pour ça.

## Le test par mutation

La couverture d'une suite entièrement faite de `static_assert` ne se mesure pas par
des lignes exécutées. On l'a donc mesurée en **cassant délibérément la bibliothèque**
et en regardant si la suite s'en aperçoit.

Boucle, pour chaque mutant : restaurer une copie vierge des en-têtes, appliquer
**exactement une** modification, recompiler les onze TU. Une TU qui échoue = mutant
**tué**. Onze TU qui passent = mutant **survivant**, donc un trou de couverture.

Deux précautions comptent plus que le score lui-même :

- **Les mutants équivalents sont exclus.** Un mutant qui ne change aucune réponse
  observable n'est pas un trou, c'est un non-événement. Avant de compter un survivant,
  il fallait produire un scénario où la version vierge et la version mutée
  **répondent différemment**, compilé contre les deux. 14 mutants ont été écartés
  ainsi, chacun avec sa preuve de non-observabilité.
- **Chaque test tueur est vérifié dans les deux sens** : il doit compiler proprement
  contre les en-têtes vierges *et* échouer contre le mutant. Un test qui échoue
  contre la version vierge n'est pas un test, c'est une assertion fausse — et c'est
  le mode de défaillance le plus fréquent.

Bilan : **260 mutants, 167 tués, 91 survivants, 14 équivalents**, soit 77 survivants
réels et un score de 167/246 = **68 %**. Détail en [03](./03-couverture-de-tests.md).

## Prouver une course sans ThreadSanitizer

**TSan n'est pas disponible avec GCC sur cette machine** (arm64 darwin) : l'édition
de liens échoue sur `Undefined symbols: ___tsan_func_entry`. Apple clang, lui, a un
TSan qui fonctionne — mais ne compile pas la réflexion, donc pas la bibliothèque.

La sortie est une **extraction** : le corps d'exécution du helper est recopié en C++
ordinaire, sans réflexion, en y figeant le choix que la bibliothèque a fait. Pour
`synchronized_value<LookupTable>`, on fige `std::shared_mutex` et
`std::shared_lock` — parce qu'un `static_assert` compilé contre les **vrais**
en-têtes établit d'abord que c'est bien ce que la bibliothèque choisit :

```cpp
static_assert(std::is_same_v<threadsafe::synchronized_value<LookupTable>::mutex,
                             std::shared_mutex>);
```

C'est le point important : **l'extraction ne prouve rien toute seule**. Elle ne vaut
que par la paire — un `static_assert` contre les vrais en-têtes pour établir *ce que
la bibliothèque décide*, puis l'extraction sous TSan pour établir *ce que cette
décision coûte à l'exécution*. Chaque extraction a été diffée ligne à ligne contre
l'en-tête (constructeur de `value_guard`, `operator->`, disposition des membres,
corps de `lock_shared`) et le rapport qui la cite dit toujours de quel compilateur
vient quelle preuve.

**AddressSanitizer, lui, fonctionne avec GCC 16 ici**, donc les use-after-free sont
prouvés directement sur la bibliothèque réelle, sans extraction.

## Les mesures

- **Compilation** : `-fsyntax-only`, meilleur de 3, machine au repos, plus
  `-ftime-report` pour la ventilation. Le point méthodologique décisif est le
  **témoin** : une TU *vide* qui n'inclut que l'en-tête parapluie. Sans lui, on
  attribue aux traits un coût qui est celui de l'analyse syntaxique des en-têtes
  standards — c'est exactement l'erreur que le présent audit corrige dans
  [06](./06-performance-compilation.md).
- **Exécution** : `-O2 -pthread`, machine au repos, et systématiquement **contre un
  équivalent écrit à la main** (`std::mutex` nu, `std::jthread` nu). Un chiffre absolu
  ne dit rien ; c'est le rapport à la version manuelle qui est la mesure.
- Les mesures prises pendant que d'autres agents compilaient ont été **refaites** au
  calme. Les chiffres publiés viennent tous d'une machine inoccupée.

## Ce que la méthode ne peut pas atteindre

À dire aussi nettement que les résultats :

- **Aucun corpus ne prouve une absence de défaut.** 357 scénarios qui n'ont pas cassé
  une règle ne démontrent pas qu'elle est correcte. Les listes « ce qui a résisté »
  des rapports sont des *tentatives échouées*, pas des preuves.
- **La réflexion ne lit que des déclarations.** Aucun scénario ne peut découvrir ce
  qu'un corps de fonction `const` fait vraiment. C'est la limite centrale de l'audit
  comme de la bibliothèque, et [01](./01-robustesse-des-traits.md) l'énonce.
- **Une seule machine, un seul compilateur** : GCC 16.2.0, arm64 darwin, libstdc++.
  Les mesures de mutex et les détails de messages dépendant de l'implémentation
  changeront ailleurs.
- **Les courses sont probabilistes.** Un scénario qui ne déclenche pas TSan n'établit
  rien ; seul un déclenchement est une preuve.

## Le corpus

357 scénarios conservés dans [`scenarios/`](./scenarios/) avec le harnais qui les
rejoue, répartis en `adversary/` (27), `traits/` (147), `helpers/` (93),
`diagnostics/` (40), `ergonomics/` (34) et `performance/` (16). Vingt-deux fragments
générés qui ne compilaient pas isolément ont été retirés : un corpus « rejouable »
qui ne se rejoue pas ne vaut rien. Le [README](./scenarios/README.md) explique
pourquoi la moitié du corpus est *censée* être rejetée.
