// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __PRIMECOIN_SECRET_H__
#define __PRIMECOIN_SECRET_H__

#include "base58.h"


/** A base58-encoded secret key */
class PrimecoinSecret : public CBase58Data
{
public:
    enum
    {
        PRIVATE_KEY = 151,  // primecoin mainnet: private key starts with '?'
        PRIVATE_KEY_TEST = 239, // primecoin testnet: private key starts with '?'
    };

    void SetSecret(const CSecret& vchSecret, bool fCompressed)
    {
        assert(vchSecret.size() == 32);
        SetData(fTestNet ? PrimecoinSecret::PRIVATE_KEY_TEST : PrimecoinSecret::PRIVATE_KEY, &vchSecret[0], vchSecret.size());
        if (fCompressed)
            vchData.push_back(1);
    }

    CSecret GetSecret(bool &fCompressedOut)
    {
        CSecret vchSecret;
        vchSecret.resize(32);
        memcpy(&vchSecret[0], &vchData[0], 32);
        fCompressedOut = vchData.size() == 33;
        return vchSecret;
    }

    bool IsValid() const
    {
        bool fExpectTestNet = false;
        switch(nVersion)
        {
            case (PrimecoinSecret::PRIVATE_KEY):
                break;

            case (PrimecoinSecret::PRIVATE_KEY_TEST):
                fExpectTestNet = true;
                break;

            default:
                return false;
        }
        return fExpectTestNet == fTestNet && (vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1));
    }

    bool SetString(const char* pszSecret)
    {
        return CBase58Data::SetString(pszSecret) && IsValid();
    }

    bool SetString(const std::string& strSecret)
    {
        return SetString(strSecret.c_str());
    }

    PrimecoinSecret(const CSecret& vchSecret, bool fCompressed)
    {
        SetSecret(vchSecret, fCompressed);
    }

    PrimecoinSecret()
    {
    }
};

#endif // __PRIMECOIN_SECRET_H__
