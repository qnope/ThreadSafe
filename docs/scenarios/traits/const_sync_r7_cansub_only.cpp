#include <meta>
#include <vector>

struct CompleteProbe {};

struct Histogram {
    int buckets_[8];
    Histogram() : buckets_{} {}
    template <class Iterator>
    Histogram(Iterator first, Iterator last) : buckets_{} {
        for (; first != last; ++first) ++buckets_[*first % 8];   // BODY
    }
};

using namespace std::meta;
consteval info ctor_template_of(info type) {
    for (info member : members_of(type, access_context::unchecked()))
        if (is_constructor_template(member))
            return member;
    return info{};
}
consteval bool viable(info argument) {
    return can_substitute(ctor_template_of(^^Histogram), std::vector{argument});
}
static_assert(viable(^^CompleteProbe), "can_substitute said no");
