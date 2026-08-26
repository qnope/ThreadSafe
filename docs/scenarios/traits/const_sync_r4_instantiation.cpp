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

// STEP 1: can_substitute only
consteval bool step_can_substitute() {
    return can_substitute(ctor_template_of(^^Histogram), std::vector{^^Probe});
}
static_assert(step_can_substitute(), "can_substitute said no");
