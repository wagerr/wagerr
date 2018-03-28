// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_ZWGRWALLET_H
#define WAGERR_ZWGRWALLET_H

#include <map>
#include "libzerocoin/Coin.h"
#include "mintpool.h"
#include "uint256.h"
#include "primitives/zerocoin.h"

class CzWGRWallet
{
private:
    uint256 seedMaster;
    uint32_t nCount;
    std::string strWalletFile;
    CMintPool mintPool;

public:
    CzWGRWallet(std::string strWalletFile, bool fFirstRun);
    bool SetMasterSeed(const uint256& seedMaster, bool fResetCount = false);
    uint256 GetMasterSeed() { return seedMaster; }
    void SyncWithChain(bool fGenerateMintPool = true);
    void GenerateDeterministicZWGR(libzerocoin::CoinDenomination denom, libzerocoin::PrivateCoin& coin, bool fGenerateOnly = false);
    void GenerateMint(uint32_t nCount, libzerocoin::PrivateCoin& coin, libzerocoin::CoinDenomination denom = libzerocoin::CoinDenomination::ZQ_ONE);
    void GenerateMintPool(int nCountStart = -1, int nCountEnd = -1);
    bool LoadMintPoolFromDB();
    void RemoveMintsFromPool(const std::vector<uint256>& vPubcoinHashes);
    bool SetMintSeen(const CBigNum& bnValue, const int& nHeight, const uint256& txid, const libzerocoin::CoinDenomination& denom);
    bool IsInMintPool(const CBigNum& bnValue) { return mintPool.Has(bnValue); }

private:
    uint512 GetNextZerocoinSeed();
    uint512 GetZerocoinSeed(uint32_t n);
    void UpdateCount();
    void SeedToZWGR(const uint512& seed, CBigNum& bnSerial, CBigNum& bnRandomness, CKey& key);
};

#endif //WAGERR_ZWGRWALLET_H
