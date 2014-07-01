#!/bin/sh
cd "$(dirname $0)"/..

# Create a config.h
echo '#define HAVE_ZLIB_H 1' > config.h
echo '#define HAVE_UNISTD_H 1' >> config.h

for ext in c h; do
  # Rename swap.*.[ch], RISC OS has no idea how to translate those names
  perl -i -pe 's/swap\.([ch])\.inc/swap-inc.$1/' *.$ext
  mv swap.$ext.inc swap-inc.$ext

  # Move source files into directories, like RISC OS likes
  mkdir -p $ext
  for f in *.$ext; do mv $f $ext/$(basename $f .$ext); done
done

# Put the build file somewhere easy
cp riscos/build build

# Remove dots from dir name
base=$(basename "$PWD")
cd ..
mv "$base" "$(echo "$base" | tr . _)"
