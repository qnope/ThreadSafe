// A user writing their own thread-safe cache wants the same vocabulary the
// library uses: "this parameter must be usable from several threads at once".
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>

template <threadsafe::synchronizable Shared>
void register_shared_resource(std::shared_ptr<Shared>) {}

int main() {
    register_shared_resource(std::make_shared<std::atomic<int>>(0));
}
