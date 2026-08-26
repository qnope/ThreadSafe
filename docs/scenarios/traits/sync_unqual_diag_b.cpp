#include <threadsafe/threadsafe.h>
#include <atomic>
#include <optional>
#include <string>
#include <vector>
struct Borrower { int *borrowed; };
static_assert((threadsafe::assert_synchronizable<volatile std::atomic<int>>(), true));
int main() {}
