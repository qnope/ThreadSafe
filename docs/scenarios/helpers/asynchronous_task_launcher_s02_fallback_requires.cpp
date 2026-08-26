#include <threadsafe/threadsafe.h>
#include <string>

namespace {
struct Bad { int* borrowed; void operator()() const {} };
}

// (a) Is the fallback visible to a requires-expression?
static_assert(!threadsafe::launchable_task<Bad>, "Bad is genuinely rejected by the concept");

static_assert(requires(threadsafe::asynchronous_task_launcher l) { l.launch_task(Bad{}); },
              "POLARITY-A: the requires-expression is satisfied even though the task is unsafe");
static_assert(requires(threadsafe::asynchronous_task_launcher l) { l.launch_scoped_task(Bad{}); },
              "POLARITY-A2");

// A user's own detection idiom therefore reports 'launchable'
template <class F, class... A>
concept looks_launchable =
    requires(threadsafe::asynchronous_task_launcher l, F f, A... a) { l.launch_task(f, a...); };

static_assert(looks_launchable<Bad>, "POLARITY-A3: detection idiom lies");
static_assert(looks_launchable<int>, "POLARITY-A4: even a non-callable 'looks launchable'");
static_assert(looks_launchable<std::string, int*>, "POLARITY-A5");

// (b) zero arguments is not ambiguous: overload resolution picks the constrained one
static_assert(requires(threadsafe::asynchronous_task_launcher l) { l.launch_task([]{}); });

int main() {}
