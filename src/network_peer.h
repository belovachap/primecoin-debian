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


// total number of buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_COUNT 64

// maximum allowed number of entries in buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_SIZE 64

// total number of buckets for new addresses
#define ADDRMAN_NEW_BUCKET_COUNT 256

// maximum allowed number of entries in buckets for new addresses
#define ADDRMAN_NEW_BUCKET_SIZE 64

// over how many buckets entries with tried addresses from a single group (/16 for IPv4) are spread
#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 4

// over how many buckets entries with new addresses originating from a single group are spread
#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 32

// in how many buckets for entries with new addresses a single address may occur
#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 4

// how many entries in a bucket with tried addresses are inspected, when selecting one to replace
#define ADDRMAN_TRIED_ENTRIES_INSPECT_ON_EVICT 4

// how old addresses can maximally be
#define ADDRMAN_HORIZON_DAYS 30

// after how many failed attempts we give up on a new node
#define ADDRMAN_RETRIES 3

// how many successive failures are allowed ...
#define ADDRMAN_MAX_FAILURES 10

// ... in at least this many days
#define ADDRMAN_MIN_FAIL_DAYS 7

// the maximum percentage of nodes to return in a getaddr call
#define ADDRMAN_GETADDR_MAX_PCT 23

// the maximum number of nodes to return in a getaddr call
#define ADDRMAN_GETADDR_MAX 2500


/** Extended statistics about a CAddress */
class NetworkPeer : public CAddress
{
private:
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

    friend class NetworkPeerManager;

public:

    IMPLEMENT_SERIALIZE(
        CAddress* pthis = (CAddress*)(this);
        READWRITE(*pthis);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    )

    void Init()
    {
        nLastSuccess = 0;
        nLastTry = 0;
        nAttempts = 0;
        nRefCount = 0;
        fInTried = false;
        nRandomPos = -1;
    }

    NetworkPeer(const CAddress &addrIn, const CNetAddr &addrSource) : CAddress(addrIn), source(addrSource)
    {
        Init();
    }

    NetworkPeer() : CAddress(), source()
    {
        Init();
    }

    // Calculate in which "tried" bucket this entry belongs
    int GetTriedBucket(const std::vector<unsigned char> &nKey) const;

    // Calculate in which "new" bucket this entry belongs, given a certain source
    int GetNewBucket(const std::vector<unsigned char> &nKey, const CNetAddr& src) const;

    // Calculate in which "new" bucket this entry belongs, using its default source
    int GetNewBucket(const std::vector<unsigned char> &nKey) const
    {
        return GetNewBucket(nKey, source);
    }

    // Determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool IsTerrible(int64 nNow = GetAdjustedTime()) const;

    // Calculate the relative chance this entry should be given when selecting nodes to connect to
    double GetChance(int64 nNow = GetAdjustedTime()) const;

};

#endif // __NETWORK_PEER_H__
