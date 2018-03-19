# Primecoin for Debian 7 ("Wheezy") amd64-gnome 

Primecoin? https://en.wikipedia.org/wiki/Primecoin

Debian? https://en.wikipedia.org/wiki/Debian

7 ("Wheezy")? https://wiki.debian.org/DebianWheezy

amd64? https://en.wikipedia.org/wiki/X86-64

gnome? https://en.wikipedia.org/wiki/GNOME

## Git :open_file_folder:

| Branch | release-v1.0 |
| Tag    | v1.0.0       |

## Travis CI :construction_worker:

[![Build Status](https://travis-ci.org/belovachap/primecoin-debian.svg?branch=release-v1.0)](https://travis-ci.org/belovachap/primecoin-debian)

## Dependencies :link:

| build-essential    | [11.5](https://packages.debian.org/wheezy/build-essential) |
| libboost-all-dev   | [1.49.0.1](https://packages.debian.org/wheezy/libboost-all-dev) |
| libdb++-dev        | [5.1.6](https://packages.debian.org/wheezy/libdb++-dev) |
| libjson-spirit-dev | [4.04-1+b1](https://packages.debian.org/wheezy/libjson-spirit-dev) |
| qt4-dev-tools      | [4:4.8.2+dfsg-11](https://packages.debian.org/wheezy/qt4-dev-tools) |

## Wallet :iphone:

```
apt-get install <dependencies>
qmake-qt4
make
```

## Server :computer:

```
apt-get install <dependencies>
cd src
make
```

## Development :wrench:

See `Dockerfile.dev` and `.travis.yml` :)
