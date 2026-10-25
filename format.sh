#!/usr/bin/env bash
# Formats every C++ source of the project with .clang-format.
# Usage: ./format.sh          format files in place
#        ./format.sh --check  only report badly formatted files (non-zero exit if any)
set -euo pipefail

cd "$(dirname "$0")"

clangFormat=${CLANG_FORMAT:-$(command -v clang-format || command -v clang-format-21 || true)}
if [[ -z "$clangFormat" ]]; then
    echo "error: clang-format not found (install it or set CLANG_FORMAT)" >&2
    exit 1
fi

mapfile -t files < <(find src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)

if [[ "${1:-}" == "--check" ]]; then
    "$clangFormat" --dry-run --Werror "${files[@]}"
    echo "All ${#files[@]} files are formatted."
else
    "$clangFormat" -i "${files[@]}"
    echo "Formatted ${#files[@]} files."
fi
