// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_db.h>
#include <main.h>
#include <util.h>
#include <base58.h>
#include <kernel.h>

/**
 * Creates the bet payout vector for all winning CUniversalBet bets.
 *
 * @return payout vector, payouts info vector.
 */
void GetPLBetPayoutsV3(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    uint64_t refundOdds{BET_ODDSDIVISOR};

    // Get all the results posted in the prev block.
    std::vector<CPeerlessResultDB> results = GetEventResults(nLastBlockHeight);

    bool fWagerrProtocolV3 = nLastBlockHeight >= Params().WagerrProtocolV3StartHeight();

    LogPrintf("Start generating payouts...\n");

    for (auto result : results) {

        LogPrintf("Looking for bets of eventId: %lu\n", result.nEventId);

        // look bets at last 14 days
        uint32_t startHeight = nLastBlockHeight >= Params().BetBlocksIndexTimespan() ? nLastBlockHeight - Params().BetBlocksIndexTimespan() : 0;

        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<PeerlessBetKey, CPeerlessBetDB>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(PeerlessBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {
            PeerlessBetKey uniBetKey;
            CPeerlessBetDB uniBet;
            CBettingDB::BytesToDbType(it->Key(), uniBetKey);
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            // skip if bet is already handled
            if (fWagerrProtocolV3 && uniBet.IsCompleted()) continue;

            bool completedBet = false;
            uint32_t odds = 0;

            // parlay bet
            if (uniBet.legs.size() > 1) {
                bool resultFound = false;
                for (auto leg : uniBet.legs) {
                    // if we found one result for parlay - check win condition for this and each other legs
                    if (leg.nEventId == result.nEventId) {
                        resultFound = true;
                        break;
                    }
                }
                if (resultFound) {
                    // make assumption that parlay is completed and this result is last
                    completedBet = true;
                    // find all results for all legs
                    bool firstOddMultiply = true;
                    for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                        CPeerlessLegDB &leg = uniBet.legs[idx];
                        CPeerlessBaseEventDB &lockedEvent = uniBet.lockedEvents[idx];
                        // skip this bet if incompleted (can't find one result)
                        CPeerlessResultDB res;
                        if (bettingsViewCache.results->Read(ResultKey{leg.nEventId}, res)) {
                            uint32_t betOdds = 0;
                            // if bet placed before 2 mins of event started - refund this bet
                            if (lockedEvent.nStartTime > 0 && uniBet.betTime > (lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                                betOdds = fWagerrProtocolV3 ? refundOdds : 0;
                            }
                            else {
                                betOdds = GetBetOdds(leg, lockedEvent, res, fWagerrProtocolV3);
                            }
                            odds = firstOddMultiply ? (firstOddMultiply = false, betOdds) : static_cast<uint32_t>(((uint64_t) odds * betOdds) / BET_ODDSDIVISOR);
                        }
                        else {
                            completedBet = false;
                            break;
                        }
                    }
                }
            }
            // single bet
            else if (uniBet.legs.size() == 1) {
                CPeerlessLegDB &singleBet = uniBet.legs[0];
                CPeerlessBaseEventDB &lockedEvent = uniBet.lockedEvents[0];

                if (singleBet.nEventId == result.nEventId) {
                    completedBet = true;

                    // if bet placed before 2 mins of event started - refund this bet
                    if (lockedEvent.nStartTime > 0 && uniBet.betTime > (lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                        if (fWagerrProtocolV3) {
                            odds = refundOdds;
                        } else {
                            odds = 0;
                        }
                    } else if ((!fWagerrProtocolV3) && nLastBlockHeight - lockedEvent.nEventCreationHeight > Params().BetBlocksIndexTimespan()) {
                        odds = 0;
                    }
                    else {
                        odds = GetBetOdds(singleBet, lockedEvent, result, fWagerrProtocolV3);
                    }
                }
            }

            if (completedBet) {
                CAmount burn;
                CAmount payout;

                if (uniBet.betAmount < (Params().MinBetPayoutRange() * COIN) || uniBet.betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                    odds = fWagerrProtocolV3 ? refundOdds : 0;
                }
                CalculatePayoutBurnAmounts(uniBet.betAmount, odds, payout, burn);

                if (payout > 0) {
                    // Add winning payout to the payouts vector.
                    CPayoutInfoDB payoutInfo(uniBetKey, odds <= refundOdds ? PayoutType::bettingRefund : PayoutType::bettingPayout);
                    vExpectedPayouts.emplace_back(payout, GetScriptForDestination(uniBet.playerAddress.Get()), uniBet.betAmount);
                    vPayoutsInfo.emplace_back(payoutInfo);

                    uniBet.resultType = odds <= refundOdds ? BetResultType::betResultRefund : BetResultType::betResultWin;
                    // write payout height: result height + 1
                    uniBet.payoutHeight = static_cast<uint32_t>(nNewBlockHeight);
                }
                else {
                    uniBet.resultType = BetResultType::betResultLose;
                }
                uniBet.payout = payout;
                LogPrintf("\nBet %s is handled!\nPlayer address: %s\nPayout: %lu\n", uniBetKey.outPoint.ToStringShort(), uniBet.playerAddress.ToString(), payout);
                LogPrintf("Legs:");
                for (auto leg : uniBet.legs) {
                    LogPrintf(" (eventId: %lu, outcome: %lu) ", leg.nEventId, leg.nOutcome);
                }
                LogPrintf("\n");
                // if handling bet is completed - mark it
                uniBet.SetCompleted();
                vEntriesToUpdate.emplace_back(std::pair<PeerlessBetKey, CPeerlessBetDB>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.bets->Update(pair.first, pair.second);
        }
    }

    GetPLRewardPayouts(nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);

    LogPrintf("Finished generating payouts...\n");

}

/**
 * Creates the bet payout vector for all winning Quick Games bets.
 *
 * @return payout vector.
 */
void GetQuickGamesBetPayouts(CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    PeerlessBetKey zeroKey{0, COutPoint()};

    LogPrintf("Start generating quick games bets payouts...\n");

    CBlockIndex *blockIndex = chainActive[nLastBlockHeight];
    uint32_t blockHeight = static_cast<uint32_t>(nLastBlockHeight);
    auto it = bettingsViewCache.quickGamesBets->NewIterator();
    std::vector<std::pair<QuickGamesBetKey, CQuickGamesBetDB>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(QuickGamesBetKey{blockHeight, COutPoint()})); it->Valid(); it->Next()) {
        QuickGamesBetKey qgKey;
        CQuickGamesBetDB qgBet;

        CBettingDB::BytesToDbType(it->Key(), qgKey);

        if (qgKey.blockHeight != blockHeight)
            break;

        CBettingDB::BytesToDbType(it->Value(), qgBet);
        // skip if already handled
        if (qgBet.IsCompleted())
            continue;

        // invalid game index
        if (qgBet.gameType >= Params().QuickGamesArr().size())
            continue;

        // handle bet by specific game handler from quick games framework
        const CQuickGamesView& gameView = Params().QuickGamesArr()[qgBet.gameType];
        // if odds == 0 - bet lose, if odds > OddsDivisor - bet win, if odds == BET_ODDSDIVISOR - bet refunded
        uint32_t odds = gameView.handler(qgBet.vBetInfo, blockIndex->hashProofOfStake);
        CAmount winningsPermille = qgBet.betAmount * odds;
        CAmount feePermille = winningsPermille > 0 ? (qgBet.betAmount * (odds - BET_ODDSDIVISOR) / 1000 * gameView.nFeePermille) : 0;
        CAmount payout = winningsPermille > 0 ? (winningsPermille - feePermille) / BET_ODDSDIVISOR : 0;

        if (payout > 0) {
            qgBet.resultType = odds == BET_ODDSDIVISOR ? BetResultType::betResultRefund : BetResultType::betResultWin;
            // Add winning payout to the payouts vector.
            CPayoutInfoDB payoutInfo(qgKey, odds == BET_ODDSDIVISOR ? PayoutType::quickGamesRefund : PayoutType::quickGamesPayout);
            CBetOut betOut(payout, GetScriptForDestination(qgBet.playerAddress.Get()), qgBet.betAmount);
            vExpectedPayouts.emplace_back(betOut);
            vPayoutsInfo.emplace_back(payoutInfo);

            // Dev reward
            CAmount devReward = (CAmount)(feePermille / 1000 * gameView.nDevRewardPermille / BET_ODDSDIVISOR);
            CPayoutInfoDB devRewardInfo(zeroKey, PayoutType::quickGamesReward);
            CBetOut devRewardOut(devReward, GetScriptForDestination(CBitcoinAddress(gameView.specialAddress).Get()), qgBet.betAmount);
            vExpectedPayouts.emplace_back(devRewardOut);
            vPayoutsInfo.emplace_back(devRewardInfo);

            // OMNO reward
            std::string OMNOPayoutAddr = Params().OMNOPayoutAddr();
            CAmount nOMNOReward = (CAmount)(feePermille / 1000 * gameView.nOMNORewardPermille / BET_ODDSDIVISOR);

            CPayoutInfoDB omnoRewardInfo(zeroKey, PayoutType::quickGamesReward);
            CBetOut omnoRewardOut(nOMNOReward, GetScriptForDestination(CBitcoinAddress(OMNOPayoutAddr).Get()), qgBet.betAmount);
            vExpectedPayouts.emplace_back(devRewardOut);
            vPayoutsInfo.emplace_back(devRewardInfo);
        }
        else {
            qgBet.resultType = BetResultType::betResultLose;
        }
        LogPrintf("\nQuick game: %s, bet %s is handled!\nPlayer address: %s\nPayout: %ll\n\n", gameView.name, qgKey.outPoint.ToStringShort(), qgBet.playerAddress.ToString(), payout);
        // if handling bet is completed - mark it
        qgBet.SetCompleted();
        qgBet.payout = payout;
        vEntriesToUpdate.emplace_back(std::pair<QuickGamesBetKey, CQuickGamesBetDB>{qgKey, qgBet});
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.quickGamesBets->Update(pair.first, pair.second);
    }
}
