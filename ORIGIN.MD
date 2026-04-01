# Origin and upstream notes

FTP2P is based in part on the mTCP project by Michael B. Brutman.

The original upstream source code was obtained from the official mTCP archive distribution published at:

- <https://www.brutman.com/mTCP/>

## What this repository is

This repository contains:

- the FTP2P application sources
- the mTCP-based source tree required to build FTP2P

This is not an official mTCP repository and not a full upstream mirror maintained on behalf of the original author.

## Relationship to mTCP

FTP2P reuses and adapts parts of the original mTCP FTP client code and is built in an environment that depends on the surrounding mTCP tree.

For that reason, the repository keeps the required mTCP-based sources together with the FTP2P sources instead of trying to present FTP2P as a fully standalone program.

## Distribution model

The intention is to distribute:

- source code for the full buildable FTP2P repository
- release binaries for FTP2P only

The main mTCP package is not repackaged here as a separate release product. However, the source tree required to build the released FTP2P binaries is included in this repository.

## Upstream credit

mTCP was created by Michael B. Brutman.

Please refer to the official mTCP site for the original project, documentation, and upstream archives.

## Local modifications

This repository may contain:

- FTP2P-specific source files
- local changes required for integration and build
- fixes and adjustments related to DOS behavior and Open Watcom 1.9 compatibility

Exact correspondence with any single upstream mTCP archive snapshot is not guaranteed unless explicitly stated in a tag or release note.