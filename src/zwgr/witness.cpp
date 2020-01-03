#include <chainparams.h>
#include <tinyformat.h>
#include "witness.h"

void CoinWitnessData::SetNull()
{
    coin = nullptr;
    pAccumulator = nullptr;
    pWitness = nullptr;
    nMintsAdded = 0;
    nHeightMintAdded = 0;
    nHeightCheckpoint = 0;
    nHeightAccStart = 0;
    nHeightAccEnd = 0;
}

CoinWitnessData::CoinWitnessData()
{
    SetNull();
}

std::string CoinWitnessData::ToString()
{
    return strprintf("Mints Added: %d\n"
            "Height Mint added: %d\n"
            "Height Checkpoint: %d\n"
            "Height Acc Start: %d\n"
            "Height Acc End: %d\n"
            "Amount: %s\n"
            "Demon: %d\n", nMintsAdded, nHeightMintAdded, nHeightCheckpoint, nHeightAccStart, nHeightAccEnd, coin->getValue().GetHex(), coin->getDenomination());
}

CoinWitnessData::CoinWitnessData(CZerocoinMint& mint)
{
    SetNull();
    denom = mint.GetDenomination();
    isV1 = libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber()) < libzerocoin::PrivateCoin::PUBKEY_VERSION;
    libzerocoin::ZerocoinParams* paramsCoin = Params().Zerocoin_Params(isV1);
    coin = std::unique_ptr<libzerocoin::PublicCoin>(new libzerocoin::PublicCoin(paramsCoin, mint.GetValue(), denom));
    libzerocoin::Accumulator accumulator1(Params().Zerocoin_Params(false), denom);
    pWitness = std::unique_ptr<libzerocoin::AccumulatorWitness>(new libzerocoin::AccumulatorWitness(Params().Zerocoin_Params(false), accumulator1, *coin));
    nHeightAccStart = mint.GetHeight();
}

void CoinWitnessData::SetHeightMintAdded(int nHeight)
{
    nHeightMintAdded = nHeight;
    nHeightCheckpoint = nHeight + (10 - (nHeight % 10));
    nHeightAccStart = nHeight - (nHeight % 10);
}
