#!/bin/sh
set -ev
brew update
brew cask install osxfuse
brew install md5sha1sum
brew upgrade autoconf automake libtool pkgconfig
