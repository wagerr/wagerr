// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The WAGERR developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "base58.h"
#include "core_io.h"
#include "init.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "zwgrchain.h"

#include "transactionrecord.h"
#include "betting/bet.h"
#include "betting/bet_db.h"
#include "betting/bet_tx.h"
#include "betting/bet_common.h"
#include "betting/bet_v2.h"
#include "betting/quickgames/dice.h"

#include <cstdlib>
#include <stdint.h>
#include <math.h>

#include "libzerocoin/Coin.h"
#include "spork.h"
#include <boost/algorithm/string.hpp>
#include "zwgr/deterministicmint.h"
#include <boost/assign/list_of.hpp>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/hex.hpp>

#include <univalue.h>
#include <iostream>


CPeerlessBetTx CheckAndGetPeerlessLegObj(const UniValue& legObj) {

    RPCTypeCheckObj(legObj, boost::assign::map_list_of("eventId", UniValue::VNUM)("outcome", UniValue::VNUM));

    uint32_t eventId = static_cast<uint32_t>(find_value(legObj, "eventId").get_int64());
    uint8_t outcome = (uint8_t) find_value(legObj, "outcome").get_int();

    if (outcome < moneyLineHomeWin || outcome > totalUnder)
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect peerless leg outcome.");

    CPeerlessExtendedEventDB plEvent;
    if (!bettingsView->events->Read(EventKey{eventId}, plEvent)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: there is no such Event: " + std::to_string(eventId));
    }

    if (GetBetPotentialOdds(CPeerlessLegDB{eventId, (PeerlessBetOutcomeType)outcome}, plEvent) == 0) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: potential odds is zero for event: " + std::to_string(eventId) + " outcome: " + std::to_string(outcome));
    }

    if (plEvent.nStage != 0) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: event " + std::to_string(eventId) + " cannot be part of parlay bet");
    }

    return CPeerlessBetTx{eventId, outcome};
}

CFieldBetTx CheckAndGetFieldLegObj(const UniValue& legObj) {

    RPCTypeCheckObj(legObj, boost::assign::map_list_of("eventId", UniValue::VNUM)("marketType", UniValue::VNUM)("contenderId", UniValue::VNUM));

    uint32_t eventId = static_cast<uint32_t>(find_value(legObj, "eventId").get_int64());
    FieldBetOutcomeType marketType = static_cast<FieldBetOutcomeType>(find_value(legObj, "marketType").get_int());
    uint32_t contenderId = static_cast<uint32_t>(find_value(legObj, "contenderId").get_int64());

    if (marketType < outright || marketType > show) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet market type for FieldEvent: " + std::to_string(eventId));
    }

    CFieldEventDB fEvent;
    if (!bettingsView->fieldEvents->Read(FieldEventKey{eventId}, fEvent)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: there is no such FieldEvent: " + std::to_string(eventId));
    }

    if (!fEvent.IsMarketOpen(marketType)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: market " + std::to_string((uint8_t)marketType) + " is closed for event " + std::to_string(eventId));
    }

    const auto& contender_it = fEvent.contenders.find(contenderId);
    if (contender_it == fEvent.contenders.end()) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: there is no such contenderId " + std::to_string(contenderId) + " in event " + std::to_string(eventId));
    }

    if (GetBetPotentialOdds(CFieldLegDB{eventId, marketType, contenderId}, fEvent) == 0) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: contender odds is zero for event: " + std::to_string(eventId) + " contenderId: " + std::to_string(contenderId));
    }

    if (fEvent.nStage != 0) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: event " + std::to_string(eventId) + " cannot be part of parlay bet");
    }

    return CFieldBetTx{eventId, (uint8_t) marketType, contenderId};
}

// TODO The Wagerr functions in this file are being placed here for speed of
// implementation, but should be moved to more appropriate locations once time
// allows.
UniValue listevents(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 2))
        throw std::runtime_error(
            "listevents\n"
            "\nGet live Wagerr events.\n"
            "\nArguments:\n"
            "1. \"openedOnly\" (bool, optional) Default - false. Gets only events which has no result.\n"
            "2. \"sportFilter\" (string, optional) Gets only events with input sport name.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": \"xxx\",         (string) The event ID\n"
            "    \"name\": \"xxx\",       (string) The name of the event\n"
            "    \"round\": \"xxx\",      (string) The round of the event\n"
            "    \"starting\": n,         (numeric) When the event will start\n"
            "    \"teams\": [\n"
            "      {\n"
            "        \"name\": \"xxxx\",  (string) Team to win\n"
            "        \"odds\": n          (numeric) Odds to win\n"
            "      }\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listevents", "") +
            HelpExampleCli("listevents", "true" "football") +
            HelpExampleRpc("listevents", "false" "tennis"));

    UniValue result{UniValue::VARR};

    std::string sportFilter = "";
    bool openedOnly = false;

    if (params.size() > 0) {
        openedOnly = params[0].get_bool();
    }
    if (params.size() > 1) {
        sportFilter = params[0].get_str();
    }

    LOCK(cs_main);

    auto it = bettingsView->events->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CPeerlessExtendedEventDB plEvent;
        CMappingDB mapping;
        CBettingDB::BytesToDbType(it->Value(), plEvent);

        if (!bettingsView->mappings->Read(MappingKey{sportMapping, plEvent.nSport}, mapping))
            continue;

        std::string sport = mapping.sName;

        // if event filter is set the don't list event if it doesn't match the filter.
        if (!sportFilter.empty() && sportFilter != sport)
            continue;

        /*
        // Only list active events.
        if ((time_t) plEvent.nStartTime < std::time(0)) {
            continue;
        }
        */

        // list only unresulted events
        if (openedOnly && bettingsView->results->Exists(ResultKey{plEvent.nEventId}))
            continue;

        //std::string round    = roundsIndex.find(plEvent.nStage)->second.sName;
        if (!bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping))
            continue;
        std::string tournament = mapping.sName;
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping))
            continue;
        std::string homeTeam = mapping.sName;
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping))
            continue;
        std::string awayTeam = mapping.sName;

        UniValue evt(UniValue::VOBJ);

        evt.push_back(Pair("event_id", (uint64_t) plEvent.nEventId));
        evt.push_back(Pair("sport", sport));
        evt.push_back(Pair("tournament", tournament));
        //evt.push_back(Pair("round", ""));

        evt.push_back(Pair("starting", (uint64_t) plEvent.nStartTime));
        evt.push_back(Pair("tester", (uint64_t) plEvent.nAwayTeam));

        UniValue teams(UniValue::VOBJ);

        teams.push_back(Pair("home", homeTeam));
        teams.push_back(Pair("away", awayTeam));

        evt.push_back(Pair("teams", teams));

        UniValue odds(UniValue::VARR);

        UniValue mlOdds(UniValue::VOBJ);
        UniValue spreadOdds(UniValue::VOBJ);
        UniValue totalsOdds(UniValue::VOBJ);

        mlOdds.push_back(Pair("mlHome", (uint64_t) plEvent.nHomeOdds));
        mlOdds.push_back(Pair("mlAway", (uint64_t) plEvent.nAwayOdds));
        mlOdds.push_back(Pair("mlDraw", (uint64_t) plEvent.nDrawOdds));

        if (plEvent.nEventCreationHeight < Params().WagerrProtocolV3StartHeight()) {
            spreadOdds.push_back(Pair("favorite", plEvent.fLegacyInitialHomeFavorite ? "home" : "away"));
        } else {
            spreadOdds.push_back(Pair("favorite", plEvent.nHomeOdds <= plEvent.nAwayOdds ? "home" : "away"));
        }
        spreadOdds.push_back(Pair("spreadPoints", (int64_t) plEvent.nSpreadPoints));
        spreadOdds.push_back(Pair("spreadHome", (uint64_t) plEvent.nSpreadHomeOdds));
        spreadOdds.push_back(Pair("spreadAway", (uint64_t) plEvent.nSpreadAwayOdds));

        totalsOdds.push_back(Pair("totalsPoints", (uint64_t) plEvent.nTotalPoints));
        totalsOdds.push_back(Pair("totalsOver", (uint64_t) plEvent.nTotalOverOdds));
        totalsOdds.push_back(Pair("totalsUnder", (uint64_t) plEvent.nTotalUnderOdds));

        odds.push_back(mlOdds);
        odds.push_back(spreadOdds);
        odds.push_back(totalsOdds);

        evt.push_back(Pair("odds", odds));

        result.push_back(evt);
    }

    return result;
}

std::string GetContenderNameById(uint32_t contenderId)
{
    CMappingDB mapping;
    if (!bettingsView->mappings->Read(MappingKey{contenderMapping, contenderId}, mapping)) {
        return "undefined";
    }
    else {
        return mapping.sName;
    }
}

UniValue GetContendersInfo(const std::map<uint32_t, ContenderInfo> mContenders)
{
    UniValue uContenders(UniValue::VARR);
    for (const auto& contender_it : mContenders) {
        UniValue uContender(UniValue::VOBJ);
        uContender.push_back(Pair("id", (uint64_t) contender_it.first));
        uContender.push_back(Pair("name", GetContenderNameById(contender_it.first)));
        uContender.push_back(Pair("modifier", (uint64_t) contender_it.second.nModifier));
        uContender.push_back(Pair("input-odds", (uint64_t) contender_it.second.nInputOdds));
        uContender.push_back(Pair("outright-odds", (uint64_t) contender_it.second.nOutrightOdds));
        uContender.push_back(Pair("place-odds", (uint64_t) contender_it.second.nPlaceOdds));
        uContender.push_back(Pair("show-odds", (uint64_t) contender_it.second.nShowOdds));
        uContenders.push_back(uContender);
    }
    return uContenders;
}

UniValue listfieldevents(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 2))
        throw std::runtime_error(
            "listfieldevents\n"
            "\nGet live Wagerr field events.\n"
            "\nArguments:\n"
            "1. \"openedOnly\" (bool, optional) Default - false. Gets only events which has no result.\n"
            "2. \"sportFilter\" (string, optional) Gets only events with input sport name.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": \"xxx\",         (string) The event ID\n"
            "    \"name\": \"xxx\",       (string) The name of the event\n"
            "    \"round\": \"xxx\",      (string) The round of the event\n"
            "    \"starting\": n,         (numeric) When the event will start\n"
            "    \"contenders\": [\n"
            "      {\n"
            "        \"name\": \"xxxx\",  (string) Conteder name\n"
            "        \"odds\": n          (numeric) Conteder win Odds\n"
            "      }\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listfieldevents", "") +
            HelpExampleCli("listfieldevents", "true" "horse racing") +
            HelpExampleRpc("listfieldevents", ""));

    UniValue result{UniValue::VARR};

    std::string sportFilter = "";
    bool openedOnly = false;

    if (params.size() > 0) {
        openedOnly = params[0].get_bool();
    }
    if (params.size() > 1) {
        sportFilter = params[0].get_str();
    }

    LOCK(cs_main);

    auto it = bettingsView->fieldEvents->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CFieldEventDB fEvent;
        CMappingDB mapping;
        CBettingDB::BytesToDbType(it->Value(), fEvent);

        // Only list active events.
        if ((time_t)fEvent.nStartTime < std::time(0)) {
            continue;
        }

        UniValue evt(UniValue::VOBJ);

        if (!bettingsView->mappings->Read(MappingKey{individualSportMapping, fEvent.nSport}, mapping))
            continue;

        std::string sport = mapping.sName;

        if (!sportFilter.empty() && sportFilter != sport)
            continue;

        // list only unresulted events
        if (openedOnly && bettingsView->fieldResults->Exists(ResultKey{fEvent.nEventId}))
            continue;

        evt.push_back(Pair("event_id", (uint64_t) fEvent.nEventId));
        evt.push_back(Pair("starting", (uint64_t) fEvent.nStartTime));
        evt.push_back(Pair("mrg-in", (uint64_t) fEvent.nMarginPercent));

        evt.push_back(Pair("sport", sport));

        if (!bettingsView->mappings->Read(MappingKey{tournamentMapping, fEvent.nTournament}, mapping))
            continue;
        evt.push_back(Pair("tournament", mapping.sName));

        if (!bettingsView->mappings->Read(MappingKey{roundMapping, fEvent.nStage}, mapping))
            continue;
        evt.push_back(Pair("round", mapping.sName));

        
        evt.push_back(Pair("contenders", GetContendersInfo(fEvent.contenders)));

        result.push_back(evt);
    }

    return result;
}

UniValue listeventsdebug(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 0))
        throw std::runtime_error(
            "listeventsdebug\n"
            "\nGet all Wagerr events from db.\n"

            "\nResult:\n"

            "\nExamples:\n" +
            HelpExampleCli("listeventsdebug", "") + HelpExampleRpc("listeventsdebug", ""));

    UniValue result{UniValue::VARR};

    auto time = std::time(0);

    LOCK(cs_main);

    auto it = bettingsView->events->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CPeerlessExtendedEventDB plEvent;
        CMappingDB mapping;
        CBettingDB::BytesToDbType(it->Value(), plEvent);

        std::stringstream strStream;

        auto started = ((time_t) plEvent.nStartTime < time) ? std::string("true") : std::string("false");

        strStream << "eventId = " << plEvent.nEventId << ", sport: " << plEvent.nSport << ", tournament: " << plEvent.nTournament << ", round: " << plEvent.nStage << ", home: " << plEvent.nHomeTeam << ", away: " << plEvent.nAwayTeam
            << ", homeOdds: " << plEvent.nHomeOdds << ", awayOdds: " << plEvent.nAwayOdds << ", drawOdds: " << plEvent.nDrawOdds
            << ", spreadPoints: " << plEvent.nSpreadPoints << ", spreadHomeOdds: " << plEvent.nSpreadHomeOdds << ", spreadAwayOdds: " << plEvent.nSpreadAwayOdds
            << ", totalPoints: " << plEvent.nTotalPoints << ", totalOverOdds: " << plEvent.nTotalOverOdds << ", totalUnderOdds: " << plEvent.nTotalUnderOdds
            << ", started: " << started << ".";

        if (!bettingsView->mappings->Read(MappingKey{sportMapping, plEvent.nSport}, mapping)) {
            strStream << " No sport mapping!";
        }
        if (!bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping)) {
            strStream << " No tournament mapping!";
        }
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping)) {
            strStream << " No home team mapping!";
        }
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping)) {
            strStream << " No away team mapping!";
        }

        result.push_back(strStream.str().c_str());
        strStream.clear();
    }

    return result;
}

UniValue listchaingamesevents(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 0))
        throw std::runtime_error(
            "listchaingamesevents\n"
            "\nGet live Wagerr chain game events.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": \"xxx\",         (string) The event ID\n"
            "    \"version\": \"xxx\",    (string) The current version\n"
            "    \"event-id\": \"xxx\",   (string) The ID of the chain games event\n"
            "    \"entry-fee\": n         (numeric) Fee to join game\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listchaingamesevents", "") + HelpExampleRpc("listchaingamesevents", ""));

    UniValue ret(UniValue::VARR);

    CBlockIndex *BlocksIndex = NULL;

    LOCK(cs_main);

    int height = (Params().NetworkID() == CBaseChainParams::MAIN) ? chainActive.Height() - 10500 : chainActive.Height() - 1500;
    BlocksIndex = chainActive[height];

    while (BlocksIndex) {
        CBlock block;
        ReadBlockFromDisk(block, BlocksIndex);

        for (CTransaction& tx : block.vtx) {

            uint256 txHash = tx.GetHash();

            const CTxIn &txin = tx.vin[0];
            bool validTx = IsValidOracleTx(txin, height);

            // Check each TX out for values
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                const CTxOut &txout = tx.vout[i];

                auto cgBettingTx = ParseBettingTx(txout);

                if (cgBettingTx == nullptr) continue;

                // Find any CChainGameEvents matching the specified id
                if (validTx && cgBettingTx->GetTxType() == cgEventTxType) {
                    CChainGamesEventTx* cgEvent = (CChainGamesEventTx*) cgBettingTx.get();
                    UniValue evt(UniValue::VOBJ);
                    evt.push_back(Pair("tx-id", txHash.ToString().c_str()));
                    evt.push_back(Pair("event-id", (uint64_t) cgEvent->nEventId));
                    evt.push_back(Pair("entry-fee", (uint64_t) cgEvent->nEntryFee));
                    ret.push_back(evt);
                }
            }
        }

        BlocksIndex = chainActive.Next(BlocksIndex);
    }

    return ret;
}


// TODO: There is a lot of code shared between `bets` and `listtransactions`.
// This would ideally be abstracted when time allows.
// TODO: The first parameter for account isn't used.
UniValue listbets(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw std::runtime_error(
            "listbets ( \"account\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"    (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
            "                                     If \"\" is set, it will list transactions for the default account.\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"event-id\":\"accountname\",       (string) The ID of the event being bet on.\n"
            "    \"team-to-win\":\"wagerraddress\",  (string) The team to win.\n"
            "    \"amount\": x.xxx,                  (numeric) The amount bet in WGR.\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 bets in the systems\n" +
            HelpExampleCli("listbets", ""));

    std::string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 3)
        if (params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue result{UniValue::VARR};

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const CWallet::TxItems & txOrdered{pwalletMain->wtxOrdered};

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        if (pwtx != 0) {

            uint256 txHash = (*pwtx).GetHash();

            for (unsigned int i = 0; i < (*pwtx).vout.size(); i++) {
                const CTxOut& txout = (*pwtx).vout[i];
                auto bettingTx = ParseBettingTx(txout);

                if (bettingTx == nullptr) continue;

                auto txType = bettingTx->GetTxType();

                if (txType == plBetTxType) {
                    CPeerlessBetTx* plBet = (CPeerlessBetTx*) bettingTx.get();
                    UniValue entry(UniValue::VOBJ);
                    entry.push_back(Pair("tx-id", txHash.ToString().c_str()));
                    entry.push_back(Pair("event-id", (uint64_t) plBet->nEventId));

                    // Retrieve the event details
                    CPeerlessExtendedEventDB plEvent;
                    if (bettingsView->events->Read(EventKey{plBet->nEventId}, plEvent)) {

                        entry.push_back(Pair("starting", plEvent.nStartTime));
                        CMappingDB mapping;
                        if (bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping)) {
                            entry.push_back(Pair("home", mapping.sName));
                        }
                        if (bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping)) {
                            entry.push_back(Pair("away", mapping.sName));
                        }
                        if (bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping)) {
                            entry.push_back(Pair("tournament", mapping.sName));
                        }
                    }

                    entry.push_back(Pair("team-to-win", (uint64_t) plBet->nOutcome));
                    entry.push_back(Pair("amount", ValueFromAmount(txout.nValue)));

                    std::string betResult = "pending";
                    CPeerlessResultDB plResult;
                    if (bettingsView->results->Read(ResultKey{plBet->nEventId}, plResult)) {

                        switch (plBet->nOutcome) {
                            case PeerlessBetOutcomeType::moneyLineHomeWin:
                                betResult = plResult.nHomeScore > plResult.nAwayScore ? "win" : "lose";

                                break;
                            case PeerlessBetOutcomeType::moneyLineAwayWin:
                                betResult = plResult.nAwayScore > plResult.nHomeScore ? "win" : "lose";

                                break;
                            case PeerlessBetOutcomeType::moneyLineDraw :
                                betResult = plResult.nHomeScore == plResult.nAwayScore ? "win" : "lose";

                                break;
                            case PeerlessBetOutcomeType::spreadHome:
                                betResult = "Check block explorer for result.";

                                break;
                            case PeerlessBetOutcomeType::spreadAway:
                                betResult = "Check block explorer for result.";

                                break;
                            case PeerlessBetOutcomeType::totalOver:
                                betResult = "Check block explorer for result.";

                                break;
                            case PeerlessBetOutcomeType::totalUnder:
                                betResult = "Check block explorer for result.";

                                break;
                            default :
                                LogPrintf("Invalid bet outcome");
                        }
                    }

                    entry.push_back(Pair("result", betResult));

                    result.push_back(entry);
                }
            }
        }

        if ((int)result.size() >= (nCount + nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)result.size())
        nFrom = result.size();
    if ((nFrom + nCount) > (int)result.size())
        nCount = result.size() - nFrom;

    std::vector<UniValue> arrTmp = result.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    result.clear();
    result.setArray();
    result.push_backV(arrTmp);

    return result;
}

UniValue getbet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "getbet \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet bet <txid>\n"

            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"

            "\nResult:\n"
            "{\n"
            "  \"tx-id\":\"accountname\",           (string) The transaction id.\n"
            "  \"event-id\":\"accountname\",        (string) The ID of the event being bet on.\n"
            "  \"starting\":\"accountname\",        (string) The event start time.\n"
            "  \"home\":\"accountname\",            (string) The home team name.\n"
            "  \"away\":\"accountname\",            (string) The away team name.\n"
            "  \"tournament\":\"accountname\",      (string) The tournament name\n"
            "  \"team-to-win\":\"wagerraddress\",   (string) The team to win.\n"
            "  \"amount\": x.xxx,                   (numeric) The amount bet in WGR.\n"
            "  \"result\":\"wagerraddress\",        (string) The bet result i.e win/lose.\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getbet", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"") +
            HelpExampleCli("getbet", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true") +
            HelpExampleRpc("getbet", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\""));

    LOCK(cs_main);

    uint256 txHash;
    txHash.SetHex(params[0].get_str());
    CBlockIndex* blockindex = nullptr;

    CTransaction tx;
    uint256 hash_block;
    if (!GetTransaction(txHash, tx, hash_block, true, blockindex)) {
        std::string errmsg;
        if (blockindex) {
            if (!(blockindex->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPC_MISC_ERROR, "Block not available");
            }
            errmsg = "No such transaction found in the provided block";
        } else {
            errmsg = fTxIndex
              ? "No such mempool or blockchain transaction"
              : "No such mempool transaction. Use -txindex to enable blockchain transaction queries";
        }
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, errmsg + ". Use gettransaction for wallet transactions.");
    }

    UniValue ret(UniValue::VOBJ);

    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];

        auto bettingTx = ParseBettingTx(txout);

        if (bettingTx == nullptr) continue;

        if (bettingTx->GetTxType() == plBetTxType) {
            CPeerlessBetTx* plBet = (CPeerlessBetTx*) bettingTx.get();

            ret.push_back(Pair("tx-id", txHash.ToString().c_str()));
            ret.push_back(Pair("event-id", (uint64_t)plBet->nEventId));

            // Retrieve the event details
            CPeerlessExtendedEventDB plEvent;
            if (bettingsView->events->Read(EventKey{plBet->nEventId}, plEvent)) {

                ret.push_back(Pair("starting", plEvent.nStartTime));
                CMappingDB mapping;
                if (bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping)) {
                    ret.push_back(Pair("home", mapping.sName));
                }
                if (bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping)) {
                    ret.push_back(Pair("away", mapping.sName));
                }
                if (bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping)) {
                    ret.push_back(Pair("tournament", mapping.sName));
                }
            }

            ret.push_back(Pair("team-to-win", (uint64_t)plBet->nOutcome));
            ret.push_back(Pair("amount", ValueFromAmount(txout.nValue)));

            std::string betResult = "pending";
            CPeerlessResultDB plResult;
            if (bettingsView->results->Read(ResultKey{plBet->nEventId}, plResult)) {

                switch (plBet->nOutcome) {
                case PeerlessBetOutcomeType::moneyLineHomeWin:
                    betResult = plResult.nHomeScore > plResult.nAwayScore ? "win" : "lose";

                    break;
                case PeerlessBetOutcomeType::moneyLineAwayWin:
                    betResult = plResult.nAwayScore > plResult.nHomeScore ? "win" : "lose";

                    break;
                case PeerlessBetOutcomeType::moneyLineDraw:
                    betResult = plResult.nHomeScore == plResult.nAwayScore ? "win" : "lose";

                    break;
                case PeerlessBetOutcomeType::spreadHome:
                    betResult = "Check block explorer for result.";

                    break;
                case PeerlessBetOutcomeType::spreadAway:
                    betResult = "Check block explorer for result.";

                    break;
                case PeerlessBetOutcomeType::totalOver:
                    betResult = "Check block explorer for result.";

                    break;
                case PeerlessBetOutcomeType::totalUnder:
                    betResult = "Check block explorer for result.";

                    break;
                default:
                    LogPrintf("Invalid bet outcome");
                }
            }
            ret.push_back(Pair("result", betResult));
        }
        break;
    }

    return ret;
}

UniValue listbetsdb(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "listbetsdb\n"
            "\nGet bets form bets DB.\n"

            "\nArguments:\n"
            "1. \"includeHandled\"   (bool, optional) Include bets that are already handled (default: false).\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"legs\":\n"
            "      [\n"
            "        {\n"
            "          \"event-id\": id,\n"
            "          \"outcome\": type,\n"
            "          \"lockedEvent\": {\n"
            "            \"homeOdds\": homeOdds\n"
            "            \"awayOdds\": awayOdds\n"
            "            \"drawOdds\": drawOdds\n"
            "            \"spreadVersion\": spreadVersion\n"
            "            \"spreadPoints\": spreadPoints\n"
            "            \"spreadHomeOdds\": spreadHomeOdds\n"
            "            \"spreadAwayOdds\": spreadAwayOdds\n"
            "            \"totalPoints\": totalPoints\n"
            "            \"totalOverOdds\": totalOverOdds\n"
            "            \"totalUnderOdds\": totalUnderOdds\n"
            "          }\n"
            "        },\n"
            "        ...\n"
            "      ],                          (list) The list of legs.\n"
            "    \"address\": playerAddress    (string) The player address.\n"
            "    \"amount\": x.xxx,            (numeric) The amount bet in WGR.\n"
            "    \"time\":\"betting time\",    (string) The betting time.\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listbetsdb", "true"));

    UniValue ret(UniValue::VARR);

    bool includeHandled = false;

    if (params.size() > 0) {
        includeHandled = params[0].get_bool();
    }

    LOCK(cs_main);

    auto it = bettingsView->bets->NewIterator();
    for(it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        PeerlessBetKey key;
        CPeerlessBetDB uniBet;
        CBettingDB::BytesToDbType(it->Value(), uniBet);
        CBettingDB::BytesToDbType(it->Key(), key);

        if (!includeHandled && uniBet.IsCompleted()) continue;

        UniValue uValue(UniValue::VOBJ);
        UniValue uLegs(UniValue::VARR);

        for (uint32_t i = 0; i < uniBet.legs.size(); i++) {
            auto &leg = uniBet.legs[i];
            auto &lockedEvent = uniBet.lockedEvents[i];
            UniValue uLeg(UniValue::VOBJ);
            UniValue uLockedEvent(UniValue::VOBJ);
            uLeg.push_back(Pair("event-id", (uint64_t) leg.nEventId));
            uLeg.push_back(Pair("outcome", (uint64_t) leg.nOutcome));
            uLockedEvent.push_back(Pair("homeOdds", (uint64_t) lockedEvent.nHomeOdds));
            uLockedEvent.push_back(Pair("awayOdds", (uint64_t) lockedEvent.nAwayOdds));
            uLockedEvent.push_back(Pair("drawOdds", (uint64_t) lockedEvent.nDrawOdds));
            uLockedEvent.push_back(Pair("spreadPoints", (int64_t) lockedEvent.nSpreadPoints));
            uLockedEvent.push_back(Pair("spreadHomeOdds", (uint64_t) lockedEvent.nSpreadHomeOdds));
            uLockedEvent.push_back(Pair("spreadAwayOdds", (uint64_t) lockedEvent.nSpreadAwayOdds));
            uLockedEvent.push_back(Pair("totalPoints", (uint64_t) lockedEvent.nTotalPoints));
            uLockedEvent.push_back(Pair("totalOverOdds", (uint64_t) lockedEvent.nTotalOverOdds));
            uLockedEvent.push_back(Pair("totalUnderOdds", (uint64_t) lockedEvent.nTotalUnderOdds));
            uLeg.push_back(Pair("lockedEvent", uLockedEvent));
            uLegs.push_back(uLeg);
        }
        uValue.push_back(Pair("betBlockHeight", (uint64_t) key.blockHeight));
        uValue.push_back(Pair("betTxHash", key.outPoint.hash.GetHex()));
        uValue.push_back(Pair("betTxOut", (uint64_t) key.outPoint.n));
        uValue.push_back(Pair("legs", uLegs));
        uValue.push_back(Pair("address", uniBet.playerAddress.ToString()));
        uValue.push_back(Pair("amount", ValueFromAmount(uniBet.betAmount)));
        uValue.push_back(Pair("time", (uint64_t) uniBet.betTime));
        ret.push_back(uValue);
    }

    return ret;
}

std::string BetResultTypeToStr(BetResultType resType)
{
    switch (resType) {
        case betResultUnknown: return std::string("pending");
        case betResultWin: return std::string("win");
        case betResultLose: return std::string("lose");
        case betResultRefund: return std::string("refund");
        case betResultPartialWin: return std::string("partial-win");
        case betResultPartialLose: return std::string("partial-lose");
        default: return std::string("error");
    }
}

std::string EventResultTypeToStr(ResultType resType)
{
    switch (resType) {
        case standardResult: return std::string("standard");
        case eventRefund: return std::string("event refund");
        case mlRefund: return std::string("ml refund");
        case spreadsRefund: return std::string("spreads refund");
        case totalsRefund: return std::string("totals refund");
        default: return std::string("error");
    }
}

template<class UniBetKeyDBType, class UniBetDBType>
void CollectPayoutInfo(UniValue& uValue, const UniBetKeyDBType& uniBetKey, const UniBetDBType& uniBet)
{
    if (uniBet.IsCompleted()) {
        if (uniBet.payoutHeight > 0) {
            auto it = bettingsView->payoutsInfo->NewIterator();
            for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{uniBet.payoutHeight, COutPoint{}})); it->Valid(); it->Next()) {
                PayoutInfoKey payoutKey;
                CPayoutInfoDB payoutInfo;
                CBettingDB::BytesToDbType(it->Key(), payoutKey);
                CBettingDB::BytesToDbType(it->Value(), payoutInfo);
                if (uniBet.payoutHeight != payoutKey.blockHeight) break;
                if (payoutInfo.betKey == uniBetKey) {
                    uValue.push_back(Pair("payoutTxHash", payoutKey.outPoint.hash.GetHex()));
                    uValue.push_back(Pair("payoutTxOut", (uint64_t) payoutKey.outPoint.n));
                    break;
                }
            }
        }
        else {
            uValue.push_back(Pair("payoutTxHash", "no"));
            uValue.push_back(Pair("payoutTxOut", "no"));
        }
    }
    else {
        uValue.push_back(Pair("payoutTxHash", "pending"));
        uValue.push_back(Pair("payoutTxOut", "pending"));
    }
}

UniValue CollectPLLegData(const CPeerlessLegDB& leg, const CPeerlessBaseEventDB& lockedEvent, const int64_t betTime, uint32_t betBlockHeight)
{
    UniValue uLeg(UniValue::VOBJ);
    UniValue uLockedEvent(UniValue::VOBJ);
    uLeg.push_back(Pair("leg-type", "peerless"));
    uLeg.push_back(Pair("event-id", (uint64_t) leg.nEventId));
    uLeg.push_back(Pair("outcome", (uint64_t) leg.nOutcome));

    uLockedEvent.push_back(Pair("homeOdds", (uint64_t) lockedEvent.nHomeOdds));
    uLockedEvent.push_back(Pair("awayOdds", (uint64_t) lockedEvent.nAwayOdds));
    uLockedEvent.push_back(Pair("drawOdds", (uint64_t) lockedEvent.nDrawOdds));
    uLockedEvent.push_back(Pair("spreadPoints", (int64_t) lockedEvent.nSpreadPoints));
    uLockedEvent.push_back(Pair("spreadHomeOdds", (uint64_t) lockedEvent.nSpreadHomeOdds));
    uLockedEvent.push_back(Pair("spreadAwayOdds", (uint64_t) lockedEvent.nSpreadAwayOdds));
    uLockedEvent.push_back(Pair("totalPoints", (uint64_t) lockedEvent.nTotalPoints));
    uLockedEvent.push_back(Pair("totalOverOdds", (uint64_t) lockedEvent.nTotalOverOdds));
    uLockedEvent.push_back(Pair("totalUnderOdds", (uint64_t) lockedEvent.nTotalUnderOdds));

    // Retrieve the event details
    CPeerlessExtendedEventDB plEvent;
    if (bettingsView->events->Read(EventKey{leg.nEventId}, plEvent)) {
        uLockedEvent.push_back(Pair("starting", plEvent.nStartTime));
        CMappingDB mapping;
        if (bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping)) {
            uLockedEvent.push_back(Pair("home", mapping.sName));
        }
        else {
            uLockedEvent.push_back(Pair("home", "undefined"));
        }
        if (bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping)) {
            uLockedEvent.push_back(Pair("away", mapping.sName));
        }
        else {
            uLockedEvent.push_back(Pair("away", "undefined"));
        }
        if (bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping)) {
            uLockedEvent.push_back(Pair("tournament", mapping.sName));
        }
        else {
            uLockedEvent.push_back(Pair("tournament", "undefined"));
        }
    }
    CPeerlessResultDB plResult;
    uint32_t legOdds = 0;
    if (bettingsView->results->Read(EventKey{leg.nEventId}, plResult)) {
        uLockedEvent.push_back(Pair("eventResultType", EventResultTypeToStr((ResultType) plResult.nResultType)));
        uLockedEvent.push_back(Pair("homeScore", (uint64_t) plResult.nHomeScore));
        uLockedEvent.push_back(Pair("awayScore", (uint64_t) plResult.nAwayScore));
        if (lockedEvent.nStartTime > 0 && betTime > ((int64_t)lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
            uLeg.push_back(Pair("legResultType", "refund - invalid bet"));
        }
        else {
            legOdds = GetBetOdds(leg, lockedEvent, plResult, (int64_t)betBlockHeight >= Params().WagerrProtocolV3StartHeight()).first;
            std::string legResultTypeStr;
            if (legOdds == 0) {
                legResultTypeStr = std::string("lose");
            }
            else if (legOdds == BET_ODDSDIVISOR / 2) {
                legResultTypeStr = std::string("half-lose");
            }
            else if (legOdds == BET_ODDSDIVISOR) {
                legResultTypeStr = std::string("refund");
            }
            else if (legOdds < GetBetPotentialOdds(leg, lockedEvent)) {
                legResultTypeStr = std::string("half-win");
            }
            else {
                legResultTypeStr = std::string("win");
            }
            uLeg.push_back(Pair("legResultType", legResultTypeStr));
        }
    }
    else {
        uLockedEvent.push_back(Pair("eventResultType", "event result not found"));
        uLockedEvent.push_back(Pair("homeScore", "undefined"));
        uLockedEvent.push_back(Pair("awayScore", "undefined"));
        uLeg.push_back(Pair("legResultType", "pending"));
    }
    uLeg.push_back(Pair("lockedEvent", uLockedEvent));

    return uLeg;
}

void CollectPLBetData(UniValue& uValue, const PeerlessBetKey& betKey, const CPeerlessBetDB& uniBet, bool requiredPayoutInfo = false) {

    UniValue uLegs(UniValue::VARR);

    uValue.push_back(Pair("type", "peerless"));

    for (uint32_t i = 0; i < uniBet.legs.size(); i++) {
        auto &leg = uniBet.legs[i];
        auto &lockedEvent = uniBet.lockedEvents[i];
        uLegs.push_back(CollectPLLegData(leg, lockedEvent, uniBet.betTime, betKey.blockHeight));
    }
    uValue.push_back(Pair("betBlockHeight", (uint64_t) betKey.blockHeight));
    uValue.push_back(Pair("betTxHash", betKey.outPoint.hash.GetHex()));
    uValue.push_back(Pair("betTxOut", (uint64_t) betKey.outPoint.n));
    uValue.push_back(Pair("legs", uLegs));
    uValue.push_back(Pair("address", uniBet.playerAddress.ToString()));
    uValue.push_back(Pair("amount", ValueFromAmount(uniBet.betAmount)));
    uValue.push_back(Pair("time", (uint64_t) uniBet.betTime));
    uValue.push_back(Pair("completed", uniBet.IsCompleted() ? "yes" : "no"));
    uValue.push_back(Pair("betResultType", BetResultTypeToStr(uniBet.resultType)));
    uValue.push_back(Pair("payout", uniBet.IsCompleted() ? ValueFromAmount(uniBet.payout) : "pending"));

    if (requiredPayoutInfo) {
        CollectPayoutInfo(uValue, betKey, uniBet);
    }
}

std::string ContenderResultToString(uint8_t result) {
    switch(result) {
        case ContenderResult::DNF:
            return "DNF";
        case ContenderResult::place1:
            return "Place1";
        case ContenderResult::place2:
            return "Place2";
        case ContenderResult::place3:
            return "Place3";
        case ContenderResult::DNR:
            return "DNR";
        default:
            return "undefined";
    }
}

UniValue CollectFieldLegData(const CFieldLegDB& leg, const CFieldEventDB& lockedEvent, const int64_t betTime)
{
    UniValue uLeg(UniValue::VOBJ);
    UniValue uLockedEvent(UniValue::VOBJ);
    uLeg.push_back(Pair("leg-type", "field"));
    uLeg.push_back(Pair("event-id", (uint64_t) leg.nEventId));
    uLeg.push_back(Pair("outcome", (uint64_t) leg.nOutcome));

    uLockedEvent.push_back(Pair("contenders", GetContendersInfo(lockedEvent.contenders)));
    uLockedEvent.push_back(Pair("starting", lockedEvent.nStartTime));
    CMappingDB mapping;
    if (bettingsView->mappings->Read(MappingKey{tournamentMapping, lockedEvent.nTournament}, mapping)) {
        uLockedEvent.push_back(Pair("tournament", mapping.sName));
    }
    else {
        uLockedEvent.push_back(Pair("tournament", "undefined"));
    }
    CFieldResultDB fResult;
    uint32_t legOdds = 0;
    if (bettingsView->fieldResults->Read(FieldResultKey{leg.nEventId}, fResult)) {
        uLockedEvent.push_back(Pair("eventResultType", EventResultTypeToStr((ResultType) fResult.nResultType)));
        UniValue results(UniValue::VARR);
        for (auto &contenderResult : fResult.contendersResults) {
            UniValue result(UniValue::VOBJ);
            result.push_back(Pair("contenderId", (int64_t) (contenderResult.first)));
            result.push_back(Pair("name", GetContenderNameById(contenderResult.first)));
            result.push_back(Pair("result", ContenderResultToString(contenderResult.second)));
            results.push_back(result);
        }
        uLockedEvent.push_back(Pair("contenderResults", results));
        if (lockedEvent.nStartTime > 0 && betTime > ((int64_t)lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
            uLeg.push_back(Pair("legResultType", "refund - invalid bet"));
        }
        else {
            legOdds = GetBetOdds(leg, lockedEvent, fResult).first;
            std::string legResultTypeStr;
            if (legOdds == 0) {
                legResultTypeStr = std::string("lose");
            }
            else if (legOdds == BET_ODDSDIVISOR) {
                legResultTypeStr = std::string("refund");
            }
            else {
                legResultTypeStr = std::string("win");
            }
            uLeg.push_back(Pair("legResultType", legResultTypeStr));
        }
    }
    else {
        uLockedEvent.push_back(Pair("eventResultType", "event result not found"));
        uLeg.push_back(Pair("legResultType", "pending"));
    }
    uLeg.push_back(Pair("lockedEvent", uLockedEvent));

    return uLeg;
}

void CollectFieldBetData(UniValue& uValue, const FieldBetKey& betKey, const CFieldBetDB& fieldBet, bool requiredPayoutInfo = false) {

    UniValue uLegs(UniValue::VARR);

    uValue.push_back(Pair("type", "field"));

    for (uint32_t i = 0; i < fieldBet.legs.size(); i++) {
        auto &leg = fieldBet.legs[i];
        auto &lockedEvent = fieldBet.lockedEvents[i];

        uLegs.push_back(CollectFieldLegData(leg, lockedEvent, fieldBet.betTime));
    }

    uValue.push_back(Pair("betBlockHeight", (uint64_t) betKey.blockHeight));
    uValue.push_back(Pair("betTxHash", betKey.outPoint.hash.GetHex()));
    uValue.push_back(Pair("betTxOut", (uint64_t) betKey.outPoint.n));
    uValue.push_back(Pair("legs", uLegs));
    uValue.push_back(Pair("address", fieldBet.playerAddress.ToString()));
    uValue.push_back(Pair("amount", ValueFromAmount(fieldBet.betAmount)));
    uValue.push_back(Pair("time", (uint64_t) fieldBet.betTime));
    uValue.push_back(Pair("completed", fieldBet.IsCompleted() ? "yes" : "no"));
    uValue.push_back(Pair("betResultType", BetResultTypeToStr(fieldBet.resultType)));
    uValue.push_back(Pair("payout", fieldBet.IsCompleted() ? ValueFromAmount(fieldBet.payout) : "pending"));

    if (requiredPayoutInfo) {
        CollectPayoutInfo(uValue, betKey, fieldBet);
    }
}

void CollectHybridBetData(UniValue& uValue, const HybridBetKey& betKey, const CHybridBetDB& hybridBet, bool requiredPayoutInfo = false) {

    UniValue uLegs(UniValue::VARR);

    uValue.push_back(Pair("type", "hybrid"));

    for (uint32_t i = 0; i < hybridBet.legs.size(); i++) {
        auto &leg = hybridBet.legs[i];
        auto &lockedEvent = hybridBet.lockedEvents[i];
        switch ((HybridVariantType) leg.variant.which()) {
            case HybridVariantType::PeerlessVariant:
            {
                const CPeerlessLegDB& plLeg = boost::get<CPeerlessLegDB>(leg.variant);
                const CPeerlessBaseEventDB& plEvent = boost::get<CPeerlessBaseEventDB>(lockedEvent.variant);
                uLegs.push_back(CollectPLLegData(plLeg, plEvent, hybridBet.betTime, betKey.blockHeight));
                break;
            }
            case HybridVariantType::FieldVariant:
            {
                const CFieldLegDB& fLeg = boost::get<CFieldLegDB>(leg.variant);
                const CFieldEventDB fEvent = boost::get<CFieldEventDB>(lockedEvent.variant);
                uLegs.push_back(CollectFieldLegData(fLeg, fEvent, hybridBet.betTime));
                break;
            }
            default:
                std::runtime_error("HybridBet: Undefined leg type");
        }
    }

    uValue.push_back(Pair("betBlockHeight", (uint64_t) betKey.blockHeight));
    uValue.push_back(Pair("betTxHash", betKey.outPoint.hash.GetHex()));
    uValue.push_back(Pair("betTxOut", (uint64_t) betKey.outPoint.n));
    uValue.push_back(Pair("legs", uLegs));
    uValue.push_back(Pair("address", hybridBet.playerAddress.ToString()));
    uValue.push_back(Pair("amount", ValueFromAmount(hybridBet.betAmount)));
    uValue.push_back(Pair("time", (uint64_t) hybridBet.betTime));
    uValue.push_back(Pair("completed", hybridBet.IsCompleted() ? "yes" : "no"));
    uValue.push_back(Pair("betResultType", BetResultTypeToStr(hybridBet.resultType)));
    uValue.push_back(Pair("payout", hybridBet.IsCompleted() ? ValueFromAmount(hybridBet.payout) : "pending"));

    if (requiredPayoutInfo) {
        CollectPayoutInfo(uValue, betKey, hybridBet);
    }
}

UniValue GetBets(uint32_t count, uint32_t from, CWallet *_pwalletMain, boost::optional<std::string> accountName, bool includeWatchonly) {
    UniValue ret(UniValue::VARR);

    bool fAllAccounts = true;
    if (accountName && *accountName != "*") {
        fAllAccounts = false;
    }

    auto it = bettingsView->bets->NewIterator();
    uint32_t skippedEntities = 0;
    for(it->SeekToLast(); it->Valid(); it->Prev()) {
        PeerlessBetKey key;
        CPeerlessBetDB uniBet;
        CBettingDB::BytesToDbType(it->Value(), uniBet);
        CBettingDB::BytesToDbType(it->Key(), key);

        if (_pwalletMain) {
            CTxDestination dest = uniBet.playerAddress.Get();
            isminetype scriptType = IsMine(*_pwalletMain, dest);
            if (scriptType == ISMINE_NO)
                continue;
            if (scriptType == ISMINE_WATCH_ONLY && !includeWatchonly)
                continue;
            if (!fAllAccounts && accountName && _pwalletMain->mapAddressBook.count(dest))
                if (_pwalletMain->mapAddressBook[dest].name != *accountName)
                    continue;
        }

        UniValue uValue(UniValue::VOBJ);

        CollectPLBetData(uValue, key, uniBet, true);

        if (skippedEntities == from) {
            ret.push_back(uValue);
        } else {
            skippedEntities++;
        }

        if (count != 0 && ret.size() == count) {
            break;
        }
    }
    std::vector<UniValue> arrTmp = ret.getValues();
    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

UniValue getallbets(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw std::runtime_error(
                "getallbets\n"
                "\nGet bets info for all wallets\n"

                "\nArguments:\n"
                "1. count (numeric, optional, default=10) Limit response to last bets number.\n"
                "2. from (numeric, optional, default=0) The number of bets to skip (from the last)\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"betBlockHeight\": height, (string) The hash of block wich store tx with bet opcode.\n"
                "    \"betTxHash\": txHash, (string) The hash of transaction wich store bet opcode.\n"
                "    \"betTxOut\": nOut, (numeric) The out number of transaction wich store bet opcode.\n"
                "    \"legs\": (array of objects)\n"
                "      [\n"
                "        {\n"
                "          \"event-id\": id, (numeric) The event id.\n"
                "          \"outcome\": typeId, (numeric) The outcome type id.\n"
                "          \"legResultType\": typeStr, (string) The string with leg result info.\n"
                "          \"lockedEvent\": (object) {\n"
                "            \"homeOdds\": homeOdds, (numeric) The moneyline odds to home team winning.\n"
                "            \"awayOdds\": awayOdds, (numeric) The moneyline odds to away team winning.\n"
                "            \"drawOdds\": drawOdds, (numeric) The moneyline odds to draw.\n"
                "            \"spreadPoints\": spreadPoints, (numeric) The spread points.\n"
                "            \"spreadHomeOdds\": spreadHomeOdds, (numeric) The spread odds to home team.\n"
                "            \"spreadAwayOdds\": spreadAwayOdds, (numeric) The spread odds to away team.\n"
                "            \"totalPoints\": totalPoints, (numeric) The total points.\n"
                "            \"totalOverOdds\": totalOverOdds, (numeric) The total odds to over.\n"
                "            \"totalUnderOdds\": totalUnderOdds, (numeric) The total odds to under.\n"
                "            \"starting\": starting, (numeric) The event start time in ms of Unix Timestamp.\n"
                "            \"home\": home command, (string) The home team name.\n"
                "            \"away\": away command, (string) The away team name.\n"
                "            \"tournament\": tournament, (string) The tournament name.\n"
                "            \"eventResultType\": type, (standard, event refund, ml refund, spreads refund, totals refund) The result type of finished event.\n"
                "            \"homeScore\": score, (numeric) The scores number of home team.\n"
                "            \"awayScore\": score, (numeric) The scores number of away team.\n"
                "          }\n"
                "        },\n"
                "        ...\n"
                "      ],                           (list) The list of legs.\n"
                "    \"address\": playerAddress,    (string) The player address.\n"
                "    \"amount\": x.xxx,             (numeric) The amount bet in WGR.\n"
                "    \"time\": \"betting time\",    (string) The betting time.\n"
                "    \"completed\": betIsCompleted, (bool), The bet status in chain.\n"
                "    \"betResultType\": type,       (lose/win/refund/pending), The info about bet result.\n"
                "    \"payout\": x.xxx,             (numeric) The bet payout.\n"
                "    \"payoutTxHash\": txHash,      (string) The hash of transaction wich store bet payout.\n"
                "    \"payoutTxOut\": nOut,        (numeric) The out number of transaction wich store bet payout.\n"
                "  },\n"
                "  ...\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getallbets", ""));

    uint32_t count = 10;
    if (params.size() >= 1)
        count = params[0].get_int();

    uint32_t from = 0;
    if (params.size() == 2)
        from = params[1].get_int();

    LOCK(cs_main);

    return GetBets(count, from, NULL, boost::optional<std::string>{}, false);
}


UniValue getmybets(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw std::runtime_error(
                "getmybets\n"
                "\nGet bets info for my wallets.\n"

                "\nArguments:\n"
                "1. account (string, optional) The account name. If not included, it will list all bets for all accounts. If \"\" is set, it will list transactions for the default account\n"
                "2. count (numeric, optional, default=10) Limit response to last bets number.\n"
                "3. from (numeric, optional, default=0) The number of bets to skip (from the last)\n"
                "4. includeWatchonly (bool, optional, default=false) Include bets to watchonly addresses\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"betBlockHeight\": height, (string) The hash of block wich store tx with bet opcode.\n"
                "    \"betTxHash\": txHash, (string) The hash of transaction wich store bet opcode.\n"
                "    \"betTxOut\": nOut, (numeric) The out number of transaction wich store bet opcode.\n"
                "    \"legs\": (array of objects)\n"
                "      [\n"
                "        {\n"
                "          \"event-id\": id, (numeric) The event id.\n"
                "          \"outcome\": typeId, (numeric) The outcome type id.\n"
                "          \"legResultType\": typeStr, (string) The string with leg result info.\n"
                "          \"lockedEvent\": (object) {\n"
                "            \"homeOdds\": homeOdds, (numeric) The moneyline odds to home team winning.\n"
                "            \"awayOdds\": awayOdds, (numeric) The moneyline odds to away team winning.\n"
                "            \"drawOdds\": drawOdds, (numeric) The moneyline odds to draw.\n"
                "            \"spreadPoints\": spreadPoints, (numeric) The spread points.\n"
                "            \"spreadHomeOdds\": spreadHomeOdds, (numeric) The spread odds to home team.\n"
                "            \"spreadAwayOdds\": spreadAwayOdds, (numeric) The spread odds to away team.\n"
                "            \"totalPoints\": totalPoints, (numeric) The total points.\n"
                "            \"totalOverOdds\": totalOverOdds, (numeric) The total odds to over.\n"
                "            \"totalUnderOdds\": totalUnderOdds, (numeric) The total odds to under.\n"
                "            \"starting\": starting, (numeric) The event start time in ms of Unix Timestamp.\n"
                "            \"home\": home command, (string) The home team name.\n"
                "            \"away\": away command, (string) The away team name.\n"
                "            \"tournament\": tournament, (string) The tournament name.\n"
                "            \"eventResultType\": type, (standard, event refund, ml refund, spreads refund, totals refund) The result type of finished event.\n"
                "            \"homeScore\": score, (numeric) The scores number of home team.\n"
                "            \"awayScore\": score, (numeric) The scores number of away team.\n"
                "          }\n"
                "        },\n"
                "        ...\n"
                "      ],                           (list) The list of legs.\n"
                "    \"address\": playerAddress,    (string) The player address.\n"
                "    \"amount\": x.xxx,             (numeric) The amount bet in WGR.\n"
                "    \"time\": \"betting time\",    (string) The betting time.\n"
                "    \"completed\": betIsCompleted, (bool), The bet status in chain.\n"
                "    \"betResultType\": type,       (lose/win/refund/pending), The info about bet result.\n"
                "    \"payout\": x.xxx,            (numeric) The bet payout.\n"
                "    \"payoutTxHash\": txHash,      (string) The hash of transaction wich store bet payout.\n"
                "    \"payoutTxOut\": nOut,        (numeric) The out number of transaction wich store bet payout.\n"
                "  },\n"
                "  ...\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmybets", ""));


    boost::optional<std::string> accountName = {};
    if (params.size() >= 1)
        accountName = params[0].get_str();

    uint32_t count = 10;
    if (params.size() >= 2)
        count = params[1].get_int();

    uint32_t from = 0;
    if (params.size() >= 3)
        from = params[2].get_int();

    bool includeWatchonly = false;
    if (params.size() == 4)
        includeWatchonly = params[3].get_bool();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return GetBets(count, from, pwalletMain, accountName, includeWatchonly);
}

void CollectQGBetData(UniValue &uValue, QuickGamesBetKey &key, CQuickGamesBetDB &qgBet, uint256 hash, bool requiredPayoutInfo = false) {

    uValue.push_back(Pair("type", "quickgame"));

    auto &gameView = Params().QuickGamesArr()[qgBet.gameType];

    uValue.push_back(Pair("blockHeight", (uint64_t) key.blockHeight));
    uValue.push_back(Pair("resultBlockHash", hash.ToString().c_str()));
    uValue.push_back(Pair("betTxHash", key.outPoint.hash.GetHex()));
    uValue.push_back(Pair("betTxOut", (uint64_t) key.outPoint.n));
    uValue.push_back(Pair("address", qgBet.playerAddress.ToString()));
    uValue.push_back(Pair("amount", ValueFromAmount(qgBet.betAmount)));
    uValue.push_back(Pair("time", (uint64_t) qgBet.betTime));
    uValue.push_back(Pair("gameName", gameView.name));
    UniValue betInfo{UniValue::VOBJ};
    for (auto val : gameView.betInfoParser(qgBet.vBetInfo, hash)) {
        betInfo.push_back(Pair(val.first, val.second));
    }
    uValue.push_back(Pair("betInfo", betInfo));
    uValue.push_back(Pair("completed", qgBet.IsCompleted() ? "yes" : "no"));
    uValue.push_back(Pair("betResultType", BetResultTypeToStr(qgBet.resultType)));
    uValue.push_back(Pair("payout", qgBet.IsCompleted() ? ValueFromAmount(qgBet.payout) : "pending"));

    if (requiredPayoutInfo) {
        if (qgBet.IsCompleted()) {
            auto it = bettingsView->payoutsInfo->NewIterator();
            // payoutHeight is next block height after block which contain bet
            uint32_t payoutHeight = key.blockHeight + 1;
            for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{payoutHeight, COutPoint{}})); it->Valid(); it->Next()) {
                PayoutInfoKey payoutKey;
                CPayoutInfoDB payoutInfo;
                CBettingDB::BytesToDbType(it->Key(), payoutKey);
                CBettingDB::BytesToDbType(it->Value(), payoutInfo);

                if (payoutHeight != payoutKey.blockHeight)
                    break;

                if (payoutInfo.betKey == key) {
                    uValue.push_back(Pair("payoutTxHash", payoutKey.outPoint.hash.GetHex()));
                    uValue.push_back(Pair("payoutTxOut", (uint64_t) payoutKey.outPoint.n));
                    break;
                }
            }
        }
        else {
            uValue.push_back(Pair("payoutTxHash", "pending"));
            uValue.push_back(Pair("payoutTxOut", "pending"));
        }
    }
}

UniValue GetQuickGamesBets(uint32_t count, uint32_t from, CWallet *_pwalletMain, boost::optional<std::string> accountName, bool includeWatchonly) {
    UniValue ret(UniValue::VARR);

    auto it = bettingsView->quickGamesBets->NewIterator();
    uint32_t skippedEntities = 0;
    for(it->SeekToLast(); it->Valid(); it->Prev()) {
        QuickGamesBetKey key;
        CQuickGamesBetDB qgBet;
        uint256 hash;
        CBettingDB::BytesToDbType(it->Value(), qgBet);
        CBettingDB::BytesToDbType(it->Key(), key);

        if (_pwalletMain) {
            CTxDestination dest = qgBet.playerAddress.Get();
            isminetype scriptType = IsMine(*_pwalletMain, dest);
            if (scriptType == ISMINE_NO)
                continue;
            if (scriptType == ISMINE_WATCH_ONLY && !includeWatchonly)
                continue;
            if (accountName && _pwalletMain->mapAddressBook.count(dest))
                if (_pwalletMain->mapAddressBook[dest].name != *accountName)
                    continue;

        }

        CBlockIndex *blockIndex = chainActive[(int) key.blockHeight];
        if (blockIndex)
            hash = blockIndex->hashProofOfStake;
        else
            hash = 0;

        UniValue bet{UniValue::VOBJ};

        CollectQGBetData(bet, key, qgBet, hash, true);

        if (skippedEntities == from) {
            ret.push_back(bet);
        } else {
            skippedEntities++;
        }

        if (count != 0 && ret.size() == count) {
            break;
        }
    }

    return ret;
}

UniValue getallqgbets(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw std::runtime_error(
                "getallqgbets\n"
                "\nGet quick games bets info for all wallets\n"

                "\nArguments:\n"
                "1. count (numeric, optional, default=10) Limit response to last bets number.\n"
                "2. from (numeric, optional, default=0) The number of bets to skip (from the last)\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"blockHeight\": height, (numeric) The block height where bet was placed.\n"
                "    \"resultBlockHash\": posHash, (string) The block hash where bet was placed. Also using for calc win number.\n"
                "    \"betTxHash\": hash, (string) The transaction hash where bet was placed.\n"
                "    \"betTxOut\": outPoint, (numeric) The transaction outpoint where bet was placed.\n"
                "    \"address\": playerAddress, (string) The player address.\n"
                "    \"amount\": x.xxx, (numeric) The amount bet in WGR.\n"
                "    \"time\": betTime, (string) The time of bet.\n"
                "    \"gameName\": name, (string) The game name on which bet has been placed.\n"
                "    \"betInfo\": info, (object) The bet info which collect specific infos about currect game params."
                "    \"completed\": yes/no, (string).\n"
                "    \"betResultType\": lose/win/refund/pending, (string).\n"
                "    \"payout\": x.xxx/pending, (numeric/string) The winning value.\n"
                "  },\n"
                "  ...\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getallqgbets", "15"));

    uint32_t count = 10;
    if (params.size()  >= 1)
        count = params[0].get_int();

    uint32_t from = 0;
    if (params.size()  == 2)
        from = params[1].get_int();

    LOCK(cs_main);

    return GetQuickGamesBets(count, from, NULL, boost::optional<std::string>{}, false);
}


UniValue getmyqgbets(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw std::runtime_error(
                "getmyqgbets\n"
                "\nGet quick games bets info for my wallets.\n"

                                "\nArguments:\n"
                "1. account (string, optional) The account name. If not included, it will list all bets for all accounts. If \"\" is set, it will list transactions for the default account\n"
                "2. count (numeric, optional, default=10) Limit response to last bets number.\n"
                "3. from (numeric, optional, default=0) The number of bets to skip (from the last)\n"
                "4. includeWatchonly (bool, optional, default=false) Include bets to watchonly addresses\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"blockHeight\": height, (numeric) The block height where bet was placed.\n"
                "    \"resultBlockHash\": posHash, (string) The block hash where bet was placed. Also using for calc win number.\n"
                "    \"betTxHash\": hash, (string) The transaction hash where bet was placed.\n"
                "    \"betTxOut\": outPoint, (numeric) The transaction outpoint where bet was placed.\n"
                "    \"address\": playerAddress, (string) The player address.\n"
                "    \"amount\": x.xxx, (numeric) The amount bet in WGR.\n"
                "    \"time\": betTime, (string) The time of bet.\n"
                "    \"gameName\": name, (string) The game name on which bet has been placed.\n"
                "    \"betInfo\": info, (object) The bet info which collect specific infos about currect game params."
                "    \"completed\": yes/no, (string).\n"
                "    \"betResultType\": lose/win/refund/pending, (string).\n"
                "    \"payout\": x.xxx/pending, (numeric/string) The winning value.\n"
                "  },\n"
                "  ...\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmyqgbets", "15"));

    boost::optional<std::string> accountName = {};
    if (params.size() >= 1)
        accountName = params[0].get_str();

    uint32_t count = 10;
    if (params.size()  >= 2)
        count = params[1].get_int();

    uint32_t from = 0;
    if (params.size() >= 3)
        from = params[2].get_int();

    bool includeWatchonly = false;
    if (params.size() == 4)
        includeWatchonly = params[3].get_bool();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return GetQuickGamesBets(count, from, pwalletMain, accountName, includeWatchonly);
}

UniValue getbetbytxid(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
                "getbetbytxid\n"
                "\nGet bet info by bet's txid.\n"

                "\nArguments:\n"
                "1. \"txid\" (string, required) Transaction ID wich has bet opcode in blockchain.\n"
                "\nResult: (array of objects)\n"
                "[\n"
                "  {\n"
                "    \"betBlockHeight\": height, (string) The hash of block wich store tx with bet opcode.\n"
                "    \"betTxHash\": txHash, (string) The hash of transaction wich store bet opcode.\n"
                "    \"betTxOut\": nOut, (numeric) The out number of transaction wich store bet opcode.\n"
                "    \"legs\": (array of objects)\n"
                "      [\n"
                "        {\n"
                "          \"event-id\": id, (numeric) The event id.\n"
                "          \"outcome\": typeId, (numeric) The outcome type id.\n"
                "          \"legResultType\": typeStr, (string) The string with leg result info.\n"
                "          \"lockedEvent\": (object) {\n"
                "            \"homeOdds\": homeOdds, (numeric) The moneyline odds to home team winning.\n"
                "            \"awayOdds\": awayOdds, (numeric) The moneyline odds to away team winning.\n"
                "            \"drawOdds\": drawOdds, (numeric) The moneyline odds to draw.\n"
                "            \"spreadPoints\": spreadPoints, (numeric) The spread points.\n"
                "            \"spreadHomeOdds\": spreadHomeOdds, (numeric) The spread odds to home team.\n"
                "            \"spreadAwayOdds\": spreadAwayOdds, (numeric) The spread odds to away team.\n"
                "            \"totalPoints\": totalPoints, (numeric) The total points.\n"
                "            \"totalOverOdds\": totalOverOdds, (numeric) The total odds to over.\n"
                "            \"totalUnderOdds\": totalUnderOdds, (numeric) The total odds to under.\n"
                "            \"starting\": starting, (numeric) The event start time in ms of Unix Timestamp.\n"
                "            \"home\": home command, (string) The home team name.\n"
                "            \"away\": away command, (string) The away team name.\n"
                "            \"tournament\": tournament, (string) The tournament name.\n"
                "            \"eventResultType\": type, (standard, event refund, ml refund, spreads refund, totals refund) The result type of finished event.\n"
                "            \"homeScore\": score, (numeric) The scores number of home team.\n"
                "            \"awayScore\": score, (numeric) The scores number of away team.\n"
                "          }\n"
                "        },\n"
                "        ...\n"
                "      ],                           (list) The list of legs.\n"
                "    \"address\": playerAddress,    (string) The player address.\n"
                "    \"amount\": x.xxx,             (numeric) The amount bet in WGR.\n"
                "    \"time\": \"betting time\",    (string) The betting time.\n"
                "    \"completed\": betIsCompleted, (bool), The bet status in chain.\n"
                "    \"betResultType\": type,       (lose/win/refund/pending), The info about bet result.\n"
                "    \"payout\": x.xxx,            (numeric) The bet payout.\n"
                "    \"payoutTxHash\": txHash,      (string) The hash of transaction wich store bet payout.\n"
                "    \"payoutTxOut\": nOut,        (numeric) The out number of transaction wich store bet payout.\n"
                "  },\n"
                "  ...\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getbetbytxid", "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"));

    uint256 txHash;
    txHash.SetHex(params[0].get_str());

    LOCK(cs_main);

    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(txHash, tx, hashBlock, true)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid bet's transaction id: " + params[0].get_str());
    }

    CBlockIndex* blockindex = mapBlockIndex[hashBlock];

    if (!blockindex) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Can't find bet's transaction id: " + params[0].get_str() + " in chain.");
    }

    UniValue ret{UniValue::VARR};

    {
        auto it = bettingsView->bets->NewIterator();
        for (it->Seek(CBettingDB::DbTypeToBytes(PeerlessBetKey{static_cast<uint32_t>(blockindex->nHeight), COutPoint{txHash, 0}})); it->Valid(); it->Next()) {
            PeerlessBetKey key;
            CPeerlessBetDB uniBet;
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            CBettingDB::BytesToDbType(it->Key(), key);

            if (key.outPoint.hash != txHash) break;

            UniValue uValue(UniValue::VOBJ);

            CollectPLBetData(uValue, key, uniBet, true);

            ret.push_back(uValue);
        }
    }
    {
        auto it = bettingsView->quickGamesBets->NewIterator();
        for (it->Seek(CBettingDB::DbTypeToBytes(PeerlessBetKey{static_cast<uint32_t>(blockindex->nHeight), COutPoint{txHash, 0}})); it->Valid(); it->Next()) {
            QuickGamesBetKey key;
            CQuickGamesBetDB qgBet;
            uint256 hash;
            CBettingDB::BytesToDbType(it->Key(), key);
            CBettingDB::BytesToDbType(it->Value(), qgBet);

            if (key.outPoint.hash != txHash) break;

            CBlockIndex *blockIndex = chainActive[(int) key.blockHeight];
            if (blockIndex)
                hash = blockIndex->hashProofOfStake;
            else
                hash = 0;

            UniValue uValue(UniValue::VOBJ);

            CollectQGBetData(uValue, key, qgBet, hash, true);

            ret.push_back(uValue);
        }
    }

    {
        auto it = bettingsView->fieldBets->NewIterator();
        for (it->Seek(CBettingDB::DbTypeToBytes(FieldBetKey{static_cast<uint32_t>(blockindex->nHeight), COutPoint{txHash, 0}})); it->Valid(); it->Next()) {
            FieldBetKey key;
            CFieldBetDB fBet;
            CBettingDB::BytesToDbType(it->Value(), fBet);
            CBettingDB::BytesToDbType(it->Key(), key);

            if (key.outPoint.hash != txHash) break;

            UniValue uValue(UniValue::VOBJ);

            CollectFieldBetData(uValue, key, fBet, true);

            ret.push_back(uValue);
        }
    }

    {
        auto it = bettingsView->hybridBets->NewIterator();
        for (it->Seek(CBettingDB::DbTypeToBytes(HybridBetKey{static_cast<uint32_t>(blockindex->nHeight), COutPoint{txHash, 0}})); it->Valid(); it->Next()) {
            HybridBetKey key;
            CHybridBetDB hBet;
            CBettingDB::BytesToDbType(it->Value(), hBet);
            CBettingDB::BytesToDbType(it->Key(), key);

            if (key.outPoint.hash != txHash) break;

            UniValue uValue(UniValue::VOBJ);

            CollectHybridBetData(uValue, key, hBet, true);

            ret.push_back(uValue);
        }
    }

    return ret;
}

UniValue listchaingamesbets(const UniValue& params, bool fHelp)
{
    // TODO The command-line parameters for this command aren't handled as.
    // described, either the documentation or the behaviour of this command
    // should be corrected when time allows.

    if (fHelp || params.size() > 4)
        throw std::runtime_error(
            "listchaingamebets ( \"account\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"    (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
            "                                     If \"\" is set, it will list transactions for the default account.\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"event-id\":\"accountname\",       (string) The ID of the event being bet on.\n"
            "    \"amount\": x.xxx,                  (numeric) The amount bet in WGR.\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 bets in the systems\n" +
            HelpExampleCli("listchaingamebets", ""));

    std::string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 3)
        if (params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const CWallet::TxItems & txOrdered = pwalletMain->wtxOrdered;

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;

        if (pwtx != 0) {

            uint256 txHash = (*pwtx).GetHash();

            for (unsigned int i = 0; i < (*pwtx).vout.size(); i++) {
                const CTxOut& txout = (*pwtx).vout[i];

                auto cgBettingTx = ParseBettingTx(txout);

                if (cgBettingTx == nullptr) continue;

                if (cgBettingTx->GetTxType() == cgBetTxType) {
                    CChainGamesBetTx* cgBet = (CChainGamesBetTx*) cgBettingTx.get();
                    UniValue entry(UniValue::VOBJ);
                    entry.push_back(Pair("tx-id", txHash.ToString().c_str()));
                    entry.push_back(Pair("event-id", (uint64_t) cgBet->nEventId));
                    entry.push_back(Pair("amount", ValueFromAmount(txout.nValue)));
                    ret.push_back(entry);
                }
            }
        }

        if ((int)ret.size() >= (nCount + nFrom)) break;
    }

    // ret is newest to oldest
    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    std::vector<UniValue> arrTmp = ret.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

std::string HelpRequiringPassphrase()
{
    return pwalletMain && pwalletMain->IsCrypted() ? "\nRequires wallet passphrase to be set with walletpassphrase call." : "";
}

void EnsureWalletIsUnlocked(bool fAllowAnonOnly)
{
    if (pwalletMain->IsLocked() || (!fAllowAnonOnly && pwalletMain->fWalletUnlockAnonymizeOnly))
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

void EnsureEnoughWagerr(CAmount total)
{

    CAmount nBalance = pwalletMain->GetBalance();

    if (total > nBalance) {
         throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Error: Not enough funds in wallet or account");
    }
}

void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry)
{
    int confirms = wtx.GetDepthInMainChain(false);
    int confirmsTotal = GetIXConfirmations(wtx.GetHash()) + confirms;
    entry.push_back(Pair("confirmations", confirmsTotal));
    entry.push_back(Pair("bcconfirmations", confirms));
    if (wtx.IsCoinBase() || wtx.IsCoinStake())
        entry.push_back(Pair("generated", true));
    if (confirms > 0) {
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        entry.push_back(Pair("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime()));
    } else {
        entry.push_back(Pair("trusted", wtx.IsTrusted()));
    }
    uint256 hash = wtx.GetHash();
    entry.push_back(Pair("txid", hash.GetHex()));
    UniValue conflicts(UniValue::VARR);
    for (const uint256& conflict : wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.push_back(Pair("walletconflicts", conflicts));
    entry.push_back(Pair("time", wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));
    for (const PAIRTYPE(std::string, std::string) & item : wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

std::string AccountFromValue(const UniValue& value)
{
    std::string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

UniValue getnewaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "getnewaddress ( \"account\" )\n"
            "\nReturns a new WAGERR address for receiving payments.\n"
            "If 'account' is specified (recommended), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"

            "\nArguments:\n"
            "1. \"account\"        (string, optional) The account name for the address to be linked to. if not provided, the default account \"\" is used. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.\n"

            "\nResult:\n"
            "\"wagerraddress\"    (string) The new wagerr address\n"

            "\nExamples:\n" +
            HelpExampleCli("getnewaddress", "") + HelpExampleCli("getnewaddress", "\"\"") +
            HelpExampleCli("getnewaddress", "\"myaccount\"") + HelpExampleRpc("getnewaddress", "\"myaccount\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    std::string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    return CBitcoinAddress(keyID).ToString();
}


CBitcoinAddress GetAccountAddress(std::string strAccount, bool bForceNew = false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid()) {
        CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
        for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it) {
            const CWalletTx& wtx = (*it).second;
            for (const CTxOut& txout : wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed) {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, "receive");
        walletdb.WriteAccount(strAccount, account);
    }

    return CBitcoinAddress(account.vchPubKey.GetID());
}

UniValue getaccountaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getaccountaddress \"account\"\n"
            "\nReturns the current WAGERR address for receiving payments to this account.\n"

            "\nArguments:\n"


            "\nResult:\n"
            "\"wagerraddress\"   (string) The account wagerr address\n"

            "\nExamples:\n" +
            HelpExampleCli("getaccountaddress", "") + HelpExampleCli("getaccountaddress", "\"\"") +
            HelpExampleCli("getaccountaddress", "\"myaccount\"") + HelpExampleRpc("getaccountaddress", "\"myaccount\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    std::string strAccount = AccountFromValue(params[0]);

    UniValue ret(UniValue::VSTR);

    ret = GetAccountAddress(strAccount).ToString();
    return ret;
}


UniValue getrawchangeaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "getrawchangeaddress\n"
            "\nReturns a new WAGERR address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"

            "\nResult:\n"
            "\"address\"    (string) The address\n"

            "\nExamples:\n" +
            HelpExampleCli("getrawchangeaddress", "") + HelpExampleRpc("getrawchangeaddress", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    return CBitcoinAddress(keyID).ToString();
}


UniValue setaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "setaccount \"wagerraddress\" \"account\"\n"
            "\nSets the account associated with the given address.\n"

            "\nArguments:\n"
            "1. \"wagerraddress\"  (string, required) The wagerr address to be associated with an account.\n"
            "2. \"account\"         (string, required) The account to assign the address to.\n"

            "\nExamples:\n" +
            HelpExampleCli("setaccount", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" \"tabby\"") + HelpExampleRpc("setaccount", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\", \"tabby\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");


    std::string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Only add the account if the address is yours.
    if (IsMine(*pwalletMain, address.Get())) {
        // Detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwalletMain->mapAddressBook.count(address.Get())) {
            std::string strOldAccount = pwalletMain->mapAddressBook[address.Get()].name;
            if (address == GetAccountAddress(strOldAccount))
                GetAccountAddress(strOldAccount, true);
        }
        pwalletMain->SetAddressBook(address.Get(), strAccount, "receive");
    } else
        throw JSONRPCError(RPC_MISC_ERROR, "setaccount can only be used with own address");

    return NullUniValue;
}


UniValue getaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getaccount \"wagerraddress\"\n"
            "\nReturns the account associated with the given address.\n"

            "\nArguments:\n"
            "1. \"wagerraddress\"  (string, required) The wagerr address for account lookup.\n"

            "\nResult:\n"
            "\"accountname\"        (string) the account address\n"

            "\nExamples:\n" +
            HelpExampleCli("getaccount", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\"") + HelpExampleRpc("getaccount", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");

    std::string strAccount;
    std::map<CTxDestination, CAddressBookData>::iterator mi = pwalletMain->mapAddressBook.find(address.Get());
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.name.empty())
        strAccount = (*mi).second.name;
    return strAccount;
}


UniValue getaddressesbyaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getaddressesbyaccount \"account\"\n"
            "\nReturns the list of addresses for the given account.\n"

            "\nArguments:\n"
            "1. \"account\"  (string, required) The account name.\n"

            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"wagerraddress\"  (string) a wagerr address associated with the given account\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getaddressesbyaccount", "\"tabby\"") + HelpExampleRpc("getaddressesbyaccount", "\"tabby\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    UniValue ret(UniValue::VARR);
    for (const PAIRTYPE(CBitcoinAddress, CAddressBookData) & item : pwalletMain->mapAddressBook) {
        const CBitcoinAddress& address = item.first;
        const std::string& strName = item.second.name;
        if (strName == strAccount)
            ret.push_back(address.ToString());
    }
    return ret;
}

void SendMoney(const CTxDestination& address, CAmount nValue, CWalletTx& wtxNew, bool fUseIX = false, const std::string& opReturn = "")
{
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    std::string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse WAGERR address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    if (!pwalletMain->CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired, strError, NULL, ALL_COINS, fUseIX, (CAmount)0, opReturn)) {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey, (!fUseIX ? "tx" : "ix")))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}

UniValue placebet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw std::runtime_error(
            "placebet \"event-id\" outcome amount ( \"comment\" \"comment-to\" )\n"
            "\nWARNING - Betting closes 20 minutes before event start time.\n"
            "Any bets placed after this time will be invalid and will not be paid out! \n"
            "\nPlace an amount as a bet on an event. The amount is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"event-id\"    (numeric, required) The event to bet on.\n"
            "2. outcome         (numeric, required) 1 means home team win,\n"
            "                                       2 means away team win,\n"
            "                                       3 means a draw."
            "3. amount          (numeric, required) The amount in wgr to send.\n"
            "4. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "5. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n" +
            HelpExampleCli("placebet", "\"000\" \"1\" 25\"donation\" \"seans outpost\"") +
            HelpExampleRpc("placebet", "\"000\", \"1\", 25, \"donation\", \"seans outpost\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CAmount nAmount = AmountFromValue(params[2]);

    // Validate bet amount so its between 25 - 10000 WGR inclusive.
    if (nAmount < (Params().MinBetPayoutRange()  * COIN ) || nAmount > (Params().MaxBetPayoutRange() * COIN)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.");
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();
    if (params.size() > 4 && !params[4].isNull() && !params[4].get_str().empty())
        wtx.mapValue["to"] = params[4].get_str();

    EnsureWalletIsUnlocked();
    EnsureEnoughWagerr(nAmount);

    CBitcoinAddress address("");
    uint32_t eventId = static_cast<uint32_t>(params[0].get_int64());
    uint8_t outcome = (uint8_t) params[1].get_int();

    if (outcome < moneyLineHomeWin || outcome > totalUnder)
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet outcome type.");

    CPeerlessExtendedEventDB plEvent;
    if (!bettingsView->events->Read(EventKey{eventId}, plEvent)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: there is no such Event: " + std::to_string(eventId));
    }

    if (GetBetPotentialOdds(CPeerlessLegDB{eventId, (PeerlessBetOutcomeType)outcome}, plEvent) == 0) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: potential odds is zero for event: " + std::to_string(eventId) + " outcome: " + std::to_string(outcome));
    }

    CPeerlessBetTx plBet(eventId, outcome);

    // TODO `address` isn't used when adding the following transaction to the
    // blockchain, so ideally it would not need to be supplied to `SendMoney`.
    // Ideally an alternative function, such as `BurnMoney`, would be developed
    // and used, which would take the `OP_RETURN` value in place of the address
    // value.
    // Note that, during testing, the `opReturn` value is added to the
    // blockchain incorrectly if its length is less than 5. This behaviour would
    // ideally be investigated and corrected/justified when time allows.
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << (uint8_t) BTX_PREFIX << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) plBetTxType << plBet;
    std::string opCode = HexStr(ss.begin(), ss.end());


    // Unhex the validated bet opcode
    std::vector<unsigned char> vectorValue;
    std::string stringValue(opCode);
    boost::algorithm::unhex(stringValue, back_inserter(vectorValue));
    std::string unHexedOpCode(vectorValue.begin(), vectorValue.end());

    SendMoney(address.Get(), nAmount, wtx, false, unHexedOpCode);

    return wtx.GetHash().GetHex();
}

UniValue placeparlaybet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw std::runtime_error(
            "placeparlaybet [{\"eventId\": event_id, \"outcome\": outcome_type}, ...] ( \"comment\" \"comment-to\" )\n"
            "\nWARNING - Betting closes 20 minutes before event start time.\n"
            "Any bets placed after this time will be invalid and will not be paid out! \n"
            "\nPlace an amount as a bet on an event. The amount is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. Legs array     (array of json objects, required) The list of bets.\n"
            "  [\n"
            "    {\n"
            "      \"eventId\": id      (numeric, required) The event to bet on.\n"
            "      \"outcome\": type    (numeric, required) 1 - home win, 2 - away win, 3 - draw,\n"
            "                                               4 - spread home, 5 - spread away,\n"
            "                                               6 - total over, 7 - total under\n"
            "    }\n"
            "  ]\n"
            "2. amount          (numeric, required) The amount in wgr to send. Min: 25, max: 4000.\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for.\n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization\n"
            "                             to which you're sending the transaction. This is not part of the\n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n" +
            HelpExampleCli("placeparlaybet", "\"[{\"eventId\": 228, \"outcome\": 1}, {\"eventId\": 322, \"outcome\": 2}]\" 25 \"Parlay bet\" \"seans outpost\"") +
            HelpExampleRpc("placeparlaybet", "\"[{\"eventId\": 228, \"outcome\": 1}, {\"eventId\": 322, \"outcome\": 2}]\", 25, \"Parlay bet\", \"seans outpost\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CPeerlessParlayBetTx parlayBetTx;
    UniValue legsArr = params[0].get_array();
    for (uint32_t i = 0; i < legsArr.size(); i++) {
        const UniValue obj = legsArr[i].get_obj();

        CPeerlessBetTx pBetTx = CheckAndGetPeerlessLegObj(obj);

        parlayBetTx.legs.emplace_back(pBetTx);
    }

    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << (uint8_t) BTX_PREFIX << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) plParlayBetTxType << parlayBetTx;
    std::string opCode = HexStr(ss.begin(), ss.end());

    CAmount nAmount = AmountFromValue(params[1]);

    // Validate parlay bet amount so its between 25 - 4000 WGR inclusive.
    if (nAmount < (Params().MinBetPayoutRange()  * COIN ) || nAmount > (Params().MaxParlayBetPayoutRange() * COIN)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet amount. Please ensure your bet is between 25 - 4000 WGR inclusive.");
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"] = params[3].get_str();

    EnsureWalletIsUnlocked();
    EnsureEnoughWagerr(nAmount);

    CBitcoinAddress address("");

    // Unhex the validated bet opcode
    std::vector<unsigned char> vectorValue;
    std::string stringValue(opCode);
    boost::algorithm::unhex(stringValue, back_inserter(vectorValue));
    std::string unHexedOpCode(vectorValue.begin(), vectorValue.end());

    SendMoney(address.Get(), nAmount, wtx, false, unHexedOpCode);

    return wtx.GetHash().GetHex();
}

UniValue placefieldbet(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() < 4 || params.size() > 6) {
        throw std::runtime_error(
            "placefieldbet event_id market_type contender_id amount ( \"comment\" \"comment-to\" )\n"
            "\nWARNING - Betting closes 20 minutes before field event start time.\n"
            "Any bets placed after this time will be invalid and will not be paid out! \n"
            "\nPlace an amount as a bet on an field event. The amount is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. event_id        (numeric, required) The field event id to bet on.\n"
            "2. market_type     (numeric, required) 1 means outright,\n"
            "                                       2 means place,\n"
            "                                       3 means show."
            "3. contender_id    (numeric, required) The field event participant identifier."
            "4. amount          (numeric, required) The amount in wgr to send.\n"
            "5. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n" +
            HelpExampleCli("placefieldbet", "1 1 100 25 \"donation\" \"seans outpost\"") +
            HelpExampleRpc("placefieldbet", "1 1 100 25 \"donation\" \"seans outpost\""));
    }

    if (chainActive.Height() < Params().WagerrProtocolV4StartHeight()) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: placefieldbet deactived for now");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CAmount nAmount = AmountFromValue(params[3]);
    // Validate bet amount so its between 25 - 10000 WGR inclusive.
    if (nAmount < (Params().MinBetPayoutRange()  * COIN ) || nAmount > (Params().MaxBetPayoutRange() * COIN)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.");
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 4 && !params[4].isNull() && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && !params[5].isNull() && !params[5].get_str().empty())
        wtx.mapValue["to"] = params[5].get_str();

    EnsureWalletIsUnlocked();
    EnsureEnoughWagerr(nAmount);

    uint32_t eventId = static_cast<uint32_t>(params[0].get_int64());
    FieldBetOutcomeType marketType = static_cast<FieldBetOutcomeType>(params[1].get_int());
    uint32_t contenderId = static_cast<uint32_t>(params[2].get_int64());

    if (marketType < outright || marketType > show) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet market type for FieldEvent: " + std::to_string(eventId));
    }

    CFieldEventDB fEvent;
    if (!bettingsView->fieldEvents->Read(FieldEventKey{eventId}, fEvent)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: there is no such FieldEvent: " + std::to_string(eventId));
    }

    if (!fEvent.IsMarketOpen(marketType)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: market " + std::to_string((uint8_t)marketType) + " is closed for event " + std::to_string(eventId));
    }


    const auto& contender_it = fEvent.contenders.find(contenderId);
    if (contender_it == fEvent.contenders.end()) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: there is no such contenderId " + std::to_string(contenderId) + " in event " + std::to_string(eventId));
    }

    if (bettingsView->fieldResults->Exists(FieldResultKey{eventId})) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: FieldEvent " + std::to_string(eventId) + " was been resulted");
    }

    if (GetBetPotentialOdds(CFieldLegDB{eventId, marketType, contenderId}, fEvent) == 0) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: contender odds is zero for event: " + std::to_string(eventId) + " contenderId: " + std::to_string(contenderId));
    }

    CFieldBetTx fBetTx{eventId, (uint8_t) marketType, contenderId};

    // TODO `address` isn't used when adding the following transaction to the
    // blockchain, so ideally it would not need to be supplied to `SendMoney`.
    // Ideally an alternative function, such as `BurnMoney`, would be developed
    // and used, which would take the `OP_RETURN` value in place of the address
    // value.
    // Note that, during testing, the `opReturn` value is added to the
    // blockchain incorrectly if its length is less than 5. This behaviour would
    // ideally be investigated and corrected/justified when time allows.
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << (uint8_t) BTX_PREFIX << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) fBetTxType << fBetTx;
    std::string opCode = HexStr(ss.begin(), ss.end());

    CBitcoinAddress address("");
    // Unhex the validated bet opcode
    std::vector<unsigned char> vectorValue;
    std::string stringValue(opCode);
    boost::algorithm::unhex(stringValue, back_inserter(vectorValue));
    std::string unHexedOpCode(vectorValue.begin(), vectorValue.end());

    SendMoney(address.Get(), nAmount, wtx, false, unHexedOpCode);

    return wtx.GetHash().GetHex();
}

UniValue placefieldparlaybet(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() < 2 || params.size() > 4) {
        throw std::runtime_error(
            "placefieldparlaybet [{\"eventId\": event_id, \"marketType\": market_type, \"contenderId\": contender_id}, ...] amount ( \"comment\" \"comment-to\" )\n"
            "\nWARNING - Betting closes 20 minutes before field event start time.\n"
            "Any bets placed after this time will be invalid and will not be paid out! \n"
            "\nPlace an amount as a bet on an field event. The amount is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. Legs array     (array of json objects, required) The list of field bets.\n"
            "  [\n"
            "    {\n"
            "      \"eventId\": id               (numeric, required) The field event id to bet on.\n"
            "      \"marketType\": market_type   (numeric, required) 1 means outright,\n"
            "                                                        2 means place,\n"
            "                                                        3 means show."
            "      \"contenderId\": contender_id (numeric, required) The field event participant identifier."
            "    }\n"
            "  ]\n"
            "2. amount          (numeric, required) The amount in wgr to send. Min: 25, max: 4000.\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for.\n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization\n"
            "                             to which you're sending the transaction. This is not part of the\n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n" +
            HelpExampleCli("placefieldparlaybet", "\"[{\"eventId\": 1, \"marketType\": 1, \"contenderId\": 100}, {\"eventId\": 2, \"marketType\": 2, \"contenderId\": 200}]\" 25 \"Parlay bet\" \"seans outpost\"") +
            HelpExampleRpc("placefieldparlaybet", "\"[{\"eventId\": 1, \"marketType\": 1, \"contenderId\": 100}, {\"eventId\": 322,\"marketType\": 2, \"contenderId\": 200}]\", 25, \"Parlay bet\", \"seans outpost\""));
    }

    if (chainActive.Height() < Params().WagerrProtocolV4StartHeight()) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: placefieldparlaybet deactived for now");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CAmount nAmount = AmountFromValue(params[1]);
    // Validate bet amount so its between 25 - 10000 WGR inclusive.
    if (nAmount < (Params().MinBetPayoutRange()  * COIN ) || nAmount > (Params().MaxBetPayoutRange() * COIN)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.");
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"] = params[3].get_str();

    EnsureWalletIsUnlocked();
    EnsureEnoughWagerr(nAmount);

    CFieldParlayBetTx fParlayBetTx;
    UniValue legsArr = params[0].get_array();
    for (uint32_t i = 0; i < legsArr.size(); i++) {
        const UniValue obj = legsArr[i].get_obj();

        CFieldBetTx fBetTx = CheckAndGetFieldLegObj(obj);

        fParlayBetTx.legs.emplace_back(fBetTx);
    }

    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << (uint8_t) BTX_PREFIX << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) fParlayBetTxType << fParlayBetTx;
    std::string opCode = HexStr(ss.begin(), ss.end());

    CBitcoinAddress address("");

    // Unhex the validated bet opcode
    std::vector<unsigned char> vectorValue;
    std::string stringValue(opCode);
    boost::algorithm::unhex(stringValue, back_inserter(vectorValue));
    std::string unHexedOpCode(vectorValue.begin(), vectorValue.end());

    SendMoney(address.Get(), nAmount, wtx, false, unHexedOpCode);

    return wtx.GetHash().GetHex();
}

UniValue placehybridparlaybet(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() < 2 || params.size() > 4) {
        throw std::runtime_error(
            "placehybridparlaybet\n"
            "\nWARNING - Betting closes 20 minutes before field event start time.\n"
            "Any bets placed after this time will be invalid and will not be paid out! \n"
            "\nPlace an amount as a bet on an field event. The amount is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() +
            "Arguments:\n"
            "1. Hybrid legs array (array of json objects, required) The list of field bets.\n"
            "[\n"
            "    {\n"
            "        legType: \"type\" (string, required) The leg type, can be \"peerless\" or \"field\";\n"
            "        leg: legObj (object, required) The leg object. Can be a PeerlessLegObject or FieldLegObject.\n"
            "    }\n"
            "].\n"
            "The PeerlessLegObject must have following structure:\n"
            "{\n"
            "    eventId: id (numeric, required) The peerless event id to bet on;\n"
            "    outcome: outcomeType (numeric, required) 1 - home win, 2 - away win, 3 - draw,\n"
            "                                                4 - spread home, 5 - spread away,\n"
            "                                                6 - total over, 7 - total under;\n"
            "}.\n"
            "The FieldLegObject must have following structure:\n"
            "{\n"
            "    eventId: id (numeric, required) The field event id to bet on;\n"
            "    marketType: market_type (numeric, required) 1 means outright, 2 means place, 3 means show;\n"
            "    contenderId: contender_id (numeric, required) The field event participant identifier;\n"
            "}.\n"
            "2. amount          (numeric, required) The amount in wgr to send. Min: 25, max: 4000.\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for.\n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization\n"
            "                             to which you're sending the transaction. This is not part of the\n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n" +
            HelpExampleCli("placehybridparlaybet", "\"[{ legType: \"field\", leg: {\"eventId\": 1, \"marketType\": 1, \"contenderId\": 100}}, {legType: \"peerless\" , leg: {\"eventId\": 2, \"marketType\": 2}}]\" 25 \"Hybrid parlay bet\" \"seans outpost\"") +
            HelpExampleRpc("placehybridparlaybet", "\"[{ legType: \"field\", leg: {\"eventId\": 1, \"marketType\": 1, \"contenderId\": 100}}, {legType: \"peerless\" , leg: {\"eventId\": 2, \"marketType\": 2}}]\" 25 \"Hybrid parlay bet\" \"seans outpost\""));
    }

    if (chainActive.Height() < Params().WagerrProtocolV4StartHeight()) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: placehybridparlaybet deactived for now");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CAmount nAmount = AmountFromValue(params[1]);
    // Validate bet amount so its between 25 - 10000 WGR inclusive.
    if (nAmount < (Params().MinBetPayoutRange()  * COIN ) || nAmount > (Params().MaxBetPayoutRange() * COIN)) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.");
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"] = params[3].get_str();

    EnsureWalletIsUnlocked();
    EnsureEnoughWagerr(nAmount);

    CHybridParlayBetTx hParlayBetTx;
    UniValue legsArr = params[0].get_array();
    for (uint32_t i = 0; i < legsArr.size(); i++) {
        const UniValue obj = legsArr[i].get_obj();

        std::string legType = find_value(obj, "legType").get_str();

        if (legType == "peerless") {

            const UniValue legObj = find_value(obj, "legObj").get_obj();

            CPeerlessBetTx pBetTx = CheckAndGetPeerlessLegObj(legObj);

            hParlayBetTx.legs.emplace_back(HybridBetTxVariant{pBetTx});
        }
        else if (legType == "field") {
            const UniValue legObj = find_value(obj, "legObj").get_obj();

            CFieldBetTx fBetTx = CheckAndGetFieldLegObj(legObj);

            hParlayBetTx.legs.emplace_back(HybridBetTxVariant{fBetTx});
        }
        else {
            throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Invalid leg type: " + legType);
        }
    }

    if (hParlayBetTx.legs.size() < 2 || hParlayBetTx.legs.size() > Params().MaxParlayLegs()) {
        throw JSONRPCError(RPC_BET_DETAILS_ERROR, "Error: Incorrect legs count.");
    }

    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << (uint8_t) BTX_PREFIX << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) hParlayBetTxType << hParlayBetTx;
    std::string opCode = HexStr(ss.begin(), ss.end());

    CBitcoinAddress address("");

    // Unhex the validated bet opcode
    std::vector<unsigned char> vectorValue;
    std::string stringValue(opCode);
    boost::algorithm::unhex(stringValue, back_inserter(vectorValue));
    std::string unHexedOpCode(vectorValue.begin(), vectorValue.end());

    SendMoney(address.Get(), nAmount, wtx, false, unHexedOpCode);

    return wtx.GetHash().GetHex();
}

/**
 * Looks up a chain game info for a given ID.
 *
 * @param params The RPC params consisting of the event id.
 * @param fHelp  Help text
 * @return
 */
UniValue getchaingamesinfo(const UniValue& params, bool fHelp)
{
   if (fHelp || params.size() > 2)
        throw std::runtime_error(
            "getchaingamesinfo ( \"eventID\" showWinner )\n"

            "\nArguments:\n"
            "1. eventID          (numeric) The event ID.\n"
            "2. showWinner       (bool, optional, default=false) Include a scan for the winner.\n");

    LOCK(cs_main);

    UniValue ret(UniValue::VARR);
    UniValue obj(UniValue::VOBJ);

    // Set default return values
    unsigned int eventID = params[0].get_int();
    int entryFee = 0;
    int totalFoundCGBets = 0;
    int gameStartTime = 0;
    int gameStartBlock = 0;
    int resultHeight = -1;
    CBetOut winningBetOut;
    bool winningBetFound = false;

    bool fShowWinner = false;
    if (params.size() > 1) {
        fShowWinner = params[1].get_bool();
    }

    CBlockIndex *BlocksIndex = NULL;
    int height = (Params().NetworkID() == CBaseChainParams::MAIN) ? chainActive.Height() - 10500 : chainActive.Height() - 14400;
    BlocksIndex = chainActive[height];

    while (BlocksIndex) {
        CBlock block;
        ReadBlockFromDisk(block, BlocksIndex);

        for (CTransaction& tx : block.vtx) {

            const CTxIn &txin = tx.vin[0];
            bool validTx = IsValidOracleTx(txin, height);

            // Check each TX out for values
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                const CTxOut &txout = tx.vout[i];

                auto cgBettingTx = ParseBettingTx(txout);

                if(cgBettingTx == nullptr) continue;

                auto txType = cgBettingTx->GetTxType();

                // Find any CChainGameEvents matching the specified id
                if (validTx && txType == cgEventTxType) {
                    CChainGamesEventTx* cgEvent = (CChainGamesEventTx*) cgBettingTx.get();
                    if (((unsigned int)cgEvent->nEventId) == eventID){
                        entryFee = cgEvent->nEntryFee;
                        gameStartTime = block.GetBlockTime();
                        gameStartBlock = BlocksIndex -> nHeight;
                    }
                }
                // Find a matching result transaction
                if (validTx && resultHeight == -1 && txType == cgResultTxType) {
                    CChainGamesResultTx* cgResult = (CChainGamesResultTx*) cgBettingTx.get();
                    if (cgResult->nEventId == (uint16_t)eventID) {
                        resultHeight = BlocksIndex->nHeight;
                    }
                }
                if (txType == cgBetTxType) {
                    CChainGamesBetTx* cgBet = (CChainGamesBetTx*) cgBettingTx.get();
                    if (((unsigned int)cgBet->nEventId) == eventID){
                        totalFoundCGBets = totalFoundCGBets + 1;
                    }
                }
            }
        }

        BlocksIndex = chainActive.Next(BlocksIndex);
    }

    if (resultHeight > Params().WagerrProtocolV2StartHeight() && fShowWinner) {
        std::vector<CBetOut> vExpectedCGLottoPayouts;
        std::vector<CPayoutInfoDB> vPayoutsInfo;
        GetCGLottoBetPayoutsV2(resultHeight, vExpectedCGLottoPayouts, vPayoutsInfo);
        for (auto lottoPayouts : vExpectedCGLottoPayouts) {
            if (!winningBetFound && lottoPayouts.nEventId == eventID) {
                winningBetOut = lottoPayouts;
                winningBetFound = true;
            }
        }
    }

    int potSize = totalFoundCGBets*entryFee;

    obj.push_back(Pair("pot-size", potSize));
    obj.push_back(Pair("entry-fee", entryFee));
    obj.push_back(Pair("start-block", gameStartBlock));
    obj.push_back(Pair("start-time", gameStartTime));
    obj.push_back(Pair("total-bets", totalFoundCGBets));
    obj.push_back(Pair("result-trigger-block", resultHeight));
    if (winningBetFound) {
        CTxDestination address;
        if (ExtractDestination(winningBetOut.scriptPubKey, address)) {
            obj.push_back(Pair("winner", CBitcoinAddress(address).ToString()));
            obj.push_back(Pair("winnings", ValueFromAmount(winningBetOut.nValue)));
        }

    }
    obj.push_back(Pair("network", Params().NetworkIDString()));

    return obj;
}

/**
 * Get total liability for each event that is currently active.
 *
 * @param params The RPC params consisting of the event id.
 * @param fHelp  Help text
 * @return
 */
UniValue getalleventliabilities(const UniValue& params, bool fHelp)
{
  if (fHelp || (params.size() != 0))
        throw std::runtime_error(
            "geteventliability\n"
            "Return the payout liabilities for all events.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"event-id\": \"xxx\", (numeric) The id of the event.\n"
            "    \"event-status\": \"status\", (string) The status of the event (running | resulted).\n"
            "    \"moneyline-home-bets\": \"xxx\", (numeric) The number of bets to moneyline home (parlays included).\n"
            "    \"moneyline-home-liability\": \"xxx\", (numeric) The moneyline home potentional liability (without parlays).\n"
            "    \"moneyline-away-bets\": \"xxx\", (numeric) The number of bets to moneyline away (parlays included).\n"
            "    \"moneyline-away-liability\": \"xxx\", (numeric) The moneyline away potentional liability (without parlays).\n"
            "    \"moneyline-draw-bets\": \"xxx\", (numeric) The number of bets to moneyline draw (parlays included).\n"
            "    \"moneyline-draw-liability\": \"xxx\", (numeric) The moneyline draw potentional liability (without parlays).\n"
            "    \"spread-home-bets\": \"xxx\", (numeric) The number of bets to spread home (parlays included).\n"
            "    \"spread-home-liability\": \"xxx\", (numeric) The spreads home potentional liability (without parlays).\n"
            "    \"spread-away-bets\": \"xxx\", (numeric) The number of bets to spread away (parlays included).\n"
            "    \"spread-away-liability\": \"xxx\", (numeric) The spread away potentional liability (without parlays).\n"
            "    \"spread-push-bets\": \"xxx\", (numeric) The number of bets to spread push (parlays included).\n"
            "    \"spread-push-liability\": \"xxx\", (numeric) The spread push potentional liability (without parlays).\n"
            "    \"total-over-bets\": \"xxx\", (numeric) The number of bets to total over (parlays included).\n"
            "    \"total-over-liability\": \"xxx\", (numeric) The total over potentional liability (without parlays).\n"
            "    \"total-under-bets\": \"xxx\", (numeric) The number of bets to total under (parlays included).\n"
            "    \"total-under-liability\": \"xxx\", (numeric) The total under potentional liability (without parlays).\n"
            "    \"total-push-bets\": \"xxx\", (numeric) The number of bets to total push (parlays included).\n"
            "    \"total-push-liability\": \"xxx\", (numeric) The total push potentional liability (without parlays).\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getalleventliabilities", "") + HelpExampleRpc("getalleventliabilities", ""));

    LOCK(cs_main);

    UniValue result{UniValue::VARR};

    auto it = bettingsView->events->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CPeerlessExtendedEventDB plEvent;
        CBettingDB::BytesToDbType(it->Value(), plEvent);

        // Only list active events.
        /*
        if (plEvent.nEventCreationHeight < chainActive.Height() - Params().BetBlocksIndexTimespan()) {
            continue;
        }
        */
        // Only list active events.
        if ((time_t) plEvent.nStartTime < std::time(0)) {
            continue;
        }

        UniValue event(UniValue::VOBJ);

        event.push_back(Pair("event-id", (uint64_t) plEvent.nEventId));
        event.push_back(Pair("event-status", "running"));
        event.push_back(Pair("moneyline-home-bets", (uint64_t) plEvent.nMoneyLineHomeBets));
        event.push_back(Pair("moneyline-home-liability", (uint64_t) plEvent.nMoneyLineHomePotentialLiability));
        event.push_back(Pair("moneyline-away-bets", (uint64_t) plEvent.nMoneyLineAwayBets));
        event.push_back(Pair("moneyline-away-liability", (uint64_t) plEvent.nMoneyLineAwayPotentialLiability));
        event.push_back(Pair("moneyline-draw-bets", (uint64_t) plEvent.nMoneyLineDrawBets));
        event.push_back(Pair("moneyline-draw-liability", (uint64_t) plEvent.nMoneyLineDrawPotentialLiability));
        event.push_back(Pair("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets));
        event.push_back(Pair("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability));
        event.push_back(Pair("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets));
        event.push_back(Pair("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability));
        event.push_back(Pair("spread-away-bets", (uint64_t) plEvent.nSpreadAwayBets));
        event.push_back(Pair("spread-away-liability", (uint64_t) plEvent.nSpreadAwayPotentialLiability));
        event.push_back(Pair("spread-push-bets", (uint64_t) plEvent.nSpreadPushBets));
        event.push_back(Pair("spread-push-liability", (uint64_t) plEvent.nSpreadPushPotentialLiability));
        event.push_back(Pair("total-over-bets", (uint64_t) plEvent.nTotalOverBets));
        event.push_back(Pair("total-over-liability", (uint64_t) plEvent.nTotalOverPotentialLiability));
        event.push_back(Pair("total-under-bets", (uint64_t) plEvent.nTotalUnderBets));
        event.push_back(Pair("total-under-liability", (uint64_t) plEvent.nTotalUnderPotentialLiability));
        event.push_back(Pair("total-push-bets", (uint64_t) plEvent.nTotalPushBets));
        event.push_back(Pair("total-push-liability", (uint64_t) plEvent.nTotalPushPotentialLiability));

        result.push_back(event);
    }

    return result;
}

/**
 * Get total liability for each event that is currently active.
 *
 * @param params The RPC params consisting of the event id.
 * @param fHelp  Help text
 * @return
 */
UniValue geteventliability(const UniValue& params, bool fHelp)
{
  if (fHelp || (params.size() != 1))
        throw std::runtime_error(
            "geteventliability\n"
            "Return the payout of each event.\n"
            "\nArguments:\n"
            "1. Event id (numeric, required) The event id required for get liability.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"event-id\": \"xxx\", (numeric) The id of the event.\n"
            "    \"event-status\": \"status\", (string) The status of the event (running | resulted).\n"
            "    \"moneyline-home-bets\": \"xxx\", (numeric) The number of bets to moneyline home (parlays included).\n"
            "    \"moneyline-home-liability\": \"xxx\", (numeric) The moneyline home potentional liability (without parlays).\n"
            "    \"moneyline-away-bets\": \"xxx\", (numeric) The number of bets to moneyline away (parlays included).\n"
            "    \"moneyline-away-liability\": \"xxx\", (numeric) The moneyline away potentional liability (without parlays).\n"
            "    \"moneyline-draw-bets\": \"xxx\", (numeric) The number of bets to moneyline draw (parlays included).\n"
            "    \"moneyline-draw-liability\": \"xxx\", (numeric) The moneyline draw potentional liability (without parlays).\n"
            "    \"spread-home-bets\": \"xxx\", (numeric) The number of bets to spread home (parlays included).\n"
            "    \"spread-home-liability\": \"xxx\", (numeric) The spreads home potentional liability (without parlays).\n"
            "    \"spread-away-bets\": \"xxx\", (numeric) The number of bets to spread away (parlays included).\n"
            "    \"spread-away-liability\": \"xxx\", (numeric) The spread away potentional liability (without parlays).\n"
            "    \"spread-push-bets\": \"xxx\", (numeric) The number of bets to spread push (parlays included).\n"
            "    \"spread-push-liability\": \"xxx\", (numeric) The spread push potentional liability (without parlays).\n"
            "    \"total-over-bets\": \"xxx\", (numeric) The number of bets to total over (parlays included).\n"
            "    \"total-over-liability\": \"xxx\", (numeric) The total over potentional liability (without parlays).\n"
            "    \"total-under-bets\": \"xxx\", (numeric) The number of bets to total under (parlays included).\n"
            "    \"total-under-liability\": \"xxx\", (numeric) The total under potentional liability (without parlays).\n"
            "    \"total-push-bets\": \"xxx\", (numeric) The number of bets to total push (parlays included).\n"
            "    \"total-push-liability\": \"xxx\", (numeric) The total push potentional liability (without parlays).\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("geteventliability", "10") + HelpExampleRpc("geteventliability", "10"));

    LOCK(cs_main);

    uint32_t eventId = static_cast<uint32_t>(params[0].get_int());

    UniValue event(UniValue::VOBJ);

    CPeerlessExtendedEventDB plEvent;
    if (bettingsView->events->Read(EventKey{eventId}, plEvent)) {

        event.push_back(Pair("event-id", (uint64_t) plEvent.nEventId));
        if (!bettingsView->results->Exists(ResultKey{eventId})) {
            event.push_back(Pair("event-status", "running"));
            event.push_back(Pair("moneyline-home-bets", (uint64_t) plEvent.nMoneyLineHomeBets));
            event.push_back(Pair("moneyline-home-liability", (uint64_t) plEvent.nMoneyLineHomePotentialLiability));
            event.push_back(Pair("moneyline-away-bets", (uint64_t) plEvent.nMoneyLineAwayBets));
            event.push_back(Pair("moneyline-away-liability", (uint64_t) plEvent.nMoneyLineAwayPotentialLiability));
            event.push_back(Pair("moneyline-draw-bets", (uint64_t) plEvent.nMoneyLineDrawBets));
            event.push_back(Pair("moneyline-draw-liability", (uint64_t) plEvent.nMoneyLineDrawPotentialLiability));
            event.push_back(Pair("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets));
            event.push_back(Pair("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability));
            event.push_back(Pair("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets));
            event.push_back(Pair("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability));
            event.push_back(Pair("spread-away-bets", (uint64_t) plEvent.nSpreadAwayBets));
            event.push_back(Pair("spread-away-liability", (uint64_t) plEvent.nSpreadAwayPotentialLiability));
            event.push_back(Pair("spread-push-bets", (uint64_t) plEvent.nSpreadPushBets));
            event.push_back(Pair("spread-push-liability", (uint64_t) plEvent.nSpreadPushPotentialLiability));
            event.push_back(Pair("total-over-bets", (uint64_t) plEvent.nTotalOverBets));
            event.push_back(Pair("total-over-liability", (uint64_t) plEvent.nTotalOverPotentialLiability));
            event.push_back(Pair("total-under-bets", (uint64_t) plEvent.nTotalUnderBets));
            event.push_back(Pair("total-under-liability", (uint64_t) plEvent.nTotalUnderPotentialLiability));
            event.push_back(Pair("total-push-bets", (uint64_t) plEvent.nTotalPushBets));
            event.push_back(Pair("total-push-liability", (uint64_t) plEvent.nTotalPushPotentialLiability));
        }
        else {
            event.push_back(Pair("event-status", "resulted"));
        }
    }

    return event;
}

/**
 * Get total liability for each field event that is currently active.
 *
 * @param params The RPC params consisting of the field event id.
 * @param fHelp  Help text
 * @return
 */
UniValue getfieldeventliability(const UniValue& params, bool fHelp)
{
  if (fHelp || (params.size() != 1))
        throw std::runtime_error(
            "getfieldeventliability\n"
            "Return the payout of each field event.\n"
            "\nArguments:\n"
            "1. FieldEvent id (numeric, required) The field event id required for get liability.\n"

            "\nResult:\n"
            "  {\n"
            "    \"event-id\": \"xxx\", (numeric) The id of the field event.\n"
            "    \"event-status\": \"status\", (string) The status of the event (running | resulted).\n"
            "    \"contenders\":\n"
            "      [\n"
            "         {\n"
            "           \"contender-id\" : xxx (numeric) contender id,\n"
            "           \"outright-bets\": \"xxx\", (numeric) The number of bets to outright market (parlays included).\n"
            "           \"outright-liability\": \"xxx\", (numeric) The outright market potentional liability (without parlays).\n"
            "         }\n"
            "      ]\n"
            "  }\n"

            "\nExamples:\n" +
            HelpExampleCli("getfieldeventliability", "10") + HelpExampleRpc("getfieldeventliability", "10"));

    LOCK(cs_main);

    uint32_t eventId = static_cast<uint32_t>(params[0].get_int());
    UniValue vEvent(UniValue::VOBJ);
    CFieldEventDB fEvent;
    if (bettingsView->fieldEvents->Read(FieldEventKey{eventId}, fEvent)) {
        vEvent.push_back(Pair("event-id", (uint64_t) fEvent.nEventId));
        if (!bettingsView->fieldResults->Exists(FieldResultKey{eventId})) {
            vEvent.push_back(Pair("event-status", "running"));
            UniValue vContenders(UniValue::VARR);
            for (const auto& contender : fEvent.contenders) {
                UniValue vContender(UniValue::VOBJ);
                vContender.push_back(Pair("contender-id", (uint64_t) contender.first));
                vContender.push_back(Pair("outright-bets", (uint64_t) contender.second.nOutrightBets));
                vContender.push_back(Pair("outright-liability", (uint64_t) contender.second.nOutrightPotentialLiability));
                vContender.push_back(Pair("place-bets", (uint64_t) contender.second.nPlaceBets));
                vContender.push_back(Pair("place-liability", (uint64_t) contender.second.nPlacePotentialLiability));
                vContender.push_back(Pair("show-bets", (uint64_t) contender.second.nShowBets));
                vContender.push_back(Pair("show-liability", (uint64_t) contender.second.nShowPotentialLiability));
                vContenders.push_back(vContender);
            }
            vEvent.push_back(Pair("contenders", vContenders));
        }
        else {
            vEvent.push_back(Pair("event-status", "resulted"));
        }
    }

    return vEvent;
}

UniValue sendtoaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw std::runtime_error(
            "sendtoaddress \"wagerraddress\" amount ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"wagerraddress\"  (string, required) The wagerr address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in WGR to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"

            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"

            "\nExamples:\n" +
            HelpExampleCli("sendtoaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0.1") +
            HelpExampleCli("sendtoaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0.1 \"donation\" \"seans outpost\"") +
            HelpExampleRpc("sendtoaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\", 0.1, \"donation\", \"seans outpost\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"] = params[3].get_str();

    EnsureWalletIsUnlocked();

    SendMoney(address.Get(), nAmount, wtx);

    return wtx.GetHash().GetHex();
}

UniValue sendtoaddressix(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw std::runtime_error(
            "sendtoaddressix \"wagerraddress\" amount ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"wagerraddress\"  (string, required) The wagerr address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in WGR to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"

            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"

            "\nExamples:\n" +
            HelpExampleCli("sendtoaddressix", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0.1") +
            HelpExampleCli("sendtoaddressix", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0.1 \"donation\" \"seans outpost\"") +
            HelpExampleRpc("sendtoaddressix", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\", 0.1, \"donation\", \"seans outpost\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"] = params[3].get_str();

    EnsureWalletIsUnlocked();

    SendMoney(address.Get(), nAmount, wtx, true);

    return wtx.GetHash().GetHex();
}

UniValue listaddressgroupings(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error(
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"

            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"wagerraddress\",     (string) The wagerr address\n"
            "      amount,                 (numeric) The amount in WGR\n"
            "      \"account\"             (string, optional) The account\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listaddressgroupings", "") + HelpExampleRpc("listaddressgroupings", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue jsonGroupings(UniValue::VARR);
    std::map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
    for (std::set<CTxDestination> grouping : pwalletMain->GetAddressGroupings()) {
        UniValue jsonGrouping(UniValue::VARR);
        for (CTxDestination address : grouping) {
            UniValue addressInfo(UniValue::VARR);
            addressInfo.push_back(CBitcoinAddress(address).ToString());
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get()) != pwalletMain->mapAddressBook.end())
                    addressInfo.push_back(pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get())->second.name);
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

UniValue signmessage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
            "signmessage \"wagerraddress\" \"message\"\n"
            "\nSign a message with the private key of an address" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"wagerraddress\"  (string, required) The wagerr address to use for the private key.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"

            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"

            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n" +
            HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n" +
            HelpExampleCli("signmessage", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" \"my message\"") +
            "\nVerify the signature\n" +
            HelpExampleCli("verifymessage", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" \"signature\" \"my message\"") +
            "\nAs json rpc\n" +
            HelpExampleRpc("signmessage", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\", \"my message\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    std::string strAddress = params[0].get_str();
    std::string strMessage = params[1].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

UniValue getreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "getreceivedbyaddress \"wagerraddress\" ( minconf )\n"
            "\nReturns the total amount received by the given wagerraddress in transactions with at least minconf confirmations.\n"

            "\nArguments:\n"
            "1. \"wagerraddress\"  (string, required) The wagerr address for transactions.\n"
            "2. minconf             (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"

            "\nResult:\n"
            "amount   (numeric) The total amount in WGR received at this address.\n"

            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n" +
            HelpExampleCli("getreceivedbyaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n" +
            HelpExampleCli("getreceivedbyaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n" +
            HelpExampleCli("getreceivedbyaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 6") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("getreceivedbyaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\", 6"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // wagerr address
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    if (!IsMine(*pwalletMain, scriptPubKey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Address not found in wallet");

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    CAmount nAmount = 0;
    for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        for (const CTxOut& txout : wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return ValueFromAmount(nAmount);
}


UniValue getreceivedbyaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\nReturns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"

            "\nArguments:\n"
            "1. \"account\"      (string, required) The selected account, may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"

            "\nResult:\n"
            "amount              (numeric) The total amount in WGR received for this account.\n"

            "\nExamples:\n"
            "\nAmount received by the default account with at least 1 confirmation\n" +
            HelpExampleCli("getreceivedbyaccount", "\"\"") +
            "\nAmount received at the tabby account including unconfirmed amounts with zero confirmations\n" +
            HelpExampleCli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n" +
            HelpExampleCli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("getreceivedbyaccount", "\"tabby\", 6"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys assigned to account
    std::string strAccount = AccountFromValue(params[0]);
    std::set<CTxDestination> setAddress = pwalletMain->GetAccountAddresses(strAccount);

    // Tally
    CAmount nAmount = 0;
    for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        for (const CTxOut& txout : wtx.vout) {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwalletMain, address) && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}


CAmount GetAccountBalance(CWalletDB& walletdb, const std::string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CAmount nBalance = 0;

    // Tally wallet transactions
    for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        bool fConflicted;
        int depth = wtx.GetDepthAndMempool(fConflicted);

        if (!IsFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || depth < 0 || fConflicted)
            continue;

        CAmount nReceived, nSent, nFee;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, nFee, filter);

        if (nReceived != 0 && depth >= nMinDepth)
            nBalance += nReceived;
        nBalance -= nSent + nFee;
    }

    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    return nBalance;
}

CAmount GetAccountBalance(const std::string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter);
}


UniValue getbalance(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw std::runtime_error(
            "getbalance ( \"account\" minconf includeWatchonly )\n"
            "\nIf account is not specified, returns the server's total available balance (excluding zerocoins).\n"
            "If account is specified, returns the balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"

            "\nArguments:\n"
            "1. \"account\"      (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"

            "\nResult:\n"
            "amount              (numeric) The total amount in WGR received for this account.\n"

            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n" +
            HelpExampleCli("getbalance", "") +
            "\nThe total amount in the server across all accounts, with at least 5 confirmations\n" +
            HelpExampleCli("getbalance", "\"*\" 6") +
            "\nThe total amount in the default account with at least 1 confirmation\n" +
            HelpExampleCli("getbalance", "\"\"") +
            "\nThe total amount in the account named tabby with at least 6 confirmations\n" +
            HelpExampleCli("getbalance", "\"tabby\" 6") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("getbalance", "\"tabby\", 6"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 0)
        return ValueFromAmount(pwalletMain->GetBalance());

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 2)
        if (params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and "getbalance * 1 true" should return the same number
        CAmount nBalance = 0;
        for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
            const CWalletTx& wtx = (*it).second;
            bool fConflicted;
            int depth = wtx.GetDepthAndMempool(fConflicted);

            if (!IsFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || depth < 0 || fConflicted)
                continue;

            CAmount allFee;
            std::string strSentAccount;
            std::list<COutputEntry> listReceived;
            std::list<COutputEntry> listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
            if (depth >= nMinDepth) {
                for (const COutputEntry& r : listReceived)
                    nBalance += r.amount;
            }
            for (const COutputEntry& s : listSent)
                nBalance -= s.amount;
            nBalance -= allFee;
        }
        return ValueFromAmount(nBalance);
    }

    std::string strAccount = AccountFromValue(params[0]);

    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}

UniValue getextendedbalance(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 0))
        throw std::runtime_error(
            "getextendedbalance\n"
            "\nGet extended balance information.\n"

            "\nResult:\n"
            "{\n"
            "  \"blocks\": \"xxx\", (string) The current block height\n"
            "  \"balance\": \"xxx\", (string) The total WGR balance\n"
            "  \"balance_locked\": \"xxx\", (string) The locked WGR balance\n"
            "  \"balance_unlocked\": \"xxx\", (string) The unlocked WGR balance\n"
            "  \"balance_unconfirmed\": \"xxx\", (string) The unconfirmed WGR balance\n"
            "  \"balance_immature\": \"xxx\", (string) The immature WGR balance\n"
            "  \"zerocoin_balance\": \"xxx\", (string) The total zWGR balance\n"
            "  \"zerocoin_balance_mature\": \"xxx\", (string) The mature zWGR balance\n"
            "  \"zerocoin_balance_immature\": \"xxx\", (string) The immature zWGR balance\n"
            "  \"watchonly_balance\": \"xxx\", (string) The total watch-only WGR balance\n"
            "  \"watchonly_balance_unconfirmed\": \"xxx\", (string) The unconfirmed watch-only WGR balance\n"
            "  \"watchonly_balance_immature\": \"xxx\", (string) The immature watch-only WGR balance\n"
            "  \"watchonly_balance_locked\": \"xxx\", (string) The locked watch-only WGR balance\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getextendedbalance", "") + HelpExampleRpc("getextendedbalance", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue obj(UniValue::VOBJ);

    obj.push_back(Pair("blocks", (int)chainActive.Height()));
    obj.push_back(Pair("balance", ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("balance_locked", ValueFromAmount(pwalletMain->GetLockedCoins())));
    obj.push_back(Pair("balance_unlocked", ValueFromAmount(pwalletMain->GetUnlockedCoins())));
    obj.push_back(Pair("balance_unconfirmed", ValueFromAmount(pwalletMain->GetUnconfirmedBalance())));
    obj.push_back(Pair("balance_immature", ValueFromAmount(pwalletMain->GetImmatureBalance())));
    obj.push_back(Pair("zerocoin_balance", ValueFromAmount(pwalletMain->GetZerocoinBalance(false))));
    obj.push_back(Pair("zerocoin_balance_mature", ValueFromAmount(pwalletMain->GetZerocoinBalance(true))));
    obj.push_back(Pair("zerocoin_balance_immature", ValueFromAmount(pwalletMain->GetImmatureZerocoinBalance())));
    obj.push_back(Pair("watchonly_balance", ValueFromAmount(pwalletMain->GetWatchOnlyBalance())));
    obj.push_back(Pair("watchonly_balance_unconfirmed", ValueFromAmount(pwalletMain->GetUnconfirmedWatchOnlyBalance())));
    obj.push_back(Pair("watchonly_balance_immature", ValueFromAmount(pwalletMain->GetImmatureWatchOnlyBalance())));
    obj.push_back(Pair("watchonly_balance_locked", ValueFromAmount(pwalletMain->GetLockedWatchOnlyBalance())));

    return obj;
}

UniValue getunconfirmedbalance(const UniValue &params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            "getunconfirmedbalance\n"
            "Returns the server's total unconfirmed balance\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance());
}


UniValue movecmd(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw std::runtime_error(
            "move \"fromaccount\" \"toaccount\" amount ( minconf \"comment\" )\n"
            "\nMove a specified amount from one account in your wallet to another.\n"

            "\nArguments:\n"
            "1. \"fromaccount\"   (string, required) The name of the account to move funds from. May be the default account using \"\".\n"
            "2. \"toaccount\"     (string, required) The name of the account to move funds to. May be the default account using \"\".\n"
            "3. amount            (numeric, required) Quantity of WGR to move between accounts.\n"
            "4. minconf           (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"       (string, optional) An optional comment, stored in the wallet only.\n"

            "\nResult:\n"
            "true|false           (boolean) true if successful.\n"

            "\nExamples:\n"
            "\nMove 0.01 WGR from the default account to the account named tabby\n" +
            HelpExampleCli("move", "\"\" \"tabby\" 0.01") +
            "\nMove 0.01 WGR from timotei to akiko with a comment\n" +
            HelpExampleCli("move", "\"timotei\" \"akiko\" 0.01 1 \"happy birthday!\"") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("move", "\"timotei\", \"akiko\", 0.01, 1, \"happy birthday!\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strFrom = AccountFromValue(params[0]);
    std::string strTo = AccountFromValue(params[1]);
    CAmount nAmount = AmountFromValue(params[2]);
    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    std::string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    pwalletMain->AddAccountingEntry(debit, walletdb);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    pwalletMain->AddAccountingEntry(credit, walletdb);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}


UniValue sendfrom(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw std::runtime_error(
            "sendfrom \"fromaccount\" \"towagerraddress\" amount ( minconf \"comment\" \"comment-to\" )\n"
            "\nSent an amount from an account to a wagerr address.\n"
            "The amount is a real and is rounded to the nearest 0.00000001." +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"fromaccount\"       (string, required) The name of the account to send funds from. May be the default account using \"\".\n"
            "2. \"towagerraddress\"  (string, required) The wagerr address to send funds to.\n"
            "3. amount                (numeric, required) The amount in WGR. (transaction fee is added on top).\n"
            "4. minconf               (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"           (string, optional) A comment used to store what the transaction is for. \n"
            "                                     This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"        (string, optional) An optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. This is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"

            "\nResult:\n"
            "\"transactionid\"        (string) The transaction id.\n"

            "\nExamples:\n"
            "\nSend 0.01 WGR from the default account to the address, must have at least 1 confirmation\n" +
            HelpExampleCli("sendfrom", "\"\" \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0.01") +
            "\nSend 0.01 from the tabby account to the given address, funds must have at least 6 confirmations\n" +
            HelpExampleCli("sendfrom", "\"tabby\" \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 0.01 6 \"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("sendfrom", "\"tabby\", \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\", 0.01, 6, \"donation\", \"seans outpost\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount = AccountFromValue(params[0]);
    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");
    CAmount nAmount = AmountFromValue(params[2]);
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && !params[4].isNull() && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && !params[5].isNull() && !params[5].get_str().empty())
        wtx.mapValue["to"] = params[5].get_str();

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (nAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    SendMoney(address.Get(), nAmount, wtx);

    return wtx.GetHash().GetHex();
}


UniValue sendmany(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw std::runtime_error(
            "sendmany \"fromaccount\" {\"address\":amount,...} ( minconf \"comment\" )\n"
            "\nSend multiple times. Amounts are double-precision floating point numbers." +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"fromaccount\"         (string, required) The account to send the funds from, can be \"\" for the default account\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) The wagerr address is the key, the numeric amount in WGR is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) A comment\n"

            "\nResult:\n"
            "\"transactionid\"          (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"

            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n" +
            HelpExampleCli("sendmany", "\"tabby\" \"{\\\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\\\":0.01,\\\"DAD3Y6ivr8nPQLT1NEPX84DxGCw9jz9Jvg\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n" +
            HelpExampleCli("sendmany", "\"tabby\" \"{\\\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\\\":0.01,\\\"DAD3Y6ivr8nPQLT1NEPX84DxGCw9jz9Jvg\\\":0.02}\" 6 \"testing\"") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("sendmany", "\"tabby\", \"{\\\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\\\":0.01,\\\"DAD3Y6ivr8nPQLT1NEPX84DxGCw9jz9Jvg\\\":0.02}\", 6, \"testing\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount = AccountFromValue(params[0]);
    UniValue sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    std::set<CBitcoinAddress> setAddress;
    std::vector<std::pair<CScript, CAmount> > vecSend;

    CAmount totalAmount = 0;
    std::vector<std::string> keys = sendTo.getKeys();
    for (const std::string& name_ : keys) {
        CBitcoinAddress address(name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid WAGERR address: ")+name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ")+name_);
        setAddress.insert(address);

        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(sendTo[name_]);
        totalAmount += nAmount;

        vecSend.push_back(std::make_pair(scriptPubKey, nAmount));
    }

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    CReserveKey keyChange(pwalletMain);
    CAmount nFeeRequired = 0;
    std::string strFailReason;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, strFailReason);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

// Defined in rpc/misc.cpp
extern CScript _createmultisig_redeemScript(const UniValue& params);

UniValue addmultisigaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw std::runtime_error(
            "addmultisigaddress nrequired [\"key\",...] ( \"account\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a WAGERR address or hex-encoded public key.\n"
            "If 'account' is specified, assign address to that account.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) A json array of wagerr addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) wagerr address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"      (string, optional) An account to assign the addresses to.\n"

            "\nResult:\n"
            "\"wagerraddress\"  (string) A wagerr address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n" +
            HelpExampleCli("addmultisigaddress", "2 \"[\\\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\\\",\\\"DAD3Y6ivr8nPQLT1NEPX84DxGCw9jz9Jvg\\\"]\"") +
            "\nAs json rpc call\n" +
            HelpExampleRpc("addmultisigaddress", "2, \"[\\\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\\\",\\\"DAD3Y6ivr8nPQLT1NEPX84DxGCw9jz9Jvg\\\"]\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    pwalletMain->AddCScript(inner);

    pwalletMain->SetAddressBook(innerID, strAccount, "send");
    return CBitcoinAddress(innerID).ToString();
}


struct tallyitem {
    CAmount nAmount;
    int nConf;
    int nBCConf;
    std::vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        nBCConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

UniValue ListReceived(const UniValue& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 2)
        if (params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    std::map<CBitcoinAddress, tallyitem> mapTally;
    for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;

        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        int nBCDepth = wtx.GetDepthInMainChain(false);
        if (nDepth < nMinDepth)
            continue;

        for (const CTxOut& txout : wtx.vout) {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if (!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = std::min(item.nConf, nDepth);
            item.nBCConf = std::min(item.nBCConf, nBCDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    UniValue ret(UniValue::VARR);
    std::map<std::string, tallyitem> mapAccountTally;
    for (const PAIRTYPE(CBitcoinAddress, CAddressBookData) & item : pwalletMain->mapAddressBook) {
        const CBitcoinAddress& address = item.first;
        const std::string& strAccount = item.second.name;
        std::map<CBitcoinAddress, tallyitem>::iterator it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount = 0;
        int nConf = std::numeric_limits<int>::max();
        int nBCConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end()) {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            nBCConf = (*it).second.nBCConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (fByAccounts) {
            tallyitem& item = mapAccountTally[strAccount];
            item.nAmount += nAmount;
            item.nConf = std::min(item.nConf, nConf);
            item.nBCConf = std::min(item.nBCConf, nBCConf);
            item.fIsWatchonly = fIsWatchonly;
        } else {
            UniValue obj(UniValue::VOBJ);
            if (fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("address", address.ToString()));
            obj.push_back(Pair("account", strAccount));
            obj.push_back(Pair("amount", ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            obj.push_back(Pair("bcconfirmations", (nBCConf == std::numeric_limits<int>::max() ? 0 : nBCConf)));
            UniValue transactions(UniValue::VARR);
            if (it != mapTally.end()) {
                for (const uint256& item : (*it).second.txids) {
                    transactions.push_back(item.GetHex());
                }
            }
            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts) {
        for (std::map<std::string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it) {
            CAmount nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            int nBCConf = (*it).second.nBCConf;
            UniValue obj(UniValue::VOBJ);
            if ((*it).second.fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("account", (*it).first));
            obj.push_back(Pair("amount", ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            obj.push_back(Pair("bcconfirmations", (nBCConf == std::numeric_limits<int>::max() ? 0 : nBCConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

UniValue listreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw std::runtime_error(
            "listreceivedbyaddress ( minconf includeempty includeWatchonly)\n"
            "\nList balances by receiving address.\n"

            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : \"true\",    (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"account\" : \"accountname\",       (string) The account of the receiving address. The default account is \"\".\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in WGR received by the address\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"bcconfirmations\" : n              (numeric) The number of blockchain confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listreceivedbyaddress", "") + HelpExampleCli("listreceivedbyaddress", "6 true") + HelpExampleRpc("listreceivedbyaddress", "6, true, true"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, false);
}

UniValue listreceivedbyaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw std::runtime_error(
            "listreceivedbyaccount ( minconf includeempty includeWatchonly)\n"
            "\nList balances by account.\n"

            "\nArguments:\n"
            "1. minconf      (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty (boolean, optional, default=false) Whether to include accounts that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : \"true\",    (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) The account name of the receiving account\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this account\n"
            "    \"confirmations\" : n           (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"bcconfirmations\" : n         (numeric) The number of blockchain confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listreceivedbyaccount", "") + HelpExampleCli("listreceivedbyaccount", "6 true") + HelpExampleRpc("listreceivedbyaccount", "6, true, true"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, true);
}

static void MaybePushAddress(UniValue & entry, const CTxDestination &dest)
{
    CBitcoinAddress addr;
    if (addr.Set(dest))
        entry.push_back(Pair("address", addr.ToString()));
}

void ListTransactions(const CWalletTx& wtx, const std::string& strAccount, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)
{
    CAmount nFee;
    std::string strSentAccount;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == std::string("*"));
    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount)) {
        for (const COutputEntry& s : listSent) {
            UniValue entry(UniValue::VOBJ);
            if (involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY))
                entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            std::map<std::string, std::string>::const_iterator it = wtx.mapValue.find("DS");
            entry.push_back(Pair("category", (it != wtx.mapValue.end() && it->second == "1") ? "darksent" : "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    // Received
    int depth = wtx.GetDepthInMainChain();
    if (listReceived.size() > 0 && depth >= nMinDepth) {
        for (const COutputEntry& r : listReceived) {
            std::string account;
            if (pwalletMain->mapAddressBook.count(r.destination))
                account = pwalletMain->mapAddressBook[r.destination].name;
            if (fAllAccounts || (account == strAccount)) {
                UniValue entry(UniValue::VOBJ);
                if (involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY))
                    entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.destination);
                if (wtx.IsCoinBase()) {
                    if (depth < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (wtx.GetBlocksToMaturity() > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));
                } else {
                    entry.push_back(Pair("category", "receive"));
                }
                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                entry.push_back(Pair("vout", r.vout));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const std::string& strAccount, UniValue& ret)
{
    bool fAllAccounts = (strAccount == std::string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount) {
        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

UniValue listtransactions(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw std::runtime_error(
            "listtransactions ( \"account\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"

            "\nArguments:\n"
            "1. \"account\"    (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
            "                                     If \"\" is set, it will list transactions for the default account.\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",       (string) The account name associated with the transaction. \n"
            "                                                It will be \"\" for the default account.\n"
            "    \"address\":\"wagerraddress\",    (string) The wagerr address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,          (numeric) The amount in WGR. This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in WGR. This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"bcconfirmations\": n,     (numeric) The number of blockchain confirmations for the transaction. Available for 'send'\n"
            "                                         'receive' category of transactions. Negative confirmations indicate the\n"
            "                                         transation conflicts with the block chain\n"
            "    \"trusted\": xxx            (bool) Whether we consider the outputs of this unconfirmed transaction safe to spend.\n"
            "                                          and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) For the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n" +
            HelpExampleCli("listtransactions", "") +
            "\nList the most recent 10 transactions for the tabby account\n" +
            HelpExampleCli("listtransactions", "\"tabby\"") +
            "\nList transactions 100 to 120 from the tabby account\n" +
            HelpExampleCli("listtransactions", "\"tabby\" 20 100") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("listtransactions", "\"tabby\", 20, 100"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 3)
        if (params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    const CWallet::TxItems & txOrdered = pwalletMain->wtxOrdered;

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        if (pwtx != 0)
            ListTransactions(*pwtx, strAccount, 0, true, ret, filter);
        CAccountingEntry* const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount + nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    std::vector<UniValue> arrTmp = ret.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

void ListTransactionRecords(const CWalletTx& wtx, const std::string& strAccount, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)
{
    std::vector<TransactionRecord> vRecs = TransactionRecord::decomposeTransaction(pwalletMain, wtx);
    for(auto&& vRec: vRecs) {
        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("type", vRec.GetTransactionRecordType()));
        entry.push_back(Pair("transactionid", vRec.getTxID()));
        entry.push_back(Pair("outputindex", vRec.getOutputIndex()));
        entry.push_back(Pair("time", vRec.time));
        entry.push_back(Pair("debit", ValueFromAmount(vRec.debit)));
        entry.push_back(Pair("credit", ValueFromAmount(vRec.credit)));
        entry.push_back(Pair("involvesWatchonly", vRec.involvesWatchAddress));

        if (fLong) {
            if (vRec.statusUpdateNeeded()) vRec.updateStatus(wtx);

            entry.push_back(Pair("depth", vRec.status.depth));
            entry.push_back(Pair("status", vRec.GetTransactionStatus()));
            entry.push_back(Pair("countsForBalance", vRec.status.countsForBalance));
            entry.push_back(Pair("matures_in", vRec.status.matures_in));
            entry.push_back(Pair("open_for", vRec.status.open_for));
            entry.push_back(Pair("cur_num_blocks", vRec.status.cur_num_blocks));
            entry.push_back(Pair("cur_num_ix_locks", vRec.status.cur_num_ix_locks));
        }
        ret.push_back(entry);
    }
}

UniValue listtransactionrecords(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw std::runtime_error(
                "listtransactionrecords ( \"account\" count from includeWatchonly)\n"
                "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"

                "\nArguments:\n"
                "1. \"account\"    (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
                "                                     If \"\" is set, it will list transactions for the default account.\n"
                "2. count          (numeric, optional, default=10) The number of transactions to return\n"
                "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
                "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"

                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"type\" : \"type\",                         (string) The output type.\n"
                "    \"transactionid\" : \"hash\",                (string) The transaction hash in hex.\n"
                "    \"outputindex\" : n,                       (numeric) The transaction output index.\n"
                "    \"time\" : ttt,                            (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
                "    \"debit\" : x.xxx,                         (numeric) The transaction debit amount. This is negative and only available \n"
                "                                                 for the 'send' category of transactions.\n"
                "    \"credit\" : x.xxx,                        (numeric) The transaction debit amount. Available for the 'receive' category \n"
                "                                                 of transactions.\n"
                "    \"involvesWatchonly\" : true|false,        (boolean) Only returned if imported addresses were involved in transaction.\n"
                "    \"depth\" : n,                             (numeric) The depth of the transaction in the blockchain.\n"
                "    \"status\" : \"status\",                     (string) The transaction status.\n"
                "    \"countsForBalance\" : true|false,         (boolean) Does the transaction count towards the available balance.\n"
                "    \"matures_in\" : n,                        (numeric) The number of blocks until the transaction is mature.\n"
                "    \"open_for\" : n,                          (numeric) The number of blocks that need to be mined before finalization.\n"
                "    \"cur_num_blocks\" : n,                    (numeric) The current number of blocks.\n"
                "    \"cur_num_ix_locks\" : n,                  (numeric) When to update transaction for ix locks.\n"
                "  }\n"
                "]\n"

                "\nExamples:\n"
                "\nList the most recent 10 transactions in the systems\n" +
                HelpExampleCli("listtransactionrecords", "") +
                "\nList the most recent 10 transactions for the tabby account\n" +
                HelpExampleCli("listtransactionrecords", "\"tabby\"") +
                "\nList transactions 100 to 120 from the tabby account\n" +
                HelpExampleCli("listtransactionrecords", "\"tabby\" 20 100") +
                "\nAs a json rpc call\n" +
                HelpExampleRpc("listtransactionrecords", "\"tabby\", 20, 100"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 3)
        if (params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    const CWallet::TxItems & txOrdered = pwalletMain->wtxOrdered;

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        if (pwtx != 0){}
            ListTransactionRecords(*pwtx, strAccount, 0, true, ret, filter);
        CAccountingEntry* const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount + nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    std::vector<UniValue> arrTmp = ret.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

UniValue listaccounts(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw std::runtime_error(
            "listaccounts ( minconf includeWatchonly)\n"
            "\nReturns Object that has account names as keys, account balances as values.\n"

            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) Only include transactions with at least this many confirmations\n"
            "2. includeWatchonly (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"

            "\nResult:\n"
            "{                      (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"

            "\nExamples:\n"
            "\nList account balances where there at least 1 confirmation\n" +
            HelpExampleCli("listaccounts", "") +
            "\nList account balances including zero confirmation transactions\n" +
            HelpExampleCli("listaccounts", "0") +
            "\nList account balances for 6 or more confirmations\n" +
            HelpExampleCli("listaccounts", "6") +
            "\nAs json rpc call\n" +
            HelpExampleRpc("listaccounts", "6"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    isminefilter includeWatchonly = ISMINE_SPENDABLE;
    if (params.size() > 1)
        if (params[1].get_bool())
            includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;

    std::map<std::string, CAmount> mapAccountBalances;
    for (const PAIRTYPE(CTxDestination, CAddressBookData) & entry : pwalletMain->mapAddressBook) {
        if (IsMine(*pwalletMain, entry.first) & includeWatchonly) // This address belongs to me
            mapAccountBalances[entry.second.name] = 0;
    }

    for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        CAmount nFee;
        std::string strSentAccount;
        std::list<COutputEntry> listReceived;
        std::list<COutputEntry> listSent;
        bool fConflicted;
        int nDepth = wtx.GetDepthAndMempool(fConflicted);
        if (wtx.GetBlocksToMaturity() > 0 || nDepth < 0 || fConflicted)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        for (const COutputEntry& s : listSent)
            mapAccountBalances[strSentAccount] -= s.amount;
        if (nDepth >= nMinDepth) {
            for (const COutputEntry& r : listReceived)
                if (pwalletMain->mapAddressBook.count(r.destination))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.destination].name] += r.amount;
                else
                    mapAccountBalances[""] += r.amount;
        }
    }

    const std::list<CAccountingEntry> & acentries = pwalletMain->laccentries;
    for (const CAccountingEntry& entry : acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    UniValue ret(UniValue::VOBJ);
    for (const PAIRTYPE(std::string, CAmount) & accountBalance : mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

UniValue listsinceblock(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error(
            "listsinceblock ( \"blockhash\" target-confirmations includeWatchonly)\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"

            "\nArguments:\n"
            "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"

            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",       (string) The account name associated with the transaction. Will be \"\" for the default account.\n"
            "    \"address\":\"wagerraddress\",    (string) The wagerr address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in WGR. This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in WGR. This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"bcconfirmations\" : n,    (numeric) The number of blockchain confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
            "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the last block\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("listsinceblock", "") +
            HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6") +
            HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBlockIndex* pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0) {
        uint256 blockId = 0;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;
    }

    if (params.size() > 1) {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    if (params.size() > 2)
        if (params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    UniValue transactions(UniValue::VARR);

    for (std::map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++) {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain(false) < depth)
            ListTransactions(tx, "*", 0, true, transactions, filter);
    }

    CBlockIndex* pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : 0;

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

UniValue gettransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "gettransaction \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"

            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"

            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,        (numeric) The transaction amount in WGR\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"bcconfirmations\" : n,   (numeric) The number of blockchain confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The block index\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",  (string) The account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"wagerraddress\",   (string) The wagerr address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx                  (numeric) The amount in WGR\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"") +
            HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true") +
            HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 1)
        if (params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    CAmount nCredit = wtx.GetCredit(filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

    entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
    if (wtx.IsFromMe(filter))
        entry.push_back(Pair("fee", ValueFromAmount(nFee)));

    WalletTxToJSON(wtx, entry);

    UniValue details(UniValue::VARR);
    ListTransactions(wtx, "*", 0, false, details, filter);
    entry.push_back(Pair("details", details));

    std::string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
    entry.push_back(Pair("hex", strHex));

    return entry;
}

UniValue abandontransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "abandontransaction \"txid\"\n"
            "\nMark in-wallet transaction <txid> as abandoned\n"
            "This will mark this transaction and all its in-wallet descendants as abandoned which will allow\n"
            "for their inputs to be respent.  It can be used to replace \"stuck\" or evicted transactions.\n"
            "It only works on transactions which are not included in a block and are not currently in the mempool.\n"
            "It has no effect on transactions which are already conflicted or abandoned.\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    EnsureWalletIsUnlocked();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    if (!pwalletMain->AbandonTransaction(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not eligible for abandonment");

    return NullUniValue;
}


UniValue backupwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "backupwallet \"destination\"\n"
            "\nSafely copies wallet.dat to destination, which can be a directory or a path with filename.\n"

            "\nArguments:\n"
            "1. \"destination\"   (string) The destination directory or file\n"

            "\nExamples:\n" +
            HelpExampleCli("backupwallet", "\"backup.dat\"") + HelpExampleRpc("backupwallet", "\"backup.dat\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strDest = params[0].get_str();
    if (!BackupWallet(*pwalletMain, strDest))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return NullUniValue;
}


UniValue keypoolrefill(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool." +
            HelpRequiringPassphrase() + "\n"

            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"

            "\nExamples:\n" +
            HelpExampleCli("keypoolrefill", "") + HelpExampleRpc("keypoolrefill", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->TopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return NullUniValue;
}


static void LockWallet(CWallet* pWallet)
{
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = 0;
    pWallet->fWalletUnlockAnonymizeOnly = false;
    pWallet->Lock();
}

UniValue walletpassphrase(const UniValue& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 3))
        throw std::runtime_error(
            "walletpassphrase \"passphrase\" timeout ( anonymizeonly )\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending WGRs\n"

            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "3. anonymizeonly      (boolean, optional, default=false) If is true sending functions are disabled."

            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one. A timeout of \"0\" unlocks until the wallet is closed.\n"

            "\nExamples:\n"
            "\nUnlock the wallet for 60 seconds\n" +
            HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nUnlock the wallet for 60 seconds but allow anonymization, automint, and staking only\n" +
            HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60 true") +
            "\nLock the wallet again (before 60 seconds)\n" +
            HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n" +
            HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    bool anonymizeOnly = false;
    if (params.size() == 3)
        anonymizeOnly = params[2].get_bool();

    if (!pwalletMain->IsLocked() && pwalletMain->fWalletUnlockAnonymizeOnly && anonymizeOnly)
        throw JSONRPCError(RPC_WALLET_ALREADY_UNLOCKED, "Error: Wallet is already unlocked.");

    // Get the timeout
    int64_t nSleepTime = params[1].get_int64();
    // Timeout cannot be negative, otherwise it will relock immediately
    if (nSleepTime < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Timeout cannot be negative.");
    }
    // Clamp timeout
    constexpr int64_t MAX_SLEEP_TIME = 100000000; // larger values trigger a macos/libevent bug?
    if (nSleepTime > MAX_SLEEP_TIME) {
        nSleepTime = MAX_SLEEP_TIME;
    }

    if (!pwalletMain->Unlock(strWalletPass, anonymizeOnly))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    pwalletMain->TopUpKeyPool();

    if (nSleepTime > 0) {
        nWalletUnlockTime = GetTime () + nSleepTime;
        RPCRunLater ("lockwallet", boost::bind (LockWallet, pwalletMain), nSleepTime);
    }

    return NullUniValue;
}


UniValue walletpassphrasechange(const UniValue& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw std::runtime_error(
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"

            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"

            "\nExamples:\n" +
            HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"") + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw std::runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return NullUniValue;
}


UniValue walletlock(const UniValue& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw std::runtime_error(
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"

            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n" +
            HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n" +
            HelpExampleCli("sendtoaddress", "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n" +
            HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n" +
            HelpExampleRpc("walletlock", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return NullUniValue;
}


UniValue encryptwallet(const UniValue& params, bool fHelp)
{
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw std::runtime_error(
            "encryptwallet \"passphrase\"\n"
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"

            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"

            "\nExamples:\n"
            "\nEncrypt you wallet\n" +
            HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending WGRs\n" +
            HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n" +
            HelpExampleCli("signmessage", "\"wagerraddress\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n" +
            HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("encryptwallet", "\"my pass phrase\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw std::runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; wagerr server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

UniValue lockunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending WGRs.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"

            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, required) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n" +
            HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n" +
            HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n" +
            HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n" +
            HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 1)
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VBOOL));
    else
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VBOOL)(UniValue::VARR));

    bool fUnlock = params[0].get_bool();

    if (params.size() == 1) {
        if (fUnlock)
            pwalletMain->UnlockAllCoins();
        return true;
    }

    UniValue output_params = params[1].get_array();

    // Create and validate the COutPoints first.
    std::vector<COutPoint> outputs;
    outputs.reserve(output_params.size());

    for (unsigned int idx = 0; idx < output_params.size(); idx++) {
        const UniValue& output = output_params[idx];
        if (!output.isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
        const UniValue& o = output.get_obj();

        RPCTypeCheckObj(o, boost::assign::map_list_of("txid", UniValue::VSTR)("vout", UniValue::VNUM));

        const std::string& txid = find_value(o, "txid").get_str();
        if (!IsHex(txid)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");
        }

        const int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");
        }

        const COutPoint outpt(uint256(txid), nOutput);

        const auto it = pwalletMain->mapWallet.find(outpt.hash);
        if (it == pwalletMain->mapWallet.end()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, unknown transaction");
        }

        const CWalletTx& wtx = it->second;

        if (outpt.n >= wtx.vout.size()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout index out of bounds");
        }

        if (pwalletMain->IsSpent(outpt.hash, outpt.n)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected unspent output");
        }

        const bool is_locked = pwalletMain->IsLockedCoin(outpt.hash, outpt.n);

        if (fUnlock && !is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected locked output");
        }

        if (!fUnlock && is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, output already locked");
        }

        outputs.push_back(outpt);
    }

    // Atomically set (un)locked status for the outputs.
    for (const COutPoint& outpt : outputs) {
        if (fUnlock) pwalletMain->UnlockCoin(outpt);
        else pwalletMain->LockCoin(outpt);
    }

    return true;
}

UniValue listlockunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n" +
            HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n" +
            HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n" +
            HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n" +
            HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("listlockunspent", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::vector<COutPoint> vOutpts;
    pwalletMain->ListLockedCoins(vOutpts);

    UniValue ret(UniValue::VARR);

    for (COutPoint& outpt : vOutpts) {
        UniValue o(UniValue::VOBJ);

        o.push_back(Pair("txid", outpt.hash.GetHex()));
        o.push_back(Pair("vout", (int)outpt.n));
        ret.push_back(o);
    }

    return ret;
}

UniValue settxfee(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw std::runtime_error(
            "settxfee amount\n"
            "\nSet the transaction fee per kB.\n"

            "\nArguments:\n"
            "1. amount         (numeric, required) The transaction fee in WGR/kB rounded to the nearest 0.00000001\n"

            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n" +
            HelpExampleCli("settxfee", "0.00001") + HelpExampleRpc("settxfee", "0.00001"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Amount
    CAmount nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]); // rejects 0.0 amounts

    payTxFee = CFeeRate(nAmount, 1000);
    return true;
}

UniValue getwalletinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"

            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total WGR balance of the wallet\n"
            "  \"unconfirmed_balance\": xxx, (numeric) the total unconfirmed balance of the wallet in WGR\n"
            "  \"immature_balance\": xxxxxx, (numeric) the total immature balance of the wallet in WGR\n"
            "  \"txcount\": xxxxxxx,         (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee configuration, set in WGR/kB\n"
            "  \"automintaddresses\": status (boolean) the status of automint addresses (true if enabled, false if disabled)\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getwalletinfo", "") + HelpExampleRpc("getwalletinfo", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance", ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("unconfirmed_balance", ValueFromAmount(pwalletMain->GetUnconfirmedBalance())));
    obj.push_back(Pair("immature_balance",    ValueFromAmount(pwalletMain->GetImmatureBalance())));
    obj.push_back(Pair("txcount", (int)pwalletMain->mapWallet.size()));
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize", (int)pwalletMain->GetKeyPoolSize()));
    if (!pwalletMain->IsCrypted()) {
        obj.push_back(Pair("encryption_status", "unencrypted"));
    } else if (pwalletMain->fWalletUnlockAnonymizeOnly) {
        obj.push_back(Pair("encryption_status", "unlocked_for_anonimization_only"));
    } else if (pwalletMain->IsLocked()) {
        obj.push_back(Pair("encryption_status", "locked"));
    } else {
        obj.push_back(Pair("encryption_status", "unlocked"));
    }
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
    obj.push_back(Pair("automintaddresses", fEnableAutoConvert));
    return obj;
}

// ppcoin: reserve balance from being staked for network protection
UniValue reservebalance(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw std::runtime_error(
            "reservebalance ( reserve amount )\n"
            "\nShow or set the reserve amount not participating in network protection\n"
            "If no parameters provided current setting is printed.\n"

            "\nArguments:\n"
            "1. reserve     (boolean, optional) is true or false to turn balance reserve on or off.\n"
            "2. amount      (numeric, optional) is a real and rounded to cent.\n"

            "\nResult:\n"
            "{\n"
            "  \"reserve\": true|false,     (boolean) Status of the reserve balance\n"
            "  \"amount\": x.xxxx       (numeric) Amount reserved\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("reservebalance", "true 5000") + HelpExampleRpc("reservebalance", "true 5000"));

    if (params.size() > 0) {
        bool fReserve = params[0].get_bool();
        if (fReserve) {
            if (params.size() == 1)
                throw std::runtime_error("must provide amount to reserve balance.\n");
            CAmount nAmount = AmountFromValue(params[1]);
            nAmount = (nAmount / CENT) * CENT; // round to cent
            if (nAmount < 0)
                throw std::runtime_error("amount cannot be negative.\n");
            nReserveBalance = nAmount;
        } else {
            if (params.size() > 1)
                throw std::runtime_error("cannot specify amount to turn off reserve.\n");
            nReserveBalance = 0;
        }
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("reserve", (nReserveBalance > 0)));
    result.push_back(Pair("amount", ValueFromAmount(nReserveBalance)));
    return result;
}

// presstab HyperStake
UniValue setstakesplitthreshold(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "setstakesplitthreshold value\n"
            "\nThis will set the output size of your stakes to never be below this number\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. value   (numeric, required) Threshold value between 1 and 999999\n"

            "\nResult:\n"
            "{\n"
            "  \"threshold\": n,    (numeric) Threshold value set\n"
            "  \"saved\": true|false    (boolean) 'true' if successfully saved to the wallet file\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("setstakesplitthreshold", "5000") + HelpExampleRpc("setstakesplitthreshold", "5000"));

    EnsureWalletIsUnlocked();

    uint64_t nStakeSplitThreshold = params[0].get_int();

    if (nStakeSplitThreshold > 999999)
        throw std::runtime_error("Value out of range, max allowed is 999999");

    CWalletDB walletdb(pwalletMain->strWalletFile);
    LOCK(pwalletMain->cs_wallet);
    {
        bool fFileBacked = pwalletMain->fFileBacked;

        UniValue result(UniValue::VOBJ);
        pwalletMain->nStakeSplitThreshold = nStakeSplitThreshold;
        result.push_back(Pair("threshold", int(pwalletMain->nStakeSplitThreshold)));
        if (fFileBacked) {
            walletdb.WriteStakeSplitThreshold(nStakeSplitThreshold);
            result.push_back(Pair("saved", "true"));
        } else
            result.push_back(Pair("saved", "false"));

        return result;
    }
}

// presstab HyperStake
UniValue getstakesplitthreshold(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getstakesplitthreshold\n"
            "Returns the threshold for stake splitting\n"

            "\nResult:\n"
            "n      (numeric) Threshold value\n"

            "\nExamples:\n" +
            HelpExampleCli("getstakesplitthreshold", "") + HelpExampleRpc("getstakesplitthreshold", ""));

    return int(pwalletMain->nStakeSplitThreshold);
}

UniValue autocombinerewards(const UniValue& params, bool fHelp)
{
    bool fEnable = false;
    if (params.size() >= 1)
        fEnable = params[0].get_bool();

    if (fHelp || params.size() < 1 || (fEnable && params.size() != 2) || params.size() > 2)
        throw std::runtime_error(
            "autocombinerewards enable ( threshold )\n"
            "\nWallet will automatically monitor for any coins with value below the threshold amount, and combine them if they reside with the same WAGERR address\n"
            "When autocombinerewards runs it will create a transaction, and therefore will be subject to transaction fees.\n"

            "\nArguments:\n"
            "1. enable          (boolean, required) Enable auto combine (true) or disable (false)\n"
            "2. threshold       (numeric, optional) Threshold amount (default: 0)\n"

            "\nExamples:\n" +
            HelpExampleCli("autocombinerewards", "true 500") + HelpExampleRpc("autocombinerewards", "true 500"));

    CWalletDB walletdb(pwalletMain->strWalletFile);
    CAmount nThreshold = 0;

    if (fEnable)
        nThreshold = params[1].get_int();

    pwalletMain->fCombineDust = fEnable;
    pwalletMain->nAutoCombineThreshold = nThreshold;

    if (!walletdb.WriteAutoCombineSettings(fEnable, nThreshold))
        throw std::runtime_error("Changed settings in wallet but failed to save to database\n");

    return NullUniValue;
}

UniValue printMultiSend()
{
    UniValue ret(UniValue::VARR);
    UniValue act(UniValue::VOBJ);
    act.push_back(Pair("MultiSendStake Activated?", pwalletMain->fMultiSendStake));
    act.push_back(Pair("MultiSendMasternode Activated?", pwalletMain->fMultiSendMasternodeReward));
    ret.push_back(act);

    if (pwalletMain->vDisabledAddresses.size() >= 1) {
        UniValue disAdd(UniValue::VOBJ);
        for (unsigned int i = 0; i < pwalletMain->vDisabledAddresses.size(); i++) {
            disAdd.push_back(Pair("Disabled From Sending", pwalletMain->vDisabledAddresses[i]));
        }
        ret.push_back(disAdd);
    }

    ret.push_back("MultiSend Addresses to Send To:");

    UniValue vMS(UniValue::VOBJ);
    for (unsigned int i = 0; i < pwalletMain->vMultiSend.size(); i++) {
        vMS.push_back(Pair("Address " + std::to_string(i), pwalletMain->vMultiSend[i].first));
        vMS.push_back(Pair("Percent", pwalletMain->vMultiSend[i].second));
    }

    ret.push_back(vMS);
    return ret;
}

UniValue printAddresses()
{
    std::vector<COutput> vCoins;
    pwalletMain->AvailableCoins(vCoins);
    std::map<std::string, double> mapAddresses;
    for (const COutput& out : vCoins) {
        CTxDestination utxoAddress;
        ExtractDestination(out.tx->vout[out.i].scriptPubKey, utxoAddress);
        std::string strAdd = CBitcoinAddress(utxoAddress).ToString();

        if (mapAddresses.find(strAdd) == mapAddresses.end()) //if strAdd is not already part of the map
            mapAddresses[strAdd] = (double)out.tx->vout[out.i].nValue / (double)COIN;
        else
            mapAddresses[strAdd] += (double)out.tx->vout[out.i].nValue / (double)COIN;
    }

    UniValue ret(UniValue::VARR);
    for (std::map<std::string, double>::const_iterator it = mapAddresses.begin(); it != mapAddresses.end(); ++it) {
        UniValue obj(UniValue::VOBJ);
        const std::string* strAdd = &(*it).first;
        const double* nBalance = &(*it).second;
        obj.push_back(Pair("Address ", *strAdd));
        obj.push_back(Pair("Balance ", *nBalance));
        ret.push_back(obj);
    }

    return ret;
}

unsigned int sumMultiSend()
{
    unsigned int sum = 0;
    for (unsigned int i = 0; i < pwalletMain->vMultiSend.size(); i++)
        sum += pwalletMain->vMultiSend[i].second;
    return sum;
}

UniValue multisend(const UniValue& params, bool fHelp)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    bool fFileBacked;
    //MultiSend Commands
    if (params.size() == 1) {
        std::string strCommand = params[0].get_str();
        UniValue ret(UniValue::VOBJ);
        if (strCommand == "print") {
            return printMultiSend();
        } else if (strCommand == "printaddress" || strCommand == "printaddresses") {
            return printAddresses();
        } else if (strCommand == "clear") {
            LOCK(pwalletMain->cs_wallet);
            {
                bool erased = false;
                if (pwalletMain->fFileBacked) {
                    if (walletdb.EraseMultiSend(pwalletMain->vMultiSend))
                        erased = true;
                }

                pwalletMain->vMultiSend.clear();
                pwalletMain->setMultiSendDisabled();

                UniValue obj(UniValue::VOBJ);
                obj.push_back(Pair("Erased from database", erased));
                obj.push_back(Pair("Erased from RAM", true));

                return obj;
            }
        } else if (strCommand == "enablestake" || strCommand == "activatestake") {
            if (pwalletMain->vMultiSend.size() < 1)
                throw JSONRPCError(RPC_INVALID_REQUEST, "Unable to activate MultiSend, check MultiSend vector");

            if (CBitcoinAddress(pwalletMain->vMultiSend[0].first).IsValid()) {
                pwalletMain->fMultiSendStake = true;
                if (!walletdb.WriteMSettings(true, pwalletMain->fMultiSendMasternodeReward, pwalletMain->nLastMultiSendHeight)) {
                    UniValue obj(UniValue::VOBJ);
                    obj.push_back(Pair("error", "MultiSend activated but writing settings to DB failed"));
                    UniValue arr(UniValue::VARR);
                    arr.push_back(obj);
                    arr.push_back(printMultiSend());
                    return arr;
                } else
                    return printMultiSend();
            }

            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to activate MultiSend, check MultiSend vector");
        } else if (strCommand == "enablemasternode" || strCommand == "activatemasternode") {
            if (pwalletMain->vMultiSend.size() < 1)
                throw JSONRPCError(RPC_INVALID_REQUEST, "Unable to activate MultiSend, check MultiSend vector");

            if (CBitcoinAddress(pwalletMain->vMultiSend[0].first).IsValid()) {
                pwalletMain->fMultiSendMasternodeReward = true;

                if (!walletdb.WriteMSettings(pwalletMain->fMultiSendStake, true, pwalletMain->nLastMultiSendHeight)) {
                    UniValue obj(UniValue::VOBJ);
                    obj.push_back(Pair("error", "MultiSend activated but writing settings to DB failed"));
                    UniValue arr(UniValue::VARR);
                    arr.push_back(obj);
                    arr.push_back(printMultiSend());
                    return arr;
                } else
                    return printMultiSend();
            }

            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to activate MultiSend, check MultiSend vector");
        } else if (strCommand == "disable" || strCommand == "deactivate") {
            pwalletMain->setMultiSendDisabled();
            if (!walletdb.WriteMSettings(false, false, pwalletMain->nLastMultiSendHeight))
                throw JSONRPCError(RPC_DATABASE_ERROR, "MultiSend deactivated but writing settings to DB failed");

            return printMultiSend();
        } else if (strCommand == "enableall") {
            if (!walletdb.EraseMSDisabledAddresses(pwalletMain->vDisabledAddresses))
                return "failed to clear old vector from walletDB";
            else {
                pwalletMain->vDisabledAddresses.clear();
                return printMultiSend();
            }
        }
    }
    if (params.size() == 2 && params[0].get_str() == "delete") {
        int del = std::stoi(params[1].get_str().c_str());
        if (!walletdb.EraseMultiSend(pwalletMain->vMultiSend))
            throw JSONRPCError(RPC_DATABASE_ERROR, "failed to delete old MultiSend vector from database");

        pwalletMain->vMultiSend.erase(pwalletMain->vMultiSend.begin() + del);
        if (!walletdb.WriteMultiSend(pwalletMain->vMultiSend))
            throw JSONRPCError(RPC_DATABASE_ERROR, "walletdb WriteMultiSend failed!");

        return printMultiSend();
    }
    if (params.size() == 2 && params[0].get_str() == "disable") {
        std::string disAddress = params[1].get_str();
        if (!CBitcoinAddress(disAddress).IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "address you want to disable is not valid");
        else {
            pwalletMain->vDisabledAddresses.push_back(disAddress);
            if (!walletdb.EraseMSDisabledAddresses(pwalletMain->vDisabledAddresses))
                throw JSONRPCError(RPC_DATABASE_ERROR, "disabled address from sending, but failed to clear old vector from walletDB");

            if (!walletdb.WriteMSDisabledAddresses(pwalletMain->vDisabledAddresses))
                throw JSONRPCError(RPC_DATABASE_ERROR, "disabled address from sending, but failed to store it to walletDB");
            else
                return printMultiSend();
        }
    }

    //if no commands are used
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
            "multisend <command>\n"
            "****************************************************************\n"
            "WHAT IS MULTISEND?\n"
            "MultiSend allows a user to automatically send a percent of their stake reward to as many addresses as you would like\n"
            "The MultiSend transaction is sent when the staked coins mature (100 confirmations)\n"
            "****************************************************************\n"
            "TO CREATE OR ADD TO THE MULTISEND VECTOR:\n"
            "multisend <WAGERR Address> <percent>\n"
            "This will add a new address to the MultiSend vector\n"
            "Percent is a whole number 1 to 100.\n"
            "****************************************************************\n"
            "MULTISEND COMMANDS (usage: multisend <command>)\n"
            " print - displays the current MultiSend vector \n"
            " clear - deletes the current MultiSend vector \n"
            " enablestake/activatestake - activates the current MultiSend vector to be activated on stake rewards\n"
            " enablemasternode/activatemasternode - activates the current MultiSend vector to be activated on masternode rewards\n"
            " disable/deactivate - disables the current MultiSend vector \n"
            " delete <Address #> - deletes an address from the MultiSend vector \n"
            " disable <address> - prevents a specific address from sending MultiSend transactions\n"
            " enableall - enables all addresses to be eligible to send MultiSend transactions\n"
            "****************************************************************\n");

    //if the user is entering a new MultiSend item
    std::string strAddress = params[0].get_str();
    CBitcoinAddress address(strAddress);
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WGR address");
    if (std::stoi(params[1].get_str().c_str()) < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid percentage");
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    unsigned int nPercent = (unsigned int) std::stoul(params[1].get_str().c_str());

    LOCK(pwalletMain->cs_wallet);
    {
        fFileBacked = pwalletMain->fFileBacked;
        //Error if 0 is entered
        if (nPercent == 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Sending 0% of stake is not valid");
        }

        //MultiSend can only send 100% of your stake
        if (nPercent + sumMultiSend() > 100)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Failed to add to MultiSend vector, the sum of your MultiSend is greater than 100%");

        for (unsigned int i = 0; i < pwalletMain->vMultiSend.size(); i++) {
            if (pwalletMain->vMultiSend[i].first == strAddress)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Failed to add to MultiSend vector, cannot use the same address twice");
        }

        if (fFileBacked)
            walletdb.EraseMultiSend(pwalletMain->vMultiSend);

        std::pair<std::string, int> newMultiSend;
        newMultiSend.first = strAddress;
        newMultiSend.second = nPercent;
        pwalletMain->vMultiSend.push_back(newMultiSend);
        if (fFileBacked) {
            if (!walletdb.WriteMultiSend(pwalletMain->vMultiSend))
                throw JSONRPCError(RPC_DATABASE_ERROR, "walletdb WriteMultiSend failed!");
        }
    }
    return printMultiSend();
}

UniValue getzerocoinbalance(const UniValue& params, bool fHelp)
{

    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getzerocoinbalance\n"
            "\nReturn the wallet's total zWGR balance.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nResult:\n"
            "amount         (numeric) Total zWGR balance.\n"

            "\nExamples:\n" +
            HelpExampleCli("getzerocoinbalance", "") + HelpExampleRpc("getzerocoinbalance", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked(true);

        UniValue ret(UniValue::VOBJ);
        ret.push_back(Pair("Total", ValueFromAmount(pwalletMain->GetZerocoinBalance(false))));
        ret.push_back(Pair("Mature", ValueFromAmount(pwalletMain->GetZerocoinBalance(true))));
        ret.push_back(Pair("Unconfirmed", ValueFromAmount(pwalletMain->GetUnconfirmedZerocoinBalance())));
        ret.push_back(Pair("Immature", ValueFromAmount(pwalletMain->GetImmatureZerocoinBalance())));
        return ret;

}

UniValue listmintedzerocoins(const UniValue& params, bool fHelp)
{

    if (fHelp || params.size() > 2)
        throw std::runtime_error(
            "listmintedzerocoins (fVerbose) (fMatureOnly)\n"
            "\nList all zWGR mints in the wallet.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. fVerbose      (boolean, optional, default=false) Output mints metadata.\n"
            "2. fMatureOnly   (boolean, optional, default=false) List only mature mints.\n"
            "                 Set only if fVerbose is specified\n"

            "\nResult (with fVerbose=false):\n"
            "[\n"
            "  \"xxx\"      (string) Pubcoin in hex format.\n"
            "  ,...\n"
            "]\n"

            "\nResult (with fVerbose=true):\n"
            "[\n"
            "  {\n"
            "    \"serial hash\": \"xxx\",   (string) Mint serial hash in hex format.\n"
            "    \"version\": n,   (numeric) Zerocoin version number.\n"
            "    \"zWGR ID\": \"xxx\",   (string) Pubcoin in hex format.\n"
            "    \"denomination\": n,   (numeric) Coin denomination.\n"
            "    \"mint height\": n     (numeric) Height of the block containing this mint.\n"
            "    \"confirmations\": n   (numeric) Number of confirmations.\n"
            "    \"hash stake\": \"xxx\",   (string) Mint serialstake hash in hex format.\n"
            "  }\n"
            "  ,..."
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listmintedzerocoins", "") + HelpExampleRpc("listmintedzerocoins", "") +
            HelpExampleCli("listmintedzerocoins", "true") + HelpExampleRpc("listmintedzerocoins", "true") +
            HelpExampleCli("listmintedzerocoins", "true true") + HelpExampleRpc("listmintedzerocoins", "true, true"));

    bool fVerbose = (params.size() > 0) ? params[0].get_bool() : false;
    bool fMatureOnly = (params.size() > 1) ? params[1].get_bool() : false;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked(true);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    std::set<CMintMeta> setMints = pwalletMain->zwgrTracker->ListMints(true, fMatureOnly, true);

    int nBestHeight = chainActive.Height();

    UniValue jsonList(UniValue::VARR);
    if (fVerbose) {
        for (auto m : setMints) {
            // Construct mint object
            UniValue objMint(UniValue::VOBJ);
            objMint.push_back(Pair("serial hash", m.hashSerial.GetHex()));  // Serial hash
            objMint.push_back(Pair("version", m.nVersion));                 // Zerocoin version
            objMint.push_back(Pair("zWGR ID", m.hashPubcoin.GetHex()));     // PubCoin
            int denom = libzerocoin::ZerocoinDenominationToInt(m.denom);
            objMint.push_back(Pair("denomination", denom));                 // Denomination
            objMint.push_back(Pair("mint height", m.nHeight));              // Mint Height
            int nConfirmations = (m.nHeight && nBestHeight > m.nHeight) ? nBestHeight - m.nHeight : 0;
            objMint.push_back(Pair("confirmations", nConfirmations));       // Confirmations
            if (m.hashStake == 0) {
                CZerocoinMint mint;
                if (pwalletMain->GetMint(m.hashSerial, mint)) {
                    uint256 hashStake = mint.GetSerialNumber().getuint256();
                    hashStake = Hash(hashStake.begin(), hashStake.end());
                    m.hashStake = hashStake;
                    pwalletMain->zwgrTracker->UpdateState(m);
                }
            }
            objMint.push_back(Pair("hash stake", m.hashStake.GetHex()));    // hashStake
            // Push back mint object
            jsonList.push_back(objMint);
        }
    } else {
        for (const CMintMeta& m : setMints)
            // Push back PubCoin
            jsonList.push_back(m.hashPubcoin.GetHex());
    }
    return jsonList;
}

UniValue listzerocoinamounts(const UniValue& params, bool fHelp)
{

    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "listzerocoinamounts\n"
            "\nGet information about your zerocoin amounts.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"denomination\": n,   (numeric) Denomination Value.\n"
            "    \"mints\": n           (numeric) Number of mints.\n"
            "  }\n"
            "  ,..."
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listzerocoinamounts", "") + HelpExampleRpc("listzerocoinamounts", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked(true);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    std::set<CMintMeta> setMints = pwalletMain->zwgrTracker->ListMints(true, true, true);

    std::map<libzerocoin::CoinDenomination, CAmount> spread;
    for (const auto& denom : libzerocoin::zerocoinDenomList)
        spread.insert(std::pair<libzerocoin::CoinDenomination, CAmount>(denom, 0));
    for (auto& meta : setMints) spread.at(meta.denom)++;


    UniValue ret(UniValue::VARR);
    for (const auto& m : libzerocoin::zerocoinDenomList) {
        UniValue val(UniValue::VOBJ);
        val.push_back(Pair("denomination", libzerocoin::ZerocoinDenominationToInt(m)));
        val.push_back(Pair("mints", (int64_t)spread.at(m)));
        ret.push_back(val);
    }
    return ret;
}

UniValue listspentzerocoins(const UniValue& params, bool fHelp)
{

    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "listspentzerocoins\n"
            "\nList all the spent zWGR mints in the wallet.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nResult:\n"
            "[\n"
            "  \"xxx\"      (string) Pubcoin in hex format.\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listspentzerocoins", "") + HelpExampleRpc("listspentzerocoins", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked(true);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    std::list<CBigNum> listPubCoin = walletdb.ListSpentCoinsSerial();

    UniValue jsonList(UniValue::VARR);
    for (const CBigNum& pubCoinItem : listPubCoin) {
        jsonList.push_back(pubCoinItem.GetHex());
    }

    return jsonList;
}

UniValue mintzerocoin(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "mintzerocoin amount ( utxos )\n"
            "\nMint the specified zWGR amount\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. amount      (numeric, required) Enter an amount of Wgr to convert to zWGR\n"
            "2. utxos       (string, optional) A json array of objects.\n"
            "                   Each object needs the txid (string) and vout (numeric)\n"
            "  [\n"
            "    {\n"
            "      \"txid\":\"txid\",    (string) The transaction id\n"
            "      \"vout\": n         (numeric) The output number\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"

            "\nResult:\n"
            "{\n"
            "   \"txid\": \"xxx\",       (string) Transaction ID.\n"
            "   \"time\": nnn            (numeric) Time to mint this transaction.\n"
            "   \"mints\":\n"
            "   [\n"
            "      {\n"
            "         \"denomination\": nnn,     (numeric) Minted denomination.\n"
            "         \"pubcoin\": \"xxx\",      (string) Pubcoin in hex format.\n"
            "         \"randomness\": \"xxx\",   (string) Hex encoded randomness.\n"
            "         \"serial\": \"xxx\",       (string) Serial in hex format.\n"
            "      },\n"
            "      ...\n"
            "   ]\n"
            "}\n"

            "\nExamples:\n"
            "\nMint 50 from anywhere\n" +
            HelpExampleCli("mintzerocoin", "50") +
            "\nMint 13 from a specific output\n" +
            HelpExampleCli("mintzerocoin", "13 \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("mintzerocoin", "13, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\""));


    if (Params().NetworkID() != CBaseChainParams::REGTEST)
        throw JSONRPCError(RPC_WALLET_ERROR, "zWGR minting is DISABLED");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 1)
    {
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));
    } else
    {
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VARR));
    }

    int64_t nTime = GetTimeMillis();
    if(sporkManager.IsSporkActive(SPORK_16_ZEROCOIN_MAINTENANCE_MODE))
        throw JSONRPCError(RPC_WALLET_ERROR, "zWGR is currently disabled due to maintenance.");

    EnsureWalletIsUnlocked(true);

    CAmount nAmount = params[0].get_int() * COIN;

    CWalletTx wtx;
    std::vector<CDeterministicMint> vDMints;
    std::string strError;
    std::vector<COutPoint> vOutpts;

    if (params.size() == 2)
    {
        UniValue outputs = params[1].get_array();
        for (unsigned int idx = 0; idx < outputs.size(); idx++) {
            const UniValue& output = outputs[idx];
            if (!output.isObject())
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
            const UniValue& o = output.get_obj();

            RPCTypeCheckObj(o, boost::assign::map_list_of("txid", UniValue::VSTR)("vout", UniValue::VNUM));

            std::string txid = find_value(o, "txid").get_str();
            if (!IsHex(txid))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

            int nOutput = find_value(o, "vout").get_int();
            if (nOutput < 0)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

            COutPoint outpt(uint256(txid), nOutput);
            vOutpts.push_back(outpt);
        }
        strError = pwalletMain->MintZerocoinFromOutPoint(nAmount, wtx, vDMints, vOutpts);
    } else
    {
        strError = pwalletMain->MintZerocoin(nAmount, wtx, vDMints);
    }

    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    UniValue retObj(UniValue::VOBJ);
    retObj.push_back(Pair("txid", wtx.GetHash().ToString()));
    retObj.push_back(Pair("time", GetTimeMillis() - nTime));
    UniValue arrMints(UniValue::VARR);
    for (CDeterministicMint dMint : vDMints) {
        UniValue m(UniValue::VOBJ);
        m.push_back(Pair("denomination", ValueFromAmount(libzerocoin::ZerocoinDenominationToAmount(dMint.GetDenomination()))));
        m.push_back(Pair("pubcoinhash", dMint.GetPubcoinHash().GetHex()));
        m.push_back(Pair("serialhash", dMint.GetSerialHash().GetHex()));
        m.push_back(Pair("seedhash", dMint.GetSeedHash().GetHex()));
        m.push_back(Pair("count", (int64_t)dMint.GetCount()));
        arrMints.push_back(m);
    }
    retObj.push_back(Pair("mints", arrMints));

    return retObj;
}

UniValue spendzerocoin(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 5 || params.size() < 3)
        throw std::runtime_error(
            "spendzerocoin amount mintchange minimizechange ( \"address\" isPublicSpend)\n"
            "\nSpend zWGR to a WGR address.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. amount          (numeric, required) Amount to spend.\n"
            "2. mintchange      (boolean, required) Re-mint any leftover change.\n"
            "3. minimizechange  (boolean, required) Try to minimize the returning change  [false]\n"
            "4. \"address\"     (string, optional, default=change) Send to specified address or to a new change address.\n"
            "                       If there is change then an address is required\n"
            "5. isPublicSpend   (boolean, optional, default=true) create a public zc spend."
            "                       If false, instead create spend version 2 (only for regression tests)"

            "\nResult:\n"
            "{\n"
            "  \"txid\": \"xxx\",             (string) Transaction hash.\n"
            "  \"bytes\": nnn,              (numeric) Transaction size.\n"
            "  \"fee\": amount,             (numeric) Transaction fee (if any).\n"
            "  \"spends\": [                (array) JSON array of input objects.\n"
            "    {\n"
            "      \"denomination\": nnn,   (numeric) Denomination value.\n"
            "      \"pubcoin\": \"xxx\",      (string) Pubcoin in hex format.\n"
            "      \"serial\": \"xxx\",       (string) Serial number in hex format.\n"
            "      \"acc_checksum\": \"xxx\", (string) Accumulator checksum in hex format.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"outputs\": [                 (array) JSON array of output objects.\n"
            "    {\n"
            "      \"value\": amount,         (numeric) Value in WGR.\n"
            "      \"address\": \"xxx\"         (string) WGR address or \"zerocoinmint\" for reminted change.\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples\n" +
            HelpExampleCli("spendzerocoin", "5000 false true \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\"") +
            HelpExampleRpc("spendzerocoin", "5000 false true \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if(sporkManager.IsSporkActive(SPORK_16_ZEROCOIN_MAINTENANCE_MODE))
        throw JSONRPCError(RPC_WALLET_ERROR, "zWGR is currently disabled due to maintenance.");

    CAmount nAmount = AmountFromValue(params[0]);        // Spending amount
    const bool fMintChange = params[1].get_bool();       // Mint change to zWGR
    const bool fMinimizeChange = params[2].get_bool();    // Minimize change
    const std::string address_str = (params.size() > 3 ? params[3].get_str() : "");
    const bool isPublicSpend = (params.size() > 4 ? params[4].get_bool() : true);

    if (Params().NetworkID() != CBaseChainParams::REGTEST) {
        if (fMintChange)
            throw JSONRPCError(RPC_WALLET_ERROR, "zWGR minting is DISABLED (except for regtest), cannot mint change");

        if (!isPublicSpend)
            throw JSONRPCError(RPC_WALLET_ERROR, "zWGR old spend only available in regtest for tests purposes");
    }

    std::vector<CZerocoinMint> vMintsSelected;
    return DoZwgrSpend(nAmount, fMintChange, fMinimizeChange, vMintsSelected, address_str, isPublicSpend);
}


UniValue spendzerocoinmints(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw std::runtime_error(
            "spendzerocoinmints mints_list (\"address\" isPublicSpend) \n"
            "\nSpend zWGR mints to a WGR address.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. mints_list     (string, required) A json array of zerocoin mints serial hashes\n"
            "2. \"address\"     (string, optional, default=change) Send to specified address or to a new change address.\n"
            "3. isPublicSpend  (boolean, optional, default=true) create a public zc spend."
            "                       If false, instead create spend version 2 (only for regression tests)"

            "\nResult:\n"
            "{\n"
            "  \"txid\": \"xxx\",             (string) Transaction hash.\n"
            "  \"bytes\": nnn,              (numeric) Transaction size.\n"
            "  \"fee\": amount,             (numeric) Transaction fee (if any).\n"
            "  \"spends\": [                (array) JSON array of input objects.\n"
            "    {\n"
            "      \"denomination\": nnn,   (numeric) Denomination value.\n"
            "      \"pubcoin\": \"xxx\",      (string) Pubcoin in hex format.\n"
            "      \"serial\": \"xxx\",       (string) Serial number in hex format.\n"
            "      \"acc_checksum\": \"xxx\", (string) Accumulator checksum in hex format.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"outputs\": [                 (array) JSON array of output objects.\n"
            "    {\n"
            "      \"value\": amount,         (numeric) Value in WGR.\n"
            "      \"address\": \"xxx\"         (string) WGR address or \"zerocoinmint\" for reminted change.\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples\n" +
            HelpExampleCli("spendzerocoinmints", "'[\"0d8c16eee7737e3cc1e4e70dc006634182b175e039700931283b202715a0818f\", \"dfe585659e265e6a509d93effb906d3d2a0ac2fe3464b2c3b6d71a3ef34c8ad7\"]' \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\"") +
            HelpExampleRpc("spendzerocoinmints", "[\"0d8c16eee7737e3cc1e4e70dc006634182b175e039700931283b202715a0818f\", \"dfe585659e265e6a509d93effb906d3d2a0ac2fe3464b2c3b6d71a3ef34c8ad7\"], \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if(sporkManager.IsSporkActive(SPORK_16_ZEROCOIN_MAINTENANCE_MODE))
        throw JSONRPCError(RPC_WALLET_ERROR, "zWGR is currently disabled due to maintenance.");

    UniValue arrMints = params[0].get_array();
    const std::string address_str = (params.size() > 1 ? params[1].get_str() : "");
    const bool isPublicSpend = (params.size() > 2 ? params[2].get_bool() : true);

    if (arrMints.size() == 0)
        throw JSONRPCError(RPC_WALLET_ERROR, "No zerocoin selected");

    if (!isPublicSpend && Params().NetworkID() != CBaseChainParams::REGTEST) {
        throw JSONRPCError(RPC_WALLET_ERROR, "zWGR old spend only available in regtest for tests purposes");
    }

    // check mints supplied and save serial hash (do this here so we don't fetch if any is wrong)
    std::vector<uint256> vSerialHashes;
    for(unsigned int i = 0; i < arrMints.size(); i++) {
        std::string serialHashStr = arrMints[i].get_str();
        if (!IsHex(serialHashStr))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex serial hash");
        vSerialHashes.push_back(uint256(serialHashStr));
    }

    // fetch mints and update nAmount
    CAmount nAmount(0);
    std::vector<CZerocoinMint> vMintsSelected;
    for(const uint256& serialHash : vSerialHashes) {
        CZerocoinMint mint;
        if (!pwalletMain->GetMint(serialHash, mint)) {
            std::string strErr = "Failed to fetch mint associated with serial hash " + serialHash.GetHex();
            throw JSONRPCError(RPC_WALLET_ERROR, strErr);
        }
        vMintsSelected.emplace_back(mint);
        nAmount += mint.GetDenominationAsAmount();
    }

    return DoZwgrSpend(nAmount, false, true, vMintsSelected, address_str, isPublicSpend);
}


extern UniValue DoZwgrSpend(const CAmount nAmount, bool fMintChange, bool fMinimizeChange, std::vector<CZerocoinMint>& vMintsSelected, std::string address_str, bool isPublicSpend)
 {
    // zerocoin mint / v2 spend is disabled. fMintChange/isPublicSpend should be false here. Double check
    if (Params().NetworkID() != CBaseChainParams::REGTEST) {
        if (fMintChange)
            throw JSONRPCError(RPC_WALLET_ERROR, "zWGR minting is DISABLED (except for regtest), cannot mint change");

        if (!isPublicSpend)
            throw JSONRPCError(RPC_WALLET_ERROR, "zWGR old spend only available in regtest for tests purposes");
    }

    int64_t nTimeStart = GetTimeMillis();
    CBitcoinAddress address = CBitcoinAddress(); // Optional sending address. Dummy initialization here.
    CWalletTx wtx;
    CZerocoinSpendReceipt receipt;
    bool fSuccess;

    std::list<std::pair<CBitcoinAddress*, CAmount>> outputs;
    if(address_str != "") { // Spend to supplied destination address
        address = CBitcoinAddress(address_str);
        if(!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid WAGERR address");
        outputs.push_back(std::pair<CBitcoinAddress*, CAmount>(&address, nAmount));
    }

    EnsureWalletIsUnlocked();
    fSuccess = pwalletMain->SpendZerocoin(nAmount, wtx, receipt, vMintsSelected, fMintChange, fMinimizeChange, outputs, nullptr, isPublicSpend);

    if (!fSuccess)
        throw JSONRPCError(RPC_WALLET_ERROR, receipt.GetStatusMessage());

    CAmount nValueIn = 0;
    UniValue arrSpends(UniValue::VARR);
    for (CZerocoinSpend spend : receipt.GetSpends()) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("denomination", spend.GetDenomination()));
        obj.push_back(Pair("pubcoin", spend.GetPubCoin().GetHex()));
        obj.push_back(Pair("serial", spend.GetSerial().GetHex()));
        uint32_t nChecksum = spend.GetAccumulatorChecksum();
        obj.push_back(Pair("acc_checksum", HexStr(BEGIN(nChecksum), END(nChecksum))));
        arrSpends.push_back(obj);
        nValueIn += libzerocoin::ZerocoinDenominationToAmount(spend.GetDenomination());
    }

    CAmount nValueOut = 0;
    UniValue vout(UniValue::VARR);
    for (unsigned int i = 0; i < wtx.vout.size(); i++) {
        const CTxOut& txout = wtx.vout[i];
        UniValue out(UniValue::VOBJ);
        out.push_back(Pair("value", ValueFromAmount(txout.nValue)));
        nValueOut += txout.nValue;

        CTxDestination dest;
        if(txout.IsZerocoinMint())
            out.push_back(Pair("address", "zerocoinmint"));
        else if(ExtractDestination(txout.scriptPubKey, dest))
            out.push_back(Pair("address", CBitcoinAddress(dest).ToString()));
        vout.push_back(out);
    }

    //construct JSON to return
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("txid", wtx.GetHash().ToString()));
    ret.push_back(Pair("bytes", (int64_t)wtx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION)));
    ret.push_back(Pair("fee", ValueFromAmount(nValueIn - nValueOut)));
    ret.push_back(Pair("duration_millis", (GetTimeMillis() - nTimeStart)));
    ret.push_back(Pair("spends", arrSpends));
    ret.push_back(Pair("outputs", vout));

    return ret;
}

UniValue resetmintzerocoin(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "resetmintzerocoin ( fullscan )\n"
            "\nScan the blockchain for all of the zerocoins that are held in the wallet.dat.\n"
            "Update any meta-data that is incorrect. Archive any mints that are not able to be found.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. fullscan          (boolean, optional) Rescan each block of the blockchain.\n"
            "                               WARNING - may take 30+ minutes!\n"

            "\nResult:\n"
            "{\n"
            "  \"updated\": [       (array) JSON array of updated mints.\n"
            "    \"xxx\"            (string) Hex encoded mint.\n"
            "    ,...\n"
            "  ],\n"
            "  \"archived\": [      (array) JSON array of archived mints.\n"
            "    \"xxx\"            (string) Hex encoded mint.\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("resetmintzerocoin", "true") + HelpExampleRpc("resetmintzerocoin", "true"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    CzWGRTracker* zwgrTracker = pwalletMain->zwgrTracker.get();
    std::set<CMintMeta> setMints = zwgrTracker->ListMints(false, false, true);
    std::vector<CMintMeta> vMintsToFind(setMints.begin(), setMints.end());
    std::vector<CMintMeta> vMintsMissing;
    std::vector<CMintMeta> vMintsToUpdate;

    // search all of our available data for these mints
    FindMints(vMintsToFind, vMintsToUpdate, vMintsMissing);

    // update the meta data of mints that were marked for updating
    UniValue arrUpdated(UniValue::VARR);
    for (CMintMeta meta : vMintsToUpdate) {
        zwgrTracker->UpdateState(meta);
        arrUpdated.push_back(meta.hashPubcoin.GetHex());
    }

    // delete any mints that were unable to be located on the blockchain
    UniValue arrDeleted(UniValue::VARR);
    for (CMintMeta mint : vMintsMissing) {
        zwgrTracker->Archive(mint);
        arrDeleted.push_back(mint.hashPubcoin.GetHex());
    }

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("updated", arrUpdated));
    obj.push_back(Pair("archived", arrDeleted));
    return obj;
}

UniValue resetspentzerocoin(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "resetspentzerocoin\n"
            "\nScan the blockchain for all of the zerocoins that are held in the wallet.dat.\n"
            "Reset mints that are considered spent that did not make it into the blockchain.\n"

            "\nResult:\n"
            "{\n"
            "  \"restored\": [        (array) JSON array of restored objects.\n"
            "    {\n"
            "      \"serial\": \"xxx\"  (string) Serial in hex format.\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("resetspentzerocoin", "") + HelpExampleRpc("resetspentzerocoin", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    CzWGRTracker* zwgrTracker = pwalletMain->zwgrTracker.get();
    std::set<CMintMeta> setMints = zwgrTracker->ListMints(false, false, false);
    std::list<CZerocoinSpend> listSpends = walletdb.ListSpentCoins();
    std::list<CZerocoinSpend> listUnconfirmedSpends;

    for (CZerocoinSpend spend : listSpends) {
        CTransaction tx;
        uint256 hashBlock = 0;
        if (!GetTransaction(spend.GetTxHash(), tx, hashBlock)) {
            listUnconfirmedSpends.push_back(spend);
            continue;
        }

        //no confirmations
        if (hashBlock == 0)
            listUnconfirmedSpends.push_back(spend);
    }

    UniValue objRet(UniValue::VOBJ);
    UniValue arrRestored(UniValue::VARR);
    for (CZerocoinSpend spend : listUnconfirmedSpends) {
        for (auto& meta : setMints) {
            if (meta.hashSerial == GetSerialHash(spend.GetSerial())) {
                zwgrTracker->SetPubcoinNotUsed(meta.hashPubcoin);
                walletdb.EraseZerocoinSpendSerialEntry(spend.GetSerial());
                RemoveSerialFromDB(spend.GetSerial());
                UniValue obj(UniValue::VOBJ);
                obj.push_back(Pair("serial", spend.GetSerial().GetHex()));
                arrRestored.push_back(obj);
                continue;
            }
        }
    }

    objRet.push_back(Pair("restored", arrRestored));
    return objRet;
}

UniValue getarchivedzerocoin(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 0)
        throw std::runtime_error(
            "getarchivedzerocoin\n"
            "\nDisplay zerocoins that were archived because they were believed to be orphans.\n"
            "Provides enough information to recover mint if it was incorrectly archived.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\": \"xxx\",           (string) Transaction ID for archived mint.\n"
            "    \"denomination\": amount,  (numeric) Denomination value.\n"
            "    \"serial\": \"xxx\",         (string) Serial number in hex format.\n"
            "    \"randomness\": \"xxx\",     (string) Hex encoded randomness.\n"
            "    \"pubcoin\": \"xxx\"         (string) Pubcoin in hex format.\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getarchivedzerocoin", "") + HelpExampleRpc("getarchivedzerocoin", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    std::list<CZerocoinMint> listMints = walletdb.ListArchivedZerocoins();
    std::list<CDeterministicMint> listDMints = walletdb.ListArchivedDeterministicMints();

    UniValue arrRet(UniValue::VARR);
    for (const CZerocoinMint& mint : listMints) {
        UniValue objMint(UniValue::VOBJ);
        objMint.push_back(Pair("txid", mint.GetTxHash().GetHex()));
        objMint.push_back(Pair("denomination", ValueFromAmount(mint.GetDenominationAsAmount())));
        objMint.push_back(Pair("serial", mint.GetSerialNumber().GetHex()));
        objMint.push_back(Pair("randomness", mint.GetRandomness().GetHex()));
        objMint.push_back(Pair("pubcoin", mint.GetValue().GetHex()));
        arrRet.push_back(objMint);
    }

    for (const CDeterministicMint& dMint : listDMints) {
        UniValue objDMint(UniValue::VOBJ);
        objDMint.push_back(Pair("txid", dMint.GetTxHash().GetHex()));
        objDMint.push_back(Pair("denomination", ValueFromAmount(libzerocoin::ZerocoinDenominationToAmount(dMint.GetDenomination()))));
        objDMint.push_back(Pair("serialhash", dMint.GetSerialHash().GetHex()));
        objDMint.push_back(Pair("pubcoinhash", dMint.GetPubcoinHash().GetHex()));
        objDMint.push_back(Pair("seedhash", dMint.GetSeedHash().GetHex()));
        objDMint.push_back(Pair("count", (int64_t)dMint.GetCount()));
        arrRet.push_back(objDMint);
    }

    return arrRet;
}

UniValue exportzerocoins(const UniValue& params, bool fHelp)
{
    if(fHelp || params.empty() || params.size() > 2)
        throw std::runtime_error(
            "exportzerocoins include_spent ( denomination )\n"
            "\nExports zerocoin mints that are held by this wallet.dat\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"include_spent\"        (bool, required) Include mints that have already been spent\n"
            "2. \"denomination\"         (integer, optional) Export a specific denomination of zWGR\n"

            "\nResult:\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"id\": \"serial hash\",  (string) the mint's zWGR serial hash \n"
            "    \"d\": n,         (numeric) the mint's zerocoin denomination \n"
            "    \"p\": \"pubcoin\", (string) The public coin\n"
            "    \"s\": \"serial\",  (string) The secret serial number\n"
            "    \"r\": \"random\",  (string) The secret random number\n"
            "    \"t\": \"txid\",    (string) The txid that the coin was minted in\n"
            "    \"h\": n,         (numeric) The height the tx was added to the blockchain\n"
            "    \"u\": used,      (boolean) Whether the mint has been spent\n"
            "    \"v\": version,   (numeric) The version of the zWGR\n"
            "    \"k\": \"privkey\"  (string) The zWGR private key (V2+ zWGR only)\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("exportzerocoins", "false 5") + HelpExampleRpc("exportzerocoins", "false 5"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    CWalletDB walletdb(pwalletMain->strWalletFile);

    bool fIncludeSpent = params[0].get_bool();
    libzerocoin::CoinDenomination denomination = libzerocoin::ZQ_ERROR;
    if (params.size() == 2)
        denomination = libzerocoin::IntToZerocoinDenomination(params[1].get_int());

    CzWGRTracker* zwgrTracker = pwalletMain->zwgrTracker.get();
    std::set<CMintMeta> setMints = zwgrTracker->ListMints(!fIncludeSpent, false, false);

    UniValue jsonList(UniValue::VARR);
    for (const CMintMeta& meta : setMints) {
        if (denomination != libzerocoin::ZQ_ERROR && denomination != meta.denom)
            continue;

        CZerocoinMint mint;
        if (!pwalletMain->GetMint(meta.hashSerial, mint))
            continue;

        UniValue objMint(UniValue::VOBJ);
        objMint.push_back(Pair("id", meta.hashSerial.GetHex()));
        objMint.push_back(Pair("d", mint.GetDenomination()));
        objMint.push_back(Pair("p", mint.GetValue().GetHex()));
        objMint.push_back(Pair("s", mint.GetSerialNumber().GetHex()));
        objMint.push_back(Pair("r", mint.GetRandomness().GetHex()));
        objMint.push_back(Pair("t", mint.GetTxHash().GetHex()));
        objMint.push_back(Pair("h", mint.GetHeight()));
        objMint.push_back(Pair("u", mint.IsUsed()));
        objMint.push_back(Pair("v", mint.GetVersion()));
        if (mint.GetVersion() >= 2) {
            CKey key;
            key.SetPrivKey(mint.GetPrivKey(), true);
            CBitcoinSecret cBitcoinSecret;
            cBitcoinSecret.SetKey(key);
            objMint.push_back(Pair("k", cBitcoinSecret.ToString()));
        }
        jsonList.push_back(objMint);
    }

    return jsonList;
}

UniValue importzerocoins(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() == 0)
        throw std::runtime_error(
            "importzerocoins importdata \n"
            "\n[{\"d\":denomination,\"p\":\"pubcoin_hex\",\"s\":\"serial_hex\",\"r\":\"randomness_hex\",\"t\":\"txid\",\"h\":height, \"u\":used},{\"d\":...}]\n"
            "\nImport zerocoin mints.\n"
            "Adds raw zerocoin mints to the wallet.dat\n"
            "Note it is recommended to use the json export created from the exportzerocoins RPC call\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"importdata\"    (string, required) A json array of json objects containing zerocoin mints\n"

            "\nResult:\n"
            "{\n"
            "  \"added\": n,        (numeric) The quantity of zerocoin mints that were added\n"
            "  \"value\": amount    (numeric) The total zWGR value of zerocoin mints that were added\n"
            "}\n"

            "\nExamples\n" +
            HelpExampleCli("importzerocoins", "\'[{\"d\":100,\"p\":\"mypubcoin\",\"s\":\"myserial\",\"r\":\"randomness_hex\",\"t\":\"mytxid\",\"h\":104923, \"u\":false},{\"d\":5,...}]\'") +
            HelpExampleRpc("importzerocoins", "[{\"d\":100,\"p\":\"mypubcoin\",\"s\":\"myserial\",\"r\":\"randomness_hex\",\"t\":\"mytxid\",\"h\":104923, \"u\":false},{\"d\":5,...}]"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VARR)(UniValue::VOBJ));
    UniValue arrMints = params[0].get_array();
    CWalletDB walletdb(pwalletMain->strWalletFile);

    int count = 0;
    CAmount nValue = 0;
    for (unsigned int idx = 0; idx < arrMints.size(); idx++) {
        const UniValue &val = arrMints[idx];
        const UniValue &o = val.get_obj();

        const UniValue& vDenom = find_value(o, "d");
        if (!vDenom.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing d key");
        int d = vDenom.get_int();
        if (d < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, d must be positive");

        libzerocoin::CoinDenomination denom = libzerocoin::IntToZerocoinDenomination(d);
        CBigNum bnValue = 0;
        bnValue.SetHex(find_value(o, "p").get_str());
        CBigNum bnSerial = 0;
        bnSerial.SetHex(find_value(o, "s").get_str());
        CBigNum bnRandom = 0;
        bnRandom.SetHex(find_value(o, "r").get_str());
        uint256 txid(find_value(o, "t").get_str());

        int nHeight = find_value(o, "h").get_int();
        if (nHeight < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, h must be positive");

        bool fUsed = find_value(o, "u").get_bool();

        //Assume coin is version 1 unless it has the version actually set
        uint8_t nVersion = 1;
        const UniValue& vVersion = find_value(o, "v");
        if (vVersion.isNum())
            nVersion = static_cast<uint8_t>(vVersion.get_int());

        //Set the privkey if applicable
        CPrivKey privkey;
        if (nVersion >= libzerocoin::PrivateCoin::PUBKEY_VERSION) {
            std::string strPrivkey = find_value(o, "k").get_str();
            CBitcoinSecret vchSecret;
            bool fGood = vchSecret.SetString(strPrivkey);
            CKey key = vchSecret.GetKey();
            if (!key.IsValid() && fGood)
                return JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "privkey is not valid");
            privkey = key.GetPrivKey();
        }

        CZerocoinMint mint(denom, bnValue, bnRandom, bnSerial, fUsed, nVersion, &privkey);
        mint.SetTxHash(txid);
        mint.SetHeight(nHeight);
        pwalletMain->zwgrTracker->Add(mint, true);
        count++;
        nValue += libzerocoin::ZerocoinDenominationToAmount(denom);
    }

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("added", count));
    ret.push_back(Pair("value", ValueFromAmount(nValue)));
    return ret;
}

UniValue reconsiderzerocoins(const UniValue& params, bool fHelp)
{
    if(fHelp || !params.empty())
        throw std::runtime_error(
            "reconsiderzerocoins\n"
            "\nCheck archived zWGR list to see if any mints were added to the blockchain.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"xxx\",           (string) the mint's zerocoin denomination \n"
            "    \"denomination\" : amount,  (numeric) the mint's zerocoin denomination\n"
            "    \"pubcoin\" : \"xxx\",        (string) The mint's public identifier\n"
            "    \"height\" : n              (numeric) The height the tx was added to the blockchain\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n" +
            HelpExampleCli("reconsiderzerocoins", "") + HelpExampleRpc("reconsiderzerocoins", ""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked(true);

    std::list<CZerocoinMint> listMints;
    std::list<CDeterministicMint> listDMints;
    pwalletMain->ReconsiderZerocoins(listMints, listDMints);

    UniValue arrRet(UniValue::VARR);
    for (const CZerocoinMint& mint : listMints) {
        UniValue objMint(UniValue::VOBJ);
        objMint.push_back(Pair("txid", mint.GetTxHash().GetHex()));
        objMint.push_back(Pair("denomination", ValueFromAmount(mint.GetDenominationAsAmount())));
        objMint.push_back(Pair("pubcoin", mint.GetValue().GetHex()));
        objMint.push_back(Pair("height", mint.GetHeight()));
        arrRet.push_back(objMint);
    }
    for (const CDeterministicMint& dMint : listDMints) {
        UniValue objMint(UniValue::VOBJ);
        objMint.push_back(Pair("txid", dMint.GetTxHash().GetHex()));
        objMint.push_back(Pair("denomination", FormatMoney(libzerocoin::ZerocoinDenominationToAmount(dMint.GetDenomination()))));
        objMint.push_back(Pair("pubcoinhash", dMint.GetPubcoinHash().GetHex()));
        objMint.push_back(Pair("height", dMint.GetHeight()));
        arrRet.push_back(objMint);
    }

    return arrRet;
}

UniValue setzwgrseed(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 1)
        throw std::runtime_error(
            "setzwgrseed \"seed\"\n"
            "\nSet the wallet's deterministic zwgr seed to a specific value.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"seed\"        (string, required) The deterministic zwgr seed.\n"

            "\nResult\n"
            "\"success\" : b,  (boolean) Whether the seed was successfully set.\n"

            "\nExamples\n" +
            HelpExampleCli("setzwgrseed", "63f793e7895dd30d99187b35fbfb314a5f91af0add9e0a4e5877036d1e392dd5") +
            HelpExampleRpc("setzwgrseed", "63f793e7895dd30d99187b35fbfb314a5f91af0add9e0a4e5877036d1e392dd5"));

    EnsureWalletIsUnlocked();

    uint256 seed;
    seed.SetHex(params[0].get_str());

    CzWGRWallet* zwallet = pwalletMain->getZWallet();
    bool fSuccess = zwallet->SetMasterSeed(seed, true);
    if (fSuccess)
        zwallet->SyncWithChain();

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("success", fSuccess));

    return ret;
}

UniValue getzwgrseed(const UniValue& params, bool fHelp)
{
    if(fHelp || !params.empty())
        throw std::runtime_error(
            "getzwgrseed\n"
            "\nCheck archived zWGR list to see if any mints were added to the blockchain.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nResult\n"
            "\"seed\" : s,  (string) The deterministic zWGR seed.\n"

            "\nExamples\n" +
            HelpExampleCli("getzwgrseed", "") + HelpExampleRpc("getzwgrseed", ""));

    EnsureWalletIsUnlocked();

    CzWGRWallet* zwallet = pwalletMain->getZWallet();
    uint256 seed = zwallet->GetMasterSeed();

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("seed", seed.GetHex()));

    return ret;
}

UniValue generatemintlist(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 2)
        throw std::runtime_error(
            "generatemintlist\n"
            "\nShow mints that are derived from the deterministic zWGR seed.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments\n"
            "1. \"count\"  : n,  (numeric) Which sequential zWGR to start with.\n"
            "2. \"range\"  : n,  (numeric) How many zWGR to generate.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"count\": n,          (numeric) Deterministic Count.\n"
            "    \"value\": \"xxx\",    (string) Hex encoded pubcoin value.\n"
            "    \"randomness\": \"xxx\",   (string) Hex encoded randomness.\n"
            "    \"serial\": \"xxx\"        (string) Hex encoded Serial.\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n" +
            HelpExampleCli("generatemintlist", "1, 100") + HelpExampleRpc("generatemintlist", "1, 100"));

    EnsureWalletIsUnlocked();

    int nCount = params[0].get_int();
    int nRange = params[1].get_int();
    CzWGRWallet* zwallet = pwalletMain->zwalletMain;

    UniValue arrRet(UniValue::VARR);
    for (int i = nCount; i < nCount + nRange; i++) {
        libzerocoin::CoinDenomination denom = libzerocoin::CoinDenomination::ZQ_ONE;
        libzerocoin::PrivateCoin coin(Params().Zerocoin_Params(false), denom, false);
        CDeterministicMint dMint;
        zwallet->GenerateMint(i, denom, coin, dMint);
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("count", i));
        obj.push_back(Pair("value", coin.getPublicCoin().getValue().GetHex()));
        obj.push_back(Pair("randomness", coin.getRandomness().GetHex()));
        obj.push_back(Pair("serial", coin.getSerialNumber().GetHex()));
        arrRet.push_back(obj);
    }

    return arrRet;
}

UniValue dzwgrstate(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "dzwgrstate\n"
                        "\nThe current state of the mintpool of the deterministic zWGR wallet.\n" +
                HelpRequiringPassphrase() + "\n"

                        "\nExamples\n" +
                HelpExampleCli("mintpoolstatus", "") + HelpExampleRpc("mintpoolstatus", ""));

    CzWGRWallet* zwallet = pwalletMain->zwalletMain;
    UniValue obj(UniValue::VOBJ);
    int nCount, nCountLastUsed;
    zwallet->GetState(nCount, nCountLastUsed);
    obj.push_back(Pair("dzwgr_count", nCount));
    obj.push_back(Pair("mintpool_count", nCountLastUsed));

    return obj;
}


void static SearchThread(CzWGRWallet* zwallet, int nCountStart, int nCountEnd)
{
    LogPrintf("%s: start=%d end=%d\n", __func__, nCountStart, nCountEnd);
    CWalletDB walletDB(pwalletMain->strWalletFile);
    try {
        uint256 seedMaster = zwallet->GetMasterSeed();
        uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
        for(int i = nCountStart; i < nCountEnd; i++) {
            boost::this_thread::interruption_point();
            CDataStream ss(SER_GETHASH, 0);
            ss << seedMaster << i;
            uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());

            CBigNum bnValue;
            CBigNum bnSerial;
            CBigNum bnRandomness;
            CKey key;
            zwallet->SeedToZWGR(zerocoinSeed, bnValue, bnSerial, bnRandomness, key);

            uint256 hashPubcoin = GetPubCoinHash(bnValue);
            zwallet->AddToMintPool(std::make_pair(hashPubcoin, i), true);
            walletDB.WriteMintPoolPair(hashSeed, hashPubcoin, i);
        }
    } catch (const std::exception& e) {
        LogPrintf("SearchThread() exception");
    } catch (...) {
        LogPrintf("SearchThread() exception");
    }
}

UniValue searchdzwgr(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 3)
        throw std::runtime_error(
            "searchdzwgr\n"
            "\nMake an extended search for deterministically generated zWGR that have not yet been recognized by the wallet.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments\n"
            "1. \"count\"       (numeric) Which sequential zWGR to start with.\n"
            "2. \"range\"       (numeric) How many zWGR to generate.\n"
            "3. \"threads\"     (numeric) How many threads should this operation consume.\n"

            "\nExamples\n" +
            HelpExampleCli("searchdzwgr", "1, 100, 2") + HelpExampleRpc("searchdzwgr", "1, 100, 2"));

    EnsureWalletIsUnlocked();

    int nCount = params[0].get_int();
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Count cannot be less than 0");

    int nRange = params[1].get_int();
    if (nRange < 1)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Range has to be at least 1");

    int nThreads = params[2].get_int();

    CzWGRWallet* zwallet = pwalletMain->zwalletMain;

    boost::thread_group* dzwgrThreads = new boost::thread_group();
    int nRangePerThread = nRange / nThreads;

    int nPrevThreadEnd = nCount - 1;
    for (int i = 0; i < nThreads; i++) {
        int nStart = nPrevThreadEnd + 1;;
        int nEnd = nStart + nRangePerThread;
        nPrevThreadEnd = nEnd;
        dzwgrThreads->create_thread(boost::bind(&SearchThread, zwallet, nStart, nEnd));
    }

    dzwgrThreads->join_all();

    zwallet->RemoveMintsFromPool(pwalletMain->zwgrTracker->GetSerialHashes());
    zwallet->SyncWithChain(false);

    //todo: better response
    return "done";
}

UniValue enableautomintaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "enableautomintaddress enable\n"
                "\nEnables or disables automint address functionality\n"

                "\nArguments\n"
                "1. enable     (boolean, required) Enable or disable automint address functionality\n"

                "\nExamples\n" +
                HelpExampleCli("enableautomintaddress", "true") + HelpExampleRpc("enableautomintaddress", "false"));

    fEnableAutoConvert = params[0].get_bool();

    return NullUniValue;
}

UniValue createautomintaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "createautomintaddress\n"
                "\nGenerates new auto mint address\n" +
                HelpRequiringPassphrase() + "\n"

                "\nResult\n"
                "\"address\"     (string) WAGERR address for auto minting\n" +
                HelpExampleCli("createautomintaddress", "") +
                HelpExampleRpc("createautomintaddress", ""));

    EnsureWalletIsUnlocked();
    LOCK(pwalletMain->cs_wallet);
    CBitcoinAddress address = pwalletMain->GenerateNewAutoMintKey();
    return address.ToString();
}

UniValue spendrawzerocoin(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 7)
        throw std::runtime_error(
            "spendrawzerocoin \"serialHex\" denom \"randomnessHex\" \"priv key\" ( \"address\" \"mintTxId\" isPublicSpend)\n"
            "\nCreate and broadcast a TX spending the provided zericoin.\n"

            "\nArguments:\n"
            "1. \"serialHex\"        (string, required) A zerocoin serial number (hex)\n"
            "2. \"randomnessHex\"    (string, required) A zerocoin randomness value (hex)\n"
            "3. denom                (numeric, required) A zerocoin denomination (decimal)\n"
            "4. \"priv key\"         (string, required) The private key associated with this coin (hex)\n"
            "5. \"address\"          (string, optional) WAGERR address to spend to. If not specified, "
            "                        or empty string, spend to change address.\n"
            "6. \"mintTxId\"         (string, optional) txid of the transaction containing the mint. If not"
            "                        specified, or empty string, the blockchain will be scanned (could take a while)"
            "7. isPublicSpend        (boolean, optional, default=true) create a public zc spend."
            "                        If false, instead create spend version 2 (only for regression tests)"

            "\nResult:\n"
                "\"txid\"             (string) The transaction txid in hex\n"

            "\nExamples\n" +
            HelpExampleCli("spendrawzerocoin", "\"f80892e78c30a393ef4ab4d5a9d5a2989de6ebc7b976b241948c7f489ad716a2\" \"a4fd4d7248e6a51f1d877ddd2a4965996154acc6b8de5aa6c83d4775b283b600\" 100 \"xxx\"") +
            HelpExampleRpc("spendrawzerocoin", "\"f80892e78c30a393ef4ab4d5a9d5a2989de6ebc7b976b241948c7f489ad716a2\", \"a4fd4d7248e6a51f1d877ddd2a4965996154acc6b8de5aa6c83d4775b283b600\", 100, \"xxx\""));

    const bool isPublicSpend = (params.size() > 6 ? params[6].get_bool() : true);
    if (Params().NetworkID() != CBaseChainParams::REGTEST && !isPublicSpend)
        throw JSONRPCError(RPC_WALLET_ERROR, "zWGR old spend only available in regtest for tests purposes");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (sporkManager.IsSporkActive(SPORK_16_ZEROCOIN_MAINTENANCE_MODE))
            throw JSONRPCError(RPC_WALLET_ERROR, "zWGR is currently disabled due to maintenance.");

    CBigNum serial;
    serial.SetHex(params[0].get_str());

    CBigNum randomness;
    randomness.SetHex(params[1].get_str());

    const int denom_int = params[2].get_int();
    libzerocoin::CoinDenomination denom = libzerocoin::IntToZerocoinDenomination(denom_int);

    std::string priv_key_str = params[3].get_str();
    CPrivKey privkey;
    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(priv_key_str);
    CKey key = vchSecret.GetKey();
    if (!key.IsValid() && fGood)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "privkey is not valid");
    privkey = key.GetPrivKey();

    // Create the coin associated with these secrets
    libzerocoin::PrivateCoin coin(Params().Zerocoin_Params(false), denom, serial, randomness);
    coin.setPrivKey(privkey);
    coin.setVersion(libzerocoin::PrivateCoin::CURRENT_VERSION);

    // Create the mint associated with this coin
    CZerocoinMint mint(denom, coin.getPublicCoin().getValue(), randomness, serial, false, CZerocoinMint::CURRENT_VERSION, &privkey);

    std::string address_str = "";
    if (params.size() > 4)
        address_str = params[4].get_str();

    if (params.size() > 5) {
        // update mint txid
        mint.SetTxHash(ParseHashV(params[5], "parameter 5"));
    } else {
        // If the mint tx is not provided, look for it
        const CBigNum& mintValue = mint.GetValue();
        bool found = false;
        {
            CBlockIndex* pindex = chainActive.Tip();
            while (!found && pindex && pindex->nHeight >= Params().Zerocoin_StartHeight()) {
                LogPrintf("%s : Checking block %d...\n", __func__, pindex->nHeight);
                if (pindex->MintedDenomination(denom)) {
                    CBlock block;
                    if (!ReadBlockFromDisk(block, pindex))
                        throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to read block from disk");
                    std::list<CZerocoinMint> listMints;
                    BlockToZerocoinMintList(block, listMints, true);
                    for (const CZerocoinMint& m : listMints) {
                        if (m.GetValue() == mintValue && m.GetDenomination() == denom) {
                            // mint found. update txid
                            mint.SetTxHash(m.GetTxHash());
                            found = true;
                            break;
                        }
                    }
                }
                pindex = pindex->pprev;
            }
        }
        if (!found)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Mint tx not found");
    }

    std::vector<CZerocoinMint> vMintsSelected = {mint};
    return DoZwgrSpend(mint.GetDenominationAsAmount(), false, true, vMintsSelected, address_str, isPublicSpend);
}
