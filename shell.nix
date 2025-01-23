{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  buildInputs = with pkgs.buildPackages; [
    fuse
    fuse3
    zlib
    zstd
    lzo
    lz4
    xz
  ];

  nativeBuildInputs = with pkgs; [
    gcc
    gnumake
    autoconf
    automake
    libtool
    pkg-config

    # For tests
    squashfsTools
  ];

  shellHook = ''
    # Get access to fusermount, if it's on the host
    export PATH="/run/wrappers/bin:$PATH"
  '';
}
