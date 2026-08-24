#include "/tmp/guard_prelude.h"
int main() { sync_int sv{1}; const sync_int::guard& g = sv.lock(); *g = 42; }
