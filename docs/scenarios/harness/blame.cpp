#include <threadsafe/threadsafe.h>
#include <bitset>
#include <chrono>
#include <complex>
#include <cstdio>
#include <meta>
#include <valarray>
using namespace std::meta;
// Ask the guard directly, isolating it from the rest of the walk.
template <class T> void blame(const char* n) {
    constexpr bool guard_ok = threadsafe::detail::has_only_default_copy_move_destroy(^^T);
    std::printf("  %-42s passes copy/move guard: %s\n", n, guard_ok ? "yes" : "NO");
}
int main() {
    blame<std::bitset<8>>("std::bitset<8>");
    blame<std::complex<double>>("std::complex<double>");
    blame<std::chrono::milliseconds>("std::chrono::milliseconds");
    blame<std::valarray<int>>("std::valarray<int>");
}
