#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .build/tests
compiler="${CXX:-c++}"
for source in tests/test_*.cpp; do
  name="$(basename "${source%.cpp}")"
  "$compiler" -std=c++17 -Wall -Wextra -Werror -g -O1 \
    -fsanitize=address,undefined -fno-omit-frame-pointer -I. \
    "$source" -o ".build/tests/$name"
  ".build/tests/$name"
done
