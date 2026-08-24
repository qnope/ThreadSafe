#include <threadsafe/threadsafe.h>
#include <bitset>
#include <chrono>
#include <complex>
#include <expected>
#include <flat_map>
#include <memory_resource>
#include <queue>
#include <stack>
#include <valarray>
#include <vector>
consteval void explain() { threadsafe::assert_synchronizable<const std::complex<double>>(); }
static_assert((explain(), true));
