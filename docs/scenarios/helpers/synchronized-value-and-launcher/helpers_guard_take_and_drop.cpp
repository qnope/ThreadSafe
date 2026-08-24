#include "/tmp/guard_prelude.h"
int main() { sync_int sv{1}; sv.lock(); }
