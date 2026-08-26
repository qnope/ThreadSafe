#include <threadsafe/threadsafe.h>
#include <string>

namespace acme {
struct Widget { std::string name; ~Widget() {} };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Widget);
}
