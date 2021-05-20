// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_v3.h>
#include <betting/bet_db.h>
#include <betting/oracles.h>
#include <main.h>
#include <util.h>
#include <base58.h>
#include <kernel.h>

void GetFeildBetPayoutsV4(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    uint64_t refundOdds{BET_ODDSDIVISOR};

    // Get all the results posted in the prev block.
    std::vector<CFieldResultDB> results = GetFieldResults(nLastBlockHeight);

    bool fWagerrProtocolV3 = nLastBlockHeight >= Params().WagerrProtocolV3StartHeight();

    CAmount effectivePayoutsSum, grossPayoutsSum = effectivePayoutsSum = 0;

    LogPrint("wagerr", "Start generating peerless bets payouts...\n");

    for (auto result : results) {

        LogPrint("wagerr", "Looking for bets of eventId: %lu\n", result.nEventId);
    }
}