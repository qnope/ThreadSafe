#!/usr/bin/env bash
# Every file here is meant to fail. Compile them one by one, show the message
# the library produced, and report a file that compiles as a regression.
#
#   ./show_errors.sh          all of them
#   ./show_errors.sh 08       only the ones whose name matches
set -u

script_directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
include_directory="$script_directory/../../include"
compiler="${CXX:-g++-16}"
name_filter="${1:-}"

unexpected_success=0

for source_file in "$script_directory"/*.cpp; do
    file_name="$(basename "$source_file")"
    if [ -n "$name_filter" ] && [[ $file_name != *$name_filter* ]]; then
        continue
    fi

    printf '\n\033[1m=== %s ===\033[0m\n' "$file_name"
    if diagnostics="$("$compiler" -std=c++26 -freflection -fsyntax-only \
            -I"$include_directory" "$source_file" 2>&1)"; then
        printf '\033[31mit compiled — the library missed this one\033[0m\n'
        unexpected_success=1
        continue
    fi

    reason="$(printf '%s\n' "$diagnostics" \
        | grep -E "what\(\)|static assertion failed" \
        | sed -E "s/.*what\(\)': '(.*)'\$/\1/;s/.*static assertion failed: //")"

    printf '\033[33m%s\033[0m\n' "${reason:-$diagnostics}"
done

exit "$unexpected_success"
