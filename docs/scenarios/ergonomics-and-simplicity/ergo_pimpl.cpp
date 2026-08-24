// The header says: "is_sendable<T> requires a complete type — specialize
// is_sendable for types holding a pointer to an incomplete type (the pimpl
// idiom)". This is that exact type.
#include <threadsafe/threadsafe.h>

#include <memory>

class widget {
public:
    widget();
    ~widget();
    widget(widget &&) noexcept;
    widget &operator=(widget &&) noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> pimpl_;
};

static_assert(!threadsafe::is_sendable_v<widget>);

int main() {}
