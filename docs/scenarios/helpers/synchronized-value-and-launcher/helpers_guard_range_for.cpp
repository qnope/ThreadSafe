#include "/tmp/guard_prelude.h"
int main() { sync_vec sv{}; for (int& element : *sv.lock()) element = 1; }
