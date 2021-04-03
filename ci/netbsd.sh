#!/bin/sh
set -ex

# Set PATH, PKG_PATH, etc
. /root/.profile

# Install packages
pkg_add autoconf automake libtool pkg-config fuse \
  lz4 lzo zstd \
  squashfs coreutils # For tests

# Enter our source dir, and print nice data on exit
cd /squashfuse
cleanup() {
  if [ "$?" != 0 ]; then
    cat config.log || true
    cat tests/*.log || true
    mksquashfs || true
  fi
}
trap "cleanup" EXIT

# Build
./autogen.sh
LDFLAGS='-L/usr/pkg/lib' CPPFLAGS='-Werror -I/usr/pkg/include' ./configure
make

# Test
# Ensure buffers are big enough that perfuse doesn't yield warnings
sysctl -w kern.sbmax=3000000
# Run on actual disk, /tmp may not be large
mkdir /root/tmp

TMPDIR=/root/tmp make check
diff -u ci/expected-features/all ci/features

# Test install
make install
