// Copyright (c) 2018 RG Huckins
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include "network_peer_manager.h"

#include "hash.h"


NetworkPeerManager::NetworkPeerManager()
    : nIdCount(0)
    , vRandom(0)
    , nTried(0)
    , vvTried(NPMConstants::TRIED_BUCKET_COUNT, std::vector<int>(0))
    , nNew(0)
    , vvNew(NPMConstants::NEW_BUCKET_COUNT, std::set<int>())
{
    nKey.resize(32);
    RAND_bytes(&nKey[0], 32);
}

int NetworkPeerManager::size()
{
    return vRandom.size();
}

bool NetworkPeerManager::Add(
    const CAddress &addr,
    const CNetAddr& source,
    int64 nTimePenalty
) {
    bool fRet = false;
    {
        LOCK(cs);
        fRet |= Add_(addr, source, nTimePenalty);
    }
    if (fRet)
        printf("Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort().c_str(), source.ToString().c_str(), nTried, nNew);
    return fRet;
}

bool NetworkPeerManager::Add(
    const std::vector<CAddress> &vAddr,
    const CNetAddr& source,
    int64 nTimePenalty
) {
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

bool NetworkPeerManager::Add_(
    const CAddress &addr,
    const CNetAddr& source,
    int64 nTimePenalty
) {
    if (!addr.IsRoutable())
        return false;

    bool fNew = false;
    int nId;
    NetworkPeer *network_peer = Find(addr, &nId);

    if (network_peer)
    {
        // periodically update nTime
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64 nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime && (!network_peer->nTime || network_peer->nTime < addr.nTime - nUpdateInterval - nTimePenalty))
            network_peer->nTime = std::max((int64)0, addr.nTime - nTimePenalty);

        // add services
        network_peer->nServices |= addr.nServices;

        // do not update if no new information is present
        if (!addr.nTime || (network_peer->nTime && addr.nTime <= network_peer->nTime))
            return false;

        // do not update if the entry was already in the "tried" table
        if (network_peer->fInTried)
            return false;

        // do not update if the max reference count is reached
        if (network_peer->nRefCount == NPMConstants::NEW_BUCKETS_PER_ADDRESS)
            return false;

        // stochastic test: previous nRefCount == N: 2^N times harder to increase it
        int nFactor = 1;
        for (int n=0; n<network_peer->nRefCount; n++)
            nFactor *= 2;
        if (nFactor > 1 && (GetRandInt(nFactor) != 0))
            return false;
    } else {
        network_peer = Create(addr, source, &nId);
        network_peer->nTime = std::max((int64)0, (int64)network_peer->nTime - nTimePenalty);
        nNew++;
        fNew = true;
    }

    int nUBucket = this->GetNewBucket(*network_peer, nKey, source);
    std::set<int> &vNew = vvNew[nUBucket];
    if (!vNew.count(nId))
    {
        network_peer->nRefCount++;
        if (vNew.size() == NPMConstants::NEW_BUCKET_SIZE)
            ShrinkNew(nUBucket);
        vvNew[nUBucket].insert(nId);
    }
    return fNew;
}

void NetworkPeerManager::Good(const CService &addr, int64 nTime)
{
    {
        LOCK(cs);
        Good_(addr, nTime);
    }
}

void NetworkPeerManager::Good_(const CService &addr, int64 nTime) {
    int nId;
    NetworkPeer *network_peer = Find(addr, &nId);

    // if not found, bail out
    if (!network_peer)
        return;

    // check whether we are talking about the exact same CService (including same port)
    if (*network_peer != addr)
        return;

    // update network_peer
    network_peer->nLastSuccess = nTime;
    network_peer->nLastTry = nTime;
    network_peer->nTime = nTime;
    network_peer->nAttempts = 0;

    // if it is already in the tried set, don't do anything else
    if (network_peer->fInTried)
        return;

    // find a bucket it is in now
    int nRnd = GetRandInt(vvNew.size());
    int nUBucket = -1;
    for (unsigned int n = 0; n < vvNew.size(); n++)
    {
        int nB = (n+nRnd) % vvNew.size();
        std::set<int> &vNew = vvNew[nB];
        if (vNew.count(nId))
        {
            nUBucket = nB;
            break;
        }
    }

    // if no bucket is found, something bad happened;
    // TODO: maybe re-add the node, but for now, just bail out
    if (nUBucket == -1) return;

    printf("Moving %s to tried\n", addr.ToString().c_str());

    // move nId to the tried tables
    MakeTried(*network_peer, nId, nUBucket);
}

void NetworkPeerManager::Attempt(const CService &addr, int64 nTime)
{
    {
        LOCK(cs);
        Attempt_(addr, nTime);
    }
}

void NetworkPeerManager::Attempt_(const CService &addr, int64 nTime)
{
    NetworkPeer *network_peer = Find(addr);

    // if not found, bail out
    if (!network_peer)
        return;

    // check whether we are talking about the exact same CService (including
    // same port)
    if (*network_peer != addr)
        return;

    // update network_peer
    network_peer->nLastTry = nTime;
    network_peer->nAttempts++;
}

CAddress NetworkPeerManager::Select(int nUnkBias)
{
    CAddress addrRet;
    {
        LOCK(cs);
        addrRet = Select_(nUnkBias);
    }
    return addrRet;
}

CAddress NetworkPeerManager::Select_(int nUnkBias)
{
    if (size() == 0)
        return CAddress();

    double nCorTried = sqrt(nTried) * (100.0 - nUnkBias);
    double nCorNew = sqrt(nNew) * nUnkBias;
    if ((nCorTried + nCorNew)*GetRandInt(1<<30)/(1<<30) < nCorTried)
    {
        // use a tried node
        double fChanceFactor = 1.0;
        while(1)
        {
            int nKBucket = GetRandInt(vvTried.size());
            std::vector<int> &vTried = vvTried[nKBucket];
            if (vTried.size() == 0) continue;
            int nPos = GetRandInt(vTried.size());
            assert(mapInfo.count(vTried[nPos]) == 1);
            NetworkPeer &network_peer = mapInfo[vTried[nPos]];
            if (GetRandInt(1<<30) < fChanceFactor*this->GetChance(network_peer)*(1<<30))
                return network_peer;
            fChanceFactor *= 1.2;
        }
    } else {
        // use a new node
        double fChanceFactor = 1.0;
        while(1)
        {
            int nUBucket = GetRandInt(vvNew.size());
            std::set<int> &vNew = vvNew[nUBucket];
            if (vNew.size() == 0) continue;
            int nPos = GetRandInt(vNew.size());
            std::set<int>::iterator it = vNew.begin();
            while (nPos--)
                it++;
            assert(mapInfo.count(*it) == 1);
            NetworkPeer &network_peer = mapInfo[*it];
            if (GetRandInt(1<<30) < fChanceFactor*this->GetChance(network_peer)*(1<<30))
                return network_peer;
            fChanceFactor *= 1.2;
        }
    }
}

std::vector<CAddress> NetworkPeerManager::GetAddr()
{
    std::vector<CAddress> vAddr;
    {
        LOCK(cs);
        GetAddr_(vAddr);
    }
    return vAddr;
}

void NetworkPeerManager::GetAddr_(std::vector<CAddress> &vAddr)
{
    int nNodes = NPMConstants::GETADDR_MAX_PCT*vRandom.size()/100;
    if (nNodes > NPMConstants::GETADDR_MAX)
        nNodes = NPMConstants::GETADDR_MAX;

    // perform a random shuffle over the first nNodes elements of vRandom
    // (selecting from all)
    for (int n = 0; n<nNodes; n++)
    {
        int nRndPos = GetRandInt(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);
        vAddr.push_back(mapInfo[vRandom[n]]);
    }
}

void NetworkPeerManager::Connected(const CService &addr, int64 nTime)
{
    {
        LOCK(cs);
        Connected_(addr, nTime);
    }
}

void NetworkPeerManager::Connected_(const CService &addr, int64 nTime)
{
    NetworkPeer *network_peer = Find(addr);

    // if not found, bail out
    if (!network_peer)
        return;

    // check whether we are talking about the exact same CService (including same port)
    if (*network_peer != addr)
        return;

    // update network_peer
    int64 nUpdateInterval = 20 * 60;
    if (nTime - network_peer->nTime > nUpdateInterval)
        network_peer->nTime = nTime;
}

int NetworkPeerManager::GetTriedBucket(
    const NetworkPeer &network_peer,
    const std::vector<unsigned char> &nKey
) const {
    CDataStream ss1(SER_GETHASH, 0);
    std::vector<unsigned char> vchKey = network_peer.GetKey();
    ss1 << nKey << vchKey;
    uint64 hash1 = Hash(ss1.begin(), ss1.end()).Get64();

    CDataStream ss2(SER_GETHASH, 0);
    std::vector<unsigned char> vchGroupKey = network_peer.GetGroup();
    ss2 << nKey << vchGroupKey << (hash1 % NPMConstants::TRIED_BUCKETS_PER_GROUP);
    uint64 hash2 = Hash(ss2.begin(), ss2.end()).Get64();
    return hash2 % NPMConstants::TRIED_BUCKET_COUNT;
}

int NetworkPeerManager::GetNewBucket(
    const NetworkPeer &network_peer,
    const std::vector<unsigned char> &nKey,
    const CNetAddr& src
) const {
    CDataStream ss1(SER_GETHASH, 0);
    std::vector<unsigned char> vchGroupKey = network_peer.GetGroup();
    std::vector<unsigned char> vchSourceGroupKey = src.GetGroup();
    ss1 << nKey << vchGroupKey << vchSourceGroupKey;
    uint64 hash1 = Hash(ss1.begin(), ss1.end()).Get64();

    CDataStream ss2(SER_GETHASH, 0);
    ss2 << nKey << vchSourceGroupKey << (hash1 % NPMConstants::NEW_BUCKETS_PER_SOURCE_GROUP);
    uint64 hash2 = Hash(ss2.begin(), ss2.end()).Get64();
    return hash2 % NPMConstants::NEW_BUCKET_COUNT;
}

int NetworkPeerManager::GetNewBucket(
    const NetworkPeer &network_peer,
    const std::vector<unsigned char> &nKey
) const {
    return this->GetNewBucket(network_peer, nKey, network_peer.source);
}

bool NetworkPeerManager::IsTerrible(
    const NetworkPeer &network_peer,
    int64 nNow
) const {
    if (network_peer.nLastTry && network_peer.nLastTry >= nNow-60) // never remove things tried the last minute
        return false;

    if (network_peer.nTime > nNow + 10*60) // came in a flying DeLorean
        return true;

    if (network_peer.nTime==0 || nNow-network_peer.nTime > NPMConstants::HORIZON_DAYS*86400) // not seen in over a month
        return true;

    if (network_peer.nLastSuccess==0 && network_peer.nAttempts>=NPMConstants::RETRIES) // tried three times and never a success
        return true;

    if (nNow-network_peer.nLastSuccess > NPMConstants::MIN_FAIL_DAYS*86400 && network_peer.nAttempts>=NPMConstants::MAX_FAILURES) // 10 successive failures in the last week
        return true;

    return false;
}

double NetworkPeerManager::GetChance(const NetworkPeer &network_peer, int64 nNow) const
{
    double fChance = 1.0;

    int64 nSinceLastSeen = nNow - network_peer.nTime;
    int64 nSinceLastTry = nNow - network_peer.nLastTry;

    if (nSinceLastSeen < 0) nSinceLastSeen = 0;
    if (nSinceLastTry < 0) nSinceLastTry = 0;

    fChance *= 600.0 / (600.0 + nSinceLastSeen);

    // deprioritize very recent attempts away
    if (nSinceLastTry < 60*10)
        fChance *= 0.01;

    // deprioritize 50% after each failed attempt
    for (int n=0; n<network_peer.nAttempts; n++)
        fChance /= 1.5;

    return fChance;
}

NetworkPeer* NetworkPeerManager::Find(const CNetAddr& addr, int *pnId)
{
    std::map<CNetAddr, int>::iterator it = mapAddr.find(addr);
    if (it == mapAddr.end())
        return NULL;
    if (pnId)
        *pnId = (*it).second;
    std::map<int, NetworkPeer>::iterator it2 = mapInfo.find((*it).second);
    if (it2 != mapInfo.end())
        return &(*it2).second;
    return NULL;
}

NetworkPeer* NetworkPeerManager::Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId)
{
    int nId = nIdCount++;
    mapInfo[nId] = NetworkPeer(addr, addrSource);
    mapAddr[addr] = nId;
    mapInfo[nId].nRandomPos = vRandom.size();
    vRandom.push_back(nId);
    if (pnId)
        *pnId = nId;
    return &mapInfo[nId];
}

void NetworkPeerManager::SwapRandom(unsigned int nRndPos1, unsigned int nRndPos2)
{
    if (nRndPos1 == nRndPos2)
        return;

    assert(nRndPos1 < vRandom.size() && nRndPos2 < vRandom.size());

    int nId1 = vRandom[nRndPos1];
    int nId2 = vRandom[nRndPos2];

    assert(mapInfo.count(nId1) == 1);
    assert(mapInfo.count(nId2) == 1);

    mapInfo[nId1].nRandomPos = nRndPos2;
    mapInfo[nId2].nRandomPos = nRndPos1;

    vRandom[nRndPos1] = nId2;
    vRandom[nRndPos2] = nId1;
}

int NetworkPeerManager::SelectTried(int nKBucket)
{
    std::vector<int> &vTried = vvTried[nKBucket];

    // random shuffle the first few elements (using the entire list)
    // find the least recently tried among them
    int64 nOldest = -1;
    int nOldestPos = -1;
    for (unsigned int i = 0; i < NPMConstants::TRIED_ENTRIES_INSPECT_ON_EVICT && i < vTried.size(); i++)
    {
        int nPos = GetRandInt(vTried.size() - i) + i;
        int nTemp = vTried[nPos];
        vTried[nPos] = vTried[i];
        vTried[i] = nTemp;
        assert(nOldest == -1 || mapInfo.count(nTemp) == 1);
        if (nOldest == -1 || mapInfo[nTemp].nLastSuccess < mapInfo[nOldest].nLastSuccess) {
           nOldest = nTemp;
           nOldestPos = nPos;
        }
    }

    return nOldestPos;
}

int NetworkPeerManager::ShrinkNew(int nUBucket)
{
    assert(nUBucket >= 0 && (unsigned int)nUBucket < vvNew.size());
    std::set<int> &vNew = vvNew[nUBucket];

    // first look for deletable items
    for (std::set<int>::iterator it = vNew.begin(); it != vNew.end(); it++)
    {
        assert(mapInfo.count(*it));
        NetworkPeer &network_peer = mapInfo[*it];
        if (this->IsTerrible(network_peer))
        {
            if (--network_peer.nRefCount == 0)
            {
                SwapRandom(network_peer.nRandomPos, vRandom.size()-1);
                vRandom.pop_back();
                mapAddr.erase(network_peer);
                mapInfo.erase(*it);
                nNew--;
            }
            vNew.erase(it);
            return 0;
        }
    }

    // otherwise, select four randomly, and pick the oldest of those to replace
    int n[4] = {GetRandInt(vNew.size()), GetRandInt(vNew.size()), GetRandInt(vNew.size()), GetRandInt(vNew.size())};
    int nI = 0;
    int nOldest = -1;
    for (std::set<int>::iterator it = vNew.begin(); it != vNew.end(); it++)
    {
        if (nI == n[0] || nI == n[1] || nI == n[2] || nI == n[3])
        {
            assert(nOldest == -1 || mapInfo.count(*it) == 1);
            if (nOldest == -1 || mapInfo[*it].nTime < mapInfo[nOldest].nTime)
                nOldest = *it;
        }
        nI++;
    }
    assert(mapInfo.count(nOldest) == 1);
    NetworkPeer &network_peer = mapInfo[nOldest];
    if (--network_peer.nRefCount == 0)
    {
        SwapRandom(network_peer.nRandomPos, vRandom.size()-1);
        vRandom.pop_back();
        mapAddr.erase(network_peer);
        mapInfo.erase(nOldest);
        nNew--;
    }
    vNew.erase(nOldest);

    return 1;
}

void NetworkPeerManager::MakeTried(NetworkPeer& network_peer, int nId, int nOrigin)
{
    assert(vvNew[nOrigin].count(nId) == 1);

    // remove the entry from all new buckets
    for (std::vector<std::set<int> >::iterator it = vvNew.begin(); it != vvNew.end(); it++)
    {
        if ((*it).erase(nId))
            network_peer.nRefCount--;
    }
    nNew--;

    assert(network_peer.nRefCount == 0);

    // what tried bucket to move the entry to
    int nKBucket = this->GetTriedBucket(network_peer, nKey);
    std::vector<int> &vTried = vvTried[nKBucket];

    // first check whether there is place to just add it
    if (vTried.size() < NPMConstants::TRIED_BUCKET_SIZE)
    {
        vTried.push_back(nId);
        nTried++;
        network_peer.fInTried = true;
        return;
    }

    // otherwise, find an item to evict
    int nPos = SelectTried(nKBucket);

    // find which new bucket it belongs to
    assert(mapInfo.count(vTried[nPos]) == 1);
    int nUBucket = this->GetNewBucket(mapInfo[vTried[nPos]], nKey);
    std::set<int> &vNew = vvNew[nUBucket];

    // remove the to-be-replaced tried entry from the tried set
    NetworkPeer& old_network_peer = mapInfo[vTried[nPos]];
    old_network_peer.fInTried = false;
    old_network_peer.nRefCount = 1;
    // do not update nTried, as we are going to move something else there immediately

    // check whether there is place in that one,
    if (vNew.size() < NPMConstants::NEW_BUCKET_SIZE)
    {
        // if so, move it back there
        vNew.insert(vTried[nPos]);
    } else {
        // otherwise, move it to the new bucket nId came from (there is certainly place there)
        vvNew[nOrigin].insert(vTried[nPos]);
    }
    nNew++;

    vTried[nPos] = nId;
    // we just overwrote an entry in vTried; no need to update nTried
    network_peer.fInTried = true;
    return;
}
