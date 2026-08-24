#include <threadsafe/threadsafe.h>
#include <cstdio>

// No static data member: the shared object is a function-local static.
struct LocalStaticCounter {
    int weight = 1;
    long long *sink;
    void operator()() const {
        static long long total = 0;
        for (int iteration = 0; iteration < 200000; ++iteration)
            total += weight;
        *sink = total;
    }
};
