#include <threadsafe/threadsafe.h>
#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace {
struct Vouched {};
struct Borrower { int* p; };
using IntAlias = int;
using AtomicAlias = std::atomic<int>;
using ArrayAlias = std::atomic<int>[4];
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Vouched);

#define AGREE(T) static_assert(threadsafe::is_synchronizable_type(^^T) \
                               == threadsafe::is_synchronizable_v<T>)

AGREE(int); AGREE(const int); AGREE(volatile int); AGREE(const volatile int);
AGREE(int&); AGREE(int&&); AGREE(const int&); AGREE(int*); AGREE(int* const);
AGREE(const int*); AGREE(const int* const); AGREE(void); AGREE(const void);
AGREE(void()); AGREE(void() const); AGREE(void (*)()); AGREE(void (&)());
AGREE(int[4]); AGREE(const int[4]); AGREE(int[]); AGREE(const int[]);
AGREE(int[2][3]); AGREE(const int[2][3]);
AGREE(std::atomic<int>); AGREE(const std::atomic<int>);
AGREE(volatile std::atomic<int>); AGREE(const volatile std::atomic<int>);
AGREE(std::atomic<int*>); AGREE(const std::atomic<int*>);
AGREE(std::atomic<int>[4]); AGREE(const std::atomic<int>[4]);
AGREE(Vouched); AGREE(const Vouched); AGREE(volatile Vouched);
AGREE(const volatile Vouched); AGREE(Vouched[3]); AGREE(const Vouched[3]);
AGREE(Vouched*); AGREE(Vouched&);
AGREE(Borrower); AGREE(const Borrower);
AGREE(std::vector<int>); AGREE(const std::vector<int>);
AGREE(std::vector<int*>); AGREE(const std::vector<int*>);
AGREE(std::optional<int>); AGREE(const std::optional<int>);
AGREE(std::string); AGREE(const std::string);
AGREE(IntAlias); AGREE(const IntAlias); AGREE(AtomicAlias); AGREE(const AtomicAlias);
AGREE(ArrayAlias); AGREE(const ArrayAlias);
AGREE(int Borrower::*); AGREE(int Borrower::* const);
AGREE(threadsafe::asynchronous_task_launcher);
AGREE(const threadsafe::asynchronous_task_launcher);
AGREE(threadsafe::synchronized_value<int>);
AGREE(const threadsafe::synchronized_value<int>);
int main() {}
