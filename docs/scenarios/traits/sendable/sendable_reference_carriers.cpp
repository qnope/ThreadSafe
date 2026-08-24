#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <optional>
#include <print>
#include <tuple>
#include <variant>

struct SyncType {};
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

#define ROW(...) std::println("{:<44} sendable={}", #__VA_ARGS__, threadsafe::is_sendable_v<__VA_ARGS__>)

int main() {
    ROW(std::optional<int&>);
    ROW(std::optional<SyncType&>);
    ROW(std::tuple<int&>);
    ROW(std::tuple<SyncType&>);
    ROW(std::pair<int&, int>);
    ROW(std::reference_wrapper<int>);
    ROW(std::reference_wrapper<SyncType>);
    ROW(std::reference_wrapper<const int>);
    ROW(std::atomic<int>);
    ROW(std::atomic<int*>);
    ROW(std::atomic<SyncType*>);
    ROW(std::atomic<int>&);
    ROW(std::atomic<int>*);
    ROW(const int&);
    ROW(const SyncType&);
    ROW(int (&)[4]);
    ROW(std::atomic<int> (&)[4]);
    ROW(void (&)());
    ROW(int SyncType::*);
}
