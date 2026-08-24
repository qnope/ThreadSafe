#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
#include <memory>

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

struct Body { std::string text; };
struct Document { copy_on_write<Body> body; int revision; };

// What DOES work today?
static_assert(is_sendable_v<copy_on_write<Body>>);
static_assert(is_sendable_v<Document>, "a plain struct holding a cow IS sendable");
static_assert(is_lifetime_aware_v<Document>, "and lifetime aware");
static_assert(is_sendable_v<std::vector<copy_on_write<Body>>>, "vector of cow is sendable");
static_assert(is_sendable_v<std::unique_ptr<Document>>);

// launch_task with a Document by value?
static_assert(threadsafe::launchable_task<decltype([](Document){}), Document>,
              "you CAN launch a task taking a Document by value");
static_assert(threadsafe::launchable_task<decltype([](std::vector<copy_on_write<Body>>){}),
                                          std::vector<copy_on_write<Body>>>);

// What fails
static_assert(!is_synchronizable_v<const copy_on_write<Body>>);
static_assert(!is_sendable_v<copy_on_write<Document>>);
static_assert(!is_sendable_v<copy_on_write<copy_on_write<Body>>>);
static_assert(!is_synchronizable_v<const std::vector<copy_on_write<Body>>>);
static_assert(!is_synchronizable_v<const Document>);
