// Copyright (c) 2019 The WAGERR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zwgr/zwgrmodule.h"
#include "zwgrchain.h"
#include "libzerocoin/Commitment.h"
#include "libzerocoin/Coin.h"
#include "hash.h"
#include "main.h"
#include "iostream"

PublicCoinSpend::PublicCoinSpend(libzerocoin::ZerocoinParams* params, const uint8_t version,
        const CBigNum& serial, const CBigNum& randomness, const uint256& ptxHash, CPubKey* pubkey):
            pubCoin(params)
{
    this->coinSerialNumber = serial;
    this->version = version;
    this->spendType = libzerocoin::SpendType::SPEND;
    this->ptxHash = ptxHash;
    this->coinVersion = libzerocoin::ExtractVersionFromSerial(coinSerialNumber);

    if (!isAllowed()) {
        // v1 coins need at least version 4 spends
        std::string errMsg = strprintf("Unable to create PublicCoinSpend version %d with coin version 1. "
                "Minimum spend version required: %d", version, PUBSPEND_SCHNORR);
        // this should be unreachable code (already checked in createInput)
        // throw runtime error
        throw std::runtime_error(errMsg);
    }

    if (pubkey && getCoinVersion() >= libzerocoin::PrivateCoin::PUBKEY_VERSION) {
        // pubkey available only from v2 coins onwards
        this->pubkey = *pubkey;
    }

    if (version < PUBSPEND_SCHNORR)
        this->randomness = randomness;
    else
        this->schnorrSig = libzerocoin::CoinRandomnessSchnorrSignature(params, randomness, ptxHash);

}

template <typename Stream>
PublicCoinSpend::PublicCoinSpend(libzerocoin::ZerocoinParams* params, Stream& strm): pubCoin(params) {
    strm >> *this;
    this->spendType = libzerocoin::SpendType::SPEND;

    if (this->version < PUBSPEND_SCHNORR) {
        // coinVersion is serialized only from v4 spends
        this->coinVersion = libzerocoin::ExtractVersionFromSerial(this->coinSerialNumber);

    } else {
        // from v4 spends, serialNumber is not serialized for v2 coins anymore.
        // in this case, we extract it from the coin public key
        if (this->coinVersion >= libzerocoin::PrivateCoin::PUBKEY_VERSION)
            this->coinSerialNumber = libzerocoin::ExtractSerialFromPubKey(this->pubkey);

    }

}

bool PublicCoinSpend::Verify(const libzerocoin::Accumulator& a, bool verifyParams) const {
    return validate();
}

bool PublicCoinSpend::validate() const {
    bool fUseV1Params = getCoinVersion() < libzerocoin::PrivateCoin::PUBKEY_VERSION;
    if (version < PUBSPEND_SCHNORR) {
        // spend contains the randomness of the coin
        if (fUseV1Params) {
            // Only v2+ coins can publish the randomness
            std::string errMsg = strprintf("PublicCoinSpend version %d with coin version 1 not allowed. "
                    "Minimum spend version required: %d", version, PUBSPEND_SCHNORR);
            return error("%s: %s", __func__, errMsg);
        }

        // Check that the coin is a commitment to serial and randomness.
        libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
        libzerocoin::Commitment comm(&params->coinCommitmentGroup, getCoinSerialNumber(), randomness);
        if (comm.getCommitmentValue() != pubCoin.getValue()) {
            return error("%s: commitments values are not equal", __func__);
        }

    } else {
        // for v1 coins, double check that the serialized coin serial is indeed a v1 serial
        if (coinVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION &&
                libzerocoin::ExtractVersionFromSerial(this->coinSerialNumber) != coinVersion) {
            return error("%s: invalid coin version", __func__);
        }

        // spend contains a shnorr signature of ptxHash with the randomness of the coin
        libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(fUseV1Params);
        if (!schnorrSig.Verify(params, getCoinSerialNumber(), pubCoin.getValue(), getTxOutHash())) {
            return error("%s: schnorr signature does not verify", __func__);
        }

    }

    // Now check that the signature validates with the serial
    if (!HasValidSignature()) {
        return error("%s: signature invalid", __func__);;
    }

    return true;
}

bool PublicCoinSpend::HasValidSignature() const
{
    if (coinVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION)
        return true;

    // for spend version 3 we must check that the provided pubkey and serial number match
    if (version < PUBSPEND_SCHNORR) {
        CBigNum extractedSerial = libzerocoin::ExtractSerialFromPubKey(this->pubkey);
        if (extractedSerial != this->coinSerialNumber)
            return error("%s: hashedpubkey is not equal to the serial!", __func__);
    }

    return pubkey.Verify(signatureHash(), vchSig);
}


const uint256 PublicCoinSpend::signatureHash() const
{
    CHashWriter h(0, 0);
    h << ptxHash << denomination << getCoinSerialNumber() << randomness << txHash << outputIndex << getSpendType();
    return h.GetHash();
}

namespace ZWGRModule {

    bool createInput(CTxIn &in, CZerocoinMint &mint, uint256 hashTxOut, const int spendVersion) {
        // check that this spend is allowed
        const bool fUseV1Params = mint.GetVersion() < libzerocoin::PrivateCoin::PUBKEY_VERSION;
        if (!PublicCoinSpend::isAllowed(fUseV1Params, spendVersion)) {
            // v1 coins need at least version 4 spends
            std::string errMsg = strprintf("Unable to create PublicCoinSpend version %d with coin version 1. "
                    "Minimum spend version required: %d", spendVersion, PUBSPEND_SCHNORR);
            return error("%s: %s", __func__, errMsg);
        }

        // create the PublicCoinSpend
        libzerocoin::ZerocoinParams *params = Params().Zerocoin_Params(fUseV1Params);
        PublicCoinSpend spend(params, spendVersion, mint.GetSerialNumber(), mint.GetRandomness(), hashTxOut, nullptr);

        spend.outputIndex = mint.GetOutputIndex();
        spend.txHash = mint.GetTxHash();
        spend.setDenom(mint.GetDenomination());

        // add public key and signature
        if (!fUseV1Params) {
            CKey key;
            if (!mint.GetKeyPair(key))
                return error("%s: failed to set zWGR privkey mint.", __func__);
            spend.setPubKey(key.GetPubKey(), true);

            std::vector<unsigned char> vchSig;
            if (!key.Sign(spend.signatureHash(), vchSig))
                return error("%s: ZWGRModule failed to sign signatureHash.", __func__);
            spend.setVchSig(vchSig);

        }

        // serialize the PublicCoinSpend and add it to the input scriptSig
        CDataStream ser(SER_NETWORK, PROTOCOL_VERSION);
        ser << spend;
        std::vector<unsigned char> data(ser.begin(), ser.end());
        CScript scriptSigIn = CScript() << OP_ZEROCOINPUBLICSPEND << data.size();
        scriptSigIn.insert(scriptSigIn.end(), data.begin(), data.end());

        // create the tx input
        in = CTxIn(mint.GetTxHash(), mint.GetOutputIndex(), scriptSigIn, mint.GetDenomination());
        in.nSequence = mint.GetDenomination();
        return true;
    }

    PublicCoinSpend parseCoinSpend(const CTxIn &in) {
        libzerocoin::ZerocoinParams *params = Params().Zerocoin_Params(false);
        // skip opcode and data-len
        uint8_t byteskip(in.scriptSig[1]);
        byteskip += 2;
        std::vector<char, zero_after_free_allocator<char> > data;
        data.insert(data.end(), in.scriptSig.begin() + byteskip, in.scriptSig.end());
        CDataStream serializedCoinSpend(data, SER_NETWORK, PROTOCOL_VERSION);

        return PublicCoinSpend(params, serializedCoinSpend);
    }

    bool parseCoinSpend(const CTxIn &in, const CTransaction &tx, const CTxOut &prevOut, PublicCoinSpend &publicCoinSpend) {
        if (!in.IsZerocoinPublicSpend() || !prevOut.IsZerocoinMint())
            return error("%s: invalid argument/s", __func__);

        PublicCoinSpend spend = parseCoinSpend(in);
        spend.outputIndex = in.prevout.n;
        spend.txHash = in.prevout.hash;
        CMutableTransaction txNew(tx);
        txNew.vin.clear();
        spend.setTxOutHash(txNew.GetHash());

        // Check prev out now
        CValidationState state;
        if (!TxOutToPublicCoin(prevOut, spend.pubCoin, state))
            return error("%s: cannot get mint from output", __func__);

        spend.setDenom(spend.pubCoin.getDenomination());
        publicCoinSpend = spend;
        return true;
    }

    bool validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction &tx, PublicCoinSpend &publicSpend) {
        // Now prove that the commitment value opens to the input
        if (!parseCoinSpend(in, tx, prevOut, publicSpend)) {
            return false;
        }
        if (libzerocoin::ZerocoinDenominationToAmount(
                libzerocoin::IntToZerocoinDenomination(in.nSequence)) != prevOut.nValue) {
            return error("PublicCoinSpend validateInput :: input nSequence different to prevout value");
        }
        return publicSpend.validate();
    }

    bool ParseZerocoinPublicSpend(const CTxIn &txIn, const CTransaction& tx, CValidationState& state, PublicCoinSpend& publicSpend)
    {
        CTxOut prevOut;
        if(!GetOutput(txIn.prevout.hash, txIn.prevout.n ,state, prevOut)){
            return state.DoS(100, error("%s: public zerocoin spend prev output not found, prevTx %s, index %d",
                                        __func__, txIn.prevout.hash.GetHex(), txIn.prevout.n));
        }
        if (!ZWGRModule::parseCoinSpend(txIn, tx, prevOut, publicSpend)) {
            return state.Invalid(error("%s: invalid public coin spend parse %s\n", __func__,
                                       tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zwgr");
        }
        return true;
    }
}
