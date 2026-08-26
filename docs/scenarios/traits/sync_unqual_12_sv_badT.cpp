#include <threadsafe/threadsafe.h>
constexpr bool answer = threadsafe::is_synchronizable_v<const threadsafe::synchronized_value<int*>>;
static_assert(!answer);
int main() {}
