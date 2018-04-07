// See COPYING for license.

#ifndef __VERSION_H__
#define __VERSION_H__

#include <string>


std::string FormatVersion(int);
std::string FormatSubVersion();

const std::string CLIENT_NAME("Chrysippus");

const int CLIENT_VERSION_MAJOR = 0;
const int CLIENT_VERSION_MINOR = 8;
const int CLIENT_VERSION_REVISION = 6;
const int CLIENT_VERSION_BUILD = 0;

static const int CLIENT_VERSION =
    1000000 * CLIENT_VERSION_MAJOR
    +   10000 * CLIENT_VERSION_MINOR
    +     100 * CLIENT_VERSION_REVISION
    +       1 * CLIENT_VERSION_BUILD;

const int PRIMECOIN_VERSION_MAJOR = 2;
const int PRIMECOIN_VERSION_MINOR = 0;
const int PRIMECOIN_VERSION_PATCH = 0;

static const int PRIMECOIN_VERSION =
    1000000 * PRIMECOIN_VERSION_MAJOR
    +   10000 * PRIMECOIN_VERSION_MINOR
    +     100 * PRIMECOIN_VERSION_PATCH;

static const int PROTOCOL_VERSION = 70001;

// earlier versions not supported as of Feb 2012, and are disconnected
static const int MIN_PROTO_VERSION = 209;

// nTime field added to CAddress, starting with this version;
// if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 31402;

// only request blocks from nodes outside this range of versions
static const int NOBLKS_VERSION_START = 32000;
static const int NOBLKS_VERSION_END = 32400;

// BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 60000;

// "mempool" command, enhanced "getdata" behavior starts with this version:
static const int MEMPOOL_GD_VERSION = 60002;

#endif // __VERSION_H__
