/**
 * @file       CoinSpend.cpp
 *
 * @brief      CoinSpend class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
#include "CoinSpend.h"
#include <iostream>
namespace libzerocoin
{

CoinSpend::CoinSpend(const ZerocoinParams* p, const PrivateCoin& coin, Accumulator& a, const uint32_t& checksum,
                     const AccumulatorWitness& witness, const uint256& ptxHash, const SpendType& spendType) : accChecksum(checksum),
                                                                                  ptxHash(ptxHash),
                                                                                  coinSerialNumber((coin.getSerialNumber())),
                                                                                  accumulatorPoK(&p->accumulatorParams),
                                                                                  serialNumberSoK(p),
                                                                                  commitmentPoK(&p->serialNumberSoKCommitmentGroup,
                                                                                                &p->accumulatorParams.accumulatorPoKCommitmentGroup),
                                                                                  spendType(spendType)
{
    denomination = coin.getPublicCoin().getDenomination();
    version = coin.getVersion();

    // Sanity check: let's verify that the Witness is valid with respect to
    // the coin and Accumulator provided.
    if (!(witness.VerifyWitness(a, coin.getPublicCoin()))) {
        std::cout << "CoinSpend: Accumulator witness does not verify\n";
        throw std::runtime_error("Accumulator witness does not verify");
    }

    // 1: Generate two separate commitments to the public coin (C), each under
    // a different set of public parameters. We do this because the RSA accumulator
    // has specific requirements for the commitment parameters that are not
    // compatible with the group we use for the serial number proof.
    // Specifically, our serial number proof requires the order of the commitment group
    // to be the same as the modulus of the upper group. The Accumulator proof requires a
    // group with a significantly larger order.
    const Commitment fullCommitmentToCoinUnderSerialParams(&p->serialNumberSoKCommitmentGroup, coin.getPublicCoin().getValue());
    this->serialCommitmentToCoinValue = fullCommitmentToCoinUnderSerialParams.getCommitmentValue();

    const Commitment fullCommitmentToCoinUnderAccParams(&p->accumulatorParams.accumulatorPoKCommitmentGroup, coin.getPublicCoin().getValue());
    this->accCommitmentToCoinValue = fullCommitmentToCoinUnderAccParams.getCommitmentValue();

    // 2. Generate a ZK proof that the two commitments contain the same public coin.
    this->commitmentPoK = CommitmentProofOfKnowledge(&p->serialNumberSoKCommitmentGroup, &p->accumulatorParams.accumulatorPoKCommitmentGroup, fullCommitmentToCoinUnderSerialParams, fullCommitmentToCoinUnderAccParams);

    // Now generate the two core ZK proofs:
    // 3. Proves that the committed public coin is in the Accumulator (PoK of "witness")
    this->accumulatorPoK = AccumulatorProofOfKnowledge(&p->accumulatorParams, fullCommitmentToCoinUnderAccParams, witness, a);

    // 4. Proves that the coin is correct w.r.t. serial number and hidden coin secret
    // (This proof is bound to the coin 'metadata', i.e., transaction hash)
    uint256 hashSig = signatureHash();
    this->serialNumberSoK = SerialNumberSignatureOfKnowledge(p, coin, fullCommitmentToCoinUnderSerialParams, hashSig);

    // 5. Sign the transaction using the private key associated with the serial number
    if (version >= PrivateCoin::PUBKEY_VERSION) {
        this->pubkey = coin.getPubKey();
        if (!coin.sign(hashSig, this->vchSig))
            throw std::runtime_error("Coinspend failed to sign signature hash");
    }
}

int ExtractVersionFromSerial(const CBigNum& bnSerial)
{
    //Serial is marked as v2 only if the first byte is 0xF
    uint256 nMark = bnSerial.getuint256() >> (256 - PrivateCoin::V2_BITSHIFT);
    if (nMark == 0xf)
        return PrivateCoin::PUBKEY_VERSION;

    return 1;
}

bool CoinSpend::Verify(const Accumulator& a) const
{
    // Double check that the version is the same as marked in the serial
    if (ExtractVersionFromSerial(coinSerialNumber) != version)
        return false;

    // Verify both of the sub-proofs using the given meta-data
    return (a.getDenomination() == this->denomination) &&
            commitmentPoK.Verify(serialCommitmentToCoinValue, accCommitmentToCoinValue) &&
            accumulatorPoK.Verify(a, accCommitmentToCoinValue) &&
            serialNumberSoK.Verify(coinSerialNumber, serialCommitmentToCoinValue, signatureHash());
}

const uint256 CoinSpend::signatureHash() const
{
    CHashWriter h(0, 0);
    h << serialCommitmentToCoinValue << accCommitmentToCoinValue << commitmentPoK << accumulatorPoK << ptxHash
      << coinSerialNumber << accChecksum << denomination;

    if (version >= PrivateCoin::PUBKEY_VERSION)
        h << spendType;

    return h.GetHash();
}

bool CoinSpend::HasValidSerial(ZerocoinParams* params) const
{
    if (coinSerialNumber <= 0)
        return false;

    if (version < PrivateCoin::PUBKEY_VERSION)
        return coinSerialNumber < params->coinCommitmentGroup.groupOrder;

    uint256 n =  ~uint256(0);
    n <<= (256 - PrivateCoin::V2_BITSHIFT);
    uint256 nGroupOrderLimit = params->coinCommitmentGroup.groupOrder.getuint256() | n;
    return coinSerialNumber.getuint256() < nGroupOrderLimit;
}

//Additional verification layer that requires the spend be signed by the private key associated with the serial
bool CoinSpend::HasValidSignature() const
{
    uint256 nSerial = coinSerialNumber.getuint256();
    if (version < PrivateCoin::PUBKEY_VERSION)
        return true;

    //Adjust the serial to drop the first 4 bits
    uint256 nMask = ~uint256(0) >> PrivateCoin::V2_BITSHIFT;
    nSerial &= nMask;

    //v2 serial requires that the signature hash be signed by the public key associated with the serial
    uint256 hashedPubkey = Hash(pubkey.begin(), pubkey.end()) >> PrivateCoin::V2_BITSHIFT;
    if (hashedPubkey != nSerial)
        return false;

    return pubkey.Verify(signatureHash(), vchSig);
}

CBigNum CoinSpend::CalculateValidSerial(ZerocoinParams* params)
{
    CBigNum bnSerial = coinSerialNumber;
    bnSerial = bnSerial.mul_mod(CBigNum(1),params->coinCommitmentGroup.groupOrder);
    return bnSerial;
}

} /* namespace libzerocoin */
