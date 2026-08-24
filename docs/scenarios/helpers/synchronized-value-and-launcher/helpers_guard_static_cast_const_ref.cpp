#include "/tmp/guard_prelude.h"
int main() { sync_int sv{1}; int& escaped = *static_cast<const sync_int::guard&>(sv.lock()); escaped = 42; }
