# Les scénarios de l'audit

Chaque affirmation chiffrée des rapports vient d'une compilation ou d'une exécution
réelle. Les fichiers rassemblés ici sont ces compilations : ils sont conservés tels
qu'ils ont été écrits et rejoués, pour que n'importe quelle conclusion puisse être
vérifiée à la main.

## Rejouer

```bash
cd docs/scenarios
./replay.sh                      # tous les répertoires
./replay.sh adversary helpers    # une sélection
VERBOSE=1 ./replay.sh traits     # nommer chaque fichier rejeté
```

Le harnais utilise `g++-16` et `../../include`. Les deux se surchargent :

```bash
CXX=g++-16 THREADSAFE_INCLUDE=/chemin/vers/include ./replay.sh
```

## Lire le résultat

`replay.sh` ne fait que compiler. **Un scénario rejeté n'est pas un scénario en
échec** — la moitié du corpus existe précisément pour être rejetée :

| ce que fait le fichier | ce que « rejeté » signifie |
|---|---|
| une suite de `static_assert` qui décrivent la réponse de la bibliothèque | le fichier **compile** ; c'est l'assertion qui est l'artefact |
| une construction que la bibliothèque doit refuser | le fichier est **rejeté**, et c'est le **texte du refus** qui est l'artefact — voir [04](../04-diagnostics.md) |
| une démonstration de défaut | le fichier est **rejeté**, et le refus *est* le défaut : `static assertion failed` sur une ligne qui énonce le comportement sûr attendu |

Trois échecs se distinguent des autres et sont eux-mêmes des résultats, documentés
dans [01](../01-robustesse-des-traits.md) : un type récursif (`cyc_*_const.cpp`,
`const_sync_w1_recursion.cpp`) rend le trait **mal formé** au lieu de répondre
`false`, et l'erreur remonte de `details/utils.h` et non du fichier de l'utilisateur.

## Les répertoires

| | contenu |
|---|---|
| `adversary/` | la chasse aux types que la bibliothèque **bénit** alors qu'ils autorisent une course réelle |
| `traits/` | `is_sendable`, `is_synchronizable<T>`, `is_synchronizable<const T>`, `is_lifetime_aware`, et la surface standard |
| `helpers/` | `copy_on_write`, `synchronized_value`, `asynchronous_task_launcher`, et les programmes de bout en bout |
| `diagnostics/` | les cas dont [04](../04-diagnostics.md) cite les messages ; presque tous sont rejetés par construction |
| `ergonomics/` | première heure d'utilisation, spécialisation d'un trait, extensibilité |
| `performance/` | les mesures de [06](../06-performance-compilation.md) et [07](../07-performance-execution.md) |

## Ce que le harnais ne rejoue pas

Trois familles de preuves demandent autre chose qu'une compilation, et les rapports
donnent à chaque fois la ligne de commande exacte :

- **Les programmes d'exécution** (`-O2 -pthread`, puis exécution) : interblocages,
  segfaults, mesures de débit.
- **ThreadSanitizer**, indisponible avec GCC sur cette machine (arm64 darwin, runtime
  absent : `Undefined symbols: ___tsan_func_entry`). Les courses sont donc prouvées
  sur une **extraction en C++ ordinaire** du corps d'exécution du helper, compilée
  avec Apple clang. La fidélité de chaque extraction est vérifiée ligne à ligne
  contre l'en-tête, et [09](../09-methodologie.md) explique la méthode et ses limites.
- **AddressSanitizer**, qui lui fonctionne avec GCC 16 ici, pour les
  use-after-free (`-fsanitize=address`).
