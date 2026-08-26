#include <meta>
#include <vector>

struct CompleteProbe {};   // complete, but supports no operations

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
consteval std::size_t arity_with_complete_probe() {
    return parameters_of(substitute(ctor_template_of(^^Histogram),
                                    std::vector{^^CompleteProbe})).size();
}
static_assert(arity_with_complete_probe() == 2, "arity not 2");
