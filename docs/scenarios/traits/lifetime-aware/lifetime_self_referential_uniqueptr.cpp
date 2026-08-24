#include <threadsafe/threadsafe.h>

#include <memory>

struct ListNode {
    int value;
    std::unique_ptr<ListNode> next;
};

static_assert(threadsafe::is_lifetime_aware_v<ListNode>,
              "a linked list that owns its tail is lifetime aware");

int main() { return 0; }
