// Copyright (c) 2021 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_V4_BET_H
#define WAGERR_V4_BET_H

#include <amount.h>

class CPayoutInfoDB;

/**
 * Creates the bet payout vector for all winning CFieldBetDB bets.
 *
 * @return payout vector, payouts info vector.
 */
void GetFeildBetPayoutsV4(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/**
 * Undo only bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoFieldBetPayouts(CBettingsView &bettingsViewCache, int height);

#endif