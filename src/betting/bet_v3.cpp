// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet_v3.h"

#include "bet.h"
#include "kernel.h"
#include "main.h"

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all bets.
 * We also calculate and add the OMNO and dev fund rewards.
 *
 * @param vExpectedPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
int64_t GetBlockPayouts(std::multimap<CPayoutInfo, CBetOut>& mExpectedPayouts, CAmount& nMNBetReward, uint32_t nBlockHeight)
{
    CAmount profitAcc = 0;
    CAmount nPayout = 0;
    CAmount totalAmountBet = 0;
    UniversalBetKey zeroKey{nBlockHeight, COutPoint()};

    // Set the OMNO and Dev reward addresses
    CScript payoutScriptDev = GetScriptForDestination(CBitcoinAddress(Params().DevPayoutAddr()).Get());
    CScript payoutScriptOMNO = GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get());

    // Loop over the payout vector and aggregate values.
    for (auto payout : mExpectedPayouts)
    {
        if (payout.first.payoutType == PayoutType::bettingPayout) {
            CAmount betValue = payout.second.nBetValue;
            CAmount payValue = payout.second.nValue;

            totalAmountBet += betValue;
            profitAcc += payValue - betValue;
            nPayout += payValue;
        }
    }

    // Calculate the OMNO reward and the Dev reward.
    CAmount nOMNOReward = (CAmount)(profitAcc * Params().OMNORewardPermille() / (1000.0 - BET_BURNXPERMILLE));
    CAmount nDevReward  = (CAmount)(profitAcc * Params().DevRewardPermille() / (1000.0 - BET_BURNXPERMILLE));
    if (nDevReward > 0) {
        // Add both reward payouts to the payout vector.
        const CBetOut betOutDev(nDevReward, payoutScriptDev, profitAcc);
        const CPayoutInfo payoutInfoDev(zeroKey, PayoutType::bettingReward);
        mExpectedPayouts.insert(std::pair<const CPayoutInfo, CBetOut>{payoutInfoDev, betOutDev});

        nPayout += nDevReward;
    }
    if (nOMNOReward > 0) {
        const CBetOut betOutOMNO(nOMNOReward, payoutScriptOMNO, profitAcc);
        const CPayoutInfo payoutInfoOMNO(zeroKey, PayoutType::bettingReward);
        mExpectedPayouts.insert(std::pair<const CPayoutInfo, CBetOut>(payoutInfoOMNO, betOutOMNO));

        nPayout += nOMNOReward;
    }

    return  nPayout;
}

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all CGLotto bets.
 *
 * @param vexpectedCGPayouts  A vector containing all the winning bets that need to be paid out.
 * @return
 */
int64_t GetCGBlockPayoutsValue(const std::multimap<CPayoutInfo, CBetOut>& mExpectedCGPayouts)
{
    CAmount nPayout = 0;

    for (auto payout : mExpectedCGPayouts)
    {
        nPayout += payout.second.nValue;
    }

    return  nPayout;
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

uint32_t CalculateAsianSpreadOdds(const CPeerlessEvent &lockedEvent, const int32_t difference, bool spreadHomeOutcome)
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
uint32_t GetBetOdds(const CPeerlessBet &bet, const CPeerlessEvent &lockedEvent, const CPeerlessResult &result, const bool fWagerrProtocolV3)
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

/**
 * Creates the bet payout vector for all winning CUniversalBet bets.
 *
 * @return payout vector, payouts info vector.
 */
void GetBetPayouts(CBettingsView &bettingsViewCache, int height, std::multimap<CPayoutInfo, CBetOut>& mExpectedPayouts, const bool fWagerrProtocolV3, bool fUpdate)
{
    int nCurrentHeight = chainActive.Height();
    uint64_t refundOdds{BET_ODDSDIVISOR};
    // Get all the results posted in the latest block.
    std::vector<CPeerlessResult> results = getEventResults(height);

    LogPrintf("Start generating payouts...\n");

    mExpectedPayouts.clear();

    for (auto result : results) {

        // look bets at last 14 days
        uint32_t startHeight = nCurrentHeight >= Params().BetBlocksIndexTimespan() ? nCurrentHeight - Params().BetBlocksIndexTimespan() : 0;

        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<UniversalBetKey, CUniversalBet>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(UniversalBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {
            UniversalBetKey uniBetKey;
            CUniversalBet uniBet;
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
                        CPeerlessBet &leg = uniBet.legs[idx];
                        CPeerlessEvent &lockedEvent = uniBet.lockedEvents[idx];
                        // skip this bet if incompleted (can't find one result)
                        CPeerlessResult res;
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
                CPeerlessBet &singleBet = uniBet.legs[0];
                CPeerlessEvent &lockedEvent = uniBet.lockedEvents[0];

                if (singleBet.nEventId == result.nEventId) {
                    completedBet = true;

                    // if bet placed before 2 mins of event started - refund this bet
                    if (lockedEvent.nStartTime > 0 && uniBet.betTime > (lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                        if (fWagerrProtocolV3) {
                            odds = refundOdds;
                        } else {
                            odds = 0;
                        }
                    } else if ((!fWagerrProtocolV3) && height - lockedEvent.nEventCreationHeight > Params().BetBlocksIndexTimespan()) {
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
                    const CPayoutInfo payoutInfo(uniBetKey, odds <= refundOdds ? PayoutType::bettingRefund : PayoutType::bettingPayout);
                    const CBetOut betOut(payout, GetScriptForDestination(uniBet.playerAddress.Get()), uniBet.betAmount);
                    mExpectedPayouts.insert(std::pair<const CPayoutInfo, CBetOut>(payoutInfo, betOut));

                    uniBet.resultType = odds <= refundOdds ? BetResultType::betResultRefund : BetResultType::betResultWin;
                }
                else {
                    uniBet.resultType = BetResultType::betResultLose;
                }
                uniBet.payout = payout;
                LogPrintf("\nBet %s is handled!\nPlayer address: %s\nPayout: %ll\n\n", uniBet.betOutPoint.ToStringShort(), uniBet.playerAddress.ToString(), payout);
                // if handling bet is completed - mark it
                uniBet.SetCompleted();
                vEntriesToUpdate.emplace_back(std::pair<UniversalBetKey, CUniversalBet>{uniBetKey, uniBet});
            }
        }
        if (fUpdate){
            for (auto pair : vEntriesToUpdate) {
                bettingsViewCache.bets->Update(pair.first, pair.second);
            }
        }
    }
    LogPrintf("Finished generating payouts...\n");

}

/**
 * Creates the bet payout std::vector for all winning CGLotto events.
 *
 * @return payout vector.
 */
void GetCGLottoBetPayouts(int height, std::multimap<CPayoutInfo, CBetOut>& mExpectedPayouts)
{
    int nCurrentHeight = chainActive.Height();
    long long totalValueOfBlock = 0;

    std::pair<std::vector<CChainGamesResult>,std::vector<std::string>> resultArray = getCGLottoEventResults(height);
    std::vector<CChainGamesResult> allChainGames = resultArray.first;
    std::vector<std::string> blockSizeArray = resultArray.second;

    // Find payout for each CGLotto game
    for (unsigned int currResult = 0; currResult < resultArray.second.size(); currResult++) {

        CChainGamesResult currentChainGame = allChainGames[currResult];
        uint32_t currentEventID = currentChainGame.nEventId;
        CAmount eventFee = 0;

        totalValueOfBlock = stoll(blockSizeArray[0]);

        //reset total bet amount and candidate array for this event
        std::vector<std::pair<std::string, UniversalBetKey>> candidates;
        CAmount totalBetAmount = 0 * COIN;

        // Look back the chain 10 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        BlocksIndex = chainActive[nCurrentHeight - 14400];

        time_t eventStart = 0;
        bool eventStartedFlag = false;
        bool currentEventFound = false;

        while (BlocksIndex) {

            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            time_t transactionTime = block.nTime;

            for (CTransaction &tx : block.vtx) {

                // Ensure if event TX that has it been posted by Oracle wallet.
                const CTxIn &txin = tx.vin[0];
                COutPoint prevout = txin.prevout;

                uint256 hashBlock;
                CTransaction txPrev;

                uint256 txHash = tx.GetHash();

                bool validTX = IsValidOracleTx(txin);

                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = txout.scriptPubKey.ToString();
                    CAmount betAmount = txout.nValue;

                    UniversalBetKey betKey{static_cast<uint32_t>(BlocksIndex->nHeight), COutPoint{txHash, static_cast<uint32_t>(i)}};

                    if (scriptPubKey.length() > 0 && 0 == strncmp(scriptPubKey.c_str(), "OP_RETURN", 9)) {
                        // Get the OP CODE from the transaction scriptPubKey.
                        std::vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, std::string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        // If bet was placed less than 20 mins before event start or after event start discard it.
                        if (eventStart > 0 && transactionTime > (eventStart - Params().BetPlaceTimeoutBlocks())) {
                            eventStartedFlag = true;
                            break;
                        }

                        // Find most recent CGLotto event
                        CChainGamesEvent chainGameEvt;
                        if (validTX && CChainGamesEvent::FromOpCode(opCode, chainGameEvt)){
                            if (chainGameEvt.nEventId == currentEventID) {
                                eventFee = chainGameEvt.nEntryFee * COIN;
                                currentEventFound = true;
                            }
                        }

                        // Find most recent CGLotto bet once the event has been found
                        CChainGamesBet chainGamesBet;
                        if (currentEventFound && CChainGamesBet::FromOpCode(opCode, chainGamesBet)) {

                            uint32_t eventId = chainGamesBet.nEventId;

                            // If current event ID matches result ID add bettor to candidate array
                            if (eventId == currentEventID) {

                                CTxDestination address;
                                ExtractDestination(tx.vout[0].scriptPubKey, address);

                                //Check Entry fee matches the bet amount
                                if (eventFee == betAmount) {

                                    totalBetAmount = totalBetAmount + betAmount;
                                    CTxDestination payoutAddress;

                                    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
                                        ExtractDestination( txPrev.vout[prevout.n].scriptPubKey, payoutAddress );
                                    }

                                    // Add the payout address of each candidate to array
                                    candidates.push_back(std::pair<std::string, UniversalBetKey>{CBitcoinAddress( payoutAddress ).ToString().c_str(), betKey});
                                }
                            }
                        }
                    }
                }

                if (eventStartedFlag) {
                    break;
                }
            }

            if (eventStartedFlag) {
                break;
            }

            BlocksIndex = chainActive.Next(BlocksIndex);
        }

        // Choose winner from candidates who entered the lotto and payout their winnings.
        if (candidates.size() == 1) {
             // Refund the single entrant.
             CAmount noOfBets = candidates.size();
             std::string winnerAddress = candidates[0].first;
             CAmount entranceFee = eventFee;
             CAmount winnerPayout = eventFee;

	         LogPrintf("\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
	         LogPrintf("Total number of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
	         LogPrintf("Winner Address: %u \n", winnerAddress);
	         LogPrintf(" This Lotto was refunded as only one person bought a ticket.\n" );

             // Only add valid payouts to the vector.
             if (winnerPayout > 0) {
                const CPayoutInfo payoutInfo(candidates[0].second, PayoutType::chainGamesRefund);
                const CBetOut betOut(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee, allChainGames[currResult].nEventId);
                mExpectedPayouts.insert(std::pair<const CPayoutInfo, CBetOut>(payoutInfo, betOut));
             }
        }
        else if (candidates.size() >= 2) {
            // Use random number to choose winner.
            auto noOfBets    = candidates.size();

            uint256 hashProofOfStake;

            std::unique_ptr <CStakeInput> stake;
            CBlock block;
            ReadBlockFromDisk(block, chainActive.Tip());
            if (initStakeInput(block, stake, chainActive.Tip()->nHeight - 1)) {
                unsigned int nTxTime = block.nTime;
                if (!GetHashProofOfStake(chainActive.Tip()->pprev, stake.get(), nTxTime, false, hashProofOfStake)) {
                    LogPrintf("%s: Failed to create entropy from at height=%d", __func__, chainActive.Tip()->nHeight);
                }
            }

            if (hashProofOfStake == 0) hashProofOfStake = chainActive[height]->GetBlockHash();

            uint256 tempVal = hashProofOfStake / noOfBets;  // quotient
            tempVal = tempVal * noOfBets;
            tempVal = hashProofOfStake - tempVal;           // remainder
            uint64_t winnerNr = tempVal.Get64();

            // Split the pot and calculate winnings.
            std::string winnerAddress = candidates[winnerNr].first;
            CAmount entranceFee = eventFee;
            CAmount totalPot = hashProofOfStake == 0 ? 0 : (noOfBets*entranceFee);
            CAmount winnerPayout = totalPot / 10 * 8;
            CAmount fee = totalPot / 50;

            LogPrintf("\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
            LogPrintf("Total number Of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrintf("Winner Address: %u (index no %u) \n", winnerAddress, winnerNr);
            LogPrintf("Total Value of Block: %u \n", totalValueOfBlock);
            LogPrintf("Total Pot: %u, Winnings: %u, Fee: %u \n", totalPot, winnerPayout, fee);

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                const CPayoutInfo payoutInfoWinner(candidates[winnerNr].second, PayoutType::chainGamesPayout);
                const CBetOut betOutWinner(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee, allChainGames[currResult].nEventId);
                mExpectedPayouts.insert(std::pair<const CPayoutInfo, CBetOut>(payoutInfoWinner, betOutWinner));
            }
            if (fee > 0) {
                UniversalBetKey zeroKey{static_cast<uint32_t>(nCurrentHeight), COutPoint()};
                const CPayoutInfo payoutInfoFee(zeroKey, PayoutType::chainGamesPayout);
                const CBetOut betOutFee(fee, GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get()), entranceFee);
                mExpectedPayouts.insert(std::pair<const CPayoutInfo, CBetOut>(payoutInfoFee, betOutFee));
            }
        }
    }
}

/**
 * Undo only quick games bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoQuickGamesBetPayouts(CBettingsView &bettingsViewCache, int height)
{
    uint32_t blockHeight = static_cast<uint32_t>(height);

    LogPrintf("Start undo quick games payouts...\n");

    auto it = bettingsViewCache.quickGamesBets->NewIterator();
    std::vector<std::pair<QuickGamesBetKey, CQuickGamesBet>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(QuickGamesBetKey{blockHeight, COutPoint()})); it->Valid(); it->Next()) {
        QuickGamesBetKey qgBetKey;
        CQuickGamesBet qgBet;
        CBettingDB::BytesToDbType(it->Key(), qgBetKey);
        CBettingDB::BytesToDbType(it->Value(), qgBet);
        // skip if bet is uncompleted
        if (!qgBet.IsCompleted()) continue;

        qgBet.SetUncompleted();
        qgBet.resultType = BetResultType::betResultUnknown;
        qgBet.payout = 0;
        vEntriesToUpdate.emplace_back(std::pair<QuickGamesBetKey, CQuickGamesBet>{qgBetKey, qgBet});
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.quickGamesBets->Update(pair.first, pair.second);
    }
    return true;
}

/**
 * Creates the bet payout vector for all winning Quick Games bets.
 *
 * @return payout vector.
 */
uint32_t GetQuickGamesBetPayouts(CBettingsView& bettingsViewCache, const int height,  std::multimap<CPayoutInfo, CBetOut>& mExpectedQGPayouts)
{
    UniversalBetKey zeroKey{0, COutPoint()};
    uint32_t expectedMint = 0;

    LogPrintf("Start generating quick games bets payouts...\n");

    CBlockIndex *blockIndex = chainActive[height];
    uint32_t blockHeight = static_cast<uint32_t>(height);
    auto it = bettingsViewCache.quickGamesBets->NewIterator();
    std::vector<std::pair<QuickGamesBetKey, CQuickGamesBet>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(QuickGamesBetKey{blockHeight, COutPoint()})); it->Valid(); it->Next()) {
        QuickGamesBetKey qgKey;
        CQuickGamesBet qgBet;

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
            CPayoutInfo payoutInfo(qgKey, odds == BET_ODDSDIVISOR ? PayoutType::quickGamesRefund : PayoutType::quickGamesPayout);
            CBetOut betOut(payout, GetScriptForDestination(qgBet.playerAddress.Get()), qgBet.betAmount);
            mExpectedQGPayouts.insert(std::pair<CPayoutInfo, CBetOut>(payoutInfo, betOut));
            expectedMint += payout;
           // Dev reward
            CAmount devReward = (CAmount)(feePermille / 1000 * gameView.nDevRewardPermille / BET_ODDSDIVISOR);
            expectedMint += devReward;
            CPayoutInfo devRewardInfo(zeroKey, PayoutType::quickGamesReward);
            CBetOut devRewardOut(devReward, GetScriptForDestination(CBitcoinAddress(gameView.specialAddress).Get()), qgBet.betAmount);
            mExpectedQGPayouts.insert(std::pair<CPayoutInfo, CBetOut>(devRewardInfo, devRewardOut));
            // OMNO reward
            std::string OMNOPayoutAddr = Params().OMNOPayoutAddr();
            CAmount nOMNOReward = (CAmount)(feePermille / 1000 * gameView.nOMNORewardPermille / BET_ODDSDIVISOR);
            expectedMint += nOMNOReward;
            CPayoutInfo omnoRewardInfo(zeroKey, PayoutType::quickGamesReward);
            CBetOut omnoRewardOut(nOMNOReward, GetScriptForDestination(CBitcoinAddress(OMNOPayoutAddr).Get()), qgBet.betAmount);
            mExpectedQGPayouts.insert(std::pair<CPayoutInfo, CBetOut>(omnoRewardInfo, omnoRewardOut));
        }
        else {
            qgBet.resultType = BetResultType::betResultLose;
        }
        LogPrintf("\nQuick game: %s, bet %s is handled!\nPlayer address: %s\nPayout: %ll\n\n", gameView.name, qgKey.outPoint.ToStringShort(), qgBet.playerAddress.ToString(), payout);
        // if handling bet is completed - mark it
        qgBet.SetCompleted();
        qgBet.payout = payout;
        vEntriesToUpdate.emplace_back(std::pair<QuickGamesBetKey, CQuickGamesBet>{qgKey, qgBet});
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.quickGamesBets->Update(pair.first, pair.second);
    }
    return expectedMint;
}