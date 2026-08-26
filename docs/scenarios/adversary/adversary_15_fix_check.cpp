#include <threadsafe/threadsafe.h>
#include <map>
#include <string>

class LookupTable {
public:
    int find(int key) const { ++probe_count_; return key * 2; }
private:
    static inline long probe_count_ = 0;
};

class Session {
public:
    explicit Session(std::string name) : name_(std::move(name)) {}
    void touch() { ++hits_[name_]; }
private:
    static inline std::map<std::string, int> hits_;
    std::string name_;
};

struct BenignStatics {
    static constexpr int limit = 4;
    static constexpr const char* label = "x";
    int payload;
};

static_assert(!threadsafe::is_sendable_v<LookupTable>);
static_assert(!threadsafe::is_synchronizable_v<const LookupTable>);
static_assert(!threadsafe::is_sendable_v<Session>);
static_assert(!threadsafe::is_synchronizable_v<const Session>);
static_assert(threadsafe::is_sendable_v<BenignStatics>,
              "static constexpr ints must still pass");
static_assert(threadsafe::is_synchronizable_v<const BenignStatics>);
