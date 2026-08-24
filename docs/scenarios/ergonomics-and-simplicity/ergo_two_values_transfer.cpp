// The transfer: move money from one account to another. Two synchronized values,
// one operation. std::scoped_lock is the standard answer; the guards are the
// only way in, and each takes its own mutex the moment it is created.
#include <threadsafe/threadsafe.h>

#include <memory>
#include <print>

using account = threadsafe::synchronized_value<int>;

void transfer(account &from, account &to, int amount) {
    auto from_guard = from.lock();
    auto to_guard = to.lock();
    *from_guard -= amount;
    *to_guard += amount;
}

int main() {
    account first{100};
    account second{100};
    transfer(first, second, 30);
    std::println("{} {}", *first.lock(), *second.lock());
}
