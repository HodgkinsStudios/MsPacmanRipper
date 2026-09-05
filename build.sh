#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
mkdir -p bin obj
CXX="${CXX:-g++}"
COMMON=(-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT/src")
SOURCES=(
  "$ROOT/src/main.cpp"
  "$ROOT/src/crypto/Hash.cpp"
  "$ROOT/src/rom/RomSet.cpp"
  "$ROOT/src/mspacman/Daughterboard.cpp"
  "$ROOT/src/disasm/Z80Disassembler.cpp"
  "$ROOT/src/analysis/StatefulAnalyzer.cpp"
  "$ROOT/src/semantic/SemanticCatalog.cpp"
  "$ROOT/src/semantic/PacmanSemanticFamilies.cpp"
)
OBJECTS=()
for src in "${SOURCES[@]}"; do
  rel="${src#$ROOT/src/}"
  obj="$ROOT/obj/${rel//\//_}"
  obj="${obj%.cpp}.o"
  "$CXX" "${COMMON[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done
"$CXX" "${OBJECTS[@]}" -o "$ROOT/bin/MsPacmanRipper"
echo "Built: $ROOT/bin/MsPacmanRipper"
