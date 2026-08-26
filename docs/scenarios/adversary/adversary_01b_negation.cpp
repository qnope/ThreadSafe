#include <threadsafe/threadsafe.h>
class LookupTable {
public:
    int find(int key) const { ++probe_count_; return key * 2; }
private:
    static inline long probe_count_ = 0;
};
static_assert(!threadsafe::is_synchronizable_v<const LookupTable>, "NEGATION");
