#!/bin/sh
set -ev
brew update
brew cask install osxfuse
brew upgrade autoconf automake libtool pkgconfig
