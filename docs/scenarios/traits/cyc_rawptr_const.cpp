#include <threadsafe/threadsafe.h>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <utility>
struct N { int v_; N* next_; };
static_assert(!threadsafe::is_synchronizable_v<const N>, "TRUE");
