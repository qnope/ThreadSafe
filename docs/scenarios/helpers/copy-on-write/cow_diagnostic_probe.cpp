#include <threadsafe/threadsafe.h>
#include <string>

using threadsafe::copy_on_write;

namespace {
struct DocumentWithCowMember {
    copy_on_write<std::string> body;
};

consteval bool why_const_cow_is_not_synchronizable() {
    threadsafe::assert_synchronizable<const copy_on_write<std::string>>();
    return true;
}
}

static_assert(why_const_cow_is_not_synchronizable());

int main() {}
