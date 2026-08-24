// copy_on_write<T> is is_sendable-specialized but has no
// is_synchronizable<const copy_on_write<T>> rule, so it cannot appear inside
// anything the const walk visits -- including another copy_on_write.

#include <threadsafe/threadsafe.h>

#include <string>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

struct Body {
    std::string text;
};

struct Document {
    copy_on_write<Body> body;
    int revision;
};

static_assert(is_sendable_v<copy_on_write<Body>>,
              "the flagship type is sendable on its own");
static_assert(!is_synchronizable_v<const copy_on_write<Body>>,
              "but a const copy_on_write is NOT const-synchronizable, although "
              "its whole const interface is operator* -> const Body&");
static_assert(!is_sendable_v<copy_on_write<copy_on_write<Body>>>,
              "so a copy_on_write cannot be nested");
static_assert(!is_sendable_v<copy_on_write<Document>>,
              "and no aggregate holding one can be shared either");
static_assert(!is_synchronizable_v<const std::vector<copy_on_write<Body>>>,
              "nor a container of them");

consteval void explain() {
    threadsafe::assert_synchronizable<const copy_on_write<Body>>();
}
static_assert((explain(), true));