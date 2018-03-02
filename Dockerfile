FROM debian:7

RUN apt-get update -y
RUN apt-get install -y build-essential \
  libboost-all-dev \
  libdb++-dev \
  libjson-spirit-dev \
  libminiupnpc-dev \
  qt4-dev-tools

COPY . .
