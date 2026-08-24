// The textbook pimpl: a unique_ptr to an incomplete type. The library's own
// diagnostic mentions the idiom, so ask the trait about one.
#include <threadsafe/threadsafe.h>

#include <memory>

namespace app {

class widget {
public:
    widget();
    ~widget();

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}

static_assert(!threadsafe::is_sendable_v<app::widget>);
