#!/usr/bin/env python3
"""Mutation-test the ThreadSafe test suite: break one rule at a time in a COPY of
the headers, rebuild every test TU, and report whether any test noticed."""
import shutil, subprocess, pathlib, sys, tempfile, os

REPO = pathlib.Path(os.environ.get("THREADSAFE_ROOT",
                    pathlib.Path(__file__).resolve().parents[3]))
SRC  = REPO / "include"
TESTS = sorted((REPO / "tests").glob("*.cpp"))

# (name, relative header, old, new)
MUTANTS = [
 ("sendable-ref-always-true", "threadsafe/details/sendable.h",
  "struct is_sendable<T&> : is_synchronizable<std::remove_cv_t<T>> {};",
  "struct is_sendable<T&> : std::true_type {};"),
 ("sendable-ptr-always-true", "threadsafe/details/sendable.h",
  "struct is_sendable<T*> : is_synchronizable<std::remove_cv_t<T>> {};",
  "struct is_sendable<T*> : std::true_type {};"),
 ("drop-copy-move-guard", "threadsafe/details/utils.h",
  "        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))\n            return false;",
  "        if (false)\n            return false;"),
 ("drop-hijack-guard", "threadsafe/details/utils.h",
  "    return std::meta::is_constructor_template(member)\n        || (std::meta::is_operator_function_template(member)\n            && std::meta::operator_of(member) == std::meta::op_equals);",
  "    return false;"),
 ("drop-unreflectable-state", "threadsafe/details/utils.h",
  "    return !std::meta::is_empty_type(type)\n        && !std::meta::is_polymorphic_type(type)\n        && std::meta::bases_of(type, context).empty()\n        && std::meta::nonstatic_data_members_of(type, context).empty();",
  "    return false;"),
 ("drop-mutable-check", "threadsafe/details/synchronizable.h",
  "            if (!is_synchronizable_type(remove_cv(member_type)))\n                reject(member,\n                       u8\"is mutable, so it is written through a const \"\n                       u8\"reference: its type must be fully synchronizable\");",
  "            if (false)\n                reject(member, u8\"unused\");"),
 ("drop-ref-member-check", "threadsafe/details/synchronizable.h",
  "            if (!is_synchronizable_type(remove_cvref(member_type)))\n                reject(member,\n                       u8\"is a reference: the const stops there — its referent \"\n                       u8\"may be written through another alias, so the \"\n                       u8\"referent must be synchronizable itself\");",
  "            if (false)\n                reject(member, u8\"unused\");"),
 ("drop-const-pointer-check", "threadsafe/details/synchronizable.h",
  "        if (!is_synchronizable_type(pointee))\n            reject(type,\n                   u8\"is a pointer: the const stops at it — the pointee may be \"\n                   u8\"written through another alias, so the pointee must be \"\n                   u8\"synchronizable itself\");",
  "        if (false)\n            reject(type, u8\"unused\");"),
 ("drop-polymorphic-guard", "threadsafe/details/utils.h",
  "        return !std::is_polymorphic_v<T> || std::is_final_v<T>;",
  "        return true;"),
 ("drop-borrowed-range-rule", "threadsafe/details/lifetime_aware.h",
  "    if (trait_value(^^std::ranges::borrowed_range, type))\n        reject(type,\n               u8\"is a borrowed range: a view over someone else's storage, it \"\n               u8\"does not keep its elements alive\");",
  "    if (false)\n        reject(type, u8\"unused\");"),
 ("lifetime-ptr-always-true", "threadsafe/details/lifetime_aware.h",
  "struct is_lifetime_aware<T*> : std::false_type {};",
  "struct is_lifetime_aware<T*> : std::true_type {};"),
 ("lifetime-refwrapper-true", "threadsafe/details/lifetime_aware.h",
  "struct is_lifetime_aware<std::reference_wrapper<T>> : std::false_type {};",
  "struct is_lifetime_aware<std::reference_wrapper<T>> : std::true_type {};"),
 ("cow-never-detach", "threadsafe/details/copy_on_write.h",
  "        if (ptr_.use_count() != 1)",
  "        if (false)"),
 ("cow-always-detach", "threadsafe/details/copy_on_write.h",
  "        if (ptr_.use_count() != 1)",
  "        if (true)"),
 ("syncval-always-shared-mutex", "threadsafe/details/synchronized_value.h",
  "        if constexpr (is_synchronizable_v<const T>) {\n            return ^^std::shared_mutex;\n        } else {\n            return ^^std::mutex;\n        }",
  "        return ^^std::shared_mutex;"),
 ("syncval-drop-sendable-assert", "threadsafe/details/synchronized_value.h",
  "    static_assert(sendable<T>,", "    static_assert(true || sendable<T>,"),
 ("launcher-drop-lifetime-req", "threadsafe/details/asynchronous_task_launcher.h",
  "                       && lifetime_aware<F>\n                       && (sendable<Args> && ...)\n                       && (lifetime_aware<Args> && ...);",
  "                       && (sendable<Args> && ...);"),
 ("launcher-drop-sendable-req", "threadsafe/details/asynchronous_task_launcher.h",
  "concept launchable_scoped_task = ownable_by_launcher<F, Args...>\n                              && sendable<F>\n                              && (sendable<Args> && ...);",
  "concept launchable_scoped_task = ownable_by_launcher<F, Args...>;"),
 ("guard-allow-rvalue-star", "threadsafe/details/synchronized_value.h",
  "    T& operator*() && noexcept = delete(\"a temporary guard is destroyed at the semicolon, so it cannot hand out a reference\");",
  "    T& operator*() && noexcept { return *value_; }"),
]

def build(inc):
    """Return set of test files that FAILED to compile."""
    failed = set()
    procs = []
    for t in TESTS:
        procs.append((t, subprocess.Popen(
            ["g++-16","-std=c++26","-freflection","-fsyntax-only","-I",str(inc),str(t)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)))
    for t,p in procs:
        if p.wait() != 0: failed.add(t.name)
    return failed

base_fail = build(SRC)
if base_fail:
    print("BASELINE IS NOT CLEAN:", base_fail); sys.exit(1)
print("baseline: all", len(TESTS), "test TUs compile\n")
print(f"{'mutant':<32} {'caught?':<9} by")
print("-"*90)
survivors = []
for name, rel, old, new in MUTANTS:
    with tempfile.TemporaryDirectory() as td:
        inc = pathlib.Path(td)/"include"
        shutil.copytree(SRC, inc)
        f = inc/rel
        s = f.read_text()
        if old not in s:
            print(f"{name:<32} {'SKIP':<9} (anchor not found)"); continue
        f.write_text(s.replace(old, new, 1))
        failed = build(inc)
        if failed:
            print(f"{name:<32} {'caught':<9} {', '.join(sorted(x.replace('test_','').replace('.cpp','') for x in failed))}")
        else:
            print(f"{name:<32} {'SURVIVED':<9} <-- no test detects this")
            survivors.append(name)
print("-"*90)
print(f"\n{len(survivors)} survivor(s) of {len(MUTANTS)}: {survivors}")
