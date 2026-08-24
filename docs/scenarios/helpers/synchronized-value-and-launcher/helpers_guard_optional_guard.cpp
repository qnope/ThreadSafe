#include "/tmp/guard_prelude.h"
int main() { sync_int sv{1}; std::optional<sync_int::guard> maybe; maybe.emplace(sv.lock()); }
