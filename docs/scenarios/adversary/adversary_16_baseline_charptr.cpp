#include <threadsafe/threadsafe.h>
struct HoldsCharPtr { const char* name; };
static_assert(!threadsafe::is_sendable_v<HoldsCharPtr>,
              "the UNPATCHED library already refuses a const char* member");
static_assert(!threadsafe::is_synchronizable_v<const HoldsCharPtr>);
