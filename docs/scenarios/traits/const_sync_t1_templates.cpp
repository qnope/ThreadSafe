#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
#include <iterator>
#include <concepts>

// 1. converting constructor template — cannot hijack (by value)
struct Meters {
    double value_;
    template <class Number> requires std::is_arithmetic_v<Number>
    constexpr Meters(Number quantity) : value_(static_cast<double>(quantity)) {}
};

// 2. two-iterator constructor template — arity 2, cannot hijack
struct Histogram {
    int buckets_[8];
    Histogram() : buckets_{} {}
    template <class Iterator>
    Histogram(Iterator first, Iterator last) : buckets_{} {
        for (; first != last; ++first) ++buckets_[*first % 8];
    }
};

// 3. explicitly guarded forwarding constructor — cannot hijack
struct GuardedForward {
    std::string text_;
    template <class Argument>
        requires (!std::same_as<std::remove_cvref_t<Argument>, GuardedForward>)
    explicit GuardedForward(Argument &&argument)
        : text_(std::forward<Argument>(argument)) {}
};

// 4. genuinely greedy forwarding constructor — CAN hijack
struct GreedyForward {
    std::string text_;
    GreedyForward() = default;
    template <class Argument>
    GreedyForward(Argument &&argument) : text_("hijacked") {}
};

// 5. templated assignment operator only
struct TemplatedAssign {
    int value_;
    template <class Number> TemplatedAssign &operator=(Number n) { value_ = int(n); return *this; }
};

// 6. templated comparison operator (not operator=)
struct TemplatedCompare {
    int value_;
    template <class Other> bool operator==(const Other &o) const { return value_ == o.value_; }
};

// 7. a variant-like user type
template <class... Alternatives>
struct MyVariant {
    unsigned index_;
    alignas(Alternatives...) unsigned char storage_[std::max({sizeof(Alternatives)...})];
    MyVariant() : index_(0), storage_{} {}
    template <class Alternative> MyVariant(Alternative) : index_(1), storage_{} {}
};

// 8. aggregate with a deduction-guide-ish template ctor
template <class Element, std::size_t Count>
struct FixedVector {
    Element data_[Count];
    template <class... Init> constexpr FixedVector(Init... init) : data_{init...} {}
};

using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const Meters>,          "Meters TRUE");
static_assert(!is_synchronizable_v<const Histogram>,       "Histogram TRUE");
static_assert(!is_synchronizable_v<const GuardedForward>,  "GuardedForward TRUE");
static_assert(!is_synchronizable_v<const GreedyForward>,   "GreedyForward TRUE");
static_assert(!is_synchronizable_v<const TemplatedAssign>, "TemplatedAssign TRUE");
static_assert(!is_synchronizable_v<const TemplatedCompare>,"TemplatedCompare TRUE");
static_assert(!is_synchronizable_v<const MyVariant<int, double>>, "MyVariant TRUE");
static_assert(!is_synchronizable_v<const FixedVector<int, 4>>,    "FixedVector TRUE");
