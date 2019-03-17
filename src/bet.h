// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

#include "util.h"
#include "chainparams.h"

#include <boost/filesystem/path.hpp>
#include <map>

// The supported bet outcome types.
typedef enum OutcomeType {
    moneyLineWin  = 0x01,
    moneyLineLose = 0x02,
    moneyLineDraw = 0x03,
    spreadHome    = 0x04,
    spreadAway    = 0x05,
    totalOver     = 0x06,
    totalUnder    = 0x07
} OutcomeType;

// The supported result types.
typedef enum ResultType {
    homeWin = 0x01,
    awayWin = 0x02,
    draw    = 0x03,
    refund  = 0x04
} ResultType;

// The supported betting TX types.
typedef enum BetTxTypes{
    mappingTxType        = 0x01,  // Mapping transaction type identifier.
    plEventTxType        = 0x02,  // Peerless event transaction type identifier.
    plBetTxType          = 0x03,  // Peerless Bet transaction type identifier.
    plResultTxType       = 0x04,  // Peerless Result transaction type identifier.
    plUpdateOddsTxType   = 0x05,  // Peerless update odds transaction type identifier.
    cgEventTxType        = 0x06,  // Chain games event transaction type identifier.
    cgBetTxType          = 0x07,  // Chain games bet transaction type identifier.
    cgResultTxType       = 0x08,  // Chain games result transaction type identifier.
    plSpreadsEventTxType = 0x09,  // Spread odds transaction type identifier.
    plTotalsEventTxType  = 0x0a   // Totals odds transaction type identifier.
} BetTxTypes;

// The supported mapping TX types.
typedef enum MappingTypes {
    sportMapping      = 0x01,
    roundMapping      = 0x02,
    teamMapping       = 0x03,
    tournamentMapping = 0x04
} MappingTypes;

/** Ensures a TX has come from an OMNO wallet. **/
bool IsValidOracleTx(const CTxIn &txin);

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
int64_t GetBlockPayouts(std::vector<CTxOut>& vexpectedPayouts, CAmount& nMNBetReward);

/** Validating the payout block using the payout vector. **/
bool IsBlockPayoutsValid(std::vector<CTxOut> vExpectedPayouts, CBlock block);

class CPeerlessEvent
{
public:
    int nVersion;

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
    uint32_t nSpreadPoints;
    uint32_t nSpreadHomeOdds;
    uint32_t nSpreadAwayOdds;
    uint32_t nTotalPoints;
    uint32_t nTotalOverOdds;
    uint32_t nTotalUnderOdds;

    // Default Constructor.
    CPeerlessEvent() {}

    static bool ToOpCode(CPeerlessEvent pe, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessEvent &pe);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nEventId);
        READWRITE(nStartTime);
        READWRITE(nSport);
        READWRITE(nTournament);
        READWRITE(nStage);
        READWRITE(nHomeTeam);
        READWRITE(nAwayTeam);
        READWRITE(nHomeOdds);
        READWRITE(nAwayOdds);
        READWRITE(nDrawOdds);
        READWRITE(nSpreadPoints);
        READWRITE(nSpreadHomeOdds);
        READWRITE(nSpreadAwayOdds);
        READWRITE(nTotalPoints);
        READWRITE(nTotalOverOdds);
        READWRITE(nTotalUnderOdds);
    }
};

// Define new map type to store Wagerr events.
typedef std::map<uint32_t, CPeerlessEvent> eventIndex_t;

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
    static bool FromOpCode(std::string opCode, CPeerlessBet &pb);
};

class CPeerlessResult
{
public:
    uint32_t nEventId;
    ResultType nMoneyLineResult;
    ResultType nSpreadResult;
    ResultType nTotalResult;

    // Default Constructor.
    CPeerlessResult() {}

    // Parametrized Constructor.
    CPeerlessResult(int eventId, ResultType mlResult, ResultType sResult, ResultType tResult)
    {
        nEventId         = eventId;
        nMoneyLineResult = mlResult;
        nSpreadResult    = sResult;
        nTotalResult     = tResult;
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

class CPeerlessSpreadsEvent
{
public:
    uint32_t nEventId;
    uint32_t nPoints;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;

    // Default Constructor.
    CPeerlessSpreadsEvent() {}

    static bool ToOpCode(CPeerlessSpreadsEvent pse, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessSpreadsEvent &pse);
};

class CPeerlessTotalsEvent
{
public:
    uint32_t nEventId;
    uint32_t nPoints;
    uint32_t nOverOdds;
    uint32_t nUnderOdds;

    // Default Constructor.
    CPeerlessTotalsEvent() {}

    static bool ToOpCode(CPeerlessTotalsEvent pte, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessTotalsEvent &pte);
};

class CMapping
{
public:
    int nVersion;

    uint32_t nMType;
    uint32_t nId;
    string sName;

    // Default Constructor.
    CMapping() {}

    static bool ToOpCode(CMapping &mapping, std::string &opCode);
    static bool FromOpCode(std::string opCode, CMapping &mapping);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nMType);
        READWRITE(nId);
        READWRITE(sName);
    }
};

// Define new map type to store Wagerr mappings.
typedef std::map<uint32_t, CMapping> mappingIndex_t;

class CMappingDB
{
protected:
    // Global variables that stores the different Wagerr mappings.
    static mappingIndex_t mSportsIndex;
    static mappingIndex_t mRoundsIndex;
    static mappingIndex_t mTeamsIndex;
    static mappingIndex_t mTournamentsIndex;

    static CCriticalSection cs_setSports;
    static CCriticalSection cs_setRounds;
    static CCriticalSection cs_setTeams;
    static CCriticalSection cs_setTournaments;

private:
    std::string mDBFileName;
    boost::filesystem::path mFilePath;

public:
    // Default constructor.
    CMappingDB() {}

    // Parametrized Constructor.
    CMappingDB(std::string fileName);

    bool Write(const mappingIndex_t& mappingIndex,  uint256 latestBlockHash);
    bool Read(mappingIndex_t& mappingIndex, uint256& lastBlockHash);

    static void GetSports(mappingIndex_t &sportsIndex);
    static void SetSports(const mappingIndex_t &sportsIndex);
    static void AddSport(CMapping sm);

    static void GetRounds(mappingIndex_t &roundsIndex);
    static void SetRounds(const mappingIndex_t &roundsIndex);
    static void AddRound(CMapping rm);

    static void GetTeams(mappingIndex_t &teamsIndex);
    static void SetTeams(const mappingIndex_t &teamsIndex);
    static void AddTeam(CMapping ts);

    static void GetTournaments(mappingIndex_t &tournamentsIndex);
    static void SetTournaments(const mappingIndex_t &tournamentsIndex);
    static void AddTournament(CMapping ts);
};

class CEventDB
{
protected:
    // Global variable that stores the current live Wagerr events.
    static eventIndex_t eventsIndex;
    static CCriticalSection cs_setEvents;

private:
    boost::filesystem::path pathEvents;

public:
    // Default constructor.
    CEventDB();

    bool Write(const eventIndex_t& eventIndex,  uint256 latestProcessedBlock);
    bool Read(eventIndex_t& eventIndex, uint256& lastBlockHash);

    static void GetEvents(eventIndex_t &eventIndex);
    static void SetEvents(const eventIndex_t &eventIndex);

    static void AddEvent(CPeerlessEvent pe);
    static void RemoveEvent(CPeerlessEvent pe);
};

/** Find peerless events. **/
std::vector<CPeerlessResult> getEventResults(int height);

/** Find chain games lotto result. **/
std::pair<std::vector<CChainGamesResult>,std::vector<std::string>> getCGLottoEventResults(int height);

/** Get the peerless winning bets from the block chain and return the payout vector. **/
std::vector<CTxOut> GetBetPayouts(int height);

/** Get the chain games winner and return the payout vector. **/
std::vector<CTxOut> GetCGLottoBetPayouts(int height);

/** Set a peerless event spread odds **/
void SetEventSpreadOdds(CPeerlessSpreadsEvent sEventOdds);

/** Set a peerless event total odds **/
void SetEventTotalOdds(CPeerlessTotalsEvent tEventOdds);

/** Set a peerless event money line odds **/
void SetEventMLOdds(CPeerlessUpdateOdds mEventOdds);

#endif // WAGERR_BET_H
