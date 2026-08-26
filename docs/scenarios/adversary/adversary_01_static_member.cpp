#include <threadsafe/threadsafe.h>
#include <map>
#include <string>

// A lookup table that keeps *statistics* about itself. The counter is a static
// member, so every instance -- and every copy sent to another thread -- shares it.
class LookupTable {
public:
    int find(int key) const {
        ++probe_count_;              // written through a CONST member function
        return key * 2;
    }
    static long probes() { return probe_count_; }

private:
    static inline long probe_count_ = 0;
};

static_assert(threadsafe::is_sendable_v<LookupTable>);
static_assert(threadsafe::is_synchronizable_v<const LookupTable>);
static_assert(threadsafe::is_lifetime_aware_v<LookupTable>);
static_assert(threadsafe::launchable_task<decltype([](LookupTable){}), LookupTable>);

// The library's own synchronized_value picks a shared_mutex for it.
static_assert(std::is_same_v<threadsafe::synchronized_value<LookupTable>::mutex,
                             std::shared_mutex>);

// static member with a non-trivial type: still invisible
class Session {
public:
    explicit Session(std::string name) : name_(std::move(name)) {}
    void touch() { ++hits_[name_]; }
private:
    static inline std::map<std::string, int> hits_;
    std::string name_;
};

static_assert(threadsafe::is_sendable_v<Session>);
static_assert(threadsafe::is_synchronizable_v<const Session>);
static_assert(threadsafe::is_lifetime_aware_v<Session>);

// thread_local member: same blindness, opposite hazard
class ArenaHandle {
public:
    static inline thread_local int* arena = nullptr;
    int index;
};
static_assert(threadsafe::is_sendable_v<ArenaHandle>);
