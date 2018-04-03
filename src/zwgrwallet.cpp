// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zwgrwallet.h"
#include "main.h"
#include "txdb.h"
#include "walletdb.h"
#include "init.h"
#include "wallet.h"
#include "primitives/deterministicmint.h"

using namespace libzerocoin;

CzWGRWallet::CzWGRWallet(std::string strWalletFile, bool fFirstRun)
{
    this->strWalletFile = strWalletFile;
    CWalletDB walletdb(strWalletFile);

    uint256 seed;
    if (!walletdb.ReadZWGRSeed(seed))
        fFirstRun = true;

    //First time running, generate master seed
    if (fFirstRun)
        seed = CBigNum::randBignum(CBigNum(~uint256(0))).getuint256();

    SetMasterSeed(seed);
    this->mintPool = CMintPool(nCountLastUsed);
}

bool CzWGRWallet::SetMasterSeed(const uint256& seedMaster, bool fResetCount)
{
    this->seedMaster = seedMaster;

    CWalletDB walletdb(strWalletFile);
    if (!walletdb.WriteZWGRSeed(seedMaster))
        return false;

    nCountLastUsed = 0;
    if (fResetCount)
        walletdb.WriteZWGRCount(nCountLastUsed);
    else if (!walletdb.ReadZWGRCount(nCountLastUsed))
        nCountLastUsed = 0;

    mintPool.Reset();

    return true;
}

//Add the next 20 mints to the mint pool
void CzWGRWallet::GenerateMintPool(uint32_t nCountStart, uint32_t nCountEnd)
{
    uint32_t n = nCountLastUsed + 1;
    if (nCountStart > 0)
        n = nCountStart;

    uint32_t nStop = n + 20;
    if (nCountEnd > 0)
        nStop = std::max(n, n + nCountEnd);

    bool fFound;

    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    LogPrintf("%s : n=%d nStop=%d\n", __func__, n, nStop - 1);
    for (uint i = n; i < nStop; ++i) {
        fFound = false;

        // Prevent unnecessary repeated minted
        for (auto& pair : mintPool) {
            if(pair.second == i) {
                fFound = true;
                break;
            }
        }

        if(fFound)
            continue;

        uint512 seedZerocoin = GetZerocoinSeed(i);
        CBigNum bnSerial;
        CBigNum bnRandomness;
        CKey key;
        SeedToZWGR(seedZerocoin, bnSerial, bnRandomness, key);

        PrivateCoin coin(Params().Zerocoin_Params(false), CoinDenomination::ZQ_ONE, bnSerial, bnRandomness);
        coin.setVersion(PrivateCoin::CURRENT_VERSION);
        coin.setPrivKey(key.GetPrivKey());
        mintPool.Add(coin.getPublicCoin().getValue(), i);
        CWalletDB(strWalletFile).WriteMintPoolPair(hashSeed, GetPubCoinHash(coin.getPublicCoin().getValue()), i);
        LogPrintf("%s : %s count=%d\n", __func__, coin.getPublicCoin().getValue().GetHex().substr(0, 6), i);
    }
}

// pubcoin hashes are stored to db so that a full accounting of mints belonging to the seed can be tracked without regenerating
bool CzWGRWallet::LoadMintPoolFromDB()
{
    map<uint256, vector<pair<uint256, uint32_t> > > mapMintPool = CWalletDB(strWalletFile).MapMintPool();

    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    for (auto& pair : mapMintPool[hashSeed])
        mintPool.Add(pair);

    return true;
}

void CzWGRWallet::RemoveMintsFromPool(const std::vector<uint256>& vPubcoinHashes)
{
    for (const uint256& hash : vPubcoinHashes)
        mintPool.Remove(hash);
}

//Catch the counter up with the chain
map<uint256, uint32_t> mapMissingMints;
void CzWGRWallet::SyncWithChain(bool fGenerateMintPool)
{
    uint32_t nLastCountUsed = 0;
    bool found = true;
    CWalletDB walletdb(strWalletFile);
    CzWGRTracker* zwgrTracker = pwalletMain->zwgrTracker;
    std::set<uint256> setChecked;
    while (found) {
        found = false;
        if (fGenerateMintPool)
            GenerateMintPool();
        LogPrintf("%s: Mintpool size=%d\n", __func__, mintPool.size());

        for (pair<uint256, uint32_t> pMint : mintPool.List()) {
            if (setChecked.count(pMint.first))
                return;
            setChecked.insert(pMint.first);

            if (ShutdownRequested())
                return;

            if (mapMissingMints.count(pMint.first))
                continue;

            if (zwgrTracker->HasPubcoinHash(pMint.first)) {
                mintPool.Remove(pMint.first);
                continue;
            }

            uint256 txHash;
            CZerocoinMint mint;
            if (zerocoinDB->ReadCoinMint(pMint.first, txHash)) {
                //this mint has already occurred on the chain, increment counter's state to reflect this
                LogPrintf("%s : Found my coin mint %s in tx %s\n", __func__, pMint.first.GetHex(), txHash.GetHex());
                found = true;

                uint256 hashBlock;
                CTransaction tx;
                if (!GetTransaction(txHash, tx, hashBlock, true)) {
                    LogPrintf("%s : failed to get transaction for mint %s!\n", __func__, pMint.first.GetHex());
                    found = false;
                    nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                    mapMissingMints.insert(pMint);
                    continue;
                }

                //Find the denomination
                CoinDenomination denomination = CoinDenomination::ZQ_ERROR;
                bool fFoundMint = false;
                CBigNum bnValue = 0;
                for (const CTxOut out : tx.vout) {
                    if (!out.scriptPubKey.IsZerocoinMint())
                        continue;

                    PublicCoin pubcoin(Params().Zerocoin_Params(false));
                    CValidationState state;
                    if (!TxOutToPublicCoin(out, pubcoin, state)) {
                        LogPrintf("%s : failed to get mint from txout for %s!\n", __func__, pMint.first.GetHex());
                        continue;
                    }

                    // See if this is the mint that we are looking for
                    uint256 hashPubcoin = GetPubCoinHash(pubcoin.getValue());
                    if (pMint.first == hashPubcoin) {
                        denomination = pubcoin.getDenomination();
                        bnValue = pubcoin.getValue();
                        fFoundMint = true;
                        break;
                    }
                }

                if (!fFoundMint || denomination == ZQ_ERROR) {
                    LogPrintf("%s : failed to get mint %s from tx %s!\n", __func__, pMint.first.GetHex(), tx.GetHash().GetHex());
                    found = false;
                    mapMissingMints.insert(pMint);
                    break;
                }

                CBlock block;
                CWalletTx wtx(pwalletMain, tx);
                if (mapBlockIndex.count(hashBlock) && ReadBlockFromDisk(block, mapBlockIndex.at(hashBlock)))
                    wtx.SetMerkleBranch(block);

                //The mint was found in the chain, so recalculate the randomness and serial and DB it
                int nHeight = 0;
                int nTimeReceived = 0;
                if (mapBlockIndex.count(hashBlock)) {
                    nHeight = mapBlockIndex.at(hashBlock)->nHeight;
                    nTimeReceived = mapBlockIndex.at(hashBlock)->nTime;
                }

                //Fill out wtx so that a transaction record can be created
                wtx.nTimeReceived = nTimeReceived;
                pwalletMain->AddToWallet(wtx);

                SetMintSeen(bnValue, nHeight, txHash, denomination);
                nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                nCountLastUsed = std::max(nLastCountUsed, nCountLastUsed);
                LogPrint("zero", "%s: updated count to %d\n", __func__, nCountLastUsed);
            }
        }
    }
}

bool CzWGRWallet::SetMintSeen(const CBigNum& bnValue, const int& nHeight, const uint256& txid, const CoinDenomination& denom)
{
    if (!mintPool.Has(bnValue))
        return error("%s: value not in pool", __func__);
    pair<uint256, uint32_t> pMint = mintPool.Get(bnValue);

    // Regenerate the mint
    uint512 seedZerocoin = GetZerocoinSeed(pMint.second);
    CBigNum bnSerial;
    CBigNum bnRandomness;
    CKey key;
    SeedToZWGR(seedZerocoin, bnSerial, bnRandomness, key);

    // Create mint object and database it
    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    uint256 hashSerial = GetSerialHash(bnSerial);
    uint256 hashPubcoin = GetPubCoinHash(bnValue);
    uint256 nSerial = bnSerial.getuint256();
    uint256 hashStake = Hash(nSerial.begin(), nSerial.end());
    CDeterministicMint dMint(PrivateCoin::CURRENT_VERSION, pMint.second, hashSeed, hashSerial, hashPubcoin, hashStake);
    dMint.SetDenomination(denom);
    dMint.SetHeight(nHeight);
    dMint.SetTxHash(txid);

    // Check if this is also already spent
    int nHeightTx;
    if (IsSerialInBlockchain(hashSerial, nHeightTx)) {
        //Find transaction details and make a wallettx and add to wallet
        dMint.SetUsed(true);
        if (chainActive.Height() < nHeightTx)
            return error("%s: tx height %d is higher than chain height", __func__, nHeightTx);

        uint256 txHash;
        if (!zerocoinDB->ReadCoinSpend(hashSerial, txHash))
            return error("%s: did not find serial hash %s in zerocoindb", __func__, hashSerial.GetHex());

        uint256 hashBlock;
        CTransaction tx;
        if (!GetTransaction(txHash, tx, hashBlock, true))
            return error("%s: could not read transaction %s", __func__, txHash.GetHex());

        CWalletTx wtx(pwalletMain, tx);
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            CBlock block;
            if (ReadBlockFromDisk(block, pindex))
                wtx.SetMerkleBranch(block);
        }

        wtx.nTimeReceived = chainActive[nHeightTx]->nTime;
        pwalletMain->AddToWallet(wtx);
    }

    // Add to zwgrTracker which also adds to database
    pwalletMain->zwgrTracker->Add(dMint, true);

    //Update the count if it is less than the mint's count
    if (nCountLastUsed < pMint.second) {
        CWalletDB walletdb(strWalletFile);
        nCountLastUsed = pMint.second;
        walletdb.WriteZWGRCount(nCountLastUsed);
    }

    //remove from the pool
    mintPool.Remove(dMint.GetPubcoinHash());

    return true;
}

// Check if the value of the commitment meets requirements
bool IsValidCoinValue(const CBigNum& bnValue)
{
    return bnValue >= Params().Zerocoin_Params(false)->accumulatorParams.minCoinValue &&
    bnValue <= Params().Zerocoin_Params(false)->accumulatorParams.maxCoinValue &&
    bnValue.isPrime();
}

void CzWGRWallet::SeedToZWGR(const uint512& seedZerocoin, CBigNum& bnSerial, CBigNum& bnRandomness, CKey& key)
{
    ZerocoinParams* params = Params().Zerocoin_Params(false);

    //convert state seed into a seed for the private key
    uint256 nSeedPrivKey = seedZerocoin.trim256();

    bool isValidKey = false;
    key = CKey();
    while (!isValidKey) {
        nSeedPrivKey = Hash(nSeedPrivKey.begin(), nSeedPrivKey.end());
        isValidKey = libzerocoin::GenerateKeyPair(params->coinCommitmentGroup.groupOrder, nSeedPrivKey, key, bnSerial);
    }

    //hash randomness seed with Bottom 256 bits of seedZerocoin & attempts256 which is initially 0
    uint256 randomnessSeed = uint512(seedZerocoin >> 256).trim256();
    uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());
    bnRandomness.setuint256(hashRandomness);
    bnRandomness = bnRandomness % params->coinCommitmentGroup.groupOrder;

    //See if serial and randomness make a valid commitment
    // Generate a Pedersen commitment to the serial number
    CBigNum commitmentValue = params->coinCommitmentGroup.g.pow_mod(bnSerial, params->coinCommitmentGroup.modulus).mul_mod(
                        params->coinCommitmentGroup.h.pow_mod(bnRandomness, params->coinCommitmentGroup.modulus),
                        params->coinCommitmentGroup.modulus);

    CBigNum random;
    uint256 attempts256 = 0;
    // Iterate on Randomness until a valid commitmentValue is found
    while (true) {
        // Now verify that the commitment is a prime number
        // in the appropriate range. If not, we'll throw this coin
        // away and generate a new one.
        if (IsValidCoinValue(commitmentValue))
            return;

        //Did not create a valid commitment value.
        //Change randomness to something new and random and try again
        attempts256++;
        hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                              attempts256.begin(), attempts256.end());
        random.setuint256(hashRandomness);
        bnRandomness = (bnRandomness + random) % params->coinCommitmentGroup.groupOrder;
        commitmentValue = commitmentValue.mul_mod(params->coinCommitmentGroup.h.pow_mod(random, params->coinCommitmentGroup.modulus), params->coinCommitmentGroup.modulus);
    }
}

uint512 CzWGRWallet::GetNextZerocoinSeed()
{
    return GetZerocoinSeed(nCountLastUsed + 1);
}

uint512 CzWGRWallet::GetZerocoinSeed(uint32_t n)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster << n;
    uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());
    return zerocoinSeed;
}

void CzWGRWallet::UpdateCount()
{
    nCountLastUsed++;
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteZWGRCount(nCountLastUsed);
}

void CzWGRWallet::GenerateDeterministicZWGR(CoinDenomination denom, PrivateCoin& coin, CDeterministicMint& dMint, bool fGenerateOnly)
{
    GenerateMint(nCountLastUsed + 1, denom, coin, dMint);
    if (fGenerateOnly)
        return;

    //TODO remove this leak of seed from logs before merge to master
    //LogPrintf("%s : Generated new deterministic mint. Count=%d pubcoin=%s seed=%s\n", __func__, nCount, coin.getPublicCoin().getValue().GetHex().substr(0,6), seedZerocoin.GetHex().substr(0, 4));
}

void CzWGRWallet::GenerateMint(const uint32_t& nCount, const CoinDenomination denom, PrivateCoin& coin, CDeterministicMint& dMint)
{
    uint512 seedZerocoin = GetZerocoinSeed(nCount);
    CBigNum bnSerial;
    CBigNum bnRandomness;
    CKey key;
    SeedToZWGR(seedZerocoin, bnSerial, bnRandomness, key);
    coin = PrivateCoin(Params().Zerocoin_Params(false), denom, bnSerial, bnRandomness);
    coin.setPrivKey(key.GetPrivKey());
    coin.setVersion(PrivateCoin::CURRENT_VERSION);

    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    uint256 hashSerial = GetSerialHash(bnSerial);
    uint256 nSerial = bnSerial.getuint256();
    uint256 hashStake = Hash(nSerial.begin(), nSerial.end());
    uint256 hashPubcoin = GetPubCoinHash(coin.getPublicCoin().getValue());
    dMint = CDeterministicMint(coin.getVersion(), nCount, hashSeed, hashSerial, hashPubcoin, hashStake);
    dMint.SetDenomination(denom);
}

bool CzWGRWallet::RegenerateMint(const CDeterministicMint& dMint, CZerocoinMint& mint)
{
    //Check that the seed is correct    todo:handling of incorrect, or multiple seeds
    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    if (hashSeed != dMint.GetSeedHash())
        return error("%s: master seed does not match!", __func__);

    //Generate the coin
    PrivateCoin coin(Params().Zerocoin_Params(false), dMint.GetDenomination(), false);
    CDeterministicMint dMintDummy;
    GenerateMint(dMint.GetCount(), dMint.GetDenomination(), coin, dMintDummy);

    //Fill in the zerocoinmint object's details
    CBigNum bnValue = coin.getPublicCoin().getValue();
    if (GetPubCoinHash(bnValue) != dMint.GetPubcoinHash())
        return error("%s: failed to correctly generate mint, pubcoin hash mismatch", __func__);
    mint.SetValue(bnValue);

    CBigNum bnSerial = coin.getSerialNumber();
    if (GetSerialHash(bnSerial) != dMint.GetSerialHash())
        return error("%s: failed to correctly generate mint, serial hash mismatch", __func__);
    mint.SetSerialNumber(bnSerial);

    mint.SetRandomness(coin.getRandomness());
    mint.SetPrivKey(coin.getPrivKey());
    mint.SetVersion(coin.getVersion());
    mint.SetDenomination(dMint.GetDenomination());
    mint.SetUsed(dMint.IsUsed());
    mint.SetTxHash(dMint.GetTxHash());
    mint.SetHeight(dMint.GetHeight());

    return true;
}
