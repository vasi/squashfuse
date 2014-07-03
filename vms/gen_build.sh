#!/bin/sh

cat <<EOCAT
$ create config.h
$ deck
#define HAVE_ZLIB_H 1
$ EOD
$ ! Use lowercase function names, to match zlib
$ cflags = "/names=(as_is)/include_directory=sys\$include"
EOCAT

# Compile sources
for source in "$@"
do
  case "$source" in
    *.c)
      echo "\$ write sys\$output \"$source\""
      echo "\$ cc 'cflags' $source"
      ;;
  esac
done

# Link them
echo '$ link /executable=squashfuse_ls -'
for source in "$@"
do
  case "$source" in
    *.c)
      base=$(basename "$source" .c)
      echo "  $base, -"
      ;;
  esac
done
echo '  sys$include:libz64/lib'
