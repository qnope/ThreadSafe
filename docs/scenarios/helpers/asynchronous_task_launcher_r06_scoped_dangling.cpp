#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <functional>
#include <memory>

namespace {
// A type the user vouched for, so std::ref of it may cross into a scoped task.
struct Reading { int millivolts = 0; };
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Reading);

namespace {
// launch_scoped_task's PRECONDITION is "f must not store a reference beyond the
// call". Nothing checks it, and nothing in the traits could: the borrow is
// stashed in a global the structural walk never inspects.
Reading *escaped_reading = nullptr;

struct StashesTheBorrow {
    void operator()(Reading &reading) const { escaped_reading = &reading; }
};

void publish_a_reading() {
    auto reading = std::make_unique<Reading>(Reading{.millivolts = 1234});
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(StashesTheBorrow{}, std::ref(*reading));
    std::printf("while it is alive: escaped_reading->millivolts = %d\n",
                escaped_reading->millivolts);
    // reading dies here; escaped_reading keeps pointing at the freed storage.
}
}

static_assert(threadsafe::launchable_scoped_task<StashesTheBorrow,
                                                 std::reference_wrapper<Reading>>,
              "launch_scoped_task accepts it: the precondition is unchecked");

int main() {
    publish_a_reading();

    // Reuse the freed storage.
    auto replacement = std::make_unique<Reading>(Reading{.millivolts = -9999});
    std::printf("a new Reading{-9999} was allocated at %s address\n",
                replacement.get() == escaped_reading ? "THE SAME" : "a different");
    std::printf("after the free:    escaped_reading->millivolts = %d\n",
                escaped_reading->millivolts);
    return escaped_reading->millivolts == 1234 ? 1 : 0;
}
