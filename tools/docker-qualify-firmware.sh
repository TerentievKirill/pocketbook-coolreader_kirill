#!/usr/bin/env bash
# Inspect firmware candidates using the existing PB632 binary and test executables.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ "$#" -eq 0 ]]; then
    echo 'Usage: bash tools/docker-qualify-firmware.sh pb634 [pb700 pb743g]' >&2
    exit 2
fi
image="${CR3_DOCKER_IMAGE:-pocketbook-coolreader-build:local}"
volume="${CR3_BASELINE_VOLUME:-coolreader-touch-hd3-work}"
baseline_target="${CR3_BASELINE_TARGET:-pb632-fw6}"
python3 "$root/tools/targets.py" --target "$baseline_target" >/dev/null
# Downloads are pinned separately from build targets. No new device is enabled.
python3 - "$root" "$@" <<'PY'
from pathlib import Path
import json, subprocess, sys
root = Path(sys.argv[1])
sys.path.insert(0, str(root / 'tools/docker'))
from download_inputs import sha256
manifest = json.loads((root / 'config/firmware-candidates.json').read_text())
devices = {entry['id']: entry for entry in manifest['devices']}
if set(sys.argv[2:]) - devices.keys():
    raise SystemExit('Unknown firmware candidate; select from: ' + ', '.join(devices))
cache = root / 'work/downloads'
cache.mkdir(parents=True, exist_ok=True)
for name in sys.argv[2:]:
    entry = devices[name]['archive']
    target = cache / entry['filename']
    if not target.exists():
        partial = target.with_suffix('.zip.part')
        subprocess.run(['curl', '--fail', '--location', '--retry', '3', '--continue-at', '-',
                        entry['url'], '--output', str(partial)], check=True)
        if sha256(partial) != entry['sha256']:
            raise SystemExit('Firmware checksum mismatch: ' + name)
        partial.replace(target)
    if sha256(target) != entry['sha256']:
        raise SystemExit('Firmware checksum mismatch: ' + name)
    print('Verified firmware archive: ' + name, flush=True)
PY
docker image inspect "$image" >/dev/null
docker volume inspect "$volume" >/dev/null
mkdir -p "$root/work/qualification"
for device in "$@"; do
    docker run --rm --init --platform linux/amd64 --user "$(id -u):$(id -g)" \
        --mount "type=bind,source=$root,target=/src,readonly" \
        --mount "type=bind,source=$root/work/qualification,target=/qualification" \
        --mount "type=bind,source=$root/work/downloads,target=/downloads,readonly" \
        --mount "type=volume,source=$volume,target=/baseline,readonly" \
        "$image" python3 tools/qualify-pocketbook-firmware.py \
        --device "$device" --archives /downloads --output "/qualification/$device" \
        --baseline-build "/baseline/build-docker-$baseline_target" \
        --baseline-firmware /baseline/firmware \
        --readelf /baseline/sdk/SDK-B288-6.10/usr/bin/arm-obreey-linux-gnueabi-readelf
done
