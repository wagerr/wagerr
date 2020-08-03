// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

#include <util.h>
#include <amount.h>

class CBettingsView;
class CPayoutInfoDB;
class CBetOut;
class CTransaction;
class CBlock;

extern CBettingsView *bettingsView;

/** Validating the payout block using the payout vector. **/
bool IsBlockPayoutsValid(CBettingsView &bettingsViewCache, const std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayoutsIn, const CBlock& block, const int nBlockHeight, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward);

/** Check Betting Tx when try accept tx to memory pool **/
bool CheckBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height);

/** Parse the transaction for betting data **/
void ProcessBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height, const int64_t blockTime, const bool wagerrProtocolV3);

CAmount GetBettingPayouts(CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayouts);

bool BettingUndo(CBettingsView& bettingsViewCache, int height, const std::vector<CTransaction>& vtx);

#endif // WAGERR_BET_H
