#include "/tmp/guard_prelude.h"
int& borrow(sync_int& sv) { auto locked = sv.lock(); return *locked; }
int main() { sync_int sv{1}; int& escaped = borrow(sv); escaped = 42; }
