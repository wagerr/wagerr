// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/oracles.h>
#include <chainparams.h>
#include <script/standard.h>
#include <base58.h>

// Returns if the current oracle is active according to the specified block height
bool COracle::IsActive(const int& nHeight)
{
    return (nHeight >= this->nStartHeight && nHeight < this->nEndHeight);
}

// Validate the tx
bool COracle::IsMyOracleTx(const std::string txAddress, const int& nTxHeight)
{
    if (!this->IsActive(nTxHeight)) return false;
    return txAddress == this->strAddress;
}

bool GetFeePayoutScripts(const int& nHeight, CScript& DevPayoutScript, CScript& OMNOPayoutScript)
{
    for (auto oracle : Params().Oracles()) {
        if (oracle.IsActive(nHeight)) {
            DevPayoutScript = GetScriptForDestination(CBitcoinAddress(oracle.getDevPayoutAddress()).Get());
            OMNOPayoutScript = GetScriptForDestination(CBitcoinAddress(oracle.getOMNOPayoutAddress()).Get());
            return true;
        }
    }
    return false;
}

bool GetFeePayoutAddresses(const int& nHeight, std::string& DevPayoutAddress, std::string& OMNOPayoutAddress)
{
    for (auto oracle : Params().Oracles()) {
        if (oracle.IsActive(nHeight)) {
            DevPayoutAddress = oracle.getDevPayoutAddress();
            OMNOPayoutAddress = oracle.getOMNOPayoutAddress();
            return true;
        }
    }
    return false;
}
