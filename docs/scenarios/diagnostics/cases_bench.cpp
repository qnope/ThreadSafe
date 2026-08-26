#include <threadsafe/threadsafe.h>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

struct Bad { int *borrowed; };
struct Good { int value; double ratio; };

// Every one of these answers false through the wrapper rule.
static_assert(!threadsafe::is_sendable_v<std::vector<Bad>>);
static_assert(!threadsafe::is_sendable_v<std::optional<Bad>>);
static_assert(!threadsafe::is_sendable_v<std::tuple<int, Bad, double>>);
static_assert(!threadsafe::is_sendable_v<std::map<int, std::vector<Bad>>>);
static_assert(!threadsafe::is_sendable_v<std::variant<int, Bad>>);
static_assert(!threadsafe::is_synchronizable_v<const std::vector<Bad>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::vector<Bad>>);
// And these answer true.
static_assert(threadsafe::is_sendable_v<std::vector<Good>>);
static_assert(threadsafe::is_sendable_v<std::map<std::string, std::vector<Good>>>);
static_assert(threadsafe::is_lifetime_aware_v<std::vector<std::string>>);
int main() {}
