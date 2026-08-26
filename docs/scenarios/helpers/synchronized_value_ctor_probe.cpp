#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <vector>
#include <string>

using sync_vec = threadsafe::synchronized_value<std::vector<int>>;

int main() {
    threadsafe::synchronized_value<int> default_constructed;      // value-init?
    threadsafe::synchronized_value<int> braced_empty{};
    sync_vec parens(3, 0);
    sync_vec braced{3, 0};

    const auto a = default_constructed.lock_shared();
    const auto b = braced_empty.lock_shared();
    const auto p = parens.lock_shared();
    const auto q = braced.lock_shared();
    std::printf("default=%d braced_empty=%d\n", *a, *b);
    std::printf("parens(3,0) -> size=%zu  [%d,%d,%d]\n", p->size(),
                (*p)[0], (*p)[1], (*p)[2]);
    std::printf("braced{3,0} -> size=%zu  [%d,%d,%d]\n", q->size(),
                (*q)[0], (*q)[1], (*q)[2]);
    return 0;
}
