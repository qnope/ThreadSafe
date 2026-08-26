#include <threadsafe/threadsafe.h>
#include <string>

// The library catches `mutable`, which is written through a const reference...
struct MutableCache {
    mutable int cache = 0;
    mutable bool computed = false;
    int value() const {
        if (!computed) { cache = 42; computed = true; }
        return cache;
    }
};
static_assert(!threadsafe::is_synchronizable_v<const MutableCache>,
              "mutable is correctly refused");

// ...but the exact same lazy cache written with const_cast is blessed.
struct ConstCastCache {
    int cache = 0;
    bool computed = false;
    int value() const {
        if (!computed) {
            auto& self = *const_cast<ConstCastCache*>(this);
            self.cache = 42;
            self.computed = true;
        }
        return cache;
    }
};
static_assert(threadsafe::is_synchronizable_v<const ConstCastCache>,
              "const_cast is blessed");
static_assert(threadsafe::is_sendable_v<ConstCastCache>);
static_assert(std::is_same_v<
                  threadsafe::synchronized_value<ConstCastCache>::mutex,
                  std::shared_mutex>,
              "and synchronized_value hands concurrent readers a shared_lock");
static_assert(threadsafe::is_sendable_v<threadsafe::copy_on_write<ConstCastCache>>,
              "copy_on_write also blesses it: readers share one block");
