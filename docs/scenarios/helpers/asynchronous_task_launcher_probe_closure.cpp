#include <meta>
#include <string>
#include <print>
#include <threadsafe/threadsafe.h>

int g = 0;
auto make_string_by_value() { std::string s = "hello"; return [s] { (void)s.size(); }; }
auto make_ptr_by_value(int* p) { return [p] { *p = 1; }; }
using StringByValue = decltype(make_string_by_value());
using PtrByValue    = decltype(make_ptr_by_value(&g));

template <class T>
consteval std::size_t nsdm_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
}
template <class T>
consteval bool empty_type() { return std::meta::is_empty_type(^^T); }

static_assert(nsdm_count<StringByValue>() == 0, "closure captures are NOT reflected as nsdm");
static_assert(!empty_type<StringByValue>());
static_assert(threadsafe::detail::has_unreflectable_state(^^StringByValue));
static_assert(nsdm_count<PtrByValue>() == 0);
static_assert(threadsafe::detail::has_unreflectable_state(^^PtrByValue));
int main(){}
