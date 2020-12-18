// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionrecord.h"

#include "base58.h"
#include "obfuscation.h"
#include "swifttx.h"
#include "timedata.h"
#include "wallet/wallet.h"
#include "zwgrchain.h"
#include "main.h"
#include "betting/bet_tx.h"

#include <algorithm>
#include <stdint.h>

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx& wtx)
{
    if (wtx.IsCoinBase()) {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain()) {
            return false;
        }
    }
    return true;
}

bool DecomposeBettingCoinstake(const CWallet* wallet, const CWalletTx& wtx, const CTxDestination address, bool fMyZMint, std::vector<TransactionRecord> &coinStakeRecords) {
    std::map<uint64_t, CTxOut> stakeRewards;
    std::map<uint64_t, CTxOut> betRewards;
    std::map<uint64_t, CTxOut> MNRewards;

    // No betting outputs, so the default decompose function suffices
    if (wtx.vout.size() < 3) return false;

    const CBlockIndex* pindexWtx;
    const int nStakeDepth = wtx.GetDepthInMainChain(pindexWtx, false);

    // When no coinstake value can be determined, the default decompose function suffices
    if (nStakeDepth <= 0) return false;

    const int nStakeHeight = pindexWtx->nHeight;
    const CAmount nStakeValue = GetBlockValue(nStakeHeight);

    // ** TODO ** refactoring check
    //const CAmount nMNExpectedRewardValue = GetMasternodePayment(nStakeHeight, nStakeValue, 1, wtx.IsZerocoinSpend());
    const CAmount nMNExpectedRewardValue = GetMasternodePayment(nStakeHeight, nStakeValue, 1, wtx.GetZerocoinSpent());


    int nMNIndex = -1;
    CAmount nActualMNValue = 0;

    COutPoint prevout = wtx.vin[0].prevout;

    // When no coinstake value can be determined, the default decompose function suffices
    uint256 hashBlock;
    CTransaction txIn;
    if (!GetTransaction(prevout.hash, txIn, hashBlock, true)) return false;
    if (txIn.vout.size() < prevout.n + 1) return false;
    CAmount nStakeInValue = txIn.vout[prevout.n].nValue;

    if (wtx.vout.size() > 2) {
        CAmount nPotentialMNValue = wtx.vout[wtx.vout.size() - 1].nValue;
        if (nPotentialMNValue == nMNExpectedRewardValue) {
            nMNIndex = wtx.vout.size() - 1;
            nActualMNValue = nPotentialMNValue;
            MNRewards.insert(std::make_pair(nMNIndex, wtx.vout[nMNIndex]));
        }
    }

    bool allStakesFound = false;
    CAmount nStakeValueFound = 0;
    for (int i = 1; i < (int)wtx.vout.size(); i++) {
        if (i == nMNIndex) continue;

        CTxOut curOut = wtx.vout[i];
        if (!allStakesFound) {
            if (nStakeValueFound + nActualMNValue + curOut.nValue <= nStakeInValue + nStakeValue) {
                nStakeValueFound += curOut.nValue;
                stakeRewards.insert(std::make_pair(i, curOut));
            } else {
                betRewards.insert(std::make_pair(i, curOut));
                allStakesFound = true;
            }
        } else {
            betRewards.insert(std::make_pair(i, curOut));
        }
    }

    int64_t nTime = wtx.GetComputedTxTime();
    const uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    TransactionRecord stakeRecord(hash, nTime);
    if (stakeRewards.size() > 0) {
         if (fMyZMint) {
            //zWGR stake reward
            stakeRecord.involvesWatchAddress = false;
            stakeRecord.type = TransactionRecord::StakeZWGR;
            stakeRecord.address = mapValue["zerocoinmint"];
            stakeRecord.credit = 0;
            for (auto stakeReward : stakeRewards) {
                if (stakeReward.second.IsZerocoinMint()){
                    stakeRecord.credit += stakeReward.second.nValue;
                }
            }
            stakeRecord.debit -= wtx.vin[0].nSequence * COIN;
            coinStakeRecords.push_back(stakeRecord);
        } else if (isminetype mine = wallet->IsMine(wtx.vout[1])){
            //WGR stake reward
            stakeRecord.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            stakeRecord.type = TransactionRecord::StakeMint;
            stakeRecord.address = CBitcoinAddress(address).ToString();
            for (auto stakeReward : stakeRewards) {
                stakeRecord.credit += wallet->GetCredit(stakeReward.second, ISMINE_ALL);
            }
            stakeRecord.credit -= wtx.GetDebit(ISMINE_ALL);
            coinStakeRecords.push_back(stakeRecord);
        }
    }

    std::vector<TransactionRecord> betRecords;
    if (betRewards.size() > 0) {
        for (auto betReward : betRewards) {
            CTxDestination destBetWin;
            if (ExtractDestination(betReward.second.scriptPubKey, destBetWin) && IsMine(*wallet, destBetWin)) {
                TransactionRecord betRecord(hash, nTime);
                isminetype mine = wallet->IsMine(betReward.second);
                betRecord.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                betRecord.type = TransactionRecord::BetWin;
                betRecord.address = CBitcoinAddress(destBetWin).ToString();
                betRecord.credit = betReward.second.nValue;
                coinStakeRecords.push_back(betRecord);
            }
        }
    }

    TransactionRecord MNRecord(hash, nTime);
    if (MNRewards.size() > 0) {
        auto MNReward = MNRewards.begin();
        // Masternode reward
        CTxDestination destMN;
        if (ExtractDestination(MNReward->second.scriptPubKey, destMN) && IsMine(*wallet, destMN)) {
            isminetype mine = wallet->IsMine(MNReward->second);
            MNRecord.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            MNRecord.type = TransactionRecord::MNReward;
            MNRecord.address = CBitcoinAddress(destMN).ToString();
            MNRecord.credit = MNReward->second.nValue;
            coinStakeRecords.push_back(MNRecord);
        }
    }

    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
std::vector<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet* wallet, const CWalletTx& wtx)
{
    std::vector<TransactionRecord> parts;
    int64_t nTime = wtx.GetComputedTxTime();
    CAmount nCredit = wtx.GetCredit(ISMINE_ALL);
    CAmount nDebit = wtx.GetDebit(ISMINE_ALL);
    CAmount nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;
    bool fZSpendFromMe = false;

    if (wtx.HasZerocoinSpendInputs()) {
        libzerocoin::CoinSpend zcspend = wtx.HasZerocoinPublicSpendInputs() ? ZWGRModule::parseCoinSpend(wtx.vin[0]) : TxInToZerocoinSpend(wtx.vin[0]);
        fZSpendFromMe = wallet->IsMyZerocoinSpend(zcspend.getCoinSerialNumber());
    }

    if (wtx.IsCoinStake()) {
        TransactionRecord sub(hash, nTime);
        CTxDestination address;
        if (!wtx.HasZerocoinSpendInputs() && !ExtractDestination(wtx.vout[1].scriptPubKey, address))
            return parts;

        if (DecomposeBettingCoinstake(wallet, wtx, address, fZSpendFromMe || wallet->zwgrTracker->HasMintTx(hash), parts)) {
            return parts;
        }

        if (wtx.HasZerocoinSpendInputs() && (fZSpendFromMe || wallet->zwgrTracker->HasMintTx(hash))) {
            //zWGR stake reward
            sub.involvesWatchAddress = false;
            sub.type = TransactionRecord::StakeZWGR;
            sub.address = mapValue["zerocoinmint"];
            sub.credit = 0;
            for (const CTxOut& out : wtx.vout) {
                if (out.IsZerocoinMint())
                    sub.credit += out.nValue;
            }
            sub.debit -= wtx.vin[0].nSequence * COIN;
        } else if (isminetype mine = wallet->IsMine(wtx.vout[1])) {
            // WGR stake reward
            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            sub.type = TransactionRecord::StakeMint;
            sub.address = CBitcoinAddress(address).ToString();
            sub.credit = nNet;
        } else {
            // Masternode reward
            CTxDestination destMN;
            int nIndexMN = wtx.vout.size() - 1;
            if (ExtractDestination(wtx.vout[nIndexMN].scriptPubKey, destMN) && IsMine(*wallet, destMN)) {
                isminetype mine = wallet->IsMine(wtx.vout[nIndexMN]);
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                sub.type = TransactionRecord::MNReward;
                sub.address = CBitcoinAddress(destMN).ToString();
                sub.credit = wtx.vout[nIndexMN].nValue;
            } else {
                // Bet winning payout
                CTxDestination destBetWin;
                for( unsigned int i = 0; i < wtx.vout.size(); i++  ){
                    if (ExtractDestination(wtx.vout[i].scriptPubKey, destBetWin) && IsMine(*wallet, destBetWin)) {
                        isminetype mine = wallet->IsMine(wtx.vout[i]);
                        sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                        sub.type = TransactionRecord::BetWin;
                        sub.address = CBitcoinAddress(destBetWin).ToString();
                        sub.credit = wtx.vout[i].nValue;
                    }
                }
            }
        }

        parts.push_back(sub);
    } else if (wtx.HasZerocoinSpendInputs()) {
        //zerocoin spend outputs
        bool fFeeAssigned = false;
        for (const CTxOut& txout : wtx.vout) {
            // change that was reminted as zerocoins
            if (txout.IsZerocoinMint()) {
                // do not display record if this isn't from our wallet
                if (!fZSpendFromMe)
                    continue;

                isminetype mine = wallet->IsMine(txout);
                TransactionRecord sub(hash, nTime);
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                sub.type = TransactionRecord::ZerocoinSpend_Change_zWgr;
                sub.address = mapValue["zerocoinmint"];
                if (!fFeeAssigned) {
                    sub.debit -= (wtx.GetZerocoinSpent() - wtx.GetValueOut());
                    fFeeAssigned = true;
                }
                sub.idx = parts.size();
                parts.push_back(sub);
                continue;
            }

            std::string strAddress = "";
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address))
                strAddress = CBitcoinAddress(address).ToString();

            // a zerocoinspend that was sent to an address held by this wallet
            isminetype mine = wallet->IsMine(txout);
            if (mine) {
                TransactionRecord sub(hash, nTime);
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                if (fZSpendFromMe) {
                    sub.type = TransactionRecord::ZerocoinSpend_FromMe;
                } else {
                    sub.type = TransactionRecord::RecvFromZerocoinSpend;
                    sub.credit = txout.nValue;
                }
                sub.address = mapValue["recvzerocoinspend"];
                if (strAddress != "")
                    sub.address = strAddress;
                sub.idx = parts.size();
                parts.push_back(sub);
                continue;
            }

            // spend is not from us, so do not display the spend side of the record
            if (!fZSpendFromMe)
                continue;

            // zerocoin spend that was sent to someone else
            TransactionRecord sub(hash, nTime);
            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            sub.debit = -txout.nValue;
            sub.type = TransactionRecord::ZerocoinSpend;
            sub.address = mapValue["zerocoinspend"];
            if (strAddress != "")
                sub.address = strAddress;
            sub.idx = parts.size();
            parts.push_back(sub);
        }
    } else if (nNet > 0 || wtx.IsCoinBase()) {
        //
        // Credit
        //
        for (const CTxOut& txout : wtx.vout) {
            isminetype mine = wallet->IsMine(txout);
            if (mine) {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number
                sub.credit = txout.nValue;
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address)) {
                    // Received by Wagerr Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                } else {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.IsCoinBase()) {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.push_back(sub);
            }
        }
    } else {
        bool fAllFromMeDenom = true;
        int nFromMe = 0;
        bool involvesWatchAddress = false;
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        for (const CTxIn& txin : wtx.vin) {
            if (wallet->IsMine(txin)) {
                fAllFromMeDenom = fAllFromMeDenom && wallet->IsDenominated(txin);
                nFromMe++;
            }
            isminetype mine = wallet->IsMine(txin);
            if (mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if (fAllFromMe > mine) fAllFromMe = mine;
        }

        isminetype fAllToMe = ISMINE_SPENDABLE;
        bool fAllToMeDenom = true;
        int nToMe = 0;
        for (const CTxOut& txout : wtx.vout) {
            if (wallet->IsMine(txout)) {
                fAllToMeDenom = fAllToMeDenom && wallet->IsDenominatedAmount(txout.nValue);
                nToMe++;
            }
            isminetype mine = wallet->IsMine(txout);
            if (mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if (fAllToMe > mine) fAllToMe = mine;
        }

        if (fAllFromMeDenom && fAllToMeDenom && nFromMe * nToMe) {
            parts.push_back(TransactionRecord(hash, nTime, TransactionRecord::ObfuscationDenominate, "", -nDebit, nCredit));
            parts.back().involvesWatchAddress = false; // maybe pass to TransactionRecord as constructor argument
        } else if (fAllFromMe && fAllToMe) {
            // Payment to self
            // TODO: this section still not accurate but covers most cases,
            // might need some additional work however

            TransactionRecord sub(hash, nTime);
            // Payment to self by default
            sub.type = TransactionRecord::SendToSelf;
            sub.address = "";

            if (mapValue["DS"] == "1") {
                sub.type = TransactionRecord::Obfuscated;
                CTxDestination address;
                if (ExtractDestination(wtx.vout[0].scriptPubKey, address)) {
                    // Sent to Wagerr Address
                    sub.address = CBitcoinAddress(address).ToString();
                } else {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.address = mapValue["to"];
                }
            } else {
                for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
                    const CTxOut& txout = wtx.vout[nOut];
                    sub.idx = parts.size();

                    if (wallet->IsCollateralAmount(txout.nValue)) sub.type = TransactionRecord::ObfuscationMakeCollaterals;
                    if (wallet->IsDenominatedAmount(txout.nValue)) sub.type = TransactionRecord::ObfuscationCreateDenominations;
                    if (nDebit - wtx.GetValueOut() == OBFUSCATION_COLLATERAL) sub.type = TransactionRecord::ObfuscationCollateralPayment;
                }
            }

            CAmount nChange = wtx.GetChange();

            sub.debit = -(nDebit - nChange);
            sub.credit = nCredit - nChange;
            parts.push_back(sub);
            parts.back().involvesWatchAddress = involvesWatchAddress; // maybe pass to TransactionRecord as constructor argument
        } else if (fAllFromMe || wtx.HasZerocoinMintOutputs()) {
            //
            // Debit
            //
            CAmount nTxFee = nDebit - wtx.GetValueOut();

            for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
                const CTxOut& txout = wtx.vout[nOut];
                TransactionRecord sub(hash, nTime);
                sub.idx = parts.size();
                sub.involvesWatchAddress = involvesWatchAddress;

                if (wallet->IsMine(txout)) {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address)) {
                    //This is most likely only going to happen when resyncing deterministic wallet without the knowledge of the
                    //private keys that the change was sent to. Do not display a "sent to" here.
                    if (wtx.HasZerocoinMintOutputs())
                        continue;
                    // Sent to Wagerr Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                } else if (txout.IsZerocoinMint()){
                    sub.type = TransactionRecord::ZerocoinMint;
                    sub.address = mapValue["zerocoinmint"];
                    sub.credit += txout.nValue;
                } else {
                    bool isBettingEntry = false;
                    bool isChainGameEntry = false;
                    if (txout.scriptPubKey.IsUnspendable()) {

                        auto bettingTx = ParseBettingTx(txout);

                        if (bettingTx) {
                            switch(bettingTx->GetTxType()) {
                                case plBetTxType:
                                case plParlayBetTxType:
                                    isBettingEntry = true;
                                    break;
                                case cgBetTxType:
                                    isChainGameEntry = true;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    if (isBettingEntry) {
                        // Placed a bet
                        sub.type = TransactionRecord::BetPlaced;
                        sub.address = mapValue["to"];
                    } else if (isChainGameEntry) {
                        // Placed a bet
                        sub.type = TransactionRecord::ChainGameEntry;
                        sub.address = mapValue["to"];
                    } else {
                        // Sent to IP, or other non-address transaction like OP_EVAL
                        sub.type = TransactionRecord::SendToOther;
                        sub.address = mapValue["to"];
                    }
                }

                if (mapValue["DS"] == "1") {
                    sub.type = TransactionRecord::Obfuscated;
                }

                CAmount nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0) {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.push_back(sub);
            }
        } else {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.push_back(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
            parts.back().involvesWatchAddress = involvesWatchAddress;
        }
    }

    return parts;
}

bool IsZWGRType(TransactionRecord::Type type)
{
    switch (type) {
        case TransactionRecord::StakeZWGR:
        case TransactionRecord::ZerocoinMint:
        case TransactionRecord::ZerocoinSpend:
        case TransactionRecord::RecvFromZerocoinSpend:
        case TransactionRecord::ZerocoinSpend_Change_zWgr:
        case TransactionRecord::ZerocoinSpend_FromMe:
            return true;
        default:
            return false;
    }
}

void TransactionRecord::updateStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);
    int chainHeight = chainActive.Height();

    CBlockIndex *pindex = nullptr;
    // Find the block the tx is in
    BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Determine transaction status

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived,
        idx);

    bool fConflicted = false;
    int depth = 0;
    bool isTrusted = wtx.IsTrusted(depth, fConflicted);
    const bool isOffline = (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0);
    int nBlocksToMaturity = (wtx.IsCoinBase() || wtx.IsCoinStake()) ? std::max(0, (Params().COINBASE_MATURITY(chainHeight) + 1) - depth) : 0;

    status.countsForBalance = isTrusted && !(nBlocksToMaturity > 0);
    status.cur_num_blocks = chainHeight;
    status.depth = depth;
    status.cur_num_ix_locks = nCompleteTXLocks;

    if (!IsFinalTx(wtx, chainHeight + 1)) {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD) {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - chainHeight;
        } else {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else if (type == TransactionRecord::Generated || type == TransactionRecord::StakeMint || type == TransactionRecord::StakeZWGR || type == TransactionRecord::MNReward) {
        if (nBlocksToMaturity > 0) {
            status.status = TransactionStatus::Immature;
            status.matures_in = nBlocksToMaturity;

            if (status.depth >= 0 && !fConflicted) {
                // Check if the block was requested by anyone
                if (isOffline)
                    status.status = TransactionStatus::MaturesWarning;
            } else {
                status.status = TransactionStatus::NotAccepted;
            }
        } else {
            status.status = TransactionStatus::Confirmed;
            status.matures_in = 0;
        }
    } else {
        if (status.depth < 0 || fConflicted) {
            status.status = TransactionStatus::Conflicted;
        } else if (isOffline) {
            status.status = TransactionStatus::Offline;
        } else if (status.depth == 0) {
            status.status = TransactionStatus::Unconfirmed;
        } else if (status.depth < RecommendedNumConfirmations) {
            status.status = TransactionStatus::Confirming;
        } else {
            status.status = TransactionStatus::Confirmed;
        }
    }
}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height() || status.cur_num_ix_locks != nCompleteTXLocks;
}

std::string TransactionRecord::getTxID() const
{
    return hash.ToString();
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}

std::string TransactionRecord::GetTransactionRecordType() const
{
    return GetTransactionRecordType(type);
}
std::string TransactionRecord::GetTransactionRecordType(Type type) const
{
    switch (type)
    {
        case Other: return "Other";
        case BetWin: return "BetPayout";
        case BetPlaced: return "BetPlaced";
        case ChainGameEntry: return "ChainGameEntry";
        case Generated: return "Generated";
        case StakeMint: return "StakeMint";
        case StakeZWGR: return "StakeZWGR";
        case SendToAddress: return "SendToAddress";
        case SendToOther: return "SendToOther";
        case RecvWithAddress: return "RecvWithAddress";
        case MNReward: return "MNReward";
        case RecvFromOther: return "RecvFromOther";
        case SendToSelf: return "SendToSelf";
        case ZerocoinMint: return "ZerocoinMint";
        case ZerocoinSpend: return "ZerocoinSpend";
        case RecvFromZerocoinSpend: return "RecvFromZerocoinSpend";
        case ZerocoinSpend_Change_zWgr: return "ZerocoinSpend_Change_zWgr";
        case ZerocoinSpend_FromMe: return "ZerocoinSpend_FromMe";
        case RecvWithObfuscation: return "RecvWithObfuscation";
        case ObfuscationDenominate: return "ObfuscationDenominate";
        case ObfuscationCollateralPayment: return "ObfuscationCollateralPayment";
        case ObfuscationMakeCollaterals: return "ObfuscationMakeCollaterals";
        case ObfuscationCreateDenominations: return "ObfuscationCreateDenominations";
        case Obfuscated: return "Obfuscated";
    }
    return NULL;
}

std::string TransactionRecord::GetTransactionStatus() const
{
    return GetTransactionStatus(status.status);
}
std::string TransactionRecord::GetTransactionStatus(TransactionStatus::Status status) const
{
    switch (status)
    {
        case TransactionStatus::Confirmed: return "Confirmed";           /**< Have 6 or more confirmations (normal tx) or fully mature (mined tx) **/
            /// Normal (sent/received) transactions
        case TransactionStatus::OpenUntilDate: return "OpenUntilDate";   /**< Transaction not yet final, waiting for date */
        case TransactionStatus::OpenUntilBlock: return "OpenUntilBlock"; /**< Transaction not yet final, waiting for block */
        case TransactionStatus::Offline: return "Offline";               /**< Not sent to any other nodes **/
        case TransactionStatus::Unconfirmed: return "Unconfirmed";       /**< Not yet mined into a block **/
        case TransactionStatus::Confirming: return "Confirmed";          /**< Confirmed, but waiting for the recommended number of confirmations **/
        case TransactionStatus::Conflicted: return "Conflicted";         /**< Conflicts with other transaction or mempool **/
            /// Generated (mined) transactions
        case TransactionStatus::Immature: return "Immature";             /**< Mined but waiting for maturity */
        case TransactionStatus::MaturesWarning: return "MaturesWarning"; /**< Transaction will likely not mature because no nodes have confirmed */
        case TransactionStatus::NotAccepted: return "NotAccepted";       /**< Mined but not accepted */
    }
    return NULL;
}
