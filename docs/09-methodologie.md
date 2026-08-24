# Méthodologie — comment chaque mesure a été obtenue

Toutes les affirmations chiffrées de ces rapports proviennent de compilations et
d'exécutions réelles sur la machine de développement. Ce document décrit le
harnais, pour que chaque résultat soit reproductible.

## Environnement

| | |
|---|---|
| Compilateur | GCC 16.2.0 (Homebrew), `-std=c++26 -freflection` |
| Plateforme | macOS 25.6.0, arm64 (Apple Silicon), 12 cœurs |
| Bibliothèque standard | libstdc++ 16 |
| CMake | 4.x |

Dans les scripts ci-dessous, `<repo>` désigne la racine du dépôt
(`/Users/amorrier/Programmation/ThreadSafe` sur la machine de mesure).

## Les trois scripts du harnais

### Compilation seule — accepter ou rejeter

```bash
#!/bin/zsh
# tsc <file.cpp> [flags] -- exit 0 = accepté / tous les static_assert tiennent.
exec g++-16 -std=c++26 -freflection -fsyntax-only \
     -I<repo>/include "$@"
```

Pour prouver qu'un type est **rejeté**, deux formes équivalentes : soit
`static_assert(!trait_v<T>)` qui doit compiler, soit une utilisation qui doit
échouer, dont on lit le diagnostic.

Pour obtenir le message explicatif de la bibliothèque plutôt qu'un `false` nu, on
passe par les fonctions `assert_*` dans une fonction `consteval` :

```cpp
consteval bool ask() { threadsafe::assert_sendable<T>(); return true; }
static_assert(ask());
```

### Exécution — comportement et mesures

```bash
#!/bin/zsh
set -e
out="${1:t:r}.bin"
g++-16 -std=c++26 -freflection -O2 -pthread \
    -I<repo>/include "$@" -o "/tmp/$out"
"/tmp/$out"
```

### ThreadSanitizer — détection de courses

C'est le point technique le moins évident : **GCC sur macOS/arm64 ne fournit pas
de runtime TSan**. Compiler avec `-fsanitize=thread` produit des références
non résolues :

```
Undefined symbols for architecture arm64:
  "___tsan_func_entry", referenced from: ...
  "___tsan_init", referenced from: ...
```

Et Apple clang, qui a le runtime, ne sait pas compiler `-freflection`. La
bibliothèque semble donc hors de portée de TSan.

La solution retenue : compiler les objets avec `g++-16` (qui émet bien les appels
d'instrumentation `__tsan_*`) et **lier explicitement le runtime d'Apple clang**,
avec le `rpath` qui va bien :

```bash
#!/bin/zsh
set -e
TSANLIB=/Library/Developer/CommandLineTools/usr/lib/clang/21/lib/darwin/libclang_rt.tsan_osx_dynamic.dylib
out="${1:t:r}.tsan"
g++-16 -std=c++26 -freflection -O1 -g -pthread -fsanitize=thread \
    -I<repo>/include -c "$1" -o "/tmp/$out.o" ${@:2}
g++-16 -pthread "/tmp/$out.o" "$TSANLIB" \
    -Wl,-rpath,"$(dirname $TSANLIB)" -o "/tmp/$out"
TSAN_OPTIONS="halt_on_error=0" "/tmp/$out"
```

**Validé sur un témoin** : un programme à course évidente (deux threads
incrémentant une globale) est bien détecté à travers cette chaîne, donc
l'instrumentation fonctionne réellement sur du code compilé avec réflexion.

```
WARNING: ThreadSanitizer: data race (pid=14651)
SUMMARY: ThreadSanitizer: data race smoke_tsan.cpp:255 in ...
```

#### Limite connue et importante

```
warning: 'atomic_thread_fence' is not supported with '-fsanitize=thread' [-Wtsan]
```

La barrière `std::atomic_thread_fence(std::memory_order_acquire)` de
`copy_on_write::as_mutable()` est **invisible pour TSan**. Un run propre ne
constitue donc *pas* une preuve que cette barrière est correcte ; toute
affirmation à son sujet doit être argumentée sur le modèle mémoire.

## Test par mutation

Script : casser une règle à la fois dans une **copie** des en-têtes, recompiler
les 11 TU de la suite en parallèle, et noter si au moins une échoue.

```python
def build(inc):
    """Return the set of test files that FAILED to compile."""
    failed = set()
    procs = []
    for t in TESTS:
        procs.append((t, subprocess.Popen(
            ["g++-16","-std=c++26","-freflection","-fsyntax-only",
             "-I",str(inc),str(t)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)))
    for t, p in procs:
        if p.wait() != 0:
            failed.add(t.name)
    return failed
```

Un mutant qui ne fait échouer aucune TU est un trou de couverture. Le détail des
19 mutants et de leurs résultats est en [03-couverture-de-tests.md](./03-couverture-de-tests.md).

Pour les mutants qui touchent le comportement d'exécution, la vérification
compile **et exécute** la TU de test runtime, et considère un code de retour non
nul comme une détection.

## Mesures de temps de compilation

Meilleur de 3 exécutions, pour absorber le bruit du système de fichiers et du
cache :

```bash
best=99999
for i in 1 2 3; do
  t0=$(python3 -c 'import time;print(time.time())')
  g++-16 -std=c++26 -freflection -fsyntax-only -I"$hdr" "$file"
  t1=$(python3 -c 'import time;print(time.time())')
  d=$(python3 -c "print(round(($t1-$t0)*1000))")
  [ "$d" -lt "$best" ] && best=$d
done
```

Les comparaisons « avant / après » se font toujours sur le **même fichier
source**, en ne changeant que le `-I` qui désigne les en-têtes de référence ou les
en-têtes corrigés, afin qu'aucune autre variable ne bouge.

## Vérification de non-régression

Tout correctif proposé dans ces rapports a été appliqué à une copie des en-têtes,
puis les 11 TU de la suite existante ont été recompilées **sans modification**.
Un correctif qui casse un test existant est signalé comme tel — c'est le cas du
`synchronized_value` contraint, discuté en
[03-couverture-de-tests.md](./03-couverture-de-tests.md).

## Consommation CMake

Deux scénarios testés pour de bon, chacun jusqu'à l'exécution du binaire
consommateur :

- `add_subdirectory(<repo>)` puis `target_link_libraries(app PRIVATE ThreadSafe::threadsafe)` ;
- `find_package(ThreadSafe REQUIRED)` contre un préfixe d'installation réel,
  après `cmake --install`.
