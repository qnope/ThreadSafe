# Scénarios exécutés

Chaque affirmation des rapports de `docs/` s'appuie sur un de ces fichiers, qui a
été **réellement compilé et, quand il a un `main()`, réellement exécuté**.

## Organisation

| dossier | contenu |
|---|---|
| `traits/synchronizable/` | `is_synchronizable<T>` — forme non qualifiée |
| `traits/const-synchronizable/` | `is_synchronizable<const T>` — la marche structurelle |
| `traits/sendable/` | `is_sendable<T>` |
| `traits/lifetime-aware/` | `is_lifetime_aware<T>` |
| `helpers/copy-on-write/` | `copy_on_write<T>` |
| `helpers/synchronized-value-and-launcher/` | `synchronized_value<T>`, `value_guard`, `asynchronous_task_launcher` |
| `performance/` | mesures compilation et exécution |
| `ergonomics-and-simplicity/` | code côté utilisateur, lisibilité |
| `proposed-tests/` | les deux fichiers de test proposés (voir `docs/03-couverture-de-tests.md`) |
| `harness/` | les scripts, le test par mutation, et les sondes transverses |

## Rejouer

```bash
cd docs/scenarios/harness

./tsc   <fichier.cpp>   # compilation seule : exit 0 = accepté / assertions tenues
./tsrun <fichier.cpp>   # build -O2 et exécution
./tstsan <fichier.cpp>  # build et exécution sous ThreadSanitizer
```

Les scripts se localisent eux-mêmes à partir du dépôt ; `THREADSAFE_ROOT` et
`TSAN_RUNTIME` permettent de les surcharger. Voir `harness/HARNESS.md` et `docs/09-methodologie.md` pour le détail, notamment
pourquoi TSan demande un montage particulier sur macOS/arm64.

## Test par mutation

```bash
python3 harness/mutation_test.py
```

Casse une règle à la fois dans une copie des en-têtes et recompile toute la
suite. Résultats et analyse : `docs/03-couverture-de-tests.md`.

## Taille, et quoi garder

Le corpus complet pèse ~2,7 Mo pour 556 fichiers. Si c'est trop pour un dépôt
pédagogique, le sous-ensemble réellement **réutilisable** tient en quelques
dizaines de kilo-octets :

- `harness/tsc`, `harness/tsrun`, `harness/tstsan` — les trois scripts ;
- `harness/mutation_test.py` — le test par mutation, rejouable tel quel ;
- `harness/HARNESS.md` et ce README ;
- `proposed-tests/` — les deux fichiers de test proposés, qui compilent contre
  les en-têtes actuels.

Tout le reste est de la trace d'exploration : conservée pour que chaque
affirmation des rapports soit vérifiable, mais sans valeur une fois les rapports
lus. Le code retenu comme proposition est **intégralement recopié dans les
rapports**, donc rien n'est perdu en supprimant le corpus.

## Avertissement

Ce corpus est un **corpus d'exploration**, pas une suite de tests. **Environ la
moitié des fichiers ne compile pas, et c'est voulu.** Trois cas :

- des programmes volontairement incorrects, écrits pour observer le diagnostic
  que la bibliothèque produit ;
- des tentatives d'attaque qui ont échoué — c'est-à-dire des points où la
  bibliothèque a tenu, et où le rejet *est* le résultat ;
- les fichiers `*_fix_*`, qui vérifient un correctif proposé et ne compilent donc
  que contre des en-têtes patchés.

Un `tsc` qui échoue sur un de ces fichiers n'est pas une régression. Le code
retenu comme proposition figure intégralement dans les rapports et dans
`proposed-tests/`, et ces deux fichiers-là compilent contre les en-têtes actuels.
