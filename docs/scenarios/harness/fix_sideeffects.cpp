#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>
using threadsafe::is_lifetime_aware_v;
struct Node { std::shared_ptr<Node> next; int v; };          // self-referential
struct Incomplete;
struct Pimpl { std::shared_ptr<Incomplete> impl; };
static_assert(is_lifetime_aware_v<std::shared_ptr<void>>, "shared_ptr<void>");
static_assert(is_lifetime_aware_v<Node>, "self-referential node still owns");
static_assert(is_lifetime_aware_v<std::shared_ptr<Node>>, "shared_ptr<Node>");
static_assert(is_lifetime_aware_v<Pimpl>, "pimpl via shared_ptr to incomplete");
static_assert(is_lifetime_aware_v<std::shared_ptr<threadsafe::synchronized_value<std::string>>>,
              "the intended shape stays accepted");
int main() {}
