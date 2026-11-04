#!/usr/bin/env bash
# Preserve the original target and cache volume for existing users.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
export CR3_DOCKER_VOLUME="${CR3_DOCKER_VOLUME:-coolreader-touch-hd3-work}"
exec bash "$root/tools/docker-build-pocketbook.sh" pb632-fw6 "$@"
