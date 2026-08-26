#include <cstdio>
#include <thread>
int shared = 0;
int main() {
  std::thread a([]{ shared = 1; });
  std::thread b([]{ shared = 2; });
  a.join(); b.join();
  std::printf("ok %d\n", shared);
}
