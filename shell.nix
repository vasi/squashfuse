{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [
    # Required build tools
    gcc
    gnumake
    autoconf
    automake
    libtool
    pkg-config

    # Required libraries
    fuse
    fuse3
    zlib
    zstd
    lzo
    lz4
    xz

    # For tests
    squashfsTools
  ];
}
