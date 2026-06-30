# Origin and upstream notes

FTP2P is based in part on the mTCP project by Michael B. Brutman.

The original upstream source code is available from the official mTCP project.

## What this repository is

This repository contains:

- the FTP2P application sources
- a reference to the mTCP source tree required to build FTP2P, recorded as a Git submodule

## Relationship to mTCP

FTP2P reuses and adapts parts of the original mTCP FTP client code and is built in an environment that depends on the surrounding mTCP tree.

For that reason, the repository records the required mTCP source tree as a Git submodule instead of trying to present FTP2P as a fully standalone program.

## Distribution model

The main mTCP package is not repackaged here as a separate release product. The source tree required to build the released FTP2P binaries is provided through the `mTCP/` Git submodule, pinned to the commit recorded by this repository.

GitHub-generated "Source code" archives do not include the full contents of Git submodules. To get a complete buildable source tree, clone the repository with submodules initialized:

git clone --recurse-submodules https://github.com/radozd/ftp2p.git

For an existing clone, use:

git submodule update --init --recursive

## Upstream credit

mTCP was created by Michael B. Brutman.

Please refer to the official mTCP site for the original project, documentation, and upstream archives.

## Local modifications

FTP2P does not currently carry local changes to mTCP.

Exact correspondence with upstream mTCP is defined by the Git submodule commit recorded in this repository.