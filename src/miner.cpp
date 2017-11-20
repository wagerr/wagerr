// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "hash.h"
#include "main.h"
#include "masternode-sync.h"
#include "net.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif
#include "validationinterface.h"
#include "masternode-payments.h"
#include "accumulators.h"
#include "blocksignature.h"
#include "spork.h"
#include "invalid.h"
#include "zwgrchain.h"


#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/hex.hpp>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// WagerrMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
int64_t nLastCoinStakeSearchInterval = 0;
int nBettingStartBlock = 35000;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) {}

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee) {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        } else {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (Params().AllowMinDifficultyBlocks())
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
}

std::pair<int, std::pair<uint256, uint256> > pCheckpointCache;
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake)
{
    CReserveKey reservekey(pwallet);

    // Create new block
    unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get())
        return NULL;
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience

    // Make sure to create the correct block version after zerocoin is enabled
    bool fZerocoinActive = GetAdjustedTime() >= Params().Zerocoin_StartTime();
    pblock->nVersion = 5;   // Supports CLTV activation

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Make sure to create the correct block version after zerocoin is enabled
    if (fZerocoinActive)
        pblock->nVersion = 4;
    else
        pblock->nVersion = 3;

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;
    pblock->vtx.push_back(txNew);
    pblocktemplate->vTxFees.push_back(-1);   // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // ppcoin: if coinstake available add coinstake tx
    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime(); // only initialized at startup

    CMutableTransaction txCoinStake;
    std::unique_ptr<CStakeInput> stakeInput;
    
    if (fProofOfStake) {
        boost::this_thread::interruption_point();
        pblock->nTime = GetAdjustedTime();
        CBlockIndex* pindexPrev = chainActive.Tip();
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
        int64_t nSearchTime = pblock->nTime; // search to current time
        bool fStakeFound = false;
        if (nSearchTime >= nLastCoinStakeSearchTime) {
            unsigned int nTxNewTime = 0;
            if (pwallet->FindCoinStake(*pwallet, pblock->nBits, nSearchTime - nLastCoinStakeSearchTime, txCoinStake, nTxNewTime, stakeInput)) {
                pblock->nTime = nTxNewTime;
                pblock->vtx[0].vout[0].SetEmpty();
                pblock->vtx.push_back(CTransaction(txCoinStake));
                pblocktemplate->vTxFees.push_back(0);   // updated at end
                pblocktemplate->vTxSigOps.push_back(-1); // updated at end
                fStakeFound = true;
            }
            nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
            nLastCoinStakeSearchTime = nSearchTime;
        }

        if (!fStakeFound)
            return NULL;
    }

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    unsigned int nBlockMaxSizeNetwork = MAX_BLOCK_SIZE_CURRENT;
    nBlockMaxSize = std::max((unsigned int)1000, std::min((nBlockMaxSizeNetwork - 1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);

        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi) {
            const CTransaction& tx = mi->second.GetTx();
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight)){
                continue;
            }
            if(GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE) && tx.ContainsZerocoins()){
                continue;
            }

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            uint256 txid = tx.GetHash();
            for (const CTxIn& txin : tx.vin) {
                //zerocoinspend has special vin
                if (tx.IsZerocoinSpend()) {
                    nTotalIn = tx.GetZerocoinSpent();

                    //Give a high priority to zerocoinspends to get into the next block
                    //Priority = (age^6+100000)*amount - gives higher priority to zwgrs that have been in mempool long
                    //and higher priority to zwgrs that are large in value
                    int64_t nTimeSeen = GetAdjustedTime();
                    double nConfs = 100000;

                    auto it = mapZerocoinspends.find(txid);
                    if (it != mapZerocoinspends.end()) {
                        nTimeSeen = it->second;
                    } else {
                        //for some reason not in map, add it
                        mapZerocoinspends[txid] = nTimeSeen;
                    }

                    double nTimePriority = std::pow(GetAdjustedTime() - nTimeSeen, 6);

                    // zWGR spends can have very large priority, use non-overflowing safe functions
                    dPriority = double_safe_addition(dPriority, (nTimePriority * nConfs));
                    dPriority = double_safe_multiplication(dPriority, nTotalIn);

                    continue;
                }

                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash)) {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash)) {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan) {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }

                //Check for invalid/fraudulent inputs. They shouldn't make it through mempool, but check anyways.
                if (invalid_out::ContainsOutPoint(txin.prevout)) {
                    LogPrintf("%s : found invalid input %s in tx %s", __func__, txin.prevout.ToString(), tx.GetHash().ToString());
                    fMissingInputs = true;
                    break;
                }

                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);

                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coins->nHeight;

                // zWGR spends can have very large priority, use non-overflowing safe functions
                dPriority = double_safe_addition(dPriority, ((double)nValueIn * nConf));

            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn - tx.GetValueOut(), nTxSize);

            if (porphan) {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            } else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        vector<CBigNum> vBlockSerials;
        vector<CBigNum> vTxSerials;
        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nMaxBlockSigOps = MAX_BLOCK_SIGOPS_CURRENT;
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= nMaxBlockSigOps)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (!tx.IsZerocoinSpend() && fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority))) {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            // double check that there are no double spent zWGR spends in this block or tx
            if (tx.IsZerocoinSpend()) {
                int nHeightTx = 0;
                if (IsTransactionInChain(tx.GetHash(), nHeightTx))
                    continue;

                bool fDoubleSerial = false;
                for (const CTxIn txIn : tx.vin) {
                    if (txIn.scriptSig.IsZerocoinSpend()) {
                        libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txIn);
                        bool fUseV1Params = libzerocoin::ExtractVersionFromSerial(spend.getCoinSerialNumber()) < libzerocoin::PrivateCoin::PUBKEY_VERSION;
                        if (!spend.HasValidSerial(Params().Zerocoin_Params(fUseV1Params)))
                            fDoubleSerial = true;
                        if (count(vBlockSerials.begin(), vBlockSerials.end(), spend.getCoinSerialNumber()))
                            fDoubleSerial = true;
                        if (count(vTxSerials.begin(), vTxSerials.end(), spend.getCoinSerialNumber()))
                            fDoubleSerial = true;
                        if (fDoubleSerial)
                            break;
                        vTxSerials.emplace_back(spend.getCoinSerialNumber());
                    }
                }
                //This zWGR serial has already been included in the block, do not add this tx.
                if (fDoubleSerial)
                    continue;
            }

            CAmount nTxFees = view.GetValueIn(tx) - tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= nMaxBlockSigOps)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
                continue;

            CTxUndo txundo;
            UpdateCoins(tx, state, view, txundo, nHeight);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            for (const CBigNum bnSerial : vTxSerials)
                vBlockSerials.emplace_back(bnSerial);

            if (fPrintPriority) {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash)) {
                BOOST_FOREACH (COrphan* porphan, mapDependers[hash]) {
                    if (!porphan->setDependsOn.empty()) {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty()) {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        if (!fProofOfStake) {
            //Masternode and general budget payments
            FillBlockPayee(txNew, nFees, fProofOfStake, false);

            //Make payee
            if (txNew.vout.size() > 1) {
                pblock->payee = txNew.vout[1].scriptPubKey;
            }
        }
        else {

            std::vector<CTxOut> voutPayouts;
            CAmount nMNBetReward = 0;

            if( nHeight > Params().BetStartHeight()) {
                //printf("\nMINER BLOCK: %i \n", nHeight);

                voutPayouts = GetBetPayouts(nHeight - 1);
                GetBlockPayouts(voutPayouts, nMNBetReward);

                /*
                for (unsigned int l = 0; l < voutPayouts.size(); l++) {
                    printf("MINER EXPECTED: %s \n", voutPayouts[l].ToString().c_str());
                }
                */
            }

            // Fill coin stake transaction.
            // pwallet->FillCoinStake(txCoinStake, nMNBetReward, voutPayouts); // Kokary: add betting fee
            if (pwallet->FillCoinStake(*pwallet, txCoinStake, nMNBetReward, voutPayouts, stakeInput)) {
                LogPrintf("%s: filled coin stake tx [%s]\n", __func__, txCoinStake.ToString());
            }
            else {
                LogPrintf("%s: failed to fill coin stake tx\n", __func__);
                return NULL;
            }

            // Sign with updated tx.
            // pwallet->SignCoinStake(txCoinStake, vwtxPrev);
            voutPayouts.clear();
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);

        // Compute final coinbase transaction.
        pblock->vtx[0].vin[0].scriptSig = CScript() << nHeight << OP_0;
        if (!fProofOfStake) {
            pblock->vtx[0] = txNew;
            pblocktemplate->vTxFees[0] = -nFees;
        } else {
            pblock->vtx[1] = CTransaction(txCoinStake);
            pblocktemplate->vTxFees[1] = -nFees;
            pblocktemplate->vTxSigOps[1] = GetLegacySigOpCount(txCoinStake);
        }

        // Fill in header
        pblock->hashPrevBlock = pindexPrev->GetBlockHash();
        if (!fProofOfStake)
            UpdateTime(pblock, pindexPrev);
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
        pblock->nNonce = 0;

        //Calculate the accumulator checkpoint only if the previous cached checkpoint need to be updated
        if (nHeight >= (Params().Zerocoin_StartHeight())) {
            uint256 nCheckpoint;
            uint256 hashBlockLastAccumulated = chainActive[nHeight - (nHeight % 10) - 10]->GetBlockHash();
            if (nHeight >= pCheckpointCache.first || pCheckpointCache.second.first != hashBlockLastAccumulated) {
                //For the period before v2 activation, zWGR will be disabled and previous block's checkpoint is all that will be needed
                pCheckpointCache.second.second = pindexPrev->nAccumulatorCheckpoint;
                if (pindexPrev->nHeight + 1 >= Params().Zerocoin_Block_V2_Start()) {
                    AccumulatorMap mapAccumulators(Params().Zerocoin_Params(false));
                    if (fZerocoinActive && !CalculateAccumulatorCheckpoint(nHeight, nCheckpoint, mapAccumulators)) {
                        LogPrintf("%s: failed to get accumulator checkpoint\n", __func__);
                    } else {
                        // the next time the accumulator checkpoint should be recalculated ( the next height that is multiple of 10)
                        pCheckpointCache.first = nHeight + (10 - (nHeight % 10));

                        // the block hash of the last block used in the accumulator checkpoint calc. This will handle reorg situations.
                        pCheckpointCache.second.first = hashBlockLastAccumulated;
                        pCheckpointCache.second.second = nCheckpoint;
                    }
                }
            }
        }

        pblock->nAccumulatorCheckpoint = pCheckpointCache.second.second;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false)) {
            LogPrintf("CreateNewBlock() : TestBlockValidity failed\n");
            mempool.clear();
            return NULL;
        }

//        if (pblock->IsZerocoinStake()) {
//            CWalletTx wtx(pwalletMain, pblock->vtx[1]);
//            pwalletMain->AddToWallet(wtx);
//        }
    }

    return pblocktemplate.release();
}

/**
 * Check a given block to see if it contains a betting result TX.
 *
 * @return results vector.
 */
std::vector<std::vector<std::string>> getEventResults( int height ) {

    // Set the Oracle wallet address. 
    std::string OracleWalletAddr = Params().OracleWalletAddr();
    std::vector<std::vector<std::string>> results;

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[height];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    BOOST_FOREACH(CTransaction& tx, block.vtx) {

        // Ensure the result TX has been posted by Oracle wallet by looking at the TX vins.
        const CTxIn &txin = tx.vin[0];
        COutPoint prevout = txin.prevout;
        bool validResult  = false;

        uint256 hashBlock;
        CTransaction txPrev;

        if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

            const CTxOut &txout      = txPrev.vout[0];
            std::string scriptPubKey = txout.scriptPubKey.ToString();

            txnouttype type;
            vector<CTxDestination> addrs;
            int nRequired;

            // Check all vouts for Oracle wallet address, if found we know it's a valid result posting.
            if (ExtractDestinations(txout.scriptPubKey, type, addrs, nRequired)) {
                BOOST_FOREACH (const CTxDestination &addr, addrs) {
                    // TODO Take this wallet address as a configuration value.
                    if (CBitcoinAddress(addr).ToString() == OracleWalletAddr) {
                        validResult = true;
                    }
                }
            }
        }

        if( validResult ) {

            // Look for result OP RETURN code in the tx vouts.
            for (unsigned int i = 0; i < tx.vout.size(); i++) {

                const CTxOut &txout = tx.vout[i];
                std::string scriptPubKey = txout.scriptPubKey.ToString();

                // TODO Remove hard-coded values from this block.
                if(scriptPubKey.length() > 0 && strncmp(scriptPubKey.c_str(), "OP_RETURN", 9) == 0) {

                    // Get OP CODE from transactions.
                    vector<unsigned char> v = ParseHex(scriptPubKey.substr(9, string::npos));
                    std::string betDescr(v.begin(), v.end());
                    std::vector<std::string> strs;

                    boost::split(strs, betDescr, boost::is_any_of("|"));

                    // Only look for result transactions.
                    if (strs.size() == 4 && (strs[0] == Params().ResultTxType() || strs[0] == Params().RefundTxType()) ) {

                        LogPrintf("RESULT OP_RETURN -> %s \n", betDescr.c_str());

                        std::vector <string> entry;

                        // Event ID.
                        entry.emplace_back(strs[2].c_str());
                        // Result
                        entry.emplace_back(strs[3].c_str());

                        // Check if betting result TX or a refund TX
                        if( strs[0] == Params().ResultTxType() ){
                            entry.emplace_back( Params().ResultTxType() );
                        }
                        else{
                            entry.emplace_back( Params().RefundTxType() );
                        }

                        results.push_back(entry);
                    }
                }
            }
        }
    }

    return results;
}


// TODO - Marty - Currently function is unused, may be required in future updates.
/**
 * Check the block chain to ensure a result has not been paid out already.
 *
 * @param results
 * @return
 */
std::vector<std::vector<std::string>> checkResults( std::vector<std::vector<std::string>> results){

    int nCurrentHeight = chainActive.Height();

    // Set the number of blocks we want to look back for results already bpaid out.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[nCurrentHeight - Params().BetBlockPayoutAmount()];

    // Check if there is a result already posted for an event in the last x blocks
    while (resultsBocksIndex) {
        CBlock block;
        ReadBlockFromDisk(block, resultsBocksIndex);

        BOOST_FOREACH(CTransaction& tx, block.vtx) {

            for (unsigned int i = 0; i < tx.vout.size(); i++) {

                const CTxOut& txout = tx.vout[i];
                std::string s = txout.scriptPubKey.ToString();

                if (s.length() > 0 && CBitcoinAddress(tx.vout[0].ToString()).ToString() == Params().OracleWalletAddr()) {

                    if (0 == strncmp(s.c_str(), "OP_RETURN", 9)) {

                        vector<unsigned char> v = ParseHex(s.substr(9, string::npos));
                        std::string betDescr(v.begin(), v.end());
                        std::vector<std::string> strs;

                        boost::split(strs, betDescr, boost::is_any_of("|"));

                        //check if the result is the correct result type (3) and size
                        if (strs.size() != 4 || strs[0] != Params().ResultTxType()) {
                            continue;
                        }

                        //loop through and check if the eventid matches a result event id
                        for (unsigned int i = 0; i < results.size(); i++) {
                            if (results[i][0] == strs[2]) {
                                //remove it from the array
                                results.erase(results.begin() + i);
                            }
                        }
                    }
                }
            }
        }

        resultsBocksIndex = chainActive.Next(resultsBocksIndex);
    }

    return results;
}

/**
 * Creates the bet payout vector for all winning bets.
 *
 * @return payout vector.
 */
std::vector<CTxOut> GetBetPayouts( int height ) {

    std::vector<CTxOut> vexpectedPayouts;
    int nCurrentHeight = chainActive.Height();

    // Get all the results posted in the latest block.
    std::vector<std::vector<std::string>> results = getEventResults( height);
    LogPrintf( "Results found: %li \n", results.size() );

    // Set the Oracle wallet address. 
    std::string OracleWalletAddr = Params().OracleWalletAddr();

    // Traverse the blockchain for an event to match a result and all the bets on a result.
    for(unsigned int currResult = 0; currResult < results.size(); currResult++) {

        bool match      = false;
        bool eventFound = false;

        // Look back the chain 14 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;

        BlocksIndex = chainActive[nCurrentHeight - Params().BetBlocksIndexTimespan()];

        unsigned int oddsDivisor    = Params().OddsDivisor();
        unsigned int betXPermille   = Params().BetXPermille();
        unsigned int latestHomeOdds = 0;
        unsigned int latestAwayOdds = 0;
        unsigned int latestDrawOdds = 0;
        time_t eventStart           = 0;
        bool eventStartedFlag       = false;

        std::string latestHomeTeam;
        std::string latestAwayTeam;

        // Traverse the blockchain to find events and bets.
        while (BlocksIndex) {

            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            time_t transactionTime = block.nTime;

            BOOST_FOREACH(CTransaction &tx, block.vtx) {

                // Ensure if event TX that has it been posted by Oracle wallet.
                const CTxIn &txin = tx.vin[0];
                COutPoint prevout = txin.prevout;

                uint256 hashBlock;
                CTransaction txPrev;
                if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

                    const CTxOut &prevTxOut = txPrev.vout[0];
                    std::string scriptPubKey = prevTxOut.scriptPubKey.ToString();

                    txnouttype type;
                    vector<CTxDestination> prevAddrs;
                    int nRequired;

                    if (ExtractDestinations(prevTxOut.scriptPubKey, type, prevAddrs, nRequired)) {
                        BOOST_FOREACH (const CTxDestination &prevAddr, prevAddrs) {
                            // TODO Take this wallet address as a configuration value.
                            if (CBitcoinAddress(prevAddr).ToString() == OracleWalletAddr) {
                                match = true;
                            }
                        }
                    }
                }

                // Check all TX vouts for an OP RETURN.
                for(unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string s       = txout.scriptPubKey.ToString();
                    CAmount betAmount   = txout.nValue;

                    if( match && s.length() > 0 && 0 == strncmp(s.c_str(), "OP_RETURN", 9)) {

                        // Get the OP CODE from the transaction scriptPubKey.
                        vector<unsigned char> v = ParseHex(s.substr(9, string::npos));
                        std::string betDescr(v.begin(), v.end());
                        std::vector<std::string> strs;

                        // Split the OP CODE on |
                        boost::split(strs, betDescr, boost::is_any_of("|"));
                        std::string txType = strs[0].c_str();

                        // Event OP RETURN transaction.
                        if(strs.size() == 11 && txType == Params().EventTxType() ) {

                            // Hold event OP CODE data.
                            std::string pVersion    = strs[1].c_str();
                            std::string eventId     = strs[2].c_str();
                            std::string timestamp   = strs[3].c_str();
                            std::string eventType   = strs[4].c_str();
                            std::string eventInfo   = strs[5].c_str();
                            std::string homeTeam    = strs[6].c_str();
                            std::string awayTeam    = strs[7].c_str();
                            std::string homeWinOdds = strs[8].c_str();
                            std::string awayWinOdds = strs[9].c_str();
                            std::string drawOdds    = strs[10].c_str();

                            CTxDestination address;
                            ExtractDestination(tx.vout[0].scriptPubKey, address);

                            LogPrintf("EVENT OP CODE - %s \n", betDescr.c_str());

                            if ( CBitcoinAddress(address).ToString() == OracleWalletAddr && pVersion == Params().OpCodeProtocolVersion() ) {

                                // If current event ID matches result ID set the teams and odds.
                                if (results[currResult][0] == eventId) {
                                    latestHomeTeam = homeTeam;
                                    latestAwayTeam = awayTeam;

                                    LogPrintf("HomeTeam = %s & AwayTeam = %s \n", latestHomeTeam.c_str(), latestAwayTeam.c_str());

                                    latestHomeOdds = (unsigned int)std::stoi(homeWinOdds);
                                    latestAwayOdds = (unsigned int)std::stoi(awayWinOdds);
                                    latestDrawOdds = (unsigned int)std::stoi(drawOdds);
                                    eventStart     = (time_t) std::strtol(strs[3].c_str(), nullptr, 10);
                                    eventFound     = true;

                                    LogPrintf("latestHomeOdds = %u & latestAwayOdds = %u & latestDrawOdds = %u \n", latestHomeOdds, latestAwayOdds, latestDrawOdds);
                                }
                            }
                        }

                        // Only payout bets that are between 50 - 10000 WRG inclusive (MaxBetPayoutRange).
                        if( betAmount >= (Params().MinBetPayoutRange() * COIN) && betAmount <= (Params().MaxBetPayoutRange() * COIN) ) {

                            // Bet OP RETURN transaction.
                            if (eventFound && strs.size() == 4 && txType == Params().BetTxType()) {
                                CAmount payout = 0 * COIN;

                                std::string pVersion = strs[1];
                                std::string eventId = strs[2];
                                std::string result = strs[3];

                                // If bet was placed less than 20 mins before event start or after event start discard it.
                                if (eventStart > 0 && transactionTime > (eventStart - Params().BetPlaceTimeoutBlocks())) {
                                    eventStartedFlag = true;
                                    break;
                                }

                                // Is the bet a winning bet?
                                if (results[currResult][0] == eventId && results[currResult][1] == result) {
                                    CAmount winnings = 0;

                                    // If bet payout result.
                                    if( results[currResult][2] == Params().ResultTxType() ) {

                                        // Calculate winnings.
                                        if (latestHomeTeam == result) {
                                            winnings = betAmount * latestHomeOdds;
                                        }
                                        else if (latestAwayTeam == result) {
                                            winnings = betAmount * latestAwayOdds;
                                        }
                                        else {
                                            winnings = betAmount * latestDrawOdds;
                                        }

                                        // Calculate the bet winnings for the current bet.
                                        if( winnings > 0) {
                                            payout = (winnings - ((winnings - betAmount*oddsDivisor) * betXPermille / 1000)) / oddsDivisor;
                                        }
                                        else{
                                            payout = 0;
                                        }
                                    }
                                    // Bet refund result.
                                    else{
                                        payout = betAmount;
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

                                    LogPrintf("WINNING PAYOUT :)\n");
                                    LogPrintf("AMOUNT: %li \n", payout);
                                    LogPrintf("ADDRESS: %s \n", CBitcoinAddress( payoutAddress ).ToString().c_str());

                                    // Only add valid payouts to the vector.
                                    if(payout > 0){
                                        // Add wining bet payout to the bet vector.
                                        vexpectedPayouts.emplace_back(payout, GetScriptForDestination(CBitcoinAddress(payoutAddress).Get()), betAmount);
                                    }
                                }
                            }
                        }
                    }
                }

                if(eventStartedFlag){
                    break;
                }
            }

            if(eventStartedFlag){
                break;
            }

            BlocksIndex = chainActive.Next(BlocksIndex);
        }
    }

    return vexpectedPayouts;
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight + 1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}

#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet, bool fProofOfStake)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey, pwallet, fProofOfStake);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("WagerrMiner : generated block is stale");
    }

    // Remove key from key pool
    reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, NULL, pblock)) {
        if (pblock->IsZerocoinStake())
            pwalletMain->zwgrTracker->RemovePending(pblock->vtx[1].GetHash());
        return error("WagerrMiner : ProcessNewBlock, block not accepted");
    }

    for (CNode* node : vNodes) {
        node->PushInventory(CInv(MSG_BLOCK, pblock->GetHash()));
    }

    return true;
}

bool fGenerateBitcoins = false;
bool fMintableCoins = false;
int nMintableLastCheck = 0;

// ***TODO*** that part changed in bitcoin, we are using a mix with old one here for now

void BitcoinMiner(CWallet* pwallet, bool fProofOfStake)
{
    LogPrintf("WagerrMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("wagerr-miner");

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;

    while (fGenerateBitcoins || fProofOfStake) {
        if (fProofOfStake) {
            //control the amount of times the client will check for mintable coins
            if ((GetTime() - nMintableLastCheck > 5 * 60)) // 5 minute check time
            {
                nMintableLastCheck = GetTime();
                fMintableCoins = pwallet->MintableCoins();
            }

            if (chainActive.Tip()->nHeight < Params().LAST_POW_BLOCK()) {
                MilliSleep(5000);
                continue;
            }

            while (vNodes.empty() || pwallet->IsLocked() || !fMintableCoins || (pwallet->GetBalance() > 0 && nReserveBalance >= pwallet->GetBalance()) || !masternodeSync.IsSynced()) {
                nLastCoinStakeSearchInterval = 0;
                // Do a separate 1 minute check here to ensure fMintableCoins is updated
                if (!fMintableCoins) {
                    if (GetTime() - nMintableLastCheck > 1 * 60) // 1 minute check time
                    {
                        nMintableLastCheck = GetTime();
                        fMintableCoins = pwallet->MintableCoins();
                    }
                }
                MilliSleep(5000);
                if (!fGenerateBitcoins && !fProofOfStake)
                    continue;
            }

            if (mapHashedBlocks.count(chainActive.Tip()->nHeight)) //search our map of hashed blocks, see if bestblock has been hashed yet
            {
                if (GetTime() - mapHashedBlocks[chainActive.Tip()->nHeight] < max(pwallet->nHashInterval, (unsigned int)1)) // wait half of the nHashDrift with max wait of 3 minutes
                {
                    MilliSleep(5000);
                    continue;
                }
            }
        }

        //
        // Create new block
        //
        unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrev = chainActive.Tip();
        if (!pindexPrev)
            continue;

        unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey, pwallet, fProofOfStake));
        if (!pblocktemplate.get())
            continue;

        CBlock* pblock = &pblocktemplate->block;
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

        //Stake miner main
        if (fProofOfStake) {
            LogPrintf("CPUMiner : proof-of-stake block found %s \n", pblock->GetHash().ToString().c_str());
            if (pblock->IsZerocoinStake()) {
                //Find the key associated with the zerocoin that is being staked
                libzerocoin::CoinSpend spend = TxInToZerocoinSpend(pblock->vtx[1].vin[0]);
                CBigNum bnSerial = spend.getCoinSerialNumber();
                CKey key;
                if (!pwallet->GetZerocoinKey(bnSerial, key)) {
                    LogPrintf("%s: failed to find zWGR with serial %s, unable to sign block\n", __func__, bnSerial.GetHex());
                    continue;
                }

                //Sign block with the zWGR key
                if (!SignBlockWithKey(*pblock, key)) {
                    LogPrintf("BitcoinMiner(): Signing new block with zWGR key failed \n");
                    continue;
                }
            } else if (!SignBlock(*pblock, *pwallet)) {
                LogPrintf("BitcoinMiner(): Signing new block with UTXO key failed \n");
                continue;
            }

            LogPrintf("CPUMiner : proof-of-stake block was signed %s \n", pblock->GetHash().ToString().c_str());
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            ProcessBlockFound(pblock, *pwallet, reservekey);
            SetThreadPriority(THREAD_PRIORITY_LOWEST);

            continue;
        }

        LogPrintf("Running WagerrMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
            ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

        //
        // Search
        //
        int64_t nStart = GetTime();
        uint256 hashTarget = uint256().SetCompact(pblock->nBits);
        while (true) {
            unsigned int nHashesDone = 0;

            uint256 hash;
            while (true) {
                hash = pblock->GetHash();
                if (hash <= hashTarget) {
                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("BitcoinMiner:\n");
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                    ProcessBlockFound(pblock, *pwallet, reservekey);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);

                    // In regression test mode, stop mining after a block is found. This
                    // allows developers to controllably generate a block on demand.
                    if (Params().MineBlocksOnDemand())
                        throw boost::thread_interrupted();

                    break;
                }
                pblock->nNonce += 1;
                nHashesDone += 1;
                if ((pblock->nNonce & 0xFF) == 0)
                    break;
            }

            // Meter hashes/sec
            static int64_t nHashCounter;
            if (nHPSTimerStart == 0) {
                nHPSTimerStart = GetTimeMillis();
                nHashCounter = 0;
            } else
                nHashCounter += nHashesDone;
            if (GetTimeMillis() - nHPSTimerStart > 4000) {
                static CCriticalSection cs;
                {
                    LOCK(cs);
                    if (GetTimeMillis() - nHPSTimerStart > 4000) {
                        dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                        nHPSTimerStart = GetTimeMillis();
                        nHashCounter = 0;
                        static int64_t nLogTime;
                        if (GetTime() - nLogTime > 30 * 60) {
                            nLogTime = GetTime();
                            LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec / 1000.0);
                        }
                    }
                }
            }

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();
            // Regtest mode doesn't require peers
            if (vNodes.empty() && Params().MiningRequiresPeers())
                break;
            if (pblock->nNonce >= 0xffff0000)
                break;
            if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                break;
            if (pindexPrev != chainActive.Tip())
                break;

            // Update nTime every few seconds
            UpdateTime(pblock, pindexPrev);
            if (Params().AllowMinDifficultyBlocks()) {
                // Changing pblock->nTime can change work required on testnet:
                hashTarget.SetCompact(pblock->nBits);
            }
        }
    }
}

void static ThreadBitcoinMiner(void* parg)
{
    boost::this_thread::interruption_point();
    CWallet* pwallet = (CWallet*)parg;
    try {
        BitcoinMiner(pwallet, false);
        boost::this_thread::interruption_point();
    } catch (std::exception& e) {
        LogPrintf("ThreadBitcoinMiner() exception");
    } catch (...) {
        LogPrintf("ThreadBitcoinMiner() exception");
    }

    LogPrintf("ThreadBitcoinMiner exiting\n");
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;
    fGenerateBitcoins = fGenerate;

    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&ThreadBitcoinMiner, pwallet));
}

#endif // ENABLE_WALLET