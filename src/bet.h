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

// The supported result types
typedef enum ResultType {
    standardResult = 0x01,
    eventRefund    = 0x02,
    mlRefund       = 0x03,
    spreadsRefund  = 0x04,
    totalsRefund   = 0x05,
} ResultType;

// The supported result types
typedef enum WinnerType {
    homeWin = 0x01,
    awayWin = 0x02,
    push    = 0x03,
} WinnerType;

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

// Class derived from CTxOut
// nBetValue is NOT serialized, nor is it included in the hash.
class CBetOut : public CTxOut {
    public:

    CAmount nBetValue;

    CBetOut() : CTxOut() {
        SetNull();
    }

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn) : CTxOut(nValueIn, scriptPubKeyIn), nBetValue(0) {};

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn) :
            CTxOut(nValueIn, scriptPubKeyIn), nBetValue(nBetValueIn) {};

    void SetNull() {
        CTxOut::SetNull();
        nBetValue = -1;
    }

    void SetEmpty() {
        CTxOut::SetEmpty();
        nBetValue = 0;
    }

    bool IsEmpty() const {
        return CTxOut::IsEmpty() && nBetValue == 0;
    }
};

/** Ensures a TX has come from an OMNO wallet. **/
bool IsValidOracleTx(const CTxIn &txin);

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
int64_t GetBlockPayouts(std::vector<CBetOut>& vExpectedPayouts, CAmount& nMNBetReward);

/** Aggregates the amount of WGR to be minted to pay out all CG Lotto winners as well as OMNO rewards. **/
int64_t GetCGBlockPayouts(std::vector<CBetOut>& vexpectedCGPayouts, CAmount& nMNBetReward);

/** Validating the payout block using the payout vector. **/
bool IsBlockPayoutsValid(std::vector<CBetOut> vExpectedPayouts, CBlock block);

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
    uint32_t nMoneyLineHomePotentialLiability;
    uint32_t nMoneyLineAwayPotentialLiability;
    uint32_t nMoneyLineDrawPotentialLiability;
    uint32_t nSpreadHomePotentialLiability;
    uint32_t nSpreadAwayPotentialLiability;
    uint32_t nSpreadPushPotentialLiability;
    uint32_t nTotalOverPotentialLiability;
    uint32_t nTotalUnderPotentialLiability;
    uint32_t nTotalPushPotentialLiability;
    uint32_t nMoneyLineHomeBets;
    uint32_t nMoneyLineAwayBets;
    uint32_t nMoneyLineDrawBets;
    uint32_t nSpreadHomeBets;
    uint32_t nSpreadAwayBets;
    uint32_t nSpreadPushBets;
    uint32_t nTotalOverBets;
    uint32_t nTotalUnderBets;
    uint32_t nTotalPushBets;

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
        READWRITE(nMoneyLineHomePotentialLiability);
        READWRITE(nMoneyLineAwayPotentialLiability);
        READWRITE(nMoneyLineDrawPotentialLiability);
        READWRITE(nSpreadHomePotentialLiability);
        READWRITE(nSpreadAwayPotentialLiability);
        READWRITE(nSpreadPushPotentialLiability);
        READWRITE(nTotalOverPotentialLiability);
        READWRITE(nTotalUnderPotentialLiability);
        READWRITE(nTotalPushPotentialLiability);
        READWRITE(nMoneyLineHomeBets);
        READWRITE(nMoneyLineAwayBets);
        READWRITE(nMoneyLineDrawBets);     
        READWRITE(nSpreadHomeBets);
        READWRITE(nSpreadAwayBets);
        READWRITE(nSpreadPushBets);
        READWRITE(nTotalOverBets);
        READWRITE(nTotalUnderBets); 
        READWRITE(nTotalPushBets);
    }
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
    static bool FromOpCode(std::string opCode, CPeerlessBet &pb);
};

class CPeerlessResult
{
public:
    int nVersion;

    uint32_t nEventId;
    uint32_t nResultType;
    uint32_t nHomeScore;
    uint32_t nAwayScore;


    // Default Constructor.
    CPeerlessResult() {}

    // Parametrized Constructor.
    CPeerlessResult(int eventId, int pResultType, int pHomeScore, int pAwayScore)
    {
        nEventId    = eventId;
        nResultType = pResultType;
        nHomeScore  = pHomeScore;
        nAwayScore  = pAwayScore;
    }

    static bool ToOpCode(CPeerlessResult pr, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessResult &pr);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nEventId);
        READWRITE(nResultType);
        READWRITE(nHomeScore);
        READWRITE(nAwayScore);
    }
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
    uint16_t nEventId;

    // Default Constructor.
    CChainGamesResult() {}

    CChainGamesResult(uint16_t nEventId) : nEventId(nEventId) {};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nEventId);
    }

    bool FromScript(CScript script);

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

// Define new map type to store Wagerr events.
typedef std::map<uint32_t, CPeerlessEvent> eventIndex_t;

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
    static void RemoveEvent(CPeerlessResult pr);
};

// Define new map type to store Wagerr results.
typedef std::map<uint32_t, CPeerlessResult> resultsIndex_t;

class CResultDB
{
protected:
    // Global variable that stores the Wagerr results.
    static resultsIndex_t resultsIndex;
    static CCriticalSection cs_setResults;

private:
    boost::filesystem::path pathResults;

public:
    // Default constructor.
    CResultDB();

    bool Write(const resultsIndex_t& resultsIndex,  uint256 latestProcessedBlock);
    bool Read(resultsIndex_t& resultsIndex, uint256& lastBlockHash);

    static void GetResults(resultsIndex_t &resultsIndex);
    static void SetResults(const resultsIndex_t &resultsIndex);

    static void AddResult(CPeerlessResult pe);
    static void RemoveResult(CPeerlessResult pe);
};

/** Find peerless events. **/
std::vector<CPeerlessResult> getEventResults(int height);

/** Find chain games lotto result. **/
std::pair<std::vector<CChainGamesResult>,std::vector<std::string>> getCGLottoEventResults(int height);

/** Get the peerless winning bets from the block chain and return the payout vector. **/
std::vector<CBetOut> GetBetPayouts(int height);

/** Get the chain games winner and return the payout vector. **/
std::vector<CBetOut> GetCGLottoBetPayouts(int height);

/** Set a peerless event spread odds **/
void SetEventSpreadOdds(CPeerlessSpreadsEvent sEventOdds);

/** Set a peerless event total odds **/
void SetEventTotalOdds(CPeerlessTotalsEvent tEventOdds);

/** Set a peerless event money line odds **/
void SetEventMLOdds(CPeerlessUpdateOdds mEventOdds);

/** Set a peerless event accumulators **/
void SetEventAccummulators (CPeerlessBet plBet, CAmount betAmount);

#endif // WAGERR_BET_H
