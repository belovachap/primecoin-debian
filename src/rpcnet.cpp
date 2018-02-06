// Copyright (c) 2009-2012 Bitcoin Developers
// Copyright (c) 2012-2013 PPCoin developers
// Copyright (c) 2013 Primecoin developers
// Copyright (c) 2017-2018 Chapman Shoop
// See COPYING for license.

#include "net.h"
#include "bitcoinrpc.h"
#include "base58.h"

json_spirit::Value getconnectioncount(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getconnectioncount\n"
            "Returns the number of connections to other nodes.");

    LOCK(cs_vNodes);
    return (int)vNodes.size();
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    BOOST_FOREACH(CNode* pnode, vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

json_spirit::Value getpeerinfo(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getpeerinfo\n"
            "Returns data about each connected network node.");

    std::vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    json_spirit::Array ret;

    BOOST_FOREACH(const CNodeStats& stats, vstats) {
        json_spirit::Object obj;

        obj.push_back(json_spirit::Pair("addr", stats.addrName));
        obj.push_back(json_spirit::Pair("services", strprintf("%08"PRI64x, stats.nServices)));
        obj.push_back(json_spirit::Pair("lastsend", (boost::int64_t)stats.nLastSend));
        obj.push_back(json_spirit::Pair("lastrecv", (boost::int64_t)stats.nLastRecv));
        obj.push_back(json_spirit::Pair("bytessent", (boost::int64_t)stats.nSendBytes));
        obj.push_back(json_spirit::Pair("bytesrecv", (boost::int64_t)stats.nRecvBytes));
        obj.push_back(json_spirit::Pair("conntime", (boost::int64_t)stats.nTimeConnected));
        obj.push_back(json_spirit::Pair("version", stats.nVersion));
        // Use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the JSON output by putting special characters in
        // their ver message.
        obj.push_back(json_spirit::Pair("subver", stats.cleanSubVer));
        obj.push_back(json_spirit::Pair("inbound", stats.fInbound));
        obj.push_back(json_spirit::Pair("startingheight", stats.nStartingHeight));
        obj.push_back(json_spirit::Pair("banscore", stats.nMisbehavior));
        if (stats.fSyncNode)
            obj.push_back(json_spirit::Pair("syncnode", true));

        ret.push_back(obj);
    }

    return ret;
}

json_spirit::Value addnode(const json_spirit::Array& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw std::runtime_error(
            "addnode <node> <add|remove|onetry>\n"
            "Attempts add or remove <node> from the addnode list or try a connection to <node> once.");

    std::string strNode = params[0].get_str();

    if (strCommand == "onetry")
    {
        CAddress addr;
        ConnectNode(addr, strNode.c_str());
        return json_spirit::Value::null;
    }

    LOCK(cs_vAddedNodes);
    std::vector<std::string>::iterator it = vAddedNodes.begin();
    for(; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add")
    {
        if (it != vAddedNodes.end())
            throw JSONRPCError(-23, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    }
    else if(strCommand == "remove")
    {
        if (it == vAddedNodes.end())
            throw JSONRPCError(-24, "Error: Node has not been added.");
        vAddedNodes.erase(it);
    }

    return json_spirit::Value::null;
}

json_spirit::Value getaddednodeinfo(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "getaddednodeinfo <dns> [node]\n"
            "Returns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"
            "If dns is false, only a list of added nodes will be provided,\n"
            "otherwise connected information will also be available.");

    bool fDns = params[0].get_bool();

    std::list<std::string> laddedNodes(0);
    if (params.size() == 1)
    {
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(std::string& strAddNode, vAddedNodes)
            laddedNodes.push_back(strAddNode);
    }
    else
    {
        std::string strNode = params[1].get_str();
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(std::string& strAddNode, vAddedNodes)
            if (strAddNode == strNode)
            {
                laddedNodes.push_back(strAddNode);
                break;
            }
        if (laddedNodes.size() == 0)
            throw JSONRPCError(-24, "Error: Node has not been added.");
    }

    if (!fDns)
    {
        json_spirit::Object ret;
        BOOST_FOREACH(std::string& strAddNode, laddedNodes)
            ret.push_back(json_spirit::Pair("addednode", strAddNode));
        return ret;
    }

    json_spirit::Array ret;

    std::list<std::pair<std::string, std::vector<CService> > > laddedAddreses(0);
    BOOST_FOREACH(std::string& strAddNode, laddedNodes)
    {
        std::vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, GetDefaultPort(), 0))
            laddedAddreses.push_back(make_pair(strAddNode, vservNode));
        else
        {
            json_spirit::Object obj;
            obj.push_back(json_spirit::Pair("addednode", strAddNode));
            obj.push_back(json_spirit::Pair("connected", false));
            json_spirit::Array addresses;
            obj.push_back(json_spirit::Pair("addresses", addresses));
        }
    }

    LOCK(cs_vNodes);
    for (std::list<std::pair<std::string, std::vector<CService> > >::iterator it = laddedAddreses.begin(); it != laddedAddreses.end(); it++)
    {
        json_spirit::Object obj;
        obj.push_back(json_spirit::Pair("addednode", it->first));

        json_spirit::Array addresses;
        bool fConnected = false;
        BOOST_FOREACH(CService& addrNode, it->second)
        {
            bool fFound = false;
            json_spirit::Object node;
            node.push_back(json_spirit::Pair("address", addrNode.ToString()));
            BOOST_FOREACH(CNode* pnode, vNodes)
                if (pnode->addr == addrNode)
                {
                    fFound = true;
                    fConnected = true;
                    node.push_back(json_spirit::Pair("connected", pnode->fInbound ? "inbound" : "outbound"));
                    break;
                }
            if (!fFound)
                node.push_back(json_spirit::Pair("connected", "false"));
            addresses.push_back(node);
        }
        obj.push_back(json_spirit::Pair("connected", fConnected));
        obj.push_back(json_spirit::Pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}


// make a public-private key pair (first introduced in ppcoin)
json_spirit::Value makekeypair(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "makekeypair [prefix]\n"
            "Make a public/private key pair.\n"
            "[prefix] is optional preferred prefix for the public key.\n");

    std::string strPrefix = "";
    if (params.size() > 0)
        strPrefix = params[0].get_str();

    CKey key;
    int nCount = 0;
    do
    {
        key.MakeNewKey(false);
        nCount++;
    } while (nCount < 10000 && strPrefix != HexStr(key.GetPubKey().Raw()).substr(0, strPrefix.size()));

    if (strPrefix != HexStr(key.GetPubKey().Raw()).substr(0, strPrefix.size()))
        return json_spirit::Value::null;

    bool fCompressed;
    CSecret vchSecret = key.GetSecret(fCompressed);
    json_spirit::Object result;
    result.push_back(json_spirit::Pair("PublicKey", HexStr(key.GetPubKey().Raw())));
    result.push_back(json_spirit::Pair("PrivateKey", CBitcoinSecret(vchSecret, fCompressed).ToString()));
    return result;
}
