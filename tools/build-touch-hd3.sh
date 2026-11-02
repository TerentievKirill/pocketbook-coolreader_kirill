#!/usr/bin/env bash
# Build on Linux x86_64 with SDK-B288-6.10 (modern glibc/libpng16 firmware).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
export POCKETBOOK_SDK="${POCKETBOOK_SDK:-$root/work/sdk/SDK-B288-6.10}"
if [[ -d "$root/work/host/usr/bin" ]]; then
    export PATH="$root/work/host/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$root/work/host/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
build="${CR3_BUILD_DIR:-$root/work/build-touch-hd3-fw6}"
revision="${CR3_REVISION:-dev}"
# Skin archives are built on the host, never with the SDK target executable.
host_zip=$(command -v zip) || { echo "Install the host zip utility" >&2; exit 1; }
python3 "$root/tools/test-ota-launcher.py"
mkdir -p "$build"
c++ -std=c++11 "$root/tests/ota-version.cpp" -o "$build/ota-version-host"
"$build/ota-version-host"
cmake -S "$root" -B "$build" \
    -DCMAKE_TOOLCHAIN_FILE="$root/tools/toolchain-touch-hd3.cmake" \
    -DZIP_EXECUTABLE="$host_zip" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON \
    -DCMAKE_C_FLAGS_RELEASE='-O2 -g -DNDEBUG' \
    -DCMAKE_CXX_FLAGS_RELEASE='-O2 -g -DNDEBUG -fno-strict-aliasing -fno-delete-null-pointer-checks' \
    -DCR3_RELEASE_VERSION="${CR3_RELEASE_VERSION:-1.0.0}" -DGUI=CRGUI_PB -DDEVICE_NAME=pb360 -DTARGET_TYPE=ARM \
    -DMAX_IMAGE_SCALE_MUL=2 -DPOCKETBOOK_PRO=ON \
    -DPOCKETBOOK_PRO_FW5=ON -DPOCKETBOOK_FW6=ON \
    -DBACKGROUND_CACHE_FILE_CREATION=ON -DENABLE_PB_DB_STATE=OFF
cmake --build "$build" --parallel "${JOBS:-$(nproc)}"
# Preserve the link output, including on no-change incremental builds.
mkdir -p "$build/package"
binary="$build/cr3gui/cr3-pb.app"
debug="$build/cr3gui/cr3-pb.debug"
staged="$build/package/cr3-pb.app"
readelf="$POCKETBOOK_SDK/usr/bin/arm-obreey-linux-gnueabi-readelf"
"$readelf" -S "$binary" | grep '\.debug_info' >/dev/null || { echo 'Missing debug information; relink the application' >&2; exit 1; }
"$POCKETBOOK_SDK/usr/bin/arm-obreey-linux-gnueabi-objcopy" --only-keep-debug "$binary" "$debug"
cp "$binary" "$staged"
"$POCKETBOOK_SDK/usr/bin/arm-obreey-linux-gnueabi-strip" "$staged"
"$POCKETBOOK_SDK/usr/bin/arm-obreey-linux-gnueabi-objcopy" --add-gnu-debuglink="$debug" "$staged"
python3 "$root/tools/test-touch-hd3.py" --build "$build" --application "$staged"
python3 "$root/tools/test-build-metadata.py"
python3 "$root/tools/package-touch-hd3.py" "$build" --binary "$staged" --revision "$revision"
