// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionrecord.h"

#include "base58.h"
#include "obfuscation.h"
#include "swifttx.h"
#include "timedata.h"
#include "wallet.h"
#include "zwgrchain.h"

#include <stdint.h>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/hex.hpp>

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

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet* wallet, const CWalletTx& wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.GetComputedTxTime();
    CAmount nCredit = wtx.GetCredit(ISMINE_ALL);
    CAmount nDebit = wtx.GetDebit(ISMINE_ALL);
    CAmount nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;
    bool fZSpendFromMe = false;

    if (wtx.IsZerocoinSpend()) {
        // a zerocoin spend that was created by this wallet
        libzerocoin::CoinSpend zcspend = TxInToZerocoinSpend(wtx.vin[0]);
        fZSpendFromMe = wallet->IsMyZerocoinSpend(zcspend.getCoinSerialNumber());
    }

    if (wtx.IsCoinStake()) {
        TransactionRecord sub(hash, nTime);
        CTxDestination address;
        if (!wtx.IsZerocoinSpend() && !ExtractDestination(wtx.vout[1].scriptPubKey, address))
            return parts;

        if (wtx.IsZerocoinSpend() && (fZSpendFromMe || wallet->zwgrTracker->HasMintTx(hash))) {
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
            }
            // Bet Winning payout.
            else{
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

        parts.append(sub);
    } else if (wtx.IsZerocoinSpend()) {
        //zerocoin spend outputs
        bool fFeeAssigned = false;
        for (const CTxOut txout : wtx.vout) {
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
                parts.append(sub);
                continue;
            }

            string strAddress = "";
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
                parts.append(sub);
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
            parts.append(sub);
        }
    } else if (nNet > 0 || wtx.IsCoinBase()) {
        //
        // Credit
        //
        BOOST_FOREACH (const CTxOut& txout, wtx.vout) {
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

                parts.append(sub);
            }
        }
    } else {
        bool fAllFromMeDenom = true;
        int nFromMe = 0;
        bool involvesWatchAddress = false;
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        BOOST_FOREACH (const CTxIn& txin, wtx.vin) {
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
        BOOST_FOREACH (const CTxOut& txout, wtx.vout) {
            if (wallet->IsMine(txout)) {
                fAllToMeDenom = fAllToMeDenom && wallet->IsDenominatedAmount(txout.nValue);
                nToMe++;
            }
            isminetype mine = wallet->IsMine(txout);
            if (mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if (fAllToMe > mine) fAllToMe = mine;
        }

        if (fAllFromMeDenom && fAllToMeDenom && nFromMe && nToMe) {
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::ObfuscationDenominate, "", -nDebit, nCredit));
            parts.last().involvesWatchAddress = false; // maybe pass to TransactionRecord as constructor argument
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
            parts.append(sub);
            parts.last().involvesWatchAddress = involvesWatchAddress; // maybe pass to TransactionRecord as constructor argument
        } else if (fAllFromMe) {
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
                    if (wtx.IsZerocoinMint())
                        continue;
                    // Sent to Wagerr Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                } else if (txout.IsZerocoinMint()){
                    sub.type = TransactionRecord::ZerocoinMint;
                    sub.address = mapValue["zerocoinmint"];
                    sub.credit += txout.nValue;
                } else {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
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

                parts.append(sub);
            }
        } else {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
            parts.last().involvesWatchAddress = involvesWatchAddress;
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
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived,
        idx);
    //status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
    status.depth = wtx.GetDepthInMainChain();

    //Determine the depth of the block
    int nBlocksToMaturity = wtx.GetBlocksToMaturity();

    status.countsForBalance = wtx.IsTrusted() && !(nBlocksToMaturity > 0);
    status.cur_num_blocks = chainActive.Height();
    status.cur_num_ix_locks = nCompleteTXLocks;

    if (!IsFinalTx(wtx, chainActive.Height() + 1)) {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD) {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - chainActive.Height();
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

            if (pindex && chainActive.Contains(pindex)) {
                // Check if the block was requested by anyone
                if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.status = TransactionStatus::MaturesWarning;
            } else {
                status.status = TransactionStatus::NotAccepted;
            }
        } else {
            status.status = TransactionStatus::Confirmed;
            status.matures_in = 0;
        }
    } else {
        if (status.depth < 0) {
            status.status = TransactionStatus::Conflicted;
        } else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0) {
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

QString TransactionRecord::getTxID() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
