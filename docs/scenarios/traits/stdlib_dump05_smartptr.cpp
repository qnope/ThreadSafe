#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace {
struct SyncType {};
struct Plain { int v; };
struct StatefulDeleter {
    int *log;                       // borrows
    void operator()(Plain *p) const { delete p; }
};
struct BorrowingDeleter {
    std::string &sink;
    void operator()(Plain *p) const { delete p; }
};
void free_deleter(Plain *p) { delete p; }
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

// unique_ptr with a reference deleter -- the classic "deleter stays behind" case
P("S unique_ptr<Plain, D&>",        is_sendable_v<std::unique_ptr<Plain, StatefulDeleter&>>);
P("S unique_ptr<Plain, SyncD&>",    is_sendable_v<std::unique_ptr<Plain, SyncType&>>);
P("S StatefulDeleter",              is_sendable_v<StatefulDeleter>);
P("S StatefulDeleter&",             is_sendable_v<StatefulDeleter&>);
P("S unique_ptr<Plain,BorrowingDeleter>", is_sendable_v<std::unique_ptr<Plain, BorrowingDeleter>>);
P("L unique_ptr<Plain, D&>",        is_lifetime_aware_v<std::unique_ptr<Plain, StatefulDeleter&>>);
P("L unique_ptr<Plain,StatefulD>",  is_lifetime_aware_v<std::unique_ptr<Plain, StatefulDeleter>>);
P("CS const unique_ptr<Plain,D&>",  is_synchronizable_v<const std::unique_ptr<const Plain, StatefulDeleter&>>);

P("S unique_ptr<int, void(*)(int*)>", is_sendable_v<std::unique_ptr<int, void(*)(int*)>>);
P("S unique_ptr<Plain[]>",            is_sendable_v<std::unique_ptr<Plain[]>>);
P("S unique_ptr<int*[]>",             is_sendable_v<std::unique_ptr<int*[]>>);
P("L unique_ptr<Plain[]>",            is_lifetime_aware_v<std::unique_ptr<Plain[]>>);
P("CS const unique_ptr<const Plain[]>", is_synchronizable_v<const std::unique_ptr<const Plain[]>>);

P("S shared_ptr<SyncType[]>",  is_sendable_v<std::shared_ptr<SyncType[]>>);
P("S shared_ptr<int[]>",       is_sendable_v<std::shared_ptr<int[]>>);
P("S weak_ptr<SyncType[]>",    is_sendable_v<std::weak_ptr<SyncType[]>>);
P("L shared_ptr<int[]>",       is_lifetime_aware_v<std::shared_ptr<int[]>>);
P("L shared_ptr<int[4]>",      is_lifetime_aware_v<std::shared_ptr<int[4]>>);

// reference_wrapper corners
P("S ref_wrapper<const SyncType>", is_sendable_v<std::reference_wrapper<const SyncType>>);
P("S ref_wrapper<const int>",      is_sendable_v<std::reference_wrapper<const int>>);
P("S ref_wrapper<void()>",         is_sendable_v<std::reference_wrapper<void()>>);
P("CS const ref_wrapper<void()>",  is_synchronizable_v<const std::reference_wrapper<void()>>);
P("L ref_wrapper<void()>",         is_lifetime_aware_v<std::reference_wrapper<void()>>);
P("S ref_wrapper<int(int)>",       is_sendable_v<std::reference_wrapper<int(int)>>);

// const shared_ptr: reader only copies the pointer
P("CS const shared_ptr<Plain>",  is_synchronizable_v<const std::shared_ptr<Plain>>);
P("CS const shared_ptr<const Plain>", is_synchronizable_v<const std::shared_ptr<const Plain>>);
P("CS const weak_ptr<Plain>",    is_synchronizable_v<const std::weak_ptr<Plain>>);

// atomic corners
P("S atomic<int>",  is_sendable_v<std::atomic<int>>);
P("CS atomic<int>", is_synchronizable_v<std::atomic<int>>);
P("L atomic<int>",  is_lifetime_aware_v<std::atomic<int>>);
P("L atomic<int*>", is_lifetime_aware_v<std::atomic<int*>>);

int main() {}
