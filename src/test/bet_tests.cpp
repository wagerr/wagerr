// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "betting/bet.h"
#include "clientversion.h"
#include "streams.h"
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
    uint8_t nResultType;
    uint32_t homeScore;
    uint32_t awayScore;
} peerless_result_test;

typedef struct _peerless_update_odds_test {
    std::string opCode;
    uint32_t nEventId;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;
} peerless_update_odds_test;

typedef struct _peerless_spreads_event_test {
    std::string opCode;
    uint32_t nEventId;
    uint32_t nPoints;
    uint32_t nOverOdds;
    uint32_t nUnderOdds;
} peerless_spreads_event_test;

typedef struct _peerless_totals_event_test {
    std::string opCode;
    uint32_t nEventId;
    uint32_t nPoints;
    uint32_t nOverOdds;
    uint32_t nUnderOdds;
} peerless_totals_event_test;

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
        .opCode = "42"           // BTX Prefix
                  "01"           // BTX protocol version
                  "02"           // BTX transaction type
                  "80808080"     // Event ID
                  "F354E55B"     // Event timestamp
                  "FA55"         // Sport
                  "FA66"         // Tournament
                  "AAFF"         // Stage
                  "01000000"     // Home team
                  "02000000"     // Away team
                  "F0550000"     // Home odds
                  "E8800000"     // Away odds
                  "CCD80000",    // Draw odds
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
        .opCode = "42"
                  "01"
                  "02"
                  "01010000"
                  "8BEBE25B"
                  "0100"
                  "0600"
                  "0700"
                  "08000000"
                  "02000000"
                  "1F1F0000"
                  "80800000"
                  "10100000",
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
        .opCode = "42"
                  "01"
                  "02"
                  "FFFFFFFF"
                  "A05CE55B"
                  "FFFF"
                  "FFFF"
                  "FFFF"
                  "FFFFFFFF"
                  "FFFFFFFF"
                  "FFFFFFFF"
                  "FFFFFFFF"
                  "FFFFFFFF",
        .nEventId    = 4294967295,
        .nStartTime  = 1541758112,
        .nSport      = 65535,
        .nTournament = 65535,
        .nStage      = 65535,
        .nHomeTeam   = 4294967295,
        .nAwayTeam   = 4294967295,
        .nHomeOdds   = 4294967295,
        .nAwayOdds   = 4294967295,
        .nDrawOdds   = 4294967295
    }
};

const peerless_bet_test pb_tests[] = {
    {
         .opCode = "42"        // BTX format
                   "01"        // BTX version number
                   "03"        // TX type
                   "1A869A01"  // Event ID
                   "02",       // Bet Outcome Type
         .nEventId = 26904090,
         .nOutcome = moneyLineLose,
    },
    {
         .opCode = "42"
                   "01"
                   "03"
                   "6DDB0F00"
                   "01",
         .nEventId = 1039213,
         .nOutcome = moneyLineWin,
    },
    {
         .opCode = "42"
                   "01"
                   "03"
                   "FFFFFFFF"
                   "03",
         .nEventId = 4294967295,
         .nOutcome = moneyLineDraw,
    },
};

const peerless_result_test pr_tests[] = {
    {
         .opCode = "42"         // BTX format
                   "01"         // BTX version number
                   "04"         // TX type
                   "09000000"   // Event ID
                   "01"         // Result Type
                   "0100"       // Home team score
                   "0200",      // Away team score
         .nEventId    = 9,
         .nResultType = 1,
         .homeScore   = 1,
         .awayScore   = 2,
    },
//    {
//         .opCode = "42"
//                   "01"
//                   "04"
//                   "01FC97A7"
//                   "02"
//                   "02"
//                   "02",
//         .nEventId         = 33331111,
//         .nMoneyLineResult = awayWin,
//         .nSpreadResult    = awayWin,
//         .nTotalResult     = awayWin
//    },
//    {
//         .opCode = "42"
//                   "01"
//                   "04"
//                   "FFFFFFFF"
//                   "03"
//                   "03"
//                   "03",
//         .nEventId         = 4294967295,
//         .nMoneyLineResult = push,
//         .nSpreadResult    = awayWin,
//         .nTotalResult     = awayWin
//    }
};

const peerless_update_odds_test puo_tests[] = {
    {
         .opCode = "42"         // BTX format
                   "01"         // BTX version number
                   "05"         // TX type
                   "A797FC01"   // Event ID
                   "1B580000"   // Home Odds
                   "03D90000"   // Away Odds
                   "F0550000",  // Draw Odds
         .nEventId  = 33331111,
         .nHomeOdds = 22555,
         .nAwayOdds = 55555,
         .nDrawOdds = 22000,
    },
    {
         .opCode = "42"
                   "01"
                   "05"
                   "40000000"
                   "F0550000"
                   "9CAD0000"
                   "6A040100",
         .nEventId  = 64,
         .nHomeOdds = 22000,
         .nAwayOdds = 44444,
         .nDrawOdds = 66666,
    },
    {
         .opCode = "42"
                   "01"
                   "05"
                   "6CCD0E02"
                   "D12F0100"
                   "E43F0000"
                   "9A680100",
         .nEventId  = 34524524,
         .nHomeOdds = 77777,
         .nAwayOdds = 16356,
         .nDrawOdds = 92314,
    },
    {
         .opCode = "42"
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

const peerless_spreads_event_test pse_tests[] = {
    {
         .opCode = "42"         // BTX format
                   "01"         // BTX version number
                   "09"         // TX type
                   "A797FC01"   // Event ID
                   "581B"       // Spread Points
                   "03D90000"   // Over Odds
                   "F0550000",  // Under Odds
         .nEventId  = 33331111,
         .nPoints = 22555,
         .nOverOdds = 55555,
         .nUnderOdds = 22000,
    },
    {
         .opCode = "42"
                   "01"
                   "09"
                   "40000000"
                   "F055"
                   "9CAD0000"
                   "6A040100",
         .nEventId  = 64,
         .nPoints = 22000,
         .nOverOdds = 44444,
         .nUnderOdds = 66666,
    },
    {
         .opCode = "42"
                   "01"
                   "09"
                   "6CCD0E02"
                   "D12F"
                   "E43F0000"
                   "9A680100",
         .nEventId  = 34524524,
         .nPoints = 12241,
         .nOverOdds = 16356,
         .nUnderOdds = 92314,
    },
    {
         .opCode = "42"
                   "01"
                   "09"
                   "FFFFFFFF"
                   "FFFF"
                   "FFFFFFFF"
                   "FFFFFFFF",
         .nEventId  = 4294967295,
         .nPoints = 65535,
         .nOverOdds = 4294967295,
         .nUnderOdds = 4294967295,
    }
};

const peerless_totals_event_test pte_tests[] = {
    {
        .opCode = "42"         // BTX format
                  "01"         // BTX version number
                  "0a"         // TX type
                  "A797FC01"   // Event ID
                  "1B58"       // Totals Points
                  "03D90000"   // Over Odds
                  "F0550000",  // Under Odds
        .nEventId  = 33331111,
        .nPoints = 22555,
        .nOverOdds = 55555,
        .nUnderOdds = 22000,
    },
    {
        .opCode = "42"
                  "01"
                  "0a"
                  "40000000"
                  "F055"
                  "9CAD0000"
                  "6A040100",
        .nEventId  = 64,
        .nPoints = 22000,
        .nOverOdds = 44444,
        .nUnderOdds = 66666,
    },
    {
        .opCode = "42"
                  "01"
                  "0a"
                  "6CCD0E02"
                  "D12F"
                  "E43F0000"
                  "9A680100",
        .nEventId  = 34524524,
        .nPoints = 12241,
        .nOverOdds = 16356,
        .nUnderOdds = 92314,
    },
    {
        .opCode = "42"
                  "01"
                  "0a"
                  "FFFFFFFF"
                  "FFFF"
                  "FFFFFFFF"
                  "FFFFFFFF",
        .nEventId  = 4294967295,
        .nPoints = 65535,
        .nOverOdds = 4294967295,
        .nUnderOdds = 4294967295,
    }
};

const chain_games_event_test cge_tests[] = {
    {
        .opCode = "42"      // BTX format
                  "01"         // BTX version number
                  "06"         // TX type
                  "0900"       // Event ID
                  "6400",      // Entry Price
        .nEventId  = 9,
        .nEntryFee = 100,
    },
    {
        .opCode = "42"
                  "01"
                  "06"
                  "D007"
                  "C800",
        .nEventId  = 2000,
        .nEntryFee = 200,
    },
    {
        .opCode = "42"
                  "01"
                  "06"
                  "FFFF"
                  "FFFF",
        .nEventId  = 65535,
        .nEntryFee = 65535,
    }
};

const chain_games_bet_test cgb_tests[] = {
    {
         .opCode = "42"         // BTX format
                   "01"         // BTX version number
                   "07"         // TX type
                   "7575",      // Event ID
         .nEventId = 30069
    },
    {
         .opCode = "42"
                   "01"
                   "07"
                   "FFFF",
         .nEventId = 65535
    }
};

const chain_games_result_test cgr_tests[] = {
    {
         .opCode = "42"         // BTX format
                   "01"         // BTX version number
                   "08"         // TX type
                   "9A68",      // Event ID
         .nEventId = 26778,
    },
    {
         .opCode = "42"
                   "01"
                   "08"
                   "FFFF",
         .nEventId = 65535,
    }
};

const mapping_test cm_tests[] = {
    {                              // Sport mapping.
         .opCode = "42"            // BTX format
                   "01"            // BTX version number
                   "01"            // TX type
                   "01"            // Mapping type
                   "B3D4"          // Mapping ID
                   "536f63636572", // Hex encoded string
         .nMType = 1,
         .nId    = 54451,
         .sName  = "Soccer"
    },
    {       // Round mapping.
         .opCode = "42"
                   "01"
                   "01"
                   "02"
                   "FE7F"
                   "526f756e642031",
         .nMType = 2,
         .nId    = 32766,
         .sName  = "Round 1"
    },
    {       // Team mapping.
         .opCode = "42"
                   "01"
                   "01"
                   "03"
                   "085AA602"
                   "4c69766572706f6f6c",
         .nMType = 3,
         .nId    = 44456456,
         .sName  = "Liverpool"
    },
    {
         .opCode = "42"
                   "01"
                   "01"
                   "04"
                   "FFFF"
                   "576f726c64204375702032303138",
         .nMType = 4,
         .nId    = 65535,
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
        BOOST_CHECK_EQUAL(pr.nResultType, t.nResultType);
        BOOST_CHECK_EQUAL(pr.nHomeScore, t.homeScore);
        BOOST_CHECK_EQUAL(pr.nAwayScore, t.awayScore);

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
    CScript cgrScript;
    cgr = CChainGamesResult(3102);

    CDataStream ssObj1(SER_DISK, CLIENT_VERSION);
    ssObj1 << cgr;

    std::vector<unsigned char> dataIn = {30, 12};
    cgrScript << OP_RETURN << dataIn;
    CScript::const_iterator pc = cgrScript.begin();
    std::vector<unsigned char> data;
    opcodetype opcode;

    BOOST_CHECK(cgrScript.GetOp(pc, opcode, data));
    BOOST_CHECK(opcode == OP_RETURN);

    BOOST_CHECK(cgrScript.GetOp(pc, opcode, data));
    BOOST_CHECK(data.size() == 2);
    uint16_t nEventId = 3102;

    CDataStream ssObj(data, SER_DISK, CLIENT_VERSION);
    ssObj >> cgr;
    BOOST_CHECK(cgr.nEventId == nEventId);
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