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
#include "bet.h"

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

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Make sure to create the correct block version after zerocoin is enabled
    bool fZerocoinActive = GetAdjustedTime() >= Params().Zerocoin_StartTime();
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
std::vector<CPeerlessResult> getEventResults( int height ) {

    // Set the Oracle wallet address. 
    std::string OracleWalletAddr = Params().OracleWalletAddr();
    std::vector<CPeerlessResult> results;

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[height];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    BOOST_FOREACH(CTransaction& tx, block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet.
        const CTxIn &txin  = tx.vin[0];
        bool validResultTx = IsValidOracleTx(txin);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (unsigned int i = 0; i < tx.vout.size(); i++) {

                const CTxOut &txout = tx.vout[i];
                std::string scriptPubKey = txout.scriptPubKey.ToString();

                // TODO Remove hard-coded values from this block.
                if(scriptPubKey.length() > 0 && strncmp(scriptPubKey.c_str(), "OP_RETURN", 9) == 0) {

                    // Get OP CODE from transactions.
                    vector<unsigned char> v = ParseHex(scriptPubKey.substr(9, string::npos));
                    std::string opCode(v.begin(), v.end());

                    LogPrintf("RESULT OP_RETURN -> %s \n", opCode.c_str());

                    CPeerlessResult plResult;
                    if (!CPeerlessResult::FromOpCode(opCode, plResult)) {
                        continue;
                    }

                    // Store the result if its a valid result OP CODE.
                    results.push_back(plResult);
                }
            }
        }
    }

    return results;
}

/**
 * Creates the bet payout vector for all winning bets.
 *
 * @return payout vector.
 */
std::vector<CTxOut> GetBetPayouts(int height) {

    std::vector<CTxOut> vexpectedPayouts;
    int nCurrentHeight = chainActive.Height();

    // Get all the results posted in the latest block.
    std::vector<CPeerlessResult> results = getEventResults(height);
    LogPrintf("Results found: %li \n", results.size());

    // Set the Oracle wallet address. 
    std::string OracleWalletAddr = Params().OracleWalletAddr();

    // Traverse the blockchain for an event to match a result and all the bets on a result.
    for (const auto& result : results) {

        bool eventFound = false;

        // Look back the chain 14 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        BlocksIndex = chainActive[nCurrentHeight - Params().BetBlocksIndexTimespan()];

        unsigned int oddsDivisor  = Params().OddsDivisor();
        unsigned int betXPermille = Params().BetXPermille();

        CPeerlessEvent latestEvent;
        latestEvent.nHomeOdds = 0;
        latestEvent.nAwayOdds = 0;
        latestEvent.nDrawOdds = 0;
        latestEvent.nHomeTeam = 0;
        latestEvent.nAwayTeam = 0;
        latestEvent.nStartTime = 0;
        bool eventStartedFlag = false;

        // Traverse the block chain to find events and bets.
        while (BlocksIndex) {

            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            time_t transactionTime = block.nTime;

            BOOST_FOREACH(CTransaction &tx, block.vtx) {

                // Ensure if event TX that has it been posted by Oracle wallet.
                const CTxIn &txin = tx.vin[0];
                bool validResultTx = IsValidOracleTx(txin);
                
                // Check all TX vouts for an OP RETURN.
                for(unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = txout.scriptPubKey.ToString();
                    CAmount betAmount = txout.nValue;

                    if( validResultTx && scriptPubKey.length() > 0 && 0 == strncmp(scriptPubKey.c_str(), "OP_RETURN", 9)) {

                        // Get the OP CODE from the transaction scriptPubKey.
                        vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        CPeerlessEvent pe;
                        if (CPeerlessEvent::FromOpCode(opCode, pe)) {

                            CTxDestination address;
                            ExtractDestination(tx.vout[0].scriptPubKey, address);

                            LogPrintf("EVENT OP CODE - %s \n", opCode.c_str());

                            if (CBitcoinAddress(address).ToString() == OracleWalletAddr) {

                                // If current event ID matches result ID set the teams and odds.
                                if (result.nEventId == pe.nEventId) {
                                    latestEvent.nHomeTeam = pe.nHomeTeam;
                                    latestEvent.nAwayTeam = pe.nAwayTeam;

                                    LogPrintf("HomeTeam = %s & AwayTeam = %s \n", latestEvent.nHomeTeam, latestEvent.nAwayTeam);

                                    latestEvent.nHomeOdds  = pe.nHomeOdds;
                                    latestEvent.nAwayOdds  = pe.nAwayOdds;
                                    latestEvent.nDrawOdds  = pe.nAwayOdds;
                                    latestEvent.nStartTime = pe.nStartTime;
                                    eventFound = true;

                                    LogPrintf("latestHomeOdds = %u & latestAwayOdds = %u & latestDrawOdds = %u \n", latestEvent.nHomeOdds, latestEvent.nAwayOdds, latestEvent.nDrawOdds);
                                }
                            }
                        }

                        // Only payout bets that are between 50 - 10000 WRG inclusive (MaxBetPayoutRange).
                        if( betAmount >= (Params().MinBetPayoutRange() * COIN) && betAmount <= (Params().MaxBetPayoutRange() * COIN) ) {

                            // Bet OP RETURN transaction.
                            CPeerlessBet pb;
                            if (CPeerlessBet::FromOpCode(opCode, pb)) {

                                CAmount payout = 0 * COIN;

                                // If bet was placed less than 20 mins before event start or after event start discard it.
                                if (latestEvent.nStartTime > 0 && transactionTime > (latestEvent.nStartTime - Params().BetPlaceTimeoutBlocks())) {
                                    eventStartedFlag = true;
                                    break;
                                }

                                // Is the bet a winning bet?
                                if (result.nEventId == pb.nEventId) {
                                    CAmount winnings = 0;

                                    // If bet payout result.
                                    if (result.nResult != ResultTypeRefund) {

                                        // Calculate winnings.
                                        if (pb.nOutcome == ResultTypeRefund) {
                                            winnings = betAmount * latestEvent.nHomeOdds;
                                        }
                                        else if (pb.nOutcome == OutcomeTypeLose) {
                                            winnings = betAmount * latestEvent.nAwayOdds;
                                        }
                                        else {
                                            winnings = betAmount * latestEvent.nDrawOdds;
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
                                        // Add winning bet payout to the bet vector.
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

/**
 * Checks a given block for any Chain Games results.
 *
 * @param height The block we want to check for the
 * @return results array.
 */
std::pair<std::vector<CChainGamesEvent>,std::vector<std::string>> getCGLottoEventResults(int height)
{
    // Set the Oracle wallet address.
    std::string OracleWalletAddr = Params().OracleWalletAddr();
    std::vector<CChainGamesEvent> chainGameResults;
    std::vector<std::string> blockTotalValues;
    CAmount totalBlockValue = 0;

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = chainActive[height];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    int blockTime = block.GetBlockTime();

    BOOST_FOREACH(CTransaction& tx, block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet by looking at the TX vins.
        const CTxIn &txin = tx.vin[0];
        COutPoint prevout = txin.prevout;

        uint256 hashBlock;
        CTransaction txPrev;

        bool validResultTx = IsValidOracleTx(txin);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                const CTxOut &txout = tx.vout[i];
                std::string scriptPubKey = txout.scriptPubKey.ToString();
                totalBlockValue = txout.nValue + totalBlockValue;

                if (scriptPubKey.length() > 0 && strncmp(scriptPubKey.c_str(), "OP_RETURN", 9) == 0) {
                    // Get OP CODE from transactions.
                    vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, string::npos));
                    std::string opCode(vOpCode.begin(), vOpCode.end());

                    CChainGamesEvent plResult;
                    if (!CChainGamesEvent::FromOpCode(opCode, plResult)) {
                        continue;
                    }

                    chainGameResults.push_back(plResult);
                }
            }
        }
    }

    unsigned long long LGTotal = blockTime + totalBlockValue;
    char strTotal[256];
    sprintf(strTotal, "%lld", LGTotal);

    // If a CGLotto result is found, append total block value to each result
    if (chainGameResults.size() != 0) {
        for (int i = 0; i < chainGameResults.size(); i++) {
            blockTotalValues.emplace_back(strTotal);
        }
    }

    return std::make_pair(chainGameResults,blockTotalValues);
}


/**
 * Creates the bet payout vector for all winning CGLotto events.
 *
 * @return payout vector.
 */
std::vector<CTxOut> GetCGLottoBetPayouts (int height)
{
    std::vector<CTxOut> vexpectedCGLottoBetPayouts;
    int nCurrentHeight = chainActive.Height();
    CAmount totalValueOfBlock = 0 * COIN;

    std::pair<std::vector<CChainGamesEvent>,std::vector<std::string>> resultArray = getCGLottoEventResults(height);
    std::vector<CChainGamesEvent> allChainGames = resultArray.first;
    std::vector<std::string> blockSizeArray = resultArray.second;
    LogPrintf("Chain game Results: %u \n", resultArray.second.size());

    // Set the Oracle wallet address.
    std::string OracleWalletAddr = Params().OracleWalletAddr();

    // Find payout for each CGLotto game
    for (unsigned int currResult = 0; currResult < resultArray.second.size(); currResult++) {

        CChainGamesEvent currentChainGame = allChainGames[currResult];
        int currentEventID = currentChainGame.nEventId;
        CAmount eventFee = 0;
        
        //reset total bet amount and candidate array for this event
        std::vector<std::string> candidates;
        CAmount totalBetAmount = 0 * COIN;

        // Look back the chain 14 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        BlocksIndex = chainActive[nCurrentHeight - Params().BetBlocksIndexTimespan()];

        time_t eventStart = 0;
        bool eventStartedFlag = false;

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

                bool validTX = IsValidOracleTx(txin);

                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = txout.scriptPubKey.ToString();
                    CAmount betAmount = txout.nValue;

                    if (validTX && scriptPubKey.length() > 0 && 0 == strncmp(scriptPubKey.c_str(), "OP_RETURN", 9)) {
                        // Get the OP CODE from the transaction scriptPubKey.
                        vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        // If bet was placed less than 20 mins before event start or after event start discard it.
                        if (eventStart > 0 && transactionTime > (eventStart - Params().BetPlaceTimeoutBlocks())) {
                            eventStartedFlag = true;
                            break;
                        }

                        // Find most recent CGLotto event
                        CChainGamesEvent chainGameEvt;
                        if (CChainGamesEvent::FromOpCode(opCode, chainGameEvt)) {
                            eventFee = chainGameEvt.nEntryFee * COIN;
                            LogPrintf("\nFound chain games event (%s), setting entry price: %i \n", chainGameEvt.nEventId, eventFee);
                        }

                        // Find most recent CGLotto bet once the event has been found
                        CChainGamesBet chainGamesBet(0);
                        if (CChainGamesBet::FromOpCode(opCode, chainGamesBet)) { //TODO: This condition had  && eventFee != 0

                            LogPrintf("\nFound chain games bet (%i), searching for id: %i \n", chainGamesBet.nEventId, currentEventID);

                            int eventId = chainGamesBet.nEventId;

                            // If current event ID matches result ID add bettor to candidate array
                            if (eventId == currentEventID) {

                                CTxDestination address;
                                ExtractDestination(tx.vout[0].scriptPubKey, address);
                                LogPrintf("EVENT OP CODE - %s \n", opCode.c_str());

                                //Check Entry fee matches the bet amount
                                if (eventFee == betAmount) {

                                    totalBetAmount = totalBetAmount + betAmount;
                                    CTxDestination payoutAddress;

                                    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
                                        ExtractDestination( txPrev.vout[prevout.n].scriptPubKey, payoutAddress );
                                    }

                                    // Add the payout address of each candidate to array
                                    candidates.push_back(CBitcoinAddress( payoutAddress ).ToString().c_str());
                                    LogPrintf("Adding bettor to candidates array. Total pot is now %u \n", totalBetAmount);
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

        // Choose winner from candidates, pay out
        if (candidates.size() >= 1) {
            
            // Use random number to choose winner from array
            CAmount noOfBets = candidates.size();
            CAmount winnerIndex = totalValueOfBlock % noOfBets;

            if (winnerIndex > noOfBets) {
                winnerIndex = noOfBets;
            }

            // Split the pot and calulate winnings
            std::string winnerAddress = candidates[winnerIndex];
            CAmount entranceFee = eventFee;
            CAmount totalPot = (noOfBets*entranceFee);
            CAmount winnerPayout = totalPot*.50;
            CAmount fee = totalPot*.2;

            LogPrintf("\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
            LogPrintf("Total number Of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrintf("Winner Address: %u (index no %u) \n", winnerAddress, winnerIndex);
            LogPrintf("Total Value of Block: %u \n", totalValueOfBlock);
            LogPrintf("Entrance fee: %u \n", entranceFee);
            LogPrintf("Total Pot: %u  , Winnings: %u , Fee: %u \n", winnerPayout, totalPot, fee);
            LogPrintf("Won: %u \n", totalPot*.50);

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                vexpectedCGLottoBetPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()));
            }
        }
    }

    return vexpectedCGLottoBetPayouts;
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