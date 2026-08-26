#include <threadsafe/threadsafe.h>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <utility>
struct B; struct A { std::vector<B> bs_; }; struct B { std::vector<A> as_; };
static_assert(!threadsafe::is_synchronizable_v<const A>, "TRUE");
