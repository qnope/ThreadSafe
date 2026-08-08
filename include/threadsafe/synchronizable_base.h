#pragma once

namespace threadsafe {

// True if a T may be used from multiple threads at the same time (≈ Rust Sync).
// Opt-in: specialize on the cv-unqualified, non-reference type. Queries must
// decay T first; cv-qualified queries are not forwarded.
//
// This header holds only the primary template, so sendable.h can build on it;
// the std::atomic rules (which need is_sendable) live in synchronizable.h.
template <class T>
constexpr bool is_synchronizable = false;

}  // namespace threadsafe
