// Copyright (c) 2018 RG Huckins
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __NETWORK_PEER_MANAGER_H__
#define __NETWORK_PEER_MANAGER_H__

#include "netbase.h"
#include "network_peer.h"
#include "protocol.h"
#include "util.h"
#include "sync.h"


#include <map>
#include <vector>

#include <openssl/rand.h>


// Stochastic address manager
//
// Design goals:
//  * Only keep a limited number of addresses around, so that addr.dat and memory requirements do not grow without bound.
//  * Keep the address tables in-memory, and asynchronously dump the entire to able in addr.dat.
//  * Make sure no (localized) attacker can fill the entire table with his nodes/addresses.
//
// To that end:
//  * Addresses are organized into buckets.
//    * Address that have not yet been tried go into 256 "new" buckets.
//      * Based on the address range (/16 for IPv4) of source of the information, 32 buckets are selected at random
//      * The actual bucket is chosen from one of these, based on the range the address itself is located.
//      * One single address can occur in up to 4 different buckets, to increase selection chances for addresses that
//        are seen frequently. The chance for increasing this multiplicity decreases exponentially.
//      * When adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
//        ones) is removed from it first.
//    * Addresses of nodes that are known to be accessible go into 64 "tried" buckets.
//      * Each address range selects at random 4 of these buckets.
//      * The actual bucket is chosen from one of these, based on the full address.
//      * When adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
//        tried ones) is evicted from it, back to the "new" buckets.
//    * Bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
//      be observable by adversaries.
//    * Several indexes are kept for high performance.

namespace NPMConstants {
    // total number of buckets for tried addresses
    const int TRIED_BUCKET_COUNT = 64;

    // maximum allowed number of entries in buckets for tried addresses
    const unsigned int TRIED_BUCKET_SIZE = 64;

    // total number of buckets for new addresses
    const int NEW_BUCKET_COUNT = 256;

    // maximum allowed number of entries in buckets for new addresses
    const unsigned int NEW_BUCKET_SIZE = 64;

    // over how many buckets entries with tried addresses from a single group (/16 for IPv4) are spread
    const int TRIED_BUCKETS_PER_GROUP = 4;

    // over how many buckets entries with new addresses originating from a single group are spread
    const int NEW_BUCKETS_PER_SOURCE_GROUP = 32;

    // in how many buckets for entries with new addresses a single address may occur
    const int NEW_BUCKETS_PER_ADDRESS = 4;

    // how many entries in a bucket with tried addresses are inspected, when selecting one to replace
    const unsigned int TRIED_ENTRIES_INSPECT_ON_EVICT = 4;

    // how old addresses can maximally be
    const int HORIZON_DAYS = 30;

    // after how many failed attempts we give up on a new node
    const int RETRIES = 3;

    // how many successive failures are allowed ...
    const int MAX_FAILURES = 10;

    // ... in at least this many days
    const int MIN_FAIL_DAYS = 7;

    // the maximum percentage of nodes to return in a getaddr call
    const int GETADDR_MAX_PCT = 23;

    // the maximum number of nodes to return in a getaddr call
    const int GETADDR_MAX = 2500;
};

class NetworkPeerManager {
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // secret key to randomize bucket select with
    std::vector<unsigned char> nKey;

    // last used nId
    int nIdCount;

    // table with information about all nIds
    std::map<int, NetworkPeer> mapInfo;

    // find an nId based on its network address
    std::map<CNetAddr, int> mapAddr;

    // randomly-ordered vector of all nIds
    std::vector<int> vRandom;

    // number of "tried" entries
    int nTried;

    // list of "tried" buckets
    std::vector<std::vector<int> > vvTried;

    // number of (unique) "new" entries
    int nNew;

    // list of "new" buckets
    std::vector<std::set<int> > vvNew;

    // Find an entry.
    NetworkPeer* Find(const CNetAddr& addr, int *pnId = NULL);

    // find an entry, creating it if necessary.
    // nTime and nServices of found node is updated, if necessary.
    NetworkPeer* Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId = NULL);

    // Swap two elements in vRandom.
    void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2);

    // Return position in given bucket to replace.
    int SelectTried(int nKBucket);

    // Remove an element from a "new" bucket.
    // This is the only place where actual deletes occur.
    // They are never deleted while in the "tried" table, only possibly evicted back to the "new" table.
    int ShrinkNew(int nUBucket);

    // Move an entry from the "new" table(s) to the "tried" table
    // @pre vvUnkown[nOrigin].count(nId) != 0
    void MakeTried(NetworkPeer& network_peer, int nId, int nOrigin);

    // Mark an entry "good", possibly moving it from "new" to "tried".
    void Good_(const CService &addr, int64 nTime);

    // Add an entry to the "new" table.
    bool Add_(const CAddress &addr, const CNetAddr& source, int64 nTimePenalty);

    // Mark an entry as attempted to connect.
    void Attempt_(const CService &addr, int64 nTime);

    // Select an address to connect to.
    // nUnkBias determines how much to favor new addresses over tried ones (min=0, max=100)
    CAddress Select_(int nUnkBias);

    // Select several addresses at once.
    void GetAddr_(std::vector<CAddress> &vAddr);

    // Mark an entry as currently-connected-to.
    void Connected_(const CService &addr, int64 nTime);

public:
    NetworkPeerManager() : vRandom(0), vvTried(NPMConstants::TRIED_BUCKET_COUNT, std::vector<int>(0)), vvNew(NPMConstants::NEW_BUCKET_COUNT, std::set<int>())
    {
         nKey.resize(32);
         RAND_bytes(&nKey[0], 32);

         nIdCount = 0;
         nTried = 0;
         nNew = 0;
    }

    // Return the number of (unique) addresses in all tables.
    int size()
    {
        return vRandom.size();
    }

    // Add a single address.
    bool Add(const CAddress &addr, const CNetAddr& source, int64 nTimePenalty = 0)
    {
        bool fRet = false;
        {
            LOCK(cs);
            fRet |= Add_(addr, source, nTimePenalty);
        }
        if (fRet)
            printf("Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort().c_str(), source.ToString().c_str(), nTried, nNew);
        return fRet;
    }

    // Add multiple addresses.
    bool Add(const std::vector<CAddress> &vAddr, const CNetAddr& source, int64 nTimePenalty = 0)
    {
        int nAdd = 0;
        {
            LOCK(cs);
            for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
                nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
        }
        if (nAdd)
            printf("Added %i addresses from %s: %i tried, %i new\n", nAdd, source.ToString().c_str(), nTried, nNew);
        return nAdd > 0;
    }

    // Mark an entry as accessible.
    void Good(const CService &addr, int64 nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Good_(addr, nTime);
        }
    }

    // Mark an entry as connection attempted to.
    void Attempt(const CService &addr, int64 nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Attempt_(addr, nTime);
        }
    }

    // Choose an address to connect to.
    // nUnkBias determines how much "new" entries are favored over "tried" ones (0-100).
    CAddress Select(int nUnkBias = 50)
    {
        CAddress addrRet;
        {
            LOCK(cs);
            addrRet = Select_(nUnkBias);
        }
        return addrRet;
    }

    // Return a bunch of addresses, selected at random.
    std::vector<CAddress> GetAddr()
    {
        std::vector<CAddress> vAddr;
        {
            LOCK(cs);
            GetAddr_(vAddr);
        }
        return vAddr;
    }

    // Mark an entry as currently-connected-to.
    void Connected(const CService &addr, int64 nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Connected_(addr, nTime);
        }
    }

    // Calculate in which "tried" bucket this network_peer belongs
    int GetTriedBucket(const NetworkPeer &network_peer, const std::vector<unsigned char> &nKey) const;

    // Calculate in which "new" bucket this network_peer belongs, given a certain source
    int GetNewBucket(const NetworkPeer &network_peer, const std::vector<unsigned char> &nKey, const CNetAddr& src) const;

    // Calculate in which "new" bucket this network_peer belongs, using its default source
    int GetNewBucket(const NetworkPeer &network_peer, const std::vector<unsigned char> &nKey) const
    {
        return GetNewBucket(network_peer, nKey, network_peer.source);
    }

    // Determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool IsTerrible(const NetworkPeer &network_peer, int64 nNow = GetAdjustedTime()) const;

    // Calculate the relative chance this entry should be given when selecting nodes to connect to
    double GetChance(const NetworkPeer &network_peer, int64 nNow = GetAdjustedTime()) const;

    IMPLEMENT_SERIALIZE
    (({
        // serialized format:
        // * version byte (currently 0)
        // * nKey
        // * nNew
        // * nTried
        // * number of "new" buckets
        // * all nNew NetworkPeers in vvNew
        // * all nTried NetworkPeers in vvTried
        // * for each bucket:
        //   * number of elements
        //   * for each element: index
        //
        // Notice that vvTried, mapAddr and vVector are never encoded explicitly;
        // they are instead reconstructed from the other information.
        //
        // This format is more complex, but significantly smaller (at most 1.5 MiB), and supports
        // changes to the NPMConstants without breaking the on-disk structure.
        {
            LOCK(cs);
            unsigned char nVersion = 0;
            READWRITE(nVersion);
            READWRITE(nKey);
            READWRITE(nNew);
            READWRITE(nTried);

            NetworkPeerManager *manager = const_cast<NetworkPeerManager*>(this);
            if (fWrite)
            {
                int nUBuckets = NPMConstants::NEW_BUCKET_COUNT;
                READWRITE(nUBuckets);
                std::map<int, int> mapUnkIds;
                int nIds = 0;
                for (std::map<int, NetworkPeer>::iterator it = manager->mapInfo.begin(); it != manager->mapInfo.end(); it++)
                {
                    if (nIds == nNew) break; // this means nNew was wrong, oh ow
                    mapUnkIds[(*it).first] = nIds;
                    NetworkPeer &network_peer = (*it).second;
                    if (network_peer.nRefCount)
                    {
                        READWRITE(network_peer);
                        nIds++;
                    }
                }
                nIds = 0;
                for (std::map<int, NetworkPeer>::iterator it = manager->mapInfo.begin(); it != manager->mapInfo.end(); it++)
                {
                    if (nIds == nTried) break; // this means nTried was wrong, oh ow
                    NetworkPeer &network_peer = (*it).second;
                    if (network_peer.fInTried)
                    {
                        READWRITE(network_peer);
                        nIds++;
                    }
                }
                for (std::vector<std::set<int> >::iterator it = manager->vvNew.begin(); it != manager->vvNew.end(); it++)
                {
                    const std::set<int> &vNew = (*it);
                    int nSize = vNew.size();
                    READWRITE(nSize);
                    for (std::set<int>::iterator it2 = vNew.begin(); it2 != vNew.end(); it2++)
                    {
                        int nIndex = mapUnkIds[*it2];
                        READWRITE(nIndex);
                    }
                }
            } else {
                int nUBuckets = 0;
                READWRITE(nUBuckets);
                manager->nIdCount = 0;
                manager->mapInfo.clear();
                manager->mapAddr.clear();
                manager->vRandom.clear();
                manager->vvTried = std::vector<std::vector<int> >(NPMConstants::TRIED_BUCKET_COUNT, std::vector<int>(0));
                manager->vvNew = std::vector<std::set<int> >(NPMConstants::NEW_BUCKET_COUNT, std::set<int>());
                for (int n = 0; n < manager->nNew; n++)
                {
                    NetworkPeer &network_peer = manager->mapInfo[n];
                    READWRITE(network_peer);
                    manager->mapAddr[network_peer] = n;
                    network_peer.nRandomPos = vRandom.size();
                    manager->vRandom.push_back(n);
                    if (nUBuckets != NPMConstants::NEW_BUCKET_COUNT)
                    {
                        manager->vvNew[manager->GetNewBucket(network_peer, manager->nKey)].insert(n);
                        network_peer.nRefCount++;
                    }
                }
                manager->nIdCount = manager->nNew;
                int nLost = 0;
                for (int n = 0; n < manager->nTried; n++)
                {
                    NetworkPeer network_peer;
                    READWRITE(network_peer);
                    std::vector<int> &vTried = manager->vvTried[manager->GetTriedBucket(network_peer, manager->nKey)];
                    if (vTried.size() < NPMConstants::TRIED_BUCKET_SIZE)
                    {
                        network_peer.nRandomPos = vRandom.size();
                        network_peer.fInTried = true;
                        manager->vRandom.push_back(manager->nIdCount);
                        manager->mapInfo[manager->nIdCount] = network_peer;
                        manager->mapAddr[network_peer] = manager->nIdCount;
                        vTried.push_back(manager->nIdCount);
                        manager->nIdCount++;
                    } else {
                        nLost++;
                    }
                }
                manager->nTried -= nLost;
                for (int b = 0; b < nUBuckets; b++)
                {
                    std::set<int> &vNew = manager->vvNew[b];
                    int nSize = 0;
                    READWRITE(nSize);
                    for (int n = 0; n < nSize; n++)
                    {
                        int nIndex = 0;
                        READWRITE(nIndex);
                        NetworkPeer &network_peer = manager->mapInfo[nIndex];
                        if (nUBuckets == NPMConstants::NEW_BUCKET_COUNT && network_peer.nRefCount < NPMConstants::NEW_BUCKETS_PER_ADDRESS)
                        {
                            network_peer.nRefCount++;
                            vNew.insert(nIndex);
                        }
                    }
                }
            }
        }
    });)
};

#endif // __NETWORK_PEER_MANAGER_H__
