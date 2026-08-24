#include <threadsafe/threadsafe.h>
#include <string>
#include <functional>
#include <memory>

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

struct Body { std::string text; };

// The dangerous shape: one handle, use_count()==1, owner calls as_mutable()
// in place while another thread reads through a const alias.
// Every alias route must stay CLOSED even with the fix in place.
static_assert(!is_sendable_v<const copy_on_write<Body>&>, "const& alias");
static_assert(!is_sendable_v<copy_on_write<Body>&>, "& alias");
static_assert(!is_sendable_v<const copy_on_write<Body>*>, "const* alias");
static_assert(!is_sendable_v<copy_on_write<Body>*>, "* alias");
static_assert(!is_sendable_v<std::reference_wrapper<const copy_on_write<Body>>>,
              "cref alias");
static_assert(!is_sendable_v<std::shared_ptr<copy_on_write<Body>>>,
              "shared_ptr alias");
static_assert(!is_synchronizable_v<const std::shared_ptr<copy_on_write<Body>>>);
static_assert(!is_synchronizable_v<const copy_on_write<Body>*>);
struct RefHolder { const copy_on_write<Body>& r; };
static_assert(!is_synchronizable_v<const RefHolder>, "reference member alias");
static_assert(!is_sendable_v<RefHolder>);
struct PtrHolder { const copy_on_write<Body>* p; };
static_assert(!is_synchronizable_v<const PtrHolder>, "pointer member alias");
static_assert(!is_sendable_v<PtrHolder>);
// non-const handle still unshareable
static_assert(!is_synchronizable_v<copy_on_write<Body>>);
