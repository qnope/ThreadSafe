#include <threadsafe/details/smart_pointers.h>
#include <memory>
struct Plain { int value; };
// synchronizable.h was never included: the const rule does not exist here.
static_assert(!threadsafe::is_synchronizable_v<const Plain>,
              "without synchronizable.h, const Plain silently answers FALSE");
