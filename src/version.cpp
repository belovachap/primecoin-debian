// Copyright (c) 2012 The Bitcoin developers
// Copyright (c) 2013 Primecoin developers
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include <cstdio>
#include <sstream>
#include <string>

#include "util.h"

#include "version.h"


std::string FormatVersion(int version) {
    if (version % 100 == 0) {
        return strprintf(
        	"%d.%d.%d",
        	(version / 1000000),
        	(version / 10000) % 100,
        	(version / 100) % 100
        );
    }
    return strprintf(
    	"%d.%d.%d.%d",
    	(version / 1000000),
    	(version / 10000) % 100,
    	(version / 100) % 100,
    	version % 100
    );
}

// Format the subversion field according to BIP 14 spec (https://en.bitcoin.it/wiki/BIP_0014)
std::string FormatSubVersion() {
    std::ostringstream ss;
    ss << "/";
    ss << CLIENT_NAME << ":" << FormatVersion(CLIENT_VERSION);
    ss << "/";
    ss << "Primecoin:" << FormatVersion(PRIMECOIN_VERSION);
    return ss.str();
}
