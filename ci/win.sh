#!/bin/sh
set -ev

./gen_swap.sh squashfs_fs.h
cd win
export PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin:$PATH"
PLATFORM=$(/bin/find "/c/Program Files (x86)/Windows Kits/10/Include" -name sdkddkver.h | perl -ne 'print "$1\n" if m,/(10\.[^/]*)/,' | /bin/sort -n | tail -n 1)
echo "Found SDK $PLATFORM"
MSBuild.exe squashfuse_ls.vcxproj -p:PlatformToolset=v141 -p:TargetPlatformVersion=$PLATFORM

mkdir test
touch test/foo test/bar test/'iggy blah'
mksquashfs test test.squashfs
ls test | sort > expected
./Debug/squashfuse_ls.exe test.squashfs | dos2unix | sort > actual
diff -u expected actual
