#!/usr/bin/env bash
# Replays every compile-time scenario of the audit and reports, per directory,
# how many compile and how many are rejected.
#
#   ./replay.sh [directory ...]      (default: every directory next to this script)
#
# A scenario that COMPILES asserts that the library answers as the file states.
# A scenario that is REJECTED is usually a diagnostics case: the rejection text
# is the artefact, and docs/04-diagnostics.md quotes it.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INCLUDE="${THREADSAFE_INCLUDE:-$ROOT/../../include}"
CXX="${CXX:-g++-16}"
FLAGS=(-std=c++26 -freflection -I"$INCLUDE" -fsyntax-only)

dirs=("$@")
[ ${#dirs[@]} -eq 0 ] && dirs=(adversary traits helpers diagnostics ergonomics performance)

total_ok=0 total_no=0
for dir in "${dirs[@]}"; do
    [ -d "$ROOT/$dir" ] || continue
    ok=0 no=0
    for file in "$ROOT/$dir"/*.cpp; do
        [ -e "$file" ] || continue
        if "$CXX" "${FLAGS[@]}" "$file" >/dev/null 2>&1; then
            ok=$((ok + 1))
        else
            no=$((no + 1))
            [ "${VERBOSE:-0}" = 1 ] && echo "    rejected: $(basename "$file")"
        fi
    done
    printf '%-14s %3d compiled  %3d rejected\n' "$dir" "$ok" "$no"
    total_ok=$((total_ok + ok)) total_no=$((total_no + no))
done
printf '%-14s %3d compiled  %3d rejected\n' TOTAL "$total_ok" "$total_no"
