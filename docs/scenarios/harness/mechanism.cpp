#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>

using threadsafe::is_synchronizable_v;

struct Body { std::string text; };

// same layout / same const interface as copy_on_write, but WITHOUT the
// variadic constructor template.
template <class T>
class cow_no_template {
public:
    cow_no_template() = default;
    const T& operator*() const noexcept { return *ptr_; }
    T& as_mutable() { return *ptr_; }
private:
    std::shared_ptr<T> ptr_;
};

// same, but WITH a constrained constructor template (like the real one)
template <class T>
class cow_with_template {
public:
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, cow_with_template> && ...))
    explicit cow_with_template(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}
    const T& operator*() const noexcept { return *ptr_; }
    T& as_mutable() { return *ptr_; }
private:
    std::shared_ptr<T> ptr_;
};

static_assert(is_synchronizable_v<const cow_no_template<Body>>,
              "without the ctor template the structural walk ACCEPTS it");
static_assert(!is_synchronizable_v<const cow_with_template<Body>>,
              "the constrained ctor template alone is what blocks it");
