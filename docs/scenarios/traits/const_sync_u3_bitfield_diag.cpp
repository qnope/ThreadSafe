#include <threadsafe/threadsafe.h>
struct UnnamedBitfield { int : 3; };
static_assert((threadsafe::assert_synchronizable<const UnnamedBitfield>(), true));
