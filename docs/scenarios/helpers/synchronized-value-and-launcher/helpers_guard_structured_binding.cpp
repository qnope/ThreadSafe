#include "/tmp/guard_prelude.h"
struct Pair { int a; int b; };
using sync_pair = threadsafe::synchronized_value<Pair>;
int main() { sync_pair sv{1,2}; auto&& [first, second] = *sv.lock(); first = 9; (void)second; }
