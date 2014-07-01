#!/bin/sh
: ${ZLIB:="HostFS:$.Apps.Library.!Zlib1g"}
CFLAGS="-Wall -I$ZLIB"
LDFLAGS="$ZLIB.libz/a"
OBJS=""

for source in "$@"
do
  case "$source" in
    *.c)
      base=$(basename "$source" .c)
      echo "echo $base"
      echo "gcc $CFLAGS -c $source"
      OBJS="$OBJS $base.o"
      ;;
  esac
done
echo "echo LINK"
echo "gcc -o squashfuse_ls $OBJS $LDFLAGS"
