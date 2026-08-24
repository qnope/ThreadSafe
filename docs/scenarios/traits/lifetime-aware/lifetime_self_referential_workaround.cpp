#include <threadsafe/threadsafe.h>

#include <vector>

struct TreeNode {
    int value;
    std::vector<TreeNode> children;
};

template <>
struct threadsafe::is_lifetime_aware<TreeNode> : std::true_type {};
template <>
struct threadsafe::is_sendable<TreeNode> : std::true_type {};

static_assert(threadsafe::is_lifetime_aware_v<TreeNode>);
static_assert(threadsafe::is_lifetime_aware_v<std::vector<TreeNode>>);

int main() { return 0; }
