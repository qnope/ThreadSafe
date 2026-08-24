#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <memory_resource>
#include <optional>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <tuple>
#include <vector>
struct UserCopyCtor { UserCopyCtor(const UserCopyCtor&); };
struct BadCompare { UserCopyCtor state; bool operator()(int,int) const; };
template <class T> struct Box { T value; };
struct MutCache { int raw; mutable int parsed; };
consteval void explain() { threadsafe::assert_synchronizable<const std::optional<int*>>(); }
static_assert((explain(), true));
