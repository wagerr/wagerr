// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_V2_BET_H
#define WAGERR_V2_BET_H

#include <util.h>
#include <amount.h>

class CBetOut;
class CPayoutInfoDB;

class LegacyPayout
{
public:
    uint16_t payoutType;
    uint32_t blockHeight;
    int vtxNr;
    CTxOut txOut;

    LegacyPayout(uint16_t payoutTypeIn, uint32_t blockHeightIn, int vtxNrIn, CTxOut txOutIn) :
        payoutType(payoutTypeIn), blockHeight(blockHeightIn), vtxNr(vtxNrIn), txOut(txOutIn) {};

    bool operator<(const LegacyPayout& rhs) const {
        if (payoutType != rhs.payoutType) return payoutType < rhs.payoutType;

        if (blockHeight != rhs.blockHeight) return blockHeight < rhs.blockHeight;

        return vtxNr < rhs.vtxNr;
    }
};

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
void GetPLRewardPayoutsV2(const uint32_t nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/** Get the peerless winning bets from the block chain and return the payout vector. **/
void GetBetPayoutsV2(const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);
/** Get the chain games winner and return the payout vector. **/
void GetCGLottoBetPayoutsV2(const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

#endif // WAGERR_V2_BET_H