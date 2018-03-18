// Copyright (c) 2018 RG Huckins
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __NETWORK_PEER_H__
#define __NETWORK_PEER_H__

#include "netbase.h"
#include "protocol.h"
#include "util.h"
#include "sync.h"

#include <map>
#include <vector>

#include <openssl/rand.h>


class NetworkPeer : public CAddress
{
    friend class NetworkPeerManager;

    // where knowledge about this address first came from
    CNetAddr source;

    // last successful connection by us
    int64 nLastSuccess;

    // last try whatsoever by us:
    // int64 CAddress::nLastTry

    // connection attempts since last successful attempt
    int nAttempts;

    // reference count in new sets (memory only)
    int nRefCount;

    // in tried set? (memory only)
    bool fInTried;

    // position in vRandom
    int nRandomPos;

public:

    NetworkPeer();
    NetworkPeer(const CAddress &addrIn, const CNetAddr &addrSource);

    IMPLEMENT_SERIALIZE(
        CAddress* pthis = (CAddress*)(this);
        READWRITE(*pthis);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    )
};

#endif // __NETWORK_PEER_H__
