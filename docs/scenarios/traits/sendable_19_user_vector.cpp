#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {

// A textbook owning container: byte-for-byte the ownership story std::vector
// tells, written by hand.
template <class T>
class small_vector {
public:
    small_vector() = default;
    small_vector(const small_vector& other) : data_(new T[other.size_]), size_(other.size_) {
        for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
    }
    small_vector(small_vector&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}
    ~small_vector() { delete[] data_; }
    T& operator[](std::size_t i) { return data_[i]; }

private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
};

}

using threadsafe::is_sendable_v;

static_assert(is_sendable_v<std::vector<int>>,
              "std::vector<int> is sendable ONLY because it is on the allow-list");
static_assert(!is_sendable_v<small_vector<int>>,
              "the identical hand-written container is refused");
static_assert(!threadsafe::is_synchronizable_v<const small_vector<int>>);
static_assert(!threadsafe::is_lifetime_aware_v<small_vector<int>>);
