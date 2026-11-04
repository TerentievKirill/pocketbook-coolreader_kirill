CoolReader for PocketBook Touch HD 3 (PB632), firmware U632.6.11.2001
Build date: 2026-10-22

Target and verification
-----------------------
This is a new build of upstream CoolReader 0.0.6-17-62.1 for the B288 / Cortex-A7
processor and modern firmware libraries. PocketBook's support API lists
6.11.2001, released September 1, 2026, as the latest PB632 firmware:
https://pocketbook.ch/front-api/api/en-ch/product?product_id=12
https://support.pocketbook-int.com/fw/632_HZO/ww/6.11.2001/rn/ww/RN_PB_Touch_HD_3_EN.pdf

Changes: use libpng16 instead of the removed libpng12; compile against the modern
InkView SDK; replace removed OpenContents with CoolReader's touch TOC; use
GetTouchInfoI and the current touch structure, and dispatch multi-touch frames
without treating frame indices as screen coordinates; adapt the logo path;
avoid a currentLang symbol collision; explicitly link libm.
Revision 2 isolates bundled FreeType/JPEG symbols so firmware InkView uses its
own matching libraries. Revision 3 fixes static initialization of the engine's
shared empty reference. On-device R3 logs confirmed screen, font and skin setup,
then identified a null pointer during CSS ancestor matching. Revision 4 adds
explicit null guards and preserves the legacy null-this checks elsewhere in the
engine. ARM tests reproduce the R3 selector crash and verify R4 can render both
the startup placeholder and a three-page Cyrillic FB2 document.

Revision 5 fixes the remaining language-string reference in skin rendering.
R4 incorrectly resolved it to firmware currentLang(), then crashed while drawing
the first page. A regression reproduces the old fault and verifies the fix.
R5 completed foreground EVT_SHOW under GDB on the physical reader with the
bundled welcome book and the previously failing Cyrillic Amnesia FB2.
Startup logging records fault PC/LR addresses; debug symbols stay on the build server. The old upstream OTA
updater is disabled because its firmware-5 package would undo this build.

Validation uses libraries extracted from the official PB632 6.11.2001 image.
The ARM firmware loader resolves dependencies, required exported symbols and
symbol versions are checked, and an LD_BIND_NOW ARM/QEMU startup reaches InkView
hardware initialization. The server cannot supply PocketBook hardware services.
Startup and first-page drawing have now been exercised on the physical reader.
The user confirmed that the book is visible and physical page-turn buttons work.
Touch/pinch, dictionary, suspend/resume, frontlight and saved reading position
still need interactive testing on the reader. Do not install this build on older firmware with an older glibc.

Install
-------
1. Connect the Touch HD 3 by USB and close any running CoolReader instance.
2. Back up system/bin/cr3-pb.app, system/share/cr3, and
   system/config/extensions.cfg if they already exist. Retain your reading
   settings and history; a full backup of system/share/cr3 is recommended.
3. Extract this ZIP. Copy/merge both its system and applications directories
   into the root of the PocketBook USB storage. Keep all additional existing
   files and directories. applications/CoolReader.app adds the Apps entry.
4. Register CoolReader in system/config/extensions.cfg:
   - If no custom file exists, copy system/share/cr3/doc/extensions.cfg.example there.
   - If one exists, add cr3-pb.app to the comma-separated application list for
     desired supported formats, following system/share/cr3/doc/extensions.cfg.example. Preserve
     existing readers and all other entries. For example:
     epub:@EPUB_file:1:eink-reader_with_blink.app,eink-reader_with_epub2.app,cr3-pb.app:ICON_EPUB
     fb2:@FB2_file:1:eink-reader.app,cr3-pb.app:ICON_FB2
   The example comes from firmware 6.11.2001, with CoolReader appended for its
   supported formats. Stock readers remain the default. PDF is left to the
   stock PDF readers. No device configuration is overwritten by this ZIP.
5. Safely eject the reader. In Library, long-press a supported book and choose
   Open with > CoolReader (the entry may appear as cr3-pb.app).
6. Test an EPUB and FB2, page buttons, menus/TOC, dictionary, rotation, swipes,
   pinch zoom, frontlight, sleep/wake, then close and reopen the book to check
   reading position. Touch HD 3's 1072x1448 screen selects the pb631fw5 skin.

Launch CoolReader from Apps using applications/CoolReader.app. It reopens the
last existing book opened through this launcher (including Library > Open with).
On first launch, or if that book was deleted, it opens the bundled Welcome.fb2
with instructions. You can also use CoolReader's Recent books menu. The remembered
path is stored in system/share/cr3/last-book.txt, which the ZIP does not overwrite.
The launcher preserves spaces in book paths, captures logs, and handles
CoolReader's restart marker. Logs are:
  system/cr3-pb.normal.log
  system/cr3-pb.crash.log
If it fails, keep those logs and the exact firmware version. Restore your backup
to return to the previous installation. Settings created by the older build
are retained when copying/merging the directories.

Reading defaults and saved preferences
-------------------------------------
New candidate builds include the Touch HD 3 preferences captured on 2026-10-22:
Droid Sans MTI Arabic, size 49, 100% line spacing; left/top/bottom margins 2 px,
right margin 0 px; Russian/English hyphenation. The font is supplied by firmware.
The complete default profile is system/share/cr3/cr3-defaults.ini. User or book
cr3.ini settings override these defaults. The package does not replace the user
configuration in system/config/cr3/cr3.ini, history, bookmarks or last-book path.
The source profile is data/touch-hd3/cr3-defaults.ini.

Candidate builds fix settings serialization to read/write memory streams,
save preferences on normal exit, preserve pinch font size, and prevent the
standby timer from accessing the closed document. Verify a preference survives
closing and reopening CoolReader before disconnecting the reader.

Rebuild on the Ubuntu server
----------------------------
Project: the extracted pocketbook-coolreader source directory
Run: bash tools/build-touch-hd3.sh
ARM regressions: python3 tools/test-touch-hd3.py (uses work/firmware runtime).
Artifacts: dist/coolreader-touch-hd3-fw6.11.2001-dev.zip and its .sha256 file.
New builds default to revision dev; set CR3_REVISION for an explicit candidate.
The build now gates packaging on eight ARM regressions; system/share/cr3/metadata/build-info.json binds
the report to this binary and records physical-device validation as not performed.
Intermediate files, SDK, host tools, build and verification logs are in work/.
The script uses local host tools in work/host if present. A standard Linux
build host needs cmake, make, zip, gettext and python3.

SDK: SDK-B288-6.10, GCC 13.3.0, Buildroot 2024.05.1 / glibc 2.39.
Download: https://github.com/Sean-on-Git/PocketBook-SDK/releases/download/6.10/SDK-B288-6.10.7z
The 6.11 SDK is for a different RK3566 processor; use the B288 SDK for PB632.
Set POCKETBOOK_SDK to an extracted SDK-B288-6.10 directory if needed.
New 7-Zip versions skip some SDK symlinks containing '..'. If that happens,
repair only links resolving inside the SDK, using tools/repair-pocketbook-sdk.py
as described in that script. The existing server SDK is already repaired.
No sudo or system package installation was used for this build.

Build metadata and binary SHA-256 are in system/share/cr3/metadata/build-info.json. The matching source,
build/package scripts and regression tests are available in this repository
and the corresponding release source archive. Source license: GPL version 2;
see system/share/cr3/licenses/COPYING.
