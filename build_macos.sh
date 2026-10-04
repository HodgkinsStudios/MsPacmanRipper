#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: build_macos.sh must be run on macOS." >&2
  exit 1
fi

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
mkdir -p bin obj/macos

if [[ -n "${CXX:-}" ]]; then
  CXX_BIN="$CXX"
else
  if ! command -v xcrun >/dev/null 2>&1; then
    echo "ERROR: Xcode Command Line Tools are required. Run: xcode-select --install" >&2
    exit 1
  fi
  CXX_BIN="$(xcrun --find clang++)"
fi

DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}"
ARCH_MODE="${MSPACMANRIPPER_MACOS_ARCHS:-native}"
ARCH_FLAGS=()
case "$ARCH_MODE" in
  native)
    HOST_ARCH="$(uname -m)"
    case "$HOST_ARCH" in
      arm64|x86_64) ARCH_FLAGS=(-arch "$HOST_ARCH") ;;
      *)
        echo "ERROR: Unsupported macOS CPU architecture: $HOST_ARCH" >&2
        exit 2
        ;;
    esac
    ;;
  arm64|x86_64)
    ARCH_FLAGS=(-arch "$ARCH_MODE")
    ;;
  *)
    echo "ERROR: MSPACMANRIPPER_MACOS_ARCHS must be native, arm64, or x86_64." >&2
    echo "Universal binaries are assembled from natively built slices by the macOS CI workflow." >&2
    exit 2
    ;;
esac

COMMON=(-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror "-mmacosx-version-min=$DEPLOYMENT_TARGET" -I"$ROOT/src")
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
  obj="$ROOT/obj/macos/${rel//\//_}"
  obj="${obj%.cpp}.o"
  "$CXX_BIN" "${COMMON[@]}" "${ARCH_FLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

"$CXX_BIN" "${ARCH_FLAGS[@]}" "-mmacosx-version-min=$DEPLOYMENT_TARGET" "${OBJECTS[@]}" -o "$ROOT/bin/MsPacmanRipper"

# Ad-hoc signing gives the locally built Mach-O a valid code signature without requiring
# a Developer ID certificate. Distribution notarization is intentionally not implied.
if command -v codesign >/dev/null 2>&1; then
  codesign --force --sign - "$ROOT/bin/MsPacmanRipper" >/dev/null 2>&1
fi

echo "Built: $ROOT/bin/MsPacmanRipper"
if command -v lipo >/dev/null 2>&1; then
  echo "Architectures: $(lipo -archs "$ROOT/bin/MsPacmanRipper")"
fi
