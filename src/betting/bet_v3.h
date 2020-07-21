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
void GetPLBetPayoutsV3(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/* Creates the bet payout vector for all winning Quick Games bets */
uint32_t GetQuickGamesBetPayouts(CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

#endif // WAGERR_V3_BET_H