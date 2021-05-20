// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util.h>
#include <betting/bet_tx.h>
#include <betting/bet_db.h>
#include <betting/bet_common.h>
#include <amount.h>
#include <main.h>

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all bets.
 * We also calculate and add the OMNO and dev fund rewards.
 *
 * @param vExpectedPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
void GetPLRewardPayoutsV2(const uint32_t nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    CAmount profitAcc = 0;
    CAmount totalAmountBet = 0;
    PeerlessBetKey zeroKey{nNewBlockHeight, COutPoint()};

    // Set the OMNO and Dev reward addresses
    CScript payoutScriptDev;
    CScript payoutScriptOMNO;
    if (!GetFeePayoutScripts(nNewBlockHeight, payoutScriptDev, payoutScriptOMNO)) {
        LogPrintf("Unable to find oracle, skipping payouts\n");
        return;
    }

    // Loop over the payout vector and aggregate values.
    for (size_t i = 0; i < vExpectedPayouts.size(); i++)
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

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all CGLotto bets.
 *
 * @param vexpectedCGPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
int64_t GetCGBlockPayoutsV2(std::vector<CBetOut>& vexpectedCGPayouts, CAmount& nMNBetReward)
{
    CAmount nPayout = 0;

    for (unsigned i = 0; i < vexpectedCGPayouts.size(); i++) {
        CAmount payValue = vexpectedCGPayouts[i].nValue;
        nPayout += payValue;
    }

    return  nPayout;
}

// TODO function will need to be refactored and cleaned up at a later stage as we have had to make rapid and frequent code changes.
/**
 * Creates the bet payout vector for all winning CPeerless bets.
 *
 * @return payout vector.
 */
void GetBetPayoutsV2(const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    int nLastBlockHeight = chainActive.Height();

    // Get all the results posted in the latest block.
    std::vector<CPeerlessResultDB> results = GetPLResults(nNewBlockHeight - 1);

    // Traverse the blockchain for an event to match a result and all the bets on a result.
    for (const auto& result : results) {
        // Look back the chain 14 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        int startHeight = nLastBlockHeight - Params().BetBlocksIndexTimespanV2();
        startHeight = startHeight < Params().WagerrProtocolV2StartHeight() ? Params().WagerrProtocolV2StartHeight() : startHeight;
        BlocksIndex = chainActive[startHeight];

        OutcomeType nMoneylineResult = (OutcomeType) 0;
        std::vector<OutcomeType> vSpreadsResult;
        std::vector<OutcomeType> vTotalsResult;

        uint64_t nMoneylineOdds     = 0;
        uint64_t nSpreadsOdds       = 0;
        uint64_t nTotalsOdds        = 0;
        uint64_t nTotalsPoints      = result.nHomeScore + result.nAwayScore;
        int64_t nSpreadsDifference = 0;
        bool HomeFavorite               = false;

        // We keep temp values as we can't be sure of the order of the TX's being stored in a block.
        // This can lead to a case were some bets don't
        uint64_t nTempMoneylineOdds = 0;
        uint64_t nTempSpreadsOdds   = 0;
        uint64_t nTempTotalsOdds    = 0;

        bool UpdateMoneyLine = false;
        bool UpdateSpreads   = false;
        bool UpdateTotals    = false;
        uint64_t nSpreadsWinner = 0;
        uint64_t nTotalsWinner  = 0;

        time_t tempEventStartTime   = 0;
        time_t latestEventStartTime = 0;
        bool eventFound = false;
        bool spreadsFound = false;
        bool totalsFound = false;

        // Find MoneyLine outcome (result).
        if (result.nHomeScore > result.nAwayScore) {
            nMoneylineResult = moneyLineHomeWin;
        }
        else if (result.nHomeScore < result.nAwayScore) {
            nMoneylineResult = moneyLineAwayWin;
        }
        else if (result.nHomeScore == result.nAwayScore) {
            nMoneylineResult = moneyLineDraw;
        }

        // Traverse the block chain to find events and bets.
        while (BlocksIndex) {
            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            time_t transactionTime = block.nTime;
            uint32_t nHeight = BlocksIndex->nHeight;

            for (CTransaction &tx : block.vtx) {
                // Ensure TX has it been posted by Oracle wallet.
                const CTxIn &txin = tx.vin[0];
                bool validOracleTx = IsValidOracleTx(txin, nHeight);
                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    CAmount betAmount = txout.nValue;

                    auto bettingTx = ParseBettingTx(txout);

                    if (bettingTx == nullptr) continue;

                    auto txType = bettingTx->GetTxType();
                    // Peerless event OP RETURN transaction.
                    if (validOracleTx && txType == plEventTxType) {
                        CPeerlessEventTx* pe = (CPeerlessEventTx*) bettingTx.get();

                        // If the current event matches the result we can now set the odds.
                        if (result.nEventId == pe->nEventId) {

                            UpdateMoneyLine    = true;
                            eventFound         = true;
                            tempEventStartTime = pe->nStartTime;

                            // Set the temp moneyline odds.
                            if (nMoneylineResult == moneyLineHomeWin) {
                                nTempMoneylineOdds = pe->nHomeOdds;
                            }
                            else if (nMoneylineResult == moneyLineAwayWin) {
                                nTempMoneylineOdds = pe->nAwayOdds;
                            }
                            else if (nMoneylineResult == moneyLineDraw) {
                                nTempMoneylineOdds = pe->nDrawOdds;
                            }

                            // Set which team is the favorite, used for calculating spreads difference & winner.
                            if (pe->nHomeOdds < pe->nAwayOdds) {
                                HomeFavorite = true;
                                if (result.nHomeScore > result.nAwayScore) {
                                    nSpreadsDifference = result.nHomeScore - result.nAwayScore;
                                }
                                else{
                                    nSpreadsDifference = 0;
                                }
                            }
                            else {
                                HomeFavorite = false;
                                if (result.nAwayScore > result.nHomeScore) {
                                    nSpreadsDifference = result.nAwayScore - result.nHomeScore;
                                }

                                else{
                                    nSpreadsDifference = 0;
                                }
                            }
                        }
                    }

                    // Peerless update odds OP RETURN transaction.
                    if (eventFound && validOracleTx && txType == plUpdateOddsTxType) {

                        CPeerlessUpdateOddsTx* puo = (CPeerlessUpdateOddsTx*) bettingTx.get();

                        if (result.nEventId == puo->nEventId ) {

                            UpdateMoneyLine = true;

                            // If current event ID matches result ID set the odds.
                            if (nMoneylineResult == moneyLineHomeWin) {
                                nTempMoneylineOdds = puo->nHomeOdds;
                            }
                            else if (nMoneylineResult == moneyLineAwayWin) {
                                nTempMoneylineOdds = puo->nAwayOdds;
                            }
                            else if (nMoneylineResult == moneyLineDraw) {
                                nTempMoneylineOdds = puo->nDrawOdds;
                            }
                        }
                    }

                    // Handle PSE, when we find a Spreads event on chain we need to update the Spreads odds.
                    if (eventFound && validOracleTx && txType == plSpreadsEventTxType) {

                        CPeerlessSpreadsEventTx* pse = (CPeerlessSpreadsEventTx*) bettingTx.get();

                        if (result.nEventId == pse->nEventId) {

                            UpdateSpreads = true;

                            // If the home team is the favourite.
                            if (HomeFavorite){
                                //  Choose the spreads winner.
                                if (nSpreadsDifference == 0) {
                                    nSpreadsWinner = WinnerType::awayWin;
                                }
                                else if (pse->nPoints < nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::homeWin;
                                }
                                else if (pse->nPoints > nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::awayWin;
                                }
                                else {
                                    nSpreadsWinner = WinnerType::push;
                                }
                            }
                            // If the away team is the favourite.
                            else {
                                // Cho0se the winner.
                                if (nSpreadsDifference == 0) {
                                    nSpreadsWinner = WinnerType::homeWin;
                                }
                                else if (pse->nPoints > nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::homeWin;
                                }
                                else if (pse->nPoints < nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::awayWin;
                                }
                                else {
                                    nSpreadsWinner = WinnerType::push;
                                }
                            }

                            // Set the temp spread odds.
                            if (nSpreadsWinner == WinnerType::push) {
                                nTempSpreadsOdds = BET_ODDSDIVISOR;
                            }
                            else if (nSpreadsWinner == WinnerType::awayWin) {
                                nTempSpreadsOdds = pse->nAwayOdds;
                            }
                            else if (nSpreadsWinner == WinnerType::homeWin) {
                                nTempSpreadsOdds = pse->nHomeOdds;
                            }
                        }
                    }

                    // Handle PTE, when we find an Totals event on chain we need to update the Totals odds.
                    if (eventFound && validOracleTx && txType == plTotalsEventTxType) {
                        CPeerlessTotalsEventTx* pte = (CPeerlessTotalsEventTx*) bettingTx.get();
                        if (result.nEventId == pte->nEventId) {

                            UpdateTotals = true;

                            // Find totals outcome (result).
                            if (pte->nPoints == nTotalsPoints) {
                                nTotalsWinner = WinnerType::push;
                            }
                            else if (pte->nPoints > nTotalsPoints) {
                                nTotalsWinner = WinnerType::awayWin;
                            }
                            else {
                                nTotalsWinner = WinnerType::homeWin;
                            }

                            // Set the totals temp odds.
                            if (nTotalsWinner == WinnerType::push) {
                                nTempTotalsOdds = BET_ODDSDIVISOR;
                            }
                            else if (nTotalsWinner == WinnerType::awayWin) {
                                nTempTotalsOdds = pte->nUnderOdds;
                            }
                            else if (nTotalsWinner == WinnerType::homeWin) {
                                nTempTotalsOdds = pte->nOverOdds;
                            }
                        }
                    }

                    // If we encounter the result after cycling the chain then we dont need go any furture so finish the payout.
                    if (eventFound && validOracleTx && txType == plResultTxType) {
                        CPeerlessResultTx* pr = (CPeerlessResultTx*) bettingTx.get();
                        if (result.nEventId == pr->nEventId ) {
                            return;
                        }
                    }

                    // Only payout bets that are between 25 - 10000 WRG inclusive (MaxBetPayoutRange).
                    if (eventFound && betAmount >= (Params().MinBetPayoutRange() * COIN) && betAmount <= (Params().MaxBetPayoutRange() * COIN)) {

                        // Bet OP RETURN transaction.
                        if (txType == plBetTxType) {

                            CPeerlessBetTx* pb = (CPeerlessBetTx*) bettingTx.get();

                            PeerlessBetKey betKey{nHeight, COutPoint(tx.GetHash(), i)};

                            CAmount payout = 0 * COIN;

                            // If bet was placed less than 20 mins before event start or after event start discard it.
                            if (latestEventStartTime > 0 && (unsigned int) transactionTime > (latestEventStartTime - Params().BetPlaceTimeoutBlocks())) {
                                continue;
                            }

                            // Is the bet a winning bet?
                            if (result.nEventId == pb->nEventId) {
                                CAmount winnings = 0;

                                // If bet payout result.
                                if (result.nResultType ==  ResultType::standardResult) {

                                    // Calculate winnings.
                                    if ((OutcomeType) pb->nOutcome == nMoneylineResult) {
                                        winnings = betAmount * nMoneylineOdds;
                                    }
                                    else if (spreadsFound && ((OutcomeType) pb->nOutcome == vSpreadsResult.at(0) || (OutcomeType) pb->nOutcome == vSpreadsResult.at(1))) {
                                        winnings = betAmount * nSpreadsOdds;
                                    }
                                    else if (totalsFound && ((OutcomeType) pb->nOutcome == vTotalsResult.at(0) || (OutcomeType) pb->nOutcome == vTotalsResult.at(1))) {
                                        winnings = betAmount * nTotalsOdds;
                                    }

                                    // Calculate the bet winnings for the current bet.
                                    if (winnings > 0) {
                                        payout = (winnings - ((winnings - betAmount*BET_ODDSDIVISOR) / 1000 * BET_BURNXPERMILLE)) / BET_ODDSDIVISOR;
                                    }
                                    else {
                                        payout = 0;
                                    }
                                }
                                // Bet refund result.
                                else if (result.nResultType ==  ResultType::eventRefund){
                                    payout = betAmount;
                                } else if (result.nResultType == ResultType::mlRefund){
                                    // Calculate winnings.
                                    if ( (OutcomeType)pb->nOutcome == OutcomeType::moneyLineDraw ||
                                            (OutcomeType) pb->nOutcome == OutcomeType::moneyLineAwayWin ||
                                            (OutcomeType) pb->nOutcome == OutcomeType::moneyLineHomeWin) {
                                        payout = betAmount;
                                    }
                                    else if (spreadsFound && ((OutcomeType) pb->nOutcome == vSpreadsResult.at(0) || (OutcomeType) pb->nOutcome == vSpreadsResult.at(1))) {
                                        winnings = betAmount * nSpreadsOdds;

                                        // Calculate the bet winnings for the current bet.
                                        if (winnings > 0) {
                                            payout = (winnings - ((winnings - betAmount*BET_ODDSDIVISOR) / 1000 * BET_BURNXPERMILLE)) / BET_ODDSDIVISOR;
                                        }
                                        else {
                                            payout = 0;
                                        }
                                    }
                                    else if (totalsFound && ((OutcomeType) pb->nOutcome == vTotalsResult.at(0) || (OutcomeType) pb->nOutcome == vTotalsResult.at(1))) {
                                        winnings = betAmount * nTotalsOdds;

                                        // Calculate the bet winnings for the current bet.
                                        if (winnings > 0) {
                                            payout = (winnings - ((winnings - betAmount*BET_ODDSDIVISOR) / 1000 * BET_BURNXPERMILLE)) / BET_ODDSDIVISOR;
                                        }
                                        else {
                                            payout = 0;
                                        }
                                    }
                                }

                                // Get the users payout address from the vin of the bet TX they used to place the bet.
                                CTxDestination payoutAddress;
                                const CTxIn &txin = tx.vin[0];
                                COutPoint prevout = txin.prevout;

                                uint256 hashBlock;
                                CTransaction txPrev;
                                if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
                                    ExtractDestination( txPrev.vout[prevout.n].scriptPubKey, payoutAddress );
                                }

                                LogPrint("wagerr", "MoneyLine Refund - PAYOUT\n");
                                LogPrint("wagerr", "AMOUNT: %li \n", payout);
                                LogPrint("wagerr", "ADDRESS: %s \n", CBitcoinAddress( payoutAddress ).ToString().c_str());

                                // Only add valid payouts to the vector.
                                if (payout > 0) {
                                    // Add winning bet payout to the bet vector.
                                    bool refund = (payout == betAmount * BET_ODDSDIVISOR) ? true : false;
                                    vPayoutsInfo.emplace_back(betKey, refund ? PayoutType::bettingRefund : PayoutType::bettingPayout);
                                    vExpectedPayouts.emplace_back(payout, GetScriptForDestination(CBitcoinAddress(payoutAddress).Get()), betAmount);
                                }
                            }
                        }
                    }
                }
            }

            // If an update transaction came in on this block, the bool would be set to true and the odds/winners will be updated (below) for the next block
            if (UpdateMoneyLine){
                UpdateMoneyLine      = false;
                nMoneylineOdds       = nTempMoneylineOdds;
                latestEventStartTime = tempEventStartTime;
            }

            // If we need to update the spreads odds using temp values.
            if (UpdateSpreads) {
                UpdateSpreads = false;
                spreadsFound = true;

                //set the payout odds (using the temp odds)
                nSpreadsOdds = nTempSpreadsOdds;
                //clear the winner vector (used to determine which bets to payout).
                vSpreadsResult.clear();

                //Depending on the calculations above we populate the winner vector (push/away/home)
                if (nSpreadsWinner == WinnerType::homeWin) {
                    vSpreadsResult.emplace_back(spreadHome);
                    vSpreadsResult.emplace_back(spreadHome);
                }
                else if (nSpreadsWinner == WinnerType::awayWin) {
                    vSpreadsResult.emplace_back(spreadAway);
                    vSpreadsResult.emplace_back(spreadAway);
                }
                else if (nSpreadsWinner == WinnerType::push) {
                    vSpreadsResult.emplace_back(spreadHome);
                    vSpreadsResult.emplace_back(spreadAway);
                }

                nSpreadsWinner = 0;
            }

            // If we need to update the totals odds using the temp values.
            if (UpdateTotals) {
                UpdateTotals = false;
                totalsFound = true;

                nTotalsOdds  = nTempTotalsOdds;
                vTotalsResult.clear();

                if (nTotalsWinner == WinnerType::homeWin) {
                    vTotalsResult.emplace_back(totalOver);
                    vTotalsResult.emplace_back(totalOver);
                }
                else if (nTotalsWinner == WinnerType::awayWin) {
                    vTotalsResult.emplace_back(totalUnder);
                    vTotalsResult.emplace_back(totalUnder);
                }
                else if (nTotalsWinner == WinnerType::push) {
                    vTotalsResult.emplace_back(totalOver);
                    vTotalsResult.emplace_back(totalUnder);
                }

                nTotalsWinner = 0;
            }

            BlocksIndex = chainActive.Next(BlocksIndex);
        }
    }

    GetPLRewardPayoutsV2(nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);

    return;
}

/**
 * Creates the bet payout std::vector for all winning CGLotto events.
 *
 * @return payout vector.
 */
void GetCGLottoBetPayoutsV2(const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    // get results from prev block
    std::vector<CChainGamesResultDB> allChainGames;
    GetCGLottoEventResults(nLastBlockHeight, allChainGames);

    // Find payout for each CGLotto game
    for (unsigned int currResult = 0; currResult < allChainGames.size(); currResult++) {

        CChainGamesResultDB currentChainGame = allChainGames[currResult];
        uint32_t currentEventID = currentChainGame.nEventId;
        CAmount eventFee = 0;

        //reset total bet amount and candidate array for this event
        std::vector<std::pair<std::string, PeerlessBetKey>> candidates;
        CAmount totalBetAmount = 0 * COIN;

        // Look back the chain 10 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        BlocksIndex = chainActive[nLastBlockHeight - 14400];

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

                bool validTX = IsValidOracleTx(txin, BlocksIndex->nHeight);

                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    CAmount betAmount = txout.nValue;

                    PeerlessBetKey betKey{static_cast<uint32_t>(BlocksIndex->nHeight), COutPoint{txHash, static_cast<uint32_t>(i)}};
                    auto cgBettingTx = ParseBettingTx(txout);

                    if (cgBettingTx == nullptr) continue;

                    // If bet was placed less than 20 mins before event start or after event start discard it.
                    if (eventStart > 0 && transactionTime > (eventStart - Params().BetPlaceTimeoutBlocks())) {
                        eventStartedFlag = true;
                        break;
                    }

                    auto txType = cgBettingTx->GetTxType();

                    // Find most recent CGLotto event
                    if (validTX && txType == cgEventTxType) {

                        CChainGamesEventTx* chainGameEvt = (CChainGamesEventTx*) cgBettingTx.get();
                        if (chainGameEvt->nEventId == currentEventID) {
                            eventFee = chainGameEvt->nEntryFee * COIN;
                            currentEventFound = true;
                        }
                    }

                    // Find most recent CGLotto bet once the event has been found
                    if (currentEventFound && txType == cgBetTxType) {

                        CChainGamesBetTx* chainGamesBet = (CChainGamesBetTx*) cgBettingTx.get();

                        uint32_t eventId = chainGamesBet->nEventId;

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
                                candidates.push_back(std::pair<std::string, PeerlessBetKey>{CBitcoinAddress( payoutAddress ).ToString().c_str(), betKey});
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

             LogPrint("wagerr", "\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
             LogPrint("wagerr", "Total number of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
             LogPrint("wagerr", "Winner Address: %u \n", winnerAddress);
             LogPrint("wagerr", " This Lotto was refunded as only one person bought a ticket.\n" );

             // Only add valid payouts to the vector.
             if (winnerPayout > 0) {
                 vPayoutsInfo.emplace_back(candidates[0].second, PayoutType::chainGamesRefund);
                 vExpectedPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee, allChainGames[currResult].nEventId);
             }
        }
        else if (candidates.size() >= 2) {
            // Use random number to choose winner.
            auto noOfBets    = candidates.size();

            CBlockIndex *winBlockIndex = chainActive[nLastBlockHeight];
            uint256 hashProofOfStake = winBlockIndex->hashProofOfStake;
            if (hashProofOfStake == 0) hashProofOfStake = winBlockIndex->GetBlockHash();
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

            LogPrint("wagerr", "\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
            LogPrint("wagerr", "Total number Of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrint("wagerr", "Winner Address: %u (index no %u) \n", winnerAddress, winnerNr);
            LogPrint("wagerr", "Total Pot: %u, Winnings: %u, Fee: %u \n", totalPot, winnerPayout, fee);

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                CScript payoutScriptDev;
                CScript payoutScriptOMNO;
                if (!GetFeePayoutScripts(nNewBlockHeight, payoutScriptDev, payoutScriptOMNO)) {
                    LogPrintf("Unable to find oracle, skipping payouts\n");
                    continue;
                }
                PeerlessBetKey zeroKey{0, COutPoint()};
                vPayoutsInfo.emplace_back(candidates[winnerNr].second, PayoutType::chainGamesPayout);
                vExpectedPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee, allChainGames[currResult].nEventId);
                vPayoutsInfo.emplace_back(zeroKey, PayoutType::chainGamesPayout);
                vExpectedPayouts.emplace_back(fee, payoutScriptOMNO, entranceFee);
            }
        }
    }
}
