#include <threadsafe/threadsafe.h>
struct MutCache { int raw; mutable int parsed; };
consteval void explain() { threadsafe::assert_synchronizable<const MutCache>(); }
static_assert((explain(), true));
