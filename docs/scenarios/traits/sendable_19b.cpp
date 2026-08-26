#include <threadsafe/threadsafe.h>
#include <cstddef>
#include <utility>
namespace {
template <class T>
class small_vector {
public:
    small_vector() = default;
    small_vector(const small_vector& o) : data_(new T[o.size_]), size_(o.size_) {}
    small_vector(small_vector&& o) noexcept : data_(std::exchange(o.data_, nullptr)), size_(std::exchange(o.size_, 0)) {}
    ~small_vector() { delete[] data_; }
private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
};
}
static_assert((threadsafe::assert_sendable<small_vector<int>>(), true));
