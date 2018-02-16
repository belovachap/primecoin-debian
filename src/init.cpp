// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2013 PPCoin developers
// Copyright (c) 2013 Primecoin developers
// Copyright (c) 2017-2018 Chapman Shoop
// See COPYING for license.

#include <signal.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <openssl/crypto.h>

#include "net.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#include "version.h"
#include "walletdb.h"

#include "init.h"


CWallet* pwalletMain;
CClientUIInterface uiInterface;

#define MIN_CORE_FILEDESCRIPTORS 150

// Used to pass flags to the Bind() function
enum BindFlags {
    BF_NONE         = 0,
    BF_EXPLICIT     = (1U << 0),
    BF_REPORT_ERROR = (1U << 1)
};

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

//
// Thread management and startup/shutdown:
//
// The network-processing threads are all part of a thread group
// created by AppInit() or the Qt main() function.
//
// A clean exit happens when StartShutdown() or the SIGTERM
// signal handler sets fRequestShutdown, which triggers
// the DetectShutdownThread(), which interrupts the main thread group.
// DetectShutdownThread() then exits, which causes AppInit() to
// continue (it .joins the shutdown thread).
// Shutdown() is then
// called to clean up database connections, and stop other
// threads that should only be stopped after the main network-processing
// threads have exited.
//
// Note that if running -daemon the parent process returns from AppInit2
// before adding any threads to the threadGroup, so .join_all() returns
// immediately and the parent exits from main().
//
// Shutdown for Qt is very similar, only it uses a QTimer to detect
// fRequestShutdown getting set, and then does the normal Qt
// shutdown thing.
//

volatile bool fRequestShutdown = false;

void StartShutdown()
{
    fRequestShutdown = true;
}
bool ShutdownRequested()
{
    return fRequestShutdown;
}

static CCoinsViewDB *pcoinsdbview;

void Shutdown()
{
    printf("Shutdown : In progress...\n");
    static CCriticalSection cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown) return;

    RenameThread("primecoin-shutoff");
    nTransactionsUpdated++;
    bitdb.Flush(false);
    StopNode();
    {
        LOCK(cs_main);
        if (pwalletMain)
            pwalletMain->SetBestChain(CBlockLocator(pindexBest));
        if (pblocktree)
            pblocktree->Flush();
        if (pcoinsTip)
            pcoinsTip->Flush();
        delete pcoinsTip; pcoinsTip = NULL;
        delete pcoinsdbview; pcoinsdbview = NULL;
        delete pblocktree; pblocktree = NULL;
    }
    bitdb.Flush(true);
    boost::filesystem::remove(GetPidFile());
    UnregisterWallet(pwalletMain);
    delete pwalletMain;
    printf("Shutdown : done\n");
}

//
// Signal handlers are very limited in what they are allowed to do, so:
//
void DetectShutdownThread(boost::thread_group *threadGroup)
{
    // Tell the main threads to shutdown.
    while (!fRequestShutdown) {
        MilliSleep(500);
    }

    threadGroup->interrupt_all();
}

void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}





//////////////////////////////////////////////////////////////////////////////
//
// Start
//
#if !defined(QT_GUI)
bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    boost::thread* detectShutdownThread = NULL;

    bool app_init_success = false;
    try
    {
        //
        // Parameters
        //
        // If Qt is used, parameters/primecoin.conf are parsed in qt/primecoin.cpp's main()
        ParseParameters(argc, argv);
        if (!boost::filesystem::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified directory does not exist\n");
            Shutdown();
        }
        ReadConfigFile(mapArgs, mapMultiArgs);

        if (mapArgs.count("-?") || mapArgs.count("--help"))
        {
            // First part of help message is specific to primecoind / RPC client
            std::string strUsage = "Primecoin version " +
                FormatVersion(PRIMECOIN_VERSION) + "\n\n" +
                "Usage:\n" +
                "  primecoind [options]\n\n" +
                HelpMessage();

            fprintf(stdout, "%s", strUsage.c_str());
            return false;
        }

        detectShutdownThread = new boost::thread(boost::bind(&DetectShutdownThread, &threadGroup));
        app_init_success = AppInit2(threadGroup);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInit()");
    }
    
    if (!app_init_success) {
        if (detectShutdownThread) {
            detectShutdownThread->interrupt();
        }
        threadGroup.interrupt_all();
    }

    if (detectShutdownThread)
    {
        detectShutdownThread->join();
        delete detectShutdownThread;
        detectShutdownThread = NULL;
    }
    Shutdown();

    return app_init_success;
}

extern void noui_connect();
int main(int argc, char* argv[])
{
    // Connect primecoind signal handlers
    noui_connect();
    return (AppInit(argc, argv) ? 0 : 1);
}
#endif

bool static InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

bool static InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
    return true;
}

bool static Bind(const CService &addr, unsigned int flags) {
    if (!(flags & BF_EXPLICIT) && IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError)) {
        if (flags & BF_REPORT_ERROR)
            return InitError(strError);
        return false;
    }
    return true;
}

// Core-specific options shared between UI and daemon
std::string HelpMessage()
{
    std::string strUsage = std::string("Options:\n") +
        std::string("  -conf=<file>           Specify configuration file (default: primecoin.conf)\n") +
        std::string("  -pid=<file>            Specify pid file (default: primecoind.pid)\n") +
        std::string("  -gen                   Generate coins (default: 0)\n") +
        std::string("  -datadir=<dir>         Specify data directory\n") +
        std::string("  -dbcache=<n>           Set database cache size in megabytes (default: 25)\n") +
        std::string("  -timeout=<n>           Specify connection timeout in milliseconds (default: 5000)\n") +
        std::string("  -dns                   Allow DNS lookups for -addnode, -seednode and -connect\n") +
        std::string("  -port=<port>           Listen for connections on <port> (default: 9911 or testnet: 9913)\n") +
        std::string("  -testnet               Use the TestNet\n") +
        std::string("  -maxconnections=<n>    Maintain at most <n> connections to peers (default: 125)\n") +
        std::string("  -addnode=<ip>          Add a node to connect to and attempt to keep the connection open\n") +
        std::string("  -connect=<ip>          Connect only to the specified node(s)\n") +
        std::string("  -seednode=<ip>         Connect to a node to retrieve peer addresses, and disconnect\n") +
        std::string("  -externalip=<ip>       Specify your own public address\n") +
        std::string("  -onlynet=<net>         Only connect to nodes in network <net> (IPv4, IPv6 or Tor)\n") +
        std::string("  -discover              Discover own IP address (default: 1 when listening and no -externalip)\n") +
        std::string("  -listen                Accept connections from outside (default: 1 if no -connect)\n") +
        std::string("  -bind=<addr>           Bind to given address and always listen on it. Use [host]:port notation for IPv6\n") +
        std::string("  -dnsseed               Find peers using DNS lookup (default: 1 unless -connect)\n") +
        std::string("  -banscore=<n>          Threshold for disconnecting misbehaving peers (default: 100)\n") +
        std::string("  -bantime=<n>           Number of seconds to keep misbehaving peers from reconnecting (default: 86400)\n") +
        std::string("  -maxreceivebuffer=<n>  Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)\n") +
        std::string("  -maxsendbuffer=<n>     Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)\n") +
        std::string("  -paytxfee=<amt>        Fee per KB to add to transactions you send (minimum 1 cent)\n") +
#ifdef QT_GUI
        std::string("  -server                Accept command line and JSON-RPC commands\n") +
#endif
#if !defined(QT_GUI)
        std::string("  -daemon                Run in the background as a daemon and accept commands\n") +
#endif
        std::string("  -debug                 Output extra debugging information. Implies all other -debug* options\n") +
        std::string("  -debugnet              Output extra network debugging information\n") +
        std::string("  -logtimestamps         Prepend debug output with timestamp (default: 1)\n") +
        std::string("  -shrinkdebugfile       Shrink debug.log file on client startup (default: 1 when no -debug)\n") +
        std::string("  -printtoconsole        Send trace/debug info to console instead of debug.log file\n") +
        std::string("  -blocknotify=<cmd>     Execute command when the best block changes (%s in cmd is replaced by block hash)\n") +
        std::string("  -walletnotify=<cmd>    Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)\n") +
        std::string("  -alertnotify=<cmd>     Execute command when a relevant alert is received (%s in cmd is replaced by message)\n") +
        std::string("  -keypool=<n>           Set key pool size to <n> (default: 100)\n") +
        std::string("  -rescan                Rescan the block chain for missing wallet transactions\n") +
        std::string("  -salvagewallet         Attempt to recover private keys from a corrupt wallet.dat\n") +
        std::string("  -checkblocks=<n>       How many blocks to check at startup (default: 288, 0 = all)\n") +
        std::string("  -checklevel=<n>        How thorough the block verification is (0-4, default: 3)\n") +
        std::string("  -txindex               Maintain a full transaction index (default: 0)\n") +
        std::string("  -loadblock=<file>      Imports blocks from external blk000??.dat file\n") +
        std::string("  -reindex               Rebuild block chain index from current blk000??.dat files\n") +
        std::string("  -par=<n>               Set the number of script verification threads (up to 16, 0 = auto, <0 = leave that many cores free, default: 0)\n") +

        std::string("\nBlock creation options:\n") +
        std::string("  -blockminsize=<n>      Set minimum block size in bytes (default: 0)\n") +
        std::string("  -blockmaxsize=<n>      Set maximum block size in bytes (default: 250000)\n") +
        std::string("  -blockprioritysize=<n> Set maximum size of high-priority/low-fee transactions in bytes (default: 27000)\n");

    return strUsage;
}

struct CImportingNow
{
    CImportingNow() {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow() {
        assert(fImporting == true);
        fImporting = false;
    }
};

void ThreadImport(std::vector<boost::filesystem::path> vImportFiles)
{
    RenameThread("primecoin-loadblk");

    // -reindex
    if (fReindex) {
        CImportingNow imp;
        int nFile = 0;
        while (true) {
            CDiskBlockPos pos(nFile, 0);
            FILE *file = OpenBlockFile(pos, true);
            if (!file)
                break;
            printf("Reindexing block file blk%05u.dat...\n", (unsigned int)nFile);
            LoadExternalBlockFile(file, &pos);
            nFile++;
        }
        pblocktree->WriteReindexing(false);
        fReindex = false;
        printf("Reindexing finished\n");
        // To avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
        InitBlockIndex();
    }

    // hardcoded $DATADIR/bootstrap.dat
    boost::filesystem::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (boost::filesystem::exists(pathBootstrap)) {
        FILE *file = fopen(pathBootstrap.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            boost::filesystem::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            printf("Importing bootstrap.dat...\n");
            LoadExternalBlockFile(file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        }
    }

    // -loadblock=
    BOOST_FOREACH(boost::filesystem::path &path, vImportFiles) {
        FILE *file = fopen(path.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            printf("Importing %s...\n", path.string().c_str());
            LoadExternalBlockFile(file);
        }
    }
}

/** Initialize primecoin.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2(boost::thread_group& threadGroup)
{
    // ********************************************************* Step 1: setup
    umask(077);

    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);

    // ********************************************************* Step 2: parameter interactions

    fTestNet = GetBoolArg("-testnet");

    if (mapArgs.count("-bind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect is specified
        SoftSetBoolArg("-listen", true);
    }

    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        SoftSetBoolArg("-dnsseed", false);
        SoftSetBoolArg("-listen", false);
    }

    if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        SoftSetBoolArg("-upnp", false);
        // network discovery still needed to identify network (e.g. IPv4)
    }

    if (mapArgs.count("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        SoftSetBoolArg("-discover", false);
    }

    if (GetBoolArg("-salvagewallet")) {
        // Rewrite just private keys: rescan to find transactions
        SoftSetBoolArg("-rescan", true);
    }

    // Make sure enough file descriptors are available
    int nBind = std::max((int)mapArgs.count("-bind"), 1);
    nMaxConnections = GetArg("-maxconnections", 125);
    nMaxConnections = std::max(std::min(nMaxConnections, (int)(FD_SETSIZE - nBind - MIN_CORE_FILEDESCRIPTORS)), 0);
    int nFD = RaiseFileDescriptorLimit(nMaxConnections + MIN_CORE_FILEDESCRIPTORS);
    if (nFD < MIN_CORE_FILEDESCRIPTORS)
        return InitError("Not enough file descriptors available.");
    if (nFD - MIN_CORE_FILEDESCRIPTORS < nMaxConnections)
        nMaxConnections = nFD - MIN_CORE_FILEDESCRIPTORS;

    // ********************************************************* Step 3: parameter-to-internal-flags

    fDebug = GetBoolArg("-debug");
    fBenchmark = GetBoolArg("-benchmark");

    // -par=0 means autodetect, but nScriptCheckThreads==0 means no concurrency
    nScriptCheckThreads = GetArg("-par", 0);
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads += boost::thread::hardware_concurrency();
    if (nScriptCheckThreads <= 1)
        nScriptCheckThreads = 0;
    else if (nScriptCheckThreads > MAX_SCRIPTCHECK_THREADS)
        nScriptCheckThreads = MAX_SCRIPTCHECK_THREADS;

    // -debug implies fDebug*
    if (fDebug)
        fDebugNet = true;
    else
        fDebugNet = GetBoolArg("-debugnet");

    fPrintToConsole = GetBoolArg("-printtoconsole");
    fPrintToDebugger = GetBoolArg("-printtodebugger");
    fLogTimestamps = GetBoolArg("-logtimestamps", true);

    if (mapArgs.count("-timeout"))
    {
        int nNewTimeout = GetArg("-timeout", 5000);
        if (nNewTimeout > 0 && nNewTimeout < 600000)
            nConnectTimeout = nNewTimeout;
    }

    // Continue to put "/P2SH/" in the coinbase to monitor
    // BIP16 support.
    // This can be removed eventually...
    const char* pszP2SH = "/P2SH/";
    COINBASE_FLAGS << std::vector<unsigned char>(pszP2SH, pszP2SH+strlen(pszP2SH));

    // Fee-per-kilobyte amount considered the same as "free"
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    //
    // primecoin: -mintxfee and -minrelaytxfee options of bitcoin disabled
    // fixed min fees defined in MIN_TX_FEE and MIN_RELAY_TX_FEE

    if (mapArgs.count("-paytxfee"))
    {
        if (!ParseMoney(mapArgs["-paytxfee"], nTransactionFee) || nTransactionFee < CTransaction::nMinTxFee)
            return InitError(strprintf("Invalid amount for -paytxfee=<amount>: '%s'", mapArgs["-paytxfee"].c_str()));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction.");
    }

    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log

    std::string strDataDir = GetDataDir().string();

    // Make sure only a single Primecoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
        return InitError(strprintf("Cannot obtain a lock on data directory %s. Primecoin is probably already running.", strDataDir.c_str()));

    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("Primecoin version %s\n", FormatVersion(PRIMECOIN_VERSION).c_str());
    printf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
    if (!fLogTimestamps)
        printf("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str());
    printf("Default data directory %s\n", GetDefaultDataDir().string().c_str());
    printf("Using data directory %s\n", strDataDir.c_str());
    printf("Using at most %i connections (%i file descriptors available)\n", nMaxConnections, nFD);
    std::ostringstream strErrors;

    if (nScriptCheckThreads) {
        printf("Using %u threads for script verification\n", nScriptCheckThreads);
        for (int i=0; i<nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
    }

    int64 nStart;

    // ********************************************************* Step 5: verify wallet database integrity

    uiInterface.InitMessage("Verifying wallet...");

    if (!bitdb.Open(GetDataDir()))
    {
        // try moving the database env out of the way
        boost::filesystem::path pathDatabase = GetDataDir() / "database";
        boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%"PRI64d".bak", GetTime());
        try {
            boost::filesystem::rename(pathDatabase, pathDatabaseBak);
            printf("Moved old %s to %s. Retrying.\n", pathDatabase.string().c_str(), pathDatabaseBak.string().c_str());
        } catch(boost::filesystem::filesystem_error &error) {
             // failure is ok (well, not really, but it's not worse than what we started with)
        }

        // try again
        if (!bitdb.Open(GetDataDir())) {
            // if it still fails, it probably means we can't even create the database env
            std::string msg = strprintf("Error initializing wallet database environment %s!", strDataDir.c_str());
            return InitError(msg);
        }
    }

    if (GetBoolArg("-salvagewallet"))
    {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, "wallet.dat", true))
            return false;
    }

    if (boost::filesystem::exists(GetDataDir() / "wallet.dat"))
    {
        CDBEnv::VerifyResult r = bitdb.Verify("wallet.dat", CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK)
        {
            std::string msg = strprintf("Warning: wallet.dat corrupt, data salvaged!"
                                     " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup.", strDataDir.c_str());
            InitWarning(msg);
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return InitError("wallet.dat corrupt, salvage failed");
    }

    // ********************************************************* Step 6: network initialization

    // see Step 2: parameter interactions for more information about these
    fNoListen = !GetBoolArg("-listen", true);
    fDiscover = GetBoolArg("-discover", true);

    bool fBound = false;
    if (!fNoListen) {
        if (mapArgs.count("-bind")) {
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-bind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort()))
                    return InitError(strprintf("Cannot resolve -bind address: '%s'", strBind.c_str()));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
            }
        }
        else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
#ifdef USE_IPV6
            fBound |= Bind(CService(in6addr_any, GetListenPort()), BF_NONE);
#endif
            fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
        }
        if (!fBound)
            return InitError("Failed to listen on any port. Use -listen=0 if you want this.");
    }

    if (mapArgs.count("-externalip")) {
        BOOST_FOREACH(std::string strAddr, mapMultiArgs["-externalip"]) {
            CService addrLocal(strAddr, GetListenPort());
            if (!addrLocal.IsValid())
                return InitError(strprintf("Cannot resolve -externalip address: '%s'", strAddr.c_str()));
            AddLocal(CService(strAddr, GetListenPort()), LOCAL_MANUAL);
        }
    }

    BOOST_FOREACH(std::string strDest, mapMultiArgs["-seednode"])
        AddOneShot(strDest);

    // ********************************************************* Step 7: load block chain

    fReindex = GetBoolArg("-reindex");

    // Upgrading to 0.8; hard-link the old blknnnn.dat files into /blocks/
    boost::filesystem::path blocksDir = GetDataDir() / "blocks";
    if (!boost::filesystem::exists(blocksDir))
    {
        boost::filesystem::create_directories(blocksDir);
        bool linked = false;
        for (unsigned int i = 1; i < 10000; i++) {
            boost::filesystem::path source = GetDataDir() / strprintf("blk%04u.dat", i);
            if (!boost::filesystem::exists(source)) break;
            boost::filesystem::path dest = blocksDir / strprintf("blk%05u.dat", i-1);
            try {
                boost::filesystem::create_hard_link(source, dest);
                printf("Hardlinked %s -> %s\n", source.string().c_str(), dest.string().c_str());
                linked = true;
            } catch (boost::filesystem::filesystem_error & e) {
                // Note: hardlink creation failing is not a disaster, it just means
                // blocks will get re-downloaded from peers.
                printf("Error hardlinking blk%04u.dat : %s\n", i, e.what());
                break;
            }
        }
        if (linked)
        {
            fReindex = true;
        }
    }

    // cache size calculations
    size_t nTotalCache = GetArg("-dbcache", 25) << 20;
    if (nTotalCache < (1 << 22))
        nTotalCache = (1 << 22); // total cache cannot be less than 4 MiB
    size_t nBlockTreeDBCache = nTotalCache / 8;
    if (nBlockTreeDBCache > (1 << 21) && !GetBoolArg("-txindex", false))
        nBlockTreeDBCache = (1 << 21); // block tree db cache shouldn't be larger than 2 MiB
    nTotalCache -= nBlockTreeDBCache;
    size_t nCoinDBCache = nTotalCache / 2; // use half of the remaining cache for coindb cache
    nTotalCache -= nCoinDBCache;
    nCoinCacheSize = nTotalCache / 300; // coins in memory require around 300 bytes

    bool fLoaded = false;
    while (!fLoaded) {
        bool fReset = fReindex;
        std::string strLoadError;

        uiInterface.InitMessage("Loading block index...");

        nStart = GetTimeMillis();
        do {
            try {
                UnloadBlockIndex();
                delete pcoinsTip;
                delete pcoinsdbview;
                delete pblocktree;

                pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
                pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex);
                pcoinsTip = new CCoinsViewCache(*pcoinsdbview);

                if (fReindex)
                    pblocktree->WriteReindexing(true);

                if (!LoadBlockIndex()) {
                    strLoadError = "Error loading block database";
                    break;
                }

                // Initialize the block index (no-op if non-empty database was already loaded)
                if (!InitBlockIndex()) {
                    strLoadError = "Error initializing block database";
                    break;
                }

                uiInterface.InitMessage("Verifying blocks...");
                if (!VerifyDB()) {
                    strLoadError = "Corrupted block database detected";
                    break;
                }
            } catch(std::exception &e) {
                strLoadError = "Error opening block database";
                break;
            }

            fLoaded = true;
        } while(false);

        if (!fLoaded) {
            // first suggest a reindex
            if (!fReset) {
                bool fRet = uiInterface.ThreadSafeMessageBox(
                    strLoadError + ".\nDo you want to rebuild the block database now?",
                    "", CClientUIInterface::MSG_ERROR | CClientUIInterface::BTN_ABORT);
                if (fRet) {
                    fReindex = true;
                    fRequestShutdown = false;
                } else {
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }

    if (mapArgs.count("-txindex") && fTxIndex != GetBoolArg("-txindex", false))
        return InitError("You need to rebuild the databases using -reindex to change -txindex");

    // as LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill primecoin-qt during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown)
    {
        printf("Shutdown requested. Exiting.\n");
        return false;
    }
    printf(" block index %15"PRI64d"ms\n", GetTimeMillis() - nStart);

    if (mapArgs.count("-printblock"))
    {
        std::string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                block.ReadFromDisk(pindex);
                block.BuildMerkleTree();
                block.print();
                printf("\n");
                nFound++;
            }
        }
        if (nFound == 0)
            printf("No blocks matching %s were found\n", strMatch.c_str());
        return false;
    }

    // ********************************************************* Step 8: load wallet

    uiInterface.InitMessage("Loading wallet...");

    nStart = GetTimeMillis();
    bool fFirstRun = true;
    pwalletMain = new CWallet("wallet.dat");
    DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            strErrors << "Error loading wallet.dat: Wallet corrupted" << "\n";
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            std::string msg("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                         " or address book entries might be missing or incorrect.");
            InitWarning(msg);
        }
        else if (nLoadWalletRet == DB_TOO_NEW)
            strErrors << "Error loading wallet.dat: Wallet requires newer version of Primecoin" << "\n";
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            strErrors << "Wallet needed to be rewritten: restart Primecoin to complete" << "\n";
            printf("%s", strErrors.str().c_str());
            return InitError(strErrors.str());
        }
        else
            strErrors << "Error loading wallet.dat" << "\n";
    }

    if (fFirstRun)
    {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();

        CPubKey newDefaultKey;
        if (pwalletMain->GetKeyFromPool(newDefaultKey, false)) {
            pwalletMain->SetDefaultKey(newDefaultKey);
            if (!pwalletMain->SetAddressBookName(pwalletMain->vchDefaultKey.GetID(), ""))
                strErrors << "Cannot write default address\n";
        }

        pwalletMain->SetBestChain(CBlockLocator(pindexBest));
    }

    printf("%s", strErrors.str().c_str());
    printf(" wallet      %15"PRI64d"ms\n", GetTimeMillis() - nStart);

    RegisterWallet(pwalletMain);

    CBlockIndex *pindexRescan = pindexBest;
    if (GetBoolArg("-rescan"))
        pindexRescan = pindexGenesisBlock;
    else
    {
        CWalletDB walletdb("wallet.dat");
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = locator.GetBlockIndex();
        else
            pindexRescan = pindexGenesisBlock;
    }
    if (pindexBest && pindexBest != pindexRescan)
    {
        uiInterface.InitMessage("Rescanning...");
        printf("Rescanning last %i blocks (from block %i)...\n", pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        pwalletMain->ScanForWalletTransactions(pindexRescan, true);
        printf(" rescan      %15"PRI64d"ms\n", GetTimeMillis() - nStart);
        pwalletMain->SetBestChain(CBlockLocator(pindexBest));
        nWalletDBUpdated++;
    }

    // ********************************************************* Step 9: import blocks

    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    CValidationState state;
    if (!ConnectBestBlock(state))
        strErrors << "Failed to connect best block";

    std::vector<boost::filesystem::path> vImportFiles;
    if (mapArgs.count("-loadblock"))
    {
        BOOST_FOREACH(std::string strFile, mapMultiArgs["-loadblock"])
            vImportFiles.push_back(strFile);
    }
    threadGroup.create_thread(boost::bind(&ThreadImport, vImportFiles));

    // ********************************************************* Step 10: load peers

    uiInterface.InitMessage("Loading addresses...");

    nStart = GetTimeMillis();

    {
        CAddrDB adb;
        if (!adb.Read(addrman))
            printf("Invalid or missing peers.dat; recreating\n");
    }

    printf("Loaded %i addresses from peers.dat  %"PRI64d"ms\n",
           addrman.size(), GetTimeMillis() - nStart);

    // ********************************************************* Step 11: start node

    if (!CheckDiskSpace())
        return false;

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    RandAddSeedPerfmon();

    //// debug print
    printf("mapBlockIndex.size() = %"PRIszu"\n",   mapBlockIndex.size());
    printf("nBestHeight = %d\n",                   nBestHeight);
    printf("setKeyPool.size() = %"PRIszu"\n",      pwalletMain->setKeyPool.size());
    printf("mapWallet.size() = %"PRIszu"\n",       pwalletMain->mapWallet.size());
    printf("mapAddressBook.size() = %"PRIszu"\n",  pwalletMain->mapAddressBook.size());

    StartNode(threadGroup);

    // ********************************************************* Step 12: finished

    uiInterface.InitMessage("Done loading");

     // Add wallet transactions that aren't already in a block to mapTransactions
    pwalletMain->ReacceptWalletTransactions();

    // Run a thread to flush wallet periodically
    threadGroup.create_thread(boost::bind(&ThreadFlushWalletDB, boost::ref(pwalletMain->strWalletFile)));

    return !fRequestShutdown;
}
