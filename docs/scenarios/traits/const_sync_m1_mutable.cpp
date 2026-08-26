#include <threadsafe/threadsafe.h>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>

struct MutMutex      { mutable std::mutex mutex_; int value_; };
struct MutSharedMutex{ mutable std::shared_mutex mutex_; int value_; };
struct MutOnceFlag   { mutable std::once_flag flag_; int value_; };
struct MutAtomicFlag { mutable std::atomic_flag flag_; int value_; };
struct MutAtomicInt  { mutable std::atomic<int> hits_; };
struct MutPtr        { mutable int *pointer_; };
struct MutAtomicPtr  { mutable std::atomic<int> *pointer_; };
struct BaseWithMutable { mutable std::atomic<int> hits_; };
struct DerivedFromMutable : BaseWithMutable {};
struct BaseWithBadMutable { mutable int hits_; };
struct DerivedFromBadMutable : BaseWithBadMutable {};
struct InnerMutable  { mutable int hits_; };
struct OuterOfMutable{ InnerMutable inner_; };
struct OuterMutableMember { mutable InnerMutable inner_; };
struct MutStdString  { mutable std::string cached_; };

using threadsafe::is_synchronizable_v;

// polarity probe: each line claims FALSE; a failing line means the answer is TRUE
static_assert(!is_synchronizable_v<const MutMutex>,        "MutMutex TRUE");
static_assert(!is_synchronizable_v<const MutSharedMutex>,  "MutSharedMutex TRUE");
static_assert(!is_synchronizable_v<const MutOnceFlag>,     "MutOnceFlag TRUE");
static_assert(!is_synchronizable_v<const MutAtomicFlag>,   "MutAtomicFlag TRUE");
static_assert(!is_synchronizable_v<const MutAtomicInt>,    "MutAtomicInt TRUE");
static_assert(!is_synchronizable_v<const MutPtr>,          "MutPtr TRUE");
static_assert(!is_synchronizable_v<const MutAtomicPtr>,    "MutAtomicPtr TRUE");
static_assert(!is_synchronizable_v<const DerivedFromMutable>, "DerivedFromMutable TRUE");
static_assert(!is_synchronizable_v<const DerivedFromBadMutable>, "DerivedFromBadMutable TRUE");
static_assert(!is_synchronizable_v<const OuterOfMutable>,  "OuterOfMutable TRUE");
static_assert(!is_synchronizable_v<const OuterMutableMember>, "OuterMutableMember TRUE");
static_assert(!is_synchronizable_v<const MutStdString>,    "MutStdString TRUE");
