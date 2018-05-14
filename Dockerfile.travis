# Copyright (c) 2018 RG Huckins
# Copyright (c) 2018 Chapman Shoop
# See COPYING for license.

FROM debian:7

RUN apt-get update -y
RUN apt-get install -y \
    build-essential \
    libboost-all-dev \
    libdb++-dev \
    libjson-spirit-dev \
    qt4-dev-tools

COPY . .
