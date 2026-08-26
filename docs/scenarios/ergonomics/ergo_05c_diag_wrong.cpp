#include <threadsafe/threadsafe.h>
#include <memory>
#include <span>
#include <vector>

// The trait answers false through a SPECIALIZATION (smart_pointers.h), not
// through the structural walk. What reason does assert_sendable give?
consteval bool probe() {
    threadsafe::assert_sendable<std::shared_ptr<const std::vector<int>>>();
    return true;
}
static_assert(probe());
