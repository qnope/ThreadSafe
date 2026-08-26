#include <threadsafe/threadsafe.h>
#include <string>
#include <meta>

int global_counter = 0;

// A lambda naming a *static storage duration* variable in its capture list.
auto names_global_by_ref = [&global_counter] { return ++global_counter; };
// A lambda that really captures an automatic variable by reference.
auto make_ref_capture(std::string &text) { return [&text] { text += "x"; }; }
using RefCapture = decltype(make_ref_capture(*(std::string *)nullptr));
auto make_ptr_capture(std::string &text) { return [p = &text] { *p += "x"; }; }
using PtrCapture = decltype(make_ptr_capture(*(std::string *)nullptr));

consteval std::size_t member_count(std::meta::info type) {
    return std::meta::nonstatic_data_members_of(type,
             std::meta::access_context::unchecked()).size();
}

static_assert(member_count(^^decltype(names_global_by_ref)) == 0, "global-ref closure HAS members");
static_assert(member_count(^^RefCapture) == 0, "RefCapture HAS members");
static_assert(std::is_empty_v<decltype(names_global_by_ref)>, "global-ref closure NOT empty");
static_assert(!std::is_empty_v<RefCapture>, "RefCapture IS empty");

using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const decltype(names_global_by_ref)>, "names_global_by_ref TRUE");
static_assert(!is_synchronizable_v<const RefCapture>, "RefCapture TRUE");
static_assert(!is_synchronizable_v<const PtrCapture>, "PtrCapture TRUE");
