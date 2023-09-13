#!/bin/sh

. "tests/lib.sh"

# Test the notification pipe mechanism: it should receive 's' on success and 'f' on failure

trap cleanup EXIT
set -e
WORKDIR=$(mktemp -d)

cleanup() {
    set +e # Don't care about errors here.
    if [ -n "$WORKDIR" ]; then
        if [ -n "$SQ_SAVE_LOGS" ]; then
            cp "$WORKDIR/squashfs.log" "$SQ_SAVE_LOGS" || true
        fi
        if sq_is_mountpoint "$WORKDIR/mount"; then
            sq_umount "$WORKDIR/mount"
        fi
        rm -rf "$WORKDIR"
    fi
}
echo "Generating random test files..."
mkdir -p "$WORKDIR/source"
mkdir -p "$WORKDIR/mount"
head -c 17000 /dev/urandom >"$WORKDIR/source/rand1"

echo "Building squashfs image..."
mksquashfs "$WORKDIR/source" "$WORKDIR/squashfs.image" -no-progress

FIFO=$(mktemp -u)
mkfifo "$FIFO"

for prog in ./squashfuse ./squashfuse_ll; do
  echo "Mounting squashfs archive with $prog"
  # Check that 's' is sent to the notification pipe in case of successful mount
  $prog -f -o notify_pipe="$FIFO" "$WORKDIR/squashfs.image" "$WORKDIR/mount" >"$WORKDIR/squashfs.log" 2>&1 &
  STATUS=$(head -c1 "$FIFO")
  if [ "$STATUS" != "s" ]; then
    echo "Mounting squashfuse on /tmp/squash failed"
    exit 1
  fi
  sq_umount "$WORKDIR/mount"

  # Check that 'f' is sent to the notification pipe in case of failure
  $prog -f -o notify_pipe="$FIFO" /dev/null "$WORKDIR/mount" >"$WORKDIR/squashfs.log" 2>&1 &
  STATUS=$(head -c1 "$FIFO")
  if [ "$STATUS" != "f" ]; then
    echo "Mountpoint should have failed"
    exit 1
  fi
done
