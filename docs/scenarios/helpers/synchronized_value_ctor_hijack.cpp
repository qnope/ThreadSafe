// The greedy variadic constructor is not guarded against a single argument of
// the wrapper's own type (copy_on_write is).  Against a *non-const* lvalue the
// template deduces Args = synchronized_value& and matches exactly, while the
// deleted copy constructor needs a qualification conversion -- so the template
// wins and silently builds a T out of the wrapper instead of reporting
// "synchronized_value is non-copyable".
#include <threadsafe/threadsafe.h>
#include <cstdio>

struct Ledger;
struct Ledger {
    int balance = 0;
    Ledger() = default;
    explicit Ledger(int starting_balance) : balance(starting_balance) {}
    // A perfectly ordinary converting constructor -- no template, so Ledger
    // stays sendable.
    Ledger(threadsafe::synchronized_value<Ledger>&) : balance(-1) {}
};

static_assert(threadsafe::is_sendable_v<Ledger>);

int main() {
    threadsafe::synchronized_value<Ledger> original{Ledger{100}};
    threadsafe::synchronized_value<Ledger> supposed_copy{original};  // compiles!

    const auto original_guard = original.lock_shared();
    const auto copy_guard = supposed_copy.lock_shared();
    std::printf("original balance = %d\n", original_guard->balance);
    std::printf("\"copy\" balance   = %d\n", copy_guard->balance);
    return 0;
}
