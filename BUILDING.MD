# Building FTP2P

FTP2P is intended to be built inside the source tree included in this repository.

## Requirements

- MS-DOS compatible build environment
- Open Watcom 1.9
- the included `mtcp/` source tree
- the usual mTCP build prerequisites expected by that tree

## Source tree

Repository layout:

- `ftp2p/` - FTP2P application sources
- `mtcp/` - mTCP-based source tree required for build and runtime integration

FTP2P is not built as a completely standalone program. It uses code and surrounding infrastructure from the included mTCP tree.

## General build approach

Build FTP2P in the same environment and with the same compiler setup used for mTCP-style DOS applications.

A typical workflow is:

1. prepare Open Watcom 1.9 environment variables
2. make sure the mTCP tree is in place
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

That is why the required mTCP-based tree is kept together with the FTP2P sources.