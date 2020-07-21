// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_COMMON_H
#define WAGERR_BET_COMMON_H

#include <betting/quickgames/qgview.h>
#include <primitives/transaction.h>
#include <base58.h>
#include <amount.h>

class CPeerlessResultDB;
class CChainGamesResultDB;
class CPeerlessLegDB;
class CPeerlessBaseEventDB;
class CPayoutInfoDB;

#define BET_ODDSDIVISOR 10000   // Odds divisor, Facilitates calculations with floating integers.
#define BET_BURNXPERMILLE 60    // Burn promillage

// The supported bet outcome types.
typedef enum OutcomeType {
    moneyLineHomeWin  = 0x01,
    moneyLineAwayWin = 0x02,
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

// The supported mapping TX types.
typedef enum MappingType {
    sportMapping      = 0x01,
    roundMapping      = 0x02,
    teamMapping       = 0x03,
    tournamentMapping = 0x04
} MappingType;

//
typedef enum PayoutType {
    bettingPayout    = 0x01,
    bettingRefund    = 0x02,
    bettingReward    = 0x03,
    chainGamesPayout = 0x04,
    chainGamesRefund = 0x05,
    chainGamesReward = 0x06,
    quickGamesPayout = 0x07,
    quickGamesRefund = 0x08,
    quickGamesReward = 0x09
} PayoutType;

typedef enum BetResultType {
    betResultUnknown = 0x00,
    betResultWin = 0x01,
    betResultLose = 0x02,
    betResultRefund = 0x03,
} BetResultType;

// Class derived from CTxOut
// nBetValue is NOT serialized, nor is it included in the hash or in comparison functions
class CBetOut : public CTxOut {
    private:

    void Set(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn = 0, uint32_t nEventIdIn = 0)
    {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
        nBetValue = nBetValueIn;
        nEventId = nEventIdIn;
    }

    public:

    CAmount nBetValue;
    uint32_t nEventId;

    CBetOut() : CTxOut() {
        SetNull();
    }

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
    {
        Set(nValueIn,scriptPubKeyIn);
    };

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn)
    {
        Set(nValueIn,scriptPubKeyIn, nBetValueIn);
    };

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn, uint32_t nEventIdIn)
    {
        Set(nValueIn,scriptPubKeyIn, nBetValueIn, nEventIdIn);
    };

    void SetNull() {
        CTxOut::SetNull();
        nBetValue = -1;
        nEventId = -1;
    }

    void SetEmpty() {
        CTxOut::SetEmpty();
        nBetValue = 0;
        nEventId = 0;
    }

    bool IsEmpty() const {
        return CTxOut::IsEmpty() && nEventId == 0;
    }

    inline int CompareTo(const CBetOut& rhs) const
    {
        if (nValue < rhs.nValue)
            return -1;
        if (nValue > rhs.nValue)
            return 1;
        if (scriptPubKey < rhs.scriptPubKey)
            return -1;
        if (scriptPubKey > rhs.scriptPubKey)
            return 1;
        if (nEventId < rhs.nEventId)
            return -1;
        if (nEventId > rhs.nEventId)
            return 1;
        return 0;
    }

    inline bool operator==(const CBetOut& rhs) const { return CompareTo(rhs) == 0; }
    inline bool operator!=(const CBetOut& rhs) const { return CompareTo(rhs) != 0; }
    inline bool operator<=(const CBetOut& rhs) const { return CompareTo(rhs) <= 0; }
    inline bool operator>=(const CBetOut& rhs) const { return CompareTo(rhs) >= 0; }
    inline bool operator<(const CBetOut& rhs) const { return CompareTo(rhs) < 0; }
    inline bool operator>(const CBetOut& rhs) const { return CompareTo(rhs) > 0; }
};


/** Ensures a TX has come from an OMNO wallet. **/
bool IsValidOracleTx(const CTxIn &txin);

//* Calculates the amount of coins paid out to bettors and the amount of coins to burn, based on bet amount and odds **/
bool CalculatePayoutBurnAmounts(const CAmount betAmount, const uint32_t odds, CAmount& nPayout, CAmount& nBurn);

/** Find peerless events. **/
std::vector<CPeerlessResultDB> GetEventResults(int nLastBlockHeight);

/** Find chain games lotto result. **/
bool GetCGLottoEventResults(const int nLastBlockHeight, std::vector<CChainGamesResultDB>& chainGameResults);

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
void GetPLRewardPayouts(const uint32_t nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/**
 * Check winning condition for current bet considering locked event and event result.
 *
 * @return Odds, mean if bet is win - return market Odds, if lose - return 0, if refund - return OddDivisor
 */
uint32_t GetBetOdds(const CPeerlessLegDB &bet, const CPeerlessBaseEventDB &lockedEvent, const CPeerlessResultDB &result, const bool fWagerrProtocolV3);

#endif