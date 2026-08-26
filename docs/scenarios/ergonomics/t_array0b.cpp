#include <threadsafe/threadsafe.h>
#include <array>
#include <print>
int main(){
  std::print("array<int*,0>: s={} c={} l={}\n",
    threadsafe::is_sendable_v<std::array<int*,0>>,
    threadsafe::is_synchronizable_v<const std::array<int*,0>>,
    threadsafe::is_lifetime_aware_v<std::array<int*,0>>);
}
