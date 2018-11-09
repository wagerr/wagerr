// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

#include "chainparams.h"

#include <boost/filesystem/path.hpp>
#include <map>

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

// The supported betting TX types.
typedef enum BetTxTypes{
    mappingTxType      = 0x01,  // Mapping transaction type identifier.
    plEventTxType      = 0x02,  // Peerless event transaction type identifier.
    plBetTxType        = 0x03,  // Peerless Bet transaction type identifier.
    plResultTxType     = 0x04,  // Peerless Result transaction type identifier.
    plUpdateOddsTxType = 0x05,  // Peerless update odds transaction type identifier.
    cgEventTxType      = 0x06,  // Chain games event transaction type identifier.
    cgBetTxType        = 0x07,  // Chain games bet transaction type identifier.
    cgResultTxType     = 0x08   // Chain games result transaction type identifier.
}BetTxTypes;

bool IsValidOracleTx(const CTxIn &txin);

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
    CPeerlessEvent() {}

    static bool ToOpCode(CPeerlessEvent pe, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessEvent &pe);
};

class CPeerlessBet
{
public:
    uint32_t nEventId;
    OutcomeType nOutcome;

    // Default constructor.
    CPeerlessBet() {}

    // Parametrized constructor.
    CPeerlessBet(int eventId, OutcomeType outcome)
    {
        nEventId = eventId;
        nOutcome = outcome;
    }

    static bool ToOpCode(CPeerlessBet pb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessBet &pb) ;
};

class CPeerlessResult
{
public:
    uint32_t nEventId;
    ResultType nResult;

    // Default Constructor.
    CPeerlessResult() {}

    // Parametrized Constructor.
    CPeerlessResult(int eventId, ResultType result){
        nEventId = eventId;
        nResult  = result;
    }

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
    CPeerlessUpdateOdds() {}

    static bool ToOpCode(CPeerlessUpdateOdds puo, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessUpdateOdds &puo);
};

class CChainGamesEvent
{
public:
    uint32_t nEventId;
    uint32_t nEntryFee;

    // Default Constructor.
    CChainGamesEvent() {}

    static bool ToOpCode(CChainGamesEvent cge, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesEvent &cge);
};

class CChainGamesBet
{
public:
    uint32_t nEventId;

    // Default Constructor.
    CChainGamesBet() {}

    // Parametrized Constructor.
    CChainGamesBet(int eventId) {
        nEventId = eventId;
    }

    static bool ToOpCode(CChainGamesBet cgb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesBet &cgb);
};

class CChainGamesResult
{
public:
    uint32_t nEventId;

    // Default Constructor.
    CChainGamesResult() {}

    static bool ToOpCode(CChainGamesResult cgr, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesResult &cgr);
};

#endif // WAGERR_BET_H
