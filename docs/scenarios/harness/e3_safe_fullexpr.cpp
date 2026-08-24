#include <threadsafe/threadsafe.h>
using sync_int = threadsafe::synchronized_value<int>;
// SAFE: the temporary guard lives to the end of the full-expression.
void bump(const sync_int::guard& locked) { *locked += 1; }
int main() {
    auto shared_counter = sync_int::make(0);
    bump(shared_counter->lock());                    // safe use of a temporary
    const sync_int::guard& extended = shared_counter->lock(); // lifetime-extended
    *extended += 1;                                  // safe: lock still held
    return *extended == 2 ? 0 : 1;
}
