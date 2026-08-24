#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <array>
#include <cstdio>
#include <thread>
#include <tuple>
#include <utility>

namespace {

constexpr int repetitions = 31;
constexpr long long operations = 500'000;

template <std::size_t Bytes>
struct heavy_payload {
    std::array<char, Bytes> storage{};
};

std::array<char, 65536> global_sink{};

template <std::size_t Bytes>
[[gnu::noinline]] void store_it(heavy_payload<Bytes> payload) {
    global_sink[0] = payload.storage[0];
    global_sink[1] = payload.storage[Bytes - 1];
    bench::clobber();
}

// exactly what launch_task does: parameter by value, then moved on
template <std::size_t Bytes>
[[gnu::noinline]] void by_value_then_move(heavy_payload<Bytes> payload) {
    store_it<Bytes>(std::move(payload));
}

template <std::size_t Bytes>
void run_pair(const char* size_label) {
    heavy_payload<Bytes> argument{};
    argument.storage[0] = 1;
    argument.storage[Bytes - 1] = 2;

    char label[128];
    std::snprintf(label, sizeof(label),
                  "%s  by-value param then move  (launch_task shape)",
                  size_label);
    bench::report(label, bench::measure(repetitions, operations, [&] {
                      for (long long i = 0; i < operations; ++i)
                          by_value_then_move<Bytes>(argument);
                  }));

    std::snprintf(label, sizeof(label),
                  "%s  direct call, one copy     (forwarding shape)",
                  size_label);
    bench::report(label, bench::measure(repetitions, operations, [&] {
                      for (long long i = 0; i < operations; ++i)
                          store_it<Bytes>(argument);
                  }));
}

}

int main() {
    std::printf("cost of the launcher's extra copy: `void launch_task(F f, "
                "Args... args)` copies the\nargument into the parameter, then "
                "moves it again into the thread's tuple.\n\n");
    run_pair<64>("   64 B");
    run_pair<1024>(" 1024 B");
    run_pair<4096>(" 4096 B");
    run_pair<65536>("65536 B");
    std::printf("\nfor scale: one std::jthread spawn measured at ~10500 ns on "
                "this machine.\n");
    bench::do_not_optimize(global_sink[0]);
}
