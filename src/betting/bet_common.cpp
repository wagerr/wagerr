// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_tx.h>
#include <betting/bet_db.h>
#include <main.h>

/**
 * Validate the transaction to ensure it has been posted by an oracle node.
 *
 * @param txin  TX vin input hash.
 * @return      Bool
 */
bool IsValidOracleTx(const CTxIn &txin)
{
    COutPoint prevout = txin.prevout;
    std::vector<std::string> oracleAddrs = Params().OracleWalletAddrs();

    uint256 hashBlock;
    CTransaction txPrev;
    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

        const CTxOut &prevTxOut = txPrev.vout[prevout.n];
        std::string scriptPubKey = prevTxOut.scriptPubKey.ToString();

        txnouttype type;
        std::vector<CTxDestination> prevAddrs;
        int nRequired;

        if (ExtractDestinations(prevTxOut.scriptPubKey, type, prevAddrs, nRequired)) {
            for (const CTxDestination &prevAddr : prevAddrs) {
                if (std::find(oracleAddrs.begin(), oracleAddrs.end(), CBitcoinAddress(prevAddr).ToString()) != oracleAddrs.end()) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool CalculatePayoutBurnAmounts(const CAmount betAmount, const uint32_t odds, CAmount& nPayout, CAmount& nBurn) {
    if (odds == 0) {
        nPayout = 0;
        nBurn = 0;
        return false;
    }
    else if (odds > 0 && odds <= BET_ODDSDIVISOR) {
        nPayout = betAmount * odds / BET_ODDSDIVISOR;
        nBurn = 0;
        return true;
    }
    // Events with odds > 92 can cause an overflow with winnings calculations when using uint64_t
    const CBigNum bBetAmount = CBigNum(uint256(betAmount));
    const CBigNum bOdds = CBigNum(uint256(odds));

    CBigNum bWinningsT = bBetAmount * bOdds;
    CBigNum bPayout = (bWinningsT - (((bWinningsT - bBetAmount * BET_ODDSDIVISOR) / 1000) * 60)) / BET_ODDSDIVISOR;
    CBigNum bBurn = bPayout - (bWinningsT / BET_ODDSDIVISOR);

    nPayout = bPayout.getuint256().Get64();
    nBurn = bBurn.getuint256().Get64();
    LogPrintf("bWinnings: %d bPayout: %d bBurn: %d\n", bWinningsT.getuint256().Get64(), nPayout, nBurn);
    return true;
}

/**
 * Check a given block to see if it contains a Peerless result TX.
 *
 * @return results vector.
 */
std::vector<CPeerlessResultDB> GetEventResults(int nLastBlockHeight)
{
    std::vector<CPeerlessResultDB> results;

    bool fMultipleResultsAllowed = (nLastBlockHeight >= Params().WagerrProtocolV3StartHeight());

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[nLastBlockHeight];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    for (CTransaction& tx : block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet.
        const CTxIn &txin  = tx.vin[0];
        bool validResultTx = IsValidOracleTx(txin);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (const CTxOut &txOut : tx.vout) {

                auto bettingTx = ParseBettingTx(txOut);

                if (bettingTx == nullptr || bettingTx->GetTxType() != plResultTxType) continue;

                CPeerlessResultTx* resultTx = (CPeerlessResultTx *)bettingTx.get();

                LogPrintf("Result for event %lu was found...\n", resultTx->nEventId);

                // Store the result if its a valid result OP CODE.
                results.emplace_back(resultTx->nEventId, resultTx->nResultType, resultTx->nHomeScore, resultTx->nAwayScore);
                if (!fMultipleResultsAllowed) return results;
            }
        }
    }

    return results;
}

/**
 * Checks a given block for any Chain Games results.
 *
 * @param height The block we want to check for the result.
 * @return results array.
 */
bool GetCGLottoEventResults(const int nLastBlockHeight, std::vector<CChainGamesResultDB>& chainGameResults)
{
    chainGameResults.clear();

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = chainActive[nLastBlockHeight];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    for (CTransaction& tx : block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet by looking at the TX vins.
        const CTxIn &txin = tx.vin[0];
        uint256 hashBlock;
        CTransaction txPrev;

        bool validResultTx = IsValidOracleTx(txin);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (const CTxOut& txOut : tx.vout) {

                auto bettingTx = ParseBettingTx(txOut);

                if (bettingTx == nullptr || bettingTx->GetTxType() != cgResultTxType) continue;

                CChainGamesResultTx* resultTx = (CChainGamesResultTx *)bettingTx.get();

                chainGameResults.emplace_back(resultTx->nEventId);
            }
        }
    }

    return (chainGameResults.size() > 0);
}

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all bets.
 * We also calculate and add the OMNO and dev fund rewards.
 *
 * @param vExpectedPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
void GetPLRewardPayouts(const uint32_t nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    CAmount profitAcc = 0;
    CAmount totalAmountBet = 0;
    PeerlessBetKey zeroKey{nNewBlockHeight, COutPoint()};

    // Set the OMNO and Dev reward addresses
    CScript payoutScriptDev = GetScriptForDestination(CBitcoinAddress(Params().DevPayoutAddr()).Get());
    CScript payoutScriptOMNO = GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get());

    // Loop over the payout vector and aggregate values.
    for (int i = 0; i < vExpectedPayouts.size(); i++)
    {
        if (vPayoutsInfo[i].payoutType == PayoutType::bettingPayout) {
            CAmount betValue = vExpectedPayouts[i].nBetValue;
            CAmount payValue = vExpectedPayouts[i].nValue;

            totalAmountBet += betValue;
            profitAcc += payValue - betValue;
        }
    }

    // Calculate the OMNO reward and the Dev reward.
    CAmount nOMNOReward = (CAmount)(profitAcc * Params().OMNORewardPermille() / (1000.0 - BET_BURNXPERMILLE));
    CAmount nDevReward  = (CAmount)(profitAcc * Params().DevRewardPermille() / (1000.0 - BET_BURNXPERMILLE));
    if (nDevReward > 0) {
        // Add both reward payouts to the payout vector.
        CBetOut betOutDev(nDevReward, payoutScriptDev, profitAcc);
        CPayoutInfoDB payoutInfoDev(zeroKey, PayoutType::bettingReward);
        vExpectedPayouts.emplace_back(betOutDev);
        vPayoutsInfo.emplace_back(payoutInfoDev);
    }
    if (nOMNOReward > 0) {
        CBetOut betOutOMNO(nOMNOReward, payoutScriptOMNO, profitAcc);
        CPayoutInfoDB payoutInfoOMNO(zeroKey, PayoutType::bettingReward);
        vExpectedPayouts.emplace_back(betOutOMNO);
        vPayoutsInfo.emplace_back(payoutInfoOMNO);
    }
}

uint32_t CalculateAsianSpreadOdds(const CPeerlessBaseEventDB &lockedEvent, const int32_t difference, bool spreadHomeOutcome)
{
    // calculate asian handicap spread points
    int32_t sign = lockedEvent.nSpreadPoints < 0 ? -1 : 1;
    int32_t sp1 = abs(lockedEvent.nSpreadPoints / 50) * 50 * sign;
    int32_t sp2 = (abs(lockedEvent.nSpreadPoints / 50) + 1) * 50 * sign;
    // if outcome is spread away - change sign for spread points
    if (!spreadHomeOutcome) {
        sp1 = -sp1;
        sp2 = -sp2;
    }
    uint32_t odd1, odd2 = odd1 = 0;
    // calculate spread odds for 2 conditions of game and return summ of odds
    if (sp1 + difference == 0) {
        odd1 = BET_ODDSDIVISOR;
    } else if (sp1 + difference > 0) {
        odd1 = spreadHomeOutcome ? lockedEvent.nSpreadHomeOdds : lockedEvent.nSpreadAwayOdds;
    }
    else {
        odd1 = 0;
    }
    if (sp2 + difference == 0) {
        odd2 =  BET_ODDSDIVISOR;
    } else if (sp2 + difference > 0) {
        odd2 = spreadHomeOutcome ? lockedEvent.nSpreadHomeOdds : lockedEvent.nSpreadAwayOdds;
    }
    else {
        odd2 = 0;
    }
    return ((BET_ODDSDIVISOR / 2) * odd1) / BET_ODDSDIVISOR + ((BET_ODDSDIVISOR / 2) * odd2) / BET_ODDSDIVISOR;
}

/**
 * Check winning condition for current bet considering locked event and event result.
 *
 * @return Odds, mean if bet is win - return market Odds, if lose - return 0, if refund - return OddDivisor
 */
uint32_t GetBetOdds(const CPeerlessLegDB &bet, const CPeerlessBaseEventDB &lockedEvent, const CPeerlessResultDB &result, const bool fWagerrProtocolV3)
{
    bool fLegacyInitialHomeFavorite = lockedEvent.fLegacyInitialHomeFavorite;
    uint32_t refundOdds{BET_ODDSDIVISOR};
    int32_t legacySpreadDiff = fLegacyInitialHomeFavorite ? result.nHomeScore - result.nAwayScore : result.nAwayScore - result.nHomeScore;
    uint32_t totalPoints = result.nHomeScore + result.nAwayScore;
    if (result.nResultType == ResultType::eventRefund)
        return refundOdds;
    switch (bet.nOutcome) {
        case moneyLineHomeWin:
            if (result.nResultType == ResultType::mlRefund || (lockedEvent.nHomeOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (result.nHomeScore > result.nAwayScore) return lockedEvent.nHomeOdds;
            break;
        case moneyLineAwayWin:
            if (result.nResultType == ResultType::mlRefund || (lockedEvent.nAwayOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (result.nAwayScore > result.nHomeScore) return lockedEvent.nAwayOdds;
            break;
        case moneyLineDraw:
            if (result.nResultType == ResultType::mlRefund || (lockedEvent.nDrawOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (result.nHomeScore == result.nAwayScore) return lockedEvent.nDrawOdds;
            break;
        case spreadHome:
            if (result.nResultType == ResultType::spreadsRefund || (lockedEvent.nSpreadHomeOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (!fWagerrProtocolV3) {
                if (legacySpreadDiff == lockedEvent.nSpreadPoints) return refundOdds;
                if (fLegacyInitialHomeFavorite) {
                    // mean bet to home will win with spread
                    if (legacySpreadDiff > lockedEvent.nSpreadPoints) return lockedEvent.nSpreadHomeOdds;
                }
                else {
                    // mean bet to home will not lose with spread
                    if (legacySpreadDiff < lockedEvent.nSpreadPoints) return lockedEvent.nSpreadHomeOdds;
                }
            }
            else { // lockedEvent.nSpreadVersion == 2
                int32_t difference = result.nHomeScore - result.nAwayScore;
                // if its 0 or 0.5 like handicap
                if (lockedEvent.nSpreadPoints % 50 == 0) {
                    if (lockedEvent.nSpreadPoints + difference == 0) {
                        return refundOdds;
                    } else if (lockedEvent.nSpreadPoints + difference > 0 ) {
                        return lockedEvent.nSpreadHomeOdds;
                    }
                }
                // if its 0.25 or 0.75 like handicap
                else if (lockedEvent.nSpreadPoints % 25 == 0) {
                    return CalculateAsianSpreadOdds(lockedEvent, difference, true);
                }
            }
            break;
        case spreadAway:
            if (result.nResultType == ResultType::spreadsRefund || (lockedEvent.nSpreadAwayOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (!fWagerrProtocolV3) {
                if (legacySpreadDiff == lockedEvent.nSpreadPoints) return refundOdds;
                if (fLegacyInitialHomeFavorite) {
                    // mean that bet to away will not lose with spread
                    if (legacySpreadDiff < lockedEvent.nSpreadPoints) return lockedEvent.nSpreadAwayOdds;
                }
                else {
                    // mean that bet to away will win with spread
                    if (legacySpreadDiff > lockedEvent.nSpreadPoints) return lockedEvent.nSpreadAwayOdds;
                }
            }
            else { // lockedEvent.nSpreadVersion == 2
                int32_t difference = result.nAwayScore - result.nHomeScore;
                // if its 0.5 like handicap
                if (lockedEvent.nSpreadPoints % 50 == 0) {
                    if ((lockedEvent.nSpreadPoints * (-1)) + difference == 0) {
                        return refundOdds;
                    } else if ((lockedEvent.nSpreadPoints * (-1)) + difference > 0) {
                        return lockedEvent.nSpreadAwayOdds;
                    }
                }
                // if its 0.25 or 0.75 handicap
                else if (lockedEvent.nSpreadPoints % 25 == 0) {
                    return CalculateAsianSpreadOdds(lockedEvent, difference, false);
                }
            }
            break;
        case totalOver:
            if (result.nResultType == ResultType::totalsRefund || (lockedEvent.nTotalOverOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (totalPoints == lockedEvent.nTotalPoints) return refundOdds;
            if (totalPoints > lockedEvent.nTotalPoints) return lockedEvent.nTotalOverOdds;
            break;
        case totalUnder:
            if (result.nResultType == ResultType::totalsRefund || (lockedEvent.nTotalUnderOdds == 0 && fWagerrProtocolV3)) return refundOdds;
            if (totalPoints == lockedEvent.nTotalPoints) return refundOdds;
            if (totalPoints < lockedEvent.nTotalPoints) return lockedEvent.nTotalUnderOdds;
            break;
        default:
            std::runtime_error("Unknown bet outcome type!");
    }
    // bet lose
    return 0;
}
