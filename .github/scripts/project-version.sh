#!/usr/bin/env bash
# Prints the version declared by project(dbus-viewer VERSION x.y.z) in a CMakeLists.txt.
# Usage: project-version.sh [FILE]   (reads stdin when FILE is omitted)
set -euo pipefail

version=$(sed -nE 's/^[[:space:]]*project\([[:space:]]*dbus-viewer[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "${1:-/dev/stdin}" | head -n1)
if [[ -z "$version" ]]; then
  echo "error: no 'project(dbus-viewer VERSION x.y.z' found" >&2
  exit 1
fi
echo "$version"
