#include <primitives/deterministicmint.h>
#include "zwgrtracker.h"
#include "util.h"
#include "sync.h"
#include "main.h"
#include "txdb.h"
#include "walletdb.h"

using namespace std;

CzWGRTracker::CzWGRTracker(std::string strWalletFile)
{
    this->strWalletFile = strWalletFile;
    mapSerialHashes.clear();
}

void CzWGRTracker::Init()
{
    //Load all CZerocoinMints and CDeterministicMints from the database
    ListMints(false, false, true);
}

bool CzWGRTracker::Archive(CMintMeta& meta)
{
    if (mapSerialHashes.count(meta.hashSerial))
        mapSerialHashes.at(meta.hashSerial).isArchived = true;

    CWalletDB walletdb(strWalletFile);
    CZerocoinMint mint;
    if (walletdb.ReadZerocoinMint(meta.hashPubcoin, mint)) {
        if (!CWalletDB(strWalletFile).ArchiveMintOrphan(mint))
            return error("%s: failed to archive zerocoinmint", __func__);
    } else {
        //failed to read mint from DB, try reading deterministic
        CDeterministicMint dMint;
        if (!walletdb.ReadDeterministicMint(meta.hashPubcoin, dMint))
            return error("%s: could not find pubcoinhash %s in db", __func__, meta.hashPubcoin.GetHex());
        if (!walletdb.ArchiveDeterministicOrphan(dMint))
            return error("%s: failed to archive deterministic ophaned mint", __func__);
    }

    LogPrintf("%s: archived pubcoinhash %s\n", __func__, meta.hashPubcoin.GetHex());
    return true;
}

bool CzWGRTracker::UnArchive(const uint256& hashPubcoin)
{
    if (!HasPubcoinHash(hashPubcoin))
        return error("%s: tracker does not have record of pubcoinhash %s", __func__, hashPubcoin.GetHex());

    CMintMeta meta = GetMetaFromPubcoin(hashPubcoin);
    CWalletDB walletdb(strWalletFile);
    if (meta.isDeterministic) {
        if (!walletdb.UnarchiveDeterministicMint(hashPubcoin))
            return error("%s: failed to unarchive", __func__);
    } else {
        if (!walletdb.UnarchiveZerocoinMint(hashPubcoin))
            return error("%s: failed to unarchive", __func__);
    }

    mapSerialHashes.at(meta.hashSerial).isArchived = false;
    LogPrintf("%s: unarchived %s\n", __func__, meta.hashPubcoin.GetHex());
    return true;
}

CMintMeta CzWGRTracker::Get(const uint256 &hashSerial)
{
    if (!mapSerialHashes.count(hashSerial))
        return CMintMeta();

    return mapSerialHashes.at(hashSerial);
}

CMintMeta CzWGRTracker::GetMetaFromPubcoin(const uint256& hashPubcoin)
{
    for (auto it : mapSerialHashes) {
        CMintMeta meta = it.second;
        if (meta.hashPubcoin == hashPubcoin)
            return meta;
    }

    return CMintMeta();
}

bool CzWGRTracker::GetMetaFromStakeHash(const uint256& hashStake, CMintMeta& meta) const
{
    for (auto& it : mapSerialHashes) {
        if (it.second.hashStake == hashStake) {
            meta = it.second;
            return true;
        }
    }

    return false;
}

std::vector<uint256> CzWGRTracker::GetSerialHashes()
{
    vector<uint256> vHashes;
    for (auto it : mapSerialHashes) {
        if (it.second.isArchived)
            continue;

        vHashes.emplace_back(it.first);
    }


    return vHashes;
}

CAmount CzWGRTracker::GetBalance(bool fConfirmedOnly, bool fUnconfirmedOnly) const
{
    CAmount nTotal = 0;
    //! zerocoin specific fields
    std::map<libzerocoin::CoinDenomination, unsigned int> myZerocoinSupply;
    for (auto& denom : libzerocoin::zerocoinDenomList) {
        myZerocoinSupply.insert(make_pair(denom, 0));
    }

    {
        //LOCK(cs_wgrtracker);
        // Get Unused coins
        for (auto& it : mapSerialHashes) {
            CMintMeta meta = it.second;
            if (meta.isUsed || meta.isArchived)
                continue;
            bool fConfirmed = ((meta.nHeight < chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations()) && !(meta.nHeight == 0));
            if (fConfirmedOnly && !fConfirmed)
                continue;
            if (fUnconfirmedOnly && fConfirmed)
                continue;

            nTotal += libzerocoin::ZerocoinDenominationToAmount(meta.denom);
            myZerocoinSupply.at(meta.denom)++;
        }
    }
    for (auto& denom : libzerocoin::zerocoinDenomList) {
        LogPrint("zero","%s My coins for denomination %d pubcoin %s\n", __func__,denom, myZerocoinSupply.at(denom));
    }
    LogPrint("zero","Total value of coins %d\n",nTotal);

    if (nTotal < 0 ) nTotal = 0; // Sanity never hurts

    return nTotal;
}

CAmount CzWGRTracker::GetUnconfirmedBalance() const
{
    return GetBalance(false, true);
}

std::vector<CMintMeta> CzWGRTracker::GetMints(bool fConfirmedOnly) const
{
    vector<CMintMeta> vMints;
    for (auto& it : mapSerialHashes) {
        CMintMeta mint = it.second;
        if (mint.isArchived || mint.isUsed)
            continue;
        bool fConfirmed = (mint.nHeight < chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations());
        if (fConfirmedOnly && !fConfirmed)
            continue;
        vMints.emplace_back(mint);
    }
    return vMints;
}

//Does a mint in the tracker have this txid
bool CzWGRTracker::HasMintTx(const uint256& txid)
{
    for (auto it : mapSerialHashes) {
        if (it.second.txid == txid)
            return true;
    }

    return false;
}

bool CzWGRTracker::HasPubcoin(const CBigNum &bnValue) const
{
    // Check if this mint's pubcoin value belongs to our mapSerialHashes (which includes hashpubcoin values)
    uint256 hash = GetPubCoinHash(bnValue);
    return HasPubcoinHash(hash);
}

bool CzWGRTracker::HasPubcoinHash(const uint256& hashPubcoin) const
{
    for (auto it : mapSerialHashes) {
        CMintMeta meta = it.second;
        if (meta.hashPubcoin == hashPubcoin)
            return true;
    }
    return false;
}

bool CzWGRTracker::HasSerial(const CBigNum& bnSerial) const
{
    uint256 hash = GetSerialHash(bnSerial);
    return HasSerialHash(hash);
}

bool CzWGRTracker::HasSerialHash(const uint256& hashSerial) const
{
    auto it = mapSerialHashes.find(hashSerial);
    return it != mapSerialHashes.end();
}

bool CzWGRTracker::UpdateZerocoinMint(const CZerocoinMint& mint)
{
    if (!HasSerial(mint.GetSerialNumber()))
        return error("%s: mint %s is not known", __func__, mint.GetValue().GetHex());

    uint256 hashSerial = GetSerialHash(mint.GetSerialNumber());

    //Update the meta object
    CMintMeta meta = Get(hashSerial);
    meta.isUsed = mint.IsUsed();
    meta.denom = mint.GetDenomination();
    meta.nHeight = mint.GetHeight();
    mapSerialHashes.at(hashSerial) = meta;

    //Write to db
    return CWalletDB(strWalletFile).WriteZerocoinMint(mint);
}

bool CzWGRTracker::UpdateState(const CMintMeta& meta)
{
    CWalletDB walletdb(strWalletFile);

    if (meta.isDeterministic) {
        CDeterministicMint dMint;
        if (!walletdb.ReadDeterministicMint(meta.hashPubcoin, dMint))
            return error("%s: failed to read deterministic mint from database", __func__);

        dMint.SetTxHash(meta.txid);
        dMint.SetHeight(meta.nHeight);
        dMint.SetUsed(meta.isUsed);
        dMint.SetDenomination(meta.denom);
        dMint.SetStakeHash(meta.hashStake);

        if (!walletdb.WriteDeterministicMint(dMint))
            return error("%s: failed to update deterministic mint when writing to db", __func__);
    } else {
        CZerocoinMint mint;
        if (!walletdb.ReadZerocoinMint(meta.hashPubcoin, mint))
            return error("%s: failed to read mint from database", __func__);

        mint.SetTxHash(meta.txid);
        mint.SetHeight(meta.nHeight);
        mint.SetUsed(meta.isUsed);
        mint.SetDenomination(meta.denom);

        if (!walletdb.WriteZerocoinMint(mint))
            return error("%s: failed to write mint to database", __func__);
    }

    mapSerialHashes[meta.hashSerial] = meta;

    return true;
}

//Only returns mints from that were not generated deterministically
std::list<CZerocoinMint> CzWGRTracker::ListZerocoinMints(bool fUnusedOnly, bool fMatureOnly, bool fUpdateStatus)
{
    std::list<CMintMeta> listTracker = ListMints(fUnusedOnly, fMatureOnly, fUpdateStatus);

    CWalletDB walletdb(strWalletFile);
    std::list<CZerocoinMint> listMintsDB = walletdb.ListMintedCoins();

    //Only select mints that were selected by the tracker
    std::list<CZerocoinMint> listReturn;
    for (auto& meta : listTracker) {
        for (auto& mint : listMintsDB) {
            uint256 hashPubcoin = GetPubCoinHash(mint.GetValue());
            if (meta.hashPubcoin == hashPubcoin) {
                listReturn.emplace_back(mint);
            }
        }
    }

    return listReturn;
}

void CzWGRTracker::Add(const CDeterministicMint& dMint, bool isNew, bool isArchived)
{
    CMintMeta meta;
    meta.hashPubcoin = dMint.GetPubcoinHash();
    meta.nHeight = dMint.GetHeight();
    meta.nVersion = dMint.GetVersion();
    meta.txid = dMint.GetTxHash();
    meta.isUsed = dMint.IsUsed();
    meta.hashSerial = dMint.GetSerialHash();
    meta.hashStake = dMint.GetStakeHash();
    meta.denom = dMint.GetDenomination();
    meta.isArchived = isArchived;
    meta.isDeterministic = true;
    mapSerialHashes[meta.hashSerial] = meta;

    if (isNew)
        CWalletDB(strWalletFile).WriteDeterministicMint(dMint);
}

void CzWGRTracker::Add(const CZerocoinMint& mint, bool isNew, bool isArchived)
{
    CMintMeta meta;
    meta.hashPubcoin = GetPubCoinHash(mint.GetValue());
    meta.nHeight = mint.GetHeight();
    meta.nVersion = libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber());
    meta.txid = mint.GetTxHash();
    meta.isUsed = mint.IsUsed();
    meta.hashSerial = GetSerialHash(mint.GetSerialNumber());
    uint256 nSerial = mint.GetSerialNumber().getuint256();
    meta.hashStake = Hash(nSerial.begin(), nSerial.end());
    meta.denom = mint.GetDenomination();
    meta.isArchived = isArchived;
    meta.isDeterministic = false;
    mapSerialHashes[meta.hashSerial] = meta;

    if (isNew)
        CWalletDB(strWalletFile).WriteZerocoinMint(mint);
}

void CzWGRTracker::SetPubcoinUsed(const uint256& hashPubcoin, const bool isUsed)
{
    if (!HasPubcoinHash(hashPubcoin))
        return;;
    CMintMeta meta = GetMetaFromPubcoin(hashPubcoin);
    meta.isUsed = isUsed;
    UpdateState(meta);
}

std::list<CMintMeta> CzWGRTracker::ListMints(bool fUnusedOnly, bool fMatureOnly, bool fUpdateStatus)
{
    CWalletDB walletdb(strWalletFile);
    if (fUpdateStatus) {
        std::list<CZerocoinMint> listMintsDB = walletdb.ListMintedCoins();
        for (auto& mint : listMintsDB)
            Add(mint);
        LogPrintf("%s: added %d zerocoinmints from DB\n", __func__, listMintsDB.size());

        std::list<CDeterministicMint> listDeterministicDB = walletdb.ListDeterministicMints();
        for (auto& dMint : listDeterministicDB)
            Add(dMint);
        LogPrintf("%s: added %d dzwgr from DB\n", __func__, listDeterministicDB.size());

//        std::list<CZerocoinMint> listArchivedZerocoinMints = walletdb.ListArchivedZerocoins();
//        for (auto& mint : listArchivedZerocoinMints)
//            Add(mint, false, true);
//        LogPrintf("%s: added %d archived zerocoinmints from DB\n", __func__, listArchivedZerocoinMints.size());
//
//        std::list<CDeterministicMint> listArchivedDeterministicMints = walletdb.ListArchivedDeterministicMints();
//        for (auto& dMint : listArchivedDeterministicMints)
//            Add(dMint, false, true);
//        LogPrintf("%s: added %d archived deterministic mints from DB\n", __func__, listArchivedDeterministicMints.size());
    }

    vector<CMintMeta> vOverWrite;
    std::list<CMintMeta> listMints;
    for (auto& it : mapSerialHashes) {
        CMintMeta mint = it.second;

        //This is only intended for unarchived coins
        if (mint.isArchived)
            continue;

        if (fUnusedOnly) {
            if (mint.isUsed)
                continue;

            //double check that we have no record of this serial being used
            int nHeight;
            if (IsSerialInBlockchain(mint.hashSerial, nHeight)) {
                mint.isUsed = true;
                vOverWrite.emplace_back(mint);
                continue;
            }
        }

        if (fMatureOnly || fUpdateStatus) {
            //if there is not a record of the block height, then look it up and assign it
            uint256 txid;
            bool isInChain = zerocoinDB->ReadCoinMint(mint.hashPubcoin, txid);
            if (!mint.nHeight || !isInChain) {
                CTransaction tx;
                uint256 hashBlock;

                if (mint.txid == 0) {
                    if (!isInChain) {
                        LogPrintf("%s failed to find tx for mint %s\n", __func__, mint.hashPubcoin.GetHex().substr(0, 6));
                        Archive(mint);
                        continue;
                    }
                    mint.txid = txid;
                }

                if (!GetTransaction(mint.txid, tx, hashBlock, true)) {
                    LogPrintf("%s failed to find tx for mint txid=%s\n", __func__, mint.txid.GetHex());
                    Archive(mint);
                    continue;
                }

                //if not in the block index, most likely is unconfirmed tx
                if (mapBlockIndex.count(hashBlock)) {
                    mint.nHeight = mapBlockIndex[hashBlock]->nHeight;
                    vOverWrite.emplace_back(mint);
                } else if (fMatureOnly) {
                    continue;
                }
            }

            //not mature
            if (mint.nHeight > chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations()) {
                if (!fMatureOnly)
                    listMints.emplace_back(mint);
                continue;
            }

            //if only requesting an update (fUpdateStatus) then skip the rest and add to list
            if (fMatureOnly) {
                // check to make sure there are at least 3 other mints added to the accumulators after this
                if (chainActive.Height() < mint.nHeight + 1)
                    continue;

                CBlockIndex *pindex = chainActive[mint.nHeight + 1];
                int nMintsAdded = 0;
                while (pindex->nHeight < chainActive.Height() - 30) { // 30 just to make sure that its at least 2 checkpoints from the top block
                    nMintsAdded += count(pindex->vMintDenominationsInBlock.begin(),
                                         pindex->vMintDenominationsInBlock.end(), mint.denom);
                    if (nMintsAdded >= Params().Zerocoin_RequiredAccumulation())
                        break;
                    pindex = chainActive[pindex->nHeight + 1];
                }

                if (nMintsAdded < Params().Zerocoin_RequiredAccumulation())
                    continue;
            }
        }
        listMints.emplace_back(mint);
    }

    //overwrite any updates
    for (CMintMeta& meta : vOverWrite)
        UpdateState(meta);

    return listMints;
}

void CzWGRTracker::Clear()
{
    mapSerialHashes.clear();
}