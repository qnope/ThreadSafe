#include <threadsafe/threadsafe.h>
#include <string>
struct Plain { int value; double ratio; };
using threadsafe::is_sendable_v; using threadsafe::is_synchronizable_v;
static_assert(is_synchronizable_v<const Plain>);       // a const Plain is read-safe
static_assert(!is_synchronizable_v<Plain>);
static_assert(is_sendable_v<const Plain&> == false, "const Plain& NOT sendable?");
static_assert(is_sendable_v<const Plain*> == false, "const Plain* NOT sendable?");
static_assert(is_sendable_v<const std::string&> == false, "const string& NOT sendable?");
