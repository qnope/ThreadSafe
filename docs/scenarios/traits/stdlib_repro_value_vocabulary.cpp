#include <threadsafe/threadsafe.h>

#include <bitset>
#include <chrono>
#include <complex>
#include <expected>
#include <queue>
#include <stack>
#include <string>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// Every one of these is a pure value: a long long, two doubles, a word array,
// a union of two values, a deque held by value. None of them can share
// anything with the thread it is sent to. All are refused.
static_assert(!is_sendable_v<std::chrono::milliseconds>, "OBSERVED");
static_assert(!is_sendable_v<std::chrono::steady_clock::time_point>, "OBSERVED");
static_assert(!is_sendable_v<std::complex<double>>, "OBSERVED");
static_assert(!is_sendable_v<std::bitset<64>>, "OBSERVED");
static_assert(!is_sendable_v<std::expected<int, std::string>>, "OBSERVED");
static_assert(!is_sendable_v<std::stack<int>>, "OBSERVED");
static_assert(!is_sendable_v<std::queue<std::string>>, "OBSERVED");
static_assert(!is_sendable_v<std::priority_queue<int>>, "OBSERVED");

static_assert(!is_synchronizable_v<const std::chrono::milliseconds>, "OBSERVED");
static_assert(!is_synchronizable_v<const std::bitset<64>>, "OBSERVED");
static_assert(!is_synchronizable_v<const std::stack<int>>, "OBSERVED");

// ... yet is_lifetime_aware, which does not run the copy/move guard, happily
// accepts the very same types: two routes to the same family disagree.
static_assert(threadsafe::is_lifetime_aware_v<std::chrono::milliseconds>);
static_assert(threadsafe::is_lifetime_aware_v<std::stack<int>>);
static_assert(threadsafe::is_lifetime_aware_v<std::complex<double>>);

using namespace std::chrono_literals;
void poll_for(std::chrono::milliseconds budget) { (void)budget; }

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&poll_for, 100ms);   // <- refused
}
