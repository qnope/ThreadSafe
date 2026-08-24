#include <threadsafe/threadsafe.h>

#include <memory>
#include <vector>

struct TreeNode {
    int value;
    std::vector<TreeNode> children;
};

static_assert(threadsafe::is_sendable_v<TreeNode>,
              "sendable copes with the same self-referential shape");

int main() { return 0; }
