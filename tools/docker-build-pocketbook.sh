#!/usr/bin/env bash
# Local Linux SDK build on macOS/Apple Silicon or Linux. Docker daemon required.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
image="${CR3_DOCKER_IMAGE:-pocketbook-coolreader-build:local}"
volume="${CR3_DOCKER_VOLUME:-coolreader-${1:-pb632-fw6}-work}"
target="${1:-pb632-fw6}"
if [[ "$#" -gt 1 ]]; then
    echo 'Usage: bash tools/docker-build-pocketbook.sh [target]' >&2
    exit 2
fi
python3 "$root/tools/targets.py" --target "$target" >/dev/null
revision="${CR3_REVISION:-local}"
if [[ ! "$revision" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]]; then
    echo 'CR3_REVISION must contain only letters, digits, dots, underscores and hyphens' >&2
    exit 2
fi
command -v docker >/dev/null
command -v python3 >/dev/null
docker info >/dev/null
mkdir -p "$root/work/downloads" "$root/work/qualification" "$root/dist"
python3 "$root/tools/docker/download_inputs.py" "$root/work/downloads" --target "$target"
docker build --platform linux/amd64 -t "$image" -f "$root/tools/docker/Dockerfile" "$root/tools/docker"
docker volume create "$volume" >/dev/null
# Sources are read-only. Linux filesystem storage holds SDK and incremental
# objects; only dist/ is written back to the host. No SSH or reader is needed.
docker run --rm --init --platform linux/amd64 \
    --mount "type=bind,source=$root,target=/src,readonly" \
    --mount "type=volume,source=$volume,target=/src/work" \
    --mount "type=bind,source=$root/work/downloads,target=/downloads,readonly" \
    --mount "type=bind,source=$root/dist,target=/src/dist" \
    --mount "type=bind,source=$root/work/qualification,target=/qualification" \
    -e CR3_QUALIFICATION_DIR=/qualification \
    -e CR3_ARCHIVE_DIR=/downloads \
    -e "CR3_TARGET=$target" -e "CR3_RELEASE_VERSION=${CR3_RELEASE_VERSION:-1.0.4}" -e "CR3_REVISION=$revision" -e "JOBS=${JOBS:-4}" \
    -e "CR3_HOST_UID=$(id -u)" -e "CR3_HOST_GID=$(id -g)" \
    "$image" bash /src/tools/docker/entrypoint.sh
