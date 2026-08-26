#include <threadsafe/threadsafe.h>
struct ZeroSizedArray { int value_; int tail_[0]; };
static_assert((threadsafe::assert_synchronizable<const ZeroSizedArray>(), true));
