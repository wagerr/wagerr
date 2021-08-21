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

    bool fWagerrProtocolV4 = nLastBlockHeight >= Params().WagerrProtocolV4StartHeight();

    if (!fWagerrProtocolV4)
        return;

    uint64_t refundOdds{BET_ODDSDIVISOR};

    // Get all the results posted in the prev block.
    std::vector<CFieldResultDB> results = GetFieldResults(nLastBlockHeight);

    CAmount effectivePayoutsSum, grossPayoutsSum = effectivePayoutsSum = 0;

    LogPrint("wagerr", "Start generating field bets payouts...\n");

    for (auto result : results) {

        if (result.nResultType == ResultType::eventClosed)
            continue;

        LogPrint("wagerr", "Looking for bets of eventId: %lu\n", result.nEventId);

        // look bets during the bet interval
        uint32_t startHeight = GetBetSearchStartHeight(nLastBlockHeight);
        auto it = bettingsViewCache.fieldBets->NewIterator();
        std::vector<std::pair<FieldBetKey, CFieldBetDB>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(FieldBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {
            bool legHalfLose = false;
            bool legHalfWin = false;
            bool legRefund = false;

            FieldBetKey uniBetKey;
            CFieldBetDB uniBet;
            CBettingDB::BytesToDbType(it->Key(), uniBetKey);
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            // skip if bet is already handled
            if (uniBet.IsCompleted()) continue;

            bool completedBet = false;
            // {onchainOdds, effectiveOdds}
            std::pair<uint32_t, uint32_t> finalOdds{0, 0};

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
                        CFieldLegDB &leg = uniBet.legs[idx];
                        CFieldEventDB &lockedEvent = uniBet.lockedEvents[idx];
                        // skip this bet if incompleted (can't find one result)
                        CFieldResultDB res;
                        if (bettingsViewCache.fieldResults->Read(FieldResultKey{leg.nEventId}, res)) {
                            // {onchainOdds, effectiveOdds}
                            std::pair<uint32_t, uint32_t> betOdds;
                            // if bet placed before 2 mins of event started - refund this bet
                            if (lockedEvent.nStartTime > 0 && uniBet.betTime > ((int64_t)lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                                betOdds = std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
                            }
                            else {
                                betOdds = GetBetOdds(leg, lockedEvent, res);
                            }

                            if (betOdds.first == 0) { }
                            else if (betOdds.first == refundOdds) {
                                legRefund = true;
                            }
                            else if (betOdds.first == refundOdds / 2) {
                                legHalfLose = true;
                            }
                            else if (betOdds.first < GetBetPotentialOdds(leg, lockedEvent)) {
                                legHalfWin = true;
                            }
                            // multiply odds
                            if (firstOddMultiply) {
                                finalOdds.first = betOdds.first;
                                finalOdds.second = betOdds.second;
                                firstOddMultiply = false;
                            }
                            else {
                                finalOdds.first = static_cast<uint32_t>(((uint64_t) finalOdds.first * betOdds.first) / BET_ODDSDIVISOR);
                                finalOdds.second = static_cast<uint32_t>(((uint64_t) finalOdds.second * betOdds.second) / BET_ODDSDIVISOR);
                            }
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
                CFieldLegDB &singleBet = uniBet.legs[0];
                CFieldEventDB &lockedEvent = uniBet.lockedEvents[0];

                if (singleBet.nEventId == result.nEventId) {
                    completedBet = true;

                    // if bet placed before 2 mins of event started - refund this bet
                    if (lockedEvent.nStartTime > 0 && uniBet.betTime > ((int64_t)lockedEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {

                        finalOdds = std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
                    }
                    else {
                        finalOdds = GetBetOdds(singleBet, lockedEvent, result);
                    }

                    if (finalOdds.first == 0) { }
                    else if (finalOdds.first == refundOdds) {
                        legRefund = true;
                    }
                    else if (finalOdds.first == refundOdds / 2) {
                        legHalfLose = true;
                    }
                    else if (finalOdds.first < GetBetPotentialOdds(singleBet, lockedEvent)) {
                        legHalfWin = true;
                    }
                }
            }

            if (completedBet) {
                if (uniBet.betAmount < (Params().MinBetPayoutRange() * COIN) || uniBet.betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                    finalOdds = std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
                }

                CAmount effectivePayout, grossPayout;

                effectivePayout = uniBet.betAmount * finalOdds.second / BET_ODDSDIVISOR;
                grossPayout = uniBet.betAmount * finalOdds.first / BET_ODDSDIVISOR;
                effectivePayoutsSum += effectivePayout;
                grossPayoutsSum += grossPayout;

                if (effectivePayout > 0) {
                    // Add winning payout to the payouts vector.
                    CPayoutInfoDB payoutInfo(uniBetKey, finalOdds.second <= refundOdds ? PayoutType::bettingRefund : PayoutType::bettingPayout);
                    vExpectedPayouts.emplace_back(effectivePayout, GetScriptForDestination(uniBet.playerAddress.Get()), uniBet.betAmount);
                    vPayoutsInfo.emplace_back(payoutInfo);

                    if (effectivePayout < uniBet.betAmount) {
                        uniBet.resultType = BetResultType::betResultPartialLose;
                    }
                    else if (finalOdds.first == refundOdds) {
                        uniBet.resultType = BetResultType::betResultRefund;
                    }
                    else if ((uniBet.legs.size() == 1 && legHalfWin) ||
                            (uniBet.legs.size() > 1 && (legHalfWin || legHalfLose || legRefund))) {
                        uniBet.resultType = BetResultType::betResultPartialWin;
                    }
                    else {
                        uniBet.resultType = BetResultType::betResultWin;
                    }
                    // write payout height: result height + 1
                    uniBet.payoutHeight = (uint32_t) nNewBlockHeight;
                }
                else {
                    uniBet.resultType = BetResultType::betResultLose;
                }
                uniBet.payout = effectivePayout;
                LogPrint("wagerr", "\nField bet %s is handled!\nPlayer address: %s\nFinal onchain odds: %lu, effective odds: %lu\nPayout: %lu\n",
                    uniBetKey.outPoint.ToStringShort(), uniBet.playerAddress.ToString(), finalOdds.first, finalOdds.second, effectivePayout);
                LogPrint("wagerr", "Legs:");
                for (auto &leg : uniBet.legs) {
                    LogPrint("wagerr", " (eventId: %lu, PeerlessBetOutcomeType: %lu, contenderId: %lu)\n", leg.nEventId, leg.nOutcome, leg.nContenderId);
                }
                // if handling bet is completed - mark it
                uniBet.SetCompleted();
                vEntriesToUpdate.emplace_back(std::pair<FieldBetKey, CFieldBetDB>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.fieldBets->Update(pair.first, pair.second);
        }
    }

    // OMNO and Dev rewards
    GetRewardPayoutsV3(nNewBlockHeight, grossPayoutsSum - effectivePayoutsSum, vExpectedPayouts, vPayoutsInfo);

    LogPrint("wagerr", "Finished generating field betting payouts...\n");

}

/**
 * Undo only bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoFieldBetPayouts(CBettingsView &bettingsViewCache, int height)
{
    int nCurrentHeight = chainActive.Height();
    // Get all the results posted in the previous block.
    std::vector<CFieldResultDB> results = GetFieldResults(height - 1);

    LogPrintf("Start undo payouts...\n");

    for (auto result : results) {

        if (result.nResultType == ResultType::eventClosed)
            continue;

        // look bets at last 14 days
        uint32_t startHeight = GetBetSearchStartHeight(nCurrentHeight);

        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<FieldBetKey, CFieldBetDB>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(FieldBetKey{startHeight, COutPoint()})); it->Valid(); it->Next()) {
            FieldBetKey uniBetKey;
            CFieldBetDB uniBet;
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
                        CFieldLegDB &leg = uniBet.legs[idx];
                        // skip this bet if incompleted (can't find one result)
                        CFieldResultDB res;
                        if (!bettingsViewCache.results->Read(ResultKey{leg.nEventId}, res)) {
                            needUndo = false;
                        }
                    }
                }
            }
            // single bet
            else if (uniBet.legs.size() == 1) {
                CFieldLegDB &singleBet = uniBet.legs[0];
                if (singleBet.nEventId == result.nEventId) {
                    needUndo = true;
                }
            }

            if (needUndo) {
                uniBet.SetUncompleted();
                uniBet.resultType = BetResultType::betResultUnknown;
                uniBet.payout = 0;
                vEntriesToUpdate.emplace_back(std::pair<FieldBetKey, CFieldBetDB>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.fieldBets->Update(pair.first, pair.second);
        }
    }
    return true;
}

std::pair<uint32_t, uint32_t> GetHybridBetOdds(const CBettingsView &bettingsViewCache, const CHybridLegDB &leg, const CHybridEventDB &lockedEvent, int64_t betTime, bool& resultFound)
{
    uint64_t refundOdds{BET_ODDSDIVISOR};
    switch ((HybridVariantType) leg.variant.which()) {
        case HybridVariantType::PeerlessVariant:
        {
            const CPeerlessLegDB& plLeg = boost::get<CPeerlessLegDB>(leg.variant);
            const CPeerlessBaseEventDB& plEvent = boost::get<CPeerlessBaseEventDB>(lockedEvent.variant);
            CPeerlessResultDB plResult{};
            if (!bettingsViewCache.results->Read(ResultKey{plLeg.nEventId}, plResult)) {
                resultFound = false;
                return {0, 0};
            }
            if (plEvent.nStartTime > 0 && betTime > ((int64_t)plEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                return std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
            }
            else {
                return GetBetOdds(plLeg, plEvent, plResult, true);
            }
            break;
        }
        case HybridVariantType::FieldVariant:
        {
            const CFieldLegDB& fLeg = boost::get<CFieldLegDB>(leg.variant);
            const CFieldEventDB fEvent = boost::get<CFieldEventDB>(lockedEvent.variant);
            CFieldResultDB fResult{};
            if (!bettingsViewCache.fieldResults->Read(FieldResultKey{fLeg.nEventId}, fResult)) {
                resultFound = false;
                return {0, 0};
            }
            if (fEvent.nStartTime > 0 && betTime > ((int64_t)fEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                return std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
            }
            else {
                return GetBetOdds(fLeg, fEvent, fResult);
            }
            break;
        }
        default:
            std::runtime_error("HybridBet: Undefined leg type");
    }
    return {0, 0};
}

template<class CResultDB>
void ProcessHybridBetResult(CBettingsView &bettingsViewCache, uint32_t nLastBlockHeight, CResultDB result, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo, CAmount& fee)
{
    CAmount effectivePayoutsSum, grossPayoutsSum = effectivePayoutsSum = 0;
    uint64_t refundOdds{BET_ODDSDIVISOR};

    uint32_t startHeight = GetBetSearchStartHeight(nLastBlockHeight);

    LogPrint("wagerr", "Looking for bets of eventId: %lu\n", result.nEventId);

    auto it = bettingsViewCache.hybridBets->NewIterator();
    std::vector<std::pair<HybridBetKey, CHybridBetDB>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(HybridBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {
        bool legHalfLose = false;
        bool legHalfWin = false;
        bool legRefund = false;

        HybridBetKey uniBetKey;
        CHybridBetDB uniBet;
        CBettingDB::BytesToDbType(it->Key(), uniBetKey);
        CBettingDB::BytesToDbType(it->Value(), uniBet);
        // skip if bet is already handled
        if (uniBet.IsCompleted()) continue;

        bool completedBet = false;
        // {onchainOdds, effectiveOdds}
        std::pair<uint32_t, uint32_t> finalOdds{0, 0};
        // parlay bet
        if (uniBet.legs.size() > 1) {
            bool resultFound = false;
            for (const auto& leg : uniBet.legs) {
                switch ((HybridVariantType) leg.variant.which()) {
                    case HybridVariantType::PeerlessVariant:
                    {
                        const CPeerlessLegDB& plLeg = boost::get<CPeerlessLegDB>(leg.variant);
                        if (std::is_same<CResultDB, CPeerlessResultDB>::value && plLeg.nEventId == result.nEventId) {
                            resultFound = true;
                        }
                        break;
                    }
                    case HybridVariantType::FieldVariant:
                    {
                        const CFieldLegDB& fLeg = boost::get<CFieldLegDB>(leg.variant);
                        if (std::is_same<CResultDB, CFieldResultDB>::value && fLeg.nEventId == result.nEventId) {
                            resultFound = true;
                        }
                        break;
                    }
                    default:
                        std::runtime_error("CHybridBetDB: Undefined leg type");
                }
            }
            if (resultFound) {
                // make assumption that parlay is completed and this result is last
                completedBet = true;
                // find all results for all legs
                bool firstOddMultiply = true;
                for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                    CHybridLegDB &leg = uniBet.legs[idx];
                    CHybridEventDB &lockedEvent = uniBet.lockedEvents[idx];

                    std::pair<uint32_t, uint32_t> betOdds = GetHybridBetOdds(bettingsViewCache, leg, lockedEvent, uniBet.betTime, completedBet);

                    // skip this bet if incompleted (can't find one result)
                    if (!completedBet)
                        break;

                    if (betOdds.first == 0) { }
                    else if (betOdds.first == refundOdds) {
                        legRefund = true;
                    }
                    else if (betOdds.first == refundOdds / 2) {
                        legHalfLose = true;
                    }
                    else if (betOdds.first < GetBetPotentialOdds(leg, lockedEvent)) {
                        legHalfWin = true;
                    }
                    // multiply odds
                    if (firstOddMultiply) {
                        finalOdds.first = betOdds.first;
                        finalOdds.second = betOdds.second;
                        firstOddMultiply = false;
                    }
                    else {
                        finalOdds.first = static_cast<uint32_t>(((uint64_t) finalOdds.first * betOdds.first) / BET_ODDSDIVISOR);
                        finalOdds.second = static_cast<uint32_t>(((uint64_t) finalOdds.second * betOdds.second) / BET_ODDSDIVISOR);
                    }
                }
            }
        }
        if (completedBet) {
            if (uniBet.betAmount < (Params().MinBetPayoutRange() * COIN) || uniBet.betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                finalOdds = std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
            }

            CAmount effectivePayout, grossPayout;

            effectivePayout = uniBet.betAmount * finalOdds.second / BET_ODDSDIVISOR;
            grossPayout = uniBet.betAmount * finalOdds.first / BET_ODDSDIVISOR;
            effectivePayoutsSum += effectivePayout;
            grossPayoutsSum += grossPayout;

            if (effectivePayout > 0) {
                // Add winning payout to the payouts vector.
                CPayoutInfoDB payoutInfo(uniBetKey, finalOdds.second <= refundOdds ? PayoutType::bettingRefund : PayoutType::bettingPayout);
                vExpectedPayouts.emplace_back(effectivePayout, GetScriptForDestination(uniBet.playerAddress.Get()), uniBet.betAmount);
                vPayoutsInfo.emplace_back(payoutInfo);

                if (effectivePayout < uniBet.betAmount) {
                    uniBet.resultType = BetResultType::betResultPartialLose;
                }
                else if (finalOdds.first == refundOdds) {
                    uniBet.resultType = BetResultType::betResultRefund;
                }
                else if ((uniBet.legs.size() == 1 && legHalfWin) ||
                        (uniBet.legs.size() > 1 && (legHalfWin || legHalfLose || legRefund))) {
                    uniBet.resultType = BetResultType::betResultPartialWin;
                }
                else {
                    uniBet.resultType = BetResultType::betResultWin;
                }
                // write payout height: result height + 1
                uniBet.payoutHeight = (uint32_t) nLastBlockHeight + 1;
            }
            else {
                uniBet.resultType = BetResultType::betResultLose;
            }
            uniBet.payout = effectivePayout;
            LogPrint("wagerr", "\nField bet %s is handled!\nPlayer address: %s\nFinal onchain odds: %lu, effective odds: %lu\nPayout: %lu\n",
                uniBetKey.outPoint.ToStringShort(), uniBet.playerAddress.ToString(), finalOdds.first, finalOdds.second, effectivePayout);
            // if handling bet is completed - mark it
            uniBet.SetCompleted();
            vEntriesToUpdate.emplace_back(std::pair<HybridBetKey, CHybridBetDB>{uniBetKey, uniBet});
        }
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.hybridBets->Update(pair.first, pair.second);
    }

    fee += grossPayoutsSum - effectivePayoutsSum;
}

void GetHybridBetPayoutsV4(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    bool fWagerrProtocolV4 = nLastBlockHeight >= Params().WagerrProtocolV4StartHeight();

    if (!fWagerrProtocolV4)
        return;

    LogPrint("wagerr", "Start generating hybrid bets payouts...\n");

    CAmount fee = 0;

    // process bets for peerless results
    for (auto& result : GetPLResults(nLastBlockHeight)) {
        if (result.nResultType == ResultType::eventClosed)
            continue;
        ProcessHybridBetResult(bettingsViewCache, nLastBlockHeight, result, vExpectedPayouts, vPayoutsInfo, fee);
    }

    // process bets for field results
    for (auto& result : GetFieldResults(nLastBlockHeight)) {
        if (result.nResultType == ResultType::eventClosed)
            continue;
        ProcessHybridBetResult(bettingsViewCache, nLastBlockHeight, result, vExpectedPayouts, vPayoutsInfo, fee);
    }

    GetRewardPayoutsV3(nNewBlockHeight, fee, vExpectedPayouts, vPayoutsInfo);

    LogPrint("wagerr", "Finished generating hybrid betting payouts...\n");
}

template<class CResultDB>
void UndoHybridBetResult(CBettingsView &bettingsViewCache, uint32_t nLastBlockHeight, CResultDB result)
{
    uint32_t startHeight = GetBetSearchStartHeight(nLastBlockHeight);

    LogPrint("wagerr", "Looking for bets of eventId: %lu\n", result.nEventId);

    auto it = bettingsViewCache.hybridBets->NewIterator();
    std::vector<std::pair<HybridBetKey, CHybridBetDB>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(HybridBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {

        HybridBetKey uniBetKey;
        CHybridBetDB uniBet;
        CBettingDB::BytesToDbType(it->Key(), uniBetKey);
        CBettingDB::BytesToDbType(it->Value(), uniBet);
        // skip if bet is already handled
        if (uniBet.IsCompleted()) continue;

        bool completedBet = false;
        // {onchainOdds, effectiveOdds}
        std::pair<uint32_t, uint32_t> finalOdds{0, 0};
        // parlay bet
        if (uniBet.legs.size() > 1) {
            bool resultFound = false;
            for (const auto& leg : uniBet.legs) {
                switch ((HybridVariantType) leg.variant.which()) {
                    case HybridVariantType::PeerlessVariant:
                    {
                        const CPeerlessLegDB& plLeg = boost::get<CPeerlessLegDB>(leg.variant);
                        if (std::is_same<CResultDB, CPeerlessResultDB>::value && plLeg.nEventId == result.nEventId) {
                            resultFound = true;
                        }
                        break;
                    }
                    case HybridVariantType::FieldVariant:
                    {
                        const CFieldLegDB& fLeg = boost::get<CFieldLegDB>(leg.variant);
                        if (std::is_same<CResultDB, CFieldResultDB>::value && fLeg.nEventId == result.nEventId) {
                            resultFound = true;
                        }
                        break;
                    }
                    default:
                        std::runtime_error("CHybridBetDB: Undefined leg type");
                }
            }
            if (resultFound) {
                // make assumption that parlay is completed and this result is last
                completedBet = true;
                // find all results for all legs
                for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                    CHybridLegDB &leg = uniBet.legs[idx];
                    CHybridEventDB &lockedEvent = uniBet.lockedEvents[idx];

                    GetHybridBetOdds(bettingsViewCache, leg, lockedEvent, uniBet.betTime, completedBet);

                    // skip this bet if incompleted (can't find one result)
                    if (!completedBet)
                        break;
                }
            }
        }
        if (completedBet) {
            uniBet.resultType = BetResultType::betResultUnknown;
            uniBet.payout = 0;
            uniBet.SetUncompleted();
            vEntriesToUpdate.emplace_back(std::pair<HybridBetKey, CHybridBetDB>{uniBetKey, uniBet});
        }
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.fieldBets->Update(pair.first, pair.second);
    }
}

bool UndoHybridBetPayoutsV4(CBettingsView &bettingsViewCache, const int height)
{

    int nCurrentHeight = chainActive.Height();

    LogPrint("wagerr", "Start undo hybrid payouts...\n");

    // process bets for peerless results
    for (auto& result : GetPLResults(height - 1)) {
        if (result.nResultType == ResultType::eventClosed)
            continue;
        UndoHybridBetResult(bettingsViewCache, nCurrentHeight, result);
    }

    // process bets for field results
    for (auto& result : GetFieldResults(height - 1)) {
        if (result.nResultType == ResultType::eventClosed)
            continue;
        UndoHybridBetResult(bettingsViewCache, nCurrentHeight, result);
    }

    return true;
}