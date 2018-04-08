// See COPYING for license.

#include "network_peer_database.h"

#include "hash.h"


NetworkPeerDatabase::NetworkPeerDatabase() {
    this->file_path = GetDataDir() / "peers.dat";
}

bool NetworkPeerDatabase::Write(const NetworkPeerManager& manager) {
    // Generate random temporary filename
    unsigned short randv = 0;
    RAND_bytes((unsigned char *)&randv, sizeof(randv));
    std::string tmpfn = strprintf("peers.dat.%04x", randv);

    // serialize addresses, checksum data up to that point, then append csum
    CDataStream ssPeers(SER_DISK, CLIENT_VERSION);
    ssPeers << FLATDATA(pchMessageStart);
    ssPeers << manager;
    uint256 hash = Hash(ssPeers.begin(), ssPeers.end());
    ssPeers << hash;

    // open temp output file, and associate with CAutoFile
    boost::filesystem::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fopen(pathTmp.string().c_str(), "wb");
    CAutoFile fileout = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!fileout)
    {
        return false;
    }

    // Write and commit header, data
    try
    {
        fileout << ssPeers;
    }
    catch (std::exception &e)
    {
        return false;
    }
    FileCommit(fileout);
    fileout.fclose();

    // replace existing peers.dat, if any, with new peers.dat.XXXX
    if (!RenameOver(pathTmp, this->file_path))
    {
        return false;
    }

    return true;
}

bool NetworkPeerDatabase::Read(NetworkPeerManager& manager) {
    // open input file, and associate with CAutoFile
    FILE *file = fopen(this->file_path.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!filein)
    {
        return false;
    }

    // use file size to size memory buffer
    int fileSize = GetFilesize(filein);
    int dataSize = fileSize - sizeof(uint256);
    //Don't try to resize to a negative number if file is small
    if ( dataSize < 0 ) dataSize = 0;
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try
    {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e)
    {
        return false;
    }
    filein.fclose();

    CDataStream ssPeers(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssPeers.begin(), ssPeers.end());
    if (hashIn != hashTmp)
    {
        return false;
    }

    unsigned char pchMsgTmp[4];
    try
    {
        // de-serialize file header (pchMessageStart magic number) and
        ssPeers >> FLATDATA(pchMsgTmp);

        // verify the network matches ours
        if (memcmp(pchMsgTmp, pchMessageStart, sizeof(pchMsgTmp))) {
            return false;
        }

        // de-serialize address data into one NetworkPeerManager object
        ssPeers >> manager;
    }
    catch (std::exception &e)
    {
        return false;
    }

    return true;
}
