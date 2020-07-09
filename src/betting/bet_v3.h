// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_V3_BET_H
#define WAGERR_V3_BET_H

#include <amount.h>

class CBetOut;
class CPayoutInfoDB;
class CBettingsView;
class CPeerlessLegDB;
class CPeerlessBaseEventDB;
class CPeerlessResultDB;


/** Using betting database for handle bets **/
void GetPLBetPayoutsV3(CBettingsView &bettingsViewCache, int height, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

uint32_t GetBetOdds(const CPeerlessLegDB &bet, const CPeerlessBaseEventDB &lockedEvent, const CPeerlessResultDB &result, const bool fWagerrProtocolV3);

/* Creates the bet payout vector for all winning Quick Games bets */
uint32_t GetQuickGamesBetPayouts(CBettingsView& bettingsViewCache, const int height, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

bool UndoQuickGamesBetPayouts(CBettingsView &bettingsViewCache, int height);

#endif // WAGERR_V3_BET_H