// (c) attempt 2: found synchronized_value in the headers, wrap the queue in it.
#include <threadsafe/threadsafe.h>

#include <queue>

int main() {
    auto pending_items =
        threadsafe::synchronized_value<std::queue<int>>::make();
}
