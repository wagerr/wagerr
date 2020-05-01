// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_V2_BET_H
#define WAGERR_V2_BET_H

#include "amount.h"

class CBetOut;
class CPayoutInfo;

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
int64_t GetBlockPayoutsV2(std::vector<CBetOut>& vExpectedPayouts, CAmount& nMNBetReward, std::vector<CPayoutInfo>& vPayoutsInfo);

/** Aggregates the amount of WGR to be minted to pay out all CG Lotto winners as well as OMNO rewards. **/
int64_t GetCGBlockPayoutsV2(std::vector<CBetOut>& vexpectedCGPayouts, CAmount& nMNBetReward);

/** Get the peerless winning bets from the block chain and return the payout vector. **/
void GetBetPayoutsV2(int height, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfo>& vPayoutsInfo);
/** Get the chain games winner and return the payout vector. **/
void GetCGLottoBetPayoutsV2(int height, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfo>& vPayoutsInfo);

#endif // WAGERR_V2_BET_H