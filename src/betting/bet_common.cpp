// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_tx.h>
#include <betting/bet_db.h>
#include <betting/oracles.h>
#include <main.h>

/**
 * Validate the transaction to ensure it has been posted by an oracle node.
 *
 * @param txin  TX vin input hash.
 * @return      Bool
 */
bool IsValidOracleTx(const CTxIn &txin, int nHeight)
{
    COutPoint prevout = txin.prevout;
    std::vector<COracle> oracles = Params().Oracles();

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
                const std::string strPrevAddr = CBitcoinAddress(prevAddr).ToString();
                if (std::find_if(oracles.begin(), oracles.end(), [strPrevAddr, nHeight](COracle oracle){
                    return oracle.IsMyOracleTx(strPrevAddr, nHeight);
                }) != oracles.end()) {
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
    LogPrint("wagerr", "Winnings: %d Payout: %d Burn: %d\n", bWinningsT.getuint256().Get64(), nPayout, nBurn);
    return true;
}

/**
 * Check a given block to see if it contains a Peerless result TX.
 *
 * @return results vector.
 */
std::vector<CPeerlessResultDB> GetPLResults(int nLastBlockHeight)
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
        bool validResultTx = IsValidOracleTx(txin, nLastBlockHeight);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (const CTxOut &txOut : tx.vout) {

                auto bettingTx = ParseBettingTx(txOut);

                if (bettingTx == nullptr || bettingTx->GetTxType() != plResultTxType) continue;

                CPeerlessResultTx* resultTx = (CPeerlessResultTx *)bettingTx.get();

                LogPrint("wagerr", "Result for event %lu was found...\n", resultTx->nEventId);

                // Store the result if its a valid result OP CODE.
                results.emplace_back(resultTx->nEventId, resultTx->nResultType, resultTx->nHomeScore, resultTx->nAwayScore);
                if (!fMultipleResultsAllowed) return results;
            }
        }
    }

    return results;
}

/**
 * Check a given block to see if it contains a Field result TX.
 *
 * @return results vector.
 */
std::vector<CFieldResultDB> GetFieldResults(int nLastBlockHeight)
{
    std::vector<CFieldResultDB> results;

    bool fMultipleResultsAllowed = (nLastBlockHeight >= Params().WagerrProtocolV3StartHeight());

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[nLastBlockHeight];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    for (CTransaction& tx : block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet.
        const CTxIn &txin  = tx.vin[0];
        bool validResultTx = IsValidOracleTx(txin, nLastBlockHeight);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (const CTxOut &txOut : tx.vout) {

                auto bettingTx = ParseBettingTx(txOut);

                if (bettingTx == nullptr || bettingTx->GetTxType() != fResultTxType) continue;

                CFieldResultTx* resultTx = (CFieldResultTx *)bettingTx.get();

                LogPrint("wagerr", "Result for field event %lu was found...\n", resultTx->nEventId);

                // Store the result if its a valid result OP CODE.
                results.emplace_back(resultTx->nEventId, resultTx->nResultType, resultTx->contendersResults);
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

        bool validResultTx = IsValidOracleTx(txin, nLastBlockHeight);

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

uint32_t CalculateEffectiveOdds(uint32_t onChainOdds) {
    if (onChainOdds == 0 ||
            onChainOdds == BET_ODDSDIVISOR / 2 ||
            onChainOdds == BET_ODDSDIVISOR) {
        return onChainOdds;
    }
    return static_cast<uint32_t>(((static_cast<uint64_t>(onChainOdds) - BET_ODDSDIVISOR) * 9400) / BET_ODDSDIVISOR + BET_ODDSDIVISOR);
}

std::pair<uint32_t, uint32_t> CalculateAsianSpreadOdds(const CPeerlessBaseEventDB &lockedEvent, const int32_t difference, bool spreadHomeOutcome)
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
    uint32_t oc_odd1, oc_odd2 = oc_odd1 = 0;
    uint32_t eff_odd1, eff_odd2 = eff_odd1 = 0;
    // calculate spread odds for 2 conditions of game and return summ of odds
    if (sp1 + difference == 0) {
        eff_odd1 = oc_odd1 = BET_ODDSDIVISOR;
    } else if (sp1 + difference > 0) {
        oc_odd1 = spreadHomeOutcome ? lockedEvent.nSpreadHomeOdds : lockedEvent.nSpreadAwayOdds;
        eff_odd1 = CalculateEffectiveOdds(oc_odd1);
    }
    else {
        eff_odd1 = oc_odd1 = 0;
    }
    if (sp2 + difference == 0) {
        eff_odd2 = oc_odd2 =  BET_ODDSDIVISOR;
    } else if (sp2 + difference > 0) {
        oc_odd2 = spreadHomeOutcome ? lockedEvent.nSpreadHomeOdds : lockedEvent.nSpreadAwayOdds;
        eff_odd2 = CalculateEffectiveOdds(oc_odd2);
    }
    else {
        eff_odd2 = oc_odd2 = 0;
    }
    return {((BET_ODDSDIVISOR / 2) * oc_odd1) / BET_ODDSDIVISOR + ((BET_ODDSDIVISOR / 2) * oc_odd2) / BET_ODDSDIVISOR,
            ((BET_ODDSDIVISOR / 2) * eff_odd1) / BET_ODDSDIVISOR + ((BET_ODDSDIVISOR / 2) * eff_odd2) / BET_ODDSDIVISOR};
}

/**
 * Check winning condition for current bet considering locked event and event result.
 *
 * @return pair of {on_chain_odds, effective_odds}, mean if bet is win - return market Odds, if lose - return 0, if refund - return OddDivisor
 */
std::pair<uint32_t, uint32_t> GetBetOdds(const CPeerlessLegDB &bet, const CPeerlessBaseEventDB &lockedEvent, const CPeerlessResultDB &result, const bool fWagerrProtocolV3)
{
    bool fLegacyInitialHomeFavorite = lockedEvent.fLegacyInitialHomeFavorite;
    uint32_t refundOdds{BET_ODDSDIVISOR};
    int32_t legacySpreadDiff = fLegacyInitialHomeFavorite ? result.nHomeScore - result.nAwayScore : result.nAwayScore - result.nHomeScore;
    uint32_t totalPoints = result.nHomeScore + result.nAwayScore;
    if (result.nResultType == ResultType::eventRefund)
        return {refundOdds, refundOdds};
    switch (bet.nOutcome) {
        case moneyLineHomeWin:
            if (result.nResultType == ResultType::mlRefund || (lockedEvent.nHomeOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (result.nHomeScore > result.nAwayScore) return {lockedEvent.nHomeOdds, CalculateEffectiveOdds(lockedEvent.nHomeOdds)};
            break;
        case moneyLineAwayWin:
            if (result.nResultType == ResultType::mlRefund || (lockedEvent.nAwayOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (result.nAwayScore > result.nHomeScore) return {lockedEvent.nAwayOdds, CalculateEffectiveOdds(lockedEvent.nAwayOdds)};
            break;
        case moneyLineDraw:
            if (result.nResultType == ResultType::mlRefund || (lockedEvent.nDrawOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (result.nHomeScore == result.nAwayScore) return {lockedEvent.nDrawOdds, CalculateEffectiveOdds(lockedEvent.nDrawOdds)};
            break;
        case spreadHome:
            if (result.nResultType == ResultType::spreadsRefund || (lockedEvent.nSpreadHomeOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (!fWagerrProtocolV3) {
                if (legacySpreadDiff == lockedEvent.nSpreadPoints) return {refundOdds, refundOdds};
                if (fLegacyInitialHomeFavorite) {
                    // mean bet to home will win with spread
                    if (legacySpreadDiff > lockedEvent.nSpreadPoints) return {lockedEvent.nSpreadHomeOdds, CalculateEffectiveOdds(lockedEvent.nSpreadHomeOdds)};
                }
                else {
                    // mean bet to home will not lose with spread
                    if (legacySpreadDiff < lockedEvent.nSpreadPoints) return {lockedEvent.nSpreadHomeOdds, CalculateEffectiveOdds(lockedEvent.nSpreadHomeOdds)};
                }
            }
            else { // lockedEvent.nSpreadVersion == 2
                int32_t difference = result.nHomeScore - result.nAwayScore;
                // if its 0 or 0.5 like handicap
                if (lockedEvent.nSpreadPoints % 50 == 0) {
                    if (lockedEvent.nSpreadPoints + difference == 0) {
                        return {refundOdds, refundOdds};
                    } else if (lockedEvent.nSpreadPoints + difference > 0 ) {
                        return {lockedEvent.nSpreadHomeOdds, CalculateEffectiveOdds(lockedEvent.nSpreadHomeOdds)};
                    }
                }
                // if its 0.25 or 0.75 like handicap
                else if (lockedEvent.nSpreadPoints % 25 == 0) {
                    return CalculateAsianSpreadOdds(lockedEvent, difference, true);
                }
            }
            break;
        case spreadAway:
            if (result.nResultType == ResultType::spreadsRefund || (lockedEvent.nSpreadAwayOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (!fWagerrProtocolV3) {
                if (legacySpreadDiff == lockedEvent.nSpreadPoints) return {refundOdds, refundOdds};
                if (fLegacyInitialHomeFavorite) {
                    // mean that bet to away will not lose with spread
                    if (legacySpreadDiff < lockedEvent.nSpreadPoints) return {lockedEvent.nSpreadAwayOdds, CalculateEffectiveOdds(lockedEvent.nSpreadAwayOdds)};
                }
                else {
                    // mean that bet to away will win with spread
                    if (legacySpreadDiff > lockedEvent.nSpreadPoints) return {lockedEvent.nSpreadAwayOdds, CalculateEffectiveOdds(lockedEvent.nSpreadAwayOdds)};
                }
            }
            else { // lockedEvent.nSpreadVersion == 2
                int32_t difference = result.nAwayScore - result.nHomeScore;
                // if its 0.5 like handicap
                if (lockedEvent.nSpreadPoints % 50 == 0) {
                    if ((lockedEvent.nSpreadPoints * (-1)) + difference == 0) {
                        return {refundOdds, refundOdds};
                    } else if ((lockedEvent.nSpreadPoints * (-1)) + difference > 0) {
                        return {lockedEvent.nSpreadAwayOdds, CalculateEffectiveOdds(lockedEvent.nSpreadAwayOdds)};
                    }
                }
                // if its 0.25 or 0.75 handicap
                else if (lockedEvent.nSpreadPoints % 25 == 0) {
                    return CalculateAsianSpreadOdds(lockedEvent, difference, false);
                }
            }
            break;
        case totalOver:
            if (result.nResultType == ResultType::totalsRefund || (lockedEvent.nTotalOverOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (totalPoints == lockedEvent.nTotalPoints) return {refundOdds, refundOdds};
            if (totalPoints > lockedEvent.nTotalPoints) return {lockedEvent.nTotalOverOdds, CalculateEffectiveOdds(lockedEvent.nTotalOverOdds)};
            break;
        case totalUnder:
            if (result.nResultType == ResultType::totalsRefund || (lockedEvent.nTotalUnderOdds == 0 && fWagerrProtocolV3)) return {refundOdds, refundOdds};
            if (totalPoints == lockedEvent.nTotalPoints) return {refundOdds, refundOdds};
            if (totalPoints < lockedEvent.nTotalPoints) return {lockedEvent.nTotalUnderOdds, CalculateEffectiveOdds(lockedEvent.nTotalUnderOdds)};
            break;
        default:
            std::runtime_error("Unknown bet outcome type!");
    }
    // bet lose
    return {0, 0};
}

/**
 * Check winning condition for current bet considering locked event and event result.
 *
 * @return pair of {on_chain_odds, effective_odds}, mean if bet is win - return market Odds, if lose - return 0, if refund - return OddDivisor
 */
std::pair<uint32_t, uint32_t> GetBetOdds(const CFieldLegDB &bet, const CFieldEventDB &lockedEvent, const CFieldResultDB &result, const bool fWagerrProtocolV4)
{
    if (result.nResultType == ResultType::eventRefund) {
        return {BET_ODDSDIVISOR, BET_ODDSDIVISOR};
    }
    uint8_t contederResult = result.contendersResults.find(bet.nContenderId) != result.contendersResults.end() ?
        result.contendersResults.at(bet.nContenderId) : (uint8_t)ContenderResult::DNF;
    ContenderInfo contenderInfo = lockedEvent.contenders.find(bet.nContenderId) != lockedEvent.contenders.end() ?
        lockedEvent.contenders.at(bet.nContenderId) : ContenderInfo{};
    switch (bet.nOutcome)
    {
        case FieldBetOutcomeType::outright:
            if (contederResult == ContenderResult::place1) {
                return {contenderInfo.nOutrightOdds, CalculateEffectiveOdds(contenderInfo.nOutrightOdds)};
            }
            break;
        case FieldBetOutcomeType::place:
            if (contederResult == ContenderResult::place1 ||
                    contederResult == ContenderResult::place2) {
                return {contenderInfo.nPlaceOdds, CalculateEffectiveOdds(contenderInfo.nPlaceOdds)};
            }
            break;
        case FieldBetOutcomeType::show:
            if (contederResult == ContenderResult::place1 ||
                    contederResult == ContenderResult::place2 ||
                    contederResult == ContenderResult::place3) {
                return {contenderInfo.nShowOdds, CalculateEffectiveOdds(contenderInfo.nShowOdds)};
            }
            break;
        default:
            break;
    }
    // bet lose
    return {0, 0};
}

uint32_t GetBetPotentialOdds(const CPeerlessLegDB &bet, const CPeerlessBaseEventDB &lockedEvent)
{
    switch(bet.nOutcome) {
        case moneyLineHomeWin:
            return lockedEvent.nHomeOdds;
        case moneyLineAwayWin:
            return lockedEvent.nAwayOdds;
        case moneyLineDraw:
            return lockedEvent.nDrawOdds;
        case spreadHome:
            return lockedEvent.nSpreadHomeOdds;
        case spreadAway:
            return lockedEvent.nSpreadAwayOdds;
        case totalOver:
            return lockedEvent.nTotalOverOdds;
        case totalUnder:
            return lockedEvent.nTotalUnderOdds;
        default:
            std::runtime_error("Unknown bet outcome type!");
    }
    return 0;
}

uint32_t GetBetPotentialOdds(const CFieldLegDB &bet, const CFieldEventDB &lockedEvent)
{
    auto contender_it = lockedEvent.contenders.find(bet.nContenderId);
    if (contender_it == lockedEvent.contenders.end()) {
        return 0;
    }

    switch (bet.nOutcome)
    {
    case outright:
        return contender_it->second.nOutrightOdds;
    case place:
        return contender_it->second.nPlaceOdds;
    case show:
        return contender_it->second.nShowOdds;
    default:
        return 0;
    }
}
