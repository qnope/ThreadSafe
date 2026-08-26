#include <threadsafe/threadsafe.h>
#include <string>
#include <meta>

int make() {
    int local = 0;
    auto by_ref = [&local]() { return local; };
    auto by_val = [v = std::string("x")]() { return v.size(); };
    auto plain  = [](int x) { return x; };
    static_assert(!threadsafe::is_lifetime_aware_v<decltype(by_ref)>, "GOT-TRUE by_ref");
    static_assert(!threadsafe::is_lifetime_aware_v<decltype(by_val)>, "GOT-TRUE by_val");
    static_assert(threadsafe::is_lifetime_aware_v<decltype(plain)>, "GOT-FALSE plain");
    static_assert(std::meta::nonstatic_data_members_of(^^decltype(by_ref), std::meta::access_context::unchecked()).size() == 0, "by_ref has visible nsdm");
    return by_ref() + (int)by_val() + plain(1);
}
int main(){ return make(); }
