#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>

struct TreeNode {
    int value;
    std::vector<std::unique_ptr<TreeNode>> children;
};

static_assert(threadsafe::is_lifetime_aware_v<TreeNode>,
              "a tree node owns its children");
int main() {}
