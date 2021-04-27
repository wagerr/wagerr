// Copyright (c) 2011-2013 The Bitcoin Core developers
// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Wagerr Test Suite

#include "test_wagerr.h"

#include "betting/bet.h"
#include "betting/bet_db.h"
#include "main.h"
#include "random.h"
#include "txdb.h"
#include "guiinterface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include <boost/test/unit_test.hpp>

CClientUIInterface uiInterface;
CWallet* pwalletMain;

uint256 insecure_rand_seed = GetRandHash();
FastRandomContext insecure_rand_ctx(insecure_rand_seed);

extern bool fPrintToConsole;
extern void noui_connect();

BasicTestingSetup::BasicTestingSetup()
{
        RandomInit();
        ECC_Start();
        SetupEnvironment();
        fPrintToDebugLog = false; // don't want to write to debug.log file
        fCheckBlockIndex = true;
        SelectParams(CBaseChainParams::UNITTEST);
}
BasicTestingSetup::~BasicTestingSetup()
{
        ECC_Stop();
}

TestingSetup::TestingSetup()
{
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
#endif
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_wagerr_%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);

        bettingsView = new CBettingsView();
        // create Level DB storage for global betting database
        bettingsView->mappingsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-mappings"), CBettingDB::dbWrapperCacheSize(), true);
        // create cacheble betting DB with LevelDB storage as main storage
        bettingsView->mappings = MakeUnique<CBettingDB>(*bettingsView->mappingsStorage.get());

        bettingsView->eventsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-events"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->events = MakeUnique<CBettingDB>(*bettingsView->eventsStorage.get());

        bettingsView->resultsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-results"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->results = MakeUnique<CBettingDB>(*bettingsView->resultsStorage.get());

        bettingsView->betsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-bets"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->bets = MakeUnique<CBettingDB>(*bettingsView->betsStorage.get());

        bettingsView->fieldEventsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-fieldevents"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->fieldEvents = MakeUnique<CBettingDB>(*bettingsView->fieldEventsStorage.get());

        bettingsView->fieldResultsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-fieldresults"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->fieldResults = MakeUnique<CBettingDB>(*bettingsView->fieldResultsStorage.get());

        bettingsView->fieldBetsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-fieldbets"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->fieldBets = MakeUnique<CBettingDB>(*bettingsView->fieldBetsStorage.get());

        bettingsView->undosStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-undos"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->undos = MakeUnique<CBettingDB>(*bettingsView->undosStorage.get());

        bettingsView->payoutsInfoStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-payoutsinfo"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->payoutsInfo = MakeUnique<CBettingDB>(*bettingsView->payoutsInfoStorage.get());

        bettingsView->quickGamesBetsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-quickgamesbets"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->quickGamesBets = MakeUnique<CBettingDB>(*bettingsView->quickGamesBetsStorage.get());

        bettingsView->chainGamesLottoEventsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-cglottoevents"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->chainGamesLottoEvents = MakeUnique<CBettingDB>(*bettingsView->chainGamesLottoEventsStorage.get());

        bettingsView->chainGamesLottoBetsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-cglottobets"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->chainGamesLottoBets = MakeUnique<CBettingDB>(*bettingsView->chainGamesLottoBetsStorage.get());

        bettingsView->chainGamesLottoResultsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-cglottoresults"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->chainGamesLottoResults = MakeUnique<CBettingDB>(*bettingsView->chainGamesLottoResultsStorage.get());

        bettingsView->failedBettingTxsStorage = MakeUnique<CStorageLevelDB>(CBettingDB::MakeDbPath("test-failedtxs"), CBettingDB::dbWrapperCacheSize(), true);
        bettingsView->failedBettingTxs = MakeUnique<CBettingDB>(*bettingsView->failedBettingTxsStorage.get());


        InitBlockIndex();
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
#ifdef ENABLE_WALLET
        UnregisterValidationInterface(pwalletMain);
        delete pwalletMain;
        pwalletMain = NULL;
#endif
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
        delete bettingsView;
#ifdef ENABLE_WALLET
        bitdb.Flush(true);
        bitdb.Reset();
#endif
        boost::filesystem::remove_all(pathTemp);
}

[[noreturn]] void Shutdown(void* parg)
{
    std::exit(0);
}

[[noreturn]] void StartShutdown()
{
    std::exit(0);
}

bool ShutdownRequested()
{
  return false;
}
