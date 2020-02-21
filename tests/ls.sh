#!/bin/sh

. "tests/lib.sh"

# trap cleanup EXIT
set -e

WORKDIR=$(mktemp -d)

cleanup() {
    set +e # Don't care about errors here.
    if [ -n "$WORKDIR" ]; then
        rm -rf "$WORKDIR"
    fi
}

find_compressors

mkdir -p "$WORKDIR/source"
head -c 100 /dev/urandom >"$WORKDIR/source/rand1"
head -c 17000 /dev/urandom >"$WORKDIR/source/rand2"
head -c 100 /dev/urandom >"$WORKDIR/source/rand3"
head -c 87 /dev/zero >"$WORKDIR/source/z1 with spaces"

(cd "$WORKDIR/source" && find .) | sed -e 's,^\./,,' | grep -v '^\.$' | sort > "$WORKDIR/files"

for comp in $compressors; do
    echo "Building $comp squashfs image,.,"
    mksquashfs "$WORKDIR/source" "$WORKDIR/squashfs.image" -comp $comp -no-progress

    ./squashfuse_ls "$WORKDIR/squashfs.image" | sort > "$WORKDIR/ls"
    if ! diff -u "$WORKDIR/files" "$WORKDIR/ls" ; then
        echo "Found differing files!"
        exit 1
    fi

    rm -f "$WORKDIR/squashfs.image"
done

echo "Success."
exit 0
