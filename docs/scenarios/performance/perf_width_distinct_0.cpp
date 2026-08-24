#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Wide {
};
static_assert(is_sendable_v<Wide>);
int main(){}
