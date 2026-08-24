#include <threadsafe/threadsafe.h>
#include <generator>
consteval bool ask() { threadsafe::assert_sendable<std::generator<int>>(); return true; }
static_assert(ask());
int main() {}
