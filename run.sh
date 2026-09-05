#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
if [[ ! -x "$ROOT/bin/MsPacmanRipper" ]]; then
  "$ROOT/build.sh"
fi
exec "$ROOT/bin/MsPacmanRipper" "$@"
