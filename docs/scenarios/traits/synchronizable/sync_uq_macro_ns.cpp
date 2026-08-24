#include <threadsafe/threadsafe.h>
namespace app {
struct Widget { int* p; };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(app::Widget);
}
