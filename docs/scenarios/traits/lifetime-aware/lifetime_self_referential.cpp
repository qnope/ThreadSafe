#include <threadsafe/threadsafe.h>

#include <memory>
#include <vector>

struct TreeNode {
    int value;
    std::vector<TreeNode> children;
};

static_assert(threadsafe::is_lifetime_aware_v<TreeNode>,
              "a tree that owns its children is lifetime aware");

int main() { return 0; }
