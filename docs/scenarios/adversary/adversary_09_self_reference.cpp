#include <threadsafe/threadsafe.h>

// Asking a trait about a type from inside that type's own definition.
struct SelfAsking {
    int payload;
    static constexpr bool answer_in_class = threadsafe::is_sendable_v<SelfAsking>;
};

// After the class is complete, the cached answer is the in-class one.
static_assert(!SelfAsking::answer_in_class,
              "asked from inside its own definition, the type was incomplete");
static_assert(!threadsafe::is_sendable_v<SelfAsking>,
              "and the whole TU now believes a plain aggregate of ints is not "
              "sendable");

// A self-referential type: the walk must not loop.
struct Node {
    int value;
    Node* next;
};
static_assert(!threadsafe::is_sendable_v<Node>);

struct OwningNode {
    int value;
    std::unique_ptr<OwningNode> next;
};
