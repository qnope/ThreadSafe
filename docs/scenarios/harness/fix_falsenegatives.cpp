#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
#include <string>

namespace {
struct OwningFreeDeleter {           // genuinely frees, stateless
    void operator()(const std::string *p) const noexcept { delete p; }
};
}
using threadsafe::is_synchronizable_v;

int main() {
    std::printf("const unique_ptr<const int>                          = %d\n",
                (int)is_synchronizable_v<const std::unique_ptr<const int>>);
    std::printf("const unique_ptr<const int[]>                        = %d\n",
                (int)is_synchronizable_v<const std::unique_ptr<const int[]>>);
    std::printf("const unique_ptr<const string>                       = %d\n",
                (int)is_synchronizable_v<const std::unique_ptr<const std::string>>);
    std::printf("const unique_ptr<const string, OwningFreeDeleter>    = %d\n",
                (int)is_synchronizable_v<const std::unique_ptr<const std::string, OwningFreeDeleter>>);
    std::printf("const unique_ptr<const int, void(*)(const int*)>     = %d\n",
                (int)is_synchronizable_v<const std::unique_ptr<const int, void(*)(const int*)>>);
}
