// Copyright (c) 2018 RG Huckins
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include "network_peer.h"

#include "network_peer_manager.h"


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
    for (unsigned int i = 0; i < ADDRMAN_TRIED_ENTRIES_INSPECT_ON_EVICT && i < vTried.size(); i++)
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
        NetworkPeer &info = mapInfo[*it];
        if (info.IsTerrible())
        {
            if (--info.nRefCount == 0)
            {
                SwapRandom(info.nRandomPos, vRandom.size()-1);
                vRandom.pop_back();
                mapAddr.erase(info);
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
    NetworkPeer &info = mapInfo[nOldest];
    if (--info.nRefCount == 0)
    {
        SwapRandom(info.nRandomPos, vRandom.size()-1);
        vRandom.pop_back();
        mapAddr.erase(info);
        mapInfo.erase(nOldest);
        nNew--;
    }
    vNew.erase(nOldest);

    return 1;
}

void NetworkPeerManager::MakeTried(NetworkPeer& info, int nId, int nOrigin)
{
    assert(vvNew[nOrigin].count(nId) == 1);

    // remove the entry from all new buckets
    for (std::vector<std::set<int> >::iterator it = vvNew.begin(); it != vvNew.end(); it++)
    {
        if ((*it).erase(nId))
            info.nRefCount--;
    }
    nNew--;

    assert(info.nRefCount == 0);

    // what tried bucket to move the entry to
    int nKBucket = info.GetTriedBucket(nKey);
    std::vector<int> &vTried = vvTried[nKBucket];

    // first check whether there is place to just add it
    if (vTried.size() < ADDRMAN_TRIED_BUCKET_SIZE)
    {
        vTried.push_back(nId);
        nTried++;
        info.fInTried = true;
        return;
    }

    // otherwise, find an item to evict
    int nPos = SelectTried(nKBucket);

    // find which new bucket it belongs to
    assert(mapInfo.count(vTried[nPos]) == 1);
    int nUBucket = mapInfo[vTried[nPos]].GetNewBucket(nKey);
    std::set<int> &vNew = vvNew[nUBucket];

    // remove the to-be-replaced tried entry from the tried set
    NetworkPeer& infoOld = mapInfo[vTried[nPos]];
    infoOld.fInTried = false;
    infoOld.nRefCount = 1;
    // do not update nTried, as we are going to move something else there immediately

    // check whether there is place in that one,
    if (vNew.size() < ADDRMAN_NEW_BUCKET_SIZE)
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
    info.fInTried = true;
    return;
}

void NetworkPeerManager::Good_(const CService &addr, int64 nTime)
{
//    printf("Good: addr=%s\n", addr.ToString().c_str());

    int nId;
    NetworkPeer *pinfo = Find(addr, &nId);

    // if not found, bail out
    if (!pinfo)
        return;

    NetworkPeer &info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastSuccess = nTime;
    info.nLastTry = nTime;
    info.nTime = nTime;
    info.nAttempts = 0;

    // if it is already in the tried set, don't do anything else
    if (info.fInTried)
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
    MakeTried(info, nId, nUBucket);
}

bool NetworkPeerManager::Add_(const CAddress &addr, const CNetAddr& source, int64 nTimePenalty)
{
    if (!addr.IsRoutable())
        return false;

    bool fNew = false;
    int nId;
    NetworkPeer *pinfo = Find(addr, &nId);

    if (pinfo)
    {
        // periodically update nTime
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64 nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime && (!pinfo->nTime || pinfo->nTime < addr.nTime - nUpdateInterval - nTimePenalty))
            pinfo->nTime = std::max((int64)0, addr.nTime - nTimePenalty);

        // add services
        pinfo->nServices |= addr.nServices;

        // do not update if no new information is present
        if (!addr.nTime || (pinfo->nTime && addr.nTime <= pinfo->nTime))
            return false;

        // do not update if the entry was already in the "tried" table
        if (pinfo->fInTried)
            return false;

        // do not update if the max reference count is reached
        if (pinfo->nRefCount == ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
            return false;

        // stochastic test: previous nRefCount == N: 2^N times harder to increase it
        int nFactor = 1;
        for (int n=0; n<pinfo->nRefCount; n++)
            nFactor *= 2;
        if (nFactor > 1 && (GetRandInt(nFactor) != 0))
            return false;
    } else {
        pinfo = Create(addr, source, &nId);
        pinfo->nTime = std::max((int64)0, (int64)pinfo->nTime - nTimePenalty);
//        printf("Added %s [nTime=%fhr]\n", pinfo->ToString().c_str(), (GetAdjustedTime() - pinfo->nTime) / 3600.0);
        nNew++;
        fNew = true;
    }

    int nUBucket = pinfo->GetNewBucket(nKey, source);
    std::set<int> &vNew = vvNew[nUBucket];
    if (!vNew.count(nId))
    {
        pinfo->nRefCount++;
        if (vNew.size() == ADDRMAN_NEW_BUCKET_SIZE)
            ShrinkNew(nUBucket);
        vvNew[nUBucket].insert(nId);
    }
    return fNew;
}

void NetworkPeerManager::Attempt_(const CService &addr, int64 nTime)
{
    NetworkPeer *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    NetworkPeer &info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastTry = nTime;
    info.nAttempts++;
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
            NetworkPeer &info = mapInfo[vTried[nPos]];
            if (GetRandInt(1<<30) < fChanceFactor*info.GetChance()*(1<<30))
                return info;
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
            NetworkPeer &info = mapInfo[*it];
            if (GetRandInt(1<<30) < fChanceFactor*info.GetChance()*(1<<30))
                return info;
            fChanceFactor *= 1.2;
        }
    }
}

#ifdef DEBUG_ADDRMAN
int NetworkPeerManager::Check_()
{
    std::set<int> setTried;
    std::map<int, int> mapNew;

    if (vRandom.size() != nTried + nNew) return -7;

    for (std::map<int, NetworkPeer>::iterator it = mapInfo.begin(); it != mapInfo.end(); it++)
    {
        int n = (*it).first;
        NetworkPeer &info = (*it).second;
        if (info.fInTried)
        {

            if (!info.nLastSuccess) return -1;
            if (info.nRefCount) return -2;
            setTried.insert(n);
        } else {
            if (info.nRefCount < 0 || info.nRefCount > ADDRMAN_NEW_BUCKETS_PER_ADDRESS) return -3;
            if (!info.nRefCount) return -4;
            mapNew[n] = info.nRefCount;
        }
        if (mapAddr[info] != n) return -5;
        if (info.nRandomPos<0 || info.nRandomPos>=vRandom.size() || vRandom[info.nRandomPos] != n) return -14;
        if (info.nLastTry < 0) return -6;
        if (info.nLastSuccess < 0) return -8;
    }

    if (setTried.size() != nTried) return -9;
    if (mapNew.size() != nNew) return -10;

    for (int n=0; n<vvTried.size(); n++)
    {
        std::vector<int> &vTried = vvTried[n];
        for (std::vector<int>::iterator it = vTried.begin(); it != vTried.end(); it++)
        {
            if (!setTried.count(*it)) return -11;
            setTried.erase(*it);
        }
    }

    for (int n=0; n<vvNew.size(); n++)
    {
        std::set<int> &vNew = vvNew[n];
        for (std::set<int>::iterator it = vNew.begin(); it != vNew.end(); it++)
        {
            if (!mapNew.count(*it)) return -12;
            if (--mapNew[*it] == 0)
                mapNew.erase(*it);
        }
    }

    if (setTried.size()) return -13;
    if (mapNew.size()) return -15;

    return 0;
}
#endif

void NetworkPeerManager::GetAddr_(std::vector<CAddress> &vAddr)
{
    int nNodes = ADDRMAN_GETADDR_MAX_PCT*vRandom.size()/100;
    if (nNodes > ADDRMAN_GETADDR_MAX)
        nNodes = ADDRMAN_GETADDR_MAX;

    // perform a random shuffle over the first nNodes elements of vRandom (selecting from all)
    for (int n = 0; n<nNodes; n++)
    {
        int nRndPos = GetRandInt(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);
        vAddr.push_back(mapInfo[vRandom[n]]);
    }
}

void NetworkPeerManager::Connected_(const CService &addr, int64 nTime)
{
    NetworkPeer *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    NetworkPeer &info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    int64 nUpdateInterval = 20 * 60;
    if (nTime - info.nTime > nUpdateInterval)
        info.nTime = nTime;
}
