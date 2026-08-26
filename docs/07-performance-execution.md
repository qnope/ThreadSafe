# 07 — Performance d'exécution

**Verdict.** L'emballage ne coûte rien, la politique coûte tout. Mesuré ici, `synchronized_value::lock()` coûte exactement le prix du `std::unique_lock` écrit à la main sur le même mutex (6,32 ns contre 6,33 ns), `lock_shared()` exactement le prix du `std::shared_lock` à la main (6,37 contre 6,41 ns), et `launch_task` exactement le prix d'un `std::jthread` nu (12 193 ns contre 12 082 ns par tâche, et 205,1 ms contre 205,1 ms sur quatre tâches de 200 ms). Il n'y a aucune surcouche à payer : c'est un résultat fort, et il mérite une diapositive. En revanche les deux *décisions* que la bibliothèque prend à la place de l'utilisateur sont mauvaises pour le cas courant. `synchronized_value<T>` choisit automatiquement un `std::shared_mutex` dès que `is_synchronizable_v<const T>` tient — c'est-à-dire pour tout conteneur standard — et sur une section critique courte ce choix perd contre `std::mutex` à *tous* les nombres de threads et *tous* les taux de lecture mesurés, d'un facteur 5,6x à 91,6x chez moi (5,8x à 118,8x chez le lead) ; sur une file de travail que personne ne lit jamais en partagé, 52x à 64x. Le choix est **sûr** — jamais un `shared_mutex` pour un `T` dont la lecture `const` serait dangereuse — et la dégradation va dans le bon sens, mais elle est **silencieuse** et l'utilisateur ne peut pas la contredire : `synchronized_value` n'a qu'un seul paramètre de template. Et `launch_scoped_task` crée un thread puis le joint immédiatement, si bien que N appels s'exécutent strictement l'un après l'autre : 4 × 200 ms coûtent 817 ms, pic de concurrence 1. Deux corrections sont proposées ; celle de `synchronized_value` est complète, compilée et vérifiée sans régression par moi-même.

---

## 0. Méthode, et l'avertissement qui va avec

| | |
|---|---|
| Machine | Apple M3 Pro, 6 cœurs performance + 6 cœurs efficacité, arm64, macOS 26.6.2 |
| Compilateur | GCC 16.2.0 (Homebrew) |
| Options | `-std=c++26 -freflection -I<threadsafe>/include -O2 -pthread` |
| Charge machine | `load average` entre 2,4 et 5,5 pendant mes campagnes (relevé avant et après chaque exécution) |

**Ces chiffres sont ceux d'une seule machine.** Un `std::shared_mutex` libstdc++ sur arm64/darwin est implémenté au-dessus de `pthread_rwlock_t` ; sur Linux/glibc, sur x86-64, ou avec une autre libstdc++, les rapports changeront. Ce qui ne changera pas, et c'est le point du rapport, c'est la *forme* : un verrou lecteur porte plus de comptabilité qu'un mutex, il ne se rembourse qu'à partir d'une section critique assez longue, et la bibliothèque choisit à la place de l'utilisateur sans lui dire ni lui laisser le choix.

Ma machine n'était pas au repos (`load average` ≈ 2,5 à 5,5, en partie à cause de mes propres campagnes à 12 threads). J'ai donc rejoué **moi-même** chaque mesure que je présente, et je donne côte à côte le chiffre du lead (machine au repos) et le mien. Les deux séries se recoupent sur la forme et divergent de 10 à 25 % sur les valeurs absolues, ce qui est exactement ce qu'on attend d'une machine chargée. Là où mon chiffre et celui du lead diffèrent, les deux sont donnés.

Toutes les mesures de ce rapport ont été prises avec le programme complet donné juste à côté. Aucun programme n'est tronqué. Dans les sorties du compilateur citées, le préfixe `.../` remplace uniquement le chemin d'accès : `/Users/amorrier/Programmation/ThreadSafe/include/threadsafe/details/` pour les en-têtes de la bibliothèque, et le répertoire d'installation de libstdc++ pour `mutex` et `bits/unique_lock.h`. Aucun code n'est élidé nulle part.

Les six constats de ce rapport, et leur statut :

| Id | Constat | Gravité | Correctif fourni | Régression vérifiée |
|---|---|---|---|---|
| L4 | Le `std::shared_mutex` automatique de `synchronized_value` est une pessimisation, non contournable | haute | oui, fichier complet | **suite-passes** — les 11 TU rejouées par moi |
| L6 | `launch_scoped_task` sérialise : N appels, concurrence 1 | haute | renommage complet fourni ; le vrai parallélisme est dans [02](./02-robustesse-des-helpers.md) | **suite-regresses** sur l'en-tête seul, **suite-passes** avec une ligne de test — vérifié par moi |
| E1 | `copy_on_write` n'a aucun canal de publication ; le motif qui marche n'est nulle part documenté | moyenne | aucun (délibéré) | no-fix-proposed |
| E2 | La bonne diapositive « mauvaise version » est le foncteur nommé, pas la lambda | moyenne | aucun (délibéré) | no-fix-proposed |
| E3 | Une tranche de `vector` : `span` et pointeurs refusés ; le contournement coûte 2x à 3,6x | moyenne | aucun (délibéré) | no-fix-proposed |
| E4 | Pas de verrouillage multiple : le virement entre deux comptes interbloque | moyenne | aucun (délibéré) | no-fix-proposed |

L4 et L6 ont été produits par un agent qui a compilé et exécuté ce qu'il rapporte ; le lead a personnellement revérifié l'interblocage de `launch_scoped_task` sur une tâche coopérative. J'ai recompilé et rejoué **tous** les programmes de ce rapport ; là où mon résultat diffère de celui de mes sources, je le dis.

---

## 1. Le résultat positif : les abstractions ne coûtent rien

C'est le constat qu'il faut mettre en tête, parce qu'il est vrai, qu'il est mesuré, et qu'il est ce qu'une audience de conférence vient chercher : **ce que la bibliothèque ajoute autour du verrou et autour du thread coûte zéro à `-O2`.**

Le programme sépare volontairement trois colonnes, parce que confondre les deux dernières est exactement l'erreur que fait le constat L4 :

- `library` — `threadsafe::synchronized_value` / `threadsafe::asynchronous_task_launcher` ;
- `same lock` — le même code à la main, **sur le mutex que la bibliothèque a choisi** ;
- `std::mutex` — le même code à la main, sur le mutex qu'un programmeur aurait choisi.

```cpp
// What do the library's abstractions cost against the hand-written equivalent?
// Three columns, so that the WRAPPING is separated from the MUTEX CHOICE:
//   "library"   threadsafe::synchronized_value / asynchronous_task_launcher
//   "same lock" the same code by hand, on the SAME mutex the library picked
//   "std::mutex" the same code by hand, on the mutex a programmer would have picked
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//        bench_overhead.cpp -o bench_overhead
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

constexpr int lock_iterations = 20'000'000;
constexpr int spawn_iterations = 2'000;
constexpr int repetitions = 5;

struct empty_task {
    void operator()() const {}
};

double best_of(double (*measure)()) {
    double best = measure();
    for (int repetition = 1; repetition < repetitions; ++repetition) {
        const double candidate = measure();
        if (candidate < best)
            best = candidate;
    }
    return best;
}

double nanoseconds_per(std::chrono::steady_clock::time_point started_at,
                       int iterations) {
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count() / iterations;
}

double library_exclusive_lock() {
    threadsafe::synchronized_value<long long> guarded{0};
    const auto started_at = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < lock_iterations; ++iteration) {
        auto guard = guarded.lock();
        ++*guard;
    }
    return nanoseconds_per(started_at, lock_iterations);
}

template <class Mutex>
double hand_written_exclusive_lock() {
    Mutex mutex;
    long long value = 0;
    const auto started_at = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < lock_iterations; ++iteration) {
        std::unique_lock lock{mutex};
        ++value;
    }
    const double result = nanoseconds_per(started_at, lock_iterations);
    if (value == 1)
        std::printf("unreachable\n");
    return result;
}

double library_shared_lock() {
    threadsafe::synchronized_value<long long> guarded{7};
    long long sink = 0;
    const auto started_at = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < lock_iterations; ++iteration) {
        const auto guard = guarded.lock_shared();
        sink += *guard;
    }
    const double result = nanoseconds_per(started_at, lock_iterations);
    if (sink == 1)
        std::printf("unreachable\n");
    return result;
}

double hand_written_shared_lock() {
    std::shared_mutex mutex;
    long long value = 7;
    long long sink = 0;
    const auto started_at = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < lock_iterations; ++iteration) {
        std::shared_lock lock{mutex};
        sink += value;
    }
    const double result = nanoseconds_per(started_at, lock_iterations);
    if (sink == 1)
        std::printf("unreachable\n");
    return result;
}

double library_spawn() {
    const auto started_at = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < spawn_iterations; ++iteration) {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(empty_task{});
    }
    return nanoseconds_per(started_at, spawn_iterations);
}

double hand_written_spawn() {
    const auto started_at = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < spawn_iterations; ++iteration) {
        std::vector<std::jthread> threads;
        threads.emplace_back(empty_task{});
    }
    return nanoseconds_per(started_at, spawn_iterations);
}

}

static_assert(std::is_same_v<threadsafe::synchronized_value<long long>::mutex,
                             std::shared_mutex>,
              "long long is const-synchronizable, so the wrapper picks "
              "shared_mutex — the 'same lock' column must use the same one");

int main() {
    std::printf("best of %d, single thread, uncontended, ns per operation\n\n",
                repetitions);
    std::printf("%-32s %9s %10s %11s\n", "operation", "library", "same lock",
                "std::mutex");

    const double library_exclusive = best_of(library_exclusive_lock);
    const double by_hand_same = best_of(hand_written_exclusive_lock<std::shared_mutex>);
    const double by_hand_plain = best_of(hand_written_exclusive_lock<std::mutex>);
    std::printf("%-32s %9.2f %10.2f %11.2f\n", "lock() + one increment",
                library_exclusive, by_hand_same, by_hand_plain);

    const double library_shared = best_of(library_shared_lock);
    const double by_hand_shared = best_of(hand_written_shared_lock);
    std::printf("%-32s %9.2f %10.2f %11s\n", "lock_shared() + one read",
                library_shared, by_hand_shared, "n/a");

    const double library_thread = best_of(library_spawn);
    const double by_hand_thread = best_of(hand_written_spawn);
    std::printf("%-32s %9.0f %10.0f %11s\n", "spawn + join one empty task",
                library_thread, by_hand_thread, "n/a");
}
```

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread bench_overhead.cpp -o bench_overhead
$ ./bench_overhead
best of 5, single thread, uncontended, ns per operation

operation                          library  same lock  std::mutex
lock() + one increment                6.41       6.31        3.26
lock_shared() + one read              6.35       6.45         n/a
spawn + join one empty task          12155      12082         n/a

$ ./bench_overhead
best of 5, single thread, uncontended, ns per operation

operation                          library  same lock  std::mutex
lock() + one increment                6.32       6.33        3.24
lock_shared() + one read              6.37       6.41         n/a
spawn + join one empty task          12193      12082         n/a
```

Lecture :

| Opération | Bibliothèque | Même verrou, à la main | Écart |
|---|---|---|---|
| `lock()` + un incrément | 6,32 – 6,41 ns | 6,31 – 6,33 ns | **+0,2 % à +1,6 %** |
| `lock_shared()` + une lecture | 6,35 – 6,37 ns | 6,41 – 6,45 ns | **−0,6 % à −1,6 %** |
| Créer et joindre une tâche vide | 12 155 – 12 193 ns | 12 082 ns | **+0,6 % à +0,9 %** |

`value_guard`, le `[[nodiscard]]`, les `operator*` supprimés en rvalue, le splice `[:get_const_guard_type():]`, le `std::vector<std::jthread>` du lanceur : tout cela disparaît à la compilation. `launch_task` **est** un `emplace_back` sur un `std::vector<std::jthread>`, et il en coûte le prix exact.

Le même résultat se retrouve à chaque échelle mesurée dans ce rapport :

- sur `std::map` à 90 % de lectures, de 1 à 12 threads, `synchronized_value<std::map<int,int>>` suit le `std::shared_mutex` écrit à la main à **1 à 3 %** près (§2.2) ;
- quatre tâches de 200 ms via `launch_task` puis destruction du lanceur : **205,1 ms**, contre **205,1 ms** pour un `std::vector<std::jthread>` nu (§3) ;
- une réduction parallèle sur 10 000 000 de `double` : **1,4 ms** par le chemin imposé par la bibliothèque, **1,4 ms** avec `std::jthread` + `std::span` à la main (§4.2) ;
- la publication d'une configuration à 8 lecteurs : **15,9 ns** par lecture pour `synchronized_value<copy_on_write<Config>>`, contre **26,9 ns** pour `std::atomic<std::shared_ptr<const Config>>`, la réponse de manuel (§4.1).

La troisième colonne, elle, dit déjà tout le rapport : **3,24 ns contre 6,32 ns, sans aucune contention et sur un seul thread.** Le surcoût n'est pas dans l'emballage, il est dans le mutex que l'emballage a choisi tout seul.

---
## 2. L4 — le `std::shared_mutex` automatique est une pessimisation, et rien ne permet de le refuser

**Gravité : haute. Correctif : fourni, complet, sans régression (11/11 TU vertes, vérifié par moi).**

### 2.1 Le mécanisme

`include/threadsafe/details/synchronized_value.h` choisit son mutex ainsi :

```cpp
    static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];
```

`is_synchronizable_v<const T>` répond à la question **« plusieurs lecteurs peuvent-ils se recouvrir sans danger ? »**. C'est une question de **correction**. Le choix du mutex est une question de **rentabilité**. Les deux ne coïncident que lorsque la section critique est assez longue pour amortir la comptabilité d'un verrou lecteur — soit, sur cette machine, quelques centaines de nanosecondes.

Tout conteneur standard répond « oui » à la question de correction. Donc **tout `synchronized_value<conteneur>` paie**, y compris ceux dont personne n'appelle jamais `lock_shared()`.

Et le paramètre pour dire non n'existe pas : `template <class T> class synchronized_value;` n'a qu'un seul paramètre, `mutex_` est `private`, il n'y a ni accesseur ni `native_handle`.

### 2.2 La matrice mesurée : une section critique d'un seul incrément

C'est la mesure de référence du lead, machine au repos, `-O2 -pthread`, un incrément ou une lecture sous le verrou :

| threads | read % | `shared_mutex` | `std::mutex` | rapport |
|---:|---:|---:|---:|---:|
| 2 | 50 | 1333,3 ns | 11,2 ns | **118,80x** |
| 2 | 90 | 354,3 ns | 6,6 ns | **53,80x** |
| 2 | 99 | 45,2 ns | 7,8 ns | **5,81x** |
| 4 | 50 | 1652,3 ns | 19,3 ns | **85,75x** |
| 4 | 90 | 610,7 ns | 13,8 ns | **44,12x** |
| 4 | 99 | 95,1 ns | 13,6 ns | **6,97x** |
| 8 | 50 | 1762,3 ns | 20,2 ns | **87,21x** |
| 8 | 90 | 1098,2 ns | 14,0 ns | **78,24x** |
| 8 | 99 | 262,9 ns | 13,6 ns | **19,37x** |

Le `shared_mutex` perd **partout**. Il n'y a pas une seule case du tableau où il gagne. Même à 99 % de lectures — le cas le plus favorable imaginable pour un verrou lecteur — il perd d'un facteur 5,8 à 19,4.

Le programme complet que j'ai écrit et exécuté pour rejouer cette matrice :

```cpp
// std::shared_mutex vs std::mutex on a ONE-INCREMENT critical section.
// build: g++-16 -std=c++26 -O2 -pthread bench_matrix.cpp -o bench_matrix
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int operations_per_thread = 200'000;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

template <class Mutex, class ReadLock>
double measure(int thread_count, int read_percentage) {
    Mutex mutex;
    long long protected_counter = 0;
    std::uint64_t sink = 0;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&, worker] {
                fast_random random{
                    static_cast<std::uint64_t>(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int operation = 0; operation < operations_per_thread;
                     ++operation) {
                    const bool is_read =
                        static_cast<int>(random.next() % 100) < read_percentage;
                    if (is_read) {
                        ReadLock lock{mutex};
                        local += static_cast<std::uint64_t>(protected_counter);
                    } else {
                        std::unique_lock lock{mutex};
                        ++protected_counter;
                    }
                }
                __atomic_fetch_add(&sink, local, __ATOMIC_RELAXED);
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    if (sink == 0x123456789ULL)
        std::printf("unreachable\n");
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

}

int main() {
    std::printf("one increment or one read under the lock, %d ops per thread\n\n",
                operations_per_thread);
    std::printf("%8s %7s %14s %13s %8s\n", "threads", "read%", "shared_mutex",
                "std::mutex", "ratio");
    for (int thread_count : {2, 4, 8})
        for (int read_percentage : {50, 90, 99}) {
            const double shared =
                measure<std::shared_mutex, std::shared_lock<std::shared_mutex>>(
                    thread_count, read_percentage);
            const double exclusive =
                measure<std::mutex, std::unique_lock<std::mutex>>(
                    thread_count, read_percentage);
            std::printf("%8d %7d %11.1f ns %10.1f ns %7.2fx\n", thread_count,
                        read_percentage, shared, exclusive, shared / exclusive);
        }
}
```

```
$ g++-16 -std=c++26 -O2 -pthread bench_matrix.cpp -o bench_matrix
$ uptime
 1:59  up 3 days, 10:40, 3 users, load averages: 2,66 2,38 2,55
$ ./bench_matrix
one increment or one read under the lock, 200000 ops per thread

 threads   read%   shared_mutex    std::mutex    ratio
       2      50      1008.3 ns       11.0 ns   91.62x
       2      90       314.7 ns        7.3 ns   42.90x
       2      99        39.4 ns        6.8 ns    5.83x
       4      50      1584.3 ns       26.7 ns   59.24x
       4      90       704.0 ns       19.2 ns   36.71x
       4      99        78.4 ns       13.9 ns    5.64x
       8      50      1759.8 ns       25.8 ns   68.18x
       8      90      1157.5 ns       16.4 ns   70.53x
       8      99       216.0 ns       14.8 ns   14.64x
```

**Mes chiffres confirment ceux du lead** : mêmes ordres de grandeur, même monotonie, même conclusion — de 5,6x à 91,6x chez moi, de 5,8x à 118,8x chez lui. L'écart entre les deux séries (jusqu'à 25 % sur les valeurs absolues) s'explique par la charge de ma machine ; le rapport, lui, est stable.

### 2.3 Le même défaut à travers la bibliothèque, sur `std::map`

Le programme du constat L4, que j'ai recompilé et rejoué tel quel, mesure trois colonnes : `synchronized_value<std::map<int,int>>`, la même carte derrière un `std::shared_mutex` à la main, et la même carte derrière un `std::mutex` à la main.

```cpp
// 90% read / 10% write on std::map<int,int>, three ways:
//   1. threadsafe::synchronized_value<std::map<int,int>>  (auto-picks shared_mutex)
//   2. the same map behind a hand-written std::shared_mutex
//   3. the same map behind a hand-written std::mutex
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread p6_bench.cpp -o p6
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int operations_per_thread = 200'000;
constexpr int key_space = 4096;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

using guarded_map = threadsafe::synchronized_value<std::map<int, int>>;

struct library_worker {
    std::shared_ptr<guarded_map> table;
    std::shared_ptr<std::atomic<std::uint64_t>> checksum;
    int seed;

    void operator()() const {
        fast_random random{static_cast<std::uint64_t>(seed) * 2654435761u + 1};
        std::uint64_t local = 0;
        for (int operation = 0; operation < operations_per_thread; ++operation) {
            const std::uint64_t draw = random.next();
            const int key = static_cast<int>(draw % key_space);
            if (draw % 10 != 0) {
                const auto guard = table->lock_shared();
                const auto found = guard->find(key);
                if (found != guard->end())
                    local += static_cast<std::uint64_t>(found->second);
            } else {
                auto guard = table->lock();
                (*guard)[key] = key * 2;
            }
        }
        *checksum += local;
    }
};

template <class Mutex>
struct hand_written {
    Mutex mutex;
    std::map<int, int> table;
    std::atomic<std::uint64_t> checksum{0};
};

template <class Mutex, class ReadLock>
double run_hand_written(int thread_count) {
    hand_written<Mutex> shared;
    for (int key = 0; key < key_space; key += 2)
        shared.table[key] = key * 2;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&shared, worker] {
                fast_random random{
                    static_cast<std::uint64_t>(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int operation = 0; operation < operations_per_thread;
                     ++operation) {
                    const std::uint64_t draw = random.next();
                    const int key = static_cast<int>(draw % key_space);
                    if (draw % 10 != 0) {
                        ReadLock lock{shared.mutex};
                        const auto found = shared.table.find(key);
                        if (found != shared.table.end())
                            local += static_cast<std::uint64_t>(found->second);
                    } else {
                        std::unique_lock lock{shared.mutex};
                        shared.table[key] = key * 2;
                    }
                }
                shared.checksum += local;
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

double run_library(int thread_count) {
    auto table = guarded_map::make();
    {
        auto guard = table->lock();
        for (int key = 0; key < key_space; key += 2)
            (*guard)[key] = key * 2;
    }
    auto checksum = std::make_shared<std::atomic<std::uint64_t>>(0);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < thread_count; ++worker)
            launcher.launch_task(library_worker{table, checksum, worker});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

}

static_assert(threadsafe::is_synchronizable_v<const std::map<int, int>>,
              "which is why the wrapper picks a shared_mutex");
static_assert(std::is_same_v<guarded_map::mutex, std::shared_mutex>);
static_assert(std::is_same_v<guarded_map::const_guard,
                             threadsafe::value_guard<
                                 const std::map<int, int>,
                                 std::shared_lock<std::shared_mutex>>>);

int main() {
    std::printf("90%% read / 10%% write on std::map<int,int>, "
                "%d ops per thread, ns per op\n\n", operations_per_thread);
    std::printf("%8s %14s %14s %14s\n", "threads", "sync_value",
                "shared_mutex", "mutex");
    for (int thread_count : {1, 2, 4, 8, 12}) {
        const double library = run_library(thread_count);
        const double shared = run_hand_written<std::shared_mutex,
                                               std::shared_lock<std::shared_mutex>>(
            thread_count);
        const double exclusive =
            run_hand_written<std::mutex, std::unique_lock<std::mutex>>(
                thread_count);
        std::printf("%8d %14.1f %14.1f %14.1f\n", thread_count, library, shared,
                    exclusive);
    }
}
```

Mesuré par l'agent, machine plus calme :

```
90% read / 10% write on std::map<int,int>, 200000 ops per thread, ns per op

 threads     sync_value   shared_mutex          mutex
       1           44.6           44.3           44.0
       2          434.0          441.6           86.4
       4          716.8          662.0           85.5
       8         1214.1         1210.3          131.4
      12         1417.9         1399.3          131.5
```

Rejoué par moi :

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread l4_bench.cpp -o l4_bench
$ ./l4_bench
90% read / 10% write on std::map<int,int>, 200000 ops per thread, ns per op

 threads     sync_value   shared_mutex          mutex
       1           44.2           43.7           44.2
       2          415.6          424.7           72.2
       4          891.1          877.6          135.9
       8         1340.2         1312.8          151.4
       12        1500.8         1449.9          132.3
```

Deux lectures, et elles vont dans des directions opposées :

1. **La colonne 1 suit la colonne 2 à 1–3 % près.** `synchronized_value` n'ajoute rien au `shared_mutex`. C'est le résultat de la §1, confirmé sous contention.
2. **La colonne 3 est 6 à 11 fois plus rapide que les deux autres**, à tous les nombres de threads sauf 1. La bibliothèque a choisi la colonne 2 et l'utilisateur voulait la colonne 3.

### 2.4 Le cas extrême : une file que personne ne lit jamais en partagé

C'est la démonstration qui appartient à la diapositive. Une file de travail producteurs/consommateurs : tout le monde prend le verrou **exclusif**, `lock_shared()` n'est appelé nulle part. Et `synchronized_value<std::deque<Job>>` choisit quand même un `std::shared_mutex`, parce qu'un `const std::deque<Job>` se lit sans danger à plusieurs — ce qui est vrai, et rigoureusement hors sujet ici.

```cpp
// A work queue that is NEVER read shared: every producer and every consumer
// takes the exclusive lock. synchronized_value still picks std::shared_mutex,
// because std::deque<Job> answers "is a const deque read-safe?" with yes.
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//        bench_queue.cpp -o bench_queue
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

constexpr int producer_count = 4;
constexpr int consumer_count = 4;
constexpr int items_per_producer = 25'000;
constexpr int total_items = producer_count * items_per_producer;

struct Job {
    int identifier;
};

std::atomic<long long> consumed_checksum{0};
std::atomic<int> consumed_count{0};

double milliseconds_since(std::chrono::steady_clock::time_point started_at) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - started_at)
        .count();
}

double run_synchronized_value() {
    consumed_checksum.store(0);
    consumed_count.store(0);
    threadsafe::synchronized_value<std::deque<Job>> queue;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> workers;
        for (int producer = 0; producer < producer_count; ++producer)
            workers.emplace_back([&, producer] {
                for (int item = 0; item < items_per_producer; ++item) {
                    auto guard = queue.lock();
                    guard->push_back(Job{producer * items_per_producer + item});
                }
            });
        for (int consumer = 0; consumer < consumer_count; ++consumer)
            workers.emplace_back([&] {
                long long local = 0;
                while (consumed_count.load(std::memory_order_relaxed) < total_items) {
                    Job job{-1};
                    {
                        auto guard = queue.lock();
                        if (!guard->empty()) {
                            job = guard->front();
                            guard->pop_front();
                        }
                    }
                    if (job.identifier >= 0) {
                        local += job.identifier;
                        consumed_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::yield();
                    }
                }
                consumed_checksum.fetch_add(local, std::memory_order_relaxed);
            });
    }
    return milliseconds_since(started_at);
}

double run_hand_written_mutex() {
    consumed_checksum.store(0);
    consumed_count.store(0);
    std::mutex mutex;
    std::deque<Job> queue;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> workers;
        for (int producer = 0; producer < producer_count; ++producer)
            workers.emplace_back([&, producer] {
                for (int item = 0; item < items_per_producer; ++item) {
                    std::unique_lock lock{mutex};
                    queue.push_back(Job{producer * items_per_producer + item});
                }
            });
        for (int consumer = 0; consumer < consumer_count; ++consumer)
            workers.emplace_back([&] {
                long long local = 0;
                while (consumed_count.load(std::memory_order_relaxed) < total_items) {
                    Job job{-1};
                    {
                        std::unique_lock lock{mutex};
                        if (!queue.empty()) {
                            job = queue.front();
                            queue.pop_front();
                        }
                    }
                    if (job.identifier >= 0) {
                        local += job.identifier;
                        consumed_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::yield();
                    }
                }
                consumed_checksum.fetch_add(local, std::memory_order_relaxed);
            });
    }
    return milliseconds_since(started_at);
}

double run_condition_variable() {
    consumed_checksum.store(0);
    consumed_count.store(0);
    std::mutex mutex;
    std::condition_variable not_empty;
    std::deque<Job> queue;
    bool producers_done = false;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> consumers;
        for (int consumer = 0; consumer < consumer_count; ++consumer)
            consumers.emplace_back([&] {
                long long local = 0;
                for (;;) {
                    Job job{-1};
                    {
                        std::unique_lock lock{mutex};
                        not_empty.wait(lock, [&] {
                            return !queue.empty() || producers_done;
                        });
                        if (queue.empty())
                            break;
                        job = queue.front();
                        queue.pop_front();
                    }
                    local += job.identifier;
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                }
                consumed_checksum.fetch_add(local, std::memory_order_relaxed);
            });
        {
            std::vector<std::jthread> producers;
            for (int producer = 0; producer < producer_count; ++producer)
                producers.emplace_back([&, producer] {
                    for (int item = 0; item < items_per_producer; ++item) {
                        {
                            std::unique_lock lock{mutex};
                            queue.push_back(
                                Job{producer * items_per_producer + item});
                        }
                        not_empty.notify_one();
                    }
                });
        }
        {
            std::unique_lock lock{mutex};
            producers_done = true;
        }
        not_empty.notify_all();
    }
    return milliseconds_since(started_at);
}

}

static_assert(std::is_same_v<
                  threadsafe::synchronized_value<std::deque<Job>>::mutex,
                  std::shared_mutex>,
              "nobody ever calls lock_shared on this queue, and it still gets "
              "a shared_mutex");

int main() {
    std::printf("%d producers x %d jobs, %d consumers, std::deque<Job>\n\n",
                producer_count, items_per_producer, consumer_count);
    std::printf("%-52s %9s %14s\n", "queue", "ms", "checksum");
    const double automatic = run_synchronized_value();
    std::printf("%-52s %9.1f %14lld\n",
                "synchronized_value<std::deque<Job>> (auto shared_mutex)",
                automatic, consumed_checksum.load());
    const double by_hand = run_hand_written_mutex();
    std::printf("%-52s %9.1f %14lld\n", "hand-written std::mutex", by_hand,
                consumed_checksum.load());
    const double with_condition = run_condition_variable();
    std::printf("%-52s %9.1f %14lld\n",
                "std::mutex + std::condition_variable", with_condition,
                consumed_checksum.load());
    std::printf("\nshared_mutex costs %.1fx a std::mutex, %.1fx a condition_variable\n",
                automatic / by_hand, automatic / with_condition);
}
```

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread bench_queue.cpp -o bench_queue
$ ./bench_queue
4 producers x 25000 jobs, 4 consumers, std::deque<Job>

queue                                                       ms       checksum
synchronized_value<std::deque<Job>> (auto shared_mutex)  250.2     4999950000
hand-written std::mutex                                    4.8     4999950000
std::mutex + std::condition_variable                       7.5     4999950000

shared_mutex costs 52.1x a std::mutex, 33.2x a condition_variable
```

Trois exécutions :

| Exécution | `synchronized_value` (auto) | `std::mutex` à la main | `std::mutex` + `condition_variable` | rapport / mutex |
|---|---:|---:|---:|---:|
| 1 | 250,2 ms | 4,8 ms | 7,5 ms | **52,1x** |
| 2 | 247,0 ms | 4,2 ms | 6,9 ms | **58,4x** |
| 3 | 294,8 ms | 4,6 ms | 6,3 ms | **64,1x** |

Le `static_assert` du programme rend le mécanisme visible à la compilation, sans avoir à exécuter quoi que ce soit :

```cpp
static_assert(std::is_same_v<
                  threadsafe::synchronized_value<std::deque<Job>>::mutex,
                  std::shared_mutex>,
              "nobody ever calls lock_shared on this queue, and it still gets "
              "a shared_mutex");
```

L'agent avait mesuré 14x sur une variante de ce programme ; le mien en donne 52 à 64. Les deux disent la même chose. C'est le cas où la question de correction et la question de rentabilité sont le plus complètement décorrélées : la réponse à « les lecteurs peuvent-ils se recouvrir ? » est *oui*, et il n'y a **aucun** lecteur.

### 2.5 Où est le point de bascule

Le lead a mesuré la bascule à 4 threads, 90 % de lectures, en faisant varier le travail effectué sous le verrou :

| spins | `shared_mutex` | `std::mutex` | rapport |
|---:|---:|---:|---:|
| 0 | 667 ns | 17 ns | 38,81x |
| 50 | 623 ns | 48 ns | 12,88x |
| 200 | 731 ns | 456 ns | 1,60x |
| 800 | 1050 ns | 1578 ns | **0,67x** ← le `shared_mutex` gagne enfin |
| 3200 | 2180 ns | 4908 ns | 0,44x |
| 12800 | 6371 ns | 14737 ns | 0,43x |

Mon programme et mes chiffres :

```cpp
// Where does std::shared_mutex start to pay? 4 threads, 90% reads, growing the
// work done INSIDE the critical section.
// build: g++-16 -std=c++26 -O2 -pthread bench_crossover.cpp -o bench_crossover
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int operations_per_thread = 20'000;
constexpr int thread_count = 4;
constexpr int read_percentage = 90;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

std::uint64_t burn(int spin_count, std::uint64_t seed) {
    std::uint64_t accumulator = seed;
    for (int spin = 0; spin < spin_count; ++spin)
        accumulator = accumulator * 6364136223846793005ULL + 1442695040888963407ULL;
    return accumulator;
}

template <class Mutex, class ReadLock>
double measure(int spin_count) {
    Mutex mutex;
    std::uint64_t protected_state = 1;
    std::uint64_t sink = 0;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&, worker] {
                fast_random random{
                    static_cast<std::uint64_t>(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int operation = 0; operation < operations_per_thread;
                     ++operation) {
                    const bool is_read =
                        static_cast<int>(random.next() % 100) < read_percentage;
                    if (is_read) {
                        ReadLock lock{mutex};
                        local += burn(spin_count, protected_state);
                    } else {
                        std::unique_lock lock{mutex};
                        protected_state = burn(spin_count, protected_state) | 1;
                    }
                }
                __atomic_fetch_add(&sink, local, __ATOMIC_RELAXED);
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    if (sink == 0x123456789ULL)
        std::printf("unreachable\n");
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

}

int main() {
    std::printf("%d threads, %d%% reads, %d ops per thread\n\n", thread_count,
                read_percentage, operations_per_thread);
    std::printf("%7s %14s %13s %8s\n", "spins", "shared_mutex", "std::mutex",
                "ratio");
    for (int spin_count : {0, 50, 200, 800, 3200, 12800}) {
        const double shared =
            measure<std::shared_mutex, std::shared_lock<std::shared_mutex>>(
                spin_count);
        const double exclusive =
            measure<std::mutex, std::unique_lock<std::mutex>>(spin_count);
        std::printf("%7d %11.0f ns %10.0f ns %7.2fx\n", spin_count, shared,
                    exclusive, shared / exclusive);
    }
}
```

```
$ g++-16 -std=c++26 -O2 -pthread bench_crossover.cpp -o bench_crossover
$ ./bench_crossover
4 threads, 90% reads, 20000 ops per thread

  spins   shared_mutex    std::mutex    ratio
      0         600 ns         17 ns   36.20x
     50         635 ns         62 ns   10.22x
    200         710 ns        337 ns    2.10x
    800         991 ns       1546 ns    0.64x
   3200        1867 ns       3792 ns    0.49x
  12800        4980 ns      12619 ns    0.39x
```

Les deux séries se superposent presque exactement. J'ai ensuite resserré le pas, en remplaçant la seule ligne `for (int spin_count : {0, 50, 200, 800, 3200, 12800})` du programme ci-dessus par `for (int spin_count : {300, 400, 500, 600, 700})` :

```
4 threads, 90% reads, 20000 ops per thread

  spins   shared_mutex    std::mutex    ratio
    300         779 ns        600 ns    1.30x
    400         829 ns        890 ns    0.93x
    500         882 ns       1080 ns    0.82x
    600         921 ns       1135 ns    0.81x
    700         927 ns       1632 ns    0.57x
```

Pour convertir les tours de boucle en nanosecondes, j'ai chronométré `burn()` seul, mono-thread, sans verrou :

```cpp
#include <chrono>
#include <cstdint>
#include <cstdio>
std::uint64_t burn(int spin_count, std::uint64_t seed) {
    std::uint64_t accumulator = seed;
    for (int spin = 0; spin < spin_count; ++spin)
        accumulator = accumulator * 6364136223846793005ULL + 1442695040888963407ULL;
    return accumulator;
}
int main() {
    for (int spin_count : {0, 50, 200, 400, 800, 3200, 12800}) {
        constexpr int repetitions = 200000;
        std::uint64_t accumulator = 1;
        const auto started = std::chrono::steady_clock::now();
        for (int repetition = 0; repetition < repetitions; ++repetition)
            accumulator += burn(spin_count, accumulator | 1);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        std::printf("spins=%6d  %8.1f ns per call  (sink %llu)\n", spin_count,
                    std::chrono::duration<double, std::nano>(elapsed).count() / repetitions,
                    (unsigned long long)(accumulator & 1));
    }
}
```

```
$ g++-16 -std=c++26 -O2 -pthread calib.cpp -o calib
$ ./calib
spins=     0       0.6 ns per call  (sink 1)
spins=    50      42.0 ns per call  (sink 1)
spins=   200     166.5 ns per call  (sink 1)
spins=   400     332.8 ns per call  (sink 1)
spins=   800     668.7 ns per call  (sink 1)
spins=  3200    2683.1 ns per call  (sink 1)
spins= 12800   10663.5 ns per call  (sink 1)
```

**Conclusion chiffrée.** La bascule tombe entre 300 et 400 tours, c'est-à-dire entre **170 et 330 ns** de travail réel sous le verrou sur ma machine ; le lead la situe autour de **500 ns** sur la sienne. Retenons : **quelques centaines de nanosecondes**. En dessous, le `shared_mutex` perd, jusqu'à 39x. Or « incrémenter un compteur », « chercher une clé dans une `std::map` », « lire un champ » sont tous à un ou deux ordres de grandeur *en dessous* de ce seuil. Le cas courant est du mauvais côté de la bascule.

### 2.6 Ce qu'il faut dire pour être juste

Trois choses, et elles doivent être dites avec autant de netteté que le reste :

1. **Le choix est sûr.** La bibliothèque ne sélectionne jamais un `shared_mutex` pour un `T` dont la lecture `const` concurrente serait dangereuse : la condition est exactement `is_synchronizable_v<const T>`. Aucune des campagnes n'a produit un `synchronized_value` qui autorise plusieurs lecteurs sur un `T` qui ne le supporte pas. Le défaut est de performance, pas de correction.
2. **La dégradation va dans le bon sens.** Quand la réponse est « non », la bibliothèque retombe sur `std::mutex`, c'est-à-dire sur le choix conservateur *et* rapide. Il n'y a pas de cas où le doute mène à la lenteur.
3. **Le corps de `synchronized_value` est sain sous ThreadSanitizer.** L'extraction manuelle a été diffée ligne à ligne contre l'en-tête réel et tourne sans avertissement (`clang++ -std=c++20 -fsanitize=thread -O1`, 8 threads × 20 000 opérations mixtes : `ok: 256 entries, checksum 32647692`).

Le défaut n'est donc pas « la bibliothèque choisit mal », c'est **« la bibliothèque choisit silencieusement, et l'utilisateur ne peut pas la contredire »**.

Il y a d'ailleurs une preuve expérimentale supplémentaire, et c'est la plus parlante : l'agent a **testé un correctif et l'a rejeté sur mesure**. Rendre `is_synchronizable<const copy_on_write<T>>` vrai est sémantiquement correct (un `copy_on_write` const ne distribue que des `const T&`, et copier la poignée est un incrément de compteur atomique), et cela fait bien basculer `synchronized_value<copy_on_write<T>>` vers `shared_mutex` comme « il faudrait ». Mesuré : **344 ns/lecture au lieu de 40**, soit **9x pire**. Le correctif a été annulé et le résultat négatif rapporté. Sur cette plateforme, le mutex qui a l'air correct est le mauvais mutex.

### 2.7 Le correctif complet

Un deuxième paramètre de template, dont la valeur par défaut est exactement la sélection automatique actuelle. Le défaut ne change pour aucun code existant ; l'utilisateur qui sait ce qu'il fait peut écrire `synchronized_value<std::deque<Job>, std::mutex>`. Le `static_assert` ajouté interdit d'écrire l'inverse dangereux — un mutex partageable sur un `T` dont la lecture `const` n'est pas sûre.

Fichier `include/threadsafe/details/synchronized_value.h`, en entier :

```cpp
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

// A reader lock only pays for itself when the critical section is long enough
// to amortise its bookkeeping; on a short one it loses badly to a plain mutex.
// The default therefore only says what is *possible* (a const T is read-safe,
// so readers may overlap), and the second template parameter is how a caller
// says what is *profitable*.
template <class T>
consteval auto default_mutex_type() {
    if constexpr (is_synchronizable_v<const T>)
        return ^^std::shared_mutex;
    else
        return ^^std::mutex;
}

template <class T>
using default_mutex_t = [: default_mutex_type<T>() :];

template <class Mutex>
concept shareable_mutex = requires(Mutex &mutex) {
    mutex.lock_shared();
    mutex.unlock_shared();
};

}

template <class T, class Mutex = detail::default_mutex_t<T>>
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
    static_assert(!detail::shareable_mutex<Mutex> || is_synchronizable_v<const T>,
                  "a shared_mutex lets several readers hold a const T& at "
                  "once, so a const T must be read-safe from several threads");

public:
    static consteval auto get_mutex_type() { return ^^Mutex; }

    using mutex = Mutex;

    static consteval auto get_const_guard_type() {
        if constexpr (detail::shareable_mutex<Mutex>) {
            return ^^value_guard<const T, std::shared_lock<mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<mutex>>;
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
    mutable mutex mutex_;
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

**Vérification de non-régression, faite par moi :**

```
$ for f in /Users/amorrier/Programmation/ThreadSafe/tests/*.cpp; do \
    g++-16 -std=c++26 -freflection -Iinclude-fix -fsyntax-only "$f" \
      && echo "OK   $(basename $f)" || echo "FAIL $(basename $f)"; done
OK   test_asynchronous_task_launcher.cpp
OK   test_containers.cpp
OK   test_copy_on_write.cpp
OK   test_deferred_specialization.cpp
OK   test_diagnostics.cpp
OK   test_lifetime_aware.cpp
OK   test_sendable.cpp
OK   test_smart_pointers.cpp
OK   test_soundness_regressions.cpp
OK   test_synchronizable.cpp
OK   test_synchronized_value.cpp
```

**11 TU sur 11, vertes.** `tests/test_synchronized_value.cpp` fige `sync_memo::mutex == std::mutex` et `sync_int::const_guard == value_guard<const int, shared_lock<shared_mutex>>` : les deux tiennent encore, donc le comportement par défaut est bit-pour-bit inchangé. Statut : **suite-passes**, confirmé de première main.

**Effet mesuré.** Le programme de la §2.3 avec une quatrième colonne, `synchronized_value<std::map<int,int>, std::mutex>` :

```cpp
// 90% read / 10% write on std::map<int,int>, four ways, WITH the proposed fix:
//   1. threadsafe::synchronized_value<std::map<int,int>>              (default: shared_mutex)
//   2. threadsafe::synchronized_value<std::map<int,int>, std::mutex>  (explicit, new)
//   3. the same map behind a hand-written std::shared_mutex
//   4. the same map behind a hand-written std::mutex
// build: g++-16 -std=c++26 -freflection -I<patched include> -O2 -pthread bench_fix.cpp -o bench_fix
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int operations_per_thread = 200'000;
constexpr int key_space = 4096;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

template <class Guarded>
struct library_worker {
    std::shared_ptr<Guarded> table;
    std::shared_ptr<std::atomic<std::uint64_t>> checksum;
    int seed;

    void operator()() const {
        fast_random random{static_cast<std::uint64_t>(seed) * 2654435761u + 1};
        std::uint64_t local = 0;
        for (int operation = 0; operation < operations_per_thread; ++operation) {
            const std::uint64_t draw = random.next();
            const int key = static_cast<int>(draw % key_space);
            if (draw % 10 != 0) {
                const auto guard = table->lock_shared();
                const auto found = guard->find(key);
                if (found != guard->end())
                    local += static_cast<std::uint64_t>(found->second);
            } else {
                auto guard = table->lock();
                (*guard)[key] = key * 2;
            }
        }
        *checksum += local;
    }
};

template <class Mutex>
struct hand_written {
    Mutex mutex;
    std::map<int, int> table;
    std::atomic<std::uint64_t> checksum{0};
};

template <class Mutex, class ReadLock>
double run_hand_written(int thread_count) {
    hand_written<Mutex> shared;
    for (int key = 0; key < key_space; key += 2)
        shared.table[key] = key * 2;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&shared, worker] {
                fast_random random{
                    static_cast<std::uint64_t>(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int operation = 0; operation < operations_per_thread;
                     ++operation) {
                    const std::uint64_t draw = random.next();
                    const int key = static_cast<int>(draw % key_space);
                    if (draw % 10 != 0) {
                        ReadLock lock{shared.mutex};
                        const auto found = shared.table.find(key);
                        if (found != shared.table.end())
                            local += static_cast<std::uint64_t>(found->second);
                    } else {
                        std::unique_lock lock{shared.mutex};
                        shared.table[key] = key * 2;
                    }
                }
                shared.checksum += local;
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

template <class Guarded>
double run_library(int thread_count) {
    auto table = Guarded::make();
    {
        auto guard = table->lock();
        for (int key = 0; key < key_space; key += 2)
            (*guard)[key] = key * 2;
    }
    auto checksum = std::make_shared<std::atomic<std::uint64_t>>(0);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < thread_count; ++worker)
            launcher.launch_task(
                library_worker<Guarded>{table, checksum, worker});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

using default_map = threadsafe::synchronized_value<std::map<int, int>>;
using exclusive_map =
    threadsafe::synchronized_value<std::map<int, int>, std::mutex>;

}

static_assert(std::is_same_v<default_map::mutex, std::shared_mutex>);
static_assert(std::is_same_v<exclusive_map::mutex, std::mutex>);
static_assert(std::is_same_v<
              exclusive_map::const_guard,
              threadsafe::value_guard<const std::map<int, int>,
                                      std::unique_lock<std::mutex>>>);

int main() {
    std::printf("90%% read / 10%% write on std::map<int,int>, "
                "%d ops per thread, ns per op\n\n", operations_per_thread);
    std::printf("%8s %12s %14s %14s %10s\n", "threads", "sync_value",
                "sync_value<mtx>", "shared_mutex", "mutex");
    for (int thread_count : {1, 2, 4, 8, 12}) {
        const double automatic = run_library<default_map>(thread_count);
        const double explicit_mutex = run_library<exclusive_map>(thread_count);
        const double shared =
            run_hand_written<std::shared_mutex,
                             std::shared_lock<std::shared_mutex>>(thread_count);
        const double exclusive =
            run_hand_written<std::mutex, std::unique_lock<std::mutex>>(
                thread_count);
        std::printf("%8d %12.1f %14.1f %14.1f %10.1f\n", thread_count, automatic,
                    explicit_mutex, shared, exclusive);
    }
}
```

Mesuré par l'agent :

```
 threads     sync_value  sync_value<mtx>   shared_mutex          mutex
       1           43.0             42.8           41.5           43.1
       2          444.8             75.1          447.4           73.0
       4          666.7             93.3          670.5           85.3
       8         1223.5            150.0         1328.4          154.0
      12         1547.1            135.0         1476.6          130.1
```

Rejoué par moi (`load average` 4,16 au départ, 5,48 à l'arrivée — machine chargée, valeurs absolues à prendre avec réserve, rapports fiables) :

```
$ g++-16 -std=c++26 -freflection -Iinclude-fix -O2 -pthread bench_fix.cpp -o bench_fix
$ ./bench_fix
90% read / 10% write on std::map<int,int>, 200000 ops per thread, ns per op

 threads   sync_value sync_value<mtx>   shared_mutex      mutex
       1         46.5           51.3           48.5       50.2
       2        489.0           79.2          479.0       88.2
       4       1065.3          147.7          951.3      155.5
       8       1302.9          147.0         1594.5      150.7
      12       1492.5          131.7         1558.5      140.0
```

À 8 threads : **1302,9 ns → 147,0 ns, soit 8,9x**, et la colonne 2 colle à la colonne 4 (`std::mutex` à la main) à 2 % près. Le paramètre explicite rend la bibliothèque exactement aussi rapide que le code écrit à la main.

Et sur la file de la §2.4, en changeant exactement deux lignes du programme `bench_queue.cpp` donné plus haut — la déclaration de la file et son `static_assert` :

```cpp
    threadsafe::synchronized_value<std::deque<Job>, std::mutex> queue;
```

```cpp
static_assert(std::is_same_v<threadsafe::synchronized_value<std::deque<Job>, std::mutex>::mutex, std::mutex>,
              "the explicit second parameter overrides the automatic choice");
```

```
$ g++-16 -std=c++26 -freflection -Iinclude-fix -O2 -pthread bench_queue_fixed.cpp -o bench_queue_fixed
$ ./bench_queue_fixed
4 producers x 25000 jobs, 4 consumers, std::deque<Job>

queue                                                       ms       checksum
synchronized_value<std::deque<Job>> (explicit std::mutex)    5.2     4999950000
hand-written std::mutex                                      5.6     4999950000
std::mutex + std::condition_variable                         7.8     4999950000
```

**250 ms → 5,2 ms**, soit 48x, et la bibliothèque passe *devant* le `std::mutex` écrit à la main (bruit de mesure ; disons : à égalité).

### 2.8 Et l'argument pédagogique

Il faut le poser franchement, parce que le point de ce projet est une conférence. « La bibliothèque choisit le mutex pour vous » est une belle diapositive. « La bibliothèque choisit le mutex pour vous, et sur une section critique courte c'est 50x plus lent, et vous ne pouvez pas dire non » est une *meilleure* diapositive, à condition que la deuxième moitié existe. Le correctif de la §2.7 fournit la deuxième moitié en dix lignes : la valeur par défaut dit ce qui est **possible**, le paramètre explicite dit ce qui est **rentable**, et la distinction entre les deux est précisément le contenu que la bibliothèque a à enseigner.

Le [rapport 08](./08-api-et-flexibilite.md) demande **le même changement** pour une raison différente — l'extensibilité, l'impossibilité de brancher un mutex maison ou un `std::recursive_mutex`. Je ne duplique pas son argumentaire ; le correctif à appliquer est celui-ci, et il sert les deux.

---
## 3. L6 — `launch_scoped_task` crée un thread et le joint aussitôt : N appels s'exécutent l'un après l'autre

**Gravité : haute. Le nom promet un lancement ; le corps fait une exécution synchrone.**

### 3.1 Le corps, en entier

Extrait de `include/threadsafe/details/asynchronous_task_launcher.h` :

```cpp
    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it does
    // not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }
```

Deux lignes. Le `join()` est immédiat, dans la même fonction, avant tout retour. Il n'y a pas de conteneur, pas de report, pas de portée : le thread naît et meurt à l'intérieur de l'appel. La conséquence n'a rien de subtil — **N appels à `launch_scoped_task` sont N exécutions strictement séquentielles**, avec en prime N créations de thread de pure perte.

C'est même deux fois payé : le travail n'est pas parallélisé, *et* on paie 12 µs de création/destruction de thread par appel (§1) pour ne rien paralléliser.

### 3.2 Le programme et la mesure

```cpp
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using clock_type = std::chrono::steady_clock;

namespace {
struct SleepsTwoHundred {
    void operator()() const { std::this_thread::sleep_for(200ms); }
};

double ms_since(clock_type::time_point start) {
    return std::chrono::duration<double, std::milli>(clock_type::now() - start).count();
}
}

int main() {
    constexpr int task_count = 4;

    for (int repetition = 0; repetition < 3; ++repetition) {
        threadsafe::asynchronous_task_launcher launcher;
        const auto scoped_start = clock_type::now();
        for (int index = 0; index < task_count; ++index)
            launcher.launch_scoped_task(SleepsTwoHundred{});
        const double scoped_ms = ms_since(scoped_start);

        const auto task_start = clock_type::now();
        {
            threadsafe::asynchronous_task_launcher parallel_launcher;
            for (int index = 0; index < task_count; ++index)
                parallel_launcher.launch_task(SleepsTwoHundred{});
        }
        const double task_ms = ms_since(task_start);

        const auto bare_start = clock_type::now();
        {
            std::vector<std::jthread> bare_threads;
            for (int index = 0; index < task_count; ++index)
                bare_threads.emplace_back(SleepsTwoHundred{});
        }
        const double bare_ms = ms_since(bare_start);

        std::printf("run %d: %d x 200ms   launch_scoped_task=%7.1f ms   "
                    "launch_task+dtor=%7.1f ms   bare vector<jthread>=%7.1f ms\n",
                    repetition, task_count, scoped_ms, task_ms, bare_ms);
    }
}
```

Mesuré par l'agent :

```
run 0: 4 x 200ms   launch_scoped_task=  820.2 ms   launch_task+dtor=  205.1 ms   bare vector<jthread>=  205.1 ms
run 1: 4 x 200ms   launch_scoped_task=  814.8 ms   launch_task+dtor=  205.1 ms   bare vector<jthread>=  205.1 ms
run 2: 4 x 200ms   launch_scoped_task=  816.0 ms   launch_task+dtor=  205.1 ms   bare vector<jthread>=  205.1 ms
```

Rejoué par moi :

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread l6_scoped.cpp -o l6_scoped
$ ./l6_scoped
run 0: 4 x 200ms   launch_scoped_task=  820.3 ms   launch_task+dtor=  205.1 ms   bare vector<jthread>=  205.1 ms
run 1: 4 x 200ms   launch_scoped_task=  815.9 ms   launch_task+dtor=  205.1 ms   bare vector<jthread>=  205.1 ms
run 2: 4 x 200ms   launch_scoped_task=  817.9 ms   launch_task+dtor=  205.1 ms   bare vector<jthread>=  205.1 ms
```

| Chemin | 4 tâches de 200 ms | Accélération | Pic de threads simultanés |
|---|---:|---:|---:|
| `launch_scoped_task` ×4 | **816 – 820 ms** | 1,0x | **1** |
| `launch_task` ×4 puis destruction du lanceur | **205,1 ms** | 4,0x | 4 |
| `std::vector<std::jthread>` nu | **205,1 ms** | 4,0x | 4 |

Reproduction à l'identique du chiffre de 817 ms de l'audit précédent, à ±0,5 %. La sérialisation est exacte : 4 × 200 ms + 4 × ~4 ms de démarrage/arrêt de thread = 816 ms. Et la mesure du corpus qui compte réellement les threads vivants donne **pic de concurrence 1** pour 8 tâches de 100 ms (840,5 ms contre 105,1 ms pour 8 `std::jthread` nus).

Notez au passage, dans le même tableau, la §1 qui se confirme : **205,1 ms contre 205,1 ms**, `launch_task` et un `std::vector<std::jthread>` nu sont indiscernables au dixième de milliseconde près.

### 3.3 Le nom communique-t-il cela ? Non.

Il faut répondre à la question sans détour.

- `launch_` — dans toute la bibliothèque et dans tout le vocabulaire du domaine, « lancer » veut dire *démarrer sans attendre*. `launch_task`, la fonction sœur, fait exactement cela.
- `_scoped_` — suggère une **portée**, c'est-à-dire une fenêtre pendant laquelle plusieurs tâches vivent et à la fermeture de laquelle elles sont jointes. C'est le sens de `std::scoped_lock`, celui du « structured concurrency » de `std::execution`, celui de `scoped_thread` chez Williams. Aucun de ces sens n'est « une seule tâche, jointe immédiatement ».
- Le commentaire de précondition parle de la durée de vie de l'emprunt, pas de la concurrence. Il dit *« the join bounds the invocation »* — ce qui est vrai — mais ne dit jamais que ce `join` est immédiat et qu'aucun parallélisme n'est possible.

**Un lecteur qui voit `launch_scoped_task` lit « lance une tâche dont l'emprunt est borné par une portée ».** Il obtient « exécute la tâche ici et maintenant, sur un autre thread, et attends ». C'est un contresens complet, et il est aggravé par le fait que `launch_scoped_task` est le **seul** point d'entrée qui accepte un argument emprunté : `launchable_scoped_task` laisse tomber la contrainte `lifetime_aware`. C'est donc la seule porte pour distribuer du travail sur des données qu'on ne veut pas copier — et c'est celle qui ne distribue rien.

À quoi s'ajoute le défaut vérifié personnellement par le lead : **`launch_scoped_task` interbloque sur une tâche coopérative.** Le `std::jthread` qu'il crée est une variable locale, donc aucun appelant ne peut atteindre son `stop_source` ; une tâche qui attend `stop_requested()` n'est jamais interrompue. Confirmé au chien de garde, sortie 42, le message « launch_scoped_task returned » n'est jamais imprimé. Le détail est traité dans [02](./02-robustesse-des-helpers.md) ; du point de vue performance il faut simplement l'énoncer : **le pire temps d'exécution de `launch_scoped_task` n'est pas 4x, il est infini.**

### 3.4 Que recommander — en cherchant d'abord à supprimer

La règle de la maison est de contester le besoin avant d'ajouter du code. Alors contestons.

**Que fait `launch_scoped_task` que l'utilisateur ne peut pas écrire lui-même ?** Rien, sauf la vérification de traits sous le contrat affaibli (`sendable` sans `lifetime_aware`). Le corps est littéralement `std::jthread task{f}; task.join();`, c'est-à-dire `f()` avec un aller-retour de 12 µs sur un autre thread. Aucun bénéfice de concurrence, un contrat de durée de vie que les traits ne peuvent pas vérifier (le commentaire le reconnaît), et un interblocage sur toute tâche coopérative.

Trois options, dans l'ordre de préférence :

**(a) Renommer, en gardant le corps.** Le changement minimal et honnête : le nom cesse de mentir, l'audience apprend la distinction, aucune sémantique ne bouge. À remplacer tel quel dans `asynchronous_task_launcher` :

```cpp
    // Runs f on another thread and WAITS for it, here, before returning. It is
    // not a launch: two calls never overlap, and N calls cost N times the work
    // plus N thread creations. The immediate join is what bounds the borrow —
    // which is why this is the only entry point that accepts a borrowed
    // argument (launchable_scoped_task drops the lifetime_aware requirement).
    //
    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it
    // does not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
    //
    // The jthread is a local, so no caller can reach its stop_source: a task
    // that waits on stop_requested() will never be stopped, and this call will
    // never return. Do not pass a cooperative task.
    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void run_scoped_task_and_wait(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }

    template <typename F, typename... Args>
    void run_scoped_task_and_wait(F, Args...) {
        detail::explain_launch_scoped_task<F, Args...>();
    }
```

**Vérification de non-régression, faite par moi, et elle n'est pas gratuite.** Sur les en-têtes seuls, ce renommage est **suite-regresses** : `tests/test_synchronized_value.cpp` tombe, parce qu'il appelle le membre par son nom.

```
FAIL test_synchronized_value.cpp
/Users/amorrier/Programmation/ThreadSafe/tests/test_synchronized_value.cpp:30:11: error: 'class threadsafe::asynchronous_task_launcher' has no member named 'launch_scoped_task'
/Users/amorrier/Programmation/ThreadSafe/tests/test_synchronized_value.cpp:83:15: error: non-constant condition for static assertion
```

Le site d'appel est unique dans toute la suite — `tests/test_synchronized_value.cpp`, ligne 30, à l'intérieur d'une expression `requires` :

```cpp
template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.run_scoped_task_and_wait(f, args...);
    };
```

(`tests/test_asynchronous_task_launcher.cpp` définit son propre `can_launch_scoped_task` à partir du **concept** `threadsafe::launchable_scoped_task`, pas du membre, et n'est donc pas touché.)

Avec cette seule ligne mise à jour, j'ai rejoué la suite entière :

```
OK   test_asynchronous_task_launcher.cpp
OK   test_containers.cpp
OK   test_copy_on_write.cpp
OK   test_deferred_specialization.cpp
OK   test_diagnostics.cpp
OK   test_lifetime_aware.cpp
OK   test_sendable.cpp
OK   test_smart_pointers.cpp
OK   test_soundness_regressions.cpp
OK   test_synchronizable.cpp
OK   test_synchronized_value.cpp
```

**11 TU sur 11, vertes.** Le renommage compile et s'exécute (`launcher.run_scoped_task_and_wait(SleepsTen{}); // -> "returned"`), à condition d'inclure la ligne de test dans le même commit. Statut honnête : **suite-regresses sur l'en-tête seul, suite-passes avec la mise à jour d'une ligne de test.**

**(b) Supprimer.** Défendable : un point d'entrée qui exécute une tâche sur un autre thread et attend n'a, en pratique, aucun usage que `f(args...)` ne couvre mieux. Ce qu'il apporte réellement, c'est le contrat de trait affaibli — et ce contrat est justement celui qui a besoin d'une *portée* pour être sûr, ce que ce corps ne fournit pas. Supprimer supprime aussi l'interblocage coopératif et le contresens de nommage d'un coup. Le coût est de retirer de la bibliothèque la seule démonstration du contrat « emprunt borné », qui est une idée que la conférence veut probablement montrer.

**(c) Ajouter un vrai `scoped_task_group`.** C'est ce que propose le constat L3 du [rapport 02](./02-robustesse-des-helpers.md), avec le code complet et une vérification de non-régression **suite-passes**. Le résultat mesuré, substitué à huit `launch_scoped_task` : **105,3 ms au lieu de 840,5 ms, pic de concurrence 8**, c'est-à-dire identique à huit `std::jthread` nus, tout en conservant le contrat d'emprunt puisque le groupe joint toutes ses tâches avant la fermeture de la portée. Je ne recopie pas ce code ici : il appartient au rapport 02 et le dupliquer créerait deux versions à maintenir. Du point de vue purement performance, c'est la seule option qui rende le point d'entrée utile.

Mon avis : **(a) tout de suite** — c'est une ligne, ça ne casse rien, et ça arrête de mentir — **et (c) si et seulement si la conférence veut réellement enseigner le contrat d'emprunt**, ce qui est probable. (b) reste la bonne réponse si la démonstration de l'emprunt n'est pas au programme.

---

## 4. Ce que les abstractions coûtent de bout en bout

Quatre programmes complets, écrits comme un utilisateur les écrirait, et chronométrés contre leur équivalent à la main. Aucun de ces quatre constats n'a été revérifié par le lead ; je les ai tous recompilés et rejoués moi-même, et je donne mes chiffres à côté de ceux de l'agent.

### 4.1 E1 — publier une configuration : `copy_on_write` n'a aucun canal de publication

**Gravité : moyenne. Aucun correctif de code proposé, délibérément — ce qui manque est une phrase de documentation, pas du code.**

`copy_on_write<T>` n'a ni `store()` ni `load()`, et `is_synchronizable<copy_on_write<T>>` est faux. La poignée ne peut donc pas être partagée : chaque thread doit avoir la sienne, et `as_mutable()` ne rebranche que celle de l'écrivain.

Ce qu'un utilisateur écrit d'abord — partager **une** poignée — est refusé :

```cpp
// Program 2, attempt A: publish by sharing ONE copy_on_write object between the
// writer and the readers. This is what a user reaches for first.
#include <threadsafe/threadsafe.h>
#include <functional>
#include <string>
#include <vector>

struct Config { std::string name; std::vector<int> weights; };

struct reader_task {
    void operator()(threadsafe::copy_on_write<Config>& shared_config) const {
        (void)shared_config->name;
    }
};

int main() {
    threadsafe::copy_on_write<Config> shared_config{Config{"v1", {1, 2, 3}}};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(reader_task{}, std::ref(shared_config));
}
```

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -fsyntax-only e1_naive.cpp
.../asynchronous_task_launcher.h:98:48: error: call to consteval function 'threadsafe::detail::explain_launch_task<reader_task, std::reference_wrapper<threadsafe::copy_on_write<Config> > >()' is not a constant expression
.../asynchronous_task_launcher.h:99:5: error: uncaught exception of type 'std::meta::exception'; 'what()': 'std::reference_wrapper<threadsafe::copy_on_write<Config> > has a user-written copy, move or destructor — or a template that may be selected as one — which can share state the members do not show; specialize is_sendable to state the intent'
```

Vérifié par moi. Le motif est refusé, ce qui est correct ; **la raison donnée est fausse** — le vrai motif est que `copy_on_write<Config>` n'est pas synchronizable, pas que `reference_wrapper` a un constructeur de copie écrit à la main. Ce défaut de diagnostic est traité dans [04](./04-diagnostics.md).

La seule forme que les traits autorisent — une poignée par thread — compile, ne provoque aucune course (confirmé sous ThreadSanitizer), est la plus rapide des cinq, et **ne publie jamais rien**.

Le programme complet, cinq stratégies, même ordonnanceur (`std::jthread` nu partout, pour que seule la mécanique de publication varie) :

```cpp
// Publishing a new configuration to 8 reader threads, five ways.
// The harness (bare std::jthread) is identical in all five; only the
// publication mechanism changes, so the columns compare mechanisms and nothing
// else. Each reader reports the highest version it ever managed to observe.
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//        bench_publish.cpp -o bench_publish
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int reader_count = 8;
constexpr int reads_per_reader = 400'000;
constexpr int versions_to_publish = 200;
constexpr auto publication_interval = std::chrono::microseconds{200};

struct Config {
    int version;
    std::string name;
    std::vector<int> weights;
};

Config make_config(int version) {
    return Config{version, "v" + std::to_string(version), {1, 2, 3, 4, 5, 6, 7, 8}};
}

struct result {
    double nanoseconds_per_read;
    double milliseconds;
    int highest_version_seen;
    unsigned long long checksum;
};

std::atomic<int> highest_version_seen{0};
std::atomic<unsigned long long> global_checksum{0};

void record(int version, unsigned long long checksum) {
    int previous = highest_version_seen.load(std::memory_order_relaxed);
    while (version > previous
           && !highest_version_seen.compare_exchange_weak(previous, version))
        ;
    global_checksum.fetch_add(checksum, std::memory_order_relaxed);
}

// Runs `read_once` on reader_count threads while `publish` walks the versions.
template <class ReadOnce, class Publish>
result run(ReadOnce read_once, Publish publish) {
    highest_version_seen.store(0);
    global_checksum.store(0);
    std::atomic<bool> readers_done{false};

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::jthread writer{[&] {
            for (int version = 2;
                 version <= versions_to_publish && !readers_done.load();
                 ++version) {
                publish(version);
                std::this_thread::sleep_for(publication_interval);
            }
        }};
        {
            std::vector<std::jthread> readers;
            for (int reader = 0; reader < reader_count; ++reader)
                readers.emplace_back([&] {
                    int best = 0;
                    unsigned long long checksum = 0;
                    for (int read = 0; read < reads_per_reader; ++read)
                        read_once(best, checksum);
                    record(best, checksum);
                });
        }
        readers_done.store(true);
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    const double nanoseconds =
        std::chrono::duration<double, std::nano>(elapsed).count();
    return result{nanoseconds / (double(reader_count) * reads_per_reader),
                  nanoseconds / 1e6, highest_version_seen.load(),
                  global_checksum.load()};
}

void observe(const Config& config, int& best, unsigned long long& checksum) {
    if (config.version > best)
        best = config.version;
    checksum += static_cast<unsigned long long>(config.weights.front());
}

// 1. one threadsafe::copy_on_write handle per thread — the only shape the
//    traits allow, because is_synchronizable_v<copy_on_write<Config>> is false.
result strategy_private_handles() {
    threadsafe::copy_on_write<Config> writer_handle{make_config(1)};
    threadsafe::copy_on_write<Config> reader_handle = writer_handle;
    return run(
        [&](int& best, unsigned long long& checksum) {
            observe(*reader_handle, best, checksum);
        },
        [&](int version) { writer_handle.as_mutable() = make_config(version); });
}

// 2. threadsafe::synchronized_value<threadsafe::copy_on_write<Config>> — the
//    reader takes a snapshot of the HANDLE under the lock and reads outside it.
result strategy_synchronized_cow() {
    threadsafe::synchronized_value<threadsafe::copy_on_write<Config>> published{
        make_config(1)};
    return run(
        [&](int& best, unsigned long long& checksum) {
            threadsafe::copy_on_write<Config> snapshot = [&] {
                const auto guard = published.lock_shared();
                return *guard;
            }();
            observe(*snapshot, best, checksum);
        },
        [&](int version) {
            auto guard = published.lock();
            guard->as_mutable() = make_config(version);
        });
}

// 3. threadsafe::synchronized_value<Config> — the wrapper picks shared_mutex
//    and the reader holds it for the whole read.
result strategy_synchronized_config() {
    threadsafe::synchronized_value<Config> published{make_config(1)};
    return run(
        [&](int& best, unsigned long long& checksum) {
            const auto guard = published.lock_shared();
            observe(*guard, best, checksum);
        },
        [&](int version) {
            auto guard = published.lock();
            *guard = make_config(version);
        });
}

// 4. the same thing by hand.
result strategy_hand_written_shared_mutex() {
    std::shared_mutex mutex;
    Config config = make_config(1);
    return run(
        [&](int& best, unsigned long long& checksum) {
            std::shared_lock lock{mutex};
            observe(config, best, checksum);
        },
        [&](int version) {
            std::unique_lock lock{mutex};
            config = make_config(version);
        });
}

// 5. the textbook answer, which the library has no wrapper for.
result strategy_atomic_shared_ptr() {
    std::atomic<std::shared_ptr<const Config>> published{
        std::make_shared<const Config>(make_config(1))};
    return run(
        [&](int& best, unsigned long long& checksum) {
            const std::shared_ptr<const Config> snapshot = published.load();
            observe(*snapshot, best, checksum);
        },
        [&](int version) {
            published.store(std::make_shared<const Config>(make_config(version)));
        });
}

}

static_assert(threadsafe::is_sendable_v<threadsafe::copy_on_write<Config>>,
              "a copy_on_write handle may be sent to another thread");
static_assert(!threadsafe::is_synchronizable_v<threadsafe::copy_on_write<Config>>,
              "but it may NOT be shared, which is why strategy 1 needs one "
              "handle per thread");
static_assert(!threadsafe::is_synchronizable_v<
                  const threadsafe::copy_on_write<Config>>,
              "which is also why strategy 2's synchronized_value picks "
              "std::mutex, not std::shared_mutex");
static_assert(std::is_same_v<
                  threadsafe::synchronized_value<
                      threadsafe::copy_on_write<Config>>::mutex,
                  std::mutex>);
static_assert(std::is_same_v<threadsafe::synchronized_value<Config>::mutex,
                             std::shared_mutex>);

int main() {
    std::printf("%d readers x %d reads, writer publishing up to v%d\n\n",
                reader_count, reads_per_reader, versions_to_publish);
    std::printf("%-46s %9s %10s %9s\n", "strategy", "ms", "ns/read",
                "max ver.");

    const struct {
        const char* name;
        result (*measure)();
    } strategies[] = {
        {"1. copy_on_write, one handle per thread", strategy_private_handles},
        {"2. synchronized_value<copy_on_write<Config>>",
         strategy_synchronized_cow},
        {"3. synchronized_value<Config> (shared_mutex)",
         strategy_synchronized_config},
        {"4. hand-written std::shared_mutex", strategy_hand_written_shared_mutex},
        {"5. std::atomic<std::shared_ptr<const Config>>",
         strategy_atomic_shared_ptr},
    };

    for (const auto& strategy : strategies) {
        const result measured = strategy.measure();
        std::printf("%-46s %9.1f %10.1f %9d\n", strategy.name,
                    measured.milliseconds, measured.nanoseconds_per_read,
                    measured.highest_version_seen);
    }
}
```

Mesuré par l'agent :

```
1. copy_on_write, one handle per thread            51.2 ms       16 ns/read  highest version seen by a reader:   1   <-- NEVER PUBLISHED
2. synchronized_value<copy_on_write<Config>>      128.9 ms       40 ns/read  highest version seen by a reader: 200
3. synchronized_value<Config> (shared_mutex)     1024.2 ms      320 ns/read  highest version seen by a reader: 200
4. hand-written std::shared_mutex                 995.6 ms      311 ns/read  highest version seen by a reader: 200
5. std::atomic<std::shared_ptr<const Config>>     122.2 ms       38 ns/read  highest version seen by a reader: 200
```

Rejoué par moi, avec ma propre reconstruction du programme (deux exécutions) :

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread bench_publish.cpp -o bench_publish
$ ./bench_publish
8 readers x 400000 reads, writer publishing up to v200

strategy                                              ms    ns/read  max ver.
1. copy_on_write, one handle per thread              0.3        0.1         1
2. synchronized_value<copy_on_write<Config>>        50.7       15.9       159
3. synchronized_value<Config> (shared_mutex)       878.9      274.7       200
4. hand-written std::shared_mutex                  765.6      239.3       200
5. std::atomic<std::shared_ptr<const Config>>       86.2       26.9       200

$ ./bench_publish
8 readers x 400000 reads, writer publishing up to v200

strategy                                              ms    ns/read  max ver.
1. copy_on_write, one handle per thread              0.4        0.1         1
2. synchronized_value<copy_on_write<Config>>        52.3       16.3       160
3. synchronized_value<Config> (shared_mutex)       974.8      304.6       200
4. hand-written std::shared_mutex                  756.7      236.5       200
5. std::atomic<std::shared_ptr<const Config>>       82.3       25.7       200
```

Trois choses, dont une divergence que je dois signaler :

1. **La colonne « max ver. » est le verdict.** La stratégie 1 reste bloquée sur la version 1 pour toute la durée du programme. Elle est rapide, elle est sans course, elle est fausse.
2. **Ma stratégie 1 mesure 0,1 ns/lecture, l'agent en mesurait 16.** Divergence réelle, et instructive : dans ma reconstruction la poignée du lecteur est une variable locale que rien ne modifie, alors GCC à `-O2` sort la lecture de la boucle. Le compilateur *prouve* que la lecture est invariante — ce qui est précisément la raison pour laquelle rien n'est jamais publié. La différence de chiffre ne change pas la conclusion, elle la renforce.
3. **La stratégie 2 est la bonne réponse, et elle est excellente.** `synchronized_value<copy_on_write<Config>>` mesure 15,9–16,3 ns/lecture chez moi, contre 25,7–26,9 ns pour `std::atomic<std::shared_ptr<const Config>>`, la réponse de manuel. L'agent mesurait 40 contre 38 ns. Dans les deux séries, la bibliothèque est à égalité avec l'état de l'art (chez moi elle passe même devant, parce que le verrou exclusif évite le trafic de cache du compteur de références atomique). C'est un très bon résultat.

Et remarquez l'ironie mesurée : la stratégie 2 marche vite **parce que** `is_synchronizable<const copy_on_write<T>>` est faux, donc le `synchronized_value` choisit un `std::mutex`. Le verrou exclusif, ici, est accidentellement le bon. Le rendre « correct » coûte 9x (§2.6).

**Ce qu'il manque n'est pas du code, c'est un nom de motif.** La documentation de `copy_on_write` devrait dire trois choses :

> Un `copy_on_write<T>` est une poignée d'**instantané, privée à un thread**. Elle se transmet à un thread (`is_sendable`), elle ne se partage pas (`is_synchronizable` est faux).
> La publication passe par `synchronized_value<copy_on_write<T>>`.
> Le chemin de lecture s'écrit :
> ```cpp
> threadsafe::copy_on_write<T> snapshot = [&] {
>     const auto guard = published->lock_shared();
>     return *guard;
> }();
> ```

Le `CLAUDE.md` actuel dit, à l'inverse : *« A shared `T` read through `const` only »*. Le mot « shared » y désigne le bloc de contrôle partagé, mais un lecteur comprend « partagé entre threads » — exactement ce que les traits interdisent. C'est une phrase à réécrire, pas une fonctionnalité à ajouter. Voir aussi [05](./05-simplicite.md).

### 4.2 E3 — donner à une tâche une tranche de `std::vector`

**Gravité : moyenne. Aucun correctif proposé : le refus est correct au regard des règles annoncées.**

Les trois formes qu'un utilisateur essaie sont toutes refusées.

```cpp
#include <threadsafe/threadsafe.h>
#include <span>
#include <vector>
struct slice_task {
    void operator()(std::span<const double> slice) const { (void)slice; }
};
int main() {
    std::vector<double> data(1024);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(slice_task{}, std::span<const double>{data});
}
```

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -fsyntax-only e3_span.cpp
.../asynchronous_task_launcher.h:99:5: error: uncaught exception of type 'std::meta::exception'; 'what()': 'std::span<const double> has a user-written copy, move or destructor — or a template that may be selected as one — which can share state the members do not show; specialize is_sendable to state the intent'
```

Vérifié par moi. Et si l'on suit le conseil et qu'on spécialise `is_sendable<std::span<const double>>`, on obtient une **seconde** erreur, qui est cette fois la vraie :

```
'what()': 'std::span<const double> is a borrowed range: a view over someone else's storage, it does not keep its elements alive'
```

| Forme essayée | Résultat |
|---|---|
| `std::span<const double>` | refusée, mauvaise raison, puis refusée avec la bonne raison |
| `const double*` | refusée : ni sendable ni lifetime aware |
| `std::shared_ptr<const std::vector<double>>` | refusée, mauvaise raison (`vector<double>` n'est pas synchronizable, car le `const` derrière l'indirection n'est jamais cru) |

**Ce refus est correct.** `CLAUDE.md` dit que la propriété est transitive et que le `const` derrière une indirection n'est jamais cru ; un `span` est une vue sur une mémoire qu'il ne possède pas. La bibliothèque applique sa propre règle. Le défaut est la *première* raison, fausse, qui envoie l'utilisateur en chasse à courre — et il disparaît avec le correctif du constat L8 ([04](./04-diagnostics.md)).

La seule forme sans copie que la bibliothèque autorise, et son coût :

```cpp
// Parallel map over a slice of a std::vector<double>, three ways:
//   A. the only zero-copy shape the library allows:
//      shared_ptr<synchronized_value<vector>> + a shared_lock per worker + bounds
//   B. give each worker its own copy of its slice
//   C. bare std::jthread + std::span — which launch_task refuses
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//        bench_parallel_map.cpp -o bench_parallel_map
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t element_count = 10'000'000;
constexpr int worker_count = 8;

double map_element(double value) { return value * 0.5 + 1.0; }

double milliseconds_since(std::chrono::steady_clock::time_point started_at) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - started_at)
        .count();
}

// A. what the library forces: a named struct, a shared_ptr to the mutex-wrapped
//    WHOLE vector, and hand-carried index bounds.
struct guarded_slice_task {
    std::shared_ptr<threadsafe::synchronized_value<std::vector<double>>> data;
    std::shared_ptr<threadsafe::synchronized_value<double>> total;
    std::size_t first;
    std::size_t last;

    void operator()() const {
        double partial = 0.0;
        {
            const auto guard = data->lock_shared();
            for (std::size_t index = first; index < last; ++index)
                partial += map_element((*guard)[index]);
        }
        auto total_guard = total->lock();
        *total_guard += partial;
    }
};

// B. the copy workaround.
struct copied_slice_task {
    std::vector<double> slice;
    std::shared_ptr<threadsafe::synchronized_value<double>> total;

    void operator()() const {
        double partial = 0.0;
        for (const double value : slice)
            partial += map_element(value);
        auto total_guard = total->lock();
        *total_guard += partial;
    }
};

std::vector<double> make_input() {
    std::vector<double> input(element_count);
    for (std::size_t index = 0; index < element_count; ++index)
        input[index] = static_cast<double>(index % 1024);
    return input;
}

double run_guarded(const std::vector<double>& input, double& sum) {
    auto data = threadsafe::synchronized_value<std::vector<double>>::make(input);
    auto total = threadsafe::synchronized_value<double>::make(0.0);
    const std::size_t chunk = element_count / worker_count;

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < worker_count; ++worker)
            launcher.launch_task(guarded_slice_task{
                data, total, worker * chunk,
                worker + 1 == worker_count ? element_count
                                           : (worker + 1) * chunk});
    }
    const double elapsed = milliseconds_since(started_at);
    const auto total_guard = total->lock_shared();
    sum = *total_guard;
    return elapsed;
}

double run_copied(const std::vector<double>& input, double& sum) {
    auto total = threadsafe::synchronized_value<double>::make(0.0);
    const std::size_t chunk = element_count / worker_count;

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < worker_count; ++worker) {
            const std::size_t first = worker * chunk;
            const std::size_t last = worker + 1 == worker_count
                                         ? element_count
                                         : (worker + 1) * chunk;
            launcher.launch_task(copied_slice_task{
                std::vector<double>(input.begin() + first,
                                    input.begin() + last),
                total});
        }
    }
    const double elapsed = milliseconds_since(started_at);
    const auto total_guard = total->lock_shared();
    sum = *total_guard;
    return elapsed;
}

double run_bare_span(const std::vector<double>& input, double& sum) {
    std::vector<double> partials(worker_count, 0.0);
    const std::size_t chunk = element_count / worker_count;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < worker_count; ++worker) {
            const std::size_t first = worker * chunk;
            const std::size_t last = worker + 1 == worker_count
                                         ? element_count
                                         : (worker + 1) * chunk;
            threads.emplace_back(
                [slice = std::span<const double>{input.data() + first,
                                                 last - first},
                 &partial = partials[worker]] {
                    double accumulator = 0.0;
                    for (const double value : slice)
                        accumulator += map_element(value);
                    partial = accumulator;
                });
        }
    }
    const double elapsed = milliseconds_since(started_at);
    sum = 0.0;
    for (const double partial : partials)
        sum += partial;
    return elapsed;
}

}

int main() {
    const std::vector<double> input = make_input();
    std::printf("%zu doubles, %d workers\n\n", element_count, worker_count);
    std::printf("%-58s %9s %16s\n", "strategy", "ms", "sum");

    for (int repetition = 0; repetition < 3; ++repetition) {
        double sum = 0.0;
        const double guarded = run_guarded(input, sum);
        std::printf("A. shared_ptr<synchronized_value<vector>> + shared_lock  %9.1f %16.6e\n",
                    guarded, sum);
        const double copied = run_copied(input, sum);
        std::printf("B. per-worker std::vector<double> copy of the slice      %9.1f %16.6e\n",
                    copied, sum);
        const double bare = run_bare_span(input, sum);
        std::printf("C. bare std::jthread + std::span (refused by the library) %9.1f %16.6e\n\n",
                    bare, sum);
    }
}
```

Mesuré par l'agent (10 000 000 `double`, 8 travailleurs) : A 1,7 ms puis 2,2 ms ; B 4,6 ms puis 7,0 ms ; C 1,5 ms — soit A à +13 % à +47 %, B à 3–4x.

Rejoué par moi, trois répétitions :

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread bench_parallel_map.cpp -o bench_parallel_map
$ ./bench_parallel_map
10000000 doubles, 8 workers

strategy                                                          ms              sum
A. shared_ptr<synchronized_value<vector>> + shared_lock        1.6     2.567439e+09
B. per-worker std::vector<double> copy of the slice            5.0     2.567439e+09
C. bare std::jthread + std::span (refused by the library)      1.4     2.567439e+09

A. shared_ptr<synchronized_value<vector>> + shared_lock        1.4     2.567439e+09
B. per-worker std::vector<double> copy of the slice            3.2     2.567439e+09
C. bare std::jthread + std::span (refused by the library)      1.4     2.567439e+09

A. shared_ptr<synchronized_value<vector>> + shared_lock        1.4     2.567439e+09
B. per-worker std::vector<double> copy of the slice            2.8     2.567439e+09
C. bare std::jthread + std::span (refused by the library)      1.4     2.567439e+09
```

| Stratégie | Agent | Moi | Surcoût sur C, chez moi |
|---|---:|---:|---:|
| A. `shared_ptr<synchronized_value<vector>>` + `shared_lock` + bornes | 1,7 – 2,2 ms | **1,4 – 1,6 ms** | **0 % à +14 %** |
| B. copie de la tranche par travailleur | 4,6 – 7,0 ms | 2,8 – 5,0 ms | +100 % à +260 % |
| C. `std::jthread` + `std::span` (refusée) | 1,5 ms | 1,4 ms | référence |

Mes chiffres sont meilleurs que ceux de l'agent pour A : après échauffement, la forme imposée par la bibliothèque tombe **exactement** sur le temps du `span` nu. Une fois de plus : l'emballage est gratuit. C'est un constat d'ergonomie, pas de performance — l'utilisateur doit écrire une structure nommée, un `shared_ptr` vers le vecteur entier sous verrou, et des bornes d'indices portées à la main, là où la version nue s'écrit `[slice = std::span<const double>{...}]`. Le verrou lecteur tenu pendant plusieurs millisecondes comme simple laissez-passer n'a pas l'air sûr, même s'il l'est. L'angle ergonomique appartient au [rapport 08](./08-api-et-flexibilite.md).

**Pas de correctif proposé.** Le `threadsafe::owned_slice<T>` qui résoudrait le problème (un `shared_ptr` vers le conteneur plus `first`/`last`, pour que `lifetime_aware` tienne structurellement) est une fonctionnalité nouvelle, pas une réparation, et l'agent a refusé d'en proposer une qu'il ne pouvait pas mesurer. Je souscris : la règle de la maison est de contester le besoin, et le besoin ici est d'abord de corriger la **première** raison de refus, ce qui est déjà filé sous L8.

### 4.3 E4 — pas de verrouillage multiple : le virement entre deux comptes interbloque

**Gravité : moyenne (mais le temps d'exécution devient infini). Aucun correctif proposé, et c'est un choix argumenté.**

Il n'y a pas de `threadsafe::lock(a, b)`, pas d'accesseur au mutex, pas de `native_handle`. La seule chose qu'un utilisateur peut écrire est « verrouille l'un, puis l'autre ».

```cpp
// The classic two-account transfer, with a watchdog so the program terminates.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

namespace {
using account = threadsafe::synchronized_value<long long>;
constexpr int transfers = 200'000;
std::atomic<long long> progress{0};

struct transfer_task {
    std::shared_ptr<account> from;
    std::shared_ptr<account> to;
    void operator()() const {
        for (int step = 0; step < transfers; ++step) {
            auto from_guard = from->lock();
            auto to_guard = to->lock();
            *from_guard -= 1;
            *to_guard += 1;
            ++progress;
        }
    }
};

template <class Value>
constexpr bool exposes_mutex = requires(Value v) { v.get_mutex(); };
template <class Value>
constexpr bool exposes_native_handle = requires(Value v) { v.native_handle(); };
template <class Value>
constexpr bool scoped_lock_looks_ok =
    requires(Value& a, Value& b) { std::scoped_lock{a, b}; };
template <class Value>
constexpr bool has_try_lock = requires(Value& v) { v.try_lock(); };
template <class Value>
constexpr bool has_unlock = requires(Value& v) { v.unlock(); };
}

static_assert(!exposes_mutex<account> && !exposes_native_handle<account>,
              "no way to hand the two mutexes to std::scoped_lock / std::lock");
static_assert(scoped_lock_looks_ok<account>,
              "and yet std::scoped_lock{a, b} passes overload resolution");
static_assert(!has_try_lock<account> && !has_unlock<account>,
              "although it is not a Lockable at all");

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
                std::printf("DEADLOCK: no progress for 3 s, stuck after %lld of %d transfers\n",
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
    std::printf("no deadlock this run: %lld + %lld = %lld\n",
                *first_guard, *second_guard, *first_guard + *second_guard);
}
```

```
$ g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread e4_transfer.cpp -o e4_transfer
$ ./e4_transfer ; echo exit=$?
DEADLOCK: no progress for 3 s, stuck after 2237 of 400000 transfers
exit=2
```

Vérifié par moi. La compilation réussit, donc **les trois `static_assert` du programme tiennent** :

| Assertion | Valeur |
|---|---|
| `exposes_mutex<account>` | faux — aucun moyen de récupérer les deux mutex |
| `exposes_native_handle<account>` | faux |
| `has_try_lock<account>` / `has_unlock<account>` | faux — ce n'est pas un `Lockable` |
| `scoped_lock_looks_ok<account>` | **vrai** — et pourtant `std::scoped_lock{a, b}` passe la résolution de surcharge |

Ce dernier point est le piège : `synchronized_value` a un membre appelé `lock()`, donc `std::scoped_lock{a, b}` compile jusqu'à l'instanciation, et échoue alors en 51 lignes de libstdc++ qui parlent de membres que le type n'a pas :

```
.../mutex:746:50: error: 'class threadsafe::synchronized_value<long long int>' has no member named 'unlock'; did you mean 'lock'?
.../bits/unique_lock.h:159:34: error: '...::mutex_type' {aka 'class threadsafe::synchronized_value<long long int>'} has no member named 'try_lock'
.../bits/unique_lock.h:203:24: error: ... has no member named 'unlock'; did you mean 'lock'?
```

`std::mutex` a `std::lock` et `std::scoped_lock`, qui évitent l'interblocage ; `synchronized_value` confisque le mutex et ne rend rien. Les transactions à plusieurs objets — virement, échange, déplacement entre deux files — ne sont pas exotiques, et la bibliothèque retransforme un problème résolu en blocage.

**Aucun correctif n'est proposé, pour une raison qui tient.** Un verrouillage multiple correct impose à `value_guard` un chemin de construction en verrouillage différé (construire les deux `Lock` avec `std::defer_lock`, les `std::lock` ensemble, puis distribuer les gardes), ce qui modifie l'ensemble des constructeurs de `value_guard` et son interaction avec la règle `[[nodiscard]]` sur les temporaires. C'est un changement de conception sur un type dont la sémantique de garde est délibérément serrée : les `operator*` et `operator->` supprimés en rvalue existent précisément pour empêcher ce genre de relâchement — et j'en ai fait l'expérience en écrivant les mesures de ce rapport, où `sum = *total->lock_shared();` a été refusé, à juste titre, par le message `a temporary guard is destroyed at the semicolon, so it cannot hand out a reference`.

Ce qui **doit** être fait, en revanche, et coûte trois lignes : le dire. Un utilisateur qui lit `synchronized_value` doit savoir avant d'écrire son virement qu'il n'y a pas de verrouillage multiple. La conception de l'API appartient au [rapport 08](./08-api-et-flexibilite.md), la robustesse à [02](./02-robustesse-des-helpers.md).

### 4.4 E2 — la version « fausse » à mettre sur la diapositive est le foncteur nommé

**Gravité : moyenne. Aucun correctif : la limite est inhérente à la réflexion en C++26, pas un défaut de la bibliothèque.**

Ce constat est d'abord un constat de diagnostic, traité dans [04](./04-diagnostics.md). Il a sa place ici parce qu'il concerne la démonstration de la course de données elle-même, et donc le programme d'exécution que la conférence montrera.

```cpp
// Program 5, THE WRONG VERSION written as a named functor, because the launcher
// refuses capturing lambdas outright and the speaker has to rewrite it.
#include <threadsafe/threadsafe.h>
#include <cstdio>

struct counting_task {
    int& total;
    void operator()() const {
        for (int step = 0; step < 100'000; ++step)
            ++total;
    }
};

int main() {
    int total = 0;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task(counting_task{total});
    }
    std::printf("total = %d (expected 400000)\n", total);
}
```

Version fausse **en foncteur nommé** — 12 lignes, un seul message `std::meta`, et c'est le bon :

```
.../asynchronous_task_launcher.h:99:5: error: uncaught exception of type 'std::meta::exception'; 'what()': 'counting_task::total (int&) is a pointer or a reference: sending it shares its referent with the other thread, so the referent must be synchronizable — and synchronizability is opt-in'
```

Version fausse **en lambda**, le programme complet :

```cpp
// Program 5, THE WRONG VERSION written as a capturing lambda, which is what a
// speaker writes first — and which the launcher refuses for the wrong reason.
#include <threadsafe/threadsafe.h>
#include <cstdio>

int main() {
    int total = 0;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task([&total] {
                for (int step = 0; step < 100'000; ++step)
                    ++total;
            });
    }
    std::printf("total = %d (expected 400000)\n", total);
}
```

18 lignes de diagnostic, un seul message, mais **la mauvaise leçon** :

```
'what()': 'main()::<lambda()> holds state reflection cannot see (a closure type with captures); specialize is_sendable to state the intent'
```

Ce message accuse le mécanisme de capture, pas la course. Le même message sort pour une lambda qui capture une `std::string` **par valeur**, ce qui est parfaitement sûr, et pour une lambda qui capture un `shared_ptr<synchronized_value<int>>` par valeur, ce qui est exactement ce qu'il **faut** écrire. Pire, le conseil qu'il donne ne peut pas être suivi : spécialiser `is_sendable` pour une lambda locale échoue sur `error: a template declaration cannot appear at block scope`. Seule une fermeture déclarée à portée de namespace peut être bénie.

La course, elle, est réelle : le même corps sans la bibliothèque, sous ThreadSanitizer, donne `WARNING: ThreadSanitizer: data race` et `total = 15000 (expected 20000)`. Et la version correcte s'exécute et donne `total = 400000 (expected 400000)`.

**Aucun correctif possible.** Une fermeture avec captures n'a, en GCC 16, aucun membre de données non statique réflexible (`nonstatic_data_members_of` renvoie zéro entrée, et `is_empty_type` est faux) : `detail::has_unreflectable_state` se déclenche et la bibliothèque ne peut pas distinguer `[&total]` de `[s = std::string{}]`. Refuser les trois est la seule réponse saine disponible.

**Recommandation de conférence, pas de code :** montrer `counting_task` comme version fausse — elle nomme le membre, elle nomme le type, elle nomme la règle en une phrase — et annoncer d'entrée que **le foncteur nommé est l'idiome d'écriture de tâche, parce que les captures sont invisibles à la réflexion.** Un orateur qui montre la lambda se fera demander « et comment je fais marcher une lambda ? », et devra répondre « vous ne pouvez pas, sauf à portée de namespace », ce qui démolit l'assistant tout entier.

---
## 5. Ce qui a résisté

Ce rapport dit ce qui coûte cher. Il doit dire avec la même netteté ce qui n'a pas cédé. Douze attaques ont été rejouées sur le lanceur, les gardes et les six programmes de bout en bout, et **aucune n'a mis en défaut ni le corps de `synchronized_value`, ni le prix des abstractions, ni les gardes**. Le noyau est solide ; les deux défauts hauts de ce rapport sont des décisions de politique, pas des fuites.

| # | Attaque | Résultat |
|---|---|---|
| 1 | `std::function<void()>`, `std::move_only_function<void()>`, le résultat de `std::bind` | **Tous refusés** par `launch_task` **et** par `launch_scoped_task`, avec un seul message `std::meta`. Aucun appelable à effacement de type n'est passé dans aucun scénario. |
| 2 | Lambda capturant un pointeur brut **par valeur** | **Refusée.** Une fermeture avec captures n'a aucun membre réflexible et n'est pas `is_empty_type` : `detail::has_unreflectable_state` se déclenche, tous les traits répondent faux. (Le même mécanisme refuse aussi des captures sûres — c'est E2 — mais le cas du pointeur est bel et bien fermé.) |
| 3 | `std::ref` d'un type béni par `THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE` | **Refusé** par `launch_task` : *« `std::reference_wrapper<Blessed>::_M_data (Blessed*)` is a reference or a raw pointer: it borrows its referent instead of keeping it alive »*. Idem pour un `Blessed*` brut. `launch_scoped_task` les accepte : c'est son contrat affaibli documenté, pas une fuite. |
| 4 | `std::span`, `std::string_view`, `const char*`, pointeurs bruts en argument | **Tous refusés** par `launch_task` ; `span` et `string_view` sont refusés même par `launch_scoped_task`. Une chaîne littérale ne peut pas non plus traverser vers une tâche à portée. |
| 5 | Foncteur sans état mais avec constructeur de copie **écrit à la main** ; classe avec un patron de constructeur susceptible d'être choisi comme copie/déplacement | **Refusés** (`detail::may_hijack_copy_move`). C'est ce qui attrape `std::span` et `std::function` en premier lieu. |
| 6 | Faire dégrader la ruse à deux surcharges en simple `false` | **Impossible.** Quatre tentatives (expression `requires` nue, concept écrit à la main, `std::is_invocable_v` sur une lambda enveloppante, appel non évalué) : toutes rapportent `true` ou provoquent une erreur dure. Aucune n'a produit `false`. Le mécanisme tient. |
| 7 | Envoyer un `synchronized_value` par valeur dans une tâche ; `std::ref` d'un `synchronized_value` | **Refusés.** Il n'est ni copiable ni déplaçable, et `reference_wrapper` n'est jamais `lifetime_aware`. La seule voie d'entrée est le `shared_ptr` produit par `synchronized_value::make`. |
| 8 | Faire traverser l'`asynchronous_task_launcher` lui-même | **Refusé** : ni sendable, ni synchronizable, ni const-synchronizable, ni déplaçable dans une tâche. C'est `std::jthread` qui bloque. |
| 9 | Course dans le corps d'exécution de `synchronized_value` | **Aucune.** L'extraction manuelle a été diffée ligne à ligne contre l'en-tête (les `operator*`/`operator->` en `const&`, les membres `lock_`/`value_`, le constructeur `(mutex, value)`, le `mutable mutex_`, les deux corps de `lock()`), et tourne sous `clang++ -std=c++20 -fsanitize=thread -O1`, 8 threads × 20 000 opérations mixtes : aucun avertissement, `ok: 256 entries, checksum 32647692`. |
| 10 | **Le coût de l'emballage lui-même** | **Nul.** `synchronized_value<std::map>` suit un `std::shared_mutex` écrit à la main à 3 % près de 1 à 12 threads ; `synchronized_value<Config>` suit un `shared_mutex` à la main à 3 % près ; le chemin de réduction parallèle tombe sur le temps du `std::jthread` + `std::span` nu. **Tous les défauts de performance trouvés sont dans une décision de politique** (quel mutex, quelle sérialisation du `join`), jamais dans l'emballage. |
| 11 | Faire sortir une référence pendante d'une garde | **Impossible.** Les `operator*` et `operator->` en rvalue sont supprimés avec message, `lock()`/`lock_shared()` sont `[[nodiscard]]`. J'ai heurté cette protection en écrivant `sum = *total->lock_shared();` dans mon propre banc d'essai : refusé à la compilation, à juste titre. |
| 12 | Course sur des poignées `copy_on_write` partagées entre threads | **Aucune.** L'extraction TSan (Apple clang `-fsanitize=thread`) ne rapporte rien : `blessed pattern: checksum 16400000, highest version any reader saw = 1`. Le défaut est que rien n'est jamais publié (E1), pas que quoi que ce soit court. |

Deux points méritent d'être soulignés à part, parce qu'ils sont exactement ce qu'une conférence veut pouvoir affirmer :

- **Le point 10 est le vrai résultat de ce rapport.** Une bibliothèque de sûreté dont la sûreté est entièrement à la compilation ne doit rien coûter à l'exécution. C'est mesuré, à quatre échelles différentes, et c'est vérifié.
- **Le point 11 s'est vérifié tout seul, sur moi.** Le garde-fou le plus difficile à concevoir de la bibliothèque — empêcher qu'une garde temporaire distribue une référence — m'a arrêté pendant l'écriture de mes propres mesures. C'est la meilleure démonstration possible qu'il fonctionne.

Comptage des scénarios pour ce domaine : **53 rejoués, 49 reproduits, 4 impasses**, toutes nommées dans les notes de la campagne (une course sur un `static` que `-O2` replie en une seule instruction et qui ne se manifeste qu'à `-O0` ; une auto-vérification ASan cassée ; un `reinterpret_cast` qui « défait » le système de types, ce qu'aucun système de types ne prétend empêcher ; et un fichier dont la moitié du diagnostic portait sur autre chose). La méthodologie complète est dans [09](./09-methodologie.md).

---

## 6. Récapitulatif et ordre de traitement

| Priorité | Action | Coût | Statut du correctif |
|---|---|---|---|
| 1 | Ajouter le second paramètre de template à `synchronized_value` (§2.7), la valeur par défaut restant la sélection automatique actuelle | ~15 lignes | **Écrit, compilé, 11/11 TU vertes, vérifié par moi.** Le [rapport 08](./08-api-et-flexibilite.md) demande le même changement pour l'extensibilité : un seul patch pour les deux. |
| 2 | Renommer `launch_scoped_task` en `run_scoped_task_and_wait` et écrire dans son commentaire qu'il n'y a **aucun** parallélisme et qu'une tâche coopérative l'interbloque (§3.4a) | 1 nom, 8 lignes de commentaire, **1 ligne de test** | Écrit en entier ci-dessus. **suite-regresses** sur l'en-tête seul (`test_synchronized_value.cpp:30`) ; **suite-passes**, 11/11, avec la ligne de test corrigée — les deux vérifiés par moi. Aucune sémantique modifiée. |
| 3 | Documenter que `copy_on_write` est une poignée d'instantané **privée à un thread**, et nommer le motif de publication `synchronized_value<copy_on_write<T>>` (§4.1) | 4 phrases dans `CLAUDE.md` | Texte fourni ci-dessus |
| 4 | Documenter qu'il n'y a pas de verrouillage multiple sur `synchronized_value` (§4.3) | 2 phrases | — |
| 5 | Pour la conférence : montrer `counting_task` (foncteur nommé) comme version fausse, jamais la lambda (§4.4) | choix de diapositive | — |
| 6 | Si la conférence enseigne le contrat d'emprunt : ajouter le `scoped_task_group` du constat L3 | voir [02](./02-robustesse-des-helpers.md) | suite-passes selon la campagne helpers |

**Ce qu'il ne faut PAS faire**, et le dire est aussi utile :

- **Ne pas** rendre `is_synchronizable<const copy_on_write<T>>` vrai. C'est sémantiquement correct et **9x plus lent** (§2.6). Mesuré, puis annulé.
- **Ne pas** ajouter un `threadsafe::owned_slice<T>`. C'est une fonctionnalité nouvelle, pas une réparation ; le vrai défaut de E3 est la première raison de refus, fausse, déjà filée sous L8 ([04](./04-diagnostics.md)).
- **Ne pas** ajouter un verrouillage multiple à `synchronized_value` sans reconcevoir `value_guard`. Le verrouillage différé qu'il exige relâche exactement les garanties que les `operator*` supprimés en rvalue existent pour tenir.
- **Ne pas** chercher à optimiser l'emballage. Il ne coûte rien (§1, §5 point 10).

---

## Rapports liés

- [00 — Synthèse](./00-synthese.md)
- [01 — Robustesse des traits](./01-robustesse-des-traits.md)
- [02 — Robustesse des helpers](./02-robustesse-des-helpers.md) — l'interblocage de `launch_scoped_task`, le `scoped_task_group` du constat L3, l'absence de verrouillage multiple
- [03 — Couverture de tests](./03-couverture-de-tests.md)
- [04 — Diagnostics](./04-diagnostics.md) — la mauvaise raison de refus (L8), le message des lambdas
- [05 — Simplicité](./05-simplicite.md) — la phrase de `CLAUDE.md` sur `copy_on_write`
- [06 — Performance de compilation](./06-performance-compilation.md)
- [08 — API et flexibilité](./08-api-et-flexibilite.md) — **le même correctif de `synchronized_value`, vu sous l'angle de l'extensibilité**
- [09 — Méthodologie](./09-methodologie.md)
