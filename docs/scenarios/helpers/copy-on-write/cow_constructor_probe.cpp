#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <mutex>

#include <concepts>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using threadsafe::copy_on_write;

namespace {
struct CountsCopies {
    static inline int copies = 0;
    static inline int moves = 0;
    int value = 0;
    CountsCopies() = default;
    explicit CountsCopies(int v) : value(v) {}
    CountsCopies(const CountsCopies& other) : value(other.value) { ++copies; }
    CountsCopies(CountsCopies&& other) noexcept : value(other.value) { ++moves; }
    CountsCopies& operator=(const CountsCopies&) = default;
};

template <class C>
constexpr bool can_detach = requires(C c) { c.as_mutable(); };

template <class C, class... A>
constexpr bool can_brace_init = requires { C{A{}...}; };

struct DerivedFromCow : copy_on_write<int> {
    using copy_on_write<int>::copy_on_write;
};
}

// --- the copy constructor is not hijacked, in every value category ---------
static_assert(std::constructible_from<copy_on_write<int>, copy_on_write<int>&>);
static_assert(std::constructible_from<copy_on_write<int>, const copy_on_write<int>&>);
static_assert(std::constructible_from<copy_on_write<int>, copy_on_write<int>&&>);
static_assert(std::constructible_from<copy_on_write<int>, const copy_on_write<int>&&>);
static_assert(std::copy_constructible<copy_on_write<int>>
              && std::move_constructible<copy_on_write<int>>);

// --- assignment exists and is implicit ------------------------------------
static_assert(std::assignable_from<copy_on_write<int>&, const copy_on_write<int>&>);
static_assert(std::assignable_from<copy_on_write<int>&, copy_on_write<int>&&>);
static_assert(std::is_nothrow_move_constructible_v<copy_on_write<int>>);
static_assert(std::is_nothrow_destructible_v<copy_on_write<int>>);

// --- zero arguments: a default constructor exists, but it is explicit -----
static_assert(std::default_initializable<copy_on_write<int>>);
static_assert(!std::is_convertible_v<void, copy_on_write<int>>);
static_assert(std::default_initializable<copy_on_write<std::mutex>>
                  && !can_detach<copy_on_write<std::mutex>>,
              "std::mutex is constructible but not copyable — construction is "
              "still allowed, only as_mutable is gone");

// --- container usability --------------------------------------------------
static_assert(std::regular<int>);
static_assert(!std::equality_comparable<copy_on_write<int>>,
              "no operator==, so cow cannot be a std::vector element compared, "
              "nor a std::map key, nor used with std::find");
static_assert(!std::totally_ordered<copy_on_write<int>>);
static_assert(requires { std::vector<copy_on_write<int>>{}; });

// --- copy_on_write<copy_on_write<T>> constructs ---------------------------
static_assert(std::constructible_from<copy_on_write<copy_on_write<int>>,
                                      copy_on_write<int>>);
static_assert(!threadsafe::is_sendable_v<copy_on_write<copy_on_write<int>>>,
              "...but is rejected as unsendable");

// --- a derived class does NOT hijack the base copy constructor ------------
static_assert(std::constructible_from<copy_on_write<int>, DerivedFromCow&>,
              "ordinary derived-to-base slicing through the copy constructor, "
              "not a variadic hijack");

int main() {
    // braced / parenthesised / const lvalue / rvalue, counting real copies
    copy_on_write<CountsCopies> from_lvalue{CountsCopies{7}};
    const CountsCopies named{9};
    copy_on_write<CountsCopies> from_const_lvalue{named};
    copy_on_write<CountsCopies> from_braced{CountsCopies{}};
    copy_on_write<CountsCopies> default_constructed{};
    std::printf("copies=%d moves=%d\n", CountsCopies::copies, CountsCopies::moves);

    // copying the handle must not copy the T
    const int copies_before_handle_copy = CountsCopies::copies;
    copy_on_write<CountsCopies> handle_copy = from_lvalue;
    copy_on_write<CountsCopies> handle_copy2{from_lvalue};
    std::printf("handle copies cost %d T-copies (want 0)\n",
                CountsCopies::copies - copies_before_handle_copy);

    // as_mutable now detaches (three handles)
    const int copies_before_detach = CountsCopies::copies;
    handle_copy.as_mutable().value = 42;
    std::printf("detach cost %d T-copies (want 1); original=%d detached=%d\n",
                CountsCopies::copies - copies_before_detach,
                from_lvalue->value, handle_copy->value);

    // initializer_list
    copy_on_write<std::vector<int>> from_init_list{std::initializer_list<int>{1, 2, 3}};
    std::printf("init_list size=%zu\n", from_init_list->size());

    // copy_on_write<std::vector<int>>{{1, 2, 3}} does NOT compile — see
    // probes/cow_braced_init.cpp.  The explicit spelling is required:
    copy_on_write<std::vector<int>> braced_explicitly{std::vector<int>{1, 2, 3}};
    std::printf("braced size=%zu\n", braced_explicitly->size());

    (void)from_const_lvalue; (void)from_braced; (void)default_constructed;
    (void)handle_copy2;
    return 0;
}
