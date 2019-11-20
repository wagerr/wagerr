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
#include "rpc/server.h"

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
                HelpExampleCli("getmappingid", "") + HelpExampleRpc("getmappingid", ""));

    const std::string name{params[1].get_str()};
    const std::string mIndex{params[0].get_str()};
    const MappingTypes type{CMapping::FromTypeName(mIndex)};
    UniValue result{UniValue::VARR};
    UniValue mappings{UniValue::VOBJ};

    if (static_cast<int>(type) < 0 || CMapping::ToTypeName(type) != mIndex) {
        throw std::runtime_error("No mapping exist for the mapping index you provided.");
    }

    bool mappingFound{false};
    unsigned int nFirstIndexFree{0};

    // Check the map for the string name.
    bool FirstIndexFreeFound = false;
    auto it = bettingsView->mappings->NewIterator();
    MappingKey key{};
    for (it->Seek(CBettingDB::DbTypeToBytes(MappingKey{type, 0})); it->Valid() && (CBettingDB::BytesToDbType(it->Key(), key), key.nMType == type); it->Next()) {
        CMapping mapping{};
        CBettingDB::BytesToDbType(it->Value(), mapping);
        LogPrintf("%s - mapping - it=[%d,%d] nId=[%d] nMType=[%d] [%s]\n", __func__, key.nMType, key.nId, mapping.nId, mapping.nMType, mapping.sName);
        if (!mappingFound) {
            if (mapping.sName == name) {
                mappings.push_back(Pair("mapping-id", (uint64_t) mapping.nId));
                mappings.push_back(Pair("exists", true));
                mappings.push_back(Pair("mapping-index", mIndex));
                mappingFound = true;
            }
        }
        // Find the first available free key in the sorted map
        if (!FirstIndexFreeFound){
            if (key.nId != nFirstIndexFree) {
                FirstIndexFreeFound = true;
            } else {
                nFirstIndexFree++;
            }
        }
    }

    // If no mapping found then create a new one and add to the given map index.
    if (!mappingFound) {
        CMapping m{};
        m.nMType   = type;
        m.nId      = nFirstIndexFree;
        m.sName    = name;

        if (bettingsView->mappings->Write(MappingKey{m.nMType, m.nId}, m)) {
            mappings.push_back(Pair("mapping-id",  (uint64_t) nFirstIndexFree));
            mappings.push_back(Pair("exists", false));
            mappings.push_back(Pair("mapping-index", mIndex));
        }
    }

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
    if (fHelp || (params.size() < 2))
        throw std::runtime_error(
                "getmappingname\n"
                "\nGet a mapping string name from the specified map index.\n"

                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"mapping name\": \"xxx\",  (string) The mapping name.\n"
                "    \"exists\": \"xxx\", (boolean) mapping transaction created or not\n"
                "    \"mapping-index\": \"xxx\" (string) The index that was searched.\n"
                "  }\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmappingname", "") + HelpExampleRpc("getmappingname", ""));

    const std::string mIndex{params[0].get_str()};
    const uint32_t id{static_cast<uint32_t>(std::stoul(params[1].get_str()))};
    const MappingTypes type{CMapping::FromTypeName(mIndex)};
    UniValue result{UniValue::VARR};
    UniValue mapping{UniValue::VOBJ};

    if (CMapping::ToTypeName(type) != mIndex) {
        throw std::runtime_error("No mapping exist for the mapping index you provided.");
    }

    CMapping map{};
    if (bettingsView->mappings->Read(MappingKey{type, id}, map)) {
        mapping.push_back(Pair("mapping-name", map.sName));
        mapping.push_back(Pair("exists", true));
        mapping.push_back(Pair("mapping-index", static_cast<uint64_t>(map.nMType)));
    }
    else {
        throw std::runtime_error("Currently no mapping name exists for the mapping name you provided.");
    }

    result.push_back(mapping);

    return result;
}
