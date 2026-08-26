#include <threadsafe/threadsafe.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
consteval bool probe() { threadsafe::assert_sendable<std::reference_wrapper<int>>(); return true; }
static_assert(probe());
