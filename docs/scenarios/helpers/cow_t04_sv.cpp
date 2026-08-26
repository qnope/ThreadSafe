#include <threadsafe/threadsafe.h>
#include <shared_mutex>
#include <mutex>
#include <string>
#include <type_traits>

using threadsafe::copy_on_write;
using threadsafe::synchronized_value;

using SV = synchronized_value<copy_on_write<std::string>>;
static_assert(std::is_same_v<SV::mutex, std::mutex>,
              "a synchronized_value of a cow gets an EXCLUSIVE mutex: readers "
              "serialize although the cow only ever hands out a const T&");
static_assert(!std::is_same_v<SV::mutex, std::shared_mutex>);

using SVS = synchronized_value<std::string>;
static_assert(std::is_same_v<SVS::mutex, std::shared_mutex>,
              "the very same T, not wrapped in a cow, gets a shared_mutex");
int main() {}
