#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>

template <class T, class Alloc = std::allocator<T>>
class my_vector {
public:
    my_vector() = default;
    template <class It> my_vector(It first, It last);
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
    Alloc allocator_;
};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

static_assert(!is_sendable_v<my_vector<int>>, "raw: not sendable");
static_assert(!is_synchronizable_v<const my_vector<int>>, "raw: not const-sync");
static_assert(!is_lifetime_aware_v<my_vector<int>>, "raw: not lifetime aware");
int main() {}
