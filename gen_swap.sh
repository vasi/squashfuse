#!/bin/sh
rm -f foo
sed -n '
/^struct squashfs_/,/^}/{
	s/^struct \(squashfs_\([^[:space:]]*\)\).*/void sqfs_swapin_\2(struct \1 *s){/p;t decl
	s/};/}/p;t
	s/^[[:space:]]*__le\([[:digit:]]*\)[[:space:]]*\([a-z_]*\);$/sqfs_swapin\1(\&s->\2);/p
}
d
:decl
s/{/;/w swap.h.inc
' < "$1" > swap.c.inc
