// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include "bet.h"
#include <string>
#include "version.h"
#include "utilstrencodings.h"
#include <iostream>

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

const peerless_event_test pe_tests[] = {
        {
                .opCode = "425458"             // BTX Prefix
                          "01"                 // BTX protocol version
                          "02"                 // BTX transaction type
                          "00000001"           // Event ID
                          "000000005BD2D91C"   // Event timestamp
                          "00000001"           // Sport
                          "00000001"           // Tournament
                          "00000001"           // Stage
                          "00000001"           // Home team
                          "00000002"           // Away team
                          "7F7F7F7F"           // Home odds
                          "00000002"           // Away odds
                          "00000003",          // Draw odds
                .nEventId    = 1,
                .nStartTime  = 1540544796,
                .nSport      = 1,
                .nTournament = 1,
                .nStage      = 1,
                .nHomeTeam   = 1,
                .nAwayTeam   = 2,
                .nHomeOdds   = 2139062143,
                .nAwayOdds   = 2,
                .nDrawOdds   = 3

        },
//        {
//                .opCode = "425458"
//                          "01"
//                          "02"
//                          "00000101"
//                          "000000005BD2D91C"
//                          "00000001"
//                          "00000001"
//                          "00000001"
//                          "00000001"
//                          "00000002"
//                          "000057E4"
//                          "00004C2C"
//                          "000088B8",
//                .nEventId    = 1,
//                .nStartTime  = 1540544796,
//                .nSport      = 1,
//                .nTournament = 1,
//                .nStage      = 1,
//                .nHomeTeam   = 1,
//                .nAwayTeam   = 2,
//                .nHomeOdds   = 2,
//                .nAwayOdds   = 2,
//                .nDrawOdds   = 3
//        },
};

const peerless_bet_test pb_tests[] = {
        {
                .opCode = "425458"    // BTX format
                          "01"        // BTX version number
                          "03"        // TX type
                          "00000009"  // Event ID
                          "00000002", // Bet Outcome Type
                .nEventId = 9,
                .nOutcome = OutcomeTypeLose,
        },
        {
                .opCode = "425458"
                          "01"
                          "03"
                          "00000040"
                          "00000001",
                .nEventId = 64,
                .nOutcome  = OutcomeTypeWin,
        },
        {
                .opCode = "425458"
                          "01"
                          "03"
                          "00000040"
                          "00000003",
                .nEventId = 64,
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
                          "00000040"
                          "00000002",
                .nEventId = 64,
                .nResult   = ResultTypeLose,
        },
        {
                .opCode = "425458"
                          "01"
                          "04"
                          "00000040"
                          "00000003",
                .nEventId = 64,
                .nResult   = ResultTypeDraw,
        },
        {
                .opCode = "425458"
                          "01"
                          "04"
                          "00000040"
                          "00000004",
                .nEventId = 64,
                .nResult   = ResultTypeRefund,
        }
};

const peerless_update_odds_test puo_tests[] = {
        {
                .opCode = "425458"     // BTX format
                          "01"         // BTX version number
                          "05"         // TX type
                          "00000009"   // Event ID
                          "00000001"   // Home Odds
                          "00000009"   // Away Odds
                          "00000001",  // Draw Odds
                .nEventId  = 64,
                .nHomeOdds = 22000,
                .nAwayOdds = 22000,
                .nDrawOdds = 22000,
        },
        {
                .opCode = "425458"
                          "01"
                          "05"
                          "00000040"
                          "00000040"
                          "00000002"
                          "00000002",
                .nEventId  = 64,
                .nHomeOdds = 22000,
                .nAwayOdds = 22000,
                .nDrawOdds = 22000,
        },
        {
                .opCode = "425458"
                          "01"
                          "05"
                          "00000040"
                          "00000040"
                          "00000002"
                          "00000002",
                .nEventId  = 64,
                .nHomeOdds = 22000,
                .nAwayOdds = 22000,
                .nDrawOdds = 22000,
        },
        {
                .opCode = "425458"
                          "01"
                          "05"
                          "00000040"
                          "00000040"
                          "00000002"
                          "00000002",
                .nEventId  = 64,
                .nHomeOdds = 22000,
                .nAwayOdds = 22000,
                .nDrawOdds = 22000,
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
                          "00000009"
                          "00000064",
                .nEventId  = 9,
                .nEntryFee = 100,
        },
        {
                .opCode = "425458"
                          "01"
                          "06"
                          "00000009"
                          "00000064",
                .nEventId  = 9,
                .nEntryFee = 100,
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
                          "0000007F",
                .nEventId = 127
        }
};

const chain_games_result_test cgr_tests[] = {
        {
                .opCode = "425458"      // BTX format
                          "01"          // BTX version number
                          "08"          // TX type
                          "00000040",   // Event ID
                .nEventId = 64,
        },
        {
                .opCode = "425458"
                          "01"
                          "08"
                          "0000003A",
                .nEventId = 58,
        }
};

BOOST_AUTO_TEST_CASE( basics ) // constructors, equality, inequality
{
    // Test CPeerLessEvent OpCodes.
    CPeerlessEvent pe;
    int num_tests = sizeof(pe_tests) / sizeof(*pe_tests);

    for (int i = 0; i < num_tests; i++) {
        peerless_event_test t = pe_tests[i];
        std::string opCodeHex = t.opCode;

        printf("Peerless Event OPCode: %s\n", opCodeHex.c_str());

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

        printf("Peerless Bet OPCode: %s\n", opCodeHex.c_str());

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

        printf("Peerless Result OPCode: %s\n", opCodeHex.c_str());

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

        printf("Peerless Update Odds OPCode: %s\n", opCodeHex.c_str());

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

        printf("Chain Games Event OPCode: %s\n", opCodeHex.c_str());

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

        printf("Chain Games Bet OPCode: %s\n", opCodeHex.c_str());

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

        printf("Chain Games Result OPCode: %s\n", opCodeHex.c_str());

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
BOOST_AUTO_TEST_SUITE_END()

