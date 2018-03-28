// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zwgrwallet.h"
#include "main.h"
#include "txdb.h"
#include "walletdb.h"
#include "init.h"
#include "wallet.h"

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
    this->mintPool = CMintPool(nCount);
}

bool CzWGRWallet::SetMasterSeed(const uint256& seedMaster, bool fResetCount)
{
    this->seedMaster = seedMaster;

    CWalletDB walletdb(strWalletFile);
    if (!walletdb.WriteZWGRSeed(seedMaster))
        return false;

    nCount = 0;
    if (fResetCount)
        walletdb.WriteZWGRCount(nCount);
    else if (!walletdb.ReadZWGRCount(nCount))
        nCount = 0;

    //TODO remove this leak of seed from logs before merge to master
    LogPrintf("%s : seed=%s count=%d\n", __func__, seedMaster.GetHex(), nCount);

    //todo fix to sync with count above
    mintPool.Reset();

    return true;
}

//Add the next 10 mints to the mint pool
void CzWGRWallet::GenerateMintPool()
{
    int n = std::max(mintPool.CountOfLastGenerated() + 1, nCount);
    int nStop = n + 20;
    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    LogPrintf("%s : n=%d nStop=%d\n", __func__, n, nStop);
    for (int i = n; i < nStop; i++) {
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
    vector<pair<uint256, uint32_t> > vGenerated;
    map<uint256, vector<pair<uint256, uint32_t> > > mapMintPool = CWalletDB(strWalletFile).MapMintPool();

    //Todo: separate hashMasterSeeds
    for (auto& it : mapMintPool) {
        //todo check for any missing
        for (auto& pair : it.second)
            mintPool.Add(pair);
    }

    return true;
}

void CzWGRWallet::RemoveMintsFromPool(const std::vector<uint256>& vPubcoinHashes)
{
    for (const uint256& hash : vPubcoinHashes)
        mintPool.Remove(hash);
}

//Catch the counter up with the chain
map<uint256, uint32_t> mapMissingMints;
void CzWGRWallet::SyncWithChain()
{
    uint32_t nLastCountUsed = 0;
    bool found = true;
    CWalletDB walletdb(strWalletFile);
    CzWGRTracker* zwgrTracker = pwalletMain->zwgrTracker;
    while (found) {
        found = false;
        GenerateMintPool();

        for (pair<uint256, uint32_t> pMint : mintPool.List()) {
            if (ShutdownRequested())
                return;

            if (mapMissingMints.count(pMint.first))
                continue;

            // See if the mint is already known to the zwgr tracker
            if (zwgrTracker->HasPubcoinHash(pMint.first))
                continue;

            uint256 txHash;
            CZerocoinMint mint;
            if (zerocoinDB->ReadCoinMint(pMint.first, txHash)) {
                //this mint has already occured on the chain, increment counter's state to reflect this
                LogPrintf("%s : Found used coin mint %s \n", __func__, pMint.first.GetHex());
                found = true;

                uint256 hashBlock;
                CTransaction tx;
                if (!GetTransaction(txHash, tx, hashBlock)) {
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
                    nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                    mapMissingMints.insert(pMint);
                    break;
                }

                //The mint was found in the chain, so recalculate the randomness and serial and DB it
                int nHeight = 0;
                if (mapBlockIndex.count(hashBlock))
                    nHeight = mapBlockIndex.at(hashBlock)->nHeight;

                SetMintSeen(bnValue, nHeight, txHash, denomination);
                nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                nCount = std::max(nLastCountUsed + 1, nCount);
                LogPrint("zero", "%s: updated count to %d\n", __func__, nCount);
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
    CZerocoinMint mint(denom, bnValue, bnRandomness, bnSerial, false, PrivateCoin::CURRENT_VERSION);
    mint.SetPrivKey(key.GetPrivKey());
    mint.SetHeight(nHeight);
    mint.SetTxHash(txid);

    //Store the mint to DB
    if (!CWalletDB(strWalletFile).WriteZerocoinMint(mint)) {
        LogPrintf("%s : failed to database mint %s!\n", __func__, mint.GetValue().GetHex());
        return false;
    }

    //Update the count if it is less than the mint's count
    if (nCount <= pMint.second) {
        CWalletDB walletdb(strWalletFile);
        nCount = pMint.second + 1;
        walletdb.WriteZWGRCount(nCount);
    }

    //remove from the pool
    mintPool.Remove(mint.GetValue());

    //fill the pool up with the next value(s)
    //GenerateMintPool();

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
    return GetZerocoinSeed(nCount);
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
    nCount++;
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteZWGRCount(nCount);
}

void CzWGRWallet::GenerateDeterministicZWGR(CoinDenomination denom, PrivateCoin& coin, bool fGenerateOnly)
{
    GenerateMint(nCount, coin, denom);
    if (fGenerateOnly)
        return;

    //TODO remove this leak of seed from logs before merge to master
    //LogPrintf("%s : Generated new deterministic mint. Count=%d pubcoin=%s seed=%s\n", __func__, nCount, coin.getPublicCoin().getValue().GetHex().substr(0,6), seedZerocoin.GetHex().substr(0, 4));

    //set to the next count
    UpdateCount();
}

void CzWGRWallet::GenerateMint(uint32_t nCount, PrivateCoin& coin, CoinDenomination denom)
{
    uint512 seedZerocoin = GetZerocoinSeed(nCount);
    CBigNum bnSerial;
    CBigNum bnRandomness;
    CKey key;
    SeedToZWGR(seedZerocoin, bnSerial, bnRandomness, key);
    coin = PrivateCoin(Params().Zerocoin_Params(false), denom, bnSerial, bnRandomness);
    coin.setPrivKey(key.GetPrivKey());
    coin.setVersion(PrivateCoin::CURRENT_VERSION);
}
