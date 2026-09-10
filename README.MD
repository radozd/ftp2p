# FTP2P

FTP2P is a dual-pane fullscreen FTP client for MS-DOS systems.

![FTP2P running under DOS](img/ftp2p.png)

It is developed as a separate utility on top of an mTCP-based source tree. The repository includes the source code required to build FTP2P, while release binaries are provided only for FTP2P itself.

## Background

Historically there was a common joke that every beginner programmer eventually writes a file manager. As a result there are countless two-panel file managers for DOS.

However, in more than 25 years no comparable two-panel FTP client for real MS-DOS has appeared. FTP2P attempts to fill that gap by
providing a simple full-screen dual-pane FTP client for DOS systems.

FTP2P is deliberately narrow in scope. It is an FTP client, and intends to remain one.

It will not grow into a general-purpose file manager, network suite, shell replacement, archive browser, editor, media viewer, or other multi-purpose cadaver assembled from unrelated features.

FTP2P is developed primarily for nostalgic and hobbyist purposes.

## Features

- dual-pane fullscreen DOS interface
- local DOS filesystem and remote FTP panels
- directory navigation on both sides
- file transfers in both directions
- switchable overwrite policy
- interrupted transfer resume
- rename, delete, and create-directory operations
- internal viewer
- passive FTP mode
- UTF-8 remote filename handling with DOS-compatible local name generation

## Requirements

- IBM PC compatible system running MS-DOS
- TCP/IP network interface supported by mTCP
- mTCP-compatible packet driver

## Limitations

FTP2P intentionally follows classic DOS filesystem constraints.
Long filename (LFN) support is not planned.

Recursive directory copy is not implemented because it would require reliable long filename handling when mapping remote paths to the local DOS filesystem.

Supported DOS code pages are currently limited to 437, 850, 852 and 866.

Each panel can display up to 3000 directory entries. The effective limit may be lower if filenames are very long.

## Repository layout

- `FTP2P/` - FTP2P application sources
- `mTCP/` - mTCP source tree, included as a Git submodule

## Status

The client is fully usable and largely feature-complete.

## Build

FTP2P targets MS-DOS and is built with Open Watcom 1.9.

See BUILDING.md for build instructions.

## Releases

GitHub releases contain FTP2P binaries only.

The corresponding source code for each release is available in this repository at the matching tag, including the FTP2P sources and the recorded `mTCP/` submodule commit used to build the released binary.

To get a complete buildable source tree, clone with submodules initialized.

## Origin

FTP2P is based in part on the original mTCP FTP client by Michael B. Brutman.

The mTCP/ directory is included as a Git submodule and points to the upstream mTCP repository.

## License

This project is distributed under the GNU General Public License v3.0.

Because FTP2P is based in part on mTCP code, the repository follows the same licensing model. See `LICENSE` for details.
