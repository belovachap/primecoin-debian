# Primecoin for Debian 8 ("Jessie") amd64-gnome

Primecoin? https://en.wikipedia.org/wiki/Primecoin

Debian? https://en.wikipedia.org/wiki/Debian

8 ("Jessie")? https://wiki.debian.org/DebianJessie

amd64? https://en.wikipedia.org/wiki/X86-64

gnome? https://en.wikipedia.org/wiki/GNOME

## :open_file_folder: Git

- Branch: release-v2
- Tag: v2.0.0

## :construction_worker: Travis CI

[![Build Status](https://travis-ci.org/belovachap/primecoin-debian.svg?branch=release-v2)](https://travis-ci.org/belovachap/primecoin-debian)

## :package: Dependencies

| Package                                                                     |
| --------------------------------------------------------------------------- |
| [build-essential](https://packages.debian.org/jessie/build-essential)       |
| [libboost-all-dev](https://packages.debian.org/jessie/libboost-all-dev)     |
| [libdb++-dev](https://packages.debian.org/jessie/libdb++-dev)               |
| [libjson-spirit-dev](https://packages.debian.org/jessie/libjson-spirit-dev) |
| [libleveldb-dev](https://packages.debian.org/jessie/libleveldb-dev) |
| [libssl-dev](https://packages.debian.org/jessie/libssl-dev)                 |
| [qt4-dev-tools](https://packages.debian.org/jessie/qt4-dev-tools)           |

## :iphone: Wallet

```
apt-get install <dependencies>
qmake-qt4
make
```

## :computer: Server

```
apt-get install <dependencies>
cd src
make
```

## :wrench: Development

See `Dockerfile.dev` and `.travis.yml` :)
