#!/usr/bin/env bash
# Builds and runs the host tests (see tests/host/README.md):
#
#   tests/host/run.sh                 # every tests/host/test_<unit>.cpp
#   tests/host/run.sh cloud_pairing   # only the units named
#
# test_<unit>.cpp is linked with components/gbb_dongle/<unit>.cpp and built
# with AddressSanitizer and UBSan by $CXX (default c++). ArduinoJson.h comes
# from $ARDUINOJSON_INCLUDE or, by default, from a firmware build's managed
# components, so the tests use the version ESPHome pins.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
component="$root/components/gbb_dongle"
out="$here/build"

json="${ARDUINOJSON_INCLUDE:-}"
if [ -z "$json" ]; then
  for header in "$root"/firmware/.esphome/build/*/managed_components/bblanchon__arduinojson/src/ArduinoJson.h; do
    if [ -f "$header" ]; then
      json="$(dirname "$header")"
      break
    fi
  done
fi
if [ ! -f "$json/ArduinoJson.h" ]; then
  echo "ArduinoJson.h not found. Compile any firmware config once, e.g." >&2
  echo "  uv run esphome -s external_components_source ../components compile firmware/gbbdongle-factory.yaml" >&2
  echo "or set ARDUINOJSON_INCLUDE to a directory that holds ArduinoJson.h." >&2
  exit 2
fi

units=()
if [ "$#" -gt 0 ]; then
  units=("$@")
else
  for test in "$here"/test_*.cpp; do
    name="$(basename "$test" .cpp)"
    units+=("${name#test_}")
  done
fi

cxx="${CXX:-c++}"
flags=(-std=gnu++20 -g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all)
# GCC reports ArduinoJson's iterators as maybe-uninitialized once they are
# inlined into our code, where -isystem no longer hides the warning.
if ! "$cxx" -dM -E -x c++ /dev/null | grep -q __clang__; then
  flags+=(-Wno-maybe-uninitialized)
fi

mkdir -p "$out"
failed=()
for unit in "${units[@]}"; do
  echo "== $unit"
  if "$cxx" "${flags[@]}" -I "$here/stubs" -I "$component" -isystem "$json" \
      "$here/test_$unit.cpp" "$component/$unit.cpp" -lpthread -o "$out/test_$unit" &&
    "$out/test_$unit"; then
    :
  else
    failed+=("$unit")
  fi
done

if [ "${#failed[@]}" -gt 0 ]; then
  echo "Failed: ${failed[*]}" >&2
  exit 1
fi
