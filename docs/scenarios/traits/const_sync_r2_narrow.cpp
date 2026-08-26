#include <meta>
#include <string>
#include <vector>
#include <concepts>
#include <type_traits>

namespace narrowed {

// A type no user template has ever seen: substituting it tells the *declared*
// shape of the first parameter apart from the argument it was given.
struct hijack_probe;

inline consteval bool takes_exactly_one_argument(const std::vector<std::meta::info> &parameters) {
    if (parameters.empty())
        return false;
    if (parameters.size() == 1)
        return true;
    return std::meta::has_default_argument(parameters[1]);
}

// Does the first parameter of `member` bind a *reference* to its own template
// parameter?  Substituting a non-reference type X tells: `U` yields X, `const U&`
// yields const X&, while the two greedy shapes `U&&` and `U&` yield X&& and X&.
inline consteval bool binds_first_parameter_by_reference(std::meta::info member,
                                                         std::meta::info type) {
    for (std::meta::info probe : {type, ^^hijack_probe, ^^int}) {
        if (!std::meta::can_substitute(member, std::vector{probe}))
            continue;
        const auto parameters = std::meta::parameters_of(std::meta::substitute(member, std::vector{probe}));
        if (!takes_exactly_one_argument(parameters))
            return false;
        const auto first = std::meta::type_of(parameters[0]);
        return first == std::meta::add_lvalue_reference(probe)
            || first == std::meta::add_rvalue_reference(probe);
    }
    return true;
}

inline consteval bool may_hijack_copy_move(std::meta::info member, std::meta::info type) {
    const bool is_candidate =
        std::meta::is_constructor_template(member)
        || (std::meta::is_operator_function_template(member)
            && std::meta::operator_of(member) == std::meta::op_equals);
    if (!is_candidate)
        return false;

    // The hijack happens on a non-const lvalue: `U&&` deduces U = T&, `U&` deduces
    // U = T.  If neither instantiation is viable the template never competes.
    const bool viable_on_own_type =
        std::meta::can_substitute(member, std::vector{std::meta::add_lvalue_reference(type)})
        || std::meta::can_substitute(member, std::vector{type});
    if (!viable_on_own_type)
        return false;

    return binds_first_parameter_by_reference(member, type);
}

inline consteval bool has_hijacking_template(std::meta::info type) {
    for (std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
        if (may_hijack_copy_move(member, type))
            return true;
    return false;
}

}

// ---- the shapes ----------------------------------------------------------
struct Meters {                                   // harmless: by value, constrained
    double value_;
    template <class Number> requires std::is_arithmetic_v<Number>
    constexpr Meters(Number quantity) : value_(double(quantity)) {}
};
struct Histogram {                                // harmless: arity 2
    int b_[8];
    Histogram() = default;
    template <class It> Histogram(It, It) {}
};
struct GuardedForward {                           // harmless: constraint excludes T
    std::string text_;
    template <class A> requires (!std::same_as<std::remove_cvref_t<A>, GuardedForward>)
    explicit GuardedForward(A &&a) : text_(std::forward<A>(a)) {}
};
struct GreedyForward {                            // HIJACKS
    std::string text_;
    GreedyForward() = default;
    template <class A> GreedyForward(A &&) : text_("hijacked") {}
};
struct GreedyLvalue {                             // HIJACKS
    int v_;
    GreedyLvalue() = default;
    template <class A> GreedyLvalue(A &) : v_(0) {}
};
struct HarmlessConstRef {                         // harmless: ties and loses
    int v_;
    HarmlessConstRef() = default;
    template <class A> HarmlessConstRef(const A &) : v_(0) {}
};
struct ByValueTemplate {                          // harmless: ties and loses
    int v_;
    ByValueTemplate() = default;
    template <class A> ByValueTemplate(A) : v_(0) {}
};
struct TemplatedAssignByValue {                   // harmless
    int v_;
    template <class N> TemplatedAssignByValue &operator=(N) { return *this; }
};
struct TemplatedAssignGreedy {                    // HIJACKS
    int v_;
    template <class N> TemplatedAssignGreedy &operator=(N &&) { return *this; }
};
struct StringLiteralCtor {                        // harmless: non-type template parameter
    int v_;
    StringLiteralCtor() = default;
    template <std::size_t N> StringLiteralCtor(const char (&)[N]) : v_(int(N)) {}
};
struct VariadicByValue {                          // harmless
    int a_, b_;
    template <class... I> constexpr VariadicByValue(I... i) : a_(0), b_(0) {}
};
struct VariadicForwarding {                       // HIJACKS
    std::string s_;
    VariadicForwarding() = default;
    template <class... A> VariadicForwarding(A &&...) {}
};
struct NoTemplateAtAll { int v_; };

using narrowed::has_hijacking_template;
static_assert(!has_hijacking_template(^^Meters),                "Meters flagged");
static_assert(!has_hijacking_template(^^Histogram),             "Histogram flagged");
static_assert(!has_hijacking_template(^^GuardedForward),        "GuardedForward flagged");
static_assert( has_hijacking_template(^^GreedyForward),         "GreedyForward MISSED");
static_assert( has_hijacking_template(^^GreedyLvalue),          "GreedyLvalue MISSED");
static_assert(!has_hijacking_template(^^HarmlessConstRef),      "HarmlessConstRef flagged");
static_assert(!has_hijacking_template(^^ByValueTemplate),       "ByValueTemplate flagged");
static_assert(!has_hijacking_template(^^TemplatedAssignByValue),"TemplatedAssignByValue flagged");
static_assert( has_hijacking_template(^^TemplatedAssignGreedy), "TemplatedAssignGreedy MISSED");
static_assert(!has_hijacking_template(^^StringLiteralCtor),     "StringLiteralCtor flagged");
static_assert(!has_hijacking_template(^^VariadicByValue),       "VariadicByValue flagged");
static_assert( has_hijacking_template(^^VariadicForwarding),    "VariadicForwarding MISSED");
static_assert(!has_hijacking_template(^^NoTemplateAtAll),       "NoTemplateAtAll flagged");
