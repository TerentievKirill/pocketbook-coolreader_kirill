CoolReader for PocketBook - shared ARMv7 Linux package
=================================================================

One application executable is used on all four models below. Runtime device
profiles select display defaults. This package is a laboratory-tested candidate;
its current binary has NOT been tested on a physical reader.

Models and firmware snapshots used for verification:
  Touch HD 3 / PB632   U632.6.11.2001
  Verse Pro / PB634    U634.6.10.3425
  Era / PB700         U700.6.10.3209
  InkPad 4 / PB743G   U743g.6.11.1683

These firmware snapshots are evidence, not an exact-version whitelist.
Known models select the highest supported API level at or below the installed
firmware version (6.10 or 6.11). Firmware API levels below 6.10, malformed
version strings and unknown models are rejected. Different patch/build numbers
are allowed; compatible system libraries remain necessary. This package requires
modern ARM Linux soft-float userland; it is not an Android, ARM64 or hard-float
build. Older PocketBook firmware may not supply its required system libraries.
The compiled CPU baseline is ARMv7-A, NEON/VFPv3, softfp calling convention.

Install and return to a previous installation
--------------------------------------------
1. Close CoolReader and connect the reader by USB.
2. Back up system/bin/cr3-pb.app, applications/CoolReader.app,
   system/share/cr3 and system/config/cr3 before copying anything.
3. Merge the package's system and applications directories into the USB root.
   Preserve existing settings, history, bookmarks, custom fonts and other files.
   These are the ZIP's only top-level directories. Documentation, licenses and
   build reports stay under system/share/cr3 so the Library does not list them
   as books. The installation guide is in system/share/cr3/doc/.
4. Safely eject. Start Applications > CoolReader, or use Library > Open with
   if CoolReader was already registered. The first launch opens this package's
   welcome book; subsequent launches reopen the last existing book.
5. To return to the previous build, close CoolReader and restore your backups.

Library file associations are optional and differ across firmware. Preserve the
existing system/config/extensions.cfg. For a supported text format such as EPUB
or FB2, append ,cr3-pb.app to its comma-separated application list, keeping the
existing stock readers. Do not replace the configuration with the comment-only
system/share/cr3/doc/extensions.cfg.example. PDFs remain with the existing PDF readers.

Defaults and runtime device selection
------------------------------------
General defaults use DejaVu Sans, 110% line spacing and algorithmic hyphenation.
Device DPI supplies font size and margins: on these 300-DPI models the starting
size is 42 pixels and margins are 24 pixels. A saved user or per-book setting
takes precedence. The package does not contain the captured Russian/English
Touch HD 3 reading profile. Screen dimensions continue to come from InkView.
InkView supplies device key mappings and rotation events; the app does not apply
an additional rotation to key codes. system/share/cr3/metadata/device-profiles.json
records the registry.

Updates and testing
-------------------
Version 1.0.6 adds six settings groups, larger text and touch controls, a font
sample and quarter-step weights from 0 (Normal) to 1.75 (Extra heavy). Font size,
line spacing and margins have direct +/- controls. Done applies pending changes;
Back returns to the parent menu, and Cancel on the main screen discards changes.
Font/page caches and idle work improve responsiveness. Reading position is saved
after idle periods and during sleep/task switching; failed saves retry and valid
backups recover unreadable settings/history. Settings-only font changes retain
the requested values. Sudden power loss can still lose the most recent page turn.
It retains the page-copy and cached status improvements from 1.0.5 and the firmware
API rules for startup and OTA. Existing shared installations can update over Wi-Fi.
Twenty ARM regression groups and four firmware-runtime checks pass. This build was
copied to a PB632; interactive device and end-to-end OTA testing remain pending.

Settings > About & updates > Check for updates checks one shared release
channel for every supported model. Settings > About & updates > Version shows
the installed release version.
Automatic checks remain optional, quiet while offline and inactive during sleep.
Downloads require confirmation and show progress/cancellation. The package's
checksum, family and device/firmware compatibility are verified before staging.
Restart now installs it; Later installs it on the next normal launch. Settings,
reading history, custom assets and last-book paths are preserved, and a failed
first run rolls back. Staged PB632-only updates are never installed here.

Install this OTA-enabled launcher once over USB if you used the earlier manual
shared candidate. Later application/asset updates can arrive through the shared
channel. The release package and channel descriptor must be published together;
a local development build does not publish them automatically.

Version 1.0.4 includes a process-local firmware-library startup compatibility
setting, following KOReader's PocketBook fix. The entry point is inside the
shared update payload, so 1.0.3 shared launchers can use it after an OTA update.
If CoolReader cannot start to download the update, install 1.0.4 over USB.
The package contains one ELF executable (bin/cr3-pb-executable.app), launched by
a small shell entry point (bin/cr3-pb.app). Both stay under system/share/cr3.
Physical verification of the reported InkPad 4 failure remains pending.

Before treating a model as supported, test EPUB and FB2, page buttons, menus,
dictionary, swipes, pinch zoom, all rotations, brightness/warmth, sleep/wake,
overnight battery use and reading-position/settings persistence. Report the
model, exact firmware and logs:
  system/cr3-pb.normal.log
  system/cr3-pb.crash.log

system/share/cr3/metadata/build-info.json binds this package to its source, exact executable, primary ARM
regressions and the three additional firmware qualification reports. Automated
checks exercise software interfaces and rendering, not actual hardware behavior.

Source and license: GPL version 2; see system/share/cr3/licenses/COPYING and
the bundled dependency notices in system/share/cr3/licenses/.
