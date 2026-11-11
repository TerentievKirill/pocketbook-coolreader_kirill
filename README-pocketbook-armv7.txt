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

Reader menu
-----------
Hold the centre of the page to open the Reader menu. Settings is the first
item; it opens the grouped settings screen. Table of contents, Bookmarks,
Dictionary and Search are also available. More tools keeps recent books,
notes, page selection, help and configured extra actions. Read returns to the
book. Existing tap and button assignments are preserved.

Dictionaries
------------
Open Settings > Dictionaries and choose the language you read. Pick definitions
or a translation dictionary to download free over Wi-Fi and use offline.
The main list uses original StarDict files. PocketBook versions are available
inside each language for use with other reader apps too.
The list includes English, Russian, Ukrainian and Czech definitions, and both
directions of English/Russian, English/Czech and Russian/Czech translation.
Each item shows its download size and whether it is installed. Downloads can
be cancelled, are verified before installation and keep existing files intact.
Once installed, open Dictionary and tap its name at the top to select a pack;
no restart is needed. The dictionary picker also has Get dictionaries.
The lookup window has labeled Search, Entries, Pick word, Google, Wikipedia
and Close controls. Tap an entire entry row to open its definition; use the
Previous and Next buttons below the list to browse more entries.
Packs are stored in system/dictionaries. To install your own StarDict dictionary,
copy its whole folder there: .ifo, .idx (or .idx.gz), .dict (or .dict.dz), and
optional .syn files. Dictionaries in applications/koreader/data/dict are also
recognized, so CoolReader and KOReader can share the same files. Text and HTML
definitions are supported; audio and external dictionary media are not installed.
Factory-protected dictionaries can
remain unavailable to third-party apps; downloading a pack avoids that limit.
Data is CC BY-SA 4.0, from WikDict/FreeDict/DBnary/Wiktionary and reader.dict.
Sources and attribution: https://readertools.eu/downloads/dictionaries/20261110/credits.txt

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
Version 1.0.7 adds dictionary downloads, original StarDict support, a labeled
lookup toolbar, readable suggestions and a Reader menu for long presses.
It fixes changing-word crashes, raw HTML in entry lists, the missing dropdown
symbol and Google/Wikipedia browser handoff. Downloads work offline after
installation; web lookups require Wi-Fi. The native browser opens the selected
entry and reports connection or launch errors. Older settings remain compatible.

Version 1.0.6 introduced six settings groups, larger text and touch controls, a font
sample and quarter-step weights from 0 (Normal) to 1.75 (Extra heavy). Font size,
line spacing and margins have direct +/- controls. Done applies pending changes;
Back returns to the parent menu, and Cancel on the main screen discards changes.
Font/page caches and idle work improve responsiveness. Reading position is saved
after idle periods and during sleep/task switching; failed saves retry and valid
backups recover unreadable settings/history. Settings-only font changes retain
the requested values. Sudden power loss can still lose the most recent page turn.
It retains the page-copy and cached status improvements from 1.0.5 and the firmware
API rules for startup and OTA. Existing shared installations can update over Wi-Fi.
Twenty-four ARM regression groups and four firmware-runtime checks pass.
Development builds were copied to a PB632. Final release hardware and end-to-end
OTA acceptance remain pending; automated checks do not establish those results.

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
