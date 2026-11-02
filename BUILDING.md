# Local Touch HD 3 build

From the repository root:

```sh
CR3_REVISION=local bash tools/docker-build-touch-hd3.sh
```

This builds the PocketBook ARM executable, runs all thirteen ARM/QEMU regression
groups and package checks, and writes the ZIP, SHA-256 file, test report and
separate debug symbols into `dist/`. It does not install or publish the build.

## Requirements

- Docker with `linux/amd64` containers enabled, Python 3.9+ and curl on the host.
- Internet for the first setup: approximately 1.1 GB for the pinned B288 SDK and
  PocketBook firmware, plus the Ubuntu build image and packages.
- Several GB of Docker disk space. SDK, runtime and incremental objects stay in
  the `coolreader-touch-hd3-work` Docker volume; archives stay in `work/downloads/`.

The SDK contains Linux x86-64 tools. On Apple Silicon, keep the VM ARM64 and
use Rosetta for the AMD64 build container. There is no need for an x86-64 VM.
For Colima, install Rosetta if absent and start the existing profile with:

```sh
softwareupdate --install-rosetta --agree-to-license
colima stop
colima start --arch aarch64 --vm-type vz --vz-rosetta --cpu 6 --memory 12
```

Stopping Colima interrupts its running containers; do that only when those
workloads can stop. The tested Mac configuration is ARM64/VZ, Rosetta enabled,
6 CPUs and 12 GiB RAM. The build wrapper does not restart or reconfigure Docker.

## What the wrapper does

1. Downloads the SDK and firmware from the URLs pinned in
   `tools/docker/inputs.json`, resumes incomplete transfers and verifies SHA-256.
2. Builds an Ubuntu 24.04 AMD64 image with the necessary host tools.
3. Extracts the SDK into the Linux volume, repairing only internal symlinks.
4. Extracts the firmware's ext2 and SquashFS files using userspace tools to
   obtain the ARM loader, libraries and test fonts. No privileged mounts,
   physical reader, SSH connection or Ubuntu server are needed.
5. Runs `tools/build-touch-hd3.sh` as the invoking user's UID/GID, with read-only
   source files and writable cache/output directories.

SDK/firmware archives and extracted proprietary inputs are local caches and
are excluded from Git. Nothing from your home directory or SSH configuration
is mounted into the container.

## Options and output

```sh
JOBS=2 CR3_REVISION=my-test bash tools/docker-build-touch-hd3.sh
```

`JOBS` defaults to 4. Revision names may contain letters, digits, `.`, `_` and
`-`. Repeated builds reuse the image, SDK, firmware runtime and compiled objects.
Use one build at a time per cache volume. Set `CR3_DOCKER_VOLUME` for an
independent cache or `CR3_DOCKER_IMAGE` to choose the image tag.

Artifacts:

- `dist/coolreader-touch-hd3-fw6.11.2001-<revision>.zip`
- The matching `.zip.sha256`
- `dist/coolreader-touch-hd3-<revision>.tests.json`
- `dist/coolreader-touch-hd3-<revision>.debug`

The ZIP's `build-info.json` identifies its source commit, uncommitted changes,
dependencies, executable hash and automated test evidence. A local build does
not inherit the published release's physical-device confirmation.

To remove only the Docker build cache, stop any build using it, then run:

```sh
docker volume rm coolreader-touch-hd3-work
```

This removes extracted inputs and object files, not the source, archives or
`dist/` artifacts. The next build prepares the volume again.

## Native Linux builds

The original script remains available for an already configured Linux x86-64
SDK environment:

```sh
export POCKETBOOK_SDK="$PWD/work/sdk/SDK-B288-6.10"
bash tools/build-touch-hd3.sh
```

Tests use the existing bundled QEMU when available, otherwise `qemu-arm` on
PATH. `CR3_QEMU` selects an explicit executable. The firmware runtime and fonts
must have the layout described in `tests/README.txt`; the Docker bootstrap
prepares that layout automatically.

## Versioned OTA releases

Set `CR3_RELEASE_VERSION=1.0.0` (SemVer MAJOR.MINOR.PATCH) alongside the build
revision. Increase the version for every published package; never replace the
bytes of a published version. The engine version is tracked separately.
The initial OTA-enabled build requires one USB installation because older FW6
builds explicitly disabled OTA and their launcher cannot install staged updates.
After that, Settings → OTA Update reads the PB632/FW6 stable channel from
https://pocketbook.nimbushub.eu/ota/pb632-fw6.txt and downloads the immutable
version path. Only newer versions for PB632 firmware U632.6.11.2001 are accepted.
The package checksum and embedded release version must match before staging.
On restart, the launcher merges assets in a separate directory and switches it
into place, preserving configuration, history, last book and custom assets.
Interrupted switches recover the previous directory; a failed first application
run rolls back. Keep enough free storage for the ZIP and two copies of assets.
The installer uses the existing launcher; future launcher changes require USB.

OTA package extraction has an ARM regression for wrong checksums, path traversal,
missing binaries and wrong versions. The launcher and version comparison also
have isolated host regressions. Physical-device OTA testing is still required.
