# CoolReader for PocketBook Touch HD 3

CoolReader for **Touch HD 3 / PB632**, targeting firmware **6.11.2001**.

[Downloads: CoolReader and Easy Connect](https://pocketbook.nimbushub.eu/) · [Installation](https://pocketbook.nimbushub.eu/installation.html) · [GitHub release](https://github.com/dderugin/pocketbook-coolreader/releases/latest)

The current build fixes standby frontlight state and adds large, outlined
settings navigation buttons. It passes all twelve ARM/QEMU regression groups
and package checks and has been installed on the Touch HD 3. Interactive
verification of these latest fixes remains pending. `build-info.json` records
automated results separately from device validation.
Other devices and firmware versions have not been verified by this fork.

New installations use the reading preferences captured from the Touch HD 3:
Droid Sans MTI Arabic at size 49, 100% line spacing, margins of 2 px on the
left/top/bottom and 0 px on the right, and Russian/English hyphenation.
All 154 preferences are stored in `data/touch-hd3/cr3-defaults.ini` and packaged
as `system/share/cr3/cr3-defaults.ini`. The font comes with the target firmware.
Saved user or book settings take precedence; upgrades preserve
`system/config/cr3/cr3.ini`. Settings saves, pinch zoom and normal exit now
retain preferences correctly.

## Install

Extract the release ZIP and merge its **system** and **applications** folders
into the reader’s USB storage, preserving existing settings. Safely eject it,
then open **Applications → CoolReader**. For Library’s **Open with** menu,
follow the [file-association instructions](https://pocketbook.nimbushub.eu/installation.html#file-associations).

See [the detailed guide](README-touch-hd3.txt) for backups and troubleshooting.

## Build and test

### Local Docker build (macOS or Linux)

Requirements: a running Docker engine, Python 3.9+ and curl. The SDK runs in a
Linux AMD64 container; the build also works on Apple Silicon using Rosetta.

For Apple Silicon with Colima, install Rosetta once if needed, then configure
an ARM64 VM with the settings tested on this project:

```sh
softwareupdate --install-rosetta --agree-to-license
colima stop
colima start --arch aarch64 --vm-type vz --vz-rosetta --cpu 6 --memory 12
```

Stopping Colima interrupts its running containers. If it is already running
with suitable settings, skip these setup commands. Docker Desktop users can
use their existing engine with Linux AMD64 emulation enabled.

From the repository root, build and test:

```sh
CR3_REVISION=local bash tools/docker-build-touch-hd3.sh
```

The first run downloads approximately 1.1 GB of pinned SDK/firmware archives,
verifies their SHA-256 hashes, prepares the Linux build environment, builds the
ARM executable, and runs all twelve ARM/QEMU regression groups and package
checks. No remote build server or connected PocketBook is required.

Artifacts are written to `dist/`:

- `coolreader-touch-hd3-fw6.11.2001-local.zip` — installation package.
- `coolreader-touch-hd3-fw6.11.2001-local.zip.sha256` — package checksum.
- `coolreader-touch-hd3-local.tests.json` — automated test results.
- `coolreader-touch-hd3-local.debug` — separate debug symbols.

To change the artifact label or number of parallel jobs:

```sh
JOBS=2 CR3_REVISION=my-test bash tools/docker-build-touch-hd3.sh
```

The default is four parallel jobs. Repeated builds reuse the Docker image,
SDK, firmware runtime and object files. Downloaded archives stay in
`work/downloads/`; extracted inputs and build objects stay in the
`coolreader-touch-hd3-work` Docker volume. Builds leave source files read-only
inside the container and do not install or publish the resulting package.

See [BUILDING.md](BUILDING.md) for cache management, Docker options and details.

### Native Linux build

Use Linux with CMake 3.18+, Make, gettext, zip, OpenSSL tools, Python 3 and the
B288 SDK described in the detailed guide. Keep SDKs, extracted firmware and host tools under `work/`.

```sh
export POCKETBOOK_SDK="$PWD/work/sdk/SDK-B288-6.10"
bash tools/build-touch-hd3.sh
```

The script builds the ARM application, keeps debug symbols, runs twelve ARM/QEMU
regression groups and checks packaging evidence before producing
`dist/coolreader-touch-hd3-fw6.11.2001-dev.zip` and its SHA-256 file.
Set `CR3_REVISION` to label a candidate and `CR3_BUILD_DIR` to use another build
directory. The QEMU tests need the compatible firmware runtime under
`work/firmware/runtime`; see [tests/README.txt](tests/README.txt).

Published binaries belong in GitHub Releases. Generated packages, translations,
SDKs and firmware images are excluded from Git. Packaged styles, hyphenation
patterns, Chinese dictionaries and manuals come from the single asset tree in
`cr3gui/data`; translations are compiled from `cr3gui/po`.

The build vendors FreeType 2.15.0, libjpeg-turbo 3.2.0, libpng 1.6.59,
zlib 1.3.2 and curl 8.23.0 as private static libraries. See
[dependency versions and checksums](thirdparty/versions.json) and
[dependency notes](thirdparty/README.txt). Unused UnRAR was removed; ZIP,
CHM and Word support are retained. HTTPS now verifies certificates using
the firmware trust store. Candidate ZIPs include third-party license notices.
The library upgrade was confirmed working on the Touch HD 3. The latest
navigation/frontlight changes still need interactive device verification.

The application uses official CREngine **3.2.59-1**, pinned at
`dcc2d4c88b7ee66b8ee1223833c1a72cad3cdbc5`, while retaining the PocketBook touch
UI. See [migration status and remaining checks](crengine/POCKETBOOK-PORT.txt).
Earlier device testing confirmed the engine upgrade, margin handling and book
display. Comprehensive hardware and bookmark migration checks remain pending.
Optional shaping/SVG integrations are not enabled yet.

## Source layout

| Directory | Purpose |
| --- | --- |
| `crengine/` | Document parsing, layout and rendering engine |
| `cr3gui/` | PocketBook UI, translations and shared runtime assets |
| `data/touch-hd3/` | Launchers, welcome book and device configuration |
| `thirdparty/`, `tinydict/` | Rendering and document-format dependencies |
| `tools/`, `tests/` | Build, package and regression tooling |
| Website | [Standalone pocketbook-web repository](https://github.com/dderugin/pocketbook-web) |
| Hosting | [Website configuration and runbook](https://github.com/dderugin/pocketbook-web/tree/main/deploy/nginx) |

CMake builds the PocketBook frontend (`CRGUI_PB`). Historical desktop frontends,
the obsolete Jinke metadata plugin, older SDK scripts and prebuilt OTA packages
are not maintained in this fork.

## Origin and license

Based on [blchinezu/pocketbook-coolreader](https://github.com/blchinezu/pocketbook-coolreader)
and the original CoolReader work by Vadim Lopatin and contributors. Older
PocketBook builds and their documentation remain available from that upstream
project and Git history.

CoolReader is licensed under [GPL version 2](LICENSE). Third-party components
retain their own notices and licenses in their source directories.

## Easy Connect companion tool

The [download website](https://pocketbook.nimbushub.eu/) also hosts
[Easy Connect](https://pocketbook.nimbushub.eu/easy-connect.html), a standalone
SSH and FTP app for PocketBook Touch HD 3 / PB632. Its page includes USB
installation, password setup, the SSH pipe-shell command, FTP client settings,
security limits and troubleshooting. CoolReader is not required. Easy Connect
has separate code and downloads in [its repository](https://github.com/dderugin/pocketbook-easy-connect).
