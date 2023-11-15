{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [
    # Required build tools
    gcc
    autoconf
    automake
    libtool
    pkg-config

    # Required libraries
    fuse3
    zlib
    zstd
    lzo
    lz4
    xz
  ];
}
