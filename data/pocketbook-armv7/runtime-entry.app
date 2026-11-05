#!/bin/sh
# PocketBook 6.11/glibc 2.41 can reject firmware libraries requesting an
# executable stack. Use the same process-local workaround as KOReader:
# https://github.com/koreader/koreader/pull/15730
# Keep this entry point inside the OTA payload so older outer launchers use it.
binary="$(dirname "$0")/cr3-pb-executable.app"
# The 1.0.3 updater sets execute permission on the entry point only.
if [ ! -x "$binary" ]; then
    chmod 0755 "$binary" || exit 126
fi
export GLIBC_TUNABLES="${GLIBC_TUNABLES}${GLIBC_TUNABLES:+:}glibc.rtld.execstack=2"
printf '%s\n' '[cr3-launch] firmware library compatibility enabled' >&2
exec "$binary" "$@"
