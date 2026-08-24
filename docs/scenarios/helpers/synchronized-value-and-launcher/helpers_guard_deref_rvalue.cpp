#include "/tmp/guard_prelude.h"
int main() { sync_int sv{1}; int& escaped = *sv.lock(); (void)escaped; }
