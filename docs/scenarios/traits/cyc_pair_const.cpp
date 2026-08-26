#include <threadsafe/threadsafe.h>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <utility>
struct N; struct N { int v_; std::vector<std::pair<int,N>> kids_; };
static_assert(!threadsafe::is_synchronizable_v<const N>, "TRUE");
