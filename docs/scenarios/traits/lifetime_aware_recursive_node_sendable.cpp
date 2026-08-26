#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>

struct TreeNode {
    int value;
    std::vector<std::unique_ptr<TreeNode>> children;
};

static_assert(threadsafe::is_sendable_v<TreeNode>, "a tree node is sendable");
int main() {}
