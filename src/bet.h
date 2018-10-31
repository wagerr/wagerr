// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>

// The supported bet outcome types.
typedef enum OutcomeType {
    OutcomeTypeWin   = 0x01,
    OutcomeTypeLose  = 0x02,
    OutcomeTypeDraw  = 0x03
} OutcomeType;

// The supported result types.
typedef enum ResultType {
    ResultTypeWin    = 0x01,
    ResultTypeLose   = 0x02,
    ResultTypeDraw   = 0x03,
    ResultTypeRefund = 0x04
} ResultType;

// Op code string lengths for the various bet transactions.
typedef enum OpCodeStrLen{
    PEFromOp  = 49,
    PEToOp    = 98,
    PBFromOp  = 13,
    PBToOp    = 26,
    PRFromOp  = 13,
    PRToOp    = 26,
    PUOFromOp = 22,
    PUOToOp   = 44,
    CGEFromOp = 13,
    CGEToOp   = 26,
    CGBFromOp = 9,
    CGBToOp   = 18,
    CGRFromOp = 9,
    CGRToOp   = 18
}OpCodeStringLengths;

/**
 *
 */
class CPeerlessEvent
{
public:
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

    // Default Constructor.
    // CPeerlessEvent();

    static bool ToOpCode(CPeerlessEvent pe, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessEvent &pe);
};

class CPeerlessBet
{
public:
    uint32_t nEventId;
    OutcomeType nOutcome;

    // Default Constructor.
    // CPeerlessBet();

    // Parametrized Constructor.
    // CPeerlessBet(int eventId, int outcome);

    static bool ToOpCode(CPeerlessBet pb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessBet &pb) ;
};

class CPeerlessResult
{
public:
    uint32_t nEventId;
    ResultType nResult;

    // Default Constructor.
    // CPeerlessResult();

    // Parametrized Constructor.
    // CPeerlessResult(int eventId, int result);

    static bool ToOpCode(CPeerlessResult pr, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessResult &pr);
};

class CPeerlessUpdateOdds
{
public:
    uint32_t nEventId;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    // Default Constructor.
    // CPeerlessUpdateOdds();

    static bool ToOpCode(CPeerlessUpdateOdds puo, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessUpdateOdds &puo);
};

class CChainGamesEvent
{
public:
    uint32_t nEventId;
    uint32_t nEntryFee;

    // Default Constructor.
    // CChainGamesEvent();

    static bool ToOpCode(CChainGamesEvent cge, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesEvent &cge);
};

class CChainGamesBet
{
public:
    uint32_t nEventId;

    // Default Constructor.
    // CChainGamesBet();

    static bool ToOpCode(CChainGamesBet cgb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesBet &cgb);
};

class CChainGamesResult
{
public:
    uint32_t nEventId;

    // Default Constructor.
    // CChainGamesBet();

    static bool ToOpCode(CChainGamesResult cgr, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesResult &cgr);
};

#endif // WAGERR_BET_H
