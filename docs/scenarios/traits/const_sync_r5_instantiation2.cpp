#include <meta>
#include <vector>

struct Probe;

struct Histogram {
    int buckets_[8];
    Histogram() : buckets_{} {}
    template <class Iterator>
    Histogram(Iterator first, Iterator last) : buckets_{} {
        for (; first != last; ++first) ++buckets_[*first % 8];   // line 11: body
    }
};

using namespace std::meta;
consteval info ctor_template_of(info type) {
    for (info member : members_of(type, access_context::unchecked()))
        if (is_constructor_template(member))
            return member;
    return info{};
}

// STEP 2: substitute + parameters_of
consteval std::size_t step_substitute() {
    return parameters_of(substitute(ctor_template_of(^^Histogram), std::vector{^^Probe})).size();
}
static_assert(step_substitute() == 2, "arity not 2");
