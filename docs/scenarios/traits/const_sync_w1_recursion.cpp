#include <threadsafe/threadsafe.h>
#include <vector>
struct TreeNode { int value_; std::vector<TreeNode> children_; };
static_assert(!threadsafe::is_synchronizable_v<const TreeNode>, "TreeNode TRUE");
