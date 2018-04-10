# See COPYING for license.

FROM debian:8

RUN apt-get update -y
RUN apt-get install -y \
    build-essential \
    libboost-all-dev \
    libdb++-dev \
    libjson-spirit-dev \
    libleveldb-dev \
    libssl-dev \
    qt5-default \
    qt5-qmake

COPY . .
