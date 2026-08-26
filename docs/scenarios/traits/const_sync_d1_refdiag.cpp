#include <threadsafe/threadsafe.h>
#include <atomic>
struct Plain { int value_; };
static_assert((threadsafe::assert_synchronizable<const Plain &>(), true));
