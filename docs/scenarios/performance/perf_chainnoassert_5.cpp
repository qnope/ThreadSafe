#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 { Level0 inner; };
struct Level2 { Level1 inner; };
struct Level3 { Level2 inner; };
struct Level4 { Level3 inner; };
struct Level5 { Level4 inner; };
