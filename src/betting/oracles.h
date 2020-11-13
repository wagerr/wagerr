// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_ORACLE_H
#define WAGERR_BET_ORACLE_H

#include <string>
#include <vector>

class CScript;

class COracle {
private:
    std::string strAddress;
    std::string strDevPayoutAddress;
    std::string strOMNOPayoutAddress;
    int nStartHeight;
    int nEndHeight;

public:

    COracle(const std::string& strAddress, const std::string& strDevPayoutAddress, const std::string& strOMNOPayoutAddress,
            const int& nStartHeight, const int& nEndHeight) :
        strAddress(strAddress), strDevPayoutAddress(strDevPayoutAddress), strOMNOPayoutAddress(strOMNOPayoutAddress),
        nStartHeight(nStartHeight), nEndHeight(nEndHeight) {};

    bool IsActive(const int& nHeight);
    bool IsMyOracleTx(const std::string txAddress, const int& nTxHeight);

    const std::string getDevPayoutAddress() { return this->strDevPayoutAddress; }
    const std::string getOMNOPayoutAddress() { return this->strOMNOPayoutAddress; }
};

bool GetFeePayoutScripts(const int& nHeight, CScript& DevPayoutScript, CScript& OMNOPayoutScript);
bool GetFeePayoutAddresses(const int& nHeight, std::string& DevPayoutAddress, std::string& OMNOPayoutAddress);

#endif