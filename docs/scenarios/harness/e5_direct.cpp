#include <threadsafe/threadsafe.h>
using sync_int = threadsafe::synchronized_value<int>;
int main() { auto sv = sync_int::make(0); int& r = *sv->lock(); return r; }
