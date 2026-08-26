#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>

template <class T, class Alloc = std::allocator<T>>
class my_vector {
public:
    my_vector() = default;
    template <class It> my_vector(It first, It last);
private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
    Alloc allocator_;
};

// ---- what the user must write today, by hand, for one class template ----
namespace threadsafe {
template <class T, class Alloc>
struct is_sendable<my_vector<T, Alloc>>
    : std::bool_constant<is_sendable_v<T> && is_sendable_v<Alloc>> {};

template <class T, class Alloc>
struct is_synchronizable<const my_vector<T, Alloc>>
    : std::bool_constant<is_synchronizable_v<const T>
                         && is_synchronizable_v<const Alloc>> {};

template <class T, class Alloc>
struct is_lifetime_aware<my_vector<T, Alloc>>
    : std::bool_constant<is_lifetime_aware_v<T> && is_lifetime_aware_v<Alloc>> {};
}
// ---- end ----

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

static_assert(is_sendable_v<my_vector<std::string>>);
static_assert(!is_sendable_v<my_vector<int*>>);
static_assert(is_synchronizable_v<const my_vector<int>>);
static_assert(!is_synchronizable_v<const my_vector<int*>>);
static_assert(is_lifetime_aware_v<my_vector<std::string>>);
static_assert(!is_lifetime_aware_v<my_vector<int*>>);
static_assert(is_sendable_v<const my_vector<int>>, "cv forwarding still works");
static_assert(is_sendable_v<std::vector<my_vector<int>>>, "nests inside std");
int main() {}
