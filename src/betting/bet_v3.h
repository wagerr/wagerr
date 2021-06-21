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
class CChainGamesEventDB;
class CChainGamesBetDB;
class CChainGamesResultDB;


void GetPLRewardPayoutsV3(const uint32_t nNewBlockHeight, const CAmount fee, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/** Using betting database for handle bets **/
void GetPLBetPayoutsV3(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/* Creates the bet payout vector for all winning Quick Games bets */
void GetQuickGamesBetPayouts(CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

/** Get the chain games winner and return the payout vector. **/
void GetCGLottoBetPayoutsV3(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo);

uint32_t GetBetSearchStartHeight(int nHeight);

bool UndoPLBetPayouts(CBettingsView &bettingsViewCache, int height);
bool UndoQGBetPayouts(CBettingsView &bettingsViewCache, int height);

#endif // WAGERR_V3_BET_H