#include <threadsafe/threadsafe.h>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <utility>
struct N { int v_; std::vector<N> kids_; };
static_assert(!threadsafe::is_sendable_v<N>, "TRUE");
