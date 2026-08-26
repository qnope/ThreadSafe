#include <meta>
#include <string>
#include <type_traits>
#include <concepts>
#include <vector>

struct Probe {};

struct Meters {
    double value_;
    template <class Number> requires std::is_arithmetic_v<Number>
    constexpr Meters(Number quantity) : value_(double(quantity)) {}
};

struct GreedyForward {
    std::string text_;
    GreedyForward() = default;
    template <class Argument> GreedyForward(Argument &&) {}
};

struct Histogram {
    int b_[8];
    Histogram() = default;
    template <class It> Histogram(It, It) {}
};

using namespace std::meta;

consteval info first_ctor_template(info type) {
    for (info member : members_of(type, access_context::unchecked()))
        if (is_constructor_template(member))
            return member;
    return info{};
}

// Does parameters_of work on an un-substituted constructor template?
consteval std::size_t param_count_of_template(info type) {
    return parameters_of(first_ctor_template(type)).size();
}

// Can we substitute a constructor template with one type argument?
consteval bool can_sub_one(info type, info argument) {
    return can_substitute(first_ctor_template(type), std::vector<info>{argument});
}

consteval info sub_one_param_type(info type, info argument) {
    info substituted = substitute(first_ctor_template(type), std::vector<info>{argument});
    auto params = parameters_of(substituted);
    return params.empty() ? info{} : type_of(params[0]);
}

static_assert(can_sub_one(^^Meters, ^^Probe) == false, "Meters accepts Probe");
static_assert(can_sub_one(^^Meters, ^^int) == true, "Meters rejects int");
static_assert(can_sub_one(^^GreedyForward, ^^Probe) == true, "Greedy rejects Probe");
static_assert(can_sub_one(^^Histogram, ^^Probe) == true, "Histogram rejects Probe");

static_assert(sub_one_param_type(^^Meters, ^^int) == ^^int, "Meters param not int");
static_assert(sub_one_param_type(^^GreedyForward, ^^Probe) == ^^Probe&&, "Greedy param not Probe&&");
static_assert(parameters_of(substitute(first_ctor_template(^^Histogram), std::vector<info>{^^Probe})).size() == 2, "Histogram arity not 2");

// the key question: is parameters_of legal on the raw template?
static_assert(param_count_of_template(^^Meters) == 1, "parameters_of on template gave != 1");
