#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <string>

template <threadsafe::synchronizable Shared>
void register_shared_resource(std::shared_ptr<Shared>) {}

// The same concept answers the read-only question, spelled on the type.
template <class T>
    requires threadsafe::synchronizable<const T>
void publish_immutable(const T &) {}

int main() {
    register_shared_resource(std::make_shared<std::atomic<int>>(0));
    publish_immutable(std::string{"read me"});
}
