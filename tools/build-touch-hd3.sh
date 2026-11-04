#!/usr/bin/env bash
# Compatibility entry point: the original command always targets PB632.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec python3 "$root/tools/build-pocketbook.py" "$@" --target pb632-fw6
