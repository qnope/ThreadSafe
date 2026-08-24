// The realistic shape: a type that owns something declared later in the file
// (a tree node, a pimpl). Every special member is implicit.
#include <threadsafe/threadsafe.h>

#include <memory>

struct node;

struct tree {
    std::unique_ptr<node> root;
};

static_assert(!threadsafe::is_sendable_v<tree>);

struct node {
    int value;
    std::unique_ptr<node> next;
};

int main() {}
