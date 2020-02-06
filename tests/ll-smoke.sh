#!/bin/bash

# Very simple smoke test for squashfuse_ll. Make some random files.
# assemble a squashfs image, mount it, compare the files.

SFLL=${1:-./squashfuse_ll}         # The squashfuse_ll binary.

IDLE_TIMEOUT=5

trap cleanup EXIT
set -e

WORKDIR=$(mktemp -d)

function cleanup {
    set +e # Don't care about errors here.
    if [ -n "$WORKDIR" ]; then
        if hostname | grep -q devvm; then
            cp "$WORKDIR/squashfs_ll.log" /tmp/ll-smoke-last-log || true
        fi
        if grep -q "$WORKDIR/mount" /proc/mounts; then
            fusermount3 -u "$WORKDIR/mount"
        fi
        rm -rf "$WORKDIR"
    fi
}

echo "Generating random test files..."
mkdir -p "$WORKDIR/source"
head -c 64M /dev/urandom >"$WORKDIR/source/rand1"
head -c 17K /dev/urandom >"$WORKDIR/source/rand2"
head -c 100M /dev/urandom >"$WORKDIR/source/rand3"
head -c 87 /dev/zero >"$WORKDIR/source/z1 with spaces"

echo "Building squashfs image..."
mksquashfs "$WORKDIR/source" "$WORKDIR/squashfs.image" -comp zstd -no-progress

mkdir -p "$WORKDIR/mount"

echo "Mounting squashfs image..."
$SFLL -f  $SFLL_EXTRA_ARGS "$WORKDIR/squashfs.image" "$WORKDIR/mount" >"$WORKDIR/squashfs_ll.log" 2>&1 &
# Wait up to 5 seconds to be mounted. TSAN builds can take some time to mount.
for _ in $(seq 5); do
  if grep -q "$WORKDIR/mount" /proc/mounts; then
    break
  fi
  sleep 1
done

if ! grep -q "$WORKDIR/mount" /proc/mounts; then
    echo "Image did not mount after 5 seconds."
    cp "$WORKDIR/squashfs_ll.log" /tmp/squashfs_ll.smoke.log
    echo "There may be clues in /tmp/squashfs_ll.smoke.log"
    exit 1
fi

if command -v fio >/dev/null; then
    echo "FIO tests..."
    fio --filename="$WORKDIR/mount/rand1" --direct=1 --rw=randread --ioengine=libaio --bs=512 --iodepth=16 --numjobs=4 --name=j1 --minimal --output=/dev/null --runtime 30
    fio --filename="$WORKDIR/mount/rand2" --rw=randread --ioengine=libaio --bs=4k --iodepth=16 --numjobs=4 --name=j2 --minimal --output=/dev/null --runtime 30
    fio --filename="$WORKDIR/mount/rand3" --rw=randread --ioengine=psync --bs=128k --name=j3 --minimal --output=/dev/null --runtime 30
else
    echo "Consider installing fio for better test coverage."
fi

echo "Comparing files..."
cmp "$WORKDIR/source/rand1" "$WORKDIR/mount/rand1"
cmp "$WORKDIR/source/rand2" "$WORKDIR/mount/rand2"
cmp "$WORKDIR/source/rand3" "$WORKDIR/mount/rand3"
cmp "$WORKDIR/source/z1 with spaces" "$WORKDIR/mount/z1 with spaces"

echo "Parallel md5sum..."
md5sum "$WORKDIR"/mount/* >"$WORKDIR/md5sums"
split -l1 "$WORKDIR/md5sums" "$WORKDIR/sumpiece"
echo "$WORKDIR"/sumpiece* | xargs -P0 -n1 md5sum -c

echo "Lookup tests..."
# Look for non-existent files to exercise failed lookup path.
if [ -e "$WORKDIR/mount/bogus" ]; then
    echo "Bogus existence test"
    exit 1
fi
# Twice so we hit cache path.
if [ -e "$WORKDIR/mount/bogus" ]; then
    echo "Bogus existence test #2"
    exit 1
fi

SRCSZ=$(stat -c "%s" "$WORKDIR/source/rand1")
MNTSZ=$(stat -c "%s" "$WORKDIR/mount/rand1")
if [ "$SRCSZ" != "$MNTSZ" ]; then
    echo "Bogus size $MNTSZ != $SRCSZ"
    exit 1
fi

echo "Unmounting..."
fusermount3 -u "$WORKDIR/mount"

echo "Remounting with idle unmount option..."
$SFLL $SFLL_EXTRA_ARGS -otimeout=$IDLE_TIMEOUT "$WORKDIR/squashfs.image" "$WORKDIR/mount"
if ! grep -q "$WORKDIR/mount" /proc/mounts; then
    echo "Not mounted?"
    exit 1
fi
echo "Waiting up to $(( IDLE_TIMEOUT + 5 )) seconds for idle unmount..."
sleep $(( IDLE_TIMEOUT + 5 ))
if grep -q "$WORKDIR/mount" /proc/mounts; then
    echo "FS did not idle unmount in timely way."
    exit 1
fi

echo "Success."
exit 0
