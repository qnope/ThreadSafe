#include "/tmp/guard_prelude.h"
int main() { sync_vec sv{}; sv.lock()->push_back(1); }
