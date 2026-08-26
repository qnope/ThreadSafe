#include <threadsafe/threadsafe.h>
#include <expected>
#include <flat_map>
#include <flat_set>
#include <stack>
#include <queue>
#include <bitset>
#include <complex>
#include <chrono>
#include <string_view>
#include <initializer_list>
#include <valarray>
#include <span>
#include <string>
#include <print>
using namespace threadsafe;
template <class T>
void row(const char* name) {
  std::print("{:46} send={:5} constsync={:5} lifetime={:5}\n", name,
    is_sendable_v<T>, is_synchronizable_v<const T>, is_lifetime_aware_v<T>);
}
int main(){
  row<std::expected<int, std::string>>("expected<int,string>");
  row<std::expected<int*, std::string>>("expected<int*,string>   [should be F]");
  row<std::flat_map<int, std::string>>("flat_map<int,string>");
  row<std::flat_map<int, int*>>("flat_map<int,int*>      [should be F]");
  row<std::flat_set<int>>("flat_set<int>");
  row<std::stack<int>>("stack<int>");
  row<std::stack<int*>>("stack<int*>             [should be F]");
  row<std::queue<int>>("queue<int>");
  row<std::priority_queue<int>>("priority_queue<int>");
  row<std::bitset<8>>("bitset<8>");
  row<std::complex<double>>("complex<double>");
  row<std::chrono::milliseconds>("chrono::milliseconds");
  row<std::chrono::system_clock::time_point>("chrono::system_clock::time_point");
  row<std::string_view>("string_view             [should be send=T lt=F]");
  row<std::initializer_list<int>>("initializer_list<int>   [should be lt=F]");
  row<std::valarray<int>>("valarray<int>");
  row<std::valarray<int*>>("valarray<int*>          [should be F]");
  row<std::span<int>>("span<int>               [should be lt=F]");
  row<std::span<const int>>("span<const int>         [should be lt=F]");
}
