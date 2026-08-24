// A shape every C++ codebase has: a node type forward-declared in the header,
// owned through a unique_ptr, all special members implicit.
#include <threadsafe/threadsafe.h>

#include <memory>

namespace app {

struct node;

struct tree {
    std::unique_ptr<node> root;
};

}

static_assert(!threadsafe::is_sendable_v<app::tree>,
              "an incomplete node cannot be judged, so the answer must be no");
