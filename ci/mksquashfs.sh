#!/bin/sh
set -ev

cd ..
curl -L -o squashfs-tools.tar.gz https://github.com/plougher/squashfs-tools/archive/4.4.tar.gz
tar xf squashfs-tools.tar.gz
cd squashfs-tools*
if [ "$TRAVIS_OS_NAME" = osx ]; then
  curl -L "https://github.com/plougher/squashfs-tools/pull/69.patch?full_index=1" | patch -p1
fi

cd squashfs-tools
if [ "$TRAVIS_OS_NAME" = osx ]; then
  make
else
  make XZ_SUPPORT=1 LZO_SUPPORT=1 LZ4_SUPPORT=1 ZSTD_SUPPORT=1
fi
sudo make install
