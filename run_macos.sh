#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: run_macos.sh must be run on macOS." >&2
  exit 1
fi

if [[ ! -x "$ROOT/bin/MsPacmanRipper" ]]; then
  "$ROOT/build_macos.sh"
fi

exec "$ROOT/bin/MsPacmanRipper" "$@"
