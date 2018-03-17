// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __NETWORK_PEER_DATABASE_H__
#define __NETWORK_PEER_DATABASE_H__

#include <boost/filesystem.hpp>

#include "hash.h"
#include "network_peer_manager.h"


/** Access to the (IP) address database (peers.dat) */
class NetworkPeerDatabase
{
    boost::filesystem::path file_path;

public:
    NetworkPeerDatabase();
    bool Write(const NetworkPeerManager& manager);
    bool Read(NetworkPeerManager& manager);
};

#endif // __NETWORK_PEER_DATABASE_H__
