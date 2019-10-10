// Copyright (c) 2019 The WAGERR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#ifndef WAGERR_ZWGRMODULE_H
#define WAGERR_ZWGRMODULE_H

#include "libzerocoin/bignum.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/CoinSpend.h"
#include "libzerocoin/Coin.h"
#include "libzerocoin/CoinRandomnessSchnorrSignature.h"
#include "libzerocoin/SpendType.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"
#include <streams.h>
#include <utilstrencodings.h>
#include "zwgr/zerocoin.h"
#include "chainparams.h"

static int const PUBSPEND_SCHNORR = 4;

class PublicCoinSpend : public libzerocoin::CoinSpend {
public:

    PublicCoinSpend(libzerocoin::ZerocoinParams* params): pubCoin(params) {};
    PublicCoinSpend(libzerocoin::ZerocoinParams* params, const uint8_t version, const CBigNum& serial, const CBigNum& randomness, const uint256& ptxHash, CPubKey* pubkey);
    template <typename Stream> PublicCoinSpend(libzerocoin::ZerocoinParams* params, Stream& strm);

    ~PublicCoinSpend(){};

    const uint256 signatureHash() const override;
    void setVchSig(std::vector<unsigned char> vchSig) { this->vchSig = vchSig; };
    bool Verify(const libzerocoin::Accumulator& a, bool verifyParams = true) const override;
    bool HasValidSignature() const;
    bool validate() const;
    static bool isAllowed(const bool fUseV1Params, const int spendVersion) { return !fUseV1Params || spendVersion >= PUBSPEND_SCHNORR; }
    bool isAllowed() const {
        const bool fUseV1Params = getCoinVersion() < libzerocoin::PrivateCoin::PUBKEY_VERSION;
        return isAllowed(fUseV1Params, version);
    }
    int getCoinVersion() const { return this->coinVersion; }

    // Members
    int coinVersion;
    CBigNum randomness;
    libzerocoin::CoinRandomnessSchnorrSignature schnorrSig;
    // prev out values
    uint256 txHash = 0;
    unsigned int outputIndex = -1;
    libzerocoin::PublicCoin pubCoin;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(version);

        if (version < PUBSPEND_SCHNORR) {
            READWRITE(coinSerialNumber);
            READWRITE(randomness);
            READWRITE(pubkey);
            READWRITE(vchSig);

        } else {
            READWRITE(coinVersion);
            if (coinVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION) {
                READWRITE(coinSerialNumber);
            }
            else {
                READWRITE(pubkey);
                READWRITE(vchSig);
            }
            READWRITE(schnorrSig);
        }
    }
};


class CValidationState;

namespace ZWGRModule {
    bool createInput(CTxIn &in, CZerocoinMint& mint, uint256 hashTxOut, const int spendVersion);
    PublicCoinSpend parseCoinSpend(const CTxIn &in);
    bool parseCoinSpend(const CTxIn &in, const CTransaction& tx, const CTxOut &prevOut, PublicCoinSpend& publicCoinSpend);
    bool validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction& tx, PublicCoinSpend& ret);

    // Public zc spend parse
    /**
     *
     * @param in --> public zc spend input
     * @param tx --> input parent
     * @param publicCoinSpend ---> return the publicCoinSpend parsed
     * @return true if everything went ok
     */
    bool ParseZerocoinPublicSpend(const CTxIn &in, const CTransaction& tx, CValidationState& state, PublicCoinSpend& publicCoinSpend);
};


#endif //WAGERR_ZWGRMODULE_H
