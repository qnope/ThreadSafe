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
using threadsafe::is_lifetime_aware_v;
using namespace std::chrono_literals;

static_assert(is_sendable_v<std::chrono::milliseconds>);
static_assert(is_sendable_v<std::chrono::steady_clock::time_point>);
static_assert(is_sendable_v<std::complex<double>>);
static_assert(is_sendable_v<std::bitset<64>>);
static_assert(is_sendable_v<std::expected<int, std::string>>);
static_assert(!is_sendable_v<std::expected<int*, int>>);
static_assert(is_sendable_v<std::stack<int>>);
static_assert(!is_sendable_v<std::stack<int*>>);
static_assert(is_sendable_v<std::queue<std::string>>);
static_assert(is_sendable_v<std::priority_queue<int>>);
static_assert(is_synchronizable_v<const std::chrono::milliseconds>);
static_assert(is_synchronizable_v<const std::bitset<64>>);
static_assert(is_synchronizable_v<const std::stack<int>>);
static_assert(!is_synchronizable_v<const std::stack<int*>>);
static_assert(is_lifetime_aware_v<std::chrono::milliseconds>);
static_assert(is_lifetime_aware_v<std::stack<int>>);
static_assert(!is_lifetime_aware_v<std::stack<int*>>);

void sleep_then(std::chrono::milliseconds delay) { (void)delay; }

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&sleep_then, 100ms);
}
