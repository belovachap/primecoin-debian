// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include "network_peer.h"
#include "hash.h"


int CAddrInfo::GetTriedBucket(const std::vector<unsigned char> &nKey) const
{
    CDataStream ss1(SER_GETHASH, 0);
    std::vector<unsigned char> vchKey = GetKey();
    ss1 << nKey << vchKey;
    uint64 hash1 = Hash(ss1.begin(), ss1.end()).Get64();

    CDataStream ss2(SER_GETHASH, 0);
    std::vector<unsigned char> vchGroupKey = GetGroup();
    ss2 << nKey << vchGroupKey << (hash1 % ADDRMAN_TRIED_BUCKETS_PER_GROUP);
    uint64 hash2 = Hash(ss2.begin(), ss2.end()).Get64();
    return hash2 % ADDRMAN_TRIED_BUCKET_COUNT;
}

int CAddrInfo::GetNewBucket(const std::vector<unsigned char> &nKey, const CNetAddr& src) const
{
    CDataStream ss1(SER_GETHASH, 0);
    std::vector<unsigned char> vchGroupKey = GetGroup();
    std::vector<unsigned char> vchSourceGroupKey = src.GetGroup();
    ss1 << nKey << vchGroupKey << vchSourceGroupKey;
    uint64 hash1 = Hash(ss1.begin(), ss1.end()).Get64();

    CDataStream ss2(SER_GETHASH, 0);
    ss2 << nKey << vchSourceGroupKey << (hash1 % ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP);
    uint64 hash2 = Hash(ss2.begin(), ss2.end()).Get64();
    return hash2 % ADDRMAN_NEW_BUCKET_COUNT;
}

bool CAddrInfo::IsTerrible(int64 nNow) const
{
    if (nLastTry && nLastTry >= nNow-60) // never remove things tried the last minute
        return false;

    if (nTime > nNow + 10*60) // came in a flying DeLorean
        return true;

    if (nTime==0 || nNow-nTime > ADDRMAN_HORIZON_DAYS*86400) // not seen in over a month
        return true;

    if (nLastSuccess==0 && nAttempts>=ADDRMAN_RETRIES) // tried three times and never a success
        return true;

    if (nNow-nLastSuccess > ADDRMAN_MIN_FAIL_DAYS*86400 && nAttempts>=ADDRMAN_MAX_FAILURES) // 10 successive failures in the last week
        return true;

    return false;
}

double CAddrInfo::GetChance(int64 nNow) const
{
    double fChance = 1.0;

    int64 nSinceLastSeen = nNow - nTime;
    int64 nSinceLastTry = nNow - nLastTry;

    if (nSinceLastSeen < 0) nSinceLastSeen = 0;
    if (nSinceLastTry < 0) nSinceLastTry = 0;

    fChance *= 600.0 / (600.0 + nSinceLastSeen);

    // deprioritize very recent attempts away
    if (nSinceLastTry < 60*10)
        fChance *= 0.01;

    // deprioritize 50% after each failed attempt
    for (int n=0; n<nAttempts; n++)
        fChance /= 1.5;

    return fChance;
}
