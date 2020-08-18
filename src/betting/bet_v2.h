// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_V2_BET_H
#define WAGERR_V2_BET_H

#include <util.h>
#include <amount.h>

class CBetOut;
class CPayoutInfoDB;

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
void GetPLRewardPayoutsV2(const uint32_t nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/** Get the peerless winning bets from the block chain and return the payout vector. **/
void GetBetPayoutsV2(const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);
/** Get the chain games winner and return the payout vector. **/
void GetCGLottoBetPayoutsV2(const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

#endif // WAGERR_V2_BET_H