// Copyright (c) 2018 RG Huckins
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include "network_peer.h"


NetworkPeer::NetworkPeer(const CAddress &address, const CNetAddr &source)
    : CAddress(address)
    , source(source)
    , nLastSuccess(0)
    , nAttempts(0)
    , nRefCount(0)
    , fInTried(false)
    , nRandomPos(-1) {
}

NetworkPeer::NetworkPeer()
    : NetworkPeer(CAddress(), CNetAddr()) {
}
