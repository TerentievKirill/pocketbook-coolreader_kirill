#!/bin/sh
# Shared-package launcher: resume books, install verified shared updates and roll back failures.
cd /mnt/ext1 || exit 1
share=/mnt/ext1/system/share/cr3
pending=/mnt/ext1/system/share/cr3-ota-shared-new
backup=/mnt/ext1/system/share/cr3-ota-shared-old
# Recover an interrupted directory switch before choosing the last book.
if [ ! -d "$share" ] && [ -d "$backup" ]; then
    mv "$backup" "$share" || exit 1
fi
last_book_file=/mnt/ext1/system/share/cr3/last-book.txt
if [ "$#" -eq 0 ] || [ -z "$1" ]; then
    last_book=
    if [ -f "$last_book_file" ]; then
        IFS= read -r last_book < "$last_book_file"
    fi
    if [ -n "$last_book" ] && [ -f "$last_book" ]; then
        set -- "$last_book"
    else
        set -- /mnt/ext1/system/share/cr3/Welcome.fb2
    fi
fi
if [ -f "$1" ]; then
    # A failed write must not truncate the last complete remembered path.
    last_book_temp=$(mktemp "${last_book_file}.tmp.XXXXXX")
    if [ -n "$last_book_temp" ]; then
        if printf '%s\n' "$1" > "$last_book_temp"; then
            mv -f "$last_book_temp" "$last_book_file"
        fi
        rm -f "$last_book_temp"
    fi
fi
while :; do
    if [ -f "$pending/.ready" ] && [ -x "$pending/bin/cr3-pb.app" ] && [ "$(cat "$pending/package-family.txt" 2>/dev/null)" = "pocketbook-armv7-softfp" ]; then
        merged=/mnt/ext1/system/share/cr3-ota-shared-merged
        rm -rf "$merged"
        # Merge into a copy so user-added fonts/assets and last-book.txt survive.
        if cp -R "$share" "$merged" && cp -R "$pending/." "$merged/"; then
            rm -rf "$backup"
            if mv "$share" "$backup"; then
                if mv "$merged" "$share"; then
                    touch "$share/.ota-first-run"
                    rm -f "$share/.ready"
                    rm -rf "$pending"
                else
                    mv "$backup" "$share" || exit 1
                fi
            fi
        fi
    fi
    /mnt/ext1/system/share/cr3/bin/cr3-pb.app "$@" > /mnt/ext1/system/cr3-pb.normal.log 2> /mnt/ext1/system/cr3-pb.crash.log
    status=$?
    if [ -f "$share/.ota-first-run" ]; then
        if [ "$status" -ne 0 ] && [ -d "$backup" ]; then
            rm -rf "$pending"
            mv "$share" "$pending" && mv "$backup" "$share" || exit 1
            rm -f "$pending/.ready" /mnt/ext1/.cr3restart
            continue
        fi
        rm -f "$share/.ota-first-run"
    fi
    [ -f /mnt/ext1/.cr3restart ] || exit "$status"
    rm -f /mnt/ext1/.cr3restart
done
