#include <threadsafe/threadsafe.h>
#include <atomic>

struct SyncTag {};
struct Host { int field_; void method(); };
class Incomplete;

struct HoldsVoidPtr        { void *opaque_; };
struct HoldsConstVoidPtr   { const void *opaque_; };
struct HoldsIncompletePtr  { Incomplete *impl_; };
struct HoldsArrayPtr       { int (*rows_)[4]; };
struct HoldsSyncArrayPtr   { std::atomic<int> (*rows_)[4]; };
struct HoldsDataMemberPtr  { int Host::*offset_; };
struct HoldsMemberFnPtr    { void (Host::*method_)(); };
struct HoldsFnPtr          { void (*callback_)(); };
struct HoldsFnRef          { void (&callback_)(); };
struct HoldsConstPtrToNonConst { int *const pinned_; };
struct HoldsPtrToSyncTag   { SyncTag *tagged_; };
struct HoldsNullptrT       { decltype(nullptr) nothing_; };

template <>
struct threadsafe::is_synchronizable<SyncTag> : std::true_type {};

using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const HoldsVoidPtr>,        "HoldsVoidPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsConstVoidPtr>,   "HoldsConstVoidPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsIncompletePtr>,  "HoldsIncompletePtr TRUE");
static_assert(!is_synchronizable_v<const HoldsArrayPtr>,       "HoldsArrayPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsSyncArrayPtr>,   "HoldsSyncArrayPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsDataMemberPtr>,  "HoldsDataMemberPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsMemberFnPtr>,    "HoldsMemberFnPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsFnPtr>,          "HoldsFnPtr TRUE");
static_assert(!is_synchronizable_v<const HoldsFnRef>,          "HoldsFnRef TRUE");
static_assert(!is_synchronizable_v<const HoldsConstPtrToNonConst>, "HoldsConstPtrToNonConst TRUE");
static_assert(!is_synchronizable_v<const HoldsPtrToSyncTag>,   "HoldsPtrToSyncTag TRUE");
static_assert(!is_synchronizable_v<const HoldsNullptrT>,       "HoldsNullptrT TRUE");
// top-level pointer types
static_assert(!is_synchronizable_v<int Host::*const>,          "int Host::* const TRUE");
static_assert(!is_synchronizable_v<void (Host::*const)()>,     "memfn ptr const TRUE");
static_assert(!is_synchronizable_v<void (*const)()>,           "fn ptr const TRUE");
static_assert(!is_synchronizable_v<const void *const>,         "const void* const TRUE");
static_assert(!is_synchronizable_v<Incomplete *const>,         "Incomplete* const TRUE");
static_assert(!is_synchronizable_v<const std::nullptr_t>,      "const nullptr_t TRUE");
