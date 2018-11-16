// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet.h"
#include "utilstrencodings.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(bet_tests)

typedef struct _peerless_event_test {
    std::string opCode;
    uint32_t nEventId;
    uint64_t nStartTime;
    uint32_t nSport;
    uint32_t nTournament;
    uint32_t nStage;
    uint32_t nHomeTeam;
    uint32_t nAwayTeam;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;
} peerless_event_test;

typedef struct _peerless_bet_test {
    std::string opCode;
    uint32_t nEventId;
    OutcomeType nOutcome;
} peerless_bet_test;

typedef struct _peerless_result_test {
    std::string opCode;
    uint32_t nEventId;
    ResultType nResult;
} peerless_result_test;

typedef struct _peerless_update_odds_test {
    std::string opCode;
    uint32_t nEventId;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;
} peerless_update_odds_test;

typedef struct _chain_games_event_test {
    std::string opCode;
    uint32_t nEventId;
    uint32_t nEntryFee;
} chain_games_event_test;

typedef struct _chain_games_bet_test {
    std::string opCode;
    uint32_t nEventId;
} chain_games_bet_test;

typedef struct _chain_games_result_test {
    std::string opCode;
    uint32_t nEventId;
} chain_games_result_test;

typedef struct _mapping_test {
    std::string opCode;
    uint32_t nMType;
    uint32_t nId;
    std::string sName;
} mapping_test;

const peerless_event_test pe_tests[] = {
        {
                .opCode = "425458"             // BTX Prefix
                          "01"                 // BTX protocol version
                          "02"                 // BTX transaction type
                          "80808080"           // Event ID
                          "000000005BE554F3"   // Event timestamp
                          "000055FA"           // Sport
                          "000066FA"           // Tournament
                          "0000FFAA"           // Stage
                          "00000001"           // Home team
                          "00000002"           // Away team
                          "000055F0"           // Home odds
                          "000080E8"           // Away odds
                          "0000D8CC",          // Draw odds
                .nEventId    = 2155905152,
                .nStartTime  = 1541756147,
                .nSport      = 22010,
                .nTournament = 26362,
                .nStage      = 65450,
                .nHomeTeam   = 1,
                .nAwayTeam   = 2,
                .nHomeOdds   = 22000,
                .nAwayOdds   = 33000,
                .nDrawOdds   = 55500
        },
        {
                .opCode = "425458"
                          "01"
                          "02"
                          "00000101"
                          "000000005BE2EB8B"
                          "00000001"
                          "00000006"
                          "00000007"
                          "00000008"
                          "00000002"
                          "00001F1F"
                          "00008080"
                          "00001010",
                .nEventId    = 257,
                .nStartTime  = 1541598091,
                .nSport      = 1,
                .nTournament = 6,
                .nStage      = 7,
                .nHomeTeam   = 8,
                .nAwayTeam   = 2,
                .nHomeOdds   = 7967,
                .nAwayOdds   = 32896,
                .nDrawOdds   = 4112
        },
        {
                .opCode = "425458"
                          "01"
                          "02"
                          "FFFFFFFF"
                          "000000005BE55CA0"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF",
                .nEventId    = 4294967295,
                .nStartTime  = 1541758112,
                .nSport      = 4294967295,
                .nTournament = 4294967295,
                .nStage      = 4294967295,
                .nHomeTeam   = 4294967295,
                .nAwayTeam   = 4294967295,
                .nHomeOdds   = 4294967295,
                .nAwayOdds   = 4294967295,
                .nDrawOdds   = 4294967295
        }
};

const peerless_bet_test pb_tests[] = {
        {
                .opCode = "425458"    // BTX format
                          "01"        // BTX version number
                          "03"        // TX type
                          "019A861A"  // Event ID
                          "00000002", // Bet Outcome Type
                .nEventId = 26904090,
                .nOutcome = OutcomeTypeLose,
        },
        {
                .opCode = "425458"
                          "01"
                          "03"
                          "000FDB6D"
                          "00000001",
                .nEventId = 1039213,
                .nOutcome  = OutcomeTypeWin,
        },
        {
                .opCode = "425458"
                          "01"
                          "03"
                          "FFFFFFFF"
                          "00000003",
                .nEventId = 4294967295,
                .nOutcome  = OutcomeTypeDraw,
        },
};

const peerless_result_test pr_tests[] = {
        {
                .opCode = "425458"     // BTX format
                          "01"         // BTX version number
                          "04"         // TX type
                          "00000009"   // Event ID
                          "00000001",  // Event result type
                .nEventId = 9,
                .nResult  = ResultTypeWin,
        },
        {
                .opCode = "425458"
                          "01"
                          "04"
                          "01FC97A7"
                          "00000002",
                .nEventId = 33331111,
                .nResult   = ResultTypeLose,
        },
        {
                .opCode = "425458"
                          "01"
                          "04"
                          "FFFFFFFF"
                          "00000003",
                .nEventId = 4294967295,
                .nResult   = ResultTypeDraw,
        }
};

const peerless_update_odds_test puo_tests[] = {
        {
                .opCode = "425458"     // BTX format
                          "01"         // BTX version number
                          "05"         // TX type
                          "01FC97A7"   // Event ID
                          "0000581B"   // Home Odds
                          "0000D903"   // Away Odds
                          "000055F0",  // Draw Odds
                .nEventId  = 33331111,
                .nHomeOdds = 22555,
                .nAwayOdds = 55555,
                .nDrawOdds = 22000,
        },
        {
                .opCode = "425458"
                          "01"
                          "05"
                          "00000040"
                          "000055F0"
                          "0000AD9C"
                          "0001046A",
                .nEventId  = 64,
                .nHomeOdds = 22000,
                .nAwayOdds = 44444,
                .nDrawOdds = 66666,
        },
        {
                .opCode = "425458"
                          "01"
                          "05"
                          "020ECD6C"
                          "00012FD1"
                          "00003FE4"
                          "0001689A",
                .nEventId  = 34524524,
                .nHomeOdds = 77777,
                .nAwayOdds = 16356,
                .nDrawOdds = 92314,
        },
        {
                .opCode = "425458"
                          "01"
                          "05"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF"
                          "FFFFFFFF",
                .nEventId  = 4294967295,
                .nHomeOdds = 4294967295,
                .nAwayOdds = 4294967295,
                .nDrawOdds = 4294967295,
        }
};

const chain_games_event_test cge_tests[] = {
        {
                .opCode = "425458"     // BTX format
                          "01"         // BTX version number
                          "06"         // TX type
                          "00000009"   // Event ID
                          "00000064",  // Entry Price
                .nEventId  = 9,
                .nEntryFee = 100,
        },
        {
                .opCode = "425458"
                          "01"
                          "06"
                          "000007D0"
                          "000000C8",
                .nEventId  = 2000,
                .nEntryFee = 200,
        },
        {
                .opCode = "425458"
                          "01"
                          "06"
                          "FFFFFFFF"
                          "FFFFFFFF",
                .nEventId  = 4294967295,
                .nEntryFee = 4294967295,
        }
};

const chain_games_bet_test cgb_tests[] = {
        {
                .opCode = "425458"      // BTX format
                          "01"          // BTX version number
                          "07"          // TX type
                          "75757575",   // Event ID
                .nEventId = 1970632053
        },
        {
                .opCode = "425458"
                          "01"
                          "07"
                          "FFFFFFFF",
                .nEventId = 4294967295
        }
};

const chain_games_result_test cgr_tests[] = {
        {
                .opCode = "425458"      // BTX format
                          "01"          // BTX version number
                          "08"          // TX type
                          "0001689A",   // Event ID
                .nEventId = 92314,
        },
        {
                .opCode = "425458"
                          "01"
                          "08"
                          "FFFFFFFF",
                .nEventId = 4294967295,
        }
};

const mapping_test cm_tests[] = {
        {
                .opCode = "425458"             // BTX format
                          "01"                 // BTX version number
                          "01"                 // TX type
                          "00000001"           // Mapping type
                          "0160D4B3"           // Mapping ID
                          "536f63636572",      // Hex encoded string
                .nMType = 1,
                .nId    = 23123123,
                .sName  = "Soccer"
        },
        {
                .opCode = "425458"
                          "01"
                          "01"
                          "00000002"
                          "00007FFE"
                          "526f756e642031",
                .nMType = 2,
                .nId    = 32766,
                .sName  = "Round 1"
        },
        {
                .opCode = "425458"
                          "01"
                          "01"
                          "00000003"
                          "02A65A08"
                          "4c69766572706f6f6c",
                .nMType = 3,
                .nId    = 44456456,
                .sName  = "Liverpool"
        },
        {
                .opCode = "425458"
                          "01"
                          "01"
                          "00000004"
                          "FFFFFFFF"
                          "576f726c64204375702032303138",
                .nMType = 4,
                .nId    = 4294967295,
                .sName  = "World Cup 2018"
        }
};

BOOST_AUTO_TEST_CASE(basics) // constructors, equality, inequality
{
    // Test CMapping OpCodes.
    CMapping cm;
    int cm_num_tests = sizeof(cm_tests) / sizeof(*cm_tests);

    for (int i = 0; i < cm_num_tests; i++) {
        mapping_test t = cm_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CMapping::FromOpCode(OpCode, cm));
        BOOST_CHECK_EQUAL(cm.nMType, t.nMType);
        BOOST_CHECK_EQUAL(cm.nId, t.nId);
        BOOST_CHECK_EQUAL(cm.sName, t.sName);
    }

    // Test CPeerLessEvent OpCodes.
    CPeerlessEvent pe;
    int num_tests = sizeof(pe_tests) / sizeof(*pe_tests);

    for (int i = 0; i < num_tests; i++) {
        peerless_event_test t = pe_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CPeerlessEvent::FromOpCode(OpCode, pe));
        BOOST_CHECK_EQUAL(pe.nEventId, t.nEventId);
        BOOST_CHECK_EQUAL(pe.nStartTime, t.nStartTime);
        BOOST_CHECK_EQUAL(pe.nSport, t.nSport);
        BOOST_CHECK_EQUAL(pe.nTournament, t.nTournament);
        BOOST_CHECK_EQUAL(pe.nStage, t.nStage);
        BOOST_CHECK_EQUAL(pe.nHomeTeam, t.nHomeTeam);
        BOOST_CHECK_EQUAL(pe.nAwayTeam, t.nAwayTeam);
        BOOST_CHECK_EQUAL(pe.nHomeOdds, t.nHomeOdds);
        BOOST_CHECK_EQUAL(pe.nAwayOdds, t.nAwayOdds);
        BOOST_CHECK_EQUAL(pe.nDrawOdds, t.nDrawOdds);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CPeerlessEvent::ToOpCode(pe, opCode));
    }

    // Test CPeerLess Bet OpCodes.
    CPeerlessBet pb;
    int pb_num_tests = sizeof(pb_tests) / sizeof(*pb_tests);

    for (int i = 0; i < pb_num_tests; i++) {
        peerless_bet_test t = pb_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CPeerlessBet::FromOpCode(OpCode, pb));
        BOOST_CHECK_EQUAL(pb.nEventId, t.nEventId);
        BOOST_CHECK_EQUAL(pb.nOutcome, t.nOutcome);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CPeerlessBet::ToOpCode(pb, opCode));
    }

    // Test CPeerLess Result OpCodes.
    CPeerlessResult pr;
    int pr_num_tests = sizeof(pr_tests) / sizeof(*pr_tests);

    for (int i = 0; i < pr_num_tests; i++) {
        peerless_result_test t = pr_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CPeerlessResult::FromOpCode(OpCode, pr));
        BOOST_CHECK_EQUAL(pr.nEventId, t.nEventId);
        BOOST_CHECK_EQUAL(pr.nResult, t.nResult);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CPeerlessResult::ToOpCode(pr, opCode));
    }

    // Test CPeerLess Update Odds OpCodes.
    CPeerlessUpdateOdds puo;
    int puo_num_tests = sizeof(puo_tests) / sizeof(*puo_tests);

    for (int i = 0; i < puo_num_tests; i++) {
        peerless_update_odds_test t = puo_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CPeerlessUpdateOdds::FromOpCode(OpCode, puo));
        BOOST_CHECK_EQUAL(puo.nEventId, t.nEventId);
        BOOST_CHECK_EQUAL(puo.nHomeOdds, t.nHomeOdds);
        BOOST_CHECK_EQUAL(puo.nAwayOdds, t.nAwayOdds);
        BOOST_CHECK_EQUAL(puo.nDrawOdds, t.nDrawOdds);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CPeerlessResult::ToOpCode(pr, opCode));
    }

    // Test CChainGames Event OpCodes.
    CChainGamesEvent cge;
    int cge_num_tests = sizeof(cge_tests) / sizeof(*cge_tests);

    for (int i = 0; i < cge_num_tests; i++) {
        chain_games_event_test t = cge_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CChainGamesEvent::FromOpCode(OpCode, cge));
        BOOST_CHECK_EQUAL(cge.nEventId, t.nEventId);
        BOOST_CHECK_EQUAL(cge.nEntryFee, t.nEntryFee);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CChainGamesEvent::ToOpCode(cge, opCode));
    }

    // Test CChainGames Bet OpCodes.
    CChainGamesBet cgb;
    int cgb_num_tests = sizeof(cgb_tests) / sizeof(*cgb_tests);

    for (int i = 0; i < cgb_num_tests; i++) {
        chain_games_bet_test t = cgb_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CChainGamesBet::FromOpCode(OpCode, cgb));
        BOOST_CHECK_EQUAL(cgb.nEventId, t.nEventId);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CChainGamesBet::ToOpCode(cgb, opCode));
    }

    // Test CChainGames Result OpCodes.
    CChainGamesResult cgr;
    int cgr_num_tests = sizeof(cgr_tests) / sizeof(*cgr_tests);

    for (int i = 0; i < cgr_num_tests; i++) {
        chain_games_result_test t = cgr_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string OpCode(vOpCode.begin(), vOpCode.end());

        // From OpCode
        BOOST_CHECK(CChainGamesResult::FromOpCode(OpCode, cgr));
        BOOST_CHECK_EQUAL(cgr.nEventId, t.nEventId);

        // To OpCode
        std::string opCode;
        BOOST_CHECK(CChainGamesResult::ToOpCode(cgr, opCode));
    }
}

BOOST_AUTO_TEST_CASE(serialisation) // Test the event index map serialisation / deserialization methods.
{
    /*** TEST THE EVENTS INDEX SERIALIZATION AND DESERIALIZATION. ***/
    int num_tests = sizeof(pe_tests) / sizeof(*pe_tests);
    eventIndex_t eventIndex;

    // Build replica event index map.
    for (int i = 0; i < num_tests; i++) {
        peerless_event_test t = pe_tests[i];
        std::string opCodeHex = t.opCode;

        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string opCode(vOpCode.begin(), vOpCode.end());

        CPeerlessEvent pe;
        CPeerlessEvent::FromOpCode(opCode, pe);

        eventIndex.insert(std::make_pair(pe.nEventId, pe));
    }

    // Write binary data to file (events.dat)
    CEventDB pedb;
    uint256 lastBlockHash = uint256("0x000007b9191bc7a17bfb6cedf96a8dacebb5730b498361bf26d44a9f9dcc1079");
    BOOST_CHECK(pedb.Write(eventIndex, lastBlockHash));

    // Read binary from file (events.dat)
    eventIndex_t eventIndexNew;
    uint256 lastBlockHashNew;

    BOOST_CHECK(pedb.Read(eventIndexNew, lastBlockHashNew));
    BOOST_CHECK_EQUAL(lastBlockHash.ToString(), lastBlockHashNew.ToString());

    // Test the deserialized objects.
    for (std::map<uint32_t, CPeerlessEvent>::iterator it=eventIndexNew.begin(); it!=eventIndexNew.end(); ++it) {
        BOOST_CHECK_EQUAL(it->second.nEventId, eventIndex[it->first].nEventId);
        BOOST_CHECK_EQUAL(it->second.nStartTime, eventIndex[it->first].nStartTime);
        BOOST_CHECK_EQUAL(it->second.nSport, eventIndex[it->first].nSport);
        BOOST_CHECK_EQUAL(it->second.nTournament, eventIndex[it->first].nTournament);
        BOOST_CHECK_EQUAL(it->second.nStage, eventIndex[it->first].nStage);
        BOOST_CHECK_EQUAL(it->second.nHomeTeam, eventIndex[it->first].nHomeTeam);
        BOOST_CHECK_EQUAL(it->second.nAwayTeam, eventIndex[it->first].nAwayTeam);
        BOOST_CHECK_EQUAL(it->second.nHomeOdds, eventIndex[it->first].nHomeOdds);
        BOOST_CHECK_EQUAL(it->second.nAwayOdds, eventIndex[it->first].nAwayOdds);
        BOOST_CHECK_EQUAL(it->second.nDrawOdds, eventIndex[it->first].nDrawOdds);
    }


    /*** TEST THE MAPPING INDEXES SERIALIZATION AND DESERIALIZATION. ***/
    mappingIndex_t mSportsIndex;
    mappingIndex_t mRoundsIndex;
    mappingIndex_t mTeamNamesIndex;
    mappingIndex_t mTournamentsIndex;

    int cm_num_tests = sizeof(cm_tests) / sizeof(*cm_tests);

    for (int i = 0; i < cm_num_tests; i++) {
        mapping_test t = cm_tests[i];
        std::vector<unsigned char> vOpCode = ParseHex(t.opCode);
        std::string opCode(vOpCode.begin(), vOpCode.end());

        CMapping cm;
        CMapping::FromOpCode(opCode, cm);

        if (cm.nMType == sportMapping) {
            mSportsIndex.insert(std::make_pair(cm.nId, cm));

            // Write binary data to file (sports.dat)
            CMappingDB cmdb("sports.dat");
            BOOST_CHECK(cmdb.Write(mSportsIndex, lastBlockHash));

            // Read binary from file (sports.dat)
            mappingIndex_t mSportIndexNew;

            BOOST_CHECK(cmdb.Read(mSportIndexNew, lastBlockHashNew));
            BOOST_CHECK_EQUAL(lastBlockHash.ToString(), lastBlockHashNew.ToString());

            // Test the deserialized objects.
            for (std::map<uint32_t, CMapping>::iterator it=mSportIndexNew.begin(); it!=mSportIndexNew.end(); ++it) {
                BOOST_CHECK_EQUAL(it->second.nMType, mSportsIndex[it->first].nMType);
                BOOST_CHECK_EQUAL(it->second.nId, mSportsIndex[it->first].nId);
                BOOST_CHECK_EQUAL(it->second.sName, mSportsIndex[it->first].sName);
            }
        }
        else if (cm.nMType == roundMapping) {
            mRoundsIndex.insert(std::make_pair(cm.nId, cm));

            // Write binary data to file (rounds.dat)
            CMappingDB cmdb("rounds.dat");
            BOOST_CHECK(cmdb.Write(mRoundsIndex, lastBlockHash));

            // Read binary from file (rounds.dat)
            mappingIndex_t mRoundsIndexNew;

            BOOST_CHECK(cmdb.Read(mRoundsIndexNew, lastBlockHashNew));
            BOOST_CHECK_EQUAL(lastBlockHash.ToString(), lastBlockHashNew.ToString());

            // Test the deserialized objects.
            for (std::map<uint32_t, CMapping>::iterator it=mRoundsIndexNew.begin(); it!=mRoundsIndexNew.end(); ++it) {
                BOOST_CHECK_EQUAL(it->second.nMType, mRoundsIndex[it->first].nMType);
                BOOST_CHECK_EQUAL(it->second.nId, mRoundsIndex[it->first].nId);
                BOOST_CHECK_EQUAL(it->second.sName, mRoundsIndex[it->first].sName);
            }
        }
        else if (cm.nMType == teamMapping) {
            mTeamNamesIndex.insert(std::make_pair(cm.nId, cm));

            // Write binary data to file (teamnames.dat)
            CMappingDB cmdb("teamnames.dat");
            BOOST_CHECK(cmdb.Write(mTeamNamesIndex, lastBlockHash));

            // Read binary from file (teamnames.dat)
            mappingIndex_t mTeamNamesIndexNew;

            BOOST_CHECK(cmdb.Read(mTeamNamesIndexNew, lastBlockHashNew));
            BOOST_CHECK_EQUAL(lastBlockHash.ToString(), lastBlockHashNew.ToString());

            // Test the deserialized objects.
            for (std::map<uint32_t, CMapping>::iterator it=mTeamNamesIndexNew.begin(); it!=mTeamNamesIndexNew.end(); ++it) {
                BOOST_CHECK_EQUAL(it->second.nMType, mTeamNamesIndex[it->first].nMType);
                BOOST_CHECK_EQUAL(it->second.nId, mTeamNamesIndex[it->first].nId);
                BOOST_CHECK_EQUAL(it->second.sName, mTeamNamesIndex[it->first].sName);
            }
        }
        else if (cm.nMType == tournamentMapping) {
            mTournamentsIndex.insert(std::make_pair(cm.nId, cm));

            // Write binary data to file (tournaments.dat)
            CMappingDB cmdb("tournaments.dat");
            BOOST_CHECK(cmdb.Write(mTournamentsIndex, lastBlockHash));

            // Read binary from file (tournaments.dat)
            mappingIndex_t mTournamentsIndexNew;

            BOOST_CHECK(cmdb.Read(mTournamentsIndexNew, lastBlockHashNew));
            BOOST_CHECK_EQUAL(lastBlockHash.ToString(), lastBlockHashNew.ToString());

            // Test the deserialized objects.
            for (std::map<uint32_t, CMapping>::iterator it=mTournamentsIndexNew.begin(); it!=mTournamentsIndexNew.end(); ++it) {
                BOOST_CHECK_EQUAL(it->second.nMType, mTournamentsIndex[it->first].nMType);
                BOOST_CHECK_EQUAL(it->second.nId, mTournamentsIndex[it->first].nId);
                BOOST_CHECK_EQUAL(it->second.sName, mTournamentsIndex[it->first].sName);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()