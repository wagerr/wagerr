// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h"
#include "main.h"
#include "betting/bet.h"
#include "betting/bet_db.h"
#include "rpc/server.h"
#include <boost/assign/list_of.hpp>

#include <univalue.h>

/**
 * Looks up a given map index for a given name. If found then it will return the mapping ID.
 * If its not found then create a new mapping ID and also indicate with a boolean that a new
 * mapping OP_CODE needs to be created and broadcast to the network.
 *
 * @param params The RPC params consisting of an map index name and name.
 * @param fHelp  Help text
 * @return
 */
UniValue getmappingid(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() < 2))
        throw std::runtime_error(
                "getmappingid\n"
                "\nGet a mapping ID from the specified mapping index.\n"

                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"mapping index id\": \"xxx\",  (numeric) The mapping index.\n"
                "    \"exists\": \"xxx\", (boolean) mapping id exists\n"
                "    \"mapping-index\": \"xxx\" (string) The index that was searched.\n"
                "  }\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmappingid", "\"sport\" \"Football\"") + HelpExampleRpc("getmappingid", "\"sport\" \"Football\""));

    const std::string name{params[1].get_str()};
    const std::string mIndex{params[0].get_str()};
    const MappingType type{CMappingDB::FromTypeName(mIndex)};
    UniValue result{UniValue::VARR};
    UniValue mappings{UniValue::VOBJ};

    if (static_cast<int>(type) < 0 || CMappingDB::ToTypeName(type) != mIndex) {
        throw std::runtime_error("No mapping exist for the mapping index you provided.");
    }

    bool mappingFound{false};

    LOCK(cs_main);

    // Check the map for the string name.
    auto it = bettingsView->mappings->NewIterator();
    MappingKey key;
    for (it->Seek(CBettingDB::DbTypeToBytes(MappingKey{type, 0})); it->Valid() && (CBettingDB::BytesToDbType(it->Key(), key), key.nMType == type); it->Next()) {
        CMappingDB mapping{};
        CBettingDB::BytesToDbType(it->Value(), mapping);
        LogPrint("wagerr", "%s - mapping - it=[%d,%d] nId=[%d] nMType=[%s] [%s]\n", __func__, key.nMType, key.nId, key.nId, CMappingDB::ToTypeName(key.nMType), mapping.sName);
        if (!mappingFound) {
            if (mapping.sName == name) {
                mappings.push_back(Pair("mapping-id", (uint64_t) key.nId));
                mappings.push_back(Pair("exists", true));
                mappings.push_back(Pair("mapping-index", mIndex));
                mappingFound = true;
            }
        }
    }
    if (mappingFound)
        result.push_back(mappings);

    return result;
}

/**
 * Looks up a given map index for a given ID. If found then it will return the mapping name.
 * If its not found return an error message.
 *
 * @param params The RPC params consisting of an map index name and id.
 * @param fHelp  Help text
 * @return
 */
UniValue getmappingname(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 2))
        throw std::runtime_error(
                "getmappingname\n"
                "\nGet a mapping string name from the specified map index.\n"
                "1. Mapping type  (string, requied) Type of mapping (\"sports\", \"rounds\", \"teams\", \"tournaments\", \"individualSports\", \"contenders\").\n"
                "2. Mapping id    (numeric, requied) Mapping id.\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"mapping-type\": \"xxx\",  (string) The mapping type.\n"
                "    \"mapping-name\": \"xxx\",  (string) The mapping name.\n"
                "    \"exists\": \"xxx\", (boolean) mapping transaction created or not\n"
                "    \"mapping-index\": \"xxx\" (string) The index that was searched.\n"
                "  }\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmappingname", "\"sport\" 0") + HelpExampleRpc("getmappingname", "\"sport\" 0"));

    const std::string mIndex{params[0].get_str()};
    const uint32_t id{static_cast<uint32_t>(params[1].get_int())};
    const MappingType type{CMappingDB::FromTypeName(mIndex)};
    UniValue result{UniValue::VARR};
    UniValue mapping{UniValue::VOBJ};

    if (CMappingDB::ToTypeName(type) != mIndex) {
        throw std::runtime_error("No mapping exist for the mapping index you provided.");
    }

    LOCK(cs_main);

    CMappingDB map{};
    if (bettingsView->mappings->Read(MappingKey{type, id}, map)) {
        mapping.push_back(Pair("mapping-type", CMappingDB::ToTypeName(type)));
        mapping.push_back(Pair("mapping-name", map.sName));
        mapping.push_back(Pair("exists", true));
        mapping.push_back(Pair("mapping-index", static_cast<uint64_t>(id)));
    }

    result.push_back(mapping);

    return result;
}

std::string GetPayoutTypeStr(PayoutType type)
{
    switch(type) {
        case PayoutType::bettingPayout:
            return std::string("Betting Payout");
        case PayoutType::bettingRefund:
            return std::string("Betting Refund");
        case PayoutType::bettingReward:
            return std::string("Betting Reward");
        case PayoutType::chainGamesPayout:
            return std::string("Chain Games Payout");
        case PayoutType::chainGamesRefund:
            return std::string("Chain Games Refund");
        case PayoutType::chainGamesReward:
            return std::string("Chain Games Reward");
        default:
            return std::string("Undefined Payout Type");
    }
}

UniValue CreatePayoutInfoResponse(const std::vector<std::pair<bool, CPayoutInfoDB>> vPayoutsInfo)
{
    UniValue responseArr{UniValue::VARR};
    for (auto info : vPayoutsInfo) {
        UniValue retObj{UniValue::VOBJ};
        if (info.first) { // if payout info was found - add info to array
            CPayoutInfoDB &payoutInfo = info.second;
            UniValue infoObj{UniValue::VOBJ};

            infoObj.push_back(Pair("payoutType", GetPayoutTypeStr(payoutInfo.payoutType)));
            infoObj.push_back(Pair("betBlockHeight", (uint64_t) payoutInfo.betKey.blockHeight));
            infoObj.push_back(Pair("betTxHash", payoutInfo.betKey.outPoint.hash.GetHex()));
            infoObj.push_back(Pair("betTxOut", (uint64_t) payoutInfo.betKey.outPoint.n));
            retObj.push_back(Pair("found", UniValue{true}));
            retObj.push_back(Pair("payoutInfo", infoObj));
        }
        else {
            retObj.push_back(Pair("found", UniValue{false}));
            retObj.push_back(Pair("payoutInfo", UniValue{UniValue::VOBJ}));
        }

        responseArr.push_back(retObj);
    }
    return responseArr;
}

/**
 * Looks up a given payout tx hash and out number for getting payout info.
 * If not found return an empty array. If found - return array of info objects.
 *
 * @param params The RPC params consisting of an array of objects
 * @param fHelp  Help text
 * @return
 */
UniValue getpayoutinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 1))
        throw std::runtime_error(
                "getpayoutinfo\n"
                "\nGet an info for given  .\n"
                "1. Payout params  (array, requied)\n"
                "[\n"
                "  {\n"
                "    \"txHash\": hash (string, requied) The payout transaction hash.\n"
                "    \"nOut\": nOut (numeric, requied) The payout transaction out number.\n"
                "  }\n"
                "]\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"found\": flag (boolean) Indicate that expected payout was found.\n"
                "    \"payoutInfo\": object (object) Payout info object.\n"
                "      {\n"
                "        \"payoutType\": payoutType (string) Payout type: bet or chain game, payout or refund or reward.\n"
                "        \"betHeight\": height (numeric) Bet block height.\n"
                "        \"betTxHash\": hash (string) Bet transaction hash.\n"
                "        \"betOut\": nOut (numeric) Bet transaction out number.\n"
                "      }\n"
                "  }\n"
                "]\n"
                "\nExamples:\n" +
                HelpExampleCli("getpayoutinfo", "[{\"txHash\": 08746e1bdb6f4aebd7f1f3da25ac11e1cd3cacaf34cd2ad144e376b2e7f74d49, \"nOut\": 3}, {\"txHash\": 4c1e6b1a26808541e9e43c542adcc0eb1c67f2be41f2334ab1436029bf1791c0, \"nOut\": 4}]") +
                    HelpExampleRpc("getpayoutinfo", "[{\"txHash\": 08746e1bdb6f4aebd7f1f3da25ac11e1cd3cacaf34cd2ad144e376b2e7f74d49, \"nOut\": 3}, {\"txHash\": 4c1e6b1a26808541e9e43c542adcc0eb1c67f2be41f2334ab1436029bf1791c0, \"nOut\": 4}]"));

    UniValue paramsArr = params[0].get_array();
    std::vector<std::pair<bool, CPayoutInfoDB>> vPayoutsInfo;

    LOCK(cs_main);

    // parse payout params
    for (uint32_t i = 0; i < paramsArr.size(); i++) {
        const UniValue obj = paramsArr[i].get_obj();
        RPCTypeCheckObj(obj, boost::assign::map_list_of("txHash", UniValue::VSTR)("nOut", UniValue::VNUM));
        uint256 txHash = uint256(find_value(obj, "txHash").get_str());
        uint32_t nOut = find_value(obj, "nOut").get_int();
        uint256 hashBlock;
        CTransaction tx;
        if (!GetTransaction(txHash, tx, hashBlock, true)) {
            vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{false, CPayoutInfoDB{}});
            continue;
        }
        if (hashBlock == 0) { // uncomfirmed tx
            vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{false, CPayoutInfoDB{}});
            continue;
        }
        uint32_t blockHeight = mapBlockIndex.at(hashBlock)->nHeight;

        CPayoutInfoDB payoutInfo;
        // try to find payout info from db
        if (!bettingsView->payoutsInfo->Read(PayoutInfoKey{blockHeight, COutPoint{txHash, nOut}}, payoutInfo)) {
            // not found
            vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{false, CPayoutInfoDB{}});
            continue;
        }
        vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{true, payoutInfo});
    }

    return CreatePayoutInfoResponse(vPayoutsInfo);
}

/**
 * Looks up a given block height for getting payouts info since this block height.
 * If not found return an empty array. If found - return array of info objects.
 *
 * @param params The RPC params consisting of an array of objects
 * @param fHelp  Help text
 * @return
 */
UniValue getpayoutinfosince(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 1))
        throw std::runtime_error(
                "getpayoutinfosince\n"
                "\nGet info for payouts in the specified block range.\n"
                "1. Last blocks (numeric, optional) default = 10.\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"found\": flag (boolean) Indicate that expected payout was found.\n"
                "    \"payoutInfo\": object (object) Payout info object.\n"
                "      {\n"
                "        \"payoutType\": payoutType (string) Payout type: bet or chain game, payout or refund or reward.\n"
                "        \"betHeight\": height (numeric) Bet block height.\n"
                "        \"betTxHash\": hash (string) Bet transaction hash.\n"
                "        \"betOut\": nOut (numeric) Bet transaction out number.\n"
                "      }\n"
                "  }\n"
                "]\n"
                "\nExamples:\n" +
                HelpExampleCli("getpayoutinfosince", "15") + HelpExampleRpc("getpayoutinfosince", "15"));

    std::vector<std::pair<bool, CPayoutInfoDB>> vPayoutsInfo;
    uint32_t nLastBlocks = 10;
    if (params.size() == 1) {
        nLastBlocks = params[0].get_int();
        if (nLastBlocks < 1)
            throw std::runtime_error("Invalid number of last blocks.");
    }

    LOCK(cs_main);

    int nCurrentHeight = chainActive.Height();

    uint32_t startBlockHeight = static_cast<uint32_t>(nCurrentHeight) - nLastBlocks + 1;

    auto it = bettingsView->payoutsInfo->NewIterator();
    for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{startBlockHeight, COutPoint()})); it->Valid(); it->Next()) {
        PayoutInfoKey key;
        CPayoutInfoDB payoutInfo;
        CBettingDB::BytesToDbType(it->Key(), key);
        CBettingDB::BytesToDbType(it->Value(), payoutInfo);
        vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{true, payoutInfo});
    }

    return CreatePayoutInfoResponse(vPayoutsInfo);
}