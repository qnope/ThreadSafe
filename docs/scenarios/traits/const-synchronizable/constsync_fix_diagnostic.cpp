#include <threadsafe/threadsafe.h>
#include <vector>
consteval void explain() {
    threadsafe::assert_synchronizable<const std::vector<int *>>();
}
static_assert((explain(), true));
