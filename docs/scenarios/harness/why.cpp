#include <threadsafe/threadsafe.h>
#include <any>
#include <bitset>
#include <chrono>
#include <complex>
#include <exception>
#include <filesystem>
#include <future>
#include <system_error>
#include <typeindex>
#include <valarray>
consteval bool q() { threadsafe::assert_sendable<std::exception_ptr>(); return true; }
static_assert(q());
