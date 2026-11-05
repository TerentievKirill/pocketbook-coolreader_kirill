# PocketBook device expansion

There are two build families: the existing `pb632-fw6` configuration and the
experimental `pocketbook-armv7-softfp` shared package. The shared package uses
one executable on four known models, with exact firmware snapshots for validation. Runtime profiles live
in `config/pocketbook-devices.json`; they are not separate per-model binaries.
Earlier PB632 builds have physical-reader evidence; every new package records
its own device validation as not performed. A successful cross-build or firmware
compatibility check does not establish hardware support.

## Compatibility matrix

| Device | Model | Status in this fork | Evidence needed next |
| --- | --- | --- | --- |
| Touch HD 3 | PB632 | Existing target and experimental shared package; U632.6.11.2001 | Repeat interactive checks for the new binary |
| Verse Pro | PB634 | Experimental shared package; U634.6.10.3425 | Physical touch, buttons, frontlight, sleep testing |
| Era | PB700 | Experimental shared package; U700.6.10.3209 | Physical buttons, rotation after wake and sleep testing |
| InkPad 4 | PB743G | Experimental shared package; U743g.6.11.1683 | Physical large-screen, rotation and sleep testing |
| Verse / Era Lite | PB629 / PB710 | Future candidates | Firmware qualification and device profiles |
| InkPad 3 / 3 Pro | PB740 / PB740-2 | Future candidates | Firmware qualification and device profiles |
| Verse Pro Color / Era Color | PB634K3 / PB700K3 | Candidate; no downloadable build | Firmware inventory and grayscale rendering on color framebuffer |
| InkPad Color / Color 2 / Color 3 | PB741 / PB743C / PB743K3 | Candidate; no downloadable build | Firmware inventory, framebuffer geometry, grayscale rendering |
| InkPad One | PB1030 | Candidate; separate ABI investigation | Hard-float SDK/runtime inventory, input, large-screen scaling |

This is a proposed test set, not a complete product catalog. Android-based
PocketBooks need a separate Android application and are outside this Linux port.
Do not assign a firmware family from its major/minor version or the CPU name.

Reference evidence inspected on 2026-11-04:

- [KOReader device profiles](https://github.com/koreader/koreader/blob/master/frontend/device/pocketbook/device.lua)
  identify model-specific DPI, rotation, buttons, natural light and color buffers.
  PB1030 selects the `pocketbookhf` update channel.
- [KOReader build definitions](https://github.com/koreader/koreader-base/blob/master/Makefile.defs)
  distinguish softfp `pocketbook` and 32-bit hard-float `pocketbookhf`. Do not
  infer an ARM64 userland from a 64-bit processor.
- [KOReader InkView definitions](https://github.com/koreader/koreader-base/blob/master/ffi/inkview_h.lua)
  select interfaces by firmware version.

## Build targets

`config/targets/*.json` contains build families. The shared target's runtime
registry identifies models, firmware, DPI, dimensions and skins within one binary.
Future devices belong in the matrix until their compatibility is known.
List targets with `python3 tools/targets.py --list`; inspect one with
`python3 tools/targets.py --target pb632-fw6`.

A profile selects compiler prefix, CPU flags, SDK sysroot, firmware runtime and
loader, CMake options, package assets, artifact names and pinned input manifest.
The build uses `tools/toolchain-pocketbook.cmake`. Build directories are bound to
the profile and SDK path; changed profiles require a separate build directory.
Docker volumes are bound to their input manifest and profile. Existing PB632
input caches can be reused, but the first profiled build uses a new object directory.

Build and package:

```sh
CR3_REVISION=profiles bash tools/docker-build-pocketbook.sh pb632-fw6
```

For a native Linux host with the SDK and extracted firmware available:

```sh
CR3_REVISION=profiles python3 tools/build-pocketbook.py --target pb632-fw6
```

`POCKETBOOK_SDK`, `CR3_FIRMWARE_DIR` and `CR3_BUILD_DIR` override native input and
output locations. Firmware directories contain `runtime/lib` and
`ebrmain/cramfs/fonts`, as in the existing PB632 test environment. Keep build
outputs outside the tracked source files.

The historical `build-touch-hd3.sh`, `docker-build-touch-hd3.sh` and
`package-touch-hd3.py` commands retain the PB632 target and artifact names.

## Firmware verification

On Linux, using the target SDK's readelf:

```sh
python3 tools/check-pocketbook-runtime.py path/to/cr3-pb.app \
  --target pb632-fw6 --runtime path/to/extracted/runtime
```

This checks ELF class, machine, endianness, ARM float ABI, requested loader,
transitive library dependencies, required symbol versions and required global
dynamic symbols against exports from the complete dependency closure.
Resolution stays inside the extracted firmware; it never invokes host `ldd`.
It does not prove CPU instruction availability, symbol signatures, InkView
structure layout, runtime dlopen/dlsym paths, kernel behavior or hardware functionality.

Every ARM regression run includes this check. Its report is bound to the binary,
source, compiler flags and canonical target profile. Packaging rejects missing or
mismatched target evidence. Build metadata includes the complete target profile.

## Admission of another target

1. Obtain the exact official firmware and corresponding SDK; record download
   provenance and SHA-256. Extract and inspect the loader, ELF ABI, libc/C++
   versions, InkView exports and header structures. Do not reuse PB632 inputs
   as evidence for another model.
2. Choose a baseline within a compatible ABI family. Verify CPU instructions,
   float ABI and runtime requirements. Older-runtime builds can use a recent
   compiler with a compatible sysroot. Keep bundled library symbols private.
3. Add a target profile and pinned input manifest. Adapt firmware extraction if
   its image layout differs from the currently supported ext2/SquashFS layout.
   Supply appropriate launchers, defaults and association examples. The current
   sixteen regressions include PB632-specific UI/OTA fixtures; adapt them to the
   new target before claiming coverage.
4. Add runtime capability detection and InkView compatibility handling where
   needed. Verify DPI/skin scaling, color-buffer conversion, button rotation,
   touch-only navigation, dictionary and optional frontlight APIs.
5. Run dependency verification and document/state/package regressions against
   each selected firmware runtime. Test representative readers physically:
   first page, EPUB/FB2, menus, touch/pinch, buttons, all rotations, dictionary,
   light/warmth, sleep/wake, overnight power use and saved position/settings.
6. Add compatibility-aware OTA selection and package identity checks before
   enabling updates for that target. The revised shared channel admits registered models with compatible API levels;
   the historical PB632 channel remains separate. A build profile alone does not
   extend the shared compatibility list.
7. Publish experimental USB packages with exact model/firmware requirements,
   checksums, matching source and debug symbols. Promote only the combinations
   with recorded hardware evidence. Add an enabled target to the CI matrix.

The next useful hardware sample is one Verse/Verse Pro, followed by an Era and
an InkPad. Color models and InkPad One need additional display/ABI validation.

## Reproduce firmware qualification without a reader

The exact official images selected for Verse Pro, Era and InkPad 4 are pinned in
`config/firmware-candidates.json`. The support API returns unsorted releases;
these were selected by numeric version, then checked against the embedded
`fwinfo.xml` version. Archive checksums are computed from the downloaded files.

| Candidate | Inspected firmware | libc | Application ABI |
| --- | --- | --- | --- |
| Verse Pro / PB634 | U634.6.10.3425 | 2.39 | ARM ELF32, soft-float calling convention |
| Era / PB700 | U700.6.10.3209 | 2.39 | ARM ELF32, soft-float calling convention |
| InkPad 4 / PB743g | U743g.6.11.1683 | 2.41 | ARM ELF32, soft-float calling convention |

These firmware images qualify the shared build family; they are not separate
build targets. After a successful local PB632 Docker build, run:

```sh
bash tools/docker-qualify-firmware.sh pb634 pb700 pb743g
```

The wrapper verifies/downloads the pinned archives and reads the existing
`coolreader-touch-hd3-work` build volume without modifying it. Set
`CR3_BASELINE_VOLUME=coolreader-pb632-fw6-work` if the baseline was built with the
generic Docker command's default volume instead of the historical HD3 wrapper.
The image and baseline build must already exist. Nothing is installed on a reader.

Reports and extraction logs are written to
`work/qualification/<candidate>/qualification.json`. They record archive and
runtime library hashes, tool hashes, the tested binary and original test report,
loader/dependency traces, required InkView exports, ARM attributes and software
software probes using the already-built ARM regression executables. They are
separate from package admission and do not relabel the PB632 test report.

The probes cover rendering, image decoding, dictionaries, reading-state migration,
history, archives and UI helpers. They use the selected build's package defaults
and mocked hardware; they do not validate the candidate's screen, buttons, sleep,
battery, actual frontlight hardware, TLS transfers or device-specific OTA.
Application startup uses eager binding and stops at an eight-second hardware
boundary; passing requires evidence that the dynamic loader transferred control
to the application. It does not require emulated hardware initialization to finish.

The initial audit found that newer firmware libcrypto builds omit eleven legacy
`ENGINE_*` exports even though the `OPENSSL_3.0.0` version namespace exists.
Checking version namespaces alone missed this. The verifier now checks individual
imports, and bundled curl is built with `OPENSSL_NO_ENGINE`; ordinary HTTPS uses
OpenSSL's supported APIs. A regression preserves the same-version/missing-symbol
failure case. CPU requirements and InkView structure layouts still need review
before an experimental package is offered for another model.

The shared package now lowers its CPU requirements to the same ARMv7-A,
NEON/VFPv3 level advertised by the inspected stock reader binaries. Its primary
suite contains sixteen groups, including real rendering at every registered
portrait/landscape size and user/book preference precedence. Additional-runtime
qualification includes thirteen probes. OTA is enabled through one shared compatibility descriptor and ZIP;
its launcher uses separate staging/rollback directories and ignores PB632-only stages. Production PB632 OTA
remains separate. Use `CR3_BASELINE_TARGET=pocketbook-armv7-softfp` and
`CR3_BASELINE_VOLUME=coolreader-pocketbook-armv7-softfp-work` to repeat the
standalone qualification wrapper against a shared build.

## Shared release 1.0.4 startup review

The latest official Verse Pro, Era and InkPad 4 downloads were rechecked on
2026-11-05 and match the pinned firmware above. All three ship `libinkview.so`
and `libhwconfig.so` with executable-stack flags. Their extracted glibc versions
are 2.39 (Verse Pro and Era) and 2.41 (InkPad 4). The shared entry point applies
the process-local loader setting used by KOReader; it is delivered inside the
OTA payload for compatibility with the 1.0.3 outer launcher. Every build records
stack policy for all four dependency closures and probes its production entry
point before running the application.

An InkPad 4 failure was reported without firmware, behavior or log details.
The loader issue is a compatibility lead, not a confirmed diagnosis of that
report. Interactive hardware testing and complete device OTA acceptance remain
pending for this release.

The revised 1.0.4 separates runtime admission from this evidence matrix. Known
models require at least API 6.10 and select the highest known compatible level
(6.10/6.11); exact patch/build equality is no longer required for startup or OTA.
The original pinned firmware checks remain mandatory before packaging. A newer
firmware build may still change its libraries, hardware behavior or ABI and needs
independent validation before being added to the evidence matrix.
