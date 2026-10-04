#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <mspacman.zip-or-rom-directory> <output-directory>" >&2
  exit 2
fi

ENGINE="${CONTAINER_ENGINE:-docker}"
IMAGE="${MSPACMANRIPPER_IMAGE:-ghcr.io/hodgkinsstudios/mspacmanripper:latest}"

if ! command -v "$ENGINE" >/dev/null 2>&1; then
  echo "ERROR: container engine '$ENGINE' was not found." >&2
  echo "Set CONTAINER_ENGINE=podman to use Podman instead." >&2
  exit 1
fi

INPUT="$(realpath "$1")"
OUTPUT="$2"
mkdir -p "$OUTPUT"
OUTPUT="$(realpath "$OUTPUT")"

if [[ ! -e "$INPUT" ]]; then
  echo "ERROR: input does not exist: $INPUT" >&2
  exit 1
fi

SELINUX_SUFFIX=""
if command -v getenforce >/dev/null 2>&1; then
  case "$(getenforce 2>/dev/null || true)" in
    Enforcing|Permissive) SELINUX_SUFFIX=",Z" ;;
  esac
fi

if [[ -d "$INPUT" ]]; then
  CONTAINER_INPUT="/input/romset"
else
  CONTAINER_INPUT="/input/$(basename "$INPUT")"
fi

exec "$ENGINE" run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$INPUT:$CONTAINER_INPUT:ro$SELINUX_SUFFIX" \
  -v "$OUTPUT:/output:rw$SELINUX_SUFFIX" \
  "$IMAGE" "$CONTAINER_INPUT" /output
