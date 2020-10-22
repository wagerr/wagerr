// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_tx.h>
#include <betting/bet_db.h>
#include <betting/bet_v2.h>
#include <betting/bet_v3.h>

#include "uint256.h"
#include "wallet/wallet.h"
#include <boost/filesystem.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/exception/to_string.hpp>

CBettingsView* bettingsView = nullptr;

bool ExtractPayouts(const CBlock& block, std::vector<CTxOut>& vFoundPayouts, uint32_t& nPayoutOffset, uint32_t& nWinnerPayments, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward)
{
    const CTransaction &tx = block.vtx[1];

    // Get the vin staking value so we can use it to find out how many staking TX in the vouts.
    COutPoint prevout         = tx.vin[0].prevout;
    CAmount stakeAmount       = 0;
    bool fStakesFound         = false;

    nPayoutOffset = 0;
    nWinnerPayments = 0;

    uint256 hashBlock;
    CTransaction txPrev;

    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
        stakeAmount = txPrev.vout[prevout.n].nValue + nExpectedMint;
    } else {
        return false;
    }

    // Set the OMNO and Dev reward addresses
    CScript devPayoutScript = GetScriptForDestination(CBitcoinAddress(Params().DevPayoutAddr()).Get());
    CScript OMNOPayoutScript = GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get());

    // Count the coinbase and staking vouts in the current block TX.
    CAmount totalStakeAcc = 0;
    const size_t txVoutSize = tx.vout.size();

    size_t nMaxVoutI = txVoutSize;
    CAmount nMNReward = 0;
    if (txVoutSize > 2 && tx.vout[txVoutSize - 1].nValue == nExpectedMNReward) {
        nMaxVoutI--;
        nMNReward = nExpectedMNReward;
    }

    for (size_t i = 0; i < nMaxVoutI; i++) {
        const CTxOut &txout = tx.vout[i];
        CAmount voutValue   = txout.nValue;
        CScript voutScript = txout.scriptPubKey;
        if (fStakesFound) {
            if (voutScript != devPayoutScript && voutScript != OMNOPayoutScript) {
                nWinnerPayments++;
            }
            if (voutValue > 0) vFoundPayouts.emplace_back(voutValue, voutScript);
        } else {
            nPayoutOffset++;
            totalStakeAcc += voutValue;

            // Needs some slack?
            if (totalStakeAcc + nMNReward == stakeAmount) {
                fStakesFound = true;
            }
        }
    }
    return fStakesFound || (nWinnerPayments == 0 && totalStakeAcc + nMNReward < stakeAmount);
}

bool IsBlockPayoutsValid(CBettingsView &bettingsViewCache, const std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayoutsIn, const CBlock& block, const int nBlockHeight, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward)
{
    const CTransaction &tx = block.vtx[1];
    std::multimap<CPayoutInfoDB, CBetOut> mExpectedPayouts = mExpectedPayoutsIn;

    std::vector<CTxOut> vFoundPayouts;
    std::multiset<CTxOut> setFoundPayouts;
    std::multiset<CTxOut> setExpectedPayouts;

    uint32_t nPayoutOffset = 0;
    uint32_t nWinnerPayments = 0; // unused

    // If we have payouts to validate. Note: bets can only happen in blocks with MN payments.
    if (!ExtractPayouts(block, vFoundPayouts, nPayoutOffset, nWinnerPayments, nExpectedMint, nExpectedMNReward)) {
        LogPrintf("%s - Not all payouts found - %s\n", __func__, block.GetHash().ToString());
        return false;
    }
    setFoundPayouts = std::multiset<CTxOut>(vFoundPayouts.begin(), vFoundPayouts.end());

    for (auto expectedPayout : mExpectedPayouts)
    {
        setExpectedPayouts.insert(expectedPayout.second);
    }

    if (setExpectedPayouts != setFoundPayouts) {
        LogPrintf("%s - Expected payouts:\n", __func__);
        for (auto expectedPayout : setExpectedPayouts) {
            LogPrintf("%s %d %d\n", expectedPayout.nRounds, expectedPayout.nValue, expectedPayout.scriptPubKey.ToString());
        }
        LogPrintf("%s - Found payouts:\n", __func__);
        for (auto foundPayouts : setFoundPayouts) {
            LogPrintf("%s %d %d\n", foundPayouts.nRounds, foundPayouts.nValue, foundPayouts.scriptPubKey.ToString());
        }
        LogPrintf("%s - Not all payouts validate - %s\n", __func__, block.GetHash().ToString());
        return false;
    }

    // Lookup txid+voutnr and store in database cache
    for (uint32_t i = 0; i < vFoundPayouts.size(); i++)
    {
        auto expectedPayout = std::find_if(mExpectedPayouts.begin(), mExpectedPayouts.end(),
                        [vFoundPayouts, i](std::pair<const CPayoutInfoDB, CBetOut> eP) {
                            return eP.second.nValue == vFoundPayouts[i].nValue && eP.second.scriptPubKey == vFoundPayouts[i].scriptPubKey;
                        });
        if (expectedPayout != mExpectedPayouts.end()) {
            PayoutInfoKey payoutInfoKey{static_cast<uint32_t>(nBlockHeight), COutPoint{tx.GetHash(), i+nPayoutOffset}};
            bettingsViewCache.payoutsInfo->Write(payoutInfoKey, expectedPayout->first);

            mExpectedPayouts.erase(expectedPayout);
        } else {
            // Redundant: should not happen after previous test 'setExpectedPayouts != setFoundPayouts'
            LogPrintf("%s - Could not find expected payout - %s\n", __func__, block.GetHash().ToString());
            return false;
        }
    }

    return true;
}

bool CheckBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height)
{
    // if is not hardfork for wagerr v3 - do not check tx
    if (height < Params().WagerrProtocolV3StartHeight()) return true;

    // Get player address
    const CTxIn& txin{tx.vin[0]};
    const bool validOracleTx{IsValidOracleTx(txin)};
    uint256 hashBlock;
    CTransaction txPrev;
    CBitcoinAddress address;
    CTxDestination prevAddr;
    // if we cant extract playerAddress - drop tx
    if (!GetTransaction(txin.prevout.hash, txPrev, hashBlock, true) ||
            !ExtractDestination(txPrev.vout[txin.prevout.n].scriptPubKey, prevAddr)) {
        return false;
    }
    address = CBitcoinAddress(prevAddr);

    for (const CTxOut &txOut : tx.vout) {
        // parse betting TX
        auto bettingTx = ParseBettingTx(txOut);

        if (bettingTx == nullptr) continue;

        CAmount betAmount{txOut.nValue};

        switch(bettingTx->GetTxType()) {
            case plBetTxType:
            {
                CPeerlessBetTx* betTx = (CPeerlessBetTx*) bettingTx.get();
                CPeerlessLegDB plBet{betTx->nEventId, (OutcomeType) betTx->nOutcome};
                // Validate bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().MinBetPayoutRange()  * COIN ) || betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                CPeerlessExtendedEventDB plEvent;
                // Find the event in DB
                if (bettingsViewCache.events->Read(EventKey{plBet.nEventId}, plEvent)) {
                    if (bettingsViewCache.results->Exists(ResultKey{plBet.nEventId})) {
                        return error("CheckBettingTX: Bet placed to resulted event %lu!", plBet.nEventId);
                    }
                }
                else {
                    return error("CheckBettingTX: Failed to find event %lu!", plBet.nEventId);
                }
                break;
            }
            case plParlayBetTxType:
            {
                CPeerlessParlayBetTx* parlayBetTx = (CPeerlessParlayBetTx*) bettingTx.get();
                std::vector<CPeerlessBetTx> &legs = parlayBetTx->legs;

                if (legs.size() > Params().MaxParlayLegs())
                    return error("CheckBettingTX: The invalid parlay bet count of legs!");

                // Validate parlay bet amount so its between 25 - 4000 WGR inclusive.
                if (betAmount < (Params().MinBetPayoutRange()  * COIN ) || betAmount > (Params().MaxParlayBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                // check event ids in legs and deny if some is equal
                {
                    std::set<uint32_t> ids;
                    for (const CPeerlessBetTx& leg : legs) {
                        if (ids.find(leg.nEventId) != ids.end())
                            return error("CheckBettingTX: Parlay bet has some legs with same event id!");
                        else
                            ids.insert(leg.nEventId);
                    }
                }

                for (const CPeerlessBetTx& leg : legs) {
                    CPeerlessExtendedEventDB plEvent;
                    // Find the event in DB
                    if (bettingsViewCache.events->Read(EventKey{leg.nEventId}, plEvent)) {
                        if (bettingsViewCache.results->Exists(ResultKey{leg.nEventId})) {
                            return error("CheckBettingTX: Bet placed to resulted event %lu!", leg.nEventId);
                        }
                    }
                    else {
                        return error("CheckBettingTX: Failed to find event %lu!", leg.nEventId);
                    }
                }
                break;
            }
            case cgBetTxType:
            {
                CChainGamesBetTx* cgBetTx = (CChainGamesBetTx*) bettingTx.get();

                CChainGamesEventDB cgEvent;
                EventKey eventKey{cgBetTx->nEventId};
                if (!bettingsViewCache.chainGamesLottoEvents->Read(eventKey, cgEvent)) {
                    return error("CheckBettingTX: Failed to find event %lu!", cgBetTx->nEventId);
                }
                // Check event result
                if (bettingsViewCache.chainGamesLottoResults->Exists(eventKey)) {
                    return error("CheckBettingTX: Bet placed to resulted event %lu!", cgBetTx->nEventId);
                }
                // Validate chain game bet amount
                if (betAmount != cgEvent.nEntryFee * COIN) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                break;
            }
            case qgBetTxType:
            {
                CQuickGamesBetTx* qgBetTx = (CQuickGamesBetTx*) bettingTx.get();
                if (!(qgBetTx->gameType == QuickGamesType::qgDice)) {
                    return error("CheckBettingTX: Invalid game type (%d)", qgBetTx->gameType);
                }
                // Validate quick game bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().MinBetPayoutRange()  * COIN ) || betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                break;
            }
            case mappingTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CMappingTx* mapTx = (CMappingTx*) bettingTx.get();

                if (bettingsViewCache.mappings->Exists(MappingKey{MappingType(mapTx->nMType), mapTx->nId}))
                    return error("CheckBettingTX: trying to create existed mapping!");
                break;
            }
            case plEventTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CPeerlessEventTx* plEventTx = (CPeerlessEventTx*) bettingTx.get();

                if (bettingsViewCache.events->Exists(EventKey{plEventTx->nEventId}))
                    return error("CheckBettingTX: trying to create existed event id %lu!", plEventTx->nEventId);

                if (!bettingsViewCache.mappings->Exists(MappingKey{sportMapping, (uint32_t) plEventTx->nSport}))
                    return error("CheckBettingTX: trying to create event with unknown sport id %lu!", plEventTx->nSport);

                if (!bettingsViewCache.mappings->Exists(MappingKey{tournamentMapping, (uint32_t) plEventTx->nTournament}))
                    return error("CheckBettingTX: trying to create event with unknown tournament id %lu!", plEventTx->nTournament);

                if (!bettingsViewCache.mappings->Exists(MappingKey{roundMapping, (uint32_t) plEventTx->nStage}))
                    return error("CheckBettingTX: trying to create event with unknown round id %lu!", plEventTx->nStage);

                if (!bettingsViewCache.mappings->Exists(MappingKey{teamMapping, plEventTx->nHomeTeam}))
                    return error("CheckBettingTX: trying to create event with unknown home team id %lu!", plEventTx->nHomeTeam);

                if (!bettingsViewCache.mappings->Exists(MappingKey{teamMapping, plEventTx->nAwayTeam}))
                    return error("CheckBettingTX: trying to create event with unknown away team id %lu!", plEventTx->nAwayTeam);

                /*
                if (!CHECK_ODDS(plEventTx->nHomeOdds))
                    return error("CheckBettingTX: invalid home odds %lu!", plEventTx->nHomeOdds);

                if (!CHECK_ODDS(plEventTx->nAwayOdds))
                    return error("CheckBettingTX: invalid away odds %lu!", plEventTx->nAwayOdds);

                if (!CHECK_ODDS(plEventTx->nDrawOdds))
                    return error("CheckBettingTX: invalid draw odds %lu!", plEventTx->nDrawOdds);
                */

                break;
            }
            case plResultTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CPeerlessResultTx* plResultTx = (CPeerlessResultTx*) bettingTx.get();

                if (!bettingsViewCache.events->Exists(EventKey{plResultTx->nEventId}))
                    return error("CheckBettingTX: trying to result not existed event id %lu!", plResultTx->nEventId);

                if (bettingsViewCache.results->Exists(ResultKey{plResultTx->nEventId}))
                    return error("CheckBettingTX: trying to result already resulted event id %lu!", plResultTx->nEventId);
                break;
            }
            case plUpdateOddsTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CPeerlessUpdateOddsTx* plUpdateOddsTx = (CPeerlessUpdateOddsTx*) bettingTx.get();

                if (!bettingsViewCache.events->Exists(EventKey{plUpdateOddsTx->nEventId}))
                    return error("CheckBettingTX: trying to update not existed event id %lu!", plUpdateOddsTx->nEventId);

                /*
                if (!CHECK_ODDS(plUpdateOddsTx->nHomeOdds))
                    return error("CheckBettingTX: invalid home odds %lu!", plUpdateOddsTx->nHomeOdds);

                if (!CHECK_ODDS(plUpdateOddsTx->nAwayOdds))
                    return error("CheckBettingTX: invalid away odds %lu!", plUpdateOddsTx->nAwayOdds);

                if (!CHECK_ODDS(plUpdateOddsTx->nDrawOdds))
                    return error("CheckBettingTX: invalid draw odds %lu!", plUpdateOddsTx->nDrawOdds);
                */

                break;
            }
            case cgEventTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CChainGamesEventTx* cgEventTx = (CChainGamesEventTx*) bettingTx.get();

                if (bettingsViewCache.chainGamesLottoEvents->Exists(EventKey{cgEventTx->nEventId}))
                    return error("CheckBettingTX: trying to create existed chain games event id %lu!", cgEventTx->nEventId);

                break;
            }
            case cgResultTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CChainGamesResultTx* cgResultTx = (CChainGamesResultTx*) bettingTx.get();

                if (!bettingsViewCache.chainGamesLottoEvents->Exists(EventKey{cgResultTx->nEventId}))
                    return error("CheckBettingTX: trying to result not existed chain games event id %lu!", cgResultTx->nEventId);

                if (bettingsViewCache.chainGamesLottoResults->Exists(ResultKey{cgResultTx->nEventId}))
                    return error("CheckBettingTX: trying to result already resulted chain games event id %lu!", cgResultTx->nEventId);

                break;
            }
            case plSpreadsEventTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CPeerlessSpreadsEventTx* plSpreadsEventTx = (CPeerlessSpreadsEventTx*) bettingTx.get();

                if (!bettingsViewCache.events->Exists(EventKey{plSpreadsEventTx->nEventId}))
                    return error("CheckBettingTX: trying to create spreads at not existed event id %lu!", plSpreadsEventTx->nEventId);

                /*
                if (!CHECK_ODDS(plSpreadsEventTx->nHomeOdds))
                    return error("CheckBettingTX: invalid spreads home odds %lu!", plSpreadsEventTx->nHomeOdds);

                if (!CHECK_ODDS(plSpreadsEventTx->nAwayOdds))
                    return error("CheckBettingTX: invalid spreads away odds %lu!", plSpreadsEventTx->nAwayOdds);
                */

                break;
            }
            case plTotalsEventTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CPeerlessTotalsEventTx* plTotalsEventTx = (CPeerlessTotalsEventTx*) bettingTx.get();

                if (!bettingsViewCache.events->Exists(EventKey{plTotalsEventTx->nEventId}))
                    return error("CheckBettingTX: trying to create totals at not existed event id %lu!", plTotalsEventTx->nEventId);

                /*
                if (!CHECK_ODDS(plTotalsEventTx->nOverOdds))
                    return error("CheckBettingTX: invalid totals over odds %lu!", plTotalsEventTx->nOverOdds);

                if (!CHECK_ODDS(plTotalsEventTx->nUnderOdds))
                    return error("CheckBettingTX: invalid totals under odds %lu!", plTotalsEventTx->nUnderOdds);
                */

                break;
            }
            case plEventPatchTxType:
            {
                if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                CPeerlessEventPatchTx* plEventPatchTx = (CPeerlessEventPatchTx*) bettingTx.get();

                if (!bettingsViewCache.events->Exists(EventKey{plEventPatchTx->nEventId}))
                    return error("CheckBettingTX: trying to patch not existed event id %lu!", plEventPatchTx->nEventId);

                break;
            }
            default:
                continue;
        }
    }
    return true;
}

void ProcessBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height, const int64_t blockTime, const bool wagerrProtocolV3)
{
    LogPrint("wagerr", "ProcessBettingTx: start, time: %lu, tx hash: %s\n", blockTime, tx.GetHash().GetHex());

    // Ensure the event TX has come from Oracle wallet.
    const CTxIn& txin{tx.vin[0]};
    const bool validOracleTx{IsValidOracleTx(txin)};
    // Get player address
    uint256 hashBlock;
    CTransaction txPrev;
    CBitcoinAddress address;
    CTxDestination prevAddr;
    // if we cant extract playerAddress - skip vout
    if (!GetTransaction(txin.prevout.hash, txPrev, hashBlock, true) ||
            !ExtractDestination(txPrev.vout[txin.prevout.n].scriptPubKey, prevAddr)) {
        return;
    }
    address = CBitcoinAddress(prevAddr);

    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CTxOut &txOut = tx.vout[i];
        // parse betting TX
        auto bettingTx = ParseBettingTx(txOut);

        if (bettingTx == nullptr) continue;

        CAmount betAmount{txOut.nValue};
        COutPoint outPoint{tx.GetHash(), (uint32_t) i};
        uint256 bettingTxId = SerializeHash(outPoint);

        switch(bettingTx->GetTxType()) {
            /* Player's tx types */
            case plBetTxType:
            {
                CPeerlessBetTx* betTx = (CPeerlessBetTx*) bettingTx.get();
                CPeerlessLegDB plBet{betTx->nEventId, (OutcomeType) betTx->nOutcome};
                CPeerlessExtendedEventDB plEvent, plCachedEvent;

                LogPrint("wagerr", "CPeerlessBet: id: %lu, outcome: %lu\n", plBet.nEventId, plBet.nOutcome);
                // Find the event in DB
                EventKey eventKey{plBet.nEventId};
                // get locked event from upper level cache for getting correct odds
                if (bettingsView->events->Read(eventKey, plCachedEvent) &&
                        bettingsViewCache.events->Read(eventKey, plEvent)) {
                    CAmount payout = 0 * COIN;
                    CAmount burn = 0;

                    LogPrint("wagerr", "plCachedEvent: homeOdds: %lu, awayOdds: %lu, drawOdds: %lu, spreadHomeOdds: %lu, spreadAwayOdds: %lu, totalOverOdds: %lu, totalUnderOdds: %lu\n",
                        plCachedEvent.nHomeOdds, plCachedEvent.nAwayOdds, plCachedEvent.nDrawOdds, plCachedEvent.nSpreadHomeOdds, plCachedEvent.nSpreadAwayOdds, plCachedEvent.nTotalOverOdds, plCachedEvent.nTotalUnderOdds);

                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});
                    // Check which outcome the bet was placed on and add to accumulators
                    switch (plBet.nOutcome) {
                        case moneyLineHomeWin:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nHomeOdds, payout, burn);

                            plEvent.nMoneyLineHomePotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineHomeBets += 1;
                            break;
                        case moneyLineAwayWin:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nAwayOdds, payout, burn);

                            plEvent.nMoneyLineAwayPotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineAwayBets += 1;
                            break;
                        case moneyLineDraw:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nDrawOdds, payout, burn);

                            plEvent.nMoneyLineDrawPotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineDrawBets += 1;
                            break;
                        case spreadHome:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nSpreadHomeOdds, payout, burn);

                            plEvent.nSpreadHomePotentialLiability += payout / COIN ;
                            plEvent.nSpreadPushPotentialLiability += betAmount / COIN;
                            plEvent.nSpreadHomeBets += 1;
                            plEvent.nSpreadPushBets += 1;
                            break;
                        case spreadAway:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nSpreadAwayOdds, payout, burn);

                            plEvent.nSpreadAwayPotentialLiability += payout / COIN ;
                            plEvent.nSpreadPushPotentialLiability += betAmount / COIN;
                            plEvent.nSpreadAwayBets += 1;
                            plEvent.nSpreadPushBets += 1;
                            break;
                        case totalOver:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nTotalOverOdds, payout, burn);

                            plEvent.nTotalOverPotentialLiability += payout / COIN ;
                            plEvent.nTotalPushPotentialLiability += betAmount / COIN;
                            plEvent.nTotalOverBets += 1;
                            plEvent.nTotalPushBets += 1;
                            break;
                        case totalUnder:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nTotalUnderOdds, payout, burn);

                            plEvent.nTotalUnderPotentialLiability += payout / COIN;
                            plEvent.nTotalPushPotentialLiability += betAmount / COIN;
                            plEvent.nTotalUnderBets += 1;
                            plEvent.nTotalPushBets += 1;
                            break;
                        default:
                            std::runtime_error("Unknown bet outcome type!");
                            break;
                    }
                    if (!bettingsViewCache.events->Update(eventKey, plEvent)) {
                        // should not happen ever
                        LogPrintf("Failed to update event!\n");
                        continue;
                    }

                    bettingsViewCache.bets->Write(PeerlessBetKey{static_cast<uint32_t>(height), outPoint}, CPeerlessBetDB(betAmount, address, {plBet}, {plCachedEvent}, blockTime));
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plParlayBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CPeerlessParlayBetTx* parlayBetTx = (CPeerlessParlayBetTx*) bettingTx.get();
                std::vector<CPeerlessBaseEventDB> lockedEvents;
                std::vector<CPeerlessLegDB> legs;
                // convert tx legs format to db
                LogPrint("wagerr", "ParlayBet: legs: ");
                for (auto leg : parlayBetTx->legs) {
                    LogPrint("wagerr", "(id: %lu, outcome: %lu), ", leg.nEventId, leg.nOutcome);
                    legs.emplace_back(leg.nEventId, (OutcomeType) leg.nOutcome);
                }
                LogPrint("wagerr", "\n");

                std::vector<CBettingUndoDB> vUndos;
                for (const CPeerlessLegDB& leg : legs) {
                    CPeerlessExtendedEventDB plEvent, plCachedEvent;
                    EventKey eventKey{leg.nEventId};
                    // Find the event in DB
                    // get locked event from upper level cache of betting view for getting correct odds
                    if (bettingsView->events->Read(eventKey, plCachedEvent) &&
                            bettingsViewCache.events->Read(eventKey, plEvent)) {

                        LogPrint("wagerr", "plCachedEvent: homeOdds: %lu, awayOdds: %lu, drawOdds: %lu, spreadHomeOdds: %lu, spreadAwayOdds: %lu, totalOverOdds: %lu, totalUnderOdds: %lu\n",
                            plCachedEvent.nHomeOdds, plCachedEvent.nAwayOdds, plCachedEvent.nDrawOdds, plCachedEvent.nSpreadHomeOdds, plCachedEvent.nSpreadAwayOdds, plCachedEvent.nTotalOverOdds, plCachedEvent.nTotalUnderOdds);

                        vUndos.emplace_back(BettingUndoVariant{plEvent}, (uint32_t)height);
                        switch (leg.nOutcome) {
                            case moneyLineHomeWin:
                                plEvent.nMoneyLineHomeBets += 1;
                                break;
                            case moneyLineAwayWin:
                                plEvent.nMoneyLineAwayBets += 1;
                                break;
                            case moneyLineDraw:
                                plEvent.nMoneyLineDrawBets += 1;
                                break;
                            case spreadHome:
                                plEvent.nSpreadHomeBets += 1;
                                plEvent.nSpreadPushBets += 1;
                                break;
                            case spreadAway:
                                plEvent.nSpreadAwayBets += 1;
                                plEvent.nSpreadPushBets += 1;
                                break;
                            case totalOver:
                                plEvent.nTotalOverBets += 1;
                                plEvent.nTotalPushBets += 1;
                                break;
                            case totalUnder:
                                plEvent.nTotalUnderBets += 1;
                                plEvent.nTotalPushBets += 1;
                                break;
                            default:
                                std::runtime_error("Unknown bet outcome type!");
                        }

                        lockedEvents.emplace_back(plCachedEvent);
                        bettingsViewCache.events->Update(eventKey, plEvent);
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                        continue;
                    }
                }
                if (!legs.empty()) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, vUndos);
                    bettingsViewCache.bets->Write(PeerlessBetKey{static_cast<uint32_t>(height), outPoint}, CPeerlessBetDB(betAmount, address, legs, lockedEvents, blockTime));
                }
                break;
            }
            case cgBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CChainGamesBetTx* cgBetTx = (CChainGamesBetTx*) bettingTx.get();

                LogPrint("wagerr", "CChainGamesBetTx: nEventId: %lu,", cgBetTx->nEventId);
                if (!bettingsView->chainGamesLottoEvents->Exists(EventKey{cgBetTx->nEventId})) {
                    LogPrintf("Failed to find event!\n");
                    continue;
                }

                if (!bettingsViewCache.chainGamesLottoBets->Write(
                        ChainGamesBetKey{static_cast<uint32_t>(height), outPoint},
                        CChainGamesBetDB{cgBetTx->nEventId, betAmount, address, blockTime})) {
                    LogPrintf("Failed to write bet!\n");
                    continue;
                }
                break;
            }
            case qgBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CQuickGamesBetTx* qgBetTx = (CQuickGamesBetTx*) bettingTx.get();

                LogPrint("wagerr", "CQuickGamesBetTx: gameType: %d, betInfo: %s\n", qgBetTx->gameType, std::string(qgBetTx->vBetInfo.begin(), qgBetTx->vBetInfo.end()));
                if (!bettingsViewCache.quickGamesBets->Write(
                        QuickGamesBetKey{static_cast<uint32_t>(height), outPoint},
                        CQuickGamesBetDB{ (QuickGamesType) qgBetTx->gameType, qgBetTx->vBetInfo, betAmount, address, blockTime})) {
                    LogPrintf("Failed to write bet!\n");
                }
                break;
            }

            /* Oracle's tx types */

            case mappingTxType:
            {
                if (!validOracleTx) break;

                CMappingTx* mapTx = (CMappingTx*) bettingTx.get();

                LogPrint("wagerr", "CMapping: type: %lu, id: %lu, name: %s\n", mapTx->nMType, mapTx->nId, mapTx->sName);
                if (!bettingsViewCache.mappings->Write(MappingKey{MappingType(mapTx->nMType), mapTx->nId}, CMappingDB{mapTx->sName})) {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to write new mapping!\n");
                }
                break;
            }
            case plEventTxType:
            {
                if (!validOracleTx) break;

                CPeerlessEventTx* plEventTx = (CPeerlessEventTx*) bettingTx.get();

                LogPrint("wagerr", "CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                    plEventTx->nEventId,
                    plEventTx->nSport,
                    plEventTx->nTournament,
                    plEventTx->nStage,
                    plEventTx->nHomeTeam,
                    plEventTx->nAwayTeam,
                    plEventTx->nHomeOdds,
                    plEventTx->nAwayOdds,
                    plEventTx->nDrawOdds);

                CPeerlessExtendedEventDB plEvent;
                plEvent.ExtractDataFromTx(*plEventTx);

                if (!wagerrProtocolV3) {
                    plEvent.nEventCreationHeight = height;
                    plEvent.fLegacyInitialHomeFavorite =  plEventTx->nHomeOdds < plEventTx->nAwayOdds ? true : false;
                }

                EventKey eventKey{plEvent.nEventId};

                if (!bettingsViewCache.events->Write(eventKey, plEvent)) {
                    CPeerlessExtendedEventDB plEventToPatch;
                    if (!wagerrProtocolV3 &&
                            bettingsViewCache.events->Read(eventKey, plEventToPatch)) {
                        LogPrint("wagerr", "CPeerlessEvent - Legacy - try to patch with new event data: id: %lu, time: %lu\n", plEvent.nEventId, plEvent.nStartTime);
                        // save prev event state to undo
                        bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEventToPatch}, (uint32_t)height}});

                        plEventToPatch.nStartTime = plEvent.nStartTime;
                        plEventToPatch.nSport = plEvent.nSport;
                        plEventToPatch.nTournament = plEvent.nTournament;
                        plEventToPatch.nStage = plEvent.nStage;
                        plEventToPatch.nHomeTeam = plEvent.nHomeTeam;
                        plEventToPatch.nAwayTeam = plEvent.nAwayTeam;
                        plEventToPatch.nHomeOdds = plEvent.nHomeOdds;
                        plEventToPatch.nAwayOdds = plEvent.nAwayOdds;
                        plEventToPatch.nDrawOdds = plEvent.nDrawOdds;

                        if (!bettingsViewCache.events->Update(eventKey, plEventToPatch)) {
                            // should not happen ever
                            LogPrintf("Failed to update event!\n");
                        }
                    } else {
                        if (!wagerrProtocolV3) {
                            // save failed tx to db, for avoiding undo issues
                            bettingsViewCache.SaveFailedTx(bettingTxId);
                        }
                        LogPrintf("Failed to write new event!\n");
                    }
                }
                break;
            }
            case plResultTxType:
            {
                if (!validOracleTx) break;

                CPeerlessResultTx* plResultTx = (CPeerlessResultTx*) bettingTx.get();

                LogPrint("wagerr", "CPeerlessResult: id: %lu, resultType: %lu, homeScore: %lu, awayScore: %lu\n",
                    plResultTx->nEventId, plResultTx->nResultType, plResultTx->nHomeScore, plResultTx->nAwayScore);

                CPeerlessResultDB plResult{plResultTx->nEventId, plResultTx->nResultType, plResultTx->nHomeScore, plResultTx->nAwayScore};

                if (!bettingsViewCache.events->Exists(EventKey{plResult.nEventId})) {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to find event!\n");
                    break;
                }

                if (!bettingsViewCache.results->Write(ResultKey{plResult.nEventId}, plResult)) {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to write result!\n");
                    break;
                }
                break;
            }
            case plUpdateOddsTxType:
            {
                if (!validOracleTx) break;

                CPeerlessUpdateOddsTx* plUpdateOddsTx = (CPeerlessUpdateOddsTx*) bettingTx.get();

                LogPrint("wagerr", "CPeerlessUpdateOdds: id: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n", plUpdateOddsTx->nEventId, plUpdateOddsTx->nHomeOdds, plUpdateOddsTx->nAwayOdds, plUpdateOddsTx->nDrawOdds);

                EventKey eventKey{plUpdateOddsTx->nEventId};
                CPeerlessExtendedEventDB plEvent;
                // First check a peerless event exists in DB.
                if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                    plEvent.ExtractDataFromTx(*plUpdateOddsTx);

                    // Update the event in the DB.
                    if (!bettingsViewCache.events->Update(eventKey, plEvent))
                        LogPrintf("Failed to update event!\n");
                }
                else {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case cgEventTxType:
            {
                if (!wagerrProtocolV3) break;

                CChainGamesEventTx* cgEventTx = (CChainGamesEventTx*) bettingTx.get();

                LogPrint("wagerr", "CChainGamesEventTx: nEventId: %d, nEntryFee: %d\n", cgEventTx->nEventId, cgEventTx->nEntryFee);

                EventKey eventKey{cgEventTx->nEventId};
                if (!bettingsViewCache.chainGamesLottoEvents->Write(
                        eventKey,
                        CChainGamesEventDB{ cgEventTx->nEventId, cgEventTx->nEntryFee })) {
                    LogPrintf("Failed to write new event!\n");
                    break;
                }
                break;
            }
            case cgResultTxType:
            {
                if (!wagerrProtocolV3) break;

                CChainGamesResultTx* cgResultTx = (CChainGamesResultTx*) bettingTx.get();

                LogPrint("wagerr", "CChainGamesResultTx: nEventId: %d\n", cgResultTx->nEventId);

                if (!bettingsViewCache.chainGamesLottoEvents->Exists(EventKey{cgResultTx->nEventId})) {
                    LogPrintf("Failed to find event!\n");
                    break;
                }
                if (!bettingsViewCache.chainGamesLottoResults->Write(
                        ResultKey{cgResultTx->nEventId},
                        CChainGamesResultDB{ cgResultTx->nEventId })) {
                    LogPrintf("Failed to write result!\n");
                    break;
                }
                break;
            }
            case plSpreadsEventTxType:
            {
                if (!validOracleTx) break;

                CPeerlessSpreadsEventTx* plSpreadsEventTx = (CPeerlessSpreadsEventTx*) bettingTx.get();

                LogPrint("wagerr", "CPeerlessSpreadsEvent: id: %lu, spreadPoints: %lu, homeOdds: %lu, awayOdds: %lu\n",
                    plSpreadsEventTx->nEventId, plSpreadsEventTx->nPoints, plSpreadsEventTx->nHomeOdds, plSpreadsEventTx->nAwayOdds);

                CPeerlessExtendedEventDB plEvent;
                EventKey eventKey{plSpreadsEventTx->nEventId};
                // First check a peerless event exists in the event index.
                if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                    plEvent.ExtractDataFromTx(*plSpreadsEventTx);
                    // Update the event in the DB.
                    if (!bettingsViewCache.events->Update(eventKey, plEvent))
                        LogPrintf("Failed to update event!\n");
                }
                else {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plTotalsEventTxType:
            {
                if (!validOracleTx) break;

                CPeerlessTotalsEventTx* plTotalsEventTx = (CPeerlessTotalsEventTx*) bettingTx.get();

                LogPrint("wagerr", "CPeerlessTotalsEvent: id: %lu, totalPoints: %lu, overOdds: %lu, underOdds: %lu\n",
                    plTotalsEventTx->nEventId, plTotalsEventTx->nPoints, plTotalsEventTx->nOverOdds, plTotalsEventTx->nUnderOdds);

                CPeerlessExtendedEventDB plEvent;
                EventKey eventKey{plTotalsEventTx->nEventId};
                // First check a peerless event exists in the event index.
                if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                    plEvent.ExtractDataFromTx(*plTotalsEventTx);

                    // Update the event in the DB.
                    if (!bettingsViewCache.events->Update(eventKey, plEvent))
                        LogPrintf("Failed to update event!\n");
                }
                else {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plEventPatchTxType:
            {
                if (!validOracleTx) break;

                CPeerlessEventPatchTx* plEventPatchTx = (CPeerlessEventPatchTx*) bettingTx.get();
                LogPrint("wagerr", "CPeerlessEventPatch: id: %lu, time: %lu\n", plEventPatchTx->nEventId, plEventPatchTx->nStartTime);
                CPeerlessExtendedEventDB plEvent;
                EventKey eventKey{plEventPatchTx->nEventId};
                // First check a peerless event exists in DB
                if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                    plEvent.ExtractDataFromTx(*plEventPatchTx);

                    if (!bettingsViewCache.events->Update(eventKey, plEvent))
                        LogPrintf("Failed to update event!\n");
                }
                else {
                    if (!wagerrProtocolV3) {
                        // save failed tx to db, for avoiding undo issues
                        bettingsViewCache.SaveFailedTx(bettingTxId);
                    }
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            default:
                break;
        }
    }
    LogPrint("wagerr", "ProcessBettingTx: end\n");
}

CAmount GetBettingPayouts(CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayouts)
{
    if (nNewBlockHeight < Params().BetStartHeight()) return 0;

    CAmount expectedMint = 0;
    std::vector<CBetOut> vExpectedPayouts;
    std::vector<CPayoutInfoDB> vPayoutsInfo;

    // Get the PL and CG bet payout TX's so we can calculate the winning bet vector which is used to mint coins and payout bets.
    if (nNewBlockHeight >= Params().WagerrProtocolV3StartHeight()) {

        GetPLBetPayoutsV3(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);

        GetCGLottoBetPayoutsV3(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);

        GetQuickGamesBetPayouts(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
    }
    else {

        GetPLBetPayoutsV3(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);

        GetCGLottoBetPayoutsV2(nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
    }

    assert(vExpectedPayouts.size() == vPayoutsInfo.size());

    mExpectedPayouts.clear();

    for (unsigned int i = 0; i < vExpectedPayouts.size(); i++) {
        expectedMint += vExpectedPayouts[i].nValue;
        mExpectedPayouts.insert(std::pair<const CPayoutInfoDB, CBetOut>(vPayoutsInfo[i], vExpectedPayouts[i]));
    }

    return expectedMint;

}

/*
 * Undo betting
 */

bool UndoEventChanges(CBettingsView& bettingsViewCache, const BettingUndoKey& undoKey, const uint32_t height)
{
    std::vector<CBettingUndoDB> vUndos = bettingsViewCache.GetBettingUndo(undoKey);

    for (auto undo : vUndos) {
        // undo data is inconsistent
        if (!undo.Inited() || undo.Get().which() != UndoPeerlessEvent || undo.height != height) {
            std::runtime_error("Invalid undo state!");
        }
        else {
            CPeerlessExtendedEventDB event = boost::get<CPeerlessExtendedEventDB>(undo.Get());
            LogPrintf("UndoEventChanges: CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu favorite: %s\n",
                            event.nEventId,
                            event.nSport,
                            event.nTournament,
                            event.nStage,
                            event.nHomeTeam,
                            event.nAwayTeam,
                            event.nHomeOdds,
                            event.nAwayOdds,
                            event.nDrawOdds,
                            event.fLegacyInitialHomeFavorite ? "home" : "away");

            if (!bettingsViewCache.events->Update(EventKey{event.nEventId}, event))
                std::runtime_error("Couldn't revert event when undo!");
        }
    }

    return bettingsViewCache.EraseBettingUndo(undoKey);
}

bool UndoBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const uint32_t height)
{
    // Ensure the event TX has come from Oracle wallet.
    const CTxIn& txin{tx.vin[0]};
    const bool validOracleTx{IsValidOracleTx(txin)};

    LogPrintf("UndoBettingTx: start undo, block heigth %lu, tx hash %s\n", height, tx.GetHash().GetHex());

    bool wagerrProtocolV3 = height >= (uint32_t)Params().WagerrProtocolV3StartHeight();

    // undo changes in back order
    for (int i = tx.vout.size() - 1; i >= 0 ; i--) {
        const CTxOut &txOut = tx.vout[i];
        // parse betting TX
        auto bettingTx = ParseBettingTx(txOut);

        if (bettingTx == nullptr) continue;

        COutPoint outPoint{tx.GetHash(), (uint32_t) i};
        uint256 bettingTxId = SerializeHash(outPoint);

        if (!wagerrProtocolV3 && bettingsViewCache.ExistFailedTx(bettingTxId)) {
            // failed tx, just skip it
            bettingsViewCache.EraseFailedTx(bettingTxId);
            continue;
        }

        switch(bettingTx->GetTxType()) {
            /* Player's tx types */
            case plBetTxType:
            {
                CPeerlessBetTx* betTx = (CPeerlessBetTx*) bettingTx.get();
                CPeerlessLegDB plBet{betTx->nEventId, (OutcomeType) betTx->nOutcome};
                CPeerlessExtendedEventDB plEvent, lockedEvent;

                LogPrintf("CPeerlessBet: id: %lu, outcome: %lu\n", plBet.nEventId, plBet.nOutcome);

                if (bettingsViewCache.events->Exists(EventKey{plBet.nEventId})) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    // erase bet from db
                    PeerlessBetKey key{static_cast<uint32_t>(height), outPoint};
                    bettingsViewCache.bets->Erase(key);
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plParlayBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CPeerlessParlayBetTx* parlayBetTx = (CPeerlessParlayBetTx*) bettingTx.get();
                std::vector<CPeerlessLegDB> legs;
                // convert tx legs format to db
                LogPrintf("ParlayBet: legs: ");
                for (auto leg : parlayBetTx->legs) {
                    LogPrintf("(id: %lu, outcome: %lu), ", leg.nEventId, leg.nOutcome);
                    legs.emplace_back(leg.nEventId, (OutcomeType) leg.nOutcome);
                }
                LogPrintf("\n");

                bool allEventsExist = true;

                for (auto leg : legs) {
                    if (!bettingsViewCache.events->Exists(EventKey{leg.nEventId})) {
                        LogPrintf("Failed to find event!\n");
                        allEventsExist = false;
                        break;
                    }
                }

                if (!legs.empty() && allEventsExist) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    // erase bet from db
                    PeerlessBetKey key{static_cast<uint32_t>(height), outPoint};
                    bettingsViewCache.bets->Erase(key);
                }

                break;
            }
            case cgBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CChainGamesBetTx* cgBetTx = (CChainGamesBetTx*) bettingTx.get();

                LogPrintf("CChainGamesBetTx: nEventId: %d\n", cgBetTx->nEventId);

                if (!bettingsView->chainGamesLottoEvents->Exists(EventKey{cgBetTx->nEventId})) {
                    LogPrintf("Failed to find event!\n");
                    continue;
                }

                if (!bettingsViewCache.chainGamesLottoBets->Erase(ChainGamesBetKey{static_cast<uint32_t>(height), outPoint})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                break;
            }
            case qgBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CQuickGamesBetTx* qgBetTx = (CQuickGamesBetTx*) bettingTx.get();

                LogPrintf("CQuickGamesBetTx: gameType: %d, betInfo: %s\n", qgBetTx->gameType, std::string(qgBetTx->vBetInfo.begin(), qgBetTx->vBetInfo.end()));

                if (!bettingsViewCache.quickGamesBets->Erase(QuickGamesBetKey{static_cast<uint32_t>(height), outPoint})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                break;
            }

            /* Oracle's tx types */

            case mappingTxType:
            {
                if (!validOracleTx) break;

                CMappingTx* mapTx = (CMappingTx*) bettingTx.get();

                LogPrintf("CMapping: type: %lu, id: %lu, name: %s\n", mapTx->nMType, mapTx->nId, mapTx->sName);

                MappingKey key{(MappingType)mapTx->nMType, mapTx->nId};

                if (bettingsViewCache.mappings->Exists(key)) {
                    if (!bettingsViewCache.mappings->Erase(key)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                break;
            }
            case plEventTxType:
            {
                if (!validOracleTx) break;

                CPeerlessEventTx* plEventTx = (CPeerlessEventTx*) bettingTx.get();

                LogPrintf("CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                    plEventTx->nEventId,
                    plEventTx->nSport,
                    plEventTx->nTournament,
                    plEventTx->nStage,
                    plEventTx->nHomeTeam,
                    plEventTx->nAwayTeam,
                    plEventTx->nHomeOdds,
                    plEventTx->nAwayOdds,
                    plEventTx->nDrawOdds);

                if (bettingsViewCache.events->Exists(EventKey{plEventTx->nEventId})) {
                    // try to undo legacy event patch
                    if (!wagerrProtocolV3 && bettingsViewCache.ExistsBettingUndo(bettingTxId)) {
                        if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                            LogPrintf("Revert failed!\n");
                            return false;
                        }
                    }
                    else if (!bettingsViewCache.events->Erase(EventKey{plEventTx->nEventId})) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }

                break;
            }
            case plResultTxType:
            {
                if (!validOracleTx) break;

                CPeerlessResultTx* plResultTx = (CPeerlessResultTx*) bettingTx.get();

                LogPrintf("CPeerlessResult: id: %lu, resultType: %lu, homeScore: %lu, awayScore: %lu\n",
                    plResultTx->nEventId, plResultTx->nResultType, plResultTx->nHomeScore, plResultTx->nAwayScore);

                if (bettingsViewCache.results->Exists(ResultKey{plResultTx->nEventId})) {
                    if (!bettingsViewCache.results->Erase(ResultKey{plResultTx->nEventId})) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                else {
                    LogPrintf("Failed to find result!\n");
                }
                break;
            }
            case plUpdateOddsTxType:
            {
                if (!validOracleTx) break;

                CPeerlessUpdateOddsTx* plUpdateOddsTx = (CPeerlessUpdateOddsTx*) bettingTx.get();

                LogPrintf("CPeerlessUpdateOdds: id: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                    plUpdateOddsTx->nEventId, plUpdateOddsTx->nHomeOdds, plUpdateOddsTx->nAwayOdds, plUpdateOddsTx->nDrawOdds);

                if (bettingsViewCache.events->Exists(EventKey{plUpdateOddsTx->nEventId})) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case cgEventTxType:
            {
                if (!validOracleTx) break;

                if (!wagerrProtocolV3) break;

                CChainGamesEventTx* cgEventTx = (CChainGamesEventTx*) bettingTx.get();

                LogPrintf("CChainGamesEventTx: nEventId: %d, nEntryFee: %d\n", cgEventTx->nEventId, cgEventTx->nEntryFee);

                if (!bettingsViewCache.chainGamesLottoEvents->Erase(EventKey{cgEventTx->nEventId})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                break;
            }
            case cgResultTxType:
            {
                if (!validOracleTx) break;

                if (!wagerrProtocolV3) break;

                CChainGamesResultTx* cgResultTx = (CChainGamesResultTx*) bettingTx.get();

                LogPrintf("CChainGamesEventTx: nEventId: %d\n", cgResultTx->nEventId);

                if (!bettingsViewCache.chainGamesLottoResults->Erase(ResultKey{cgResultTx->nEventId})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                break;
            }
            case plSpreadsEventTxType:
            {
                if (!validOracleTx) break;

                CPeerlessSpreadsEventTx* plSpreadsEventTx = (CPeerlessSpreadsEventTx*) bettingTx.get();

                LogPrintf("CPeerlessSpreadsEvent: id: %lu, spreadPoints: %lu, homeOdds: %lu, awayOdds: %lu\n",
                    plSpreadsEventTx->nEventId, plSpreadsEventTx->nPoints, plSpreadsEventTx->nHomeOdds, plSpreadsEventTx->nAwayOdds);

                if (bettingsViewCache.events->Exists(EventKey{plSpreadsEventTx->nEventId})) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plTotalsEventTxType:
            {
                if (!validOracleTx) break;

                CPeerlessTotalsEventTx* plTotalsEventTx = (CPeerlessTotalsEventTx*) bettingTx.get();

                LogPrintf("CPeerlessTotalsEvent: id: %lu, totalPoints: %lu, overOdds: %lu, underOdds: %lu\n",
                    plTotalsEventTx->nEventId, plTotalsEventTx->nPoints, plTotalsEventTx->nOverOdds, plTotalsEventTx->nUnderOdds);

                if (bettingsViewCache.events->Exists(EventKey{plTotalsEventTx->nEventId})) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }

                break;
            }
            case plEventPatchTxType:
            {
                if (!validOracleTx) break;

                CPeerlessEventPatchTx* plEventPatchTx = (CPeerlessEventPatchTx*) bettingTx.get();
                LogPrintf("CPeerlessEventPatch: id: %lu, time: %lu\n", plEventPatchTx->nEventId, plEventPatchTx->nStartTime);

                if (bettingsViewCache.events->Exists(EventKey{plEventPatchTx->nEventId})) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            default:
                break;
        }
    }

    LogPrintf("UndoBettingTx: end\n");
    return true;
}

/**
 * Undo only bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoBetPayouts(CBettingsView &bettingsViewCache, int height)
{
    int nCurrentHeight = chainActive.Height();
    // Get all the results posted in the previous block.
    std::vector<CPeerlessResultDB> results = GetEventResults(height - 1);

    LogPrintf("Start undo payouts...\n");

    for (auto result : results) {

        // look bets at last 14 days
        uint32_t startHeight = nCurrentHeight >= Params().BetBlocksIndexTimespan() ? nCurrentHeight - Params().BetBlocksIndexTimespan() : 0;

        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<PeerlessBetKey, CPeerlessBetDB>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(PeerlessBetKey{startHeight, COutPoint()})); it->Valid(); it->Next()) {
            PeerlessBetKey uniBetKey;
            CPeerlessBetDB uniBet;
            CBettingDB::BytesToDbType(it->Key(), uniBetKey);
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            // skip if bet is uncompleted
            if (!uniBet.IsCompleted()) continue;

            bool needUndo = false;

            // parlay bet
            if (uniBet.legs.size() > 1) {
                bool resultFound = false;
                for (auto leg : uniBet.legs) {
                    // if we found one result for parlay - check each other legs
                    if (leg.nEventId == result.nEventId) {
                        resultFound = true;
                    }
                }
                if (resultFound) {
                    // make assumption that parlay is handled
                    needUndo = true;
                    // find all results for all legs
                    for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                        CPeerlessLegDB &leg = uniBet.legs[idx];
                        // skip this bet if incompleted (can't find one result)
                        CPeerlessResultDB res;
                        if (!bettingsViewCache.results->Read(ResultKey{leg.nEventId}, res)) {
                            needUndo = false;
                        }
                    }
                }
            }
            // single bet
            else if (uniBet.legs.size() == 1) {
                CPeerlessLegDB &singleBet = uniBet.legs[0];
                if (singleBet.nEventId == result.nEventId) {
                    needUndo = true;
                }
            }

            if (needUndo) {
                uniBet.SetUncompleted();
                uniBet.resultType = BetResultType::betResultUnknown;
                uniBet.payout = 0;
                vEntriesToUpdate.emplace_back(std::pair<PeerlessBetKey, CPeerlessBetDB>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.bets->Update(pair.first, pair.second);
        }
    }
    return true;
}

/* Revert payouts info from DB */
bool UndoPayoutsInfo(CBettingsView &bettingsViewCache, int height)
{
    // we should save array of entries to delete because
    // changing (add/delete) of flushable DB when iterating is not allowed
    std::vector<PayoutInfoKey> entriesToDelete;
    auto it = bettingsViewCache.payoutsInfo->NewIterator();
    for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{static_cast<uint32_t>(height), COutPoint()})); it->Valid(); it->Next()) {
        PayoutInfoKey key;
        CBettingDB::BytesToDbType(it->Key(), key);
        if ((int64_t)key.blockHeight != height)
            break;
        else
            entriesToDelete.emplace_back(key);
    }

    // delete all entries with height of disconnected block
    for (auto&& key : entriesToDelete) {
        if (!bettingsViewCache.payoutsInfo->Erase(key))
            return false;
    }

    return true;
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
    std::vector<std::pair<QuickGamesBetKey, CQuickGamesBetDB>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(QuickGamesBetKey{blockHeight, COutPoint()})); it->Valid(); it->Next()) {
        QuickGamesBetKey qgBetKey;
        CQuickGamesBetDB qgBet;
        CBettingDB::BytesToDbType(it->Key(), qgBetKey);
        CBettingDB::BytesToDbType(it->Value(), qgBet);
        // skip if bet is uncompleted
        if (!qgBet.IsCompleted()) continue;

        qgBet.SetUncompleted();
        qgBet.resultType = BetResultType::betResultUnknown;
        qgBet.payout = 0;
        vEntriesToUpdate.emplace_back(std::pair<QuickGamesBetKey, CQuickGamesBetDB>{qgBetKey, qgBet});
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.quickGamesBets->Update(pair.first, pair.second);
    }
    return true;
}

bool BettingUndo(CBettingsView& bettingsViewCache, int height, const std::vector<CTransaction>& vtx)
{
        // Revert betting dats
    if (height > Params().BetStartHeight()) {
        // revert complete bet payouts marker
        if (!UndoBetPayouts(bettingsViewCache, height)) {
            error("DisconnectBlock(): undo payout data is inconsistent");
            return false;
        }
        if (!UndoQuickGamesBetPayouts(bettingsViewCache, height)) {
            error("DisconnectBlock(): undo payout data for quick games bets is inconsistent");
            return false;
        }
        if (!UndoPayoutsInfo(bettingsViewCache, height)) {
            error("DisconnectBlock(): undo payouts info failed");
            return false;
        }

        // undo betting txs in back order
        for (auto it = vtx.crbegin(); it != vtx.crend(); it++) {
            if (!UndoBettingTx(bettingsViewCache, *it, height)) {
                error("DisconnectBlock(): custom transaction and undo data inconsistent");
                return false;
            }
        }
    }
    return true;
}