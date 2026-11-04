# CoolReader for PocketBook

[Downloads](https://pocketbook.nimbushub.eu/) · [Installation](https://pocketbook.nimbushub.eu/installation.html) · [Release 1.0.3](https://github.com/dderugin/pocketbook-coolreader/releases/tag/v1.0.3)

Release **1.0.3** provides one shared ARMv7 Linux executable and install ZIP for
these exact model/firmware combinations:

| Device | Firmware |
| --- | --- |
| Touch HD 3 / PB632 | U632.6.11.2001 |
| Verse Pro / PB634 | U634.6.10.3425 |
| Era / PB700 | U700.6.10.3209 |
| InkPad 4 / PB743G | U743g.6.11.1683 |

This shared release is experimental. Sixteen ARM regression groups and thirteen
software probes on each additional firmware pass; interactive hardware testing
and end-to-end device OTA acceptance remain pending. Other model/firmware pairs
are rejected. See the [device matrix](DEVICE-SUPPORT.md) and
[shared installation guide](README-pocketbook-armv7.txt) for qualification limits.

Every model uses the same download and Wi-Fi update channel. **Settings → Check
for updates** checks manually; automatic checks are optional. **Settings → Version**
shows the installed release. Moving from the earlier PB632-only release requires
one USB installation of the shared package and launcher; later shared releases
can update over Wi-Fi.

Install ZIPs contain only `system/` and `applications/` at the top level. Guides,
licenses and build reports live under `system/share/cr3/`, keeping them out of
Library. Existing settings, history, bookmarks and custom assets are preserved.

Fresh shared installations use general reading defaults and runtime device DPI.
Saved user and book settings take precedence. The historical `pb632-fw6` build
configuration remains available with its earlier Touch HD 3 reading preferences;
its existing downloads and update channel remain separate.

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

The equivalent target-aware command is
`CR3_REVISION=local bash tools/docker-build-pocketbook.sh pb632-fw6`.
List enabled targets with `python3 tools/targets.py --list`.

The first run downloads approximately 1.1 GB of pinned SDK/firmware archives,
verifies their SHA-256 hashes, prepares the Linux build environment, builds the
ARM executable, and runs all sixteen ARM/QEMU regression groups and package
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

The script builds the ARM application, keeps debug symbols, runs sixteen ARM/QEMU
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
