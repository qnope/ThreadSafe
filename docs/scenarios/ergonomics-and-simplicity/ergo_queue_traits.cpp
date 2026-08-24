// (c) is the obvious producer/consumer buffer even a legal T?
#include <threadsafe/threadsafe.h>

#include <deque>
#include <queue>
#include <stack>
#include <vector>

using threadsafe::is_sendable_v;

static_assert(is_sendable_v<std::deque<int>>);
static_assert(!is_sendable_v<std::queue<int>>, "std::queue is refused");
static_assert(!is_sendable_v<std::stack<int>>, "std::stack is refused");
static_assert(!is_sendable_v<std::priority_queue<int>>,
              "std::priority_queue is refused");
int main() {}
