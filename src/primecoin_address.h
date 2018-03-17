// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __PRIMECOIN_ADDRESS_H__
#define __PRIMECOIN_ADDRESS_H__

#include <boost/variant.hpp>

#include "base58.h"
#include "key.h"
#include "script.h"


/** base58-encoded Primecoin addresses.
 * Public-key-hash-addresses have version 0 (or 111 testnet).
 * The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
 * Script-hash-addresses have version 5 (or 196 testnet).
 * The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
 */
class PrimecoinAddress;
class PrimecoinAddressVisitor : public boost::static_visitor<bool>
{
private:
    PrimecoinAddress *addr;
public:
    PrimecoinAddressVisitor(PrimecoinAddress *addrIn) : addr(addrIn) { }
    bool operator()(const CKeyID &id) const;
    bool operator()(const CScriptID &id) const;
    bool operator()(const CNoDestination &no) const;
};

class PrimecoinAddress : public CBase58Data
{
public:
    enum
    {
        PUBKEY_ADDRESS = 23,  // primecoin mainnet: pubkey address starts with 'A'
        SCRIPT_ADDRESS = 83,  // primecoin mainnet: script address starts with 'a'
        PUBKEY_ADDRESS_TEST = 111, // primecoin testnet: pubkey address starts with 'm'
        SCRIPT_ADDRESS_TEST = 196, // primecoin testnet: script address starts with '2'
    };

    bool Set(const CKeyID &id) {
        SetData(fTestNet ? PUBKEY_ADDRESS_TEST : PUBKEY_ADDRESS, &id, 20);
        return true;
    }

    bool Set(const CScriptID &id) {
        SetData(fTestNet ? SCRIPT_ADDRESS_TEST : SCRIPT_ADDRESS, &id, 20);
        return true;
    }

    bool Set(const CTxDestination &dest)
    {
        return boost::apply_visitor(PrimecoinAddressVisitor(this), dest);
    }

    bool IsValid() const
    {
        unsigned int nExpectedSize = 20;
        bool fExpectTestNet = false;
        switch(nVersion)
        {
            case PUBKEY_ADDRESS:
                nExpectedSize = 20; // Hash of public key
                fExpectTestNet = false;
                break;
            case SCRIPT_ADDRESS:
                nExpectedSize = 20; // Hash of CScript
                fExpectTestNet = false;
                break;

            case PUBKEY_ADDRESS_TEST:
                nExpectedSize = 20;
                fExpectTestNet = true;
                break;
            case SCRIPT_ADDRESS_TEST:
                nExpectedSize = 20;
                fExpectTestNet = true;
                break;

            default:
                return false;
        }
        return fExpectTestNet == fTestNet && vchData.size() == nExpectedSize;
    }

    PrimecoinAddress()
    {
    }

    PrimecoinAddress(const CTxDestination &dest)
    {
        Set(dest);
    }

    PrimecoinAddress(const std::string& strAddress)
    {
        SetString(strAddress);
    }

    PrimecoinAddress(const char* pszAddress)
    {
        SetString(pszAddress);
    }

    CTxDestination Get() const {
        if (!IsValid())
            return CNoDestination();
        switch (nVersion) {
        case PUBKEY_ADDRESS:
        case PUBKEY_ADDRESS_TEST: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            return CKeyID(id);
        }
        case SCRIPT_ADDRESS: 
        case SCRIPT_ADDRESS_TEST: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            return CScriptID(id);
        }
        }
        return CNoDestination();
    }

    bool GetKeyID(CKeyID &keyID) const {
        if (!IsValid())
            return false;
        switch (nVersion) {
        case PUBKEY_ADDRESS: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            keyID = CKeyID(id);
            return true;
        }
        default: return false;
        }
    }

    bool IsScript() const {
        if (!IsValid())
            return false;
        switch (nVersion) {
        case SCRIPT_ADDRESS: {
            return true;
        }
        default: return false;
        }
    }
};

bool inline PrimecoinAddressVisitor::operator()(const CKeyID &id) const         { return addr->Set(id); }
bool inline PrimecoinAddressVisitor::operator()(const CScriptID &id) const      { return addr->Set(id); }
bool inline PrimecoinAddressVisitor::operator()(const CNoDestination &id) const { return false; }

#endif // __PRIMECOIN_ADDRESS_H__