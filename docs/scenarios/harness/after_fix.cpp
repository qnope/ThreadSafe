#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
#include <optional>
#include <atomic>

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

struct Body { std::string text; };
struct Document { copy_on_write<Body> body; int revision; };
struct Cache { int raw; mutable std::optional<int> parsed; };

// the claim's four blocked cases now hold
static_assert(is_synchronizable_v<const copy_on_write<Body>>);
static_assert(is_sendable_v<copy_on_write<copy_on_write<Body>>>);
static_assert(is_sendable_v<copy_on_write<Document>>);
static_assert(is_synchronizable_v<const std::vector<copy_on_write<Body>>>);

// and the guards still bite
static_assert(!is_synchronizable_v<copy_on_write<Body>>,
              "non-const handle still not shareable by reference");
static_assert(!is_synchronizable_v<const copy_on_write<int*>>,
              "borrowing payload still refused");
static_assert(!is_synchronizable_v<const copy_on_write<Cache>>,
              "const-mutating payload still refused");
static_assert(!is_sendable_v<copy_on_write<copy_on_write<Cache>>>);
static_assert(!is_sendable_v<copy_on_write<copy_on_write<int*>>>);
