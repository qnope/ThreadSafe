#include <threadsafe/threadsafe.h>
#include <optional>
#include <string>
#include <vector>
struct Borrower { int *borrowed; };
static_assert((threadsafe::assert_synchronizable<const std::vector<std::optional<Borrower>>>(), true));
int main() {}
