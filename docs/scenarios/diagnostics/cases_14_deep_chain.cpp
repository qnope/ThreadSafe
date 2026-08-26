#include <threadsafe/threadsafe.h>

struct Level0 { int *pointer; };
struct Level1 { Level0 inner; };
struct Level2 { Level1 inner; };
struct Level3 { Level2 inner; };
struct Level4 { Level3 inner; };
struct Level5 { Level4 inner; };
struct Level6 { Level5 inner; };
struct Level7 { Level6 inner; };
struct Level8 { Level7 inner; };
struct Level9 { Level8 inner; };
struct Level10 { Level9 inner; };
struct Level11 { Level10 inner; };
struct Level12 { Level11 inner; };
struct Level13 { Level12 inner; };
struct Level14 { Level13 inner; };
struct Level15 { Level14 inner; };
struct Level16 { Level15 inner; };
struct Level17 { Level16 inner; };
struct Level18 { Level17 inner; };
struct Level19 { Level18 inner; };
struct Level20 { Level19 inner; };

int main() {
    threadsafe::assert_sendable<Level20>();
}
