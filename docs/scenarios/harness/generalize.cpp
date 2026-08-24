#include <threadsafe/threadsafe.h>
#include <cstdio>

// (A) a namespace-scope global, touched by an EMPTY class callable.
long g_counter = 0;

struct Bumper {
    long operator()() const { return ++g_counter; }   // empty class: no members
};

// (B) a captureless lambda doing exactly the same thing.
using CapturelessBumper = decltype([] { return ++g_counter; });

// (C) an empty class touching its OWN function-local static.
struct StaticBumper {
    long operator()() const { static long n = 0; return ++n; }
};

// (D) the closure the claim calls "the same state": a per-object counter.
using Counting = decltype([n = 0L]() mutable { return ++n; });

static_assert(threadsafe::is_sendable_v<Bumper>);
static_assert(threadsafe::is_lifetime_aware_v<Bumper>);
static_assert(threadsafe::launchable_task<Bumper>);

static_assert(threadsafe::is_sendable_v<CapturelessBumper>);
static_assert(threadsafe::launchable_task<CapturelessBumper>);

static_assert(threadsafe::is_sendable_v<StaticBumper>);
static_assert(threadsafe::launchable_task<StaticBumper>);

static_assert(!threadsafe::is_sendable_v<Counting>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(Bumper{});
    launcher.launch_task(CapturelessBumper{});
    launcher.launch_task(StaticBumper{});
    launcher.launch_task(StaticBumper{});
    std::printf("ok %ld\n", g_counter);
}
