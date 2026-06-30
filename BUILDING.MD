# Building FTP2P

FTP2P is intended to be built with the `mTCP/` source tree initialized as a Git submodule.

## Requirements

- MS-DOS compatible build environment
- Open Watcom 1.9
- the mTCP/ source tree, initialized as a Git submodule
- the usual mTCP build prerequisites expected by that tree

## Source tree

Repository layout:

- `FTP2P/` - FTP2P application sources
- `mTCP/` - mTCP source tree, included as a Git submodule

Clone with submodules:
git clone --recurse-submodules https://github.com/radozd/ftp2p.git

For an existing clone:
git submodule update --init --recursive

## General build approach

Build FTP2P in the same environment and with the same compiler setup used for mTCP-style DOS applications.

A typical workflow is:

1. prepare Open Watcom 1.9 environment variables
2. make sure the mTCP submodule is initialized
3. build FTP2P against that tree
4. copy the resulting executable into your DOS runtime setup

The exact build commands may depend on your local batch files, include paths, library paths, and tree layout.

## Runtime notes

FTP2P is not just source-dependent on mTCP. Some runtime usage also expects tools from the same ecosystem.

In particular, your DOS setup may need the usual mTCP runtime utilities and configuration expected in a working mTCP installation.

## Releases

GitHub releases are intended to contain FTP2P binaries only.

The full corresponding source used to build those binaries remains available in the repository.

## Notes

This repository is meant to preserve a buildable and understandable setup rather than a heavily minimized one.

That is why the required mTCP source tree is recorded as a Git submodule next to the FTP2P sources.