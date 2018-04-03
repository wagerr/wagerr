#ifndef WAGERR_ZWGRTRACKER_H
#define WAGERR_ZWGRTRACKER_H

#include "primitives/zerocoin.h"
#include <list>

class CDeterministicMint;

class CzWGRTracker
{
private:
    std::string strWalletFile;
    std::map<uint256, CMintMeta> mapSerialHashes;
    std::map<uint256, CMintMeta> mapArchivedSerialHashes;
public:
    CzWGRTracker(std::string strWalletFile);
    void Add(const CDeterministicMint& dMint, bool isNew = false, bool isArchived = false);
    void Add(const CZerocoinMint& mint, bool isNew = false, bool isArchived = false);
    bool Archive(CMintMeta& meta);
    bool HasPubcoin(const CBigNum& bnValue) const;
    bool HasPubcoinHash(const uint256& hashPubcoin) const;
    bool HasSerial(const CBigNum& bnSerial) const;
    bool HasSerialHash(const uint256& hashSerial) const;
    bool HasMintTx(const uint256& txid);
    bool IsEmpty() const { return mapSerialHashes.empty(); }
    void Init();
    CMintMeta Get(const uint256& hashSerial);
    CMintMeta GetMetaFromPubcoin(const uint256& hashPubcoin);
    bool GetMetaFromStakeHash(const uint256& hashStake, CMintMeta& meta) const;
    CAmount GetBalance(bool fConfirmedOnly, bool fUnconfirmedOnly) const;
    std::vector<uint256> GetSerialHashes();
    std::vector<CMintMeta> GetMints(bool fConfirmedOnly) const;
    CAmount GetUnconfirmedBalance() const;
    std::list<CMintMeta> ListMints(bool fUnusedOnly, bool fMatureOnly, bool fUpdateStatus);
    std::list<CZerocoinMint> ListZerocoinMints(bool fUnusedOnly, bool fMatureOnly, bool fUpdateStatus);
    void SetPubcoinUsed(const uint256& hashPubcoin, const bool isUsed);
    bool UnArchive(const uint256& hashPubcoin);
    bool UpdateZerocoinMint(const CZerocoinMint& mint);
    bool UpdateState(const CMintMeta& meta);
    void Clear();
};

#endif //WAGERR_ZWGRTRACKER_H
