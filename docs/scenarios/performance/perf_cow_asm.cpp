#include <threadsafe/threadsafe.h>
#include <memory>

struct big { int table[64]; };

int& via_cow(threadsafe::copy_on_write<big>& value) { return value.as_mutable().table[0]; }
int& via_shared_ptr(std::shared_ptr<big>& value) { return value->table[0]; }

int read_via_cow(const threadsafe::copy_on_write<big>& value) { return value->table[0]; }
int read_via_shared_ptr(const std::shared_ptr<const big>& value) { return value->table[0]; }
