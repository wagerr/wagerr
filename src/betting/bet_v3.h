// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_V3_BET_H
#define WAGERR_V3_BET_H

#include "amount.h"

class CBetOut;
class CBettingsView;
class CPayoutInfo;
class CPeerlessBet;
class CPeerlessEvent;
class CPeerlessResult;

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
int64_t GetBlockPayouts(std::multimap<CPayoutInfo, CBetOut>& mExpectedPayouts, CAmount& nMNBetReward, uint32_t nBlockHeight);

/** Aggregates the amount of WGR to be minted to pay out all CG Lotto winners as well as OMNO rewards. **/
int64_t GetCGBlockPayoutsValue(const std::multimap<CPayoutInfo, CBetOut>& mExpectedCGPayouts);

/** Using betting database for handle bets **/
void GetBetPayouts(CBettingsView &bettingsViewCache, int height, std::multimap<CPayoutInfo, CBetOut>& mExpectedPayouts, const bool fWagerrProtocolV3, bool fUpdate = true);

/** Get the chain games winner and return the payout vector. **/
void GetCGLottoBetPayouts(int height, std::multimap<CPayoutInfo, CBetOut>& mExpectedPayouts);

uint32_t GetBetOdds(const CPeerlessBet &bet, const CPeerlessEvent &lockedEvent, const CPeerlessResult &result, const bool fWagerrProtocolV3);

#endif // WAGERR_V2_BET_H