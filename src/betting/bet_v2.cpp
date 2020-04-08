// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet_v2.h"

#include "bet.h"
#include "main.h"

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all bets.
 * We also calculate and add the OMNO and dev fund rewards.
 *
 * @param vExpectedPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
int64_t GetBlockPayoutsV2(std::vector<CBetOut>& vExpectedPayouts, CAmount& nMNBetReward, std::vector<CPayoutInfo>& vPayoutsInfo)
{
    CAmount profitAcc = 0;
    CAmount nPayout = 0;
    CAmount totalAmountBet = 0;
    UniversalBetKey zeroKey{0, COutPoint()};

    // Loop over the payout vector and aggregate values.
    for (unsigned i = 0; i < vExpectedPayouts.size(); i++) {
        CAmount betValue = vExpectedPayouts[i].nBetValue;
        CAmount payValue = vExpectedPayouts[i].nValue;

        totalAmountBet += betValue;
        profitAcc += payValue - betValue;
        nPayout += payValue;
    }

    if (vExpectedPayouts.size() > 0) {
        // Calculate the OMNO reward and the Dev reward.
        CAmount nOMNOReward = (CAmount)(profitAcc * Params().OMNORewardPermille() / (1000.0 - BET_BURNXPERMILLE));
        CAmount nDevReward  = (CAmount)(profitAcc * Params().DevRewardPermille() / (1000.0 - BET_BURNXPERMILLE));

        // Add both reward payouts to the payout vector.
        vExpectedPayouts.emplace_back(nDevReward, GetScriptForDestination(CBitcoinAddress(Params().DevPayoutAddr()).Get()));
        vPayoutsInfo.emplace_back(zeroKey, PayoutType::bettingReward);
        vExpectedPayouts.emplace_back(nOMNOReward, GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get()));
        vPayoutsInfo.emplace_back(zeroKey, PayoutType::bettingReward);
        nPayout += nDevReward + nOMNOReward;
    }

    return  nPayout;
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
void GetBetPayoutsV2(int height, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfo>& vPayoutsInfo)
{
    int nCurrentHeight = chainActive.Height();

    // Get all the results posted in the latest block.
    std::vector<CPeerlessResult> results = getEventResults(height);

    // Traverse the blockchain for an event to match a result and all the bets on a result.
    for (const auto& result : results) {
        // Look back the chain 14 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        int startHeight = nCurrentHeight - Params().BetBlocksIndexTimespan();
        startHeight = startHeight < Params().BetStartHeight() ? Params().BetStartHeight() : startHeight;
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
                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = txout.scriptPubKey.ToString();
                    CAmount betAmount = txout.nValue;

                    if (scriptPubKey.length() > 0 && 0 == strncmp(scriptPubKey.c_str(), "OP_RETURN", 9)) {

                        // Ensure TX has it been posted by Oracle wallet.
                        const CTxIn &txin = tx.vin[0];
                        bool validOracleTx = IsValidOracleTx(txin);

                        // Get the OP CODE from the transaction scriptPubKey.
                        std::vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, std::string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        // Peerless event OP RETURN transaction.
                        CPeerlessEvent pe;
                        if (validOracleTx && CPeerlessEvent::FromOpCode(opCode, pe)) {

                            // If the current event matches the result we can now set the odds.
                            if (result.nEventId == pe.nEventId) {

                                LogPrintf("EVENT OP CODE - %s \n", opCode.c_str());

                                UpdateMoneyLine    = true;
                                eventFound         = true;
                                tempEventStartTime = pe.nStartTime;

                                // Set the temp moneyline odds.
                                if (nMoneylineResult == moneyLineHomeWin) {
                                    nTempMoneylineOdds = pe.nHomeOdds;
                                }
                                else if (nMoneylineResult == moneyLineAwayWin) {
                                    nTempMoneylineOdds = pe.nAwayOdds;
                                }
                                else if (nMoneylineResult == moneyLineDraw) {
                                    nTempMoneylineOdds = pe.nDrawOdds;
                                }

                                // Set which team is the favorite, used for calculating spreads difference & winner.
                                if (pe.nHomeOdds < pe.nAwayOdds) {
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
                        CPeerlessUpdateOdds puo;
                        if (eventFound && validOracleTx && CPeerlessUpdateOdds::FromOpCode(opCode, puo) && result.nEventId == puo.nEventId ) {

                            LogPrintf("PUO EVENT OP CODE - %s \n", opCode.c_str());

                            UpdateMoneyLine = true;

                            // If current event ID matches result ID set the odds.
                            if (nMoneylineResult == moneyLineHomeWin) {
                                nTempMoneylineOdds = puo.nHomeOdds;
                            }
                            else if (nMoneylineResult == moneyLineAwayWin) {
                                nTempMoneylineOdds = puo.nAwayOdds;
                            }
                            else if (nMoneylineResult == moneyLineDraw) {
                                nTempMoneylineOdds = puo.nDrawOdds;
                            }
                        }

                        // Handle PSE, when we find a Spreads event on chain we need to update the Spreads odds.
                        CPeerlessSpreadsEvent pse;
                        if (eventFound && validOracleTx && CPeerlessSpreadsEvent::FromOpCode(opCode, pse) && result.nEventId == pse.nEventId) {

                            LogPrintf("PSE EVENT OP CODE - %s \n", opCode.c_str());

                            UpdateSpreads = true;

                            if (pse.nVersion == 1) {
                                // If the home team is the favourite.
                                if (HomeFavorite){
                                    //  Choose the spreads winner.
                                    if (nSpreadsDifference == 0) {
                                        nSpreadsWinner = WinnerType::awayWin;
                                    }
                                    else if (pse.nPoints < nSpreadsDifference) {
                                        nSpreadsWinner = WinnerType::homeWin;
                                    }
                                    else if (pse.nPoints > nSpreadsDifference) {
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
                                    else if (pse.nPoints > nSpreadsDifference) {
                                        nSpreadsWinner = WinnerType::homeWin;
                                    }
                                    else if (pse.nPoints < nSpreadsDifference) {
                                        nSpreadsWinner = WinnerType::awayWin;
                                    }
                                    else {
                                        nSpreadsWinner = WinnerType::push;
                                    }
                                }
                            } else { // if (nVersion == 2)
                                if (nSpreadsDifference == 0) {  // This seems redundant
                                    nSpreadsWinner = HomeFavorite ? WinnerType::awayWin : WinnerType::homeWin;
                                } else {
                                    int32_t difference = result.nHomeScore - result.nAwayScore;
                                    if (pse.nPoints < difference) {
                                        nSpreadsWinner = WinnerType::homeWin;
                                    } else if (pse.nPoints > difference) {
                                        nSpreadsWinner = WinnerType::awayWin;
                                    } else { // if (pse.nPoints = difference)
                                        nSpreadsWinner = WinnerType::push;
                                    }
                                }
                            }

                            // Set the temp spread odds.
                            if (nSpreadsWinner == WinnerType::push) {
                                nTempSpreadsOdds = BET_ODDSDIVISOR;
                            }
                            else if (nSpreadsWinner == WinnerType::awayWin) {
                                nTempSpreadsOdds = pse.nAwayOdds;
                            }
                            else if (nSpreadsWinner == WinnerType::homeWin) {
                                nTempSpreadsOdds = pse.nHomeOdds;
                            }
                        }

                        // Handle PTE, when we find an Totals event on chain we need to update the Totals odds.
                        CPeerlessTotalsEvent pte;
                        if (eventFound && validOracleTx && CPeerlessTotalsEvent::FromOpCode(opCode, pte) && result.nEventId == pte.nEventId) {

                            LogPrintf("PTE EVENT OP CODE - %s \n", opCode.c_str());

                            UpdateTotals = true;

                            // Find totals outcome (result).
                            if (pte.nPoints == nTotalsPoints) {
                                nTotalsWinner = WinnerType::push;
                            }
                            else if (pte.nPoints > nTotalsPoints) {
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
                                nTempTotalsOdds = pte.nUnderOdds;
                            }
                            else if (nTotalsWinner == WinnerType::homeWin) {
                                nTempTotalsOdds = pte.nOverOdds;
                            }
                        }

                        // If we encounter the result after cycling the chain then we dont need go any furture so finish the payout.
                        CPeerlessResult pr;
                        if (eventFound && validOracleTx && CPeerlessResult::FromOpCode(opCode, pr) && result.nEventId == pr.nEventId ) {

                            LogPrintf("Result found ending search \n");

                            return;
                        }

                        // Only payout bets that are between 25 - 10000 WRG inclusive (MaxBetPayoutRange).
                        if (eventFound && betAmount >= (Params().MinBetPayoutRange() * COIN) && betAmount <= (Params().MaxBetPayoutRange() * COIN)) {

                            // Bet OP RETURN transaction.
                            CPeerlessBet pb;
                            if (CPeerlessBet::FromOpCode(opCode, pb)) {

                                UniversalBetKey betKey{nHeight, COutPoint(tx.GetHash(), i)};

                                CAmount payout = 0 * COIN;

                                // If bet was placed less than 20 mins before event start or after event start discard it.
                                if (latestEventStartTime > 0 && (unsigned int) transactionTime > (latestEventStartTime - Params().BetPlaceTimeoutBlocks())) {
                                    continue;
                                }

                                // Is the bet a winning bet?
                                if (result.nEventId == pb.nEventId) {
                                    CAmount winnings = 0;

                                    // If bet payout result.
                                    if (result.nResultType ==  ResultType::standardResult) {

                                        // Calculate winnings.
                                        if (pb.nOutcome == nMoneylineResult) {
                                            winnings = betAmount * nMoneylineOdds;
                                        }
                                        else if (spreadsFound && (pb.nOutcome == vSpreadsResult.at(0) || pb.nOutcome == vSpreadsResult.at(1))) {
                                            winnings = betAmount * nSpreadsOdds;
                                        }
                                        else if (totalsFound && (pb.nOutcome == vTotalsResult.at(0) || pb.nOutcome == vTotalsResult.at(1))) {
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
                                        if (pb.nOutcome == OutcomeType::moneyLineDraw ||
                                                pb.nOutcome == OutcomeType::moneyLineAwayWin ||
                                                pb.nOutcome == OutcomeType::moneyLineHomeWin) {
                                            payout = betAmount;
                                        }
                                        else if (spreadsFound && (pb.nOutcome == vSpreadsResult.at(0) || pb.nOutcome == vSpreadsResult.at(1))) {
                                            winnings = betAmount * nSpreadsOdds;

                                            // Calculate the bet winnings for the current bet.
                                            if (winnings > 0) {
                                                payout = (winnings - ((winnings - betAmount*BET_ODDSDIVISOR) / 1000 * BET_BURNXPERMILLE)) / BET_ODDSDIVISOR;
                                            }
                                            else {
                                                payout = 0;
                                            }
                                        }
                                        else if (totalsFound && (pb.nOutcome == vTotalsResult.at(0) || pb.nOutcome == vTotalsResult.at(1))) {
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

                                    LogPrintf("MoneyLine Refund - PAYOUT\n");
                                    LogPrintf("AMOUNT: %li \n", payout);
                                    LogPrintf("ADDRESS: %s \n", CBitcoinAddress( payoutAddress ).ToString().c_str());

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

    return;
}

/**
 * Creates the bet payout std::vector for all winning CGLotto events.
 *
 * @return payout vector.
 */
void GetCGLottoBetPayoutsV2(int height, std::vector<CBetOut>& vexpectedCGLottoBetPayouts, std::vector<CPayoutInfo>& vPayoutsInfo)
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
                 vPayoutsInfo.emplace_back(candidates[0].second, PayoutType::chainGamesRefund);
                 vexpectedCGLottoBetPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee, allChainGames[currResult].nEventId);
             }
        }
        else if (candidates.size() >= 2) {
            // Use random number to choose winner.
            auto noOfBets    = candidates.size();

            CBlockIndex *winBlockIndex = chainActive[height];
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

            LogPrintf("\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
            LogPrintf("Total number Of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrintf("Winner Address: %u (index no %u) \n", winnerAddress, winnerNr);
            LogPrintf("Total Value of Block: %u \n", totalValueOfBlock);
            LogPrintf("Total Pot: %u, Winnings: %u, Fee: %u \n", totalPot, winnerPayout, fee);

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                UniversalBetKey zeroKey{0, COutPoint()};
                vPayoutsInfo.emplace_back(candidates[winnerNr].second, PayoutType::chainGamesPayout);
                vexpectedCGLottoBetPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee, allChainGames[currResult].nEventId);
                vPayoutsInfo.emplace_back(zeroKey, PayoutType::chainGamesPayout);
                vexpectedCGLottoBetPayouts.emplace_back(fee, GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get()), entranceFee);
            }
        }
    }
}
